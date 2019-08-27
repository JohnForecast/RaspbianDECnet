/******************************************************************************
    (c) 2008-2013 Christine Caulfield             christine.caulfield@gmail.com

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
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>

#define BUFLEN 4096

static void makelower(char *s)
{
	int i;
	for (i=0; s[i]; i++) s[i] = tolower(s[i]);
}


static int get_node_list(char *nodename, char *ldif_dc)
{
	struct accessdata_dn accessdata;
	char node[BUFLEN];
	unsigned char reply[BUFLEN];
	int sockfd;
	int status;
	struct nodeent	*np;
	struct sockaddr_dn sockaddr;
	char *exec_dev;
	struct dn_naddr *exec_addr;
	unsigned int exec_area;
	unsigned int nodeaddr;
	char *local_user=NULL;
	struct nodeent *exec_node;
	char command[] = {0x14, 0, 0xff}; // NICE Command to fetch all known nodes

	// Get exec data as that's not in the remote node list!
	exec_addr = getnodeadd();
	if (!exec_addr) {
		return -1;
	}
	exec_area = exec_addr->a_addr[1]>>2;
	nodeaddr = exec_addr->a_addr[0] | exec_addr->a_addr[1]<<8;
	exec_dev = getexecdev();
	if (!exec_dev)
		return -1;
	exec_node = getnodebyaddr((char*)exec_addr->a_addr, 2, AF_DECnet);

	memset(&accessdata, 0, sizeof(accessdata));
	memset(&sockaddr, 0, sizeof(sockaddr));

	if (!local_user) local_user = getenv("USER");
	if (local_user)
	{
		strcpy((char *)accessdata.acc_acc, local_user);
		accessdata.acc_accl = strlen((char *)accessdata.acc_acc);
	}
	else
	{
		accessdata.acc_acc[0] = '\0';
	}

	np = getnodebyname(nodename);
	if (!np)
	{
		fprintf(stderr, "Cannot find node name '%s'\n", nodename);
		return -1;
	}

	if ((sockfd=socket(AF_DECnet, SOCK_SEQPACKET, DNPROTO_NSP)) == -1)
	{
		return -1;
	}


	if (!ldif_dc) {
		// Print header
		printf("\
#\n						\
#               DECnet hosts file\n		\
#\n\
#Node           Node            Name            Node    Line    Line\n\
#Type           Address         Tag             Name    Tag     Device\n\
#-----          -------         -----           -----   -----   ------\n");

	// Print exec line
	printf("executor\t%d.%d\t\tname\t\t%s\tline\t%s\n",
	       nodeaddr >> 10, nodeaddr & 0x3FF, exec_node->n_name, exec_dev);
	}

	/* Connect to network Management Listener */
	sockaddr.sdn_family   = AF_DECnet;
	sockaddr.sdn_flags    = 0x00;
	sockaddr.sdn_objnum   = DNOBJECT_NICE;
	sockaddr.sdn_objnamel = 0;
	memcpy(sockaddr.sdn_add.a_addr, np->n_addr,2);
	sockaddr.sdn_add.a_len = 2;

	if (connect(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
	{
		close(sockfd);
		return -1;
	}

// Now run the command
	if (write(sockfd, command, sizeof(command)) < (int)sizeof(command))
	{
		close(sockfd);
		return -1;
	}

	do
	{
		status = read(sockfd, reply, BUFLEN);
		if (reply[0] == 2)
			continue; // Success - data to come
		if ((signed char)reply[0] == -1)
		{
			fprintf(stderr, "error %d: %s\n", reply[1] | reply[2]<<8, reply+3);
			break;
		}
		if (reply[0] == 1)
		{
			unsigned int namelen;

			// Data response
			switch (reply[3])
			{
			case 0: // node
				nodeaddr = reply[4] | reply[5] << 8;
				if (nodeaddr >> 10 == 0) // In exec area
					nodeaddr |= exec_area << 10;

				namelen = reply[6] & 0x7f; // Top bit indicates EXEC
				memcpy(node, reply+7, namelen);
				node[namelen] = 0;
				makelower(node);

				if (ldif_dc) {
					printf("dn: cn=%s,ou=hosts,%s\n", node, ldif_dc);
					printf("cn: %s\n", node);
					printf("macAddress: AA:00:04:00:%02X:%02X\n", nodeaddr&0xff, nodeaddr>>8);
					printf("objectClass: top\n");
					printf("objectClass: ipHost\n");
					printf("objectClass: device\n");
					printf("objectClass: ieee802Device\n");
					printf("\n");
				}
				else {
					printf("node\t\t%d.%d\t\tname\t\t%s\n", nodeaddr >> 10, nodeaddr & 0x3FF, node);
				}
				break;
			default: // more ?
				break;
			}

		if (reply[0] == 128)
			break; // end of data
		}
	} while (status > 0 && reply[0] != 128);

	close (sockfd);
	return status;
}


int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "\nusage %s <node> [ldif <dc>]\n\n", argv[0]);
		fprintf(stderr, "  Generates a decnet.conf file from another node's\n");
		fprintf(stderr, "  known node list\n\n");
		return 1;
	}

	if (argv[2])
		return get_node_list(argv[1], argv[2]);
	else
		return get_node_list(argv[1], 0);
}
