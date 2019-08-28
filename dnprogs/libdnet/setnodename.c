/*
 * (C) 1998 Steve Whitehouse


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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __NetBSD__
#include <sys/param.h>
#endif

#include "netdnet/dn.h"


int setnodename(const char *name, size_t len)
{
	FILE* procfile = fopen("/proc/sys/net/decnet/nodename", "w");
	if (procfile == NULL)
		return -1;
	if (fwrite(name, 1, len, procfile) != len)
	{
		fclose(procfile);
		return -1;
	}
	if (fclose(procfile) == EOF)
		return -1;
	return 0;
}


