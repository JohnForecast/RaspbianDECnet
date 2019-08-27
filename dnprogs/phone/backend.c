/*
 * callbacks.c
 *
 * Routines called by the front-end processors
 */
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <ctype.h>
#include <utmp.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "phone.h"
#include "common.h"
#include "backend.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static struct callback_routines cr;

static void do_answer(int reject);
static int  receive_fd(int pipe);
static int  socket_callback(int fd);
static void do_hold(int);
static void do_hangup(void);
static void do_facsimile(char *filename);
static void do_help(void);
static void do_directory(char *node);

int  dial_remote(char *remuser);
int  get_fd_from_userpipe(char *, int);
void send_dial(int sig);
void localsock_callback(int fd);

static int  dial_fd   = -1;
static int  dial_flag = 1;
static char dial_user[64];
static int  dir_sock  = -1;

// Form the local NODE::USER name
char *get_local_name(void)
{
    static char local_name[64] = {'\0'};
    char   *dot;
    struct dn_naddr *addr;
    struct nodeent *ne;
    int    i;

    if (local_name[0] == '\0')
    {
        addr = getnodeadd();
	if (!addr)
		return NULL;

	sprintf(local_name, "%s::%s", dnet_htoa(addr), getenv("LOGNAME"));


	// Make it all upper case
	for (i=0; i<strlen(local_name); i++)
	{
	    if (islower(local_name[i])) local_name[i] = toupper(local_name[i]);
	}
    }
    return local_name;
}

// Same as above but just returns the DECnet node name
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

// Initialise the backend and return the pipe fd
int init_backend()
{
    struct sockaddr_un sockaddr;
    int                user_pipe;
    char               len;
    char              *localname;

    user_pipe = socket(AF_UNIX, SOCK_STREAM, PF_UNIX);
    if (user_pipe == -1)
    {
	perror("Can't create socket");
	return -1; /* arggh ! */
    }

    strcpy(sockaddr.sun_path, SOCKETNAME);
    sockaddr.sun_family = AF_UNIX;
    if (connect(user_pipe, (struct sockaddr *)&sockaddr, sizeof(sockaddr)))
    {
	close(user_pipe);
	return -1;
    }

// Send the local username
    localname = get_local_name();
    if (!localname)
	    return -1;

    len = strlen(localname)+1; // make sure it includes \0
    if (write(user_pipe, &len, 1) == -1 ||
	write(user_pipe, get_local_name(), len) == -1)
    {
	close (user_pipe);
	return -1;
    }

    return user_pipe;
}

/*
 * Register the callback callbacks (call-forwards?) for the back-end
 * routines to register network FDs, show error messages etc.
 */
void register_callbacks(struct callback_routines *new_cr)
{
    cr = *new_cr;
}

/*
 * Called with a phone command in the string
 */
void do_command(char *cmd)
{
    // Clear message line
    cr.show_error(0, "");

    // Exit and quit are synonymous
    if (strcasecmp(cmd, "exit") == 0) { cr.quit(); return; }
    if (strcasecmp(cmd, "quit") == 0) { cr.quit(); return; }

    if (strncasecmp(cmd, "ans", 3) == 0) { do_answer(0); return; }
    if (strncasecmp(cmd, "rej", 3) == 0) { do_answer(1); return; }

    if (strncasecmp(cmd, "hol", 3) == 0) { do_hold(2); return; }
    if (strncasecmp(cmd, "unh", 3) == 0) { do_hold(0); return; }
    if (strncasecmp(cmd, "han", 3) == 0) { do_hangup(); return; }
    if (strncasecmp(cmd, "hel", 3) == 0) { do_help(); return; }

    // DIRECTORY command
    if (strncasecmp(cmd, "dir", 3) == 0)
    {
	char *space = rindex(cmd, ' ');
	if (space)
	{
	    do_directory(space+1);
	    return;
	}
	else
	{
	    cr.show_error(1, "The syntax is DIR node[::]");
	    return;
	}
    }


    // DIAL or PHONE command
    if (strncasecmp(cmd, "dia", 3) == 0 ||
	strncasecmp(cmd, "pho", 3) == 0)
    {
	char *space = rindex(cmd, ' ');
	if (space)
	{
	    dial_remote(space+1);
	    return;
	}
	else
	{
	    cr.show_error(1, "The syntax is DIAL node::user");
	    return;
	}
    }

    // FACSIMILE command
    if (strncasecmp(cmd, "fac", 3) == 0)
    {
	char *space = rindex(cmd, ' ');
	if (space)
	{
	    do_facsimile(space+1);
	    return;
	}
	else
	{
	    cr.show_error(1, "The syntax is FACSIMILE <filename>");
	    return;
	}
    }

    // The default command is also DIAL but only if the string contains
    // a double colon
    if (strstr(cmd, "::")) { dial_remote(cmd); return; }

    if (strlen(cmd) > 0)
	cr.show_error(0, "Unknown command");
}

// FACSIMILE command
// Send a file to the remote users(s), also show it in the local window
static void do_facsimile(char *filename)
{
    FILE *f = fopen(filename, "r");
    struct fd_list  fds[6];
    int  i;
    int  num_fds = cr.get_fds(fds);

    if (f)
    {
	char buf[1024];
	while (!feof(f))
	{
	    fgets(buf, sizeof(buf), f);

	    // Convert Unix LF to CR as required by PHONE
	    if (buf[strlen(buf)-1] == '\n')
		buf[strlen(buf)-1] = '\r';

	    // Make sure the line ends in a CR
	    if (buf[strlen(buf)-1] != '\r')
		strcat(buf, "\r");
	    for (i=0; i<num_fds; i++)
	    {
		send_data(fds[i].out_fd, buf, strlen(buf));
	    }
	    cr.write_text(get_local_name(), buf);
	}
	fclose(f);

	// Send the (EOF) string - use correct line-ending for the
	// local copy so it doesn't try to write into the constant.
	cr.write_text(get_local_name(), "(EOF)\n");

	for (i=0; i<num_fds; i++)
	{
	    send_data(fds[i].out_fd, "(EOF)\r", 6);
	}
    }
    else
    {
	cr.show_error(0, "Can't open that file");
    }

}


// Answer an incoming call. This involves establishing an outgoing connection
// to the all remote systems that have unanswered fds
static void do_answer(int reject)
{
    int  in_fd;
    int  out_fd;
    struct sockaddr_dn out_sockaddr;
    unsigned int  len_out_sockaddr = sizeof(struct sockaddr_dn);
    int  len;
    int  i;
    int  found=FALSE;
    char outbuf[128];
    struct fd_list  fds[6];
    int  num_fds = cr.get_fds(fds);

    if (num_fds == 0)
    {
        cr.show_error(0, "No one is calling you now.");
	return;
    }

    for (i=0; i<num_fds; i++)
    {
	if (fds[i].in_fd >-1 && fds[i].out_fd == -1) // Not answered
	{
	    in_fd = fds[i].in_fd;
	    found = TRUE;

            // Make outgoing connection...
	    if ((out_fd = socket(AF_DECnet, SOCK_SEQPACKET, DNPROTO_NSP)) == -1)
	    {
		perror("socket");
		return;
	    }

	    // Connect back to the system that connected to us.
	    memset(&out_sockaddr, 0, sizeof(out_sockaddr));
	    getpeername(in_fd, (struct sockaddr *)&out_sockaddr, &len_out_sockaddr);
	    // Fix bits of the peer name before reconnecting;
	    out_sockaddr.sdn_family    = AF_DECnet;
	    out_sockaddr.sdn_flags     = 0x00;
	    out_sockaddr.sdn_objnum    = PHONE_OBJECT;
	    out_sockaddr.sdn_objnamel  = 0x00;
	    out_sockaddr.sdn_add.a_len = 0x02;
	    if (connect(out_fd, (struct sockaddr *)&out_sockaddr, len_out_sockaddr) < 0)
	    {
		cr.show_error(1, "Cannot connect to caller");
		close(out_fd);
		close(in_fd);

		cr.delete_caller(in_fd);
		return;
	    }

// Send a CALL message
	    outbuf[0] = PHONE_CONNECT;
	    strcpy(outbuf+1, get_local_name());
	    strcpy(outbuf+strlen(outbuf)+1, fds[i].remote_name);
	    if (write(out_fd, outbuf, strlen(outbuf)+strlen(fds[i].remote_name)+1) < 0)
	    {
		cr.show_error(1, "Cannot send connect to caller");
		perror("connect error");
		close(out_fd);
		close(in_fd);
		cr.delete_caller(in_fd);
		return;
	    }

	    // Send ANSWER or REJECT
	    if (reject)
		outbuf[0] = PHONE_REJECT;
	    else
		outbuf[0] = PHONE_ANSWER;

	    strcpy(outbuf+1, get_local_name());
	    if (write(out_fd, outbuf, strlen(outbuf)+1) < 0)
	    {
		cr.show_error(1, "Cannot answer caller");
		close(out_fd);
		close(in_fd);
		cr.delete_caller(in_fd);
		return;
	    }

	    // If we rejected the call then tidy up.
	    if (reject)
	    {
		cr.delete_caller(in_fd);
		close(out_fd);
		close(in_fd);
	    }
	    else
	    {
		cr.answer(in_fd, out_fd);
	    }
	}
    }

    if (!found)
    {
        cr.show_error(0, "No one is calling you now.");
	return;
    }

}

// Called when the backend DECnet socket becomes readable
static int socket_callback(int fd)
{
    char buf[256];
    char msgbuf[256];
    char *remote_name = buf+1;
    int status;

    status = read(fd, buf, sizeof(buf));
    if (status > 0)
    {
	char *text = buf+strlen(buf)+1;
	buf[status] = '\0';

	switch (buf[0])
	{
	case PHONE_DATA:
	    cr.write_text(remote_name, text);
	    break;
	case PHONE_HOLD:
	    cr.hold(1, remote_name);
	    break;
	case PHONE_UNHOLD:
	    cr.hold(0, remote_name);
	    break;
	case PHONE_HANGUP:
	    remote_name = strchr(remote_name, ':')+2;
	    sprintf(msgbuf, "%s just hung up the phone.", remote_name);
	    cr.show_error(0,msgbuf);
	    break;
	case PHONE_REJECT:
	    cr.show_error(0,"That person has rejected your call at this time.");
	    alarm(0);
	    close(dial_fd);
	    dial_fd = -1;
	    break;
	case PHONE_DIAL:
	    {
		char d[25];
		char message[132];
		time_t t=time(NULL);
		struct tm tm = *localtime(&t);

                /* Send a message to the user */
		strftime(d, sizeof(d), "%d-%b-%Y %H:%M:%S", &tm);
		sprintf(message, "\007%s is phoning you on %s::     (%s)", buf+1, get_local_node(), d);
		cr.show_error(0, message);

		d[0] = PHONE_REPLYOK;
		write(fd, d, 1);
	    }
	    break;
	case PHONE_GOODBYE:
	    cr.delete_caller(fd);
	    break;
	case 10: // CC wot's this ??
	    write(fd, "\1", 1);
	    break;
	case PHONE_ANSWER:
	    if (dial_fd != -1)
	    {
	        cr.answer(fd, dial_fd);
		cr.show_error(0, "That person has answered your call.");
	        alarm(0);
		dial_fd = -1;
	    }
	    else
	    {
		syslog(LOG_DEBUG, "got ANSWER but we hadn't dialled anyone!\n");
	    }
	    break;
	}
    }
    else
    {
	// It's gone away
	cr.delete_caller(fd);
    }
    return 0;
}

// Receive a file descriptor from another process.
// This code is largely lifted from Stevens' book.
static int receive_fd(int pipe)
{
    int  nread, status;
    int  newfd = -1;
    char *ptr, buf[2];
    struct iovec  iov[1];
    struct msghdr  msg;
    static struct cmsghdr *cmptr = NULL;
#define CONTROLLEN (sizeof(struct cmsghdr) + sizeof(int))

    status = -1;
    for ( ; ; )
    {
	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	if (cmptr == NULL && (cmptr = (struct cmsghdr *)malloc(CONTROLLEN)) == NULL)
	    return -1;
	msg.msg_control = (caddr_t) cmptr;
	msg.msg_controllen = CONTROLLEN;

	nread = recvmsg(pipe, &msg, 0);

	if (nread <= 0)
	{
	    syslog(LOG_DEBUG, "PHONED: nread: %d\n",nread);
	    return -1;
	}

	for (ptr = buf; ptr < &buf[nread]; )
	{
	    if (*ptr++ == 0) {
		if (ptr != &buf[nread-1])
		{
		    syslog(LOG_DEBUG, "Message format error");
		    return -1;
		}
		status = *ptr & 255;
		if (status == 0)
		{
		    if (msg.msg_controllen != CONTROLLEN)
		    {
			syslog(LOG_DEBUG, "status was 0 but no fd found");
			return -1;
		    }
		    newfd = *(int *)CMSG_DATA(cmptr);
		}
		else
		{
		    newfd = -status;
		}
		nread -= 2;
	    }
	}
	if (nread > 0)
	    return -1;
	if (status >= 0)
	    return newfd;
    }
}

// Put remote users on hold
static void do_hold(int hold)
{
    struct fd_list  fds[6];
    char buf[128];
    int  i;
    int  num_fds = cr.get_fds(fds);

    if (hold)
	buf[0] = PHONE_HOLD;
    else
	buf[0] = PHONE_UNHOLD;
    strcpy(buf+1, get_local_name());

    for (i=0; i< num_fds; i++)
    {
	write(fds[i].out_fd, buf, strlen(buf)+1);
    }

    // Hold/unhold the local user
    if (hold)
	cr.hold(2, get_local_name());
    else
	cr.hold(0, get_local_name());
}


// Hangup all remote users
static void do_hangup()
{
    struct fd_list fds[6];
    int i;
    int num_fds = cr.get_fds(fds);

    for (i=0; i< num_fds; i++)
    {
	close_connection(fds[i].out_fd);
	cr.delete_caller(fds[i].in_fd);
    }
}


// Send data to a remote user
void send_data(int fd, char *text, int len)
{
    char buf[2048];

    buf[0] = PHONE_DATA;
    strcpy(buf+1, get_local_name());
    memcpy(buf+strlen(buf)+1, text, len);

    write(fd, buf, strlen(buf)+1+len);
}


// Close the connection neatly. This means sending a HANGUP and a
// GOODBYE message
void close_connection(int fd)
{
    char buf[128];

    if (fd > -1)
    {
	buf[0] = PHONE_HANGUP;
	strcpy(buf+1, get_local_name());
	write(fd, buf, strlen(buf)+1);

	buf[0] = PHONE_GOODBYE;
	write(fd, buf, strlen(buf)+1);
    }
}

// Called from the ALARM signal handler
void send_dial(int sig)
{
    char buf[128];
    int incoming_fd;

    // Carry on ringing
    if (dial_fd != -1)
    {
	buf[0] = PHONE_DIAL;
	strcpy(buf+1, get_local_name());
	buf[strlen(buf)+1] = dial_flag;
	write(dial_fd, buf, strlen(buf)+2);

	read(dial_fd, buf, 1);
	if (buf[0] == PHONE_HANGUP) // User is /NOBROADCAST
	{
	    close(dial_fd);
	    dial_fd = -1;
	    cr.show_error(0, "That person's phone is unplugged (/NOBROADCAST).");
	    return;
	}

	dial_flag = 0;
	signal(SIGALRM, send_dial);
	alarm(10);
    }
}


// Send a HOLD/UNHOLD message
void send_hold(int held, int fd)
{
    char buf[128];

    if (held)
	buf[0] = PHONE_HOLD;
    else
	buf[0] = PHONE_UNHOLD;

    strcpy(buf+1, get_local_name());
    write(fd, buf, strlen(buf)+1);

    cr.hold(held, get_local_name());
}

// Dial a remote user
int dial_remote(char *remuser)
{
    char  *colons;
    char   node[16];
    char   msg[128];
    char   newuser[128];
    char   buf[64];
    int    sockfd;
    int    i,len;
    struct nodeent      *np;
    struct sockaddr_dn   sockaddr;
    struct timeval timeout = {60, 0};

    colons = strstr(remuser, "::");
    if (!colons || strlen(colons) < 3 || strlen(colons) > 66)
    {
	cr.show_error(1, "username must be in NODE::USER format");
	return -1;
    }

    // This way madness lies, well, a deadlock anyway.
    if (strcasecmp(remuser, get_local_name()) == 0)
    {
	cr.show_error(1, "Talking to yourself is the first sign of madness");
	return -1;
    }

    // Open a connection to the remote host.
    if ((sockfd=socket(AF_DECnet,SOCK_SEQPACKET,DNPROTO_NSP)) == -1)
    {
	cr.show_error(1, "can't get socket for remote connection");
	return -1;
    }

    *colons = '\0';

    if ( (np=getnodebyname(remuser)) == NULL)
    {
	cr.show_error(1, "Cannot resolve remote node name");
	return -1;
    }
    strcpy(node, np->n_name);

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sdn_family    = AF_DECnet;
    sockaddr.sdn_flags	   = 0x00;
    sockaddr.sdn_objnum	   = PHONE_OBJECT;
    sockaddr.sdn_objnamel  = 0x00;
    sockaddr.sdn_add.a_len = 0x02;
    memcpy(sockaddr.sdn_add.a_addr, np->n_addr,2);

    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (connect(sockfd, (struct sockaddr *)&sockaddr,
		sizeof(sockaddr)) < 0)
    {
	cr.show_error(1, "Cannot connect to remote node");
	close(sockfd);
	return -1;
    }

    sprintf(msg, "Ringing %s...              (Press any key to cancel call and continue.)", colons+2);

    cr.show_error(0, msg);

    // If the node name was actually an address then look up the name.
    if (isdigit(node[0]))
    {
	struct nodeent *np2;
	if ( (np2=getnodebyaddr((char*)np->n_addr, 2, AF_DECnet)) != NULL)
	{
	    strcpy(node, np2->n_name);
	}
    }

    // Rebuild the node::user from the real node name and make it all caps
    sprintf(newuser, "%s::%s", node, colons+2);
    for (i=0; i<strlen(newuser); i++)
    {
	if (islower(newuser[i])) newuser[i] = toupper(newuser[i]);
    }

    // Send initial connect message
    msg[0] = PHONE_CONNECT;
    strcpy(msg+1, get_local_name());
    strcpy(msg+strlen(msg)+1, newuser);

    if (write(sockfd, msg, strlen(msg)+strlen(newuser)+1) < 0)
    {
	cr.show_error(1, "Cannot send message to remote node");
	close(sockfd);
	return -1;
    }

    // Get acknowledgement
    if ( (len=read(sockfd, buf, 1)) < 1)
    {
	cr.show_error(1, "Error communicating with remote node");
	close(sockfd);
	return -1;
    }

    // A 6 byte means that user is not available
    if (buf[0] == 6)
    {
	cr.show_error(0, "No one with that name is available at this time.");
	close(sockfd);
	return -1;
    }

    if (buf[0] == 9)
    {
	cr.show_error(0, "That user's phone is unplugged. /NOBROADCAST");
	close(sockfd);
	return -1;
    }

    // We assume it's all OK now - send a DIAL request.
    strcpy(dial_user, remuser);
    dial_fd = sockfd;
    dial_flag = 1;
    send_dial(SIGALRM);

    return 0;
}

// Returns -1 if no pipe is available
int get_fd_from_userpipe(char *inbuf, int user_pipe)
{
    char sockname[32];
    int  incoming_fd;
    char inhead[2];

// Connected to the server, get the connecting username
    if (read(user_pipe, inhead, 2) < 2) return -1;
    if (read(user_pipe, inbuf, inhead[0]) <= 0) return -1;

    incoming_fd = receive_fd(user_pipe);
    if (incoming_fd == -1)
    {
	cr.show_error(1, "Error in communication with daemon");
	return -1;
    }

    // Should we ACK the message ?
    if (inhead[1])
    {
	char replybuf[1];
	replybuf[0] = PHONE_REPLYOK;
	write(incoming_fd, replybuf, 1);
    }

    return incoming_fd;
}


void localsock_callback(int fd)
{
    int remote_fd;
    char buf[1024];

    remote_fd = get_fd_from_userpipe(buf, fd);
    if (remote_fd != -1)
    {
	cr.new_caller(remote_fd, -1, buf, socket_callback);
    }
    else
    {
        // Oh dear!
	cr.lost_server();
    }
}


static void do_help(void)
{
  cr.open_display_window("Phone Help");

  cr.display_line("To type a command while having a conversation you need to type the");
  cr.display_line("'switch-hook' character (default is %) first. This can be changed on the");
  cr.display_line("command line with the -s switch.");
  cr.display_line("");
  cr.display_line("Command are:");
  cr.display_line("");
  cr.display_line("DIAL <node::user>     Call a user");
  cr.display_line("ANSWER                Answer an incoming call");
  cr.display_line("REJECT                Reject an incoming call");
  cr.display_line("EXIT or QUIT          Return the the command prompt");
  cr.display_line("HOLD                  Hold all callers");
  cr.display_line("UNHOLD                Unhold callers");
  cr.display_line("HANGUP                Hangup the phone");
  cr.display_line("DIR <node>            Show users logged into <node>");
  cr.display_line("FACSIMILE <file>      Send <file>");
  cr.display_line("HELP                  Show this help");

  cr.display_line("");
  cr.display_line("press any key to return to phone.");
}

static void do_directory(char *node)
{
    char  *colon;
    char   msg[128];
    char   buf[64];
    int    sockfd;
    int    num_users = 0;
    int    i,len, status;
    struct nodeent      *np;
    struct sockaddr_dn   sockaddr;
    struct timeval timeout = {60, 0};

    // colons are optional but we don't want them
    colon = strstr(node, ":");
    if (colon) *colon = '\0';

    // Open a connection to the remote host.
    if ((sockfd=socket(AF_DECnet,SOCK_SEQPACKET,DNPROTO_NSP)) == -1)
    {
	cr.show_error(1, "can't get socket for remote connection");
	return;
    }

    if ( (np=getnodebyname(node)) == NULL)
    {
	cr.show_error(1, "Cannot resolve remote node name");
	return;
    }

    sockaddr.sdn_family    = AF_DECnet;
    sockaddr.sdn_flags	   = 0x00;
    sockaddr.sdn_objnum	   = PHONE_OBJECT;
    sockaddr.sdn_objnamel  = 0x00;
    sockaddr.sdn_add.a_len = 0x02;
    memcpy(sockaddr.sdn_add.a_addr, np->n_addr,2);

    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (connect(sockfd, (struct sockaddr *)&sockaddr,
		sizeof(sockaddr)) < 0)
    {
	cr.show_error(1, "Cannot connect to remote node");
	close(sockfd);
	return;
    }
    cr.show_error(0,"Press any key to cancel the directory listing and continue.");

    sprintf(buf, "Directory of %s::", node);
    cr.open_display_window(buf);
    cr.display_line("Process Name    User Name       Terminal        Phone Status");
    cr.display_line("");

// Send DIRECTORY request message
    msg[0] = PHONE_DIRECTORY;
    write(sockfd, msg, 1);

// Wait for response or do it non-blocking?(ie properly!)
    while ( (status=read(sockfd, buf, sizeof(buf))) > 0)
    {
        write(sockfd, msg, 1); // Send for next line
        buf[status] = '\0';
	cr.display_line(buf);
	num_users++;
    }
    close(sockfd);
    cr.display_line("");
    sprintf(buf, "%d person%s listed.", num_users, num_users==1?"":"s");
    cr.display_line(buf);
}

void cancel_dial(void)
{
    if (dial_fd != -1)
    {
	close(dial_fd);
	alarm(0);
	dial_fd = -1;
	cr.show_error(0, "Dial was cancelled");
    }
}
