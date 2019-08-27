/******************************************************************************
    dnet_priv_check.c from libdnet_daemon

    Copyright (C) 2008-2010 Philipp 'ph3-der-loewe' Schafft <lion@lion.leolix.org>

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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>

#define LINELEN       1024
#define LISTDELM      " \t,"

int dnet_priv_check(const char * file, const char * proc,
                    const struct sockaddr_dn * local, const struct sockaddr_dn * remote) {
    FILE           * fh;
    char             line[LINELEN];
    char           * clients;
    int              match;
    char           * c;
    char             nodeaddr[12];
    struct nodeent * ne;
    char           * tokptr = NULL;

    snprintf(nodeaddr, sizeof(nodeaddr), "%i.%i", remote->sdn_add.a_addr[1] >> 2,
                               remote->sdn_add.a_addr[0] +
                               ((remote->sdn_add.a_addr[1] & 0x3) << 8));
    nodeaddr[sizeof(nodeaddr)-1] = 0;

    if ( (fh = fopen(file, "r")) == NULL )
	return -1;

    // walk thru the file and search for matches
    // if a match is found we return directly from within this loop.
    while (fgets(line, LINELEN, fh) != NULL) {
	// skip comments.
	if ( line[0] == '#' )
	    continue;

	// split into service/daemon and clients part
	if ( (clients = strchr(line, ':')) == NULL )
	    continue;

	*clients = 0;
	clients++;

	// clean up lion endings
	c = &clients[strlen(clients) - 1];
	if ( *c == '\n' )
	    *c = 0;

	match = 0; // reset match to 'no match found'

	// now walk thru the list of services/daemons/objects and
	// see if we have a match here.
	c = strtok_r(line, LISTDELM, &tokptr);
	while (c != NULL) {
	    if ( !strcmp(c, "ALL") ) {      // ALL matches all services
		match = 1;
	    } else if ( *c == '$' ) {       // match local object
		c++;
		if ( *c == '#' ) {          // if this starts with '#' we mach object number
		   c++;
		   if ( isalpha(*c) ) {
			if ( getobjectbyname(c) == local->sdn_objnum )
			    match = 1; 
		   } else {
			if ( atoi(c) == local->sdn_objnum )
			    match = 1; 
		   }
		} else {
		    if ( *c == '=' )        // new format start with '=' for object name.
			c++;                // but we continue if we don't find this to be
                                            // compatible with old format.
		    if ( local->sdn_objnamel &&
		         !strncmp(c, (char *)local->sdn_objname, local->sdn_objnamel) )
			match = 1;
		}
	    } else if (proc != NULL && !strcmp(c, proc)) {  // match process/service/... name
		match = 1;
	    }

	    if ( match ) break; // end this loop if we have a match

	    c = strtok_r(NULL, LISTDELM, &tokptr); // get next element
	}

	if ( !match ) // continue with outer loop if we have no match
	    continue;

	// we now have a object match, search for a remode node match

	match = 0; // reset match so we can use it for remote node matching

	// now walk thru the list of clients:
	c = strtok_r(clients, LISTDELM, &tokptr);
	while (c != NULL) {
	    if ( !strcmp(c, "ALL") ) {            // test for wildcard
		match = 1;
	    } else if ( !strcmp(c, nodeaddr) ) {  // test for numerical node address
		match = 1;
	    } else if ( isalpha(*c) ) {           // test for node name
		if ( (ne = getnodebyname(c)) != NULL )
		    if ( *(int16_t*)ne->n_addr == *(int16_t*)remote->sdn_add.a_addr )
			match = 1;
	    }

	    // do we have a match?
	    if (match) {
		// do cleanup and return.
		fclose(fh);
		return 1;
	    }

	    // get next client
	    c = strtok_r(NULL, LISTDELM, &tokptr);
	}
    }
    // no match was found.

    // cleanup
    fclose(fh);
    return 0;
}
