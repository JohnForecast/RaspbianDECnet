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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include <sys/types.h>
#include <regex.h>
#include "connection.h"
#include "protocol.h"
#include "logging.h"

#include "file.h"
#include "dnetfile.h"

// Assume the file name is a DECnet name if it has two consecutive colons in it
bool dnetfile::isMine(const char *name)
{
    if (strstr(name, "::"))
	return TRUE;
    else
	return FALSE;
}

// Constructor.
dnetfile::dnetfile(const char *n, int verbosity):
    conn(verbosity)
{
    isOpen = FALSE;
    verbose = verbosity;
    lasterror = NULL;
    protection = NULL;
    strcpy(fname, n);
    strcpy(name, n);

    // Is this a wildcard filename?
    if (strchr(name, '*') ||
	strchr(name, '%'))
	wildcard = TRUE;
    else
	wildcard = FALSE;
}

dnetfile::~dnetfile()
{
    dap_close_link();
}

void dnetfile::set_protection(char *vmsprot)
{
    if (vmsprot[0] != '\0')
	protection = vmsprot;
}

int dnetfile::setup_link(unsigned int bufsize, int rfm, int rat, int xfer_mode, int flags, int timeout)
{
// If there was a parse error in the file name then fail here
    if (lasterror)
	return -1;

    init_logging("dncopy", 'e', false);

// Save these for later
    user_rfm = rfm;
    user_rat = rat;
    transfer_mode = xfer_mode;
    user_bufsize = bufsize;
    user_flags = flags;

    struct accessdata_dn accessdata;
    memset(&accessdata, 0, sizeof(accessdata));
    if (!conn.parse(fname, accessdata, node, name))
    {
	lasterror = conn.get_error();
	return -1;
    }

    conn.set_connect_timeout(timeout);

    strcpy(user, (char *)accessdata.acc_user);
    strcpy(password, (char *)accessdata.acc_pass);

    if (!conn.connect(node, user, password, dap_connection::FAL_OBJECT))
    {
	lasterror = conn.get_error();
	return -1;
    }
    if (!conn.exchange_config())
    {
	lasterror = conn.get_error();
	return -1;
    }

    strcpy(filname, name);

    return 0;
}

// Open a file for writing
int dnetfile::open(const char *filename, const char *mode)
{
    // Add the remote end's directory spec to the filename.
    strcpy(filname, name);
    strcat(filname, filename);
    return open(mode);
}

// Open a file already named
int dnetfile::open(const char *mode)
{
    int real_rfm, real_rat;
    int status;

    switch (mode[0])
    {
    case 'r':
	writing = FALSE;
	break;

    case 'a':   // Not supported yet (if ever!)
	lasterror ="append unsupported";
	return -1;
	break;

    case 'w':
	writing = TRUE;
	break;
    }

    if (!isOpen) // Open connection for the first time
    {
	file_fsz = 0;
	if (writing)
	{
	    status = dap_send_attributes();

	    // Send default file name
	    if (wildcard)
	    {
		status = dap_send_name();
		if (status) return status;
	    }
	    status = dap_send_access();
	    if (status) return status;

	    status = dap_get_file_entry(&real_rfm, &real_rat);
	    if (status) return status;
	}
	else
	{
	    status = dap_send_access();
	    if (status) return status;
	}

	if (!writing)
	{
	    status = dap_get_file_entry(&real_rfm, &real_rat);
	    if (status) return status;
	    if (verbose > 1) printf("File attributes: rfm: %d, rat: %d\n",
				    real_rfm, real_rat);

// Use the file's real attributes if the user just wants defaults.
	    if (user_rfm == RFM_DEFAULT)  file_rfm = real_rfm;
	    if (user_rat == RAT_DEFAULT)  file_rat = real_rat;

// Get the file's real name in case the user specified a wildcard
	    strcpy(name, filname);
	}

	isOpen = TRUE;
	ateof = FALSE;
    }
    else if (writing) // new file to create on already open link
    {
        dap_send_attributes();
        dap_send_access();
	status = dap_get_file_entry(&real_rfm, &real_rat);
    	if (status) return status;
    }
    status = dap_send_connect();
    if (status) return status;

    status = dap_send_get_or_put();
    return status;
}

// Close the file but leave the link open in case there are any more to
// read/write
int dnetfile::close()
{
    return dap_send_accomp();
}

// Read a block or a record
int dnetfile::read(char *buf, int len)
{
    int retlen = dap_get_record(buf, len);
    if (retlen < 0) return retlen; // Empty record.

// If the file has implied carriage control then add an LF to the end of the
// line
    if ((file_rfm != RFM_STMLF) &&
	(file_rat & RAT_CR || file_rat & RAT_PRN))
	buf[retlen++] = '\n';

// Print files have a two-byte header indicating the line length.
    if (file_rat & RAT_PRN)
    {
	memmove(buf, buf+file_fsz, retlen-file_fsz);
	retlen -= file_fsz;
    }

// FORTRAN files have a leading character that indicates carriage control
    if (file_rat & RAT_FTN)
    {
	switch (buf[0])
	{

	case '+': // No new line
	  buf[0] = '\r';
	  break;

	case '1': // Form Feed
	  buf[0] = '\f';
	  break;

	case '0': // Two new lines
	  memmove(buf+1, buf, retlen+1);
	  buf[0] = '\n';
	  buf[1] = '\n';
	  retlen++;
	  break;


	case ' ': // new line
	default:  // Default to a new line. This seems to be what VMS does.
	  buf[0] = '\n';
	  break;
	}
    }
    return retlen;
}

// Send a block/record
int dnetfile::write(char *buf, int len)
{
    return dap_put_record(buf, len);
}

/* Get the next filename in a wildcard list */
int dnetfile::next()
{
    int status;
    int real_rfm, real_rat;

    if (!wildcard)
    {
	return FALSE;
    }

    if ( ((status = dap_get_file_entry(&real_rfm, &real_rat))) )
    {
	if (status != -2) perror("last file");
	return FALSE; // No more files.
    }
    else
    {

// Use the file's real attributes if the user just wants defaults.
        if (user_rfm == RFM_DEFAULT)  file_rfm = real_rfm;
        if (user_rat == RAT_DEFAULT)  file_rat = real_rat;
        if (verbose > 1) printf("File attributes: rfm: %d, rat: %d\n",
                                 real_rfm, real_rat);

// Get the file's real name in case the user specified a wildcard
        strcpy(name, filname);

        ateof = FALSE;
  	return TRUE;
    }
}

void dnetfile::perror(const char *msg)
{
    if (lasterror)
	fprintf(stderr, "%s: %s\n", msg, lasterror);
    else
	fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

char *dnetfile::get_basename(int keep_version)
{
    make_basename(keep_version);
    return &basename[0];
}

// Returns TRUE if the target filename looks like a directory
bool dnetfile::isdirectory()
{
    // If the last character of the filename is ']' or ':' then we assume
    // the name is a directory. This means that logical names for
    // directories MUST have a colon at the end.
    if (fname[strlen(fname)-1] == ':' ||
	fname[strlen(fname)-1] == ']')
	return TRUE;
    else
	return FALSE;
}

bool dnetfile::eof()
{
    return ateof;
}

bool dnetfile::iswildcard()
{
    return wildcard;
}

/* Work out the base name so that when we copy VMS->Unix we don't get the
   device and version numbers in the final filename */
void dnetfile::make_basename(int keep_version)
{
    char        *start;
    char        *end;
    unsigned int i;

    /* Find the start of the name */
    start = rindex(name, ']');
    if (!start) start = rindex(name, ':');
    if (!start) start = name-1;
    strcpy(basename, start+1);

    /* Remove a version number */
    if (!keep_version)
    {
	end = rindex(basename, ';');
	if (end) *end = '\0';
    }

    /* make all lower case */
    for (i=0; i < strlen(basename); i++)
    {
	basename[i] = tolower(basename[i]);
    }
}

// Return a pretty-printed version of the file name for confirmation and
// logging
char *dnetfile::get_printname(char *filename)
{
    static char pname[1024];

    strcpy(pname, node);
    if (*user)
    {
	strcat(pname, "\"");
	strcat(pname, user);
	if (*password) strcat(pname, " password");
	strcat(pname, "\"");
    }
    strcat(pname, "::");
    strcat(pname, volname);

    if (filename) // Use passed-in filename if there is one
    {
        strcat(pname, filename);
    }
    else
    {
        strcat(pname, dirname);
        strcat(pname, filname);
    }
    return pname;
}

char *dnetfile::get_printname()
{
    return get_printname((char *)NULL);
}

// Return a string telling the user whether we transferred blocks or
// records or what.
const char *dnetfile::get_format_name()
{
    switch (transfer_mode)
    {
    case MODE_DEFAULT:
	switch (file_rat)
	{
	case RAT_NONE:
	    return "blocks";

	default:
	    return "records";
	}
	break;

    case MODE_RECORD:
	return "records";
    case MODE_BLOCK:
	return "blocks";
    }
    return "things";
}


int dnetfile::get_umask()
{
    if (verbose > 1) printf("protection = %o\n", prot);
    return prot;
}

int dnetfile::set_umask(int mask)
{
// TODO Set the protection - this may have to be done at $CREATE time in
// which case dncopy needs rejigging (clang!)
    return 0;
}


// Return the largest buffer size less than 'biggest'
int dnetfile::max_buffersize(int biggest)
{
    return biggest>conn.get_blocksize()?conn.get_blocksize():biggest;
}
