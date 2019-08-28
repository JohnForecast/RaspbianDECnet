/******************************************************************************
    (c) 1999 Steve Whitehouse                       stevew@ACM.org

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
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include <ctype.h>
#include <string.h>
#include "dn_endian.h"


static __inline__ int do_digit(char *str, u_int16_t *addr, u_int16_t scale, size_t *pos, size_t len, int *started)
{
	u_int16_t tmp = *addr / scale;

	if (*pos == len)
		return 1;

	if (((tmp) > 0) || *started || (scale == 1)) {
		*str = tmp + '0';
		*started = 1;
		(*pos)++;
		*addr -= (tmp * scale);
	}

	return 0;
}


static const char *dnet_ntop1(const struct dn_naddr *dna, char *str, size_t len)
{
	u_int16_t addr1;
	u_int16_t addr;
	u_int16_t area;
	size_t pos = 0;
	int started = 0;

	memcpy(&addr1, dna->a_addr, sizeof(u_int16_t));
	addr = dn_ntohs(addr1);
	area = addr >> 10;

	if (dna->a_len != 2)
		return NULL;

	addr &= 0x03ff;

	if (len == 0)
		return str;

	if (do_digit(str + pos, &area, 10, &pos, len, &started))
		return str;

	if (do_digit(str + pos, &area, 1, &pos, len, &started))
		return str;

	if (pos == len)
		return str;

	*(str + pos) = '.';
	pos++;
	started = 0;

	if (do_digit(str + pos, &addr, 1000, &pos, len, &started))
		return str;

	if (do_digit(str + pos, &addr, 100, &pos, len, &started))
		return str;

	if (do_digit(str + pos, &addr, 10, &pos, len, &started))
		return str;

	if (do_digit(str + pos, &addr, 1, &pos, len, &started))
		return str;

	if (pos == len)
		return str;

	*(str + pos) = 0;

	return str;
}


const char *dnet_ntop(int af, const void *addr, char *str, size_t len)
{
	switch(af) {
		case AF_DECnet:
			errno = 0;
			return dnet_ntop1((struct dn_naddr *)addr, str, len);
		default:
			errno = EAFNOSUPPORT;
	}

	return NULL;
}
