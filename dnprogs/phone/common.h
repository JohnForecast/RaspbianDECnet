/* Common definitions for phone client and server processes */

/* This is the name of the socket used to communicte between the
   server and user processes - the user's name is appended to this
   string
 */
#define SOCKETNAME "/var/run/phoned"

/* Phone network commands - ie the first byte of a packet read from
   the phone client or sent by the phone server.

   The names are mine - the numbers were got fron snooping the
   network connection.
 */
#define PHONE_REPLYOK     0x01
#define PHONE_REPLYNOUSER 0x06
#define PHONE_CONNECT     0x07
#define PHONE_DIAL        0x08
#define PHONE_HANGUP      0x09
#define PHONE_UNSURE      0x0a // Something to do with error recovery
#define PHONE_ANSWER      0x0b
#define PHONE_REJECT      0x0c
#define PHONE_GOODBYE     0x0d
#define PHONE_DATA        0x0e
#define PHONE_DIRECTORY   0x0f
#define PHONE_HOLD        0x12
#define PHONE_UNHOLD      0x13

/* DECnet phone object number */
#define PHONE_OBJECT 29

/* Needed for libc5 systems */
#ifndef CMSG_DATA
# define CMSG_DATA(cmsg) ((unsigned char *) ((struct cmsghdr *) (cmsg) + 1))
#endif
