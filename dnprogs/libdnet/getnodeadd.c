/******************************************************************************
    (c) 1995-1998 E.M. Serrat          emserrat@geocities.com

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>

static char             nodetag[80],nametag[80],nodeadr[80],nodename[80];
static struct dn_naddr	ldnaddr;
struct dn_naddr		*naddr;
/*--------------------------------------------------------------------------*/
struct dn_naddr *getnodeadd(void)
{
	FILE		*dnhosts;
	char		nodeln[80];

	
	if ((dnhosts = fopen(SYSCONF_PREFIX "/etc/decnet.conf","r")) == NULL)
	{
		printf("getnodeadd: Can not open " SYSCONF_PREFIX "/etc/decnet.conf\n");
		return 0;
	}
	while (fgets(nodeln,80,dnhosts) != NULL)
	{
		sscanf(nodeln,"%s%s%s%s\n",nodetag,nodeadr,nametag,nodename);
		if (strncmp(nodetag,"#",1) != 0)	
		{
		   if (((strcmp(nodetag,"executor") != 0) &&
	    	       (strcmp(nodetag,"node")     != 0)) ||
		       (strcmp(nametag,"name")     != 0))
		   {
		       printf("getnodeadd: Invalid decnet.conf syntax\n");
			 fclose(dnhosts);
		       return 0;
		   }
		   if (strcmp(nodetag,"executor") == 0)	
		   { 
			fclose(dnhosts);
			if ( (naddr=dnet_addr(nodename)) != NULL)
			{
				memcpy(&ldnaddr,naddr,sizeof(struct dn_naddr));
				return &ldnaddr;
			}
			else return 0;
                   }
		}
	}
	fclose(dnhosts);
	return 0;
}
/*--------------------------------------------------------------------------*/
