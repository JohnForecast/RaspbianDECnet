#ifndef __DNRTLINK_H__
#define __DNRTLINK_H__ 1

#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

extern int dnrt_open(struct rtnl_handle *rth, unsigned subscriptions);
extern int dnrt_wilddump_request(struct rtnl_handle *rth, int fam, int type);
extern int dnrt_dump_request(struct rtnl_handle *rth, int type, void *req, int len);
extern int dnrt_dump_filter(struct rtnl_handle *rth,
			    int (*filter)(struct sockaddr_nl *, struct nlmsghdr *n, void *),
			    void *arg1,
			    int (*junk)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
			    void *arg2);
extern int dnrt_talk(struct rtnl_handle *rtnl, struct nlmsghdr *n, pid_t peer,
		     unsigned groups, struct nlmsghdr *answer,
		     int (*junk)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
		     void *jarg);
extern int dnrt_send(struct rtnl_handle *rth, char *buf, int);

extern int dnrt_listen(struct rtnl_handle *, int (*handler)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
		       void *jarg);
extern int dnrt_from_file(FILE *, int (*handler)(struct sockaddr_nl *,struct nlmsghdr *n, void *),
		       void *jarg);

#endif /* __DNRTTLINK_H__ */

