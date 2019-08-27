/*
    open.cc from librms

    Copyright (C) 1999-2001 Christine Caulfield       christine.caulfield@googlemail.com

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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "connection.h"
#include "protocol.h"
#include "logging.h"
#include "rms.h"
#include "rmsp.h"

static char *open_error = NULL;

static void build_access_message(dap_access_message *acc, struct FAB *fab)
{
    if (fab->fab$b_fac) acc->set_fac(fab->fab$b_fac);
    if (fab->fab$b_shr) acc->set_shr(fab->fab$b_shr);
}

static void build_attrib_message(dap_attrib_message *att, struct FAB *fab)
{
    if (fab->fab$b_rfm) att->set_rfm((int)fab->fab$b_rfm);
    if (fab->fab$b_rat) att->set_rat((int)fab->fab$b_rat);
    //att->set_fop((int)fab->fab$l_fop);
}

RMSHANDLE rms_open(char *name, int mode, struct FAB *fab)
{
    struct accessdata_dn accessdata;
    char fname[256];
    char user[256];
    char node[256];
    char password[256];
    int verbose = 0;

    if (getenv("LIBRMS_VERBOSE"))
    {
        verbose = atoi(getenv("LIBRMS_VERBOSE"));
        if (!verbose) verbose = 1;
  	init_logging("librms", 'e', false);
    }

    dap_connection *conn = new dap_connection(verbose);
  
    errno = ENOENT;
    memset(&accessdata, 0, sizeof(accessdata));
    if (!conn->parse(name, accessdata, node, fname)) 
    {
	open_error = conn->get_error();
	delete conn;
	return NULL;
    }
  
    strcpy(user, (char *)accessdata.acc_user);
    strcpy(password, (char *)accessdata.acc_pass);


    if (!conn->connect(node, user, password, dap_connection::FAL_OBJECT)) 
    {
	open_error = conn->get_error();
	delete conn;
	return NULL;
    }
    if (!conn->exchange_config())
    {
	open_error = conn->get_error();
	delete conn;
	return NULL;
    }
    rms_conn *rc = new rms_conn(conn);

    // VMS needs an attrib message so give it one.
    dap_attrib_message att;
    att.set_rfm(dap_attrib_message::FB$VAR);
    if (fab) build_attrib_message(&att, fab);

    conn->set_blocked(true);     
    if (!att.write(*conn))
    {
	delete conn;
	return NULL;
    }

    dap_alloc_message all;
    if (!all.write(*conn))
    {
        delete conn;
        return NULL;
    }
    conn->set_blocked(false); 

    // Set up the access method
    dap_access_message acc;
    acc.set_accfunc(dap_access_message::OPEN);
    acc.set_display(dap_access_message::DISPLAY_MAIN_MASK);
    if (mode & O_CREAT) acc.set_accfunc(dap_access_message::CREATE);

    if (mode & O_RDWR || mode & O_CREAT)
	acc.set_fac(1<<dap_access_message::FB$PUT |
		    1<<dap_access_message::FB$GET |
		    1<<dap_access_message::FB$UPD |
		    1<<dap_access_message::FB$DEL);
    if (mode & O_WRONLY)
	acc.set_fac(1<<dap_access_message::FB$PUT |
		    1<<dap_access_message::FB$UPD |
		    1<<dap_access_message::FB$DEL);
    

    if (fab) build_access_message(&acc, fab);
    acc.set_filespec(fname);
    if (!acc.write(*conn))
    {
	delete conn;
	return NULL;
    }

    // Get reply to OPEN
    dap_message *m;
    int r = rms_getreply(rc, 1, fab, &m);
    if (r < 0) 
    {
	if (r == -2) delete m;
	open_error = rms_lasterror(rc);
	delete conn;
	delete rc;
	return NULL;
    }

  
    // Connect RAB
    dap_control_message ctl;
    ctl.set_ctlfunc(dap_control_message::CONNECT);
    ctl.write(*conn);

    // Get reply to CONNECT
    r = rms_getreply(rc, 1, fab, &m);
    if (r < 0) 
    {
	if (r == -2) delete m;
	open_error = rms_lasterror(rc);
	delete conn;
	delete rc;
	return NULL;
    }

    // Return new struct
    return (RMSHANDLE)rc;
}

// Open with text arguments
RMSHANDLE rms_t_open(char *name, int mode, char *options, ...)
{
    struct FAB fab;
    struct RAB rab;
    va_list ap;

    va_start(ap, options);
    if (!parse_options(NULL, options, &fab, &rab, ap)) return NULL;
    
    return rms_open(name, mode, &fab);
}

char *rms_openerror()
{
    return open_error;
}
