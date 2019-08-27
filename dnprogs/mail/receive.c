/******************************************************************************
    (c) 1998-2000 Christine Caulfield               christine.caulfield@googlemail.com
    
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <syslog.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
// Horrible hack for glibc 2.1+ which defines getnodebyname
#if (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1) || __GLIBC__ >= 3
#define getnodebyname ipv6_getnodebyname
#endif

#include <netdb.h>

#if (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1) || __GLIBC__ >= 3
#undef getnodebyname 
#endif

#include <netdnet/dnetdb.h>
#include <netdnet/dn.h>
#include <netinet/in.h>
#include <uudeview.h>
#include <dn_endian.h>
#include "configfile.h"

#ifndef SENDMAIL_COMMAND
#define SENDMAIL_COMMAND "/usr/sbin/sendmail -oem"
#endif

#define FB$UDF   0
#define FB$FIX   1
#define FB$VAR   2
#define FB$VFC   3
#define FB$STM   4
#define FB$STMLF 5
#define FB$STMCR 6

//RATs:
#define FB$FTN   0
#define FB$CR    1
#define FB$PRN   2
#define FB$BLK   3
#define FB$LSA   6

// should perhaps be included from ../libdap/protocol.h somehow
static const int OS_VAXVMS  =  7;

#define read(x,y,z) dnet_recv(x,y,z,MSG_EOR)

struct config_data
{
    unsigned char protocol_ver;
    unsigned char eco;
    unsigned char custeco;
    unsigned char os;
    unsigned int  options; /* Little-endian */
    unsigned int  iomode;  /* Little-endian */
    unsigned char rfm;
    unsigned char rat;
};
extern int block_mode;

char response[1024];
int send_smtp(int sock,
	      char *addressees,
	      char *cc_addressees,
	      char *remote_hostname,
	      char *remote_user,
	      char *subject,
	      char *full_user
	      );

int send_smail(int sock,
	       char *addressees,
	       char *cc_addressees,
	       char *remote_hostname,
	       char *remote_user,
	       char *subject,
	       char *full_user
	      );
int send_body(int dnsock, FILE *unixfile);

int smtp_response(FILE *sockstream);
int smtp_error(FILE *sockstream);


extern int verbosity;
static int is_binary = 0;
static struct config_data *config;

// Receive a message from VMS and pipe it into sendmail
void receive_mail(int sock)
{
    char   remote_user[256]; // VMS only sends 12 but...just in case!
    char   local_user[256];
    char   addressees[65536];
    char   cc_addressees[65536];
    char   full_user[256];
    char   subject[256];
    char   remote_hostname[256];
    struct sockaddr_dn sockaddr;
    struct optdata_dn  optdata;
    int    num_addressees=0;
    int    i;
    int    stat;
    socklen_t namlen = sizeof(sockaddr);
    socklen_t optlen = sizeof(optdata);

    // See if we are being sent binary data.
    // We classify binary data as anything with fixed length records
    // or an undefined record type.

#ifdef DSO_CONDATA
    config=(struct config_data *)optdata.opt_data;
    getsockopt(sock, DNPROTO_NSP, DSO_CONDATA, &optdata, &optlen);
    if (config->rfm == FB$FIX || config->rfm == FB$UDF)
    {
	fprintf(stderr, "rfm=%d, rat=%d\n", config->rfm, config->rat);
	is_binary = 1;
    }
#else
    config->rfm=FB$VAR;
#endif

    if (config->os != OS_VAXVMS) is_binary = 0;

    // Get the remote host name
    stat = getpeername(sock, (struct sockaddr *)&sockaddr, &namlen);
    if (!stat)
    {
        strcpy(remote_hostname, dnet_htoa(&sockaddr.sdn_add));
    }
    else
    {
	sprintf(remote_hostname, "%d.%d",
		(sockaddr.sdn_add.a_addr[1] >> 2),
		(((sockaddr.sdn_add.a_addr[1] & 0x03) << 8) |
		 sockaddr.sdn_add.a_addr[0]));
    }
    
    // Read the remote user name - this comes in padded with spaces to
    // 12 characters
    stat = read(sock, remote_user, sizeof(remote_user)); 
    if (stat < 0)
    {
	DNETLOG((LOG_ERR, "Error reading remote user: %m\n"));
	return;
    }

    // Trim the remote user.
    remote_user[stat] = '\0'; // Just in case its the full buffer
    i=stat;
    while (remote_user[i] == ' ') remote_user[i--] = '\0';
    
    // The rest of the message should be the local user names. These could
    // be a real local user or an internet mail name. They are not
    // padded.
    addressees[0] = '\0';
    do
    {
	stat = read(sock, local_user, sizeof(local_user));
	if (stat == -1)
	{
	    DNETLOG((LOG_ERR, "Error reading local user: %m\n"));
	    return;
	}
	if (local_user[0] != '\0')
	{
	    local_user[stat] = '\0';
	    DNETLOG((LOG_DEBUG, "got local user: %s\n", local_user));
	    strcat(addressees, local_user);
	    strcat(addressees, ",");
	    
	    // Send acknowledge
	    write(sock, "\001\000\000\000", 4);
	    num_addressees++;
	}
    }
    while (local_user[0] != '\0');

    // Remove trailing comma
    addressees[strlen(addressees)-1] = '\0';

    // TODO: This should be more intelligent and only lower-case the
    // addressable part of the email name.
    for (i=0; i<strlen(addressees); i++)
	addressees[i] = tolower(addressees[i]);
    
    // This is the collected list of users to send the message to,
    // as entered by the user.
    // We enter it in an extension header
    stat = read(sock, full_user, sizeof(full_user));
    if (stat == -1)
    {
	DNETLOG((LOG_ERR, "Error reading full_user: %m\n"));
	return;
    }
    full_user[stat] = '\0';

    cc_addressees[0] = '\0';

#ifdef DSO_CONDATA 
    if (config->os == OS_VAXVMS) {
    stat = read(sock, cc_addressees, sizeof(cc_addressees));
    cc_addressees[stat] = '\0';
    for (i=0; i<strlen(cc_addressees); i++)
	cc_addressees[i] = tolower(cc_addressees[i]);
    }
#endif

    // Get the subject
    stat = read(sock, subject, sizeof(subject));
    if (stat == -1)
    {
	DNETLOG((LOG_ERR, "Error reading subject: %m\n"));
	return;
    }
    subject[stat] = '\0';

/* Not sure about this so just swallow it */

    if (config->os == OS_VAXVMS)

    { char bcc[255];
    stat = read(sock, bcc, sizeof(bcc));
    bcc[stat]='\0';
    }

    DNETLOG((LOG_INFO, "Forwarding mail from %s::%s to %s (n=%d)\n",
	     remote_hostname, remote_user, addressees, num_addressees));

// Decide on sendmail or SMTP

    if (config_smtphost[0] == '\0')
	stat = send_smail(sock, addressees, cc_addressees,
			  remote_hostname, remote_user,
			  subject, full_user);
    else
	stat = send_smtp(sock, addressees, cc_addressees,
			 remote_hostname, remote_user,
			 subject, full_user);
	
    DNETLOG((LOG_INFO, "Forwarded mail? %d %s\n", stat, config_smtphost));

    if (stat == 0)
    {
	// Send acknowledge - one for each addressee
	for (i=0; i<num_addressees; i++)
	{
	    write(sock, "\001\000\000\000", 4);
	}
    }
    close(sock);
}

int send_smtp(int sock,
	      char *addressees,
	      char *cc_addressees,
	      char *remote_hostname,
	      char *remote_user,
	      char *subject,
	      char *full_user
	      )
{
    FILE              *sockfile;
    struct sockaddr_in serv_addr;
    struct hostent    *host_info;
    struct servent    *servent;
    int                port;
    char              *addr;
    int                stat;
    int                numbytes;
    int                smtpsock;

    smtpsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (smtpsock == -1)
    {
	DNETLOG((LOG_ERR, "Can't open socket for SMTP: %m\n"));
	return -1;
    }
    
    host_info = gethostbyname(config_smtphost);
    if (host_info == NULL)
    {
        DNETLOG((LOG_ERR, "Cannot resolve host name: %s\n", config_smtphost));
        return -1;
    }
    
    servent = getservbyname("smtp", "tcp");
    if (servent != NULL)
    {
	port = ntohs(servent->s_port);
    }
    if (port == 0) port = 25;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    serv_addr.sin_addr.s_addr = *(int*)host_info->h_addr_list[0];
    
    if (connect(smtpsock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    {
	DNETLOG((LOG_ERR, "Cannot connect to SMTP server: %m\n"));
	return -1;
    }

/* Send initial SMTP commands and swallow responses */
    sockfile = fdopen(smtpsock, "a+");
    if (smtp_response(sockfile) != 220) return smtp_error(sockfile);
	
    fprintf(sockfile, "HELO %s\n", config_hostname);
    stat = smtp_response(sockfile);
    if (stat == 220)
	stat = smtp_response(sockfile);
    if (stat != 250) return smtp_error(sockfile);
    
    fprintf(sockfile, "MAIL FROM: <%s@%s>\n",
	    config_vmsmailuser, config_hostname);
    if (smtp_response(sockfile) != 250) return smtp_error(sockfile);

    addr = strtok(addressees, ",");
    while(addr)
    {
	fprintf(sockfile, "RCPT TO:<%s>\n", addr);
	if (smtp_response(sockfile) != 250) return smtp_error(sockfile);
	
	addr = strtok(NULL, ",");
    }

    addr = strtok(cc_addressees, ",");
    while(addr)
    {
	fprintf(sockfile, "RCPT TO:<%s>\n", addr);
	if (smtp_response(sockfile) != 250) return smtp_error(sockfile);
	
	addr = strtok(NULL, ",");
    }
    
    fprintf(sockfile, "DATA\n");
    if (smtp_response(sockfile) != 354) return smtp_error(sockfile);

    // Send the header
    fprintf(sockfile, "From: %s@%s (%s::%s)\n",
	    config_vmsmailuser, config_hostname, remote_hostname,
	    remote_user);
    fprintf(sockfile, "Subject: %s\n", subject);
    fprintf(sockfile, "To: %s\n", addressees);
    fprintf(sockfile, "Cc: %s\n", cc_addressees);
    if (is_binary)
    {
	fprintf(sockfile, "Mime-Version: 1.0\n");
	fprintf(sockfile, "Content-Type: application/octet-stream\n");
	fprintf(sockfile, "Content-Transfer-Encoding: base64\n");
    }
    fprintf(sockfile, "X-VMSmail: %s\n", full_user);
    
    fprintf(sockfile, "\n");
    
    send_body(sock, sockfile);

    fprintf(sockfile, ".\n");
    if (smtp_response(sockfile) != 250) return smtp_error(sockfile);
    
    fprintf(sockfile, "QUIT\n");
    if (smtp_response(sockfile) != 221) return smtp_error(sockfile);

    fclose(sockfile);
    return 0;
}


int send_smail(int sock,
	       char *addressees,
	       char *cc_addressees,
	       char *remote_hostname,
	       char *remote_user,
	       char *subject,
	       char *full_user
    )
{
    char   buf[65536];
    FILE  *mailpipe;
    int    stat;
     
    // Open a pipe to sendmail.
    sprintf(buf, "%s '%s'" , SENDMAIL_COMMAND, addressees);
    mailpipe = popen(buf, "w");
    if (mailpipe != NULL)
    {
	// Send the header
	fprintf(mailpipe, "From: %s@%s (%s::%s)\n",
		config_vmsmailuser, config_hostname, remote_hostname,
		remote_user);
	fprintf(mailpipe, "Subject: %s\n", subject);
	fprintf(mailpipe, "To: %s\n", addressees);
	fprintf(mailpipe, "Cc: %s\n", cc_addressees);
	if (is_binary)
	{
	    fprintf(mailpipe, "Mime-Version: 1.0\n");
	    fprintf(mailpipe, "Content-Type: application/octet-stream\n");
	    fprintf(mailpipe, "Content-Transfer-Encoding: base64\n");
	    fprintf(mailpipe, "Content-Disposition: inline\n");
	}
	fprintf(mailpipe, "X-VMSmail: %s\n", full_user);

	fprintf(mailpipe, "\n");

	send_body(sock, mailpipe);
	
	pclose(mailpipe);
    }
    else
    {
	DNETLOG((LOG_ERR, "Can't open pipe to sendmail: %m\n"));
	return -1;
    }
    return 0;
}


/*
  Read a response from the MTA and return the code number
  and the text of the response.
 */
int smtp_response(FILE *sockstream)
{
    int   status;
    int   len;
    char  codestring[5];

    if (fgets(response, sizeof(response), sockstream) == NULL)
    {
	strcpy(response, strerror(errno));
	return 0;
    }
    
    strncpy(codestring, response, 3);
    codestring[3] = '\0';
    status = atoi(codestring);

    return status;
}

int smtp_error(FILE *sockstream)
{
    DNETLOG((LOG_ERR, "SMTP Error: %s\n", response));
    fclose(sockstream);
    return -1;
}

int send_with_nlreplacement(FILE *f, char *buf, int len, char nlchar)
{
    int i,j;
    char newbuf[len];

    for (i=0, j=0; i<len; i++)
    {
	if (buf[i] == nlchar)
	    newbuf[j++] = '\n';
	else
	    newbuf[j++] = buf[i];	    
    }
    return fwrite(newbuf, j, 1, f);    
}

/* Convert RMS formatted text to Unix StreamLF */
int write_text(FILE *f, char *buf, int len)
{
    int reclen;
    int done,i;
    char *ptr = buf;
    char nlchar;
    static int carry = 0;
    
    if (block_mode)
    {
	switch (config->rfm)
	{
	case FB$UDF:
	case FB$FIX:
	case FB$STMLF:
	default:
	    return fwrite(buf, len, 1, f);
	    break;
	    
	case FB$VAR:
	case FB$VFC:
	    done = 0;
	    if (carry)
	    {
		fprintf(f, "%.*s\n", carry, ptr);
		ptr += carry;
		if (carry%2) ptr++;
		carry = 0;
	    }

	    /* Process the buffer */
	    while (done < len)
	    {
		reclen = dn_ntohs(*(unsigned short *)ptr);
		ptr++;
		ptr++;
		/* VFC files have two extra bytes at the start of each
		   record */
		if (config->rfm == FB$VFC)
		{
		    reclen -= 2;
		    ptr += 2;
		}

		/* Check for end of block */
		if ((ptr-buf)+reclen > len)
		{
		    carry = (done+reclen) - len + 4;
		    reclen = len-done-2;
		    fprintf(f, "%.*s", reclen, ptr);
		    return 0;
		}
		fprintf(f, "%.*s\n", reclen, ptr);
		
		/* Records are word-padded */
		if (reclen%2) reclen++;
		ptr += reclen;
		done = ptr-buf;
	    }
	    break;
	    
	case FB$STMCR:
	    send_with_nlreplacement(f, buf, len, '\r');
	    break;
	}
    }
    else
    {
	fprintf(f, "%.*s\n", len, buf);
    }
    return 0;
}

// Sends the message body, converting binary messages to MIME if necessary
int send_body(int dnsock, FILE *unixfile)
{
    char  buf[65535];
    int   stat, i;
    int   finished = 0;
    
    // Get the text of the message
    if (!is_binary)
    {
	do
	{
	    stat = read(dnsock, buf, sizeof(buf));
	    DNETLOG((LOG_DEBUG, "body: %s %d\n", buf, stat));
            if (stat == -1 && errno == EINTR)
                continue;

	    if (stat == -1 && errno != ENOTCONN)
	    {
		DNETLOG((LOG_ERR, "Error reading message text: %m\n"));
		return -1;
	    }

	    // VMS sends a lone NUL as the end of the message
	    if ((stat == 1 && buf[0] == '\0')) finished = 1;
	    if (stat > 0 && !finished)
	    {
		/* Interpret text format*/
		write_text(unixfile, buf, stat);
	    }
	}
	while (stat >= 0 && !finished);
    }
    else
    {
	int len;
	char tempname[PATH_MAX];
	int tempfile;

	sprintf(tempname, "/tmp/vmsmailXXXXXX");
	tempfile = mkstemp(tempname);
	if (tempfile < 0)
	{
	    DNETLOG((LOG_ERR, "Failed to make temp file: %s\n", strerror(errno)));
	    return -1;
	}
	while ( (len=read(dnsock, buf, sizeof(buf))) > 1)
	{	    
	    write(tempfile, buf, len);
	}
	close (tempfile);

	// Base64 encode it.
	UUInitialize();
	UUEncodeToStream(unixfile, NULL, tempname, B64ENCODED, "temp.bin", 0);
	unlink(tempname);
    }
    return 0;
}
