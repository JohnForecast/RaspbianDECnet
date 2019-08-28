//getobjectbyX.c:

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
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>

#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>

#define DNETD_FILE SYSCONF_PREFIX "/etc/dnetd.conf"

static char * _dnet_objhinum_string   = NULL;
static int    _dnet_objhinum_handling = DNOBJHINUM_ERROR;

static struct {
 int          num;
 const char * name;
} _dnet_objdb[] = {
 { 17, "FAL"   },
 { 18, "HLD"   },
 { 19, "NML"   },
 { 19, "NICE"  }, // alias!
 { 23, "DTERM" },
 { 23, "REMACP"}, // alias! (nml.c)
 { 25, "MIRROR"},
 { 26, "EVR"   },
 { 27, "MAIL11"},
 { 27, "MAIL"  }, // alias!
 { 29, "PHONE" },
 { 42, "CTERM" },
 { 51, "VPM"   },
 { 63, "DTR"   },
 { -1, NULL}      // END OF LIST
};

int dnet_setobjhinum_handling (int handling, int min) {
 if ( handling == DNOBJHINUM_RESET ) {
  _dnet_objhinum_string = NULL;
  dnet_checkobjectnumber(256);
  return 0;
 }

 if ( min ) {
  dnet_checkobjectnumber(256);
  if ( _dnet_objhinum_handling > handling )
   return 1;
 }

 _dnet_objhinum_handling = handling;

 return 0;
}

int dnet_checkobjectnumber (int num) {

 if ( _dnet_objhinum_string == NULL ) {
  if ( (_dnet_objhinum_string = getenv(DNOBJ_HINUM_ENV)) == NULL )
   _dnet_objhinum_string = DNOBJ_HINUM_DEF;

  if ( !*_dnet_objhinum_string )
   _dnet_objhinum_string = DNOBJ_HINUM_DEF;

  if ( !strcasecmp(_dnet_objhinum_string, "error") ) {
   dnet_setobjhinum_handling(DNOBJHINUM_ERROR, 0); // error case
  } else if ( !strcasecmp(_dnet_objhinum_string, "zero") ) {
   dnet_setobjhinum_handling(DNOBJHINUM_ZERO, 0); // return as object number 0
  } else if ( !strcasecmp(_dnet_objhinum_string, "return") ) {
   dnet_setobjhinum_handling(DNOBJHINUM_RETURN, 0); // return as object number unchanged
  } else if ( !strcasecmp(_dnet_objhinum_string, "alwayszero") ) {
   dnet_setobjhinum_handling(DNOBJHINUM_ALWAYSZERO, 0); // specal case to prevent app from using numbered objects
  }
 }

 if ( _dnet_objhinum_handling == DNOBJHINUM_ALWAYSZERO && num != -1 )
  return 0;

 if ( num < 256 )
  return num;

 switch (_dnet_objhinum_handling) {
  case DNOBJHINUM_ERROR:
   errno = EINVAL;
   return -1;
  case DNOBJHINUM_ZERO:
   return 0;
  case DNOBJHINUM_RETURN:
   return num;
  default:
   errno = ENOSYS;
   return -1;
 }
}

static int getobjectbyname_nis(const char * name) {
 char           * cur, *next;
 char             proto[16];
 struct servent * se;
 static char    * search_order = NULL;

 if ( search_order == NULL ) {
  if ( (search_order = getenv(DNOBJ_SEARCH_ENV)) == NULL )
   search_order = DNOBJ_SEARCH_DEF;
  if ( !*search_order )
   search_order = DNOBJ_SEARCH_DEF;
 }

 cur = search_order;

 while (cur && *cur) {
  if ( (next = strstr(cur, " ")) == NULL ) {
   strncpy(proto, cur, 16);
  } else {
   strncpy(proto, cur, next-cur);
   proto[next-cur] = 0;
   next++;
  }
  cur = next;

  if ( (se = getservbyname(name, proto)) != NULL ) {
   return ntohs(se->s_port);
  }
 }

 errno = ENOENT;
 return -1;
}

static const char * getobjectbynumber_nis(int num) {
 char           * cur, *next;
 char             proto[16];
 struct servent * se;
 static char    * search_order = NULL;

 if ( search_order == NULL ) {
  if ( (search_order = getenv(DNOBJ_SEARCH_ENV)) == NULL )
   search_order = DNOBJ_SEARCH_DEF;
  if ( !*search_order )
   search_order = DNOBJ_SEARCH_DEF;
 }

 cur = search_order;

 num = htons(num);

 while (cur && *cur) {
  if ( (next = strstr(cur, " ")) == NULL ) {
   strncpy(proto, cur, 16);
  } else {
   strncpy(proto, cur, next-cur);
   proto[next-cur] = 0;
   next++;
  }
  cur = next;

  if ( (se = getservbyport(num, proto)) != NULL ) {
   if ( strcmp(proto, se->s_proto) == 0 ) /* check if we got what we requested,
                                             may help on buggy libcs */
    return se->s_name;
  }
 }

 errno = ENOENT;
 return NULL;
}

static int getobjectbyname_static(const char * name) {
 int i;

 for (i = 0; _dnet_objdb[i].num != -1; i++) {
  if ( !strcasecmp(name, _dnet_objdb[i].name) )
   return _dnet_objdb[i].num;
 }

 errno = ENOENT;
 return -1;
}

static const char * getobjectbynumber_static(int num) {
 int i;

 for (i = 0; _dnet_objdb[i].num != -1; i++) {
  if ( _dnet_objdb[i].num == num )
   return _dnet_objdb[i].name;
 }

 errno = ENOENT;
 return NULL;
}

static int getobjectbyname_dnetd(const char * name) {
 FILE * dnd;
 int    found = -1;
 int    ret;
 int    curr;
 char   line[1024], cname[16], rest[1024];

 cname[15] = 0; // work around bugy *scanf()s

 if ( (dnd = fopen(DNETD_FILE, "r")) == NULL ) {
  return -1;
 }

 while (fgets(line, 1024, dnd) != NULL) {
  if ( sscanf(line, "%15s %i %1024s\n", cname, &curr, rest) == 3 ) {
   if ( *cname != '#' && strcasecmp(name, cname) == 0 ) {
    found = curr;
    break;
   }
  }
 }

 fclose(dnd);

 if ( found == -1 )
  errno = ENOENT;

 return found;
}

static const char * getobjectbynumber_dnetd(int num) {
 FILE * dnd;
 int    curr;
 static char   cname[16]; // this is not thread safe
 char   line[1024], rest[1024];

 cname[15] = 0; // work around bugy *scanf()s

 if ( (dnd = fopen(DNETD_FILE, "r")) == NULL ) {
  return NULL;
 }

 while (fgets(line, 1024, dnd) != NULL) {
  if ( sscanf(line, "%15s %i %1024s\n", cname, &curr, rest) == 3 ) {
   if ( *cname != '#' && num == curr ) {
    fclose(dnd);
    return cname;
   }
  }
 }

 fclose(dnd);

 errno = ENOENT;
 return NULL;
}

int getobjectbyname(const char * name) {
 int num;
 int old_errno = errno;

 if ( (num = getobjectbyname_nis(name)) == -1 )
  if ( (num = getobjectbyname_dnetd(name)) == -1 )
   num = getobjectbyname_static(name);

 if ( num != -1 )
  errno = old_errno;

 return dnet_checkobjectnumber(num);
}

int getobjectbynumber(int number, char * name, size_t name_len) {
 int num;
 const char * rname = NULL;
 int old_errno = errno;

 if ( (num = dnet_checkobjectnumber(number)) == -1 ) { // errno is set correctly after this call
  return -1;
 }

 if ( name_len < 2 ) {
  errno = EINVAL;
  return -1;
 }

 if ( (rname = getobjectbynumber_nis(number)) == NULL )
  if ( (rname = getobjectbynumber_dnetd(number)) == NULL )
   rname = getobjectbynumber_static(number);

 if ( rname == NULL ) {
  errno = ENOENT;
  return -1;
 }

 errno = old_errno;

 strncpy(name, rname, name_len-1);
 name[name_len-1] = 0;

 return num;
}

//ll
