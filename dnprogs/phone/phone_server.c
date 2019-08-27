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
// phone_server.c
// Phone server code. called from phoned.
////
// connection.cc
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <utmp.h>
#include <grp.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "phoned.h"
#include "common.h"

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

static uid_t local_uid;
extern uid_t unpriv_user; // in phoned.c
static int verbose = 1;
static int fd_sd = 0; // used for DIRECTORY

void call_user(int entry, char *buf);
void dial_user(int entry, char *buf, int nomsg);
int  send_fd(int fd_to_send, int pipe);
int  send_directory(int fd);
void remove_entry(int entry);

char *get_local_node(void)
{
    static char local_name[16] = {'\0'};
    char   *dot;
    struct dn_naddr *addr;
    struct nodeent *ne;
    int    i;

    if (local_name[0] == '\0')
    {
        addr = getnodeadd();
	sprintf(local_name, "%s", dnet_htoa(addr));
	
	
	// Make it all upper case
	for (i=0; i<strlen(local_name); i++)
	{
	    if (islower(local_name[i])) local_name[i] = toupper(local_name[i]);
	}
    }
    return local_name;
}


/* Open up a call to the user and see if anyone by that name is
   logged in or not
 */
void call_user(int entry, char *buf)
{
    struct passwd *pw;
    int i;
    char *uptr;
    struct stat st;
    struct sockaddr_un sockaddr;

// Lookup the UID from the username.    
    uptr = strchr((buf+strlen(buf)+1), ':') + 2;
    if (uptr == (char *)2) return; //Gargh!

    for (i = 0; i<=strlen(uptr); i++)
	fdarray[entry].local_login[i] = tolower(uptr[i]);

    if ( !(pw = getpwnam(fdarray[entry].local_login)) )
    {
	char replybuf[1];
	replybuf[0] = PHONE_REPLYNOUSER;
	syslog(LOG_INFO, "user %s does not exist\n",
	       fdarray[entry].local_login);
	write(fdarray[entry].fd, replybuf, 1);
	return;
    }
    if (verbose) syslog(LOG_INFO, "calling user %s (%d)\n",
			fdarray[entry].local_user, pw->pw_uid);
    local_uid = pw->pw_uid;

// First call to dial_user() just sees if the user is logged in.
    dial_user(entry, buf, TRUE);
}

/* Finds out where (if anywhere) the user is logged in and broadcasts
   a message (unless nomsg is set) to tell them that there is a phone
   call for them
 */
void dial_user(int entry, char *buf, int nomsg)
{
    struct utmp ut, *realut;
    char  replybuf[64];
    char *uptr;
    int   found;
    int   sent;

/* Look for the user */
    setutent();
    realut = getutent();
    found = FALSE;
    sent  = FALSE;
    while ((realut=getutent()))
    {
	/* Look for USER processes for the requested user */
	if (realut->ut_type == USER_PROCESS &&
	    memcmp(realut->ut_user, fdarray[entry].local_login,
		   strlen(fdarray[entry].local_login)) == 0)
	{
	    char devname[64];
	    char message[256];
	    int fd;
	    char d[25];
	    time_t t=time(NULL);
	    struct tm tm = *localtime(&t);
	    struct stat st;

	    found = TRUE;

	    /* Send a message to the terminal */
	    strftime(d, sizeof(d), "%H:%M:%S", &tm);
	    sprintf(message, "\n\7%s is phoning you on %s::     (%s)\n",
		    buf+1, get_local_node(), d);
	    
	    sprintf(devname, "/dev/%s", realut->ut_line);
	    
	    // Check if 'mesg' is switched off
	    stat(devname, &st);

	    if (st.st_mode & S_IWGRP)
	    {
		sent = TRUE; // Pretend we have sent it for the purposes
                             // of the initial connection.
		if (!nomsg)
		{
		    seteuid(0);
		    fd = open(devname, O_WRONLY|O_NONBLOCK);
		    if (fd != -1)
		    {
			write(fd, message, strlen(message));
			close(fd);
		    }
		    seteuid(unpriv_user);
		}
	    }
	}
    }
    endutent();
    
    if (!found)
    {
	replybuf[0] = PHONE_REPLYNOUSER;
	syslog(LOG_INFO, "user %s is not logged in\n",
	       fdarray[entry].local_user);
	write(fdarray[entry].fd, replybuf, 1);
    }
    else
    {
	if (!sent)
	{
	    replybuf[0] = PHONE_HANGUP; // All terminals are set 'mesg n'
	    write(fdarray[entry].fd, replybuf, 1);

	    remove_entry(entry);
	}
	else
	{
	    replybuf[0] = PHONE_REPLYOK;
	    write(fdarray[entry].fd, replybuf, 1);
	}
    }
}

// Send a file descriptor to another process.
// This code is largely lifted from Stevens' book.
int send_fd(int fd_to_send, int pipe)
{
    struct iovec  iov[1];
    struct msghdr msg;
    int           res;
    char          buf[2];
    static struct cmsghdr *cmptr = NULL;
#define CONTROLLEN (sizeof(struct cmsghdr) + sizeof(int))

    iov[0].iov_base = buf;
    iov[0].iov_len = 2;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    if (fd_to_send < 0) 
    {
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	buf[1] = -fd_to_send;
	if (buf[1] == 0)  buf[1] = 1;
    }
    else
    {
	if (cmptr == NULL && (cmptr = (struct cmsghdr *)malloc(CONTROLLEN)) == NULL)
	    return -1;
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	cmptr->cmsg_len = CONTROLLEN;
	msg.msg_control = (caddr_t) cmptr;
	msg.msg_controllen = CONTROLLEN;
	*(int *)CMSG_DATA(cmptr) = fd_to_send;
	buf[1] = 0;
    }
    buf[0] = 0;
    
    res = sendmsg(pipe, &msg, 0);
    if (res != 2)
	return -1;
    
    return 0;
}

//
// Send one line of a directory listing back to VMS
//
int send_directory(int fd)
{
    static int first = 1;
    struct utmp ut, *realut;
    char replybuf[64];
    char *uptr;
    int found = 0;

    if (first)
    {
	setutent();
	first = 0;
    }

    while ((realut=getutent()))
    {
	/* Look for USER processes  */
	if (realut->ut_type == USER_PROCESS)
	{
	    char message[256];
	    char proc_name[64];
	    char devname[64];
	    char cmdline[128];
	    char *avail;
	    int  proc_fd;
	    struct stat st;

	    // Get the "process name" from the command line
	    sprintf(proc_name, "/proc/%d/cmdline", realut->ut_pid);
	    proc_fd = open(proc_name, O_RDONLY);
	    if (proc_fd > -1)
	    {
		int len;
		
		len = read(proc_fd, cmdline, sizeof(cmdline));
		cmdline[len] = '\0';
		close(proc_fd);
	    }
	    else
		cmdline[0] = '\0';

	    // If the users tty has group:w then it's available else
	    // it's unplugged.
	    sprintf(devname, "/dev/%s", realut->ut_line);
	    stat(devname, &st);
	    if (st.st_mode & S_IWGRP)
		avail = "available";
	    else
		avail = "/nobroadcast";
	    
	    sprintf(message, "%-15s %-12s    %-12s    %s", cmdline, realut->ut_user, realut->ut_line, avail);
	    write(fd, message, strlen(message));
	    found = 1;
	    return found;
	}
    }
    endutent();
    return found;
}

// Look for a connected client and send it the FD if there is one.
// NOTE: This may send multiple FDs to the client if there is more than one
// person calling
int send_to_client(int entry, int ack_remote)
{
    int i;
    int found = FALSE;

    for (i=0; i<MAX_CONNECTIONS; i++)
    {
	// Look for a matching local user
	if (fdarray[i].type != INACTIVE &&
	    strcmp(fdarray[i].local_user, fdarray[entry].local_user) == 0 &&
	    fdarray[i].type != fdarray[entry].type)
	{
	    char msghead[2];
	    int  decnet_fd, unix_fd;

            // Send the DECnet fd via the unix fd
	    if (fdarray[i].type == DECNET)
	    {
		decnet_fd = i;
		unix_fd   = entry;
	    }
	    else
	    {
		decnet_fd = entry;
		unix_fd   = i;
	    }
	    
	    msghead[0] = strlen(fdarray[decnet_fd].remote_user)+1; // send NUL
	    msghead[1] = ack_remote;

	    write(fdarray[unix_fd].fd, msghead, 2);
	    write(fdarray[unix_fd].fd, fdarray[decnet_fd].remote_user, msghead[0]);
	    send_fd(fdarray[decnet_fd].fd, fdarray[unix_fd].fd);
	    
	    remove_entry(decnet_fd);
	    found = TRUE;
	}
    }
    return found;
}


// Add a new entry into the array
void add_new_fd(int fd, sock_type type)
{
    int i;

    for (i=0; i<MAX_CONNECTIONS; i++)
    {
	if (fdarray[i].type == INACTIVE)
	{
	    fdarray[i].fd   = fd;
	    fdarray[i].type = type;
	    break;
	}
    }
}

// Remove an entry from the array, closing the socket.
void remove_entry(int entry)
{
    close(fdarray[entry].fd);

    fdarray[entry].local_user[0] = '\0';
    fdarray[entry].local_login[0] = '\0';
    fdarray[entry].remote_user[0] = '\0';	    
    fdarray[entry].type = INACTIVE;
}

/* ------------------------------------------------------------------------ */
/*                     Service routines called from phoned                  */
/* ------------------------------------------------------------------------ */

// Accept a new DECnet connection
void accept_decnet(int entry)
{
    int newsock;
    struct sockaddr_dn sockaddr;
    socklen_t len=sizeof(sockaddr);
    
    if ( (newsock=accept(fdarray[entry].fd, (struct sockaddr *)&sockaddr, &len)) < 0)
    {
	syslog(LOG_INFO, "PHONED: accept failed: %m\n");
	return;
    }

//    fcntl(newsock, F_SETFL, fcntl(newsock, F_GETFL, 0) | O_NONBLOCK);

    // Just add it to the list
    add_new_fd(newsock, DECNET);
}

void accept_unix(int entry)
{
    int newsock;
    struct sockaddr_un sockaddr;
    socklen_t len=sizeof(sockaddr);
    
    if ( (newsock=accept(fdarray[entry].fd, (struct sockaddr *)&sockaddr, &len)) < 0)
    {
	syslog(LOG_INFO, "PHONED: accept failed: %m\n");
	return;
    }

    // Just add it to the list
    add_new_fd(newsock, UNIX);
}

void read_decnet(int entry)
{
    char buf[1024];
    int  status;

    if ( (status = read(fdarray[entry].fd, buf, sizeof(buf))) >0 )
    {
	buf[status] = '\0';
	if (verbose) syslog(LOG_INFO, "phone_server: read_decnet %d %d\n",
			    entry, buf[0]);
	usleep(50000);
	switch (buf[0])
	{
	case PHONE_CONNECT:
	    strcpy(fdarray[entry].remote_user, &buf[1]);
	    strcpy(fdarray[entry].local_user, buf+strlen(buf)+1);

	    if (!send_to_client(entry, TRUE))
	    {
		call_user(entry, buf);
	    }
	    break;
	    
	case PHONE_DIAL:
	    if (!send_to_client(entry, TRUE))
	    {
		dial_user(entry, buf, FALSE);
	    }
	    break;
	    
	case PHONE_DIRECTORY:
	  // Because we use getutent/getutline in a loop we could easily
	  // interleave with other clients doing DIRECTORY commands or
	  // just trying to connect. So we fork to make sure that
	  // we have a unique context for getutent.
	  // Yes, this is easier on VMS but this is Unix(sigh!)

	  // The fork code didn't work properly; just send the whole directory
          // it is fast enough, at least with good connections...

	    if (fd_sd==0 || fd_sd==fdarray[entry].fd)
	    {
		fd_sd = fdarray[entry].fd;

	    	if (!send_directory(fdarray[entry].fd))
	    	{
	        	remove_entry(entry);
			fd_sd = 0;
	        }
	    } else {
		// but reply something if not yet available
		char empty = '.';
		write(fdarray[entry].fd, &empty, 1);
	    }
	    break;
	    
	case PHONE_GOODBYE:
	    remove_entry(entry);
	    break;
	}
    }
    else
    {
	syslog(LOG_ERR, "EOF on phone socket\n");
	remove_entry(entry);
    }
}

// The only message received down the UNIX socket is the local username
// preceded by it's length
void read_unix(int entry)
{
    char len;

    if (read(fdarray[entry].fd, &len, 1) <= 0)
    {
	// Client socket was closed
	remove_entry(entry);
	return;
    }
    if (len > 64)
    {
	syslog(LOG_ERR, "Got bad length of %d on phone socket\n", (int)len);
	remove_entry(entry);
	return;
    }
    read(fdarray[entry].fd, fdarray[entry].local_user, len);

    // See if there is a server FD for us
    send_to_client(entry, FALSE);
}
