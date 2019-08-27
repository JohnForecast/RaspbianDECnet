#define MAX_CONNECTIONS 100
typedef enum {INACTIVE=0, DECNET_RENDEZVOUS, UNIX_RENDEZVOUS, UNIX, DECNET} sock_type;

struct fd_array
{
    int  fd;
    char local_user[64];  // node::user in CAPS
    char local_login[64]; // user in lower
    char remote_user[32]; // remote node::user in CAPS
    sock_type type;
};

extern struct fd_array fdarray[MAX_CONNECTIONS];
