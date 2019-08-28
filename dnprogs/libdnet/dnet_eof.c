#include <sys/types.h>
#include <sys/socket.h>
#include <netdnet/dnetdb.h>
#include <netdnet/dn.h>
#include <errno.h>
#include <stdio.h>



int dnet_eof(int s)
{
#ifdef DSO_LINKINFO
    struct linkinfo_dn li;
    socklen_t len=4;

    if (getsockopt(s, DNPROTO_NSP, DSO_LINKINFO, &li, &len) == -1)
	return -1;

    if (li.idn_linkstate==LL_DISCONNECTING)
    {
	errno = ENOTCONN;
	return -1;
    }
    else
	return 0;
    
#else
    errno = EINVAL;
    return -1;
#endif

}
