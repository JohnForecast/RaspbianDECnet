/**********************************************************************************
    (c) 2006,2008     Christine Caulfield        christine.caulfield@googlemail.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    ********************************************************************************


    Multinet server proxy server for Linux.

    This daemon talks to a VMS system running a multinet DECnet/IP tunnel.
    (www.process.com/tcpip/multinet.html)

    You'll need to set up the VMS end to talk to this box.
    It creates a tun/tap device and assigns it a suitable
    MAC address.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/signal.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#ifdef __NetBSD__
#include <net/if.h>
#include <net/if_tun.h>
#else
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

static int verbose;
static int ipfd;
static int tunfd;
static unsigned short local_addr[2];
static struct sockaddr_in remote_addr;
static int got_remote_addr;
static unsigned char remote_decnet_addr[2];
static char *remote_host_name;
static int got_verification;

static int router_priority = 64;
static int router_level = 2;
static int mtu = 578;
static int port = 700;
static int ip_timeout = 300;
static int hello_timer = 60;
static char old_default[1024];
static time_t last_ip_packet;
static sig_atomic_t running;
static sig_atomic_t reload_config;

#define DUMP_MAX 1024

/* Resolve the remote IP host name.
 *  Called at startup and on receipt of SIGHUP
 */
static int lookup_name(void)
{
	struct addrinfo *ainfo;
	struct addrinfo ahints;
	int res;

	memset(&ahints, 0, sizeof(ahints));
	ahints.ai_socktype = SOCK_DGRAM;
	ahints.ai_protocol = IPPROTO_UDP;

	/* Lookup the nodename address */
	if ( (res=getaddrinfo(remote_host_name, NULL, &ahints, &ainfo)) )
	{
		fprintf(stderr, "Can't resolve name '%s': %s\n", remote_host_name, gai_strerror(res));
		return 2;
	}

	memcpy(&remote_addr, ainfo->ai_addr, sizeof(struct sockaddr_in));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(port);

	return 0;
}

static void do_sighup(int sig)
{
	if (verbose)
		fprintf(stderr, "Got sighup, re-reading config\n");
	reload_config = 1;
}

static void do_shutdown(int sig)
{
	if (verbose)
		fprintf(stderr, "Got signal, shutting down\n");
	running = 0;
}

static int send_ip(int fudge_header, unsigned char *, int len);

static void dump_data(char *from, unsigned char *databuf, int datalen)
{
	int i;
	if (!verbose)
		return;

	fprintf(stderr, "%s (%d)", from, datalen);
	for (i=0; i<(datalen>DUMP_MAX?DUMP_MAX:datalen); i++)
	{
		fprintf(stderr, "%02x  ", databuf[i]);
	}
	fprintf(stderr, "\n");
}

static void send_start(unsigned short addr)
{
	unsigned char start[] = { 0x01, 0x08, 0x05, 0x05, 0x40,0x02, 0x02,0x00, 0x00,0x2c, 0x01,0x00, 0x00,0x00,};
	unsigned char verf[] =  { 0x03, 0x02, 0x0c, 0x00};

	start[1] = addr & 0xff;
	start[2] = addr >> 8;

	send_ip(0, start, sizeof(start));

	verf[1] = addr & 0xff;
	verf[2] = addr >> 8;

	send_ip(0, verf, sizeof(verf));

}


static int send_tun(int mcast, unsigned char *buf, int len)
{
	unsigned char header[38];
	struct iovec iov[2];
	int header_len;

	if (!got_remote_addr)
		return 0; /* Can't send yet */

	memset(header, 0, sizeof(header));

	/* Add ethernet header */
	header[0] = 0xAA;
	header[1] = 0x00;
	header[2] = 0x04;
	header[3] = 0x00;
	header[4] = local_addr[0];
	header[5] = local_addr[1];

	if (mcast) /* Routing multicast - type may need to be in callers params */
	{
		header[6]  = 0xab;
		header[7]  = 0x00;
		header[8]  = 0x00;
		header[9]  = 0x03;
		header[10] = 0x00;
		header[11] = 0x00;

		header_len = 14;
		// TODO Maybe want to send to 09:00:2b:02:00:00 too,
		// Not sure why VMS sends to both, probably backward compatibility
	}
	else
	{
		header[6] = 0xAA;
		header[7] = 0x00;
		header[8] = 0x04;
		header[9] = 0x00;
		header[10] = remote_decnet_addr[0];
		header[11] = remote_decnet_addr[1];

		header_len = sizeof(header);
	}
	header[12] = 0x60; /* DECnet packet type */
	header[13] = 0x03;

	if (!mcast)
	{
		/* DECnet packet length */
		header[14] = (len+16) & 0xFF;
		header[15] = (len+16) >> 8;

		/* Fake Long DECnet header */
		header[16] = 0x81; header[17] = 0x26;  // TODO Don't know what this is!
		header[18] = header[19] = 0;

		header[20] = header[28] = 0xAA;
		header[21] = header[29] = 0x00;
		header[22] = header[30] = 0x04;
		header[23] = header[31] = 0x00;
		header[24] = buf[1]; /* Dest addr */
		header[25] = buf[2];
		header[32] = buf[3]; /* src addr */
		header[33] = buf[4];

		buf += 6;
		len -= 6;
	}

	iov[0].iov_base = header;
	iov[0].iov_len = header_len;
	iov[1].iov_base = buf;
	iov[1].iov_len = len;

	dump_data("to TUN0:", header, header_len);
	dump_data("to TUN1:", buf, len);

	writev(tunfd, iov, 2);
	return len;
}

static int send_ip(int fudge_header, unsigned char *buf, int len)
{
	struct iovec iov[2];
	unsigned char header[4];
	struct msghdr msg;
	static unsigned short seq;

	memset(&msg, 0, sizeof(msg));
	seq++;
	header[0] = seq & 0xFF;
	header[1] = seq >> 8;
	header[2] = header[3] = 0;

	if (fudge_header)
	{
		/* Shorten DECnet addresses */
		buf[0] = 0x02;    /* Short data message */
		buf[1] = buf[8];  /* Destination */
		buf[2] = buf[9];
		buf[3] = buf[16]; /* Source */
		buf[4] = buf[17];
		buf[5] = 0;
		memmove(buf+6, buf+22, len-16);

		len -= 16;
	}

	iov[0].iov_base = header;
	iov[0].iov_len = sizeof(header);
	iov[1].iov_base = buf;
	iov[1].iov_len = len;

	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	msg.msg_name = (void *)&remote_addr;
	msg.msg_namelen = sizeof(struct sockaddr_in);

	dump_data("Send to IP0", header, sizeof(header));
	dump_data("Send to IP1:", buf, len);

	if (sendmsg(ipfd, &msg, 0) <= 0)
		perror("sendmsg");

	return 0;
}


static int setup_ip(int port)
{
	int fd;
	int flag = 1;
	struct sockaddr_in sin;

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0)
		return -1;


	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&flag, sizeof(flag));

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;

	if (bind(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)))
	{
		perror("bind");
		close(fd);
		return -1;
	}

	return fd;
}

void resend_start(int sig)
{
	if (!got_verification)
	{
		unsigned short addr = (local_addr[0] | local_addr[1]<<8);
		send_start(addr);
		alarm(10);
	}
}

static void read_ip(void)
{
	unsigned char buf[1600];
	int len;
	struct sockaddr_in sin;
	unsigned int sinlen = sizeof(sin);

	len = recvfrom(ipfd, buf, sizeof(buf), 0, (struct sockaddr *)&sin, &sinlen);
	if (len <= 0) return;

	/* Ignore packets from people we're not talking to */
	if (sin.sin_port != remote_addr.sin_port ||
	    sin.sin_addr.s_addr != remote_addr.sin_addr.s_addr)
		return;

	last_ip_packet = time(NULL);

	dump_data("from IP:", buf, len);

	if (buf[4] == 0x05) /* PtP hello, make into ethernet hello */
	{
		unsigned char hello[] = {
			0x00, 0x00,           /* Length, filled in later */
			0x0b,                 /* FLAGS: Router hello */
			0x02, 0x00, 0x00,     /* Router version */
			0xaa, 0x00, 0x04, 0x00, buf[5], buf[6], /* Routers MAC addr */
			3-router_level,         /* Info, including routing level */
			mtu % 0xFF, mtu >> 8, /* Data block size  */
			router_priority,      /* Priority */
			0x00,                 /* Reserved */
			hello_timer&0xFF,
			hello_timer >> 8,     /* Hello timer (seconds) */
			0x00,                 /* Reserved */
			0x0f,                 /* Length of (other 'logical' ethernets) message that follows */
			0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* "e-list" name */
			0x07,                 /* Elist name */
			0xaa, 0x00, 0x04, 0x00, remote_decnet_addr[0], remote_decnet_addr[1],
			0x40 /* state : (=priority) */
		};

		hello[0] = sizeof(hello); /* Allow me to edit it at will */
		send_tun(1, hello, sizeof(hello));

		got_verification = 1;
		alarm(0); /* cancel START timer */
	}

	/* Trap INIT & VERF & test messages, they're for us */
	if (buf[4] == 0x01 || buf[4] == 0x05 || buf[4] == 0x03)
	{
		if (!got_remote_addr)
		{
			unsigned short addr = buf[6]<<8 | buf[5];
			got_remote_addr = 1;
			remote_decnet_addr[0] = buf[5];
			remote_decnet_addr[1] = buf[6];

			printf("Remote address = %d.%d (%d)\n", addr>>10, addr&1023, addr);
		}
		return;
	}

	if (buf[4] == 0x07 || buf[4] == 0x09) /* Routing info */
	{
		/*
		  off ethernet:
		  13:17:58.021480 lev-2-routing src 3.35 {areas 1-64 cost 4 hops 1}
		  0x0000:  8800 0923 0c00 3f00 0100 0404 0a04 0000
		  0x0010:  ff7f ff7f ff7f ff7f ff7f ff7f ff7f 0404
		  0x0020:  ff7f ff7f ff7f ff7f ff7f ff7f ff7f ff7f
		  0x0030:  ff7f ff7f ff7f ff7f ff7f ff7f 2428 1e08
		  0x0040:  ff7f ff7f ff7f ff7f ff7f ff7f ff7f ff7f
		  0x0050:  ff7f ff7f ff7f ff7f ff7f ff7f ff7f ff7f
		  0x0060:  ff7f ff7f ff7f ff7f ff7f ff7f ff7f ff7f
		  0x0070:  ff7f ff7f ff7f ff7f ff7f ff7f ff7f 1408
		  0x0080:  ff7f ff7f ff7f ff7f 8d44

		  off Multinet:
		  09  00  00  00  multinet header
		  09  23  0c  00  3f  00  01  00  04  04  0a  04  00  00
		  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  04  04
		  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f
		  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  04  04  1e  08
		  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f
		  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f
		  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f
		  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  ff  7f  14  08
		  ff  7f  ff  7f  ff  7f  ff  7f  6d  20
		*/
		buf[2] = len % 0xFF;
		buf[3] = len >> 8;
		send_tun(1, buf+2, len-2);
		return;
	}

	send_tun(0, buf+4, len-4);
}

static void read_tun(void)
{
	unsigned char buf[1600];
	int len;

	len = read(tunfd, buf, sizeof(buf));
	if (len <= 0) return;

	/* We get local HELLOs from time to time so this should ensure that we're not
	   flooding the IP link with dodgy UDP continously
	*/
	if (time(NULL) - last_ip_packet > ip_timeout)
	{
		got_verification = 0;
		resend_start(SIGALRM);
	}

	/* Only forward DECnet packets... */
	if (buf[12] == 0x60 && buf[13] == 0x03)
	{
		dump_data("DECnet from TUN:", buf, len);

		/* Ignore our echoed packets */
		if (buf[4] == local_addr[0] &&
		    buf[5] == local_addr[1])
		{
			if (verbose)
				fprintf(stderr, "Ignoring our own packet\n");
			return;
		}

		/* Ethernet endnode or router hello,
		   we replace these with PtP hello messages */
		if (buf[16] == 0x0d || buf[16] == 0x0b)
		{
			unsigned char ptp_hello[] = { 0x5, buf[10], buf[11], 0252, 0252, 0252, 0252};
			if (verbose)
				fprintf(stderr, "Sending PTP hello\n");
			send_ip(0, ptp_hello, sizeof(ptp_hello));
			return ;
		}
		/* Routing messages, Don't fudge the header on these */
		if (buf[16] == 0x07 || buf[16] == 0x09)
		{
			send_ip(0, buf+16, len-16);
			return;
		}

		/* Data or other packet */
		send_ip(1, buf+16, len-16);
	}
}

static int setup_tun(unsigned short addr, int make_default)
{
	int ret;
	struct ifreq ifr;
	char cmd[132];
	FILE *procfile;

	tunfd = open("/dev/net/tun", O_RDWR);
	if (tunfd < 0)
	{
		perror("could not open /dev/net/tun");
		exit(2);
	}
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strcpy(ifr.ifr_name, "tap%d");
	ret = ioctl(tunfd, TUNSETIFF, (void *) &ifr);
	if (ret != 0)
	{
		perror("could not configure /dev/net/tun");
		exit(2);
	}
	fprintf(stderr, "using tun device %s\n", ifr.ifr_name);

	/* Bring the interface up
	 *   TODO: use ioctls...perhaps
	 */
	sprintf(cmd, "/sbin/ifconfig %s hw ether AA:00:04:00:%02X:%02X allmulti mtu %d up\n",
		ifr.ifr_name, addr & 0xFF, addr>>8, mtu);
	system(cmd);


	/* Configure the interface.
	 * Sigh, this maybe should be done using sysctl but that interface
	 * is probably worse than poking values into /proc !
	 */
	sprintf(cmd, "/proc/sys/net/decnet/conf/%s/forwarding", ifr.ifr_name);
	procfile = fopen(cmd, "w");
	if (!procfile)
	{
		fprintf(stderr, "Cannot set forwarding on interface\n");
	}
	else
	{
		fprintf(procfile, "1");
		fclose(procfile);
	}

	sprintf(cmd, "/proc/sys/net/decnet/conf/%s/priority", ifr.ifr_name);
	procfile = fopen(cmd, "w");
	if (!procfile)
	{
		fprintf(stderr, "Cannot set priority on interface\n");
	}
	else
	{
		fprintf(procfile, "%d", router_priority);
		fclose(procfile);
	}

	sprintf(cmd, "/proc/sys/net/decnet/conf/%s/t3", ifr.ifr_name);
	procfile = fopen(cmd, "w");
	if (!procfile)
	{
		fprintf(stderr, "Cannot set hello timer\n");
	}
	else
	{
		fprintf(procfile, "%d", hello_timer);
		fclose(procfile);
	}

	if (make_default)
	{
		procfile = fopen("/proc/sys/net/decnet/default_device", "w+");
		if (!procfile)
		{
			fprintf(stderr, "Cannot set default device\n");
		}
		else
		{
			fgets(old_default, sizeof(old_default), procfile);
			fprintf(procfile, "%s", ifr.ifr_name);
			fclose(procfile);
		}
	}

	return tunfd;
}

static void restore_interface(void)
{
	if (old_default[0])
	{
		FILE *procfile;

		procfile = fopen("/proc/sys/net/decnet/default_device", "w+");
		if (!procfile)
		{
			fprintf(stderr, "Cannot restore default device\n");
		}
		else
		{
			fprintf(procfile, "%s", old_default);
			fclose(procfile);
		}
	}
}

static void usage(char *cmd)
{

	printf("Usage: %s [options] <local-decnet-addr> <remote-IP-host>\n", cmd);
	printf("eg     %s -D 3.2 zarqon\n\n", cmd);

	printf("    -v       Verbose output\n");
	printf("    -1       Advertise as a level 1 router\n");
	printf("    -2       Advertise as a level 2 router (default)\n");
	printf("    -D       Make this the default DECnet interface (default no)\n");
	printf("    -p<prio> Router priority (default 64)\n");
	printf("    -P<port> IP port to talk to multinet on (default 700)\n");
	printf("    -m<MTU>  MTU of interface (default 576)\n");
	printf("    -t<secs> Timeout to restart IP connections (default 600)\n");
	printf("    -H<secs> Hello timer (default 60)\n");

}

int main(int argc, char *argv[])
{
	struct pollfd pfds[2];
	unsigned int area, node;
	unsigned short addr;
	int make_default = 0;
	int opt;

	while ((opt=getopt(argc,argv,"vp:12m:P:t:H:D?h")) != EOF)
	{
		switch(opt)
		{
		case 'v':
			verbose++;
			break;

		case '2':
			router_level = 2;
			break;
		case '1':
			router_level = 1;
			break;

		case 'p':
			router_priority = atoi(optarg);
			if (router_priority > 127 || router_priority < 0)
			{
				fprintf(stderr, "Router priority must be between 0 & 127\n");
				exit(2);
			}
			break;
		case 'P':
			port = atoi(optarg);
			break;

		case 'D':
			make_default = 1;
			break;

		case 'H':
			hello_timer = atoi(optarg);
			break;

		case 't':
			ip_timeout = atoi(optarg);
			break;

		case 'm':
			mtu = atoi(optarg);
			if (mtu > 1500 || mtu < 20)
			{
				fprintf(stderr, "MTU is invalid\n");
				exit(2);
			}
			break;

		case '?':
		case 'h':
			usage(argv[0]);
			break;

		}
	}

	if (argc - optind < 2)
	{
		usage(argv[0]);
		exit(2);
	}

	if (sscanf(argv[optind], "%d.%d", &area, &node) != 2)
	{
		fprintf(stderr, "DECnet address %s not valid\n", argv[optind]);
		return -1;
	}
	if (area > 63 || node > 1023)
	{
		fprintf(stderr, "DECnet address %d.%d not valid\n", area, node);
		return -1;
	}

	/* Save address for later */
	addr = (area<<10) | node;
	local_addr[0] = addr & 0xFF;
	local_addr[1] = addr >> 8;

	/* Check remote address is valid */
	optind++;
	remote_host_name = argv[optind];

	if (lookup_name())
		return 2;

	if (!verbose)
		daemon(0,0);

	signal(SIGINT, do_shutdown);
	signal(SIGTERM, do_shutdown);
	signal(SIGHUP, do_sighup);

	/* Initialise network ports */
	tunfd = setup_tun(addr, make_default);
	ipfd = setup_ip(port);

	/* Wait for START */
	signal(SIGALRM, resend_start);
	alarm(10);
	send_start(addr);

	pfds[0].fd = ipfd;
	pfds[0].events = POLLIN;

	pfds[1].fd = tunfd;
	pfds[1].events = POLLIN;

	fcntl(ipfd, F_SETFL, fcntl(ipfd, F_GETFL, 0) | O_NONBLOCK);
	fcntl(tunfd, F_SETFL, fcntl(tunfd, F_GETFL, 0) | O_NONBLOCK);

	running = 1;
	while (running)
	{
		int status;

		status = poll(pfds, 2, -1);
		if (status == -1 && errno != EINTR)
		{
			perror("poll error");
			exit(1);
		}

		if (reload_config)
		{
			lookup_name();
			reload_config = 0;
		}

		if (pfds[0].revents & POLLIN)
			read_ip();
		if (pfds[1].revents & POLLIN)
			read_tun();
	}

	if (make_default)
		restore_interface();

	return 0;
}
