/******************************************************************************
    (c) 1995-1998 E.M. Serrat          emserrat@geocities.com
    (c) 2008 Christine Caulfield       christine.caulfield@googlemail.com
    (c) 2011 Philipp Schafft           lion@lion.leolix.org

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
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "dn_endian.h"

char proxy_requested = 1;

static int parse_host(char *host, char *hname, struct accessdata_dn *acc)
{
	int i;

	errno = EINVAL;
	if (!*host)
		return -1;

	errno = ENAMETOOLONG;
	i = 0;
	while(*host && *host != '/') {
		*hname++ = *host++;
		if (++i > DN_MAXNODEL)
		return -1;
	}
	*hname = 0;

	errno = 0;
	if (!*host)
		return 0;

	host++;
	i = 0;
	errno = ENAMETOOLONG;
	while(*host && *host != '/') {
		acc->acc_user[i++] = *host++;
		if (i > DN_MAXACCL)
		       return -1;
		   }
	acc->acc_userl = i;

	errno = EINVAL;
	if (*host != '/')
		return -1;

	host++;
	i = 0;
	errno = ENAMETOOLONG;
	while(*host && *host != '/') {
		acc->acc_pass[i++] = *host++;
		if (i > DN_MAXACCL)
			return -1;
	}
	acc->acc_passl = i;

	errno = EINVAL;
	if (*host != '/')
		return -1;

	host++;
	i = 0;
	errno = ENAMETOOLONG;
	while(*host) {
		acc->acc_acc[i++] = *host++;
		if (i > DN_MAXACCL)
			return -1;
	}
	acc->acc_accl = i;

	return 0;
}

static int set_object_proxy(struct sockaddr_dn *sdn)
{
	int uid = getuid();

	/*
	 * Well, the manual says usernames are only allowed for root
	 * but I'd like to see some capability stuff here really. This
	 * could be improved. Not that the kernel checks this stuff at
	 * the moment, anyway.
	 */
	if (uid == 0) {
		char *tname = cuserid(NULL);
		char *uname = (tname && *tname) ? tname : "anonymous";
		int len = strlen(uname);

		if (len <= DN_MAXOBJL) {
			strncpy((char*)sdn->sdn_objname, uname, len);
			sdn->sdn_objnamel = dn_htons(len);
			return 0;
		}
	}

	/*
	 * So if we can't have a user name, we turn the UID into ascii
	 * and use that instead.
	 */
	sprintf((char*)sdn->sdn_objname, "%d", uid);
	sdn->sdn_objnum = 0;
	sdn->sdn_objnamel = dn_htons(strlen((char*)sdn->sdn_objname));
	return 0;
}

static int set_object_name(struct sockaddr_dn *sdn, char *name)
{
	int len;

	if (*name == '#') {
		sdn->sdn_objnum = atoi(name + 1);
		return 0;
	}
	errno = ENAMETOOLONG;
	len = strlen(name);
	if (len > DN_MAXOBJL)
		return -1;
	strncpy((char*)sdn->sdn_objname, name, len);
	sdn->sdn_objnamel = dn_htons(len);
	return 0;
}

/*
 * Bug List:
 *
 * o Doesn't use PAM to prompt for a passwd when required (should have option
 *   of not using PAM too in which case use stdin/stdout)
 * o Can't cope with the possibility of multiple local network interfaces
 * o Isn't thread safe (really we need dnet_conn_r() as well)
 * o I'm not 100% sure of the proxy_requested semantics... this appears to
 *   be correct according to the manual that I've got here, but the details
 *   are thin on the ground.
 */

int dnet_conn(char *host, char *objname, int type, unsigned char *opt_out, int opt_outl, unsigned char *opt_in, int *opt_inl)
{
#ifndef SDF_PROXY
    return -1;
#else
    struct sockaddr_dn saddr;
	char hname[DN_MAXNODEL + 1];
	struct accessdata_dn access;
	int s;
	struct timeval timeout = {60, 0};

	errno = EINVAL;
	if (!host || !objname)
		return -1;

#ifdef ESOCKTNOSUPPORT
	errno = ESOCKTNOSUPPORT;
#elif defined(EPROTONOSUPPORT)
	errno = EPROTONOSUPPORT;
#else
	errno = ENOSYS;
#endif
	if (type != SOCK_SEQPACKET && type != SOCK_STREAM)
		return -1;

	memset(&access, 0, sizeof(struct accessdata_dn));

	if (parse_host(host, hname, &access) < 0)
		return -1;

	memset(&saddr, 0, sizeof(struct sockaddr_dn));
	saddr.sdn_family = AF_DECnet;

	if (dnet_pton(AF_DECnet, hname, &saddr.sdn_add) != 1) {
		/*
		 * FIXME: This isn't thread safe.
		 */
		struct nodeent *ne = getnodebyname(hname);
		if (ne) {
			saddr.sdn_nodeaddrl = 2;
			memcpy(saddr.sdn_nodeaddr, ne->n_addr, 2);
		} else {
			errno = EADDRNOTAVAIL;
	    		return -1;
		}
	}

	if (set_object_name(&saddr, objname))
		return -1;

	s = socket(PF_DECnet, type, DNPROTO_NSP);
	if (s < 0)
		return -1;

	/*
	 * FIXME: What can we use instead of getnodeadd() to select a suitable
	 * local interface ? Perhaps we should have the concept of a default
	 * node address to bind to ?
	 */
	if (proxy_requested) {
		struct sockaddr_dn sa_bind;
		struct dn_naddr *dna = getnodeadd();

		sa_bind.sdn_family = AF_DECnet;
		sa_bind.sdn_flags = 0;
		memcpy(&sa_bind.sdn_add, dna, sizeof(*dna));
		if (set_object_proxy(&sa_bind))
			return -1;
		if (bind(s, (struct sockaddr *)&sa_bind, sizeof(sa_bind)) < 0)
			goto out_err;

		saddr.sdn_flags |= SDF_PROXY;
		   }

	if (access.acc_accl || access.acc_passl || access.acc_userl) {
		if (setsockopt(s, DNPROTO_NSP, DSO_CONACCESS, &access, sizeof(access)) < 0)
			goto out_err;
		}

	if (opt_out && opt_outl) {
		if (setsockopt(s, DNPROTO_NSP, DSO_CONDATA, opt_out, opt_outl) < 0)
			goto out_err;
	}

	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
		goto out_err;

	if (opt_in && opt_inl) {
		if (getsockopt(s, DNPROTO_NSP, DSO_CONDATA, opt_in, (socklen_t*)opt_inl) < 0)
			goto out_err;
	}

	errno = 0;
	return s;
out_err:
	{
		int tmp;
		tmp = errno;
		close(s);
		errno = tmp;
	}
	return -1;
#endif
}

