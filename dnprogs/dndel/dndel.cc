/******************************************************************************
    (c) 1998-2008      Christine Caulfield          christine.caulfield@googlemail.com

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
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include <unistd.h>
#include <regex.h>

#include "connection.h"
#include "protocol.h"
#include "logging.h"


static void usage(void)
{
    printf("\nUSAGE: dndel [OPTIONS] 'node\"user password\"::filespec'\n\n");
    printf("NOTE: The VMS filename really should be in single quotes to\n");
    printf("      protect it from the shell\n");

    printf("\nOptions:\n");
    printf("  -i           interactive - prompt before deleting\n");
    printf("  -v           verbose - display files that have been deleted\n");
    printf("  -T <secs>    connect timeout (default 60)\n");
    printf("  -? -h        display this help message\n");
    printf("  -V           show version number\n");

    printf("\nExamples:\n\n");
    printf(" dndel -l 'myvax::*.*;'\n");
    printf(" dndel 'cluster\"christine thecats\"::disk$users:[cats.pics]otto.jpg;'\n");
    printf("\n");
}


// Check for an extra STATUS message following a NAME
static bool check_status(dap_connection &conn, char *name)
{
  dap_message *m = dap_message::read_message(conn, false);
  if (m)
  {
      if (m->get_type() == dap_message::STATUS)
      {
	  dap_status_message *sm = (dap_status_message *)m;
	  fprintf(stderr, "Error deleting %s: %s\n", name, sm->get_message());
	  delete m;
	  return false;
      }

      // The file must exist if we get an ATTRIB message.
      if (m->get_type() == dap_message::ATTRIB)
      {
	  delete m;
	  return true;
      }

      fprintf(stderr, "Got unexpected message: %s\n", m->type_name());
      delete m;
  }
  return true;
}


// Send an ACCESS DIRECTORY request. If we are running non-interactively then
// we actually delete the files here instead.
static	bool dap_directory_lookup(dap_connection &c, char *dirname,
				  bool interactive)
{
    dap_access_message acc;
    if (interactive)
	acc.set_accfunc(dap_access_message::DIRECTORY);
    else
	acc.set_accfunc(dap_access_message::ERASE);
    acc.set_accopt(1);
    acc.set_filespec(dirname);
    acc.set_display(dap_access_message::DISPLAY_MAIN_MASK);
    return acc.write(c);
}

// Delete one file
static bool dap_delete_file(dap_connection &c, char *name)
{
    dap_access_message acc;
    acc.set_accfunc(dap_access_message::ERASE);
    acc.set_accopt(1);
    acc.set_filespec(name);
    acc.set_display(0);
    return acc.write(c);
}

// Get a response from the delete request
static bool dap_get_delete_ack(dap_connection &c)
{
    dap_message *m = dap_message::read_message(c, true);
    if (m)
    {
	switch (m->get_type())
	{
	case dap_message::ACCOMP:
	case dap_message::ACK:
	    return true;

	case dap_message::STATUS:
	    {
		dap_status_message *sm = (dap_status_message *)m;
		fprintf(stderr, "Error deleting file: %s\n", sm->get_message());
		return false;
	    }
	}
    }
    return false;
}

// Send CONTRAN/SKIP message. We need this if a file in the list is locked.
static bool dap_send_skip(dap_connection &conn)
{
    dap_contran_message cm;
    cm.set_confunc(dap_contran_message::SKIP);
    return cm.write(conn);
}

// Get the file information. Actually just the name
static int dap_get_dir_entry(dap_connection &c, char *name)
{
    static char volume[256];
    static char dir[256];
    static bool got_name = false;

    dap_message *m;
    while ( ((m = dap_message::read_message(c, true))) )
    {
	switch (m->get_type())
	{
	case dap_message::NAME:
	    {
		dap_name_message *nm = (dap_name_message *)m;
		switch (nm->get_nametype())
		{
		case dap_name_message::VOLUME:
		    strcpy(volume, nm->get_namespec());
		    break;

		case dap_name_message::DIRECTORY:
		    strcpy(dir, nm->get_namespec());
		    break;

		case dap_name_message::FILENAME:
		    sprintf(name, "%s%s%s", volume, dir, nm->get_namespec());
		    got_name = true;
		    delete m;
		    if (check_status(c, name))
		        return dap_message::NAME;
		    else
		        return -1;

		case dap_name_message::FILESPEC:
		    strcpy(name, nm->get_namespec());
		    got_name = true;
		    delete m;
		    if (check_status(c, name))
		        return dap_message::NAME;
		    else
		        return -1;
		}

	    }
	    break;

	case dap_message::ACCOMP:
	    delete m;
	    return -1;

	case dap_message::ACK:
	    delete m;
	    return dap_message::ACK;

	case dap_message::STATUS:
	    {
		dap_status_message *sm = (dap_status_message *)m;
		if (sm->get_code() == 0x4030) // Locked
		{
		    dap_send_skip(c);
		    break;
		}
		fprintf(stderr, "Error deleting %s: %s\n",
			got_name?name:"file",
			sm->get_message());
		delete m;
		return -1;
	    }

	default:
	    delete m;
	    return m->get_type();
	}
	delete m;
    }
    return -1;
}

// Start here...
int main(int argc, char *argv[])
{
    int	    opt,retval;
    char    name[256];
    bool    interactive = false;
    int     verbose = 0;
    int     two_links = 0;
    int     connect_timeout = 60;

    if (argc < 2)
    {
	usage();
	exit(0);
    }

/* Get command-line options */
    opterr = 0;
    optind = 0;
    while ((opt=getopt(argc,argv,"?hvViT:")) != EOF)
    {
	switch(opt)
	{
	case 'h':
	case '?':
	    usage();
	    exit(1);

	case 'i':
	    interactive = true;
	    two_links++;
	    break;

	case 'T':
	    connect_timeout = atoi(optarg);
	    break;

	case 'v':
	    verbose++;
	    two_links++;
	    break;

	case 'V':
	    printf("\ndndel from dnprogs version %s\n\n", VERSION);
	    exit(1);
	    break;
	}
    }
    init_logging("dndel", 'e', false);

    dap_connection dir_conn(verbose);
    dap_connection del_conn(verbose);

    dir_conn.set_connect_timeout(connect_timeout);
    del_conn.set_connect_timeout(connect_timeout);

    /* We open one link to get the file names and (if interactive or verbose)
       another to do the actual deletion.
       This is not a waste of resources - it's what VMS does !
     */
    if (!dir_conn.connect(argv[optind], dap_connection::FAL_OBJECT, name))
    {
	fprintf(stderr, "%s\n", dir_conn.get_error());
	return -1;
    }
    // Exchange config messages
    if (!dir_conn.exchange_config())
    {
	fprintf(stderr, "Error in config: %s\n", dir_conn.get_error());
	return -1;
    }

    /* Open the second link to actually delete the file(s) */
    if (two_links)
    {
	if (!del_conn.connect(argv[optind], dap_connection::FAL_OBJECT, name))
	{
	    fprintf(stderr, "%s\n", del_conn.get_error());
	    return -1;
	}
	// Exchange config messages
	if (!del_conn.exchange_config())
	{
	    fprintf(stderr, "Error in config: %s\n", del_conn.get_error());
	    return -1;
	}
    }

    /* If non-interactive then the next command just deletes the files */
    dap_directory_lookup(dir_conn, name, interactive || (verbose>0) );

    /* Loop through the files we find */
    while ((retval=dap_get_dir_entry(dir_conn, name)) > 0)
    {
	if (retval == dap_message::NAME)
	{
	    if (interactive)
	    {
		char response[255];

		printf("Delete %s ? ", name);
		fgets(response, sizeof(response), stdin);
		if (tolower(response[0]) == 'y')
		{
		    dap_delete_file(del_conn, name);
		    if (dap_get_delete_ack(del_conn))
			if (verbose)
			    printf("Deleted %s\n", name);
		}
	    }
	    else if (verbose)
	    {
		dap_delete_file(del_conn, name);
		if (dap_get_delete_ack(del_conn))
		    printf("Deleted %s\n", name);
	    }
	}
    }

    dir_conn.close();
    if (two_links) del_conn.close();
    return 0;
}

