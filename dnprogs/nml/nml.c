/******************************************************************************
    (c) 2008 Christine Caulfield            christine.caulfield@googlemail.com

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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "libnetlink.h"

/* Sigh - people keep removing features ... */
#ifndef NDA_RTA
#define NDA_RTA(r)  ((struct rtattr*)(((char*)(r)) + NLMSG_ALIGN(sizeof(struct ndmsg))))
#endif

#define IDENT_STRING "DECnet for Linux"

#define PROC_DECNET "/proc/net/decnet"
#define PROC_DECNET_DEV "/proc/net/decnet_dev"

#define TYPE_NODE             0
#define TYPE_LINE             1
#define TYPE_LOGGING          2
#define TYPE_CIRCUIT          3
#define TYPE_MODULE           4
#define TYPE_AREA             5

#define NODESTATE_UNKNOWN    -1
#define NODESTATE_ON          0
#define NODESTATE_OFF         1
#define NODESTATE_SHUT        2
#define NODESTATE_RESTRICTED  3
#define NODESTATE_REACHABLE   4
#define NODESTATE_UNREACHABLE 5

#define FORMAT_SIGNIFICANT   251
#define FORMAT_ADJACENT      252
#define FORMAT_LOOP          253
#define FORMAT_ACTIVE        254
#define FORMAT_KNOWN         255
#define FORMAT_NUMBER          0

typedef int (*neigh_fn_t)(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg);

static struct rtnl_handle talk_rth;
static struct rtnl_handle listen_rth;
static int first_time = 1;
static int verbose;
static unsigned short local_node, router_node;

#define MAX_ADJACENT_NODES 1024
static int num_adj_nodes = 0;
static unsigned short adj_node[MAX_ADJACENT_NODES];

static int num_link_nodes = 0;
static struct link_node
{
        unsigned char node, area;
        unsigned int links;
} link_nodes[MAX_ADJACENT_NODES];

// Object definition from dnetd.conf
#define USERNAME_LENGTH 65
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

struct object
{
    char  name[USERNAME_LENGTH]; // Object name
    unsigned int number;         // Object number
    int  proxy;                 // Whether to use proxies
    char  user[USERNAME_LENGTH]; // User to use if proxies not used
    char  daemon[PATH_MAX];      // Name of daemon

    struct object *next;
};
static struct object *object_db = NULL;


static void makeupper(char *s)
{
        int i;
        for (i=0; s[i]; i++) s[i] = toupper(s[i]);
}

static int adjacent_node(struct nodeent *n)
{
    int i;
    unsigned short nodeid = n->n_addr[0] | n->n_addr[1]<<8;

    for (i=0; i<num_adj_nodes; i++)
        if (adj_node[i] == nodeid)
            return 1;

    return 0;
}

static char *if_index_to_name(int ifindex)
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


/* Get the router address from /proc */
static unsigned short get_router(void)
{
        char buf[256];
        char var1[32];
        char var2[32];
        char var3[32];
        char var4[32];
        char var5[32];
        char var6[32];
        char var7[32];
        char var8[32];
        char var9[32];
        char var10[32];
        char var11[32];
        unsigned short router = 0;
        FILE *procfile = fopen(PROC_DECNET_DEV, "r");

        if (!procfile)
                return 0;
        while (!feof(procfile))
        {
                if (!fgets(buf, sizeof(buf), procfile))
                        break;
                if (sscanf(buf, "%s %s %s %s %s %s %s %s %s ethernet %s\n",
                           var1,var2,var3,var4,var5,var6,var7,var8,var9,var11) == 10)
                {
                        int area, node;
                        sscanf(var11, "%d.%d\n", &area, &node);
                        router = area<<10 | node;
                        break;
                }
        }
        fclose(procfile);
        dnetlog(LOG_DEBUG, "Router node is %x\n", router);
        return router;
}

/* This assumes that count_links() has already ben called */
static int get_link_count(unsigned char addr1, unsigned char addr2)
{
        int node = addr1 | (addr2<<8 & 0x3);
        int area = addr2 >> 2;
        int i;

        for (i=0; i<num_link_nodes; i++) {
                if (link_nodes[i].area == area &&
                    link_nodes[i].node == node) {
                        return link_nodes[i].links;
                }
        }
        return 0;
}

/* Send a single node */
static int send_node(int sock, struct nodeent *n, int exec, char *device, int state)
{
        char buf[1024];
        int ptr = 0;

        makeupper(n->n_name);
        buf[ptr++] = 1; // Here is your data miss
        buf[ptr++] = 0;
        buf[ptr++] = 0; // Status
        buf[ptr++] = 0; // Node information
        buf[ptr++] = n->n_addr[0];
        buf[ptr++] = n->n_addr[1];
        buf[ptr++] = strlen(n->n_name) | (exec?0x80:0);
        memcpy(&buf[ptr], n->n_name, strlen(n->n_name));
        ptr += strlen(n->n_name);

        /* Device */
        if (device) {
                buf[ptr++] = 0x36;  //
                buf[ptr++] = 0x3;   // 822=CIRCUIT

                buf[ptr++] = 0x40;  // ASCII text
                buf[ptr++] = strlen(device);
                strcpy(&buf[ptr], device);
                ptr += strlen(device);
        }

        /* Node State */
        if (state != NODESTATE_UNKNOWN) {
                struct nodeent *rn;
                struct nodeent scratch_n;
                unsigned char scratch_na[2];
                int links;

                scratch_na[0] = router_node & 0xFF;
                scratch_na[1] = router_node >>8;

                buf[ptr++] = 0;   // 0=Node state
                buf[ptr++] = 0;

                buf[ptr++] = 0x81; // Data type & length of 'state'
                buf[ptr++] = state;

                /* For reachable nodes show the next node, ie next router
                   it itself */
                if (state == NODESTATE_REACHABLE) {
                        if (((n->n_addr[0] | n->n_addr[1]<<8) & 0xFC00) !=
                            (router_node & 0xFC00)) {
                                rn = getnodebyaddr((char *)scratch_na, 2, AF_DECnet);
                                if (!rn) {
                                    rn = &scratch_n;
                                    scratch_n.n_addr = scratch_na;
                                    scratch_n.n_name = NULL;
                                }
                        }
                        else {
                                rn = n;
                        }

                        /* We might not know the router yet */
                        if (rn->n_addr[0] && rn->n_addr[1]) {
                                buf[ptr++] = 0x3e;  // 830=NEXT NODE
                                buf[ptr++] = 0x03;

                                if (rn->n_name && strlen(rn->n_name)) {
                                        buf[ptr++] = 0xC2;      // CM-2
                                        buf[ptr++] = 0x02;
                                        buf[ptr++] = rn->n_addr[0];
                                        buf[ptr++] = rn->n_addr[1];

                                        buf[ptr++] = 0x40;      // ASCII text
                                        makeupper(rn->n_name);
                                        buf[ptr++] = strlen(rn->n_name);
                                        strcpy(&buf[ptr], rn->n_name);
                                        ptr += strlen(rn->n_name);
                                }
                                else {
                                        buf[ptr++] = 0xC1;      // CM-1
                                        buf[ptr++] = 0x02;
                                        buf[ptr++] = rn->n_addr[0];
                                        buf[ptr++] = rn->n_addr[1];
                                }
                        }

                        // Also show the number of active links
                        links = get_link_count(n->n_addr[0], n->n_addr[1]);
                        if (links) {
                                buf[ptr++] = 0x58; // 600=Active links
                                buf[ptr++] = 0x2;  // Unsigned decimal

                                buf[ptr++] = 2;    // Data length
                                buf[ptr++] = links & 0xFFFF;
                                buf[ptr++] = links >> 16;
                        }
                }
        }

        /* Show exec details, name etc */
        if (exec) {
                struct utsname un;
                char ident[256];

                uname(&un);
                sprintf(ident, "%s V%s on %s", IDENT_STRING, un.release, un.machine);

                buf[ptr++] = 0x64;
                buf[ptr++] = 0;     // 100=Identification

                buf[ptr++] = 0x40;  // ASCII text
                buf[ptr++] = strlen(ident);
                strcpy(&buf[ptr], ident);
                ptr += strlen(ident);
        }
        return write(sock, buf, ptr);
}

/* Get the bits for SHOW EXECUTOR */
static int send_exec(int sock)
{
        struct dn_naddr *exec_addr;
        struct nodeent *exec_node;
        char *dev;

        /* Get and send the exec information */
        exec_addr = getnodeadd();
        exec_node = getnodebyaddr((char*)exec_addr->a_addr, 2, AF_DECnet);
        dev = getexecdev();

        return send_node(sock, exec_node, 1, dev, NODESTATE_ON);
}

/* Save a neighbour entry in a list so we can check it when doing the
   KNOWN NODES display
 */
static int save_neigh(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
        struct ndmsg *r = NLMSG_DATA(n);
        struct rtattr * tb[NDA_MAX+1];
        int sock = (intptr_t)arg;

        memset(tb, 0, sizeof(tb));
        parse_rtattr(tb, NDA_MAX, NDA_RTA(r), n->nlmsg_len - NLMSG_LENGTH(sizeof(*r)));

        if (tb[NDA_DST])
        {
            unsigned char *addr = RTA_DATA(tb[NDA_DST]);
            if (++num_adj_nodes < MAX_ADJACENT_NODES)
                adj_node[num_adj_nodes] = addr[0] | (addr[1]<<8);
        }
        return 0;
}

/* Called for each neighbour node in the list - send it to the socket */
static int send_neigh(struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
        struct ndmsg *r = NLMSG_DATA(n);
        struct rtattr * tb[NDA_MAX+1];
        int sock = (intptr_t)arg;

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
                struct nodeent *ne;
                char *devname = if_index_to_name(interface);

                /* Do not consider ourselves or loopback device adjacent */
                if ((faddr == local_node) || (strcmp(devname, "lo") == 0))
                        return 0;

                ne = getnodebyaddr((char *)addr, 2, AF_DECnet);

                if (ne) {
                        send_node(sock, ne, 0, if_index_to_name(interface), NODESTATE_REACHABLE);
                }
                else {
                        struct nodeent tne;
                        unsigned char tne_addr[2];

                        tne.n_name = "";
                        tne.n_addr = tne_addr;
                        tne.n_addr[0] = addr[0];
                        tne.n_addr[1] = addr[1];
                        send_node(sock, &tne, 0, if_index_to_name(interface), NODESTATE_REACHABLE);
                }
        }
        return 0;
}

/* SHOW ADJACENT NODES */
static int get_neighbour_nodes(int sock, neigh_fn_t neigh_fn)
{
        router_node = get_router();

        if (first_time)
        {
                if (rtnl_open(&talk_rth, 0) < 0)
                        return -1;
                if (rtnl_open(&listen_rth, 0) < 0)
                        return -1;
                first_time = 0;
        }

        /* Get the list of adjacent nodes */
        if (rtnl_wilddump_request(&listen_rth, AF_DECnet, RTM_GETNEIGH) < 0) {
                syslog(LOG_ERR, "Cannot send dump request: %m");
                return -1;
        }

        /* Calls got_neigh() for each adjacent node */
        if (rtnl_dump_filter(&listen_rth, neigh_fn, (void *)(intptr_t)sock, NULL, NULL) < 0) {
                syslog(LOG_ERR, "Dump terminated: %m\n");
                return -1;
        }
        dnetlog(LOG_DEBUG, "end of get_neighbour_nodes\n");
        return 0;
}

/* SHOW/LIST KNOWN NODES */
static int send_all_nodes(int sock, unsigned char perm_only)
{
        void *nodelist;
        char *nodename;

        send_exec(sock);

        /* Get adjacent nodes */
        num_adj_nodes = 0;
        if (!perm_only)
            get_neighbour_nodes(sock, save_neigh);

        /* Now iterate the permanent database */
        nodelist = dnet_getnode();
        nodename = dnet_nextnode(nodelist);
        while (nodename)
        {
                struct nodeent *n = getnodebyname(nodename);

                send_node(sock, n, 0, NULL, adjacent_node(n)?NODESTATE_REACHABLE:NODESTATE_UNKNOWN);
                nodename = dnet_nextnode(nodelist);
        }
        dnet_endnode(nodelist);
        return 0;
}

/*
 * We read /proc/net/decnet and fill in the number of links to each node
 * that we find
 */
static int count_links(void)
{
        char buf[256];
        char var1[32];
        char var2[32];
        char var3[32];
        char var4[32];
        char var5[32];
        char var6[32];
        char var7[32];
        char var8[32];
        char var9[32];
        char var10[32];
        char var11[32];
        int i;
        FILE *procfile = fopen(PROC_DECNET, "r");

        if (!procfile)
                return 0;

        while (!feof(procfile))
        {
                if (!fgets(buf, sizeof(buf), procfile))
                        break;
                if (sscanf(buf, "%s %s %s %s %s %s %s %s %s %s %s\n",
                           var1,var2,var3,var4,var5,var6,var7,var8, var9, var10, var11) == 11) {
                        int area, node;
                        struct link_node *lnode = NULL;

                        sscanf(var6, "%d.%d\n", &area, &node);

                        /* Ignore 0.0 links (listeners) and anything not in RUN state */
                        if (area == 0 || node == 0 || strcmp(var11, "RUN"))
                                continue;

                        for (i=0; i<num_link_nodes; i++) {
                                if (link_nodes[i].area == area &&
                                    link_nodes[i].node == node) {
                                        lnode = &link_nodes[i];
                                        break;
                                }
                        }
                        if (!lnode && i < MAX_ADJACENT_NODES) {
                                lnode = &link_nodes[num_link_nodes++];
                                lnode->area = area;
                                lnode->node = node;
                        }
                        if (lnode) {
                                lnode->links++;
                        }
                }
        }
        fclose(procfile);
        return 0;
}

/* Copied from libdnet_daemon ... bad girl */
static int load_dnetd_conf(void)
{
    FILE          *f;
    char           buf[4096];
    int            line;
    struct object *last_object = NULL;

    f = fopen("/etc/dnetd.conf", "r");
    if (!f)
    {
        DNETLOG((LOG_ERR, "Can't open dnetd.conf database: %s\n",
                 strerror(errno)));
        return -1;
    }

    line = 0;

    while (!feof(f))
    {
        char tmpbuf[1024];
        char *bufp;
        char *comment;
        struct object *newobj;
        int    state = 1;

        line++;
        if (!fgets(buf, sizeof(buf), f)) break;

        // Skip whitespace
        bufp = buf;
        while (*bufp == ' ' || *bufp == '\t') bufp++;

        if (*bufp == '#') continue; // Comment

        // Remove trailing LF
        if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';

        // Remove any trailing comments
        comment = strchr(bufp, '#');
        if (comment) *comment = '\0';

        if (*bufp == '\0') continue; // Empty line

        // Split into fields
        newobj = malloc(sizeof(struct object));
        state = 1;
        bufp = strtok(bufp, " \t");
        while(bufp)
        {
            char *nextspace = bufp+strlen(bufp);
            if (*nextspace == ' ' || *nextspace == '\t') *nextspace = '\0';
            switch (state)
            {
            case 1:
                strcpy(newobj->name, bufp);
                break;
            case 2:
                strcpy(tmpbuf, bufp);
                if ( strcmp(tmpbuf, "*") == 0 ) {
                    if ( strcmp(newobj->name, "*") == 0 ) {
                        newobj->number = 0;
                    } else {
                        newobj->number = getobjectbyname(newobj->name);
                    }
                } else {
                    newobj->number = atoi(tmpbuf);
                }

                break;
            case 3:
                strcpy(tmpbuf, bufp);
                newobj->proxy = (toupper(tmpbuf[0])=='Y'?TRUE:FALSE);
                break;
            case 4:
                strcpy(newobj->user, bufp);
                break;
            case 5:
                strcpy(newobj->daemon, bufp);
                break;
            default:
                // Copy parameters
                strcat(newobj->daemon, " ");
                strcat(newobj->daemon, bufp);
                break;
            }
            bufp = strtok(NULL, " \t");
            state++;
        }

        // Did we get all the info ?
        if (state > 5)
        {
            // Add to the list
            if (last_object)
            {
                last_object->next = newobj;
            }
            else
            {
                object_db = newobj;
            }
            last_object = newobj;
        }
        else
        {
            DNETLOG((LOG_ERR, "Error in dnet.conf line %d, state = %d\n", line, state));
            free(newobj);
        }
    }
    return 0;
}

/* SHOW KNOWN OBJECTS */
static int send_objects(int sock)
{
        struct object *obj;
        char buf[256];
        char response;
        int ptr;

        if (load_dnetd_conf()) {
                buf[0] = -3; // Privilege violation
                buf[1] = 0; // Privilege violation
                buf[2] = 0; // Privilege violation
                write(sock, buf, 3);
                return -1;
        }

        response = 2;
        write(sock, &response, 1);

        obj = object_db;
        while (obj) {
                dnetlog(LOG_DEBUG, "object %s (%d)\n", obj->name, obj->number);

                ptr = 0;
                buf[ptr++] = 1;

                buf[ptr++] = 0xff; // Object Name
                buf[ptr++] = 0xff;
                buf[ptr++] = 0x0;
                buf[ptr++] = strlen(obj->name);
                strcpy(&buf[ptr], obj->name);
                ptr+=strlen(obj->name);

                buf[ptr++] = 0x01;   // 513 Object number (not in NETMAN40.txt)
                buf[ptr++] = 0x02;
                buf[ptr++] = 0x01;
                buf[ptr++] = obj->number;

                if (obj->daemon) {
                        buf[ptr++] = 0x12; // 530 FILE  (not in NETMAN40.txt)
                        buf[ptr++] = 0x02;
                        buf[ptr++] = 0x40; // ASCII text
                        buf[ptr++] = strlen(obj->daemon);
                        strcpy(&buf[ptr], obj->daemon);
                        ptr+=strlen(obj->daemon);
                }

                if (!obj->proxy) {
                        buf[ptr++] = 0x26; // 550 username  (not in NETMAN40.txt)
                        buf[ptr++] = 0x02;
                        buf[ptr++] = 0x40; // ASCII Text
                        buf[ptr++] = strlen(obj->user);
                        strcpy(&buf[ptr], obj->user);
                        ptr+=strlen(obj->user);
                }
                write(sock, buf, ptr);

                obj = obj->next;
        }

        response = -128;
        write(sock, &response, 1);
        return 0;
}

/* SHOW KNOWN LINKS */
static int send_links(int sock)
{
        char inbuf[256];
        char buf[256];
        char var1[32];
        char var2[32];
        char var3[32];
        char var4[32];
        char luser[32];
        char var6[32];
        char var7[32];
        char var8[32];
        char var9[32];
        char ruser[32];
        char state[32];
        int i;
        char response;
        int ptr = 0;
        FILE *procfile = fopen(PROC_DECNET, "r");

        if (!procfile)
                return 0;

        response = 2;

        // Tell remote end we are sending the data.
        write(sock, &response, 1);

        while (!feof(procfile))
        {
                if (!fgets(inbuf, sizeof(inbuf), procfile))
                        break;
                if (sscanf(inbuf, "%s %s %s %s %s %s %s %s %s %s %s\n",
                           var1,var2,var3,var4,luser,var6,var7,var8, var9, ruser, state) == 11) {
                        int area, node;
                        int llink, rlink;
                        unsigned char scratch_na[2];
                        struct nodeent *nent;
                        int objnum;

                        /* We're only interested in the remote node address here
                           but want both link numbers */
                        sscanf(var1, "%d.%d/%x\n", &area, &node, &llink);
                        sscanf(var6, "%d.%d/%x\n", &area, &node, &rlink);

                        /* Ignore 0.0 links (listeners) and anything not in RUN state */
                        if (area == 0 || node == 0 || strcmp(state, "RUN"))
                                continue;

                        dnetlog(LOG_DEBUG, "node %d.%d links %d & %d state=%s\n", area,node, llink,rlink, state);

                        /* Get remote node name */
                        scratch_na[1] = area<<2 | node>>8;
                        scratch_na[0] = node & 0xFF;
                        nent = getnodebyaddr((char *)scratch_na, 2, AF_DECnet);

                        /* We don't really show users as such for remote connections,
                           so make the object numbers look friendlier */
                        objnum = atoi(luser);
                        if (objnum)
                                getobjectbynumber(objnum, luser, sizeof(luser));

                        objnum = atoi(ruser);
                        if (objnum)
                                getobjectbynumber(objnum, ruser, sizeof(ruser));

                        ptr = 0;
                        buf[ptr++] = 1; // Here is your data miss

                        buf[ptr++] = 0xff;
                        buf[ptr++] = 0xff; // Local link (seems to be compulsory!
                        buf[ptr++] = 0;
                        buf[ptr++] = 0;
                        buf[ptr++] = rlink & 0xFF;
                        buf[ptr++] = rlink>>8;

                        buf[ptr++] = 120; // Remote link
                        buf[ptr++] = 0;
                        buf[ptr++] = 2;
                        buf[ptr++] = llink & 0xFF;
                        buf[ptr++] = llink>>8;

                        buf[ptr++] = 0x79; // Remote user
                        buf[ptr++] = 0;
                        buf[ptr++] = 0x40;
                        buf[ptr++] = strlen(ruser);
                        memcpy(&buf[ptr], ruser, strlen(ruser));
                        ptr += strlen(ruser);

                        buf[ptr++] = 131; // Local process
                        buf[ptr++] = 0;
                        buf[ptr++] = 0x40;
                        buf[ptr++] = strlen(luser);
                        memcpy(&buf[ptr], luser, strlen(luser));
                        ptr += strlen(luser);

                        if (nent) {
                                buf[ptr++] = 0x66; // 0x66 Remote node (addr+name)
                                buf[ptr++] = 0x00;
                                buf[ptr++] = 0xc2; // CM-2
                                buf[ptr++] = 0x02;
                                buf[ptr++] = scratch_na[0];
                                buf[ptr++] = scratch_na[1];
                                buf[ptr++] = 0x40; // Text data
                                buf[ptr++] = strlen(nent->n_name);
                                makeupper(nent->n_name);
                                memcpy(&buf[ptr], nent->n_name, strlen(nent->n_name));
                                ptr += strlen(nent->n_name);
                        }
                        write(sock, buf, ptr);
                }
        }
        fclose(procfile);

        // End of data.
        response = -128;
        write(sock, &response, 1);
        return 0;
}

/* SHOW KNOWN/ACTIVE CIRCUITS */
static int send_circuits(int sock)
{
        char buf[256];
        char var1[32];
        char var2[32];
        char var3[32];
        char var4[32];
        char var5[32];
        char var6[32];
        char var7[32];
        char var8[32];
        char var9[32];
        char var10[32];
        char var11[32];
        int count;
        int ptr;
        FILE *procfile = fopen(PROC_DECNET_DEV, "r");

        if (!procfile)
                return 0;

        while (!feof(procfile))
        {
                if (!fgets(buf, sizeof(buf), procfile))
                        break;

                count = sscanf(buf, "%s %s %s %s %s %s %s %s %s %s %s\n",
                               var1, var2, var3, var4, var5, var6, var7,
                               var8, var9, var10, var11);

                if (strcmp(var10, "ethernet"))
                        continue;

                if ((count == 10) || (count == 11))
                {
                        dnetlog(LOG_DEBUG, "name=%s, count=%d\n",
                                var1, count);
                        ptr = 0;
                        buf[ptr++] = 1;

                        buf[ptr++] = 0xFF;
                        buf[ptr++] = 0xFF;
                        buf[ptr++] = 0x0;
                        buf[ptr++] = strlen(var1);
                        strcpy(&buf[ptr], var1);
                        ptr += strlen(var1);

                        buf[ptr++] = 0x0;       // Circuit state = on
                        buf[ptr++] = 0x0;
                        buf[ptr++] = 0x81;
                        buf[ptr++] = 0;

                        if (count == 11)
                        {
                                struct nodeent *rn;
                                int area, node;
                                unsigned short addr;

                                sscanf(var11, "%d.%d\n", &area, &node);
                                addr = (area << 10) | node;
                                rn = getnodebyaddr((char *)&addr, 2, PF_DECnet);

                                buf[ptr++] = 0x20;      // 800=ADJACENT NODE
                                buf[ptr++] = 0x03;

                                if (rn && rn->n_name) {
                                        buf[ptr++] = 0xC2;      // CM-2
                                        buf[ptr++] = 0x02;
                                        buf[ptr++] = addr & 0xFF;
                                        buf[ptr++] = (addr >> 8) & 0xFF;

                                        buf[ptr++] = 0x40;      // ASCII text
                                        makeupper(rn->n_name);
                                        buf[ptr++] = strlen(rn->n_name);
                                        strcpy(&buf[ptr], rn->n_name);
                                        ptr += strlen(rn->n_name);
                                }
                                else {
                                        buf[ptr++] = 0xC1;      // CM-1
                                        buf[ptr++] = 0x2;
                                        buf[ptr++] = addr & 0xFF;
                                        buf[ptr++] = (addr >> 8) & 0xFF;
                                }
                        }

                        write(sock, buf, ptr);
                }
        }
        fclose(procfile);
        return 0;
}

/* SHOW NODE Lists */
static int read_information(int sock, unsigned char *buf, int length)
{
        unsigned char option = buf[1];
        unsigned char entity = buf[2];

        char response = 2;

        // Tell remote end we are sending the data.
        write(sock, &response, 1);

// Parameter entries from [3] onwards.

        // option & 0x80: 1=perm DB, 0=volatile DB
        // option & 0x78: type 0=summary, 1=status, 2=char, 3=counters, 4=events
        // option & 0x07: entity type

        // entity: 0=node, 1=line, 2=logging, 3=circuit, 4=module 5=area
        dnetlog(LOG_DEBUG, "option=%d. entity=%d\n", option, entity);

        switch (option & 0x7f)
        {
        case 0:  // nodes summary
        case 16: // nodes char
        case 32: // nodes state
                if (entity == FORMAT_ACTIVE) { // ACTIVE NODES
                        send_exec(sock);
                        count_links();
                        get_neighbour_nodes(sock, send_neigh);
                }
                if (entity == FORMAT_KNOWN) { // KNOWN NODES
                        count_links();
                        send_all_nodes(sock, option & 0x80);
                }
                if (entity == FORMAT_ADJACENT) { // ADJACENT NODES
                        count_links();
                        get_neighbour_nodes(sock, send_neigh);
                }
                if (entity == 0x00)
                        send_exec(sock);
                break;
        case 3:  // circuits summary
        case 19: // circuits char
        case 35: // circuits state
                if ((entity == FORMAT_ACTIVE) || (entity == FORMAT_KNOWN))
                        send_circuits(sock);
                break;
        default:
                break;
        }

        // End of data.
        response = -128;
        write(sock, &response, 1);
        return 0;
}

static void unsupported(int sock)
{
        char buf[256];

        buf[0] = -1;
        buf[1] = 0;
        buf[2] = 0;
        write(sock, buf, 3);
}

int process_request(int sock, int verbosity)
{
        unsigned char buf[4096];
        int status;
        struct dn_naddr *exec_addr = getnodeadd();

        local_node = exec_addr->a_addr[0] | exec_addr->a_addr[1]<<8;

        verbose = verbosity;
        do
        {
                status = read(sock, buf, sizeof(buf));
                if (status == -1 || status == 0)
                        break;

                if (verbosity > 1)
                {
                        int i;

                        dnetlog(LOG_DEBUG, "Received message %d bytes: \n", status);
                        for (i=0; i<status; i++)
                                dnetlog(LOG_DEBUG, "%02x ", buf[i]);
                        dnetlog(LOG_DEBUG, "\n");
                }

                // TODO support single items, eg 'tell jeltz sho node zaphod'
                switch (buf[0])
                {
                case 15://          Request down-line load
                        unsupported(sock);
                        break;
                case 16://          Request up-line dump
                        unsupported(sock);
                        break;
                case 17://          Trigger bootstrap
                        unsupported(sock);
                        break;
                case 18://          Test
                        unsupported(sock);
                        break;
                case 19://          Change parameter
                        unsupported(sock);
                        break;
                case 20://          Read information
                        read_information(sock, buf, status);
                        break;
                case 21://          Zero counters
                        unsupported(sock);
                        break;
                case 22://          System-specific function
                        switch (buf[3] & 0x7f) {
                        case 7:
                                send_links(sock);
                                break;
                        case 4:
                                send_objects(sock);
                                break;
                        default:
                                unsupported(sock);
                        }
                        break;
                default:
                        unsupported(sock);
                        // Send error
                }

        } while (status > 0);
        close(sock);
        return 0;
}
