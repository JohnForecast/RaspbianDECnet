/*
 * dnroute.c    DECnet routing daemon (eventually...)
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:     Christine Caulfield <christine.caulfield@googlemail.com>
 *              based on rtmon.c by Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <sys/types.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <string.h>
#include <linux/netfilter_decnet.h>
#include <netdnet/dnetdb.h>

#include "libnetlink.h"
#include "utils.h"
#include "csum.h"

extern void  send_route_msg(int);


char *if_index_to_name(int ifindex)
{
    struct ifreq ifr;
    static char buf[64];

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    ifr.ifr_ifindex = ifindex;

    if (ioctl(sock, SIOCGIFNAME, &ifr) == 0)
    {
	strcpy(buf, ifr.ifr_name);
    }
    else
    {
	sprintf(buf, "if%d", ifindex);
    }

    close(sock);
    return buf;
}

int if_name_to_index(char *name)
{
    struct ifreq ifr;
    static char buf[64];

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    ifr.ifr_ifindex = -1;
    strcpy(ifr.ifr_name, name);

    ioctl(sock, SIOCGIFINDEX, &ifr);

    close(sock);
    return ifr.ifr_ifindex;
}


static int dump_neigh_msg(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
    struct nf_dn_rtmsg *rtm;
    unsigned short *ptr2;
    unsigned char  *ptr1;
    int            len, i;
    unsigned int   sum=1;

    rtm = (struct nf_dn_rtmsg *)NLMSG_DATA(n);
    ptr2 = (unsigned short *)NFDN_RTMSG(rtm);
    ptr1 = (unsigned char *)NFDN_RTMSG(rtm);
    len = n->nlmsg_len - sizeof(*n) - sizeof(*rtm);

    printf("CC: got rtnetlink message, len = %d\n", len);

#define DUMP_PACKET
#ifdef DUMP_PACKET
    for (i=0; i<len/2; i++)
    {
	if (!(i&0xf)) fprintf(stderr, "\n");
	fprintf(stderr, "%04x ", *(ptr2+i));
    }
    fprintf(stderr, "\n");
#endif

    return 0;
}

/* Dump a routing message to stdout */
static int dump_rtg_msg(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
    struct nf_dn_rtmsg *rtm;
    unsigned short *ptr2;
    unsigned char  *ptr1;
    int            len, i;
    unsigned int   sum=1;

    rtm = (struct nf_dn_rtmsg *)NLMSG_DATA(n);
    ptr2 = (unsigned short *)NFDN_RTMSG(rtm);
    ptr1 = (unsigned char *)NFDN_RTMSG(rtm);
    len = n->nlmsg_len - sizeof(*n) - sizeof(*rtm);

#ifdef DUMP_PACKET
    for (i=0; i<len/2; i++)
    {
	if (!(i&0xf)) fprintf(stderr, "\n");
	fprintf(stderr, "%04x ", *(ptr2+i));
    }
    fprintf(stderr, "\n");
#endif

    /* Level 1 Routing Message */
    if ( (ptr1[0] & 0xE)>>1 == 3)
    {
	struct dn_naddr add;
	char   node[32];
	int    num;
	int    num_ids;
	int    start_id;
	int    entry;

	i = 4; /* Start of segments */

	add.a_len = 2;
	add.a_addr[0] = ptr1[1];
	add.a_addr[1] = ptr1[2];
	dnet_ntop(AF_DECnet, &add, node, sizeof(node));

	printf("Level 1 routing message from %s on %s, len = %d\n",
	       node, if_index_to_name(rtm->nfdn_ifindex), len);

	while (i < len-4)
	{
	    num_ids = ptr1[i] | ptr1[i+1]<<8;
	    i+=2;
	    start_id = ptr1[i] | ptr1[i+1]<<8;
	    i+=2; /* Start of entries */

	    for (num = 0; num<num_ids; num++)
	    {
		entry = ptr1[i] | ptr1[i+1]<<8;
		if (entry != 0x7fff)
		{
		    printf("  Node %d reachable. Hops %d, cost %d\n",
			   num+start_id, (entry&0x7E00)>>9, entry&0x1FF);
		}
		i+=2;
	    }
	}
    }

    /* Level 2 Routing Message */
    if ( (ptr1[0] & 0xE)>>1 == 4)
    {
	struct dn_naddr add;
	char   node[32];
	int    num;
	int    num_ids;
	int    start_id;
	int    entry;

	i = 4; /* Start of segments */

	add.a_len = 2;
	add.a_addr[0] = ptr1[1];
	add.a_addr[1] = ptr1[2];
	dnet_ntop(AF_DECnet, &add, node, sizeof(node));

	printf("Level 2 routing message from %s on %s, len = %d\n",
	       node, if_index_to_name(rtm->nfdn_ifindex), len);
	while (i < len-4)
	{
	    num_ids = ptr1[i] | ptr1[i+1]<<8;
	    i+=2;
	    start_id = ptr1[i] | ptr1[i+1]<<8;
	    i+=2; /* Start of entries */

	    for (num = 0; num<num_ids; num++)
	    {
		entry = ptr1[i] | ptr1[i+1]<<8;
		if (entry != 0x7fff)
		{
		    printf("  Area %d reachable. Hops %d, cost %d\n",
			   num+start_id, (entry&0x7E00)>>9, entry&0x1FF);
		}
		i+=2;
	    }
	}

    }

    /* Check the checksum */
    sum = route_csum(ptr1, 4, i);

    printf("Calc sum=%x, got sum: %x\n", sum, *(unsigned short *)(ptr1+i));

    return 0;
}

