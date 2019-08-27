/*
 * send_route.c    DECnet routing daemon
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:     Christine Caulfield <christine.caulfield@googlemail.com>
 *
 */

#include <sys/types.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include <limits.h>
#include <linux/netfilter_decnet.h>
#include <netdnet/dnetdb.h>
#include <features.h>    /* for the glibc version number */
#if (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1) || __GLIBC__ >= 3
#include <netpacket/packet.h>
#include <net/ethernet.h>     /* the L2 protocols */
#include <net/if.h>
#include <net/if_arp.h>
#else
#include <asm/types.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#endif

#include "utils.h"
#include "libnetlink.h"
#include "dnrtlink.h"
#include "csum.h"
#include "dnroute.h"

extern char *if_index_to_name(int ifindex);
extern int dnet_socket;

/* Send a Level 1 routing message for nodes "start" to "end".
 * "start" should be a multiple of 32 for the header to be
 * added correctly/
 */
static int send_routing_message(unsigned char type, struct routeinfo *node_table, int start, int end,
				struct dn_naddr *exec, int interface)
{
    struct sockaddr_ll sock_info;
    unsigned char packet[1600];
    unsigned short sum;
    int i,j;

    fprintf(stderr,"Sending message type %d. start=%d, end=%d\n", type,start,end);

    i=0;

    packet[i++] = 0x00; /* Length, filled in at end */
    packet[i++] = 0x00;

    packet[i++] = type;
    packet[i++] = exec->a_addr[0]; /* Our node address */
    packet[i++] = exec->a_addr[1];
    packet[i++] = 0x00; /* Reserved */


    /* Header */
    packet[i++] = (end-start) & 0xFF;
    packet[i++] = (end-start) >> 8;
    packet[i++] = start & 0xFF;
    packet[i++] = start >> 8;

    for (j=start; j<end; j++)
    {
	if (node_table[j].valid)
	{
		packet[i++] = node_table[j].cost; /* cost can use the low 2 bit of the next byte... */
		packet[i++] = (node_table[j].hops << 2) | ((node_table[j].cost>>8) & 3);   /* hops - starting bit 2 */
	}
	else
	{
	    packet[i++] = 0xff;
	    packet[i++] = 0x7f;
	}
    }

    /* Add in checksum */
    sum = route_csum(packet, 6, i);
    packet[i++] = sum & 0xFF;
    packet[i++] = sum >> 8;

    packet[0] = (i-2) & 0xFF;
    packet[1] = (i-2) >> 8;

    /* Build the sockaddr_ll structure */
    sock_info.sll_family   = AF_PACKET;
    sock_info.sll_protocol = htons(ETH_P_DNA_RT);
    sock_info.sll_ifindex  = interface;
    sock_info.sll_hatype   = 0;
    sock_info.sll_pkttype  = PACKET_MULTICAST;
    sock_info.sll_halen    = 6;

    /* This is the DECnet routing multicast address */
    sock_info.sll_addr[0]  = 0xab;
    sock_info.sll_addr[1]  = 0x00;
    sock_info.sll_addr[2]  = 0x00;
    sock_info.sll_addr[3]  = 0x03;
    sock_info.sll_addr[4]  = 0x00;
    sock_info.sll_addr[5]  = 0x00;

    if (sendto(dnet_socket, packet, i, 0,
	       (struct sockaddr *)&sock_info, sizeof(sock_info)) < 0)
    {
	    perror("sendto");
	    return -1;
    }
    return 0;
}

static void send_route_msg(unsigned char type, struct routeinfo *node_table, int start, int num)
{
    struct ifreq ifr;
    int iindex = 1;
    struct dn_naddr *addr;
    int last_node;
    int sock;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
	    return;

    /* Get our node address */
    addr = getnodeadd();

    for (iindex = 0; iindex < 128; iindex++)
    {
	    ifr.ifr_ifindex = iindex;
	    if (ioctl(sock, SIOCGIFNAME, &ifr) == 0)
	    {
		    /* Only send to ethernet interfaces */
		    ioctl(sock, SIOCGIFHWADDR, &ifr);
		    if (ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER)
		    {
			    int mtu;
			    int num_nodes;

			    last_node = start;

			    ioctl(sock, SIOCGIFMTU, &ifr);
			    mtu = ifr.ifr_mtu;

			    /* Work out how many blocks we get into one MTU-sized packet */
			    /* header = 32 bytes, 2 bytes per node, in multiples of 32 nodes */
			    num_nodes = (mtu-32)/2 & 0xFFC0;
			    if (num_nodes > 256) num_nodes = 256; /* cap it */

			    while (last_node < num)
			    {
				    /* Don't overflow the end of the list */
				    if (last_node+num_nodes > num)
					    num_nodes = num-last_node;

				    send_routing_message(type, node_table, last_node, last_node+num_nodes, addr, iindex);
				    last_node += num_nodes;
			    }
		    }
		    ifr.ifr_ifindex = iindex;
	    }
    }

    close(sock);
    return;
}


void send_level1_msg(struct routeinfo *node_table)
{
	send_route_msg(0x07, node_table, 0, 1024);
}

void send_level2_msg(struct routeinfo *area_table)
{
	send_route_msg(0x09, area_table, 1, 64);
}
