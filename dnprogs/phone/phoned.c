/******************************************************************************
    (c) 1999 Christine Caulfield               christine.caulfield@googlemail.com
    
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
// phoned.c
// Phone processing daemon for Linux
////

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <netdnet/dn.h>

#include "common.h"
#include "phone_server.h"
#include "phoned.h"

void sigchild(int s);
void sigterm(int s);
void clear_old_sockets(void);
int open_server_socket(void);
int open_client_socket(void);

// Global variables.
static int verbosity = 0;
static volatile int do_shutdown = 0;
struct fd_array fdarray[MAX_CONNECTIONS];
uid_t  unpriv_user = 65535;

void usage(char *prog, FILE *f)
{
    fprintf(f,"\n%s options:\n", prog);
    fprintf(f," -v        Verbose messages\n");
    fprintf(f," -u <user> Username to run as. Defaults to 'nobody'\n");
    fprintf(f," -h        Show this help text\n");
    fprintf(f," -d        Debug - don't do initial fork\n");
    fprintf(f," -V        Show version number\n\n");
}


// Start here...
int main(int argc, char *argv[])
{
    pid_t              pid;
    char               opt;
    struct sockaddr_dn sockaddr;
    struct optdata_dn  optdata;
    int		       debug=0;
    int                status;
    struct passwd      *pwd;
    char               *user="nobody";
    int                len = sizeof(sockaddr);
    struct             sigaction siga;
    sigset_t           ss;

    // Set up syslog logging
    openlog("phoned", LOG_PID, LOG_DAEMON);

    memset(&fdarray, 0, sizeof(fdarray));
    
    // Deal with command-line arguments. Do these before the check for root
    // so we can check the version number and get help without being root.
    opterr = 0;
    optind = 0;
    while ((opt=getopt(argc,argv,"?vu:Vhd")) != EOF)
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

	case 'u':
	    user = optarg;
	    break;

	case 'V':
	    printf("\nphoned from dnprogs version %s\n\n", VERSION);
	    exit(1);
	    break;
	}
    }

    if (getuid() != 0)
    {
	fprintf(stderr, "Must be root to run phone daemon\n");
	exit(2);
    }

#ifndef NO_FORK
    if (!debug)
    {
	switch(pid=fork())
	{
	case -1:
	    perror("phoned cannot fork");
	    exit(2);
	
	case 0:
	    setsid();
	    close(0); close(1); close(2);
	    chdir("/");
	    break;
	
	default:
	    if (verbosity) syslog(LOG_INFO, "forked process %d\n", pid);
	    exit(0);
	}
    }
#endif
    
    // Set up signal handlers.
    signal(SIGHUP,  SIG_IGN);

    sigemptyset(&ss);
    siga.sa_handler=sigchild;
    siga.sa_mask  = ss;
    siga.sa_flags = 0;
    sigaction(SIGCHLD, &siga, NULL);

    siga.sa_handler=sigterm;
    sigaction(SIGTERM, &siga, NULL);
    
    // Open the sockets
    fdarray[0].fd   = open_client_socket();
    fdarray[0].type = UNIX_RENDEZVOUS;
    fdarray[1].fd   = open_server_socket();
    fdarray[1].type = DECNET_RENDEZVOUS;

    // Become the unprivileged user
    if ( (pwd = getpwnam(user)) )
    {
	seteuid(pwd->pw_uid);
	setegid(pwd->pw_gid);
	if (verbosity) syslog(LOG_INFO, "becoming user '%s'\n", user);
	unpriv_user = pwd->pw_uid;
    }
    else
    {
	syslog(LOG_WARNING, "User '%s' does not exist: running as root\n", user);
    }

    // Main loop.
    do
    {
	fd_set fds;
	int    i;
	    
	FD_ZERO(&fds);
	for (i=0; i<MAX_CONNECTIONS; i++)
	{
	    if (fdarray[i].type != INACTIVE)
		FD_SET(fdarray[i].fd, &fds);
	}
	
	status = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
	if (status <= 0)
	{
	    if (errno != EINTR)
	    {
		perror("Error in select");
		do_shutdown = 1;
	    }
	}
	else
	{
	    // Unix will never scale while this is necessary
	    for (i=0; i<MAX_CONNECTIONS; i++)
	    {
		if (FD_ISSET(fdarray[i].fd, &fds))
		{
		    if (verbosity>0 && fdarray[i].type!=INACTIVE) syslog(LOG_INFO, "PHONED: connection socket type %d\n", fdarray[i].type);
		    switch (fdarray[i].type)
		    {
		    case INACTIVE:
			break; // do nothing;
		    case UNIX_RENDEZVOUS:
			accept_unix(i);
			break;
		    case DECNET_RENDEZVOUS:
			accept_decnet(i);
			break;	
		    case UNIX:
			read_unix(i);
			break;
		    case DECNET:
			read_decnet(i);
			break;
		    }
		}
	    }
	}
    }
    while(!do_shutdown);
    syslog(LOG_INFO, "closing down\n");
    return 0;
}


// Catch child process shutdown
void sigchild(int s)
{
    int status, pid;

    // Make sure we reap all children
    do 
    { 
	pid = waitpid(-1, &status, WNOHANG); 
    }
    while (pid > 0);
}

// Catch termination signal
void sigterm(int s)
{
    syslog(LOG_INFO, "SIGTERM caught, going down\n");
    do_shutdown = 1;
}


// Create a UNIX domain socket to talk to the user processes
// It has to be a socket so we can pass a file-descriptor down it.
int open_client_socket(void)
{
    int user_pipe;
    struct sockaddr_un sockaddr;
    
    unlink(SOCKETNAME);
    user_pipe = socket(AF_UNIX, SOCK_STREAM, PF_UNIX);
    if (user_pipe == -1)
    {
	perror("Can't create socket");
	return -1; /* arggh ! */
    }
    fcntl(user_pipe, F_SETFL, fcntl(user_pipe, F_GETFL, 0) | O_NONBLOCK);
    
    strcpy(sockaddr.sun_path, SOCKETNAME);
    sockaddr.sun_family = AF_UNIX;
    if (bind(user_pipe, (struct sockaddr *)&sockaddr, sizeof(sockaddr)))
    {
	perror("can't bind socket");
	return -1;
    }

    chmod(SOCKETNAME, 0666);
    
    if (listen(user_pipe,1) < 0)
    {
	perror("Listen");
	return -1;
    }
    return user_pipe;
}


int open_server_socket()
{
    int sockfd;
    struct sockaddr_dn sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    
    if ((sockfd=socket(AF_DECnet,SOCK_SEQPACKET,DNPROTO_NSP)) == -1) 
    {
	syslog(LOG_ERR, "PHONED: socket failed: %m\n");
	exit(-1);
    }
    
    sockaddr.sdn_family   = AF_DECnet;
    sockaddr.sdn_flags	  = 0x00;
    sockaddr.sdn_objnum	  = PHONE_OBJECT;
    sockaddr.sdn_objnamel = 0x00;

    if (bind(sockfd, (struct sockaddr *)&sockaddr, 
	     sizeof(sockaddr)) < 0) 
    {
	syslog(LOG_ERR, "PHONED: bind failed: %m\n");
	exit(-1);
    }
    
    if (listen(sockfd,1) < 0)
    {
	syslog(LOG_ERR, "PHONED: listen failed: %m\n");
	exit(-1);
    }
//    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

    return sockfd;
}   
