/******************************************************************************
    (c) 1998-1999 Christine Caulfield               christine.caulfield@googlemail.com
    
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
////
// vmsmaild.c
// VMSmail processing daemon for Linux
////

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <regex.h>
#include <pwd.h>
#include <sys/socket.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "configfile.h"
#include "receive.h"

// Global variables.
int verbosity = 0;
int block_mode = 0;

void usage(char *prog, FILE *f)
{
    fprintf(f,"\n%s options:\n", prog);
    fprintf(f," -v        Verbose messages\n");
    fprintf(f," -h        Show this help text\n");
    fprintf(f," -f        Accept MAIL/FOREIGN\n");
    fprintf(f," -l<type>  Logging type(s:syslog, e:stderr, m:mono)\n");
    fprintf(f," -U        Don't check that reply user exists\n");
    fprintf(f," -V        Show version number\n\n");
}



int main(int argc, char *argv[])
{
    pid_t              pid;
    char               opt;
    struct sockaddr_dn sockaddr;
    struct optdata_dn  optdata;
    int		       insock;
    int                debug;
    int                len = sizeof(sockaddr);
    int                check_user=1;
    char               log_char = 'l'; // Default to syslog(3)
    char               optdata_bytes[] = {  03, 01, 00, 18, 00, 00, 00, 00,
					  0xA0, 02, 00, 00, 01, 00, 00, 00};

    read_configfile();
    
    // Deal with command-line arguments. Do these before the check for root
    // so we can check the version number and get help without being root.
    opterr = 0;
    optind = 0;
    while ((opt=getopt(argc,argv,"?vVdhu:Ufl:")) != EOF)
    {
	switch(opt) 
	{
	case 'h': 
	    usage(argv[0], stdout);
	    exit(0);

	case '?':
	    usage(argv[0], stderr);
	    exit(0);

	case 'v':
	    verbosity++;
	    break;

	case 'f':
	    optdata_bytes[8] |= 0x02;
	    block_mode = 1;
	    break;

	case 'd':
	    debug++;
	    break;

	case 'V':
	    printf("\nvmsmaild from dnprogs version %s\n\n", VERSION);
	    exit(1);
	    break;

	case 'U':
	    check_user=0;
	    break;

	case 'u':
	    strcpy(config_vmsmailuser, optarg);
	    break;

	case 'l':
	    if (optarg[0] != 's' &&
		optarg[0] != 'm' &&
		optarg[0] != 'e')
	    {
		usage(argv[0], stderr);
		exit(2);
	    }
	    log_char = optarg[0];
	    break;

	}
    }

    // Initialise logging
    init_daemon_logging("vmsmaild", log_char);

    // See if the vmsmail user exists on this system
    if (check_user)
    {
	if (!getpwnam(config_vmsmailuser))
	{
	    DNETLOG((LOG_ERR, "user for vmsmail (%s) does not exist, change the user in /etc/vmsmail.conf or use the -U option to get rid of this message\n",
		   config_vmsmailuser));
	}
    }

    // Wait for something to happen (or check to see if it already has)
    insock = dnet_daemon(DNOBJECT_MAIL11, NULL, verbosity, !debug);
 
    if (insock > -1)
    {
        dnet_accept(insock, 0, optdata_bytes, sizeof(optdata_bytes));
	receive_mail(insock);
    }
    return 0;
}
