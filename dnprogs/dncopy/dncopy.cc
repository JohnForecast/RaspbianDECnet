/******************************************************************************
    (c) 1998-2008 Christine Caulfield               christine.caulfield@googlemail.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 ******************************************************************************
 */
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdnet/dn.h>
#include <regex.h>
#include "connection.h"
#include "protocol.h"

#include "file.h"
#include "dnetfile.h"
#include "unixfile.h"

static bool  dntype = false;
static bool  cont_on_error = false;

// Prototypes
static void usage(char *name, int dntype, FILE *f);
static file *getFile(const char *name, int verbosity);
static void get_env_as_args(char **argv[], int &argc, char *env);
static void do_options(int argc, char *argv[],
		       int &rfm, int &rat, int &org,
		       int &interactive, int &keep_version, int &user_bufsize,
		       int &remove_cr, int &show_stats, int &verbose,
		       int &flags, char *protection, int &connect_timeout);

// Start here:
int main(int argc, char *argv[])
{
    file *out;
    file *in;
    int   num_input_files;
    int   filenum;
    char *buf;
    int   rat = file::RAT_DEFAULT;
    int   rfm = file::RFM_DEFAULT;
    int   org = file::MODE_RECORD;
    int   user_bufsize = -1;
    int   bufsize = 16384;
    int   verbose = 0;
    int   interactive = FALSE;
    int   keep_version = FALSE;
    int   last_infile;
    int   remove_cr = 0;
    int   show_stats = 0;
    int   printfile = 0;
    int   flags = 0;
    int   connect_timeout = 60;
    char  opt;
    char  protection[255]={'\0'};
    struct timeval start_tv;
    unsigned long long bytes_copied = 0;

    if (argc < 2)
    {
	usage(argv[0], dntype, stdout);
	return 0;
    }

    // See if we are dntype or dncopy
    if ((strstr(argv[0], "dntype") != NULL))
    {
        dntype = TRUE;
    }

// If there is a DNCOPY_OPTIONS environment variable then parse that first
    char **env_argv;
    int    env_argc;
    get_env_as_args(&env_argv, env_argc, getenv("DNCOPY_OPTIONS"));
    if (env_argc)
	do_options(env_argc, env_argv,
		   rfm, rat,org,
		   interactive, keep_version, user_bufsize,
		   remove_cr, show_stats, verbose, flags, protection, connect_timeout);


// Parse the command-line options
    do_options(argc, argv,
	       rfm, rat,org,
	       interactive, keep_version, user_bufsize,
	       remove_cr, show_stats, verbose, flags, protection, connect_timeout);

    // Work out the buffer size. The default for block transfers is 512
    // bytes unless the user specified otherwise.
    if (user_bufsize > -1)
        bufsize = user_bufsize;
    else
      if (org == file::MODE_BLOCK)
	  bufsize = 512;

    // If the user wants a block transfer and did not specify
    // a record format then default to FIXed length records
    // with no carriage control.
    if (org == file::MODE_BLOCK)
    {
	if (rat == file::RAT_DEFAULT) rat = file::RAT_NONE;
        if (rfm == file::RFM_DEFAULT) rfm = file::RFM_FIX;
    }

    // Get the input file name(s)
    num_input_files = argc - optind - 1;

    // If the command is dntype then output to stdout
    if (dntype)
    {
	out = getFile("-", verbose);
	last_infile = argc;
    }
    else
    {
	if ( (argc - optind) < 2) // Are there enough args?
	{
	    usage(argv[0], dntype, stderr);
	    return 0;
	}
	out = getFile(argv[argc-1], verbose);
	last_infile = argc-1;
    }

    // If there are multiple input files or the one input file is a wildcard
    // then the output must be a directory.
    if (num_input_files > 1 && !dntype)
    {
	if (!out->isdirectory())
	{
	    fprintf(stderr, "Output must be a directory\n");
	    return 3;
	}
    }

    // Allocate transfer buffer. Always allocate the transfer size to allow
    // for the remote end streaming data to us.
    buf = (char *)malloc(65536);
    if (!buf)
    {
	fprintf(stderr, "Cannot allocate transfer buffer");
	out->close();
	return 2;
    }

    // Set up the network links if necessary
    if (out->setup_link(bufsize, rfm, rat, org, flags, connect_timeout))
    {
	out->perror("Error setting up output link");
	return 1;
    }

    // Reduce the buffer size to the biggest the output host can handle.
    bufsize = out->max_buffersize(bufsize);

    for (filenum = optind; filenum < last_infile; filenum++)
    {
	    in = getFile(argv[filenum], verbose);

	// Now we have the first file name, if it is the only input file then
	// we can check to see if it is a wildcard. If that is so then the
	// output must be a directory.
	if (in->iswildcard())
	{
	    if (!out->isdirectory() && !dntype)
	    {
		fprintf(stderr, "Output must be a directory\n");
		return 3;
	    }
	}

	if (in->setup_link(bufsize, rfm, rat, org, flags, connect_timeout))
	{
	    in->perror("Error setting up input link");
	    return 1;
	}

	// Start the timer after the link setup. You may think that's
	// a bit of a cheat, well - tough.
	if (show_stats)
	    gettimeofday(&start_tv, NULL);

	// Copy the file(s)
	do
	{
	    int buflen;
	    int blocks = 0;
	    int do_copy = !interactive;


	    // Open the input file
	    if (in->open("r"))
	    {
		in->perror("Error opening file for input");
		return 1;
	    }

	    if (interactive)
	    {
		char response[80];

		if (dntype)
		    printf("Type %s ? ",
		       in->get_printname());
		else
		    printf("Copy %s to %s ? ",
			   in->get_printname(),
			   out->get_printname(in->get_basename(keep_version)));

		fgets(response, sizeof(response), stdin);
		if (tolower(response[0]) == 'y')
		    do_copy = TRUE;
	    }

	    if (do_copy)
	    {
		// If the output is a directory so we need to add the
		// input file's basename to it.
		out->set_protection(protection);
		if (out->isdirectory())
		{
		    if (out->open(in->get_basename(keep_version), "w+"))
		    {
			out->perror("Error opening file for output");
			in->close();
			if (cont_on_error)
			    continue;
			else
			    return 1;
		    }
		}
		else
		{
		    if (out->open("w+"))
		    {
			out->perror("Error opening file for output");
			in->close();
			return 1;
		    }
		}

		if (dntype && verbose) printf("\n%s\n\n", in->get_printname());

		// Copy the data
		while ( ((buflen = in->read(buf, bufsize))) >= 0 )
		{
		    // Remove trailing CRs if required
		    if (remove_cr &&
			org == file::MODE_RECORD &&
			buf[buflen-2] == '\r')
		    {
			// CR is before the LF in the buffer.
			buf[buflen-2] = buf[buflen-1];
			buflen--;
		    }

		    if (out->write(buf, buflen) < 0)
		    {
			out->perror("Error writing");
			in->close();
			return 3;
		    }
		    blocks++;
		    bytes_copied += buflen;
		}

		// If we finished with an error then display it
		if (!in->eof())
		{
		    in->perror("Error reading");
		    out->close();
		    return 3;
		}

		// Set the file protection.
		if (out->set_umask(in->get_umask()) && !dntype)
		{
		    out->perror("Error setting protection");
		    // Non-fatal error this one.
		}
		if (!dntype)
		{
		    if (out->close())
		    {
			out->perror("Error closing output");
			return 3;
		    }
		}

		// Log the operation if we were asked
		if (verbose && !dntype)
		    printf("'%s' copied to '%s', %d %s\n",
			   in->get_printname(),
			   out->get_printname(),
			   blocks,
			   in->get_format_name());

	    }
	    if (in->close())
	    {
		out->perror("Error closing input");
		return 3;
	    }
	}
	while(in->next());
    }

    // Show stats
    if (show_stats)
    {
	struct timeval stop_tv;
	long centi_seconds;
	double rate;
	double show_secs;

	gettimeofday(&stop_tv, NULL);
	centi_seconds = (stop_tv.tv_sec - start_tv.tv_sec) * 100 +
	    (stop_tv.tv_usec - start_tv.tv_usec) / 10000;
	show_secs = (double)centi_seconds/100.0;

	rate = (double)(bytes_copied/1024) / (double)centi_seconds * 100.0;
	printf("Sent %lld bytes in %1.2f seconds: %4.2fK/s\n",
	       bytes_copied, show_secs, rate);
    }
}

// Print a usage message. We can be called as dncopy or dntype so adapt
// accordingly
static void usage(char *name, int dntype, FILE *f)
{
    fprintf(f, "\nusage: %s [OPTIONS] infile", name);
    if (!dntype)
    {
	fprintf(f, " outfile");
	fprintf(f, "\n   or: %s [OPTIONS] infiles...", name);
	fprintf(f, " directory");
    }

    fprintf(f, "\n\n");
    fprintf(f, " Options\n");
    fprintf(f, "  -? -h        display this help message\n");
    fprintf(f, "  -v           verbose operation.\n");
    fprintf(f, "  -s           show transfer statistics.\n");
    if (!dntype)
    {
        fprintf(f, "  -i           interactive. Prompt before copying\n");
        fprintf(f, "  -k        (r)keep version numbers on files\n");
        fprintf(f, "  -m <mode>    access mode: (record, block)\n");
        fprintf(f, "  -a <att>  (s)record attributes (none, ftn, cr, prn)\n");
        fprintf(f, "  -r <fmt>  (s)record format (fix, var, vfc, stm)\n");
        fprintf(f, "  -b <n>       use a block size of <n> bytes\n");
        fprintf(f, "  -d        (s)remove trailing CR on record (DOS file transfer)\n");
        fprintf(f, "  -l        (r)ignore interlocks on remote file\n");
	fprintf(f, "  -P        (s)print file to SYS$PRINT\n");
	fprintf(f, "  -D        (s)delete file on close. Only really useful with -P\n");
	fprintf(f, "  -T <secs>    connect timeout in seconds (default 60)\n");
        fprintf(f, "  -V           show version number\n");
        fprintf(f, "\n");
        fprintf(f, " (s) - only useful when sending files to VMS\n");
        fprintf(f, " (r) - only useful when receiving files from VMS\n");
    }
    else
    {
        fprintf(f, "  -i           interactive. Prompt before displaying\n");
    }
    fprintf(f, "\n");
    fprintf(f, "NOTE: It is a good idea to put VMS filenames in single quotes\n");
    fprintf(f, "to stop the shell from swallowing special characters such as double\n");
    fprintf(f, "quotes and dollar signs. eg:\n");
    fprintf(f, "\n");
    fprintf(f, "%s 'mynode\"christine password\"::sys$manager:sylogin.com'", name);
    if (!dntype)
    {
	fprintf(f, " .");
    }
    fprintf(f, "\n\n");
}

// Run through the file types and return an object that matches the
// type of the name we were passed.
static file *getFile(const char *name, int verbosity)
{
    if (dnetfile::isMine(name))
	return new dnetfile(name, verbosity);

    // Lots of opportunities for other file types here...


    // Default to a local file.
    return new unixfile(name);
}

static void get_env_as_args(char **argv[], int &argc, char *env)
{
    int count = 0;
    char *ptr;

    argc = 0;
    if (!env) return; // No variable.

    // Take a copy of the arglist as we mangle it.
    char *arglist = (char *)malloc(strlen(env)+1);
    strcpy(arglist, env);

// Quickly run through the variable to see how many options there are.
    ptr = strtok(arglist, " ");
    while(ptr)
    {
	count++;
	ptr = strtok(NULL, " ");
    }

    *argv = (char **)malloc((sizeof(char *) * (count+2)));
    argc = count+1;
    char **pargv = *argv;
    strcpy(arglist, env);

// Now build the array of args, starting at 1
// 'cos 0 is the program name, remember?
    count = 1;
    ptr = strtok(arglist, " ");
    while(ptr)
    {
	pargv[count] = (char *)malloc(strlen(ptr)+1);
	strcpy(pargv[count], ptr);
	ptr = strtok(NULL, " ");
	count++;
    }
    pargv[count] = NULL; // Make sure the last element is NULL
    free(arglist);
}

// Process the options
static void do_options(int argc, char *argv[],
		       int &rfm, int &rat, int &org,
		       int &interactive, int &keep_version, int &user_bufsize,
		       int &remove_cr, int &show_stats, int &verbose,
		       int &flags, char *protection, int &connect_timeout)
{
    int opt;
    opterr = 0;
    optind = 0;
    while ((opt=getopt(argc,argv,"?Vvhdr:a:b:kislm:p:PDET:")) != EOF)
    {
	switch(opt) {
	case 'h':
	    usage(argv[0], dntype, stdout);
	    exit(0);

	case '?': // Called if getopt doesn't recognise the option
	    usage(argv[0], dntype, stderr);
	    exit(0);

	case 'v':
	    verbose++;
	    break;

	case 'l':
	    flags |= file::FILE_FLAGS_RRL;
	    break;

	case 'E':
	    cont_on_error = true;
	    break;

	case 'T':
	    connect_timeout = atoi(optarg);
	    break;

	case 'r':
	    if (tolower(optarg[0]) == 'f') rfm = file::RFM_FIX;
	    if (tolower(optarg[0]) == 's') rfm = file::RFM_STM;
	    if (!strncmp(optarg, "va", 2)) rfm = file::RFM_VAR;
	    if (!strncmp(optarg, "vf", 2)) rfm = file::RFM_VFC;
	    if (rfm == file::RFM_DEFAULT)
	    {
		fprintf(stderr, "Invalid record format string\n");
		fprintf(stderr, "%s -h for more information\n", argv[0]);
		exit(1);
	    }
	    break;

	case 'a':
	    if (tolower(optarg[0]) == 'f') rat = file::RAT_FTN;
	    if (tolower(optarg[0]) == 'c') rat = file::RAT_CR;
	    if (tolower(optarg[0]) == 'p') rat = file::RAT_PRN;
	    if (tolower(optarg[0]) == 'n') rat = file::RAT_NONE;
	    if (rat == file::RAT_DEFAULT)
	    {
		fprintf(stderr, "Invalid record attributes string\n");
		fprintf(stderr, "%s -h for more information\n", argv[0]);
		exit(1);
	    }
	    break;

	case 'm':
	    org = file::MODE_DEFAULT; // Set to default so w can catch errors
	    if (tolower(optarg[0]) == 'r') org = file::MODE_RECORD;
	    if (tolower(optarg[0]) == 'b') org = file::MODE_BLOCK;
	    if (org == file::MODE_DEFAULT)
	    {
		fprintf(stderr, "Invalid transfer mode\n");
		fprintf(stderr, "%s -h for more information\n", argv[0]);
		exit(1);
	    }
	    break;

	case 'i':
	    interactive++;
	    break;

	case 's':
	    show_stats++;
	    break;

	case 'k':
	    keep_version = TRUE;
	    break;

	case 'P':
	    flags |= file::FILE_FLAGS_SPOOL;
	    break;

	case 'D':
	    flags |= file::FILE_FLAGS_DELETE;
	    break;

	case 'b':
	    user_bufsize = atoi(optarg);
	    break;

	case 'p':
	    strcpy(protection, optarg);
	    strcpy(protection, optarg);
	    {
                    // Validate protection
                    dap_protect_message prot;
                    if (prot.set_protection(protection) == -1)
                    {
                            fprintf(stderr, "Invalid VMS protection string\n");
                            exit(1);
                    }
            }
	    break;

	case 'd':
	    remove_cr++;
	    break;

	case 'V':
	    printf("\ndncopy/dntype from dnprogs version %s\n\n", VERSION);
	    exit(1);
	    break;
	}
    }
}
