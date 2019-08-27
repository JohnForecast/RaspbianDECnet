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



/*-------------------------------------------------------------------------*/
static void usage(FILE *f, bool dnprint)
{
    if (dnprint)
    {
	fprintf(f,"\nUSAGE: dnprint [OPTIONS] 'node\"user password\"::filespec'\n\n");
    }
    else
    {
	fprintf(f, "\nUSAGE: dnsubmit [OPTIONS] 'node\"user password\"::filespec'\n\n");
    }
    fprintf(f,"NOTE: The VMS filename really should be in single quotes to\n");
    fprintf(f,"      protect it from the shell\n");

    fprintf(f,"\nOptions:\n");
    fprintf(f,"  -? -h        display this help message\n");
    fprintf(f,"  -T <secs>    connect timeout (default 60)\n");
    fprintf(f,"  -v           increase verbosity\n");
    fprintf(f,"  -V           show version number\n");

    fprintf(f,"\nExample:\n\n");
    if (dnprint)
    {
	fprintf(f," dnprint  'serv1\"user password\"::helloworld.lis'\n");
    }
    else
    {
	fprintf(f," dnsubmit  'serv1\"user password\"::myjob.com'\n");
    }
    fprintf(f,"\n");
}
/*-------------------------------------------------------------------------*/
static int submit(dap_connection &conn, char *dirname, bool print)
{
    dap_attrib_message att;
    dap_access_message acc;

    // When printing we set the SPOOL option on closing the file.
    if (print)
	acc.set_accfunc(dap_access_message::OPEN);
    else
	acc.set_accfunc(dap_access_message::SUBMIT);

    acc.set_filespec(dirname);

    conn.set_blocked(true);
    att.set_file("/", false);
    att.write(conn); // Send empty ATTRIB message
    acc.write(conn);
    return conn.set_blocked(false);
}
/*-------------------------------------------------------------------------*/
static int read_reply(dap_connection &conn)
{
    dap_message *m;

    m = dap_message::read_message(conn, true);
    if (!m) return -1;

    switch (m->get_type())
    {
    case dap_message::ATTRIB:
	break;

    case dap_message::ACK:
	break;

    case dap_message::STATUS:
        {
	    dap_status_message *sm = (dap_status_message *)m;
	    fprintf(stderr, "%s\n", sm->get_message());
	    return -1;
	}

    case dap_message::ACCOMP:
	break;

    case dap_message::NAME:
	break;

    default:
	printf("Unknown mesage received: 0x%x\n", m->get_type());
	return -1;
	break;
    }
    return 0;
}


int main(int argc, char *argv[])
{
    int	    opt,retval;
    int     verbose = 0;
    bool    dnprint = false;
    int     connect_timeout = 60;

    // Work out the command name
    if (strstr(argv[0], "dnprint"))
    {
	dnprint = true;
    }

    if (argc < 2)
    {
	usage(stderr, dnprint);
	exit(0);
    }

/* Get command-line options */
    opterr = 0;
    optind = 0;
    while ((opt=getopt(argc,argv,"?hvVT:")) != EOF)
    {
	switch(opt)
	{
	case 'h':
	    usage(stdout, dnprint);
	    exit(1);

	case '?':
	    usage(stderr, dnprint);
	    exit(1);

	case 'v':
	    verbose++;
	    break;

	case 'T':
	    connect_timeout = atoi(optarg);
	    break;

	case 'V':
	    printf("\ndnsubmit from dnprogs version %s\n\n", VERSION);
	    exit(1);
	    break;
	}
    }
    if (optind >= argc)
    {
	usage(stderr, dnprint);
	exit(2);
    }

    init_logging("dnsubmit", 'e', false);

    dap_connection conn(verbose);
    conn.set_connect_timeout(connect_timeout);

    char dirname[256] = {'\0'};
    if (!conn.connect(argv[optind], dap_connection::FAL_OBJECT, dirname))
    {
	fprintf(stderr, "%s\n", conn.get_error());
	return -1;
    }

    // Exchange config messages
    if (!conn.exchange_config())
    {
	fprintf(stderr, "Error in config: %s\n", conn.get_error());
	return -1;
    }

    if (!submit(conn, dirname, dnprint))
    {
	fprintf(stderr, "Error in opening: %s %s\n", dirname, conn.get_error());
	return -1;
    }

    retval = read_reply(conn);
    if (retval < 0) dnprint=false; // Don't try the rest

    // When printing we have to send an ACCOMP message with the SPL bit set.
    if (dnprint)
    {
	dap_accomp_message acc;

	acc.set_cmpfunc(dap_accomp_message::CLOSE);
	acc.set_fop_bit(dap_attrib_message::FB$SPL);
	acc.write(conn);

	retval = read_reply(conn);
    }

    conn.close();
    return 0;
}
