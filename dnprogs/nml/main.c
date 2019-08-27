/******************************************************************************
    (c) 2008 Christine Caulfield            christine.caulfield@googlemail.com

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

extern int process_request(int sock, int verbosity);

// Global variables.
int verbosity = 0;

void usage(char *prog, FILE *f)
{
    fprintf(f,"\n%s options:\n", prog);
    fprintf(f," -v        Verbose messages\n");
    fprintf(f," -h        Show this help text\n");
    fprintf(f," -d        Show debug logging\n");
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

    // Deal with command-line arguments. Do these before the check for root
    // so we can check the version number and get help without being root.
    opterr = 0;
    optind = 0;
    while ((opt=getopt(argc,argv,"?vVdh")) != EOF)
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

	case 'd':
	    debug++;
	    break;

	case 'V':
	    printf("\nnml from dnprogs version %s\n\n", VERSION);
	    exit(1);
	    break;
	}
    }

    // Initialise logging
    init_daemon_logging("nml", debug?'e':'s');

    // Wait for something to happen (or check to see if it already has)
    insock = dnet_daemon(DNOBJECT_NICE, NULL, verbosity, !debug);

    if (insock > -1)
    {
	    /* This sets NML version 4.0, which RSX-11 likes to see */
	    char ver[] = {4, 0, 0};
	    dnet_accept(insock, 0, ver, sizeof(ver));

	    process_request(insock, verbosity);
    }
    return 0;
}
