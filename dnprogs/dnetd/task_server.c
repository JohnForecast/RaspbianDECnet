/******************************************************************************
    (c) 1999-2002 Christine Caulfield               christine.caulfield@googlemail.com

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
// task_server.c
// Task processing module
////

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#ifdef DNETUSE_DEVPTS
#include <pty.h>
#endif
#include "dn_endian.h"

static void execute_file(char *name, int newsock, int verbose);
static void copy (int pty, int sock, pid_t pid);

void task_server(int newsock, int verbosity, int secure)
{
    struct sockaddr_dn sockaddr;
    char name[200];
    char tryname[PATH_MAX];
    char *taskdir;
    struct stat st;
    socklen_t len = sizeof(sockaddr);
    int i;

    /* Get the object that was wanted */
    getsockname(newsock, (struct sockaddr *)&sockaddr, &len);
    if (sockaddr.sdn_objnamel)
    {

	strncpy(name, (char*)sockaddr.sdn_objname, dn_ntohs(sockaddr.sdn_objnamel));
	name[dn_ntohs(sockaddr.sdn_objnamel)] = '\0';
    }
    else
    {
	DNETLOG((LOG_WARNING, "connection to object number %d rejected\n", sockaddr.sdn_objnum));
	close(newsock);
	return; // Don't do object numbers !
    }

// Convert the name to lower case
    for (i=0; i<strlen(name); i++)
    {
	if (isupper(name[i])) name[i] = tolower(name[i]);
    }


    if (verbosity) DNETLOG((LOG_INFO, "got connection for %s\n", name));

    // Reject anything starting / or containing .. as
    // a security problem
    if (name[0] == '/' ||
	strstr(name, ".."))
    {
	DNETLOG((LOG_WARNING, "Rejecting %s as a security risk", name));
	dnet_reject(newsock, DNSTAT_OBJECT, NULL, 0);
	return;
    }


    // Look for the file.
    //
    // a) in the home directory (if not secure)
    if (!secure)
    {
	if (!stat(name, &st) && (st.st_mode & S_IXUSR))
	{
	    execute_file(name, newsock, verbosity);
	    return;
	}
    }

    // b) in $DNTASKDIR
    taskdir = getenv("DNTASKDIR");
    if (taskdir)
    {
	strcpy(tryname, taskdir);
	strcat(tryname, "/");
	strcat(tryname, name);
	if (!stat(tryname, &st) && (st.st_mode & S_IXUSR))
	{
	    execute_file(tryname, newsock, verbosity);
	    return;
	}
    }

    // c) in /usr/local/decnet/tasks
    strcpy(tryname, "/usr/local/decnet/tasks/");
    strcat(tryname, name);
    if (!stat(tryname, &st) && (st.st_mode & S_IXUSR))
    {
	execute_file(tryname, newsock, verbosity);
	return;
    }

// Can't find it.
    DNETLOG((LOG_INFO, "Unable to execute task %s, file was not found", name));
    dnet_reject(newsock, DNSTAT_OBJECT, NULL, 0);
}
/*
 * All of the mucking about with ptys is to do the (apparently)
 * trivial task of converting LF line endings into CRLF
 * line endings!
 */
static void execute_file(char *name, int newsock, int verbose)
{
    int	        i,c, t;
    pid_t       pid;
    int         pty;
    struct stat	stb;
    char	ptyname[] = "/dev/ptyCP";
    int	        gotpty =0;
    char       *line;
    char       *argv[2] = {name, NULL};
    char       *env[2] = {NULL};

    if (verbose) DNETLOG((LOG_INFO, "About to exec %s\n", name));
    dnet_accept(newsock, 0, NULL, 0);

#ifdef DNETUSE_DEVPTS
    if (openpty(&pty, &t,NULL, NULL, NULL) != 0)
    {
	DNETLOG((LOG_ERR, "No ptys available for object execution"));
	exit(-1);
    }
#else

    for (c='p'; c <= 'z'; c++)
    {
	line = ptyname;
	line[strlen("/dev/pty")] = c;
	line[strlen("/dev/ptyC")] = '0';
	if (stat(line,&stb) < 0)
	    break;
	for (i=0; i < 16; i++)
	{
	    line[strlen("/dev/ptyC")]= "0123456789abcdef"[i];
	    if ( (pty=open(line,O_RDWR)) > 0)
	    {
		gotpty = 1;
		break;
	    }
	}
	if (gotpty) break;
    }

    if (!gotpty)
    {
	DNETLOG((LOG_ERR, "No ptys available for connection"));
	return;
    }


    line[strlen("/dev/")] = 't';
    if ( (t=open(line,O_RDWR)) < 0)
    {
	DNETLOG((LOG_ERR, "Error connecting to physical terminal: %m"));
	return;
    }
#endif

    if ( ( pid=fork() ) < 0)
    {
	DNETLOG((LOG_ERR, "Error forking"));
	return;
    }

    if (pid)  // Parent
    {
	close(t); // close slave

	// do copy from pty to socket.
	copy(pty, newsock, pid);
	return ;
    }

    setsid();

    close(pty); close(newsock);
    if (t != 0) dup2 (t,0);
    if (t != 1) dup2 (t,1);
    if (t != 2) dup2 (t,2);

    if (t > 2) close(t);

    putenv("TERM=vt100");
    execve(name, argv, env);
    DNETLOG((LOG_ERR, "Error executing command"));
    return;
}


// Catch child process shutdown
static void sigchild(int s)
{
    int status, pid;

    // Make sure we reap all children
    do
    {
	pid = waitpid(-1, &status, WNOHANG);
    }
    while (pid > 0);
}

// Just copy stuff from the slave pty to the DECnet socket
static void copy (int pty, int sock, pid_t pid)
{
    char	buf[1024];
    fd_set	rdfs;
    int	        cnt;
    struct      sigaction siga;
    sigset_t    ss;
    char        *s, *bp;

    setsid();

    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    sigemptyset(&ss);
    siga.sa_handler=sigchild;
    siga.sa_mask  = ss;
    siga.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &siga, NULL);

    for (;;)
    {
	FD_ZERO(&rdfs);
	FD_SET(pty,&rdfs);
	FD_SET(sock,&rdfs);

	if (select(FD_SETSIZE,&rdfs,NULL,NULL,NULL) > 0)
	{
	    if (FD_ISSET(pty,&rdfs))
	    {
		cnt=read(pty,buf,sizeof(buf));
		if (cnt <= 0) goto finished;
		buf[cnt] = '\0';
		bp = &(buf[0]);
		s = strsep(&bp, "\r\n");
		if (bp) if (*bp) bp++;
		while (s && bp) {
		    cnt=strlen(s);
		    if (cnt==0) cnt=1; /* output empty line */
		    cnt=write(sock, s, cnt);
		    s = strsep(&bp, "\r\n");
		    if (bp) if (*bp) bp++;
		}
	    }
	    if (FD_ISSET(sock,&rdfs))
	    {
		cnt=read(sock,buf,sizeof(buf));
		if (cnt <= 0) goto finished;
		write(pty, buf, cnt);
	    }
	}
	else
	{
	    DNETLOG((LOG_INFO, "Error in select: %m"));
	    goto finished;
	}
    }

 finished:
    buf[0] = -128; /* as in nml.c, to flush, but here probably a hack */
    write(sock, buf, 1);
    DNETLOG((LOG_INFO, "Task completed"));
    close(pty);
    close(sock);
    kill(pid, SIGTERM);
    exit(0);
}

