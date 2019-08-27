/******************************************************************************
    (c) 1998 Eduardo.M Serrat             emserrat@hotmail.com
    Username/Password additions by David G North 1999
    Optarg additions by Rob Davies - Feb 2000

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
******************************************************************************/
/* dnping.c */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>

#define DNF_PKT_COUNT     0x001
#define DNF_INTERVAL      0x002
#define DNF_PKT_SIZE      0x004
#define DNF_USERNAME      0x008
#define DNF_PASSWORD      0x010
#define DNF_VERBOSE       0x020
#define DNF_QUIET         0x040
#define DNF_DEBUG         0x080
#define DNF_TIMESTAMPS    0x100
#define DNF_TIMEOUT       0x200

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define MAX_DN_PACKETSIZE 1518     /* set Ethernet max as maximum size */
#define MAX_DN_HDRSIZE    68       /* this is a wild guess -
                                      it needs to be calculated properly */
#define MAX_DN_DATASIZE   (MAX_DN_PACKETSIZE - MAX_DN_HDRSIZE)

int   options = 0;

/*-------------------------------------------------------------------------*/
static void usage(void)
{
    printf("Usage:\n");
    printf("\ndnping nodename [user pass] count\n");
    printf("\n\t*or*\n");
    printf("\ndnping [options] nodename\n");
    printf("\twhere [options]:\n");
    printf("\t-c number      number of packets to send {10}\n");
    printf("\t-d             debug mode {OFF}\n");
    printf("\t-i interval    interval between packets in microseconds {0}\n");
    printf("\t-p password    access control password {}\n");
    printf("\t-q             quiet mode {OFF}\n");
    printf("\t-s size        size of frame to send in bytes {%d data + %d hdr}\n",
	   40,MAX_DN_HDRSIZE);
    printf("\t-t             timestamps mode {OFF}\n");
    printf("\t-u username    access control username {}\n");
    printf("\t-v             verbose mode {OFF}\n");
    printf("\t-w seconds     maximum wait time (timeout)\n");
    exit(0);
}

void tvsub(register struct timeval *out, register struct timeval *in)
{
    if ((out->tv_usec -= in->tv_usec) < 0)
    {
	--out->tv_sec;
	out->tv_usec += 1000000;
    }
    out->tv_sec -= in->tv_sec;
}


void init_accdata( char *user, char *password,
		   struct accessdata_dn *accessdata)
{
    char *local_user = cuserid(NULL);
    char *cp;

    if ((options & DNF_DEBUG) && (local_user))
    {
	printf("cuserid() - LOCAL USER: %s\n",local_user);
    }

    memset(accessdata, 0, sizeof(*accessdata));

    memcpy(accessdata->acc_user, user, MIN(strlen(user),DN_MAXACCL));
    accessdata->acc_user[DN_MAXACCL-1] = '\0';
    accessdata->acc_userl = strlen((char *)accessdata->acc_user);


    /* If the password is "-" and fd 0 is a tty then
       prompt for a password */
    if (password[0] == '-' && password[1] == '\0' && isatty(0))
    {
	password = getpass("Password: ");
	if (password == NULL || strlen(password) > (unsigned int)DN_MAXACCL)
	{
	    fprintf(stderr, "Password too long");
	    return;
	}

    }

    memcpy(accessdata->acc_pass, password, MIN(strlen(password),DN_MAXACCL));
    accessdata->acc_pass[DN_MAXACCL-1] = '\0';
    accessdata->acc_passl = strlen((char *)accessdata->acc_pass);

    /* Try very hard to get the local username for proxy access */
    if (!local_user || local_user == (char *)0xffffffff)
    {
	local_user = getenv("LOGNAME");

	if ((options & DNF_DEBUG) && (local_user))
	{
	    printf("getenv(LOGNAME) - LOCAL USER: %s\n",local_user);
	}

    }

    if (!local_user)
    {
	local_user = getenv("USER");

	if ((options & DNF_DEBUG) && (local_user))
	{
	    printf("getenv(USER) - LOCAL USER: %s\n",local_user);
	}

    }

    if (local_user)
    {
	strncpy((char *)accessdata->acc_acc, local_user,
		MIN(strlen(local_user),DN_MAXACCL));
	accessdata->acc_acc[DN_MAXACCL-1] = '\0';
	accessdata->acc_accl = strlen((char *)accessdata->acc_acc);
	for (cp=(char *)accessdata->acc_acc; *cp!='\0'; *cp=toupper(*cp), ++cp);
    }
    else
    {
	accessdata->acc_acc[0] = '\0';

	if (options & DNF_DEBUG)
	{
	    printf("LOCAL USER: NULL\n");
	}

    }

    return;
}

void sig_alarm(int s)
{
    printf("Connect timed out\n");
    exit(1);
}

/*-------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    struct  sockaddr_dn             sockaddr;
    struct  accessdata_dn           accessdata;
    static  struct  nodeent         *np;
    int                     sockfd,i,ch;
    char                    nodename[20],
	ibuf[MAX_DN_PACKETSIZE],
	obuf[MAX_DN_PACKETSIZE];
    short                   snd,rcv,num;

    char                    username[DN_MAXACCL],password[DN_MAXACCL];
    int                     npackets = 10,
	datalen = 40,
	interval = 0,
	cmplen, offset;
    struct timeval          tv, *tp;
    long                    triptime = 0,
	tmin = LONG_MAX, /* minimum round trip time */
	tmax = 0;        /* maximum round trip time */
    unsigned long           tsum = 0; /* sum of all times, for doing average */
    unsigned long           timeout_sec;
    struct timeval          timeout;

    signal(SIGALRM, sig_alarm);

    while ((ch = getopt(argc, argv, "c:di:qs:u:p:w:vt")) != EOF)
    {
	switch(ch)
	{
	case 'c':               /* number of packets to send */
            npackets = atoi(optarg);
            if (npackets <= 0)
	    {
		fprintf(stderr, "ping: bad number of packets to transmit.\n");
		exit(-1);
	    }
            options |= DNF_PKT_COUNT;
            break;
	case 'd':               /* turn on debug option */
            options |= DNF_DEBUG;
            break;
	case 't':               /* turn on timestamps option */
            options |= DNF_TIMESTAMPS;
            break;
	case 'i':               /* wait between sending packets */
            interval = atoi(optarg);
            if ( (interval <= 0) || (interval > 60000000) )
	    {
		fprintf(stderr, "ping: bad timing interval.\n");
		exit(-1);
	    }
            options |= DNF_INTERVAL;
            break;
	case 'q':               /* quiet mode */
            options |= DNF_QUIET;
            break;
	case 's':               /* size of data portion to send */
            options |= DNF_PKT_SIZE;
            datalen = atoi(optarg) - MAX_DN_HDRSIZE;
            if (datalen > MAX_DN_DATASIZE)
	    {
		fprintf(stderr, "ping: packet size too large.\n");
		exit(-1);
	    }
            if (datalen <= 0)
	    {
		fprintf(stderr, "ping: illegal packet size.\n");
		exit(-1);
	    }
            break;
	case 'v':               /* verbose mode */
            options |= DNF_VERBOSE;
            break;
	case 'u':               /* access control username */
            options |= DNF_USERNAME;
            snprintf(username,sizeof(username),"%s",optarg);
            break;
	case 'p':               /* access control password */
            options |= DNF_PASSWORD;
            snprintf(password,sizeof(password),"%s",optarg);
            break;
	case 'w':
	    options |= DNF_TIMEOUT;
	    timeout_sec = atoi(optarg);
	    break;
	default:
            usage();
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (options & DNF_DEBUG) printf("ARGC : %d\n",argc);

    if ((argc < 1) || (argc > 4))
    {
	usage();
    }

    snprintf(nodename,sizeof(nodename),"%s",*argv);
    if ( ( (argc == 4) || (argc == 2) )
	 && ((options & DNF_PKT_COUNT) == 0) )
    {
	npackets=atoi(argv[argc-1]);
    }

    if ( (np=getnodebyname(nodename)) == NULL)
    {
	if ( (options & DNF_QUIET) == 0 )
	{
	    printf("Unknown node name %s\n",nodename);
	}
	exit(-1);
    }

    if ((sockfd=socket(AF_DECnet,SOCK_SEQPACKET,DNPROTO_NSP)) == -1)
    {
	if ( (options & DNF_QUIET) == 0 )
	{
	    perror("socket");
	}
	exit(-1);
    }

    if ((((options & DNF_USERNAME) == 0) && ((options & DNF_PASSWORD) != 0)) ||
	(((options & DNF_USERNAME) != 0) && ((options & DNF_PASSWORD) == 0)))
    {
	if ( (options & DNF_QUIET) == 0 )
	{
	    printf("Must specify both username and password for access control\n");
	}
	exit(-1);
    }

    if ( ((options & (DNF_USERNAME|DNF_PASSWORD) ) == 0) && (argc > 2) )
    {
	snprintf(username,sizeof(username),"%s",argv[1]);
	options |= DNF_USERNAME;
	snprintf(password,sizeof(password),"%s",argv[2]);
	options |= DNF_PASSWORD;
    }

    if ( (options&(DNF_USERNAME|DNF_PASSWORD)) == (DNF_USERNAME|DNF_PASSWORD))
    {
	if ( (options & DNF_DEBUG) != 0)
	{
	    printf("USERNAME: %s\nPASSWORD: %s\n", username, password);
	}

	init_accdata(username, password, &accessdata);
	if (setsockopt(sockfd, DNPROTO_NSP, SO_CONACCESS, &accessdata,
		       sizeof(accessdata)) < 0)
	{
	    if ( (options & DNF_QUIET) == 0 )
            {
		perror("setsockopt");
            }
	    exit(-1);
	}
    }

    if (options & DNF_TIMESTAMPS)
    {
	if (datalen < sizeof(struct timeval))
	{
	    if ( (options & DNF_QUIET) == 0 )
            {
		printf("Packet size not large enough to store timestamp\n");
            }
	    exit(-1);
	}
    }

    sockaddr.sdn_family = AF_DECnet;
    sockaddr.sdn_flags  = 0x00;
    sockaddr.sdn_objnum  = DNOBJECT_MIRROR;
    sockaddr.sdn_objnamel  = 0x00;
    memcpy(sockaddr.sdn_add.a_addr, np->n_addr,2);

    /* This is the cheesy, cowards way of checking for
       a connect timeout */
    if (options & DNF_TIMEOUT)
	alarm(timeout_sec);

    if (connect(sockfd, (struct sockaddr *)&sockaddr,
		sizeof(sockaddr)) < 0)
    {
	if ( (options & DNF_QUIET) == 0 )
	{
	    perror("socket");
	}
	exit(-1);
    }

    /* Cancel the connect() alarm */
    if (options & DNF_TIMEOUT)
	alarm(0);


    for (i = 0; i < datalen; i++)
    {
	obuf[i]=0x85;
    }
    obuf[0]=0x00;

    cmplen = (datalen - 1) -
	(options & DNF_TIMESTAMPS)?sizeof(struct timeval):0;
    if (options & DNF_DEBUG)
    {
	printf("CMPLEN: %d\n",cmplen);
    }

    offset = 1 + (options & DNF_TIMESTAMPS)?sizeof(struct timeval):0;
    if (options & DNF_DEBUG)
    {
	printf("OFFSET: %d\n",offset);
    }

    snd=0;
    rcv=0;

    for (i = 0; i < npackets; i++)
    {
	if (options & DNF_TIMESTAMPS)
	{
	    gettimeofday((struct timeval *)&obuf[1], (struct timezone *)NULL);
	}
	num = write(sockfd,obuf,datalen);
	if ( num < 0 )
	{
	    if ( (options & DNF_QUIET) == 0 )
            {
		perror("Write");
            }
	    exit(-1);
	}
	if (options & (DNF_DEBUG|DNF_VERBOSE))
	{
	    printf("PKT: %-4d   WRITE: %d ",i+1,num);
	}
	snd++;

	if (options & DNF_TIMEOUT)
	{
	    int status;
	    fd_set in_fd;

	    FD_ZERO(&in_fd);
	    FD_SET(sockfd, &in_fd);
	    timeout.tv_sec = timeout_sec;
	    timeout.tv_usec = 0;

	    status = select(sockfd+1, &in_fd, NULL, NULL, &timeout);
	    if (status < 0)
	    {
		perror("select");
		close(sockfd);
		exit(-1);
	    }
	    if (status == 0)
	    {
		fprintf(stderr, "Timeout\n");
		close(sockfd);
		exit(-1);
	    }
	}

	num = read(sockfd,ibuf,sizeof(ibuf));
	if ( num < 0 )
	{
	    if ( (options & DNF_QUIET) == 0 )
            {
		perror("Read");
            }
	    exit(-1);
	}

	gettimeofday(&tv, (struct timezone *)NULL);

	if (options & (DNF_DEBUG|DNF_VERBOSE))
	{
	    printf("READ: %d ",num);
	}

	if (memcmp(&obuf[offset],&ibuf[offset],cmplen) != 0)
	{
	    if ( (options & (DNF_QUIET|DNF_DEBUG|DNF_VERBOSE)) == 0 )
            {
		printf("Loopback Error\n");
            }
	    if ( (options & (DNF_DEBUG|DNF_VERBOSE)) != 0)
            {
		printf("**error**\n");
            }
	}
	else
	{
	    rcv++;
	    if (options & DNF_TIMESTAMPS)
            {
		tp = (struct timeval *)&ibuf[1];
		tvsub(&tv, tp);
		triptime = tv.tv_sec * 10000 + (tv.tv_usec / 100);
		tsum += triptime;
		if (triptime < tmin)
		{
		    tmin = triptime;
		}
		if (triptime > tmax)
		{
		    tmax = triptime;
		}
            }

	    if ( (options & (DNF_DEBUG|DNF_VERBOSE)) != 0)
            {
		if (options & DNF_TIMESTAMPS)
		{
		    printf("  RTT: %ld.%ld\n", triptime/10, triptime%10);
		}
		else
		{
		    printf("\n");
		}
            }
	}

	if (interval > 0)
	{
	    usleep(interval);
	}

    } /* end for loop */

    close(sockfd);
    if ( (options & DNF_QUIET) == 0 )
    {
	printf("Sent %d packets, Received %d packets\n",snd,rcv);
	if ((rcv > 0) && (options & DNF_TIMESTAMPS))
	{
	    if (options & DNF_DEBUG)
            {
		printf("TSUM: %ld\n",tsum);
            }

	    printf("\tround-trip min/avg/max = %ld.%ld/%lu.%ld/%ld.%ld ms\n",
		   tmin/10, tmin%10,
		   (tsum / (rcv*10)),
		   (tsum / rcv)%10,
		   tmax/10, tmax%10);
	}
    }

    return( ((snd - rcv) != 0) ? -1 : 0 );
}
