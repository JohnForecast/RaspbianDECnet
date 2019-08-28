/******************************************************************************
    (c) 1999 Christine Caulfield          christine.caulfield@googlemail.com

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

/*
 * This routine reads a full record from the DECnet socket. it will
 * block until the whole record has been received.
 *
 * Most of the functionality of this is (partially) implemented in Eduardo's
 * kernel hence the #ifdefs here
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <string.h>
#include <stdlib.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>

int dnet_recv(int s, void *buf, int len, unsigned int flags)
{
    unsigned int recvflags = flags;

    if (recvflags & MSG_EOR)
	recvflags ^= MSG_EOR;
    
/* Do MSG_EOR... */

#ifdef SDF_UICPROXY // Steve's kernel   
    if (flags & MSG_EOR)
    {
	struct iovec iov;
	struct msghdr msg;
	int status;
	size_t offset = 0;

	msg.msg_control = NULL;	
	msg.msg_controllen = 0;
	msg.msg_iovlen = 1;
	msg.msg_iov = &iov;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	iov.iov_len = len;
	iov.iov_base = buf;

	do
	{
 	    status = recvmsg(s, &msg, recvflags);
	    offset += status;
	    iov.iov_base += status;
	} 
	while (status > 0 && !(msg.msg_flags & MSG_EOR));

	if (status > 0) status = offset; /* Return size read */

	return status;
    }
    else
#endif
    {
        return recv(s, buf, len, recvflags);
    }
}

