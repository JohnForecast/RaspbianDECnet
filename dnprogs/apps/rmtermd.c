
/******************************************************************************
    rmtermd -- old decnet remote terminal protocol daemon

    (c) 2000 Paul Koning	ni1d@arrl.net

    Based on ctermd by E.M. Serrat and Christine Caulfield
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*******************************************************************************/

/* 
 * This program implements the old (object 23, pre-Cterm) remote terminal
 * protocol.  That protocol is OS-dependent; there are actually four flavors
 * of it, for RSTS/E, RSX, VMS, and TOPS-20.  The one we have here, 
 * following the example of Ultrix, is the TOPS-20 variant -- which is also
 * the simplest of the bunch.
 * The purpose of this daemon is to allow decnet remote terminal access
 * to Linux from DECnet nodes that do not implement Cterm.  That includes
 * the PDP-11 implementations and possibly TOPS-20 as well.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <ctype.h>
#include <regex.h>
#include <stdlib.h>
#include <utmp.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#ifdef DNETUSE_DEVPTS
#include <pty.h>
#endif

#include "dn_endian.h"

static	struct	sockaddr_dn		sockaddr;
static	char				*line;
static	int				s,t,net,pty,len;
static	int				read_present;
/*-----------------------------------------------------------------------*/
void rmterm_child(int s)
{
	int	sts;
	wait (&sts);
}
/*-----------------------------------------------------------------------*/
void rmterm_bind(void)
{
	/* bind message says we're Ultrix, and speak Tops-20 protocol */
	char	rmterm_bind_msg[] = {0x01,0x01,0x01,0x00,18,0x00,0x08,0x00};

	if (write(net,rmterm_bind_msg,sizeof(rmterm_bind_msg)) < 0)
	{
		perror("Rmterm bind request failed");
		exit (-1);
	}
}
/*-----------------------------------------------------------------------*/
void rmterm_reset(int s)
{
	struct	utmp	entry;
	struct	utmp	*lentry;
	char	*p;

	p=line+sizeof("/dev/")-1;

	setutent();
	memcpy(entry.ut_line,p,strlen(p));
	entry.ut_line[strlen(p)]='\0';
	lentry=getutline(&entry);
	lentry->ut_type=DEAD_PROCESS;
	
	memset(lentry->ut_line,0,UT_LINESIZE);
	memset(lentry->ut_user,0,UT_NAMESIZE);
	lentry->ut_time=0;
	pututline(lentry);

	(void)chmod(line,0666);
	(void)chown(line,0,0);
	*p='p';
	(void)chmod(line,0666);
	(void)chown(line,0,0);
	close(pty);
	close(net);
	exit(1);
}
/*-----------------------------------------------------------------------*/
void rmterm_purge(void)
{
	char	buf[4000];
	char	Control_c = 0x03;
	long	numbytes;

	write(pty,&Control_c,1);
	ioctl(pty,FIONREAD,&numbytes);
	while (numbytes)
	{
	 	read(pty,buf,numbytes);
		ioctl(pty,FIONREAD,&numbytes);
	}
}
/*-----------------------------------------------------------------------*/
void rmterm_write (char *buf)
{
	if (write(net,buf,strlen(buf)) < 0)
	{
		perror("rmterm_write");
		exit(-1);
	}
	
}
/*-----------------------------------------------------------------------*/
void rmterm (void)
{
	char	 buf[100];
	struct	 timeval tv;
	fd_set	 rdfs;
	int	 cnt;
	struct   sigaction siga;
	sigset_t ss;

	setsid();

	sigemptyset(&ss);
	siga.sa_handler=rmterm_reset;
	siga.sa_mask  = ss;
	siga.sa_flags = 0;
	sigaction(SIGCHLD, &siga, NULL);
	
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);

	rmterm_write("RMTERM Version 1.0.3\r\nDECnet for Linux\r\n\n");
	
	for (;;)
	{
		FD_ZERO(&rdfs);
		FD_SET(pty,&rdfs);
		tv.tv_sec = 0;
		tv.tv_usec= 200;
		
		if (select(pty+1,&rdfs,NULL,NULL,&tv) > 0)
		{
			if (FD_ISSET(pty,&rdfs))
			{
				cnt=read(pty,buf,sizeof(buf)-1);
				buf[cnt]='\0';
				rmterm_write(buf);
			}
		}
		FD_ZERO(&rdfs);
		FD_SET(net,&rdfs);
		tv.tv_sec = 0;
		tv.tv_usec= 200;
		
		if (select(net+1,&rdfs,NULL,NULL,&tv) > 0)
		{
			if (FD_ISSET(net,&rdfs))
			{
				cnt=read(net,buf,sizeof(buf));
				if (buf[0] == 3)
					rmterm_purge();
				write(pty,buf,cnt);
			}
		}
	}
	rmterm_reset(0);
}
/*-----------------------------------------------------------------------*/
void doit  (void)
{
	int	i,c;
	struct	stat	stb;
	char	ptyname[] = "/dev/ptyCP";
	int	gotpty =0;

	rmterm_bind();

#ifdef DNETUSE_DEVPTS
 	if (openpty(&pty, &t,NULL, NULL, NULL) == 0)
		gotpty = 1;
	else
	{
		rmterm_write("No ptys available for connection");
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
		rmterm_write("No ptys available for connection");
		exit(-1);
	}


	line[strlen("/dev/")] = 't';
	if ( (t=open(line,O_RDWR)) < 0) 
	{
		rmterm_write("Error connecting to physical terminal");
		exit(-1);
	}
#endif
	if ( fchmod(t,0) ) 
	{
		rmterm_write("Error setting terminal mode");
		exit(-1);
	}

	if ( ( i=fork() ) < 0)
	{
		rmterm_write("Error forking");
		exit(-1);
	}
	if (i) 
	{
		rmterm();
	}

	setsid();
	
	close(pty); close(net);
	if (t != 0) dup2 (t,0);
	if (t != 1) dup2 (t,1);
	if (t != 2) dup2 (t,2);

	if (t > 2) close(t);

	putenv("TERM=vt100");
	execlp("/bin/login","login",(char *)0);
	rmterm_write("Error executing login command");
	exit(-1);
}

void usage(char *prog, FILE *f)
{
	fprintf(f,"\n%s options:\n", prog);
	fprintf(f," -v        Verbose messages\n");
	fprintf(f," -h        Show this help text\n");
	fprintf(f," -d        Run in debug mode - don't do initial fork\n");
	fprintf(f," -l<type>  Logging type(s:syslog, e:stderr, m:mono)\n");
	fprintf(f," -V        Show version number\n\n");
}


/*-----------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
	int verbosity;
	int debug = 0;
	char log_char = 'l';
	char opt;
    
	// Deal with command-line arguments.
	opterr = 0;
	optind = 0;
	while ((opt=getopt(argc,argv,"?vVdhl:")) != EOF)
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
			printf("\nrmtermd from dnprogs version %s\n\n", VERSION);
			exit(1);
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
	init_daemon_logging("rmtermd", log_char);
    
	net = dnet_daemon(DNOBJECT_DTERM, NULL, verbosity, !debug);

	if (net > -1)
	{
		dnet_accept(net, 0, NULL, 0);
		doit();
	}
	return 0;
}
/*-------------------------------------------------------------------------*/
