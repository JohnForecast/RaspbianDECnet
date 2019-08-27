/*
 * get_neigh.c  DECnet routing daemon
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:     Christine Caulfield <christine.caulfield@googlemail.com)
 *              bits based on rtmon.c by Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
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

#include "dn_endian.h"
#include "utils.h"
#include "libnetlink.h"
#include "csum.h"
#include "hash.h"
#include "dnroute.h"

/* Sigh - people keep removing features ... */
#ifndef NDA_RTA
#define NDA_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct ndmsg))))
#endif

/* Where we write our status info to */
#define STATUS_SOCKET "/var/run/dnroute.status"
#define PIDFILE "/var/run/dnroute.pid"

/* What we stick in the hash table */
struct nodeinfo
{
	unsigned int interface;
	int scanned;
	int priority;
	int level;
	int deleted;
};

#define MAX_DEVICES 256

/* cost per device */
static unsigned char cost[MAX_DEVICES];

/* A list of all possible nodes in our area. Router will always be 0 */
static struct routeinfo node_table[1024];

/* The same for areas */
static struct routeinfo area_table[64];

/* Node hash is basically a copy of the neighbour table (so may have nodes
   from other areas in it) */
static struct dm_hash_table *node_hash;

extern char *if_index_to_name(int ifindex);
extern int if_name_to_index(char *name);
extern int send_level1_msg(struct routeinfo *);
extern int send_level2_msg(struct routeinfo *);
extern int pidfile_create(const char *pidFile, pid_t pid);

int dnet_socket;
static int info_socket;
static int debugging;
static int verbose;
static int send_routing;
static int send_level2;
static int no_routes;
static int routing_multicast_timer = 15;
struct dn_naddr *exec_addr;

static struct rtnl_handle talk_rth;
static struct rtnl_handle listen_rth;
static int first_time = 1;
static sig_atomic_t running;
static sig_atomic_t show_network;
static sig_atomic_t alarm_rang;

#define debuglog(fmt, args...) do { if (debugging) fprintf(stderr, fmt, ## args); } while (0)

static void term_sig(int sig)
{
	running = 0;
}

static void usr1_sig(int sig)
{
	show_network = 1;
}

static void alarm_sig(int sig)
{
	alarm_rang = 1;
	alarm(routing_multicast_timer);
}

static void read_conffile(void)
{
	char line[255];

	FILE *fp = fopen(SYSCONF_PREFIX "/etc/dnroute.conf", "r");
	if (!fp)
		return;

	while (fgets (line, sizeof(line), fp))
	{
		char *space;
		int ifindex;

		/* comment */
		if (line[0] == '#')
			continue;
		space = strchr(line, ' ');
		if (space)
		{
			*space = '\0';

			/* Look for an interface and get cost */
			if ( (ifindex = if_name_to_index(line)) != -1)
			{
				cost[ifindex] = atoi(space+1);
				debuglog("cost of %s(%d) is %d\n", line, ifindex, cost[ifindex]);
			}

			/* Router level */
			if (strcmp(line, "level") == 0)
			{
				int level = atoi(space+1);
				if (level == 1)
				{
					debuglog("We are a Level 1 router\n");
					send_level2 = 0;
					send_routing = 1;
				}
				if (level == 2)
				{
					debuglog("We are a Level 2 router\n");
					send_level2 = 1;
					send_routing = 1;
				}
			}

			/* Timer */
			if (strcmp(line, "timer") == 0)
			{
				routing_multicast_timer = atoi(space+1);
			}

			/* Manual routes - we ignore these areas */
			if (strcmp(line, "manual") == 0)
			{
				int area = atoi(space+1);
				if (area > 0 && area < 64)
				{
					if (verbose)
						syslog(LOG_INFO, "not setting routes for area %d - manual control requested\n", area);
					area_table[area].manual = 1;
				}

			}
		}
	}
	fclose(fp);
}

/* Send network status down the FIFO */
static void do_show_network(void)
{
	FILE *fp;
	int i;
	int first = 1;
	unsigned char dn_addr[2];
	struct nodeent *ne;
	struct sockaddr saddr;
	socklen_t addrlen;
	int new_fd;

	new_fd = accept(info_socket, &saddr, &addrlen);
	if (new_fd < 0)
		return;

	fp=fdopen(new_fd, "w");
	if (!fp)
	{
		debuglog("Can't send status to FIFO\n");
		return;
	}

	/* Areas */
	if (send_level2)
	{
		for (i=1; i<64; i++)
		{
			if (area_table[i].valid)
			{
				struct nodeinfo *n;
				unsigned short area_node = area_table[i].router;
				char *ifname = "";
				int local = 0;

				if (first)
				{
					first = 0;
					fprintf(fp, "\n     Area     Cost    Hops     Next Hop to Area\n");
				}

				if (!area_node) /* Local */
				{
					area_node = exec_addr->a_addr[1] << 8 | exec_addr->a_addr[0];
					ifname = "(local)";
					local = 1;
				}

				n = dm_hash_lookup_binary(node_hash, (void*)&area_table[i].router, 2);
				dn_addr[0] = area_node & 0xFF;
				dn_addr[1] = area_node>>8;
				ne = getnodebyaddr((const char *)dn_addr, 2, AF_DECnet);

				fprintf(fp, "     %3d      %3d    %3d        %-7s   ->   %2d.%-4d    %s   %s\n",
					i, area_table[i].cost, area_table[i].hops,
					n?if_index_to_name(n->interface):ifname,
					area_node>>10, area_node & 0x03FF,
					ne?ne->n_name:"",
					(area_table[i].manual && !local)?"(M)":"");
			}
		}
	}
	else
	{
		int area = exec_addr->a_addr[1] >> 2;
		unsigned short area_node = area_table[area].router;
		dn_addr[0] = area_node & 0xFF;
		dn_addr[1] = area_node >> 8;
		ne = getnodebyaddr((const char *)dn_addr, 2, AF_DECnet);

		fprintf(fp, "\nThe next hop to the nearest area router is node %d.%d %s\n\n",
			area_node>>10, area_node & 1023,
			ne?ne->n_name:"");
	}

	/* Nodes */
	if (!first)
		fprintf(fp, "\n");
	first = 1;
	for (i=1; i<1023; i++)
	{
		if (node_table[i].valid)
		{
			struct nodeinfo *n;
			unsigned short addr;
			char *nodename = NULL;
			char *routername = NULL;
			unsigned short router;
			int interface = 0;

			addr = exec_addr->a_addr[1] << 8 | i;
			n = dm_hash_lookup_binary(node_hash, (void *)&addr, 2);
			if (n && n->deleted)
				continue;

			dn_addr[0] = addr & 0xFF;
			dn_addr[1] = addr>>8;
			ne = getnodebyaddr((const char *)dn_addr, 2, AF_DECnet);
			if (ne)
				nodename = strdup(ne->n_name);

			/* Is this node behind a level 1 router ? */
			if (node_table[i].router)
			{
				struct nodeinfo *rn;

				router = node_table[i].router;
				dn_addr[0] = router & 0xFF;
				dn_addr[1] = router>>8;
				ne = getnodebyaddr((const char *)dn_addr, 2, AF_DECnet);
				if (ne)
					routername = strdup(ne->n_name);
				rn = dm_hash_lookup_binary(node_hash, (void *)&router, 2);
				if (rn)
					interface = rn->interface;
			}
			else
			{
				router = exec_addr->a_addr[1] << 8 | i;
				routername = nodename;
				if (n)
					interface = n->interface;
			}

			if (first)
			{
				first = 0;
				fprintf(fp, "     Node              Cost    Hops   Next hop to node\n");
			}

			fprintf(fp, "  %2d.%-3d  %-12s  %3d    %3d    %-5s   ->  %2d.%-3d  %-12s\n",
				exec_addr->a_addr[1]>>2, i,
				nodename?nodename:"",
				node_table[i].cost, node_table[i].hops,
				if_index_to_name(interface),
				router>>10, router & 0x3FF,
				routername?routername:"");
		}
	}
	fclose(fp);
}

/* Add or replace a direct route to a node */
static int edit_dev_route(int function, unsigned short node, int interface)
{
	struct {
		struct nlmsghdr         n;
		struct rtmsg            r;
		char                    buf[1024];
	} req;


	if (verbose)
	{
		syslog(LOG_INFO, "%sing route to %d.%d via %s\n",
		       function == RTM_NEWROUTE?"Add":"Remov",
		       node>>10, node&0x3FF, if_index_to_name(interface));
	}

	if (no_routes)
		return 0;

	memset(&req, 0, sizeof(req));
	node = dn_htons(node);

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_REPLACE;
	req.n.nlmsg_type = function;
	req.r.rtm_family = AF_DECnet;
	req.r.rtm_table = RT_TABLE_MAIN;
	req.r.rtm_protocol = RTPROT_DNROUTED;
	req.r.rtm_scope = RT_SCOPE_LINK;
	req.r.rtm_type = RTN_UNICAST;
	req.r.rtm_dst_len = 16;

	ll_init_map(&talk_rth);

	addattr_l(&req.n, sizeof(req), RTA_DST, &node, 2);
	addattr32(&req.n, sizeof(req), RTA_OIF, interface);

	return rtnl_talk(&talk_rth, &req.n, 0, 0, NULL, NULL, NULL);
}


/* Add or replace an indirect route to a node */
static int edit_via_route(int function, unsigned short addr, unsigned short via_node, int bits)
{
	struct {
		struct nlmsghdr         n;
		struct rtmsg            r;
		char                    buf[1024];
	} req;

	if (verbose)
	{
		struct dn_naddr add;
		char   nodename[32];

		add.a_len = 2;
		add.a_addr[0] = via_node & 0xFF;
		add.a_addr[1] = via_node >> 8;
		dnet_ntop(AF_DECnet, &add, nodename, sizeof(nodename));

		syslog(LOG_INFO, "%sing route to %d.%d via %s\n",
		       function == RTM_NEWROUTE?"Add":"Remov",
		       addr>>10, addr&0x3FF, nodename);
	}

	if (no_routes)
		return 0;

	assert (addr>>10);
	assert (via_node);

	memset(&req, 0, sizeof(req));
	via_node = dn_htons(via_node);
	addr = dn_htons(addr);

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_REPLACE;
	req.n.nlmsg_type = function;
	req.r.rtm_family = AF_DECnet;
	req.r.rtm_table = RT_TABLE_MAIN;
	req.r.rtm_protocol = RTPROT_DNROUTED;
	req.r.rtm_scope = RT_SCOPE_UNIVERSE;
	req.r.rtm_type = RTN_UNICAST;
	req.r.rtm_dst_len = bits;

	ll_init_map(&talk_rth);

	addattr_l(&req.n, sizeof(req), RTA_DST, &addr, 2);
	addattr_l(&req.n, sizeof(req), RTA_GATEWAY, &via_node, 2);

	return rtnl_talk(&talk_rth, &req.n, 0, 0, NULL, NULL, NULL);
}

static inline int add_dev_route(unsigned short node, int interface)
{
	/* Don't add a route to any of our addresses */
	if (strcmp(if_index_to_name(interface), "lo") == 0)
		return 0;

	debuglog("adding device route for node %d via interface %d\n", node, interface);
	return edit_dev_route(RTM_NEWROUTE, node, interface);
}

static inline int del_dev_route(unsigned short node, int interface)
{
	return edit_dev_route(RTM_DELROUTE, node, interface);
}

static inline int add_via_route(unsigned short addr, unsigned short via_node)
{
	int bits;

	if ((addr&0x3FF) == 0)/* Area */
	{
		if (area_table[addr>>10].manual == 1)
			return 0;
	       	bits = 6;
	}
	else
	{
		bits = 16;
	}

	return edit_via_route(RTM_NEWROUTE, addr, via_node, bits);
}

static inline int del_via_route(unsigned short addr, unsigned short via_node)
{
	int bits;

	if ((addr&0x3FF) == 0)/* Area */
	{
		if (area_table[addr>>10].manual == 1)
			return 0;
	       	bits = 6;
	}
	else
	{
		bits = 16;
	}
	return edit_via_route(RTM_DELROUTE, addr, via_node, bits);
}


static void set_lowest_cost_route(struct routeinfo *routehead, unsigned short addr)
{
	struct routeinfo *route, *cheaproute = NULL;
	unsigned short cost = 0xFFFF;

	for (route = routehead->next; route; route=route->next)
	{
		if (route->valid &&
		    (route->cost < cost ||
		     ((route->cost == cost && cheaproute && route->priority > cheaproute->priority))))
		{
			cost = route->cost;
			cheaproute = route;
		}
	}

	/* Make it the current route */
	if (cheaproute)
	{
		/* Set route if it's changed */
		if (cheaproute->router != routehead->router)
			add_via_route(addr, cheaproute->router);

		/* Always copy these in case they have changed */
		routehead->hops = cheaproute->hops;
		routehead->cost = cheaproute->cost;
		routehead->router = cheaproute->router;
		routehead->priority = cheaproute->priority;
		routehead->valid = 1;

	}
	else
	{
		/* No more routes to this node/area, we can't reach it. */
		routehead->valid = 0;
		del_via_route(addr, routehead->router);
	}
}

static void add_routeinfo(unsigned short addr, struct routeinfo *routehead, int cost, int hops,
			  unsigned short from_node)
{
	struct routeinfo *route, *lastroute=NULL;
	struct nodeinfo *n;

	/* RSX sometimes leaves hops at zero */
	if (!hops)
		hops = 1;

	/* Look for an entry for our node, adding it if necessary */
	for (route = routehead->next; route; route=route->next)
	{
		lastroute = route;
		if (route->router == from_node)
			break;
	}
	if (!route)
	{
		route = malloc(sizeof(struct routeinfo));
		if (!route)
			return;
		memset(route, 0, sizeof(*route));
		route->router = from_node;

		if (lastroute)
			lastroute->next = route;
		else
			routehead->next = route;
	}
	route->hops = hops;
	route->cost = cost;
	route->valid = 1;

	n = dm_hash_lookup_binary(node_hash, (void*)&from_node, 2);
	if (n)
		route->priority = n->priority;

	/* Then copy the lowest cost route into the first entry
	   and tell the kernel */
	set_lowest_cost_route(routehead, addr);
}

static void add_area_routeinfo(unsigned short area, int cost, int hops, unsigned short from_node)
{
	/* Don't add a local area route if we're the area router */
	if (area == (exec_addr->a_addr[1] >> 2) && send_level2)
	{
		area_table[area].cost = 0;
		area_table[area].hops = 0;
		area_table[area].valid = 1;
		area_table[area].router = 0;
	}
	else
	{
		add_routeinfo(area << 10, &area_table[area], cost, hops, from_node);
	}
}

/* When a routing node goes down, look for alternative routes. */
static void invalidate_route(struct routeinfo *routehead, unsigned short node)
{
	struct routeinfo *route, *cheaproute = NULL;

	for (route = routehead->next; route; route=route->next)
	{
		if (route->router == node)
		{
			route->valid = 0;
		}
	}
	set_lowest_cost_route(routehead, node);
}


/* Called for each neighbour node in the list */
static int got_neigh(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct ndmsg *r = NLMSG_DATA(n);
	struct rtattr * tb[NDA_MAX+1];

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, NDA_MAX, NDA_RTA(r), n->nlmsg_len - NLMSG_LENGTH(sizeof(*r)));

	if (tb[NDA_DST])
	{
		unsigned char *addr = RTA_DATA(tb[NDA_DST]);
		unsigned short faddr = addr[0] | (addr[1]<<8);
		struct nodeinfo *n, *n1;
		int interface = r->ndm_ifindex;
		int node = faddr & 0x3ff;
		int area = faddr >> 10;

		/* Look it up in the hash table */
		n = dm_hash_lookup_binary(node_hash, (void*)&faddr, 2);

		debuglog("Got neighbour node %d.%d on %s(%d woz %d)\n",
			 area, node, if_index_to_name(interface), interface, n?n->interface:0);

		/* If this node has already been scanned then ignore it.
		   This can happen if a node has two NICS on one ethernet
		   and we don't want routes to flip-flop */
		if (n && n->scanned)
			return 0;

		/* If it's not there or the interface has changed then
		   update the routing table */
		if (!n || n->interface != interface || n->deleted)
		{
			if (!n)
			{
				n = malloc(sizeof(struct nodeinfo));
				memset(n, 0, sizeof(*n));
			}
			n->interface = interface;

			/* Update hash table */
			dm_hash_insert_binary(node_hash, (void*)&faddr, 2, n);

			/* Add a route to it */
			add_dev_route(faddr, interface);

			/* If it's in a different area to us then
			   don't add it to the nodes list, add it to the areas
			   instead.
			*/
			if (area != exec_addr->a_addr[1] >> 2)
			{
				if (n->level == 2)
				{
					add_area_routeinfo(area, cost[interface], 1, faddr);
				}
			}
			else
			{
				/* If it exists in the node table with a router then remove the route
				   as the node is now available locally */
				if (node_table[node].router)
				{
					del_via_route(faddr, node_table[node].router);
					node_table[node].router = 0;
					node_table[node].valid = 0; /* Rewrite info */
				}

				/* Update the node table */
				if (strcmp(if_index_to_name(interface), "lo") == 0)
				{
					node_table[node].cost = 0; /* Us, cost = 0, hops = 0 */
					node_table[node].hops = 0;
				}
				else
				{
					if (!node_table[node].valid)
					{
						node_table[node].cost = cost[interface];
						node_table[node].hops = 1;
					}
				}
				node_table[node].valid = 1;
			}
		}
		n->scanned = 1;
		n->deleted = 0;
	}

	return 0;
}

/* Called on a timer, read the neighbours list and send router messages */
static void get_neighbours(void)
{
	struct dm_hash_node *entry;

	if (first_time)
	{
		if (rtnl_open(&talk_rth, 0) < 0)
			return;
		if (rtnl_open(&listen_rth, 0) < 0)
			return;
		first_time = 0;
	}

	/* Get the list of adjacent nodes */
	if (rtnl_wilddump_request(&listen_rth, AF_DECnet, RTM_GETNEIGH) < 0) {
		syslog(LOG_ERR, "Cannot send dump request: %m");
		return;
	}

	/* Calls got_neigh() for each adjacent node */
	if (rtnl_dump_filter(&listen_rth, got_neigh, stdout, NULL, NULL) < 0) {
		syslog(LOG_ERR, "Dump terminated: %m\n");
		return;
	}

	/* Iterate through hash table, removing any unprocessed nodes */
	dm_hash_iterate(entry, node_hash)
	{
		struct nodeinfo *n = dm_hash_get_data(node_hash, entry);
		unsigned short addr = *(unsigned short *)dm_hash_get_key(node_hash, entry);

		if (!n->scanned && !n->deleted)
		{
			/* Remove it */
			debuglog("node %d removed\n", addr);
			del_dev_route(addr, n->interface);

			/* It can no longer route either ! */
			if (n->level)
			{
				/* See if it was an area router (but not our area) */
				if (n->level == 2 && area_table[(addr>>10)].valid && (addr>>10) != (exec_addr->a_addr[1]>>2))
				{
					/* Changes to the next lowest router
				   	   or disables the route */
					invalidate_route(&area_table[addr>>10], addr);
				}
			}

			n->deleted = 1;
		}
		n->scanned = 0;
	}

	if (!no_routes)
	{
		/* Send messages */
		if (send_routing)
			send_level1_msg(node_table);

		if (send_level2)
			send_level2_msg(area_table);
	}
}

static void add_routing_neighbour(unsigned short nodeaddr, int level, int priority, int override)
{
	struct nodeinfo *n;

	// TODO Add if not found ??
	n = dm_hash_lookup_binary(node_hash, (void*)&nodeaddr, 2);
	if (n)
	{
		n->level = level;

		/* Only overwrite priority if it's from a hello message or
		   not currently set */
		if (override || !n->priority)
			n->priority = priority;
	}
}

static void process_level1_message(unsigned char *buf, int len, int iface)
{
	int i;
	int num_ids, start_id;
	int entry;
	unsigned short csum;
	struct dn_naddr add;
	char   node[32];
	int    num;
	unsigned short nodeaddr;

	nodeaddr = (buf[4]<<8) | buf[3];

	add.a_len = 2;
	add.a_addr[0] = buf[3];
	add.a_addr[1] = buf[4];
	dnet_ntop(AF_DECnet, &add, node, sizeof(node));

	csum = route_csum(buf, 6, len-2);

	/* Ignore level 1 messages from other areas */
	if (buf[4]>>2 != exec_addr->a_addr[1]>>2)
		return;

	if (csum != (buf[len-2] | buf[len-1]<<8))
	{
		syslog(LOG_INFO, "Bad checksum in level 1 routing message from %s on %s\n", node, if_index_to_name(iface));
		debuglog("Bad checksum in level 1 routing message from %s on %d\n", node, iface);
		return;
	}

	debuglog("Level 1 routing message from %s on %s len = %d\n", node, if_index_to_name(iface), len);

	/* Look for nodes that are not in our neighbour list and add routes for them */
	i=6;
	while (i < len-4)
	{
		num_ids = buf[i] | buf[i+1]<<8;
		i+=2;
		start_id = buf[i] | buf[i+1]<<8;
		i+=2; /* Start of entries */
		debuglog("CC start_id = %d, num_ids = %d pos=%d\n", start_id, num_ids, i);

		for (num = 0; num<num_ids; num++)
		{
			entry = buf[i] | buf[i+1]<<8;
			if ((entry & 0xFF) != 0xff && (num+start_id != 0))
			{
				struct nodeinfo *n;
				unsigned short remote_addr = ((exec_addr->a_addr[1]&0xFC) << 8) | (num+start_id);
				int n_hops, n_cost;

				n_hops = (entry&0x7E00)>>9;
				n_cost = entry&0x1FF;

				debuglog("  node %d reachable Hops %d, cost %d\n",
					 num+start_id, n_hops, n_cost);

				n = dm_hash_lookup_binary(node_hash, (void*)&remote_addr, 2);
				if (num+start_id > 1023) // Should not happen...but....
					return;

				/* We can't see this node but the router can - add a route for it */
				if (!n || n->deleted)
				{
					/* Clear the interface so if it turns up in the neighbout table
					   we can do the right thing */
					if (n)
						n->interface = 0;

					debuglog("Adding route to node %d via %d\n", remote_addr, nodeaddr);

					/* Add our own cost to the router */
					n_hops++;
					n_cost += cost[iface];

					add_routeinfo(remote_addr, &node_table[num+start_id],
						      n_cost, n_hops, nodeaddr);
				}

			}
			i+=2;
		}
	}
}

static void process_level2_message(unsigned char *buf, int len, int iface)
{
	int i;
	int num_ids, start_id;
	int entry;
	unsigned short csum;
	struct dn_naddr add;
	char   node[32];
	int    num;
	unsigned short nodeaddr;

	add.a_len = 2;
	add.a_addr[0] = buf[3];
	add.a_addr[1] = buf[4];

	nodeaddr = (buf[4]<<8) | buf[3];

	dnet_ntop(AF_DECnet, &add, node, sizeof(node));

	csum = route_csum(buf, 6, len-2);
	if (csum != (buf[len-2] | buf[len-1]<<8))
	{
		syslog(LOG_INFO, "Bad checksum in level 2 routing message from %s on %s\n",
		       node, if_index_to_name(iface));
		debuglog("Bad checksum in level 2 routing message from %s on %d\n", node, iface);
		return;
	}

	debuglog("Level 2 routing message from %s(%d) on %s len = %d\n",
		 node, nodeaddr, if_index_to_name(iface), len);

	/* In case we don't get a router hello in time, some nodes seem
	   not to send them (RSX).  Assign a default priority too. */
	add_routing_neighbour(nodeaddr, 2, 64, 0);

	/* Add areas to areas list, and add a route to all non-local areas */
	i=6;
	while (i < len-4)
	{
		num_ids = buf[i] | buf[i+1]<<8;
		i+=2;
		start_id = buf[i] | buf[i+1]<<8;
		i+=2; /* Start of entries */

		debuglog("CC start_id = %d, num_ids = %d\n", start_id, num_ids);
		for (num = 0; num<num_ids; num++)
		{
			entry = buf[i] | buf[i+1]<<8;
			if ((entry & 0xFF) != 0xff && (num+start_id != 0))
			{
				struct nodeinfo *n;

				debuglog("  Area %d reachable Hops %d, cost %d (pos=%d)\n",
					 num+start_id, (entry&0x7E00)>>9, entry&0x1FF, i);

				if (num+start_id > 63 || num+start_id < 1) // Should not happen...but....
					return;

				n = dm_hash_lookup_binary(node_hash, (void*)&nodeaddr, 2);
				if (n)
					add_area_routeinfo(num+start_id,
							   (entry&0x1FF) + cost[n->interface], ((entry&0x7E00)>>9),
							   nodeaddr);
			}
			i+=2;
		}
	}
}

static void process_routing_message(unsigned char *buf, int len, int iface)
{

	/* Level 1 Routing Message */
	if ( (buf[2] & 0xE)>>1 == 3)
	{
		process_level1_message(buf, len, iface);
	}

	/* Level 2 Routing Message */
	if ( (buf[2] & 0xE)>>1 == 4)
	{
		process_level2_message(buf, len, iface);
	}

	/* Router Hello Message */
	if ( (buf[2] & 0xE)>>1 == 5)
	{
		unsigned short nodeaddr;
		int level;
		int priority;
		struct nodeinfo *n;

		nodeaddr = (buf[11]<<8) | buf[10];
		level = 3-(buf[12] & 0x3);
		priority = buf[15];

		debuglog("Got router hello from %d on %s, level = %d, prio = %d\n",
			 nodeaddr, if_index_to_name(iface), level, priority);

		/* Add to (or update) neighbour hash */
		add_routing_neighbour(nodeaddr, level, priority, 1);
	}
}

static void usage(char *cmd, FILE *f)
{

	fprintf(f, "\nusage: %s [OPTIONS]\n\n", cmd);
	fprintf(f, " -h           Print this help message\n");
	fprintf(f, " -d           Don't fork into background\n");
	fprintf(f, " -D           Show lots of debugging information on stderr\n");
	fprintf(f, " -n           Do not set routes (used for testing or network monitoring)\n");
	fprintf(f, " -2           Send DECnet routing level 2 messages (implies -r)\n");
	fprintf(f, " -r           Send DECnet routing level 1 messages\n");
	fprintf(f, " -t<secs>     Time between routing messages (default 15)\n");
	fprintf(f, " -V           Show program version\n");
	fprintf(f, "\n");
}

int main(int argc, char **argv)
{
	int opt;
	struct timespec ts;
	int no_daemon=0;
	mode_t oldmode;
	struct sockaddr_un sockaddr;

	/* Initialise the node hash table */
	node_hash = dm_hash_create(1024);

	/* Mark everyone unavailable */
	memset(node_table, 0, sizeof(node_table));
	memset(area_table, 0, sizeof(area_table));
	memset(cost, 4, sizeof(cost)); /* Default cost is 4 per device */

	/* Get our node address */
	exec_addr = getnodeadd();

	/* Do this first so that command-line options override the config */
	read_conffile();

	while ((opt=getopt(argc,argv,"?VvhrdDnt:2")) != EOF)
	{
		switch(opt) {
		case 'h':
			usage(argv[0], stdout);
			exit(0);

		case '?':
			usage(argv[0], stderr);
			exit(0);

		case 'D':
			debugging++;
			break;

		case 'v':
			verbose++;
			break;

		case 'd':
			no_daemon++;
			break;

		case 'r':
			send_routing++;
			break;

		case '2':
			send_level2++;
			send_routing++;
			break;

		case 't':
			routing_multicast_timer = atoi(optarg);
			break;

		case 'n':
			no_routes++;
			break;

		case 'V':
			printf("\ndnroute from dnprogs version %s\n\n", VERSION);
			exit(1);
			break;
		}
	}

	if (!no_daemon)
	{
		pid_t pid;
		int devnull;

		switch ( pid=fork() )
		{
		case -1:
			perror("dnroute: can't fork");
			exit(2);

		case 0: /* child */
			break;

		default: /* Parent */
			debuglog("server: forked process %d\n", pid);
			exit(0);
		}

		/* There should only be one of us */
		pidfile_create(PIDFILE, getpid());

		/* Detach ourself from the calling environment */
		devnull = open("/dev/null", O_RDWR);
		close(0);
		close(1);
		close(2);
		setsid();
		dup2(devnull, 0);
		dup2(devnull, 1);
		dup2(devnull, 2);
		chdir("/");
	}

	signal(SIGTERM, term_sig);
	signal(SIGINT, term_sig);
	signal(SIGPIPE, SIG_IGN);

	oldmode = umask(0);
	chmod(STATUS_SOCKET, 0660);
	umask(oldmode);

	signal(SIGUSR1, usr1_sig);
	signal(SIGALRM, alarm_sig);

	/* Socket for sending "SHOW NETWORK" information */
	unlink(STATUS_SOCKET);
	info_socket = socket(AF_UNIX, SOCK_STREAM, PF_UNIX);
	if (info_socket < 0)
	{
		syslog(LOG_ERR, "Unable to open Unix socket for information output: %m\n");
		return 1;
	}
	fcntl(info_socket, F_SETFL, fcntl(info_socket, F_GETFL, 0) | O_NONBLOCK);

	strcpy(sockaddr.sun_path, STATUS_SOCKET);
	sockaddr.sun_family = AF_UNIX;
	if (bind(info_socket, (struct sockaddr *)&sockaddr, sizeof(sockaddr)))
	{
		syslog(LOG_ERR, "Unable to bind Unix socket for information output: %m\n");
		return 1;
	}
	/* Wait for connections */
	listen(info_socket, 5);

	/* Socket for listening for routing messages
	   and sending our own */
	dnet_socket = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_DNA_RT));
	if (dnet_socket < 0)
	{
		syslog(LOG_ERR, "Unable to open packet socket for DECnet routing messages: %m\n");
		return 1;
	}

	fcntl(dnet_socket, F_SETFL, fcntl(dnet_socket, F_GETFL, 0) | O_NONBLOCK);

	/*
	 * Add an entry for our area. If we are a level2 router, then it's us.
	 */
	if (send_level2)
	{
		area_table[exec_addr->a_addr[1]>>2].router = 0;
		area_table[exec_addr->a_addr[1]>>2].valid = 1;
	}

	/* Set the local area to "manual control" so we don't force all
	   traffic through a local router */
	area_table[exec_addr->a_addr[1]>>2].manual = 1;

	/* Start it off */
	get_neighbours();
	alarm(routing_multicast_timer);

	/* Process routing messages */
	running = 1;
	while (running)
	{
		fd_set fds;
		unsigned char buf[2048];
		sigset_t ss;
		int len;
		int status;

		FD_ZERO(&fds);
		FD_SET(dnet_socket, &fds);
		FD_SET(info_socket, &fds);
		sigfillset(&ss);
		sigdelset(&ss, SIGUSR1);
		sigdelset(&ss, SIGALRM);
		sigdelset(&ss, SIGTERM);
		sigdelset(&ss, SIGINT);

		status = pselect(FD_SETSIZE, &fds, NULL, NULL, NULL, &ss);

		if (running)
		{
			if (FD_ISSET(dnet_socket, &fds))
			{
				struct sockaddr_ll sll;
				unsigned int sll_len = sizeof(sll);
				len = recvfrom(dnet_socket, buf, sizeof(buf), 0, (struct sockaddr *)&sll, &sll_len);
				if (len > 0)
					process_routing_message(buf, len, sll.sll_ifindex);
				if (len < 0)
					syslog(LOG_ERR, "Error reading DECnet messages: %m\n");
			}

			if (FD_ISSET(info_socket, &fds))
			{
				do_show_network();
			}

			/* Things that interrupt our sleep */
			if (alarm_rang)
			{
				get_neighbours();
				alarm_rang = 0;
			}
		}
	}
	close(dnet_socket);
	close(info_socket);
	exit(0);
}
