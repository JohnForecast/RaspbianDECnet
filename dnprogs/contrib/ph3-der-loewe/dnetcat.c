//dnetcat.c:

/*
    copyright 2008 Philipp 'ph3-der-loewe' Schafft <lion@lion.leolix.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    version 3.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <signal.h>

// to support old standards:
#include <sys/time.h>
#include <sys/types.h>

// currrent standard:
#include <sys/select.h>

#include <sys/socket.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>

char * progname = NULL;

void usage (void) {
 fprintf(stderr, "Usage:\n");
 fprintf(stderr, "%s [OPTIONS] node[::] object\n", progname);
 fprintf(stderr, "or\n");
 fprintf(stderr, "%s [OPTIONS] node::object\n", progname);
 fprintf(stderr, "or\n");
 fprintf(stderr, "%s [OPTIONS] -l object\n", progname);
 fprintf(stderr, "\n");
 fprintf(stderr, "Options:\n");
 fprintf(stderr, " -h   --help        this help\n"
                 " -v                 be verbose\n"
                 " -l                 listen mode\n"
                 " -f                 accepts multible connects in listening mode\n"
                 " -z                 zero IO mode (used for scanning)\n"
        );
}

int main_loop (int local_in, int local_out, int sock) {
 int    len = 1;
 char   buf[1024];
 fd_set sl;
 struct timeval tv;
 int maxfh = (local_in > sock ? local_in : sock) + 1;

 while (len > 0) {
  FD_ZERO(&sl);
  FD_SET(local_in, &sl);
  FD_SET(sock, &sl);

  tv.tv_sec  = 1;
  tv.tv_usec = 0;

  if (select(maxfh, &sl, NULL, NULL, &tv) > 0) {
   if ( FD_ISSET(sock, &sl) ) {
    if ( (len = read(sock, buf, 1024)) == -1 )
     return -1;
    if ( write(local_out, buf, len) != len )
     return -1;
   } else {
    if ( (len = read(local_in, buf, 1024)) == -1 )
     return -1;
    if ( write(sock, buf, len) != len )
     return -1;
   }
  }
 }
 return 0;
}

char * localnode (void) {
 static char node[16] = {0};
 struct dn_naddr      *binaddr;
 struct nodeent       *dp;

 if ( !node[0] ) {
  if ( (binaddr=getnodeadd()) == NULL)
   return NULL;

  if ( (dp=getnodebyaddr((char*)binaddr->a_addr, binaddr->a_len, PF_DECnet)) == NULL )
   return NULL;

  strncpy(node, dp->n_name, 15);
  node[15] = 0;
 }

 return node;
}

int main (int argc, char * argv[]) {
 int sock;
 int i;
 int verbose   = 0;
 int zeroio    = 0;
 int listening = 0;
 int forking   = 0;
 char * k      = NULL;
 char * node   = NULL;
 char * object = NULL;
 int  objnum   = 0;
 struct sockaddr_dn sockaddr;
 socklen_t          socklen = sizeof(sockaddr);
 struct nodeent   * ne;

 progname = argv[0];

 for (i = 1; i < argc; i++) {
  k = argv[i];
  if ( strcmp(k, "-h") == 0 || strcmp(k, "--help") == 0 ) {
   usage();
   return 0;
  } else if ( strcmp(k, "-z") == 0 ) {
   zeroio = 1;
  } else if ( strcmp(k, "-l") == 0 ) {
   listening = 1;
  } else if ( strcmp(k, "-f") == 0 ) {
   forking = 1;
  } else if ( strcmp(k, "-v") == 0 ) {
   verbose++;
  } else if ( node == NULL ) {
   node = k;
  } else if ( object == NULL ) {
   object = k;
  } else {
   usage();
   return 1;
  }
 }

 if ( node == NULL ) {
  fprintf(stderr, "Error: need both, node and object name/number\n");
  usage();
  return 1;
 }

 if ( (k = strstr(node, "::")) != NULL ) {
  *k = 0;
  if ( k[2] != 0 && object == NULL )
   object = k+2;
 }

 if ( listening && node != NULL && object == NULL ) {
  object = node;
  node   = localnode();
 }

 if ( node == NULL || object == NULL ) {
  fprintf(stderr, "Error: need both, node and object name/number\n");
  usage();
  return 1;
 }

 if (*object == '#') {
  objnum = atoi(object+1);
 }

 if ( verbose ) {
  if ( listening ) {
   // listening on [any] 1234 ...
   fprintf(stderr, "listening on [any] %i (%s) ...\n", objnum, object);
  }
 }

 errno = 0;
 if ( listening ) {
  sock = dnet_daemon(objnum, object, 0, 0);
 } else {
  sock = dnet_conn(node, object, SOCK_STREAM, 0, 0, 0, 0);
 }

 if ( sock == -1 ) {
  // localhost [127.0.0.1] 23 (telnet) : Connection refused
  fprintf(stderr, "%s %i (%s): %s\n", node, objnum, object, strerror(errno));
  return 2;
 }

 if ( verbose ) {
  if ( ! listening ) {
   // localhost [127.0.0.1] 22 (ssh) open
   fprintf(stderr, "%s %i (%s) open\n", node, objnum, object);
  }
 }

 if ( listening ) {
  dnet_accept(sock, 0, NULL, 0);
  if ( verbose ) {
   memset(&sockaddr, 0, socklen);

   if ( getpeername(sock, (struct sockaddr*)&sockaddr, &socklen) == 0 ) {
    if ( sockaddr.sdn_objnum )
     sprintf((char*)sockaddr.sdn_objname, "#%u", sockaddr.sdn_objnum);

    ne = getnodebyaddr((char *)sockaddr.sdn_add.a_addr, sockaddr.sdn_add.a_len, PF_DECnet);
    fprintf(stderr, "connect to %s::%s from %s::%s \n", node, object, (char*)(ne == NULL ? NULL : ne->n_name), sockaddr.sdn_objname);
   } else {
    // connect to [127.0.0.1] from localhost [127.0.0.1] 53255
    fprintf(stderr, "connect to %s::%s from unknown \n", node, object);
   }
  }
 }

 if ( !zeroio )
  main_loop(0, 1, sock);

 if ( listening && !forking )
  kill(getppid(), SIGTERM);

 close(sock);

 return 0;
}

//ll
