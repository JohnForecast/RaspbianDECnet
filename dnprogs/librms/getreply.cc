/*
    getreply.cc from librms

    Copyright (C) 1999-2005 Christine Caulfield     christine.caulfield@googlemail.com

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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "connection.h"
#include "protocol.h"
#include "rms.h"
#include "rmsp.h"

int check_status(rms_conn *rc, dap_message *m)
{
    if (!rc) return -1;

    if (m->get_type() != dap_message::STATUS) return -1;

    dap_status_message *sm = (dap_status_message *)m;

    // Save this stuff so we can delete the message
    int code = sm->get_code() & 0xFF;
    char *err = (char *)sm->get_message();

    delete m;

    rc->lasterr = code;

    if (code == 0225) return 0;    // Success
    if (code == 047)  return code; // EOF
    rc->lasterror = err;
    if (code == 060)
      errno = EBUSY;
    else
      errno = ENOENT; // Bad fallback.
    return -1;
}

static void fill_fab(dap_attrib_message *am, struct FAB *fab)
{
	if (am->get_menu_bit(dap_attrib_message::MENU_ORG)) 
	    fab->fab$b_org = am->get_org();

	if (am->get_menu_bit(dap_attrib_message::MENU_ALQ))
	    fab->fab$l_alq = am->get_alq();

	if (am->get_menu_bit(dap_attrib_message::MENU_DEQ))
	    fab->fab$w_deq = am->get_deq();

	if (am->get_menu_bit(dap_attrib_message::MENU_RAT))
	    fab->fab$b_rat = am->get_rat();

	if (am->get_menu_bit(dap_attrib_message::MENU_RFM))
	    fab->fab$b_rfm = am->get_rfm();

	if (am->get_menu_bit(dap_attrib_message::MENU_JNL))
	    fab->fab$l_jnl = am->get_jnl();

	if (am->get_menu_bit(dap_attrib_message::MENU_MRS))
	    fab->fab$w_mrs = am->get_mrs();

	if (am->get_menu_bit(dap_attrib_message::MENU_MRN))
	    fab->fab$l_mrn = am->get_mrn();

	if (am->get_menu_bit(dap_attrib_message::MENU_FSZ))
	    fab->fab$b_fsz = am->get_fsz();
}


// Users should not need to call this.
int rms_getreply(RMSHANDLE h, int wait, struct FAB *fab, dap_message **msg)
{
    rms_conn *rc = (rms_conn *)h;
    dap_connection *conn = (dap_connection *)rc->conn;
    *msg = NULL;

    if (!wait)
    {
	int next_msg = dap_message::peek_message_type(*conn);

	if (next_msg == dap_message::DATA) return 0;
    }

    dap_message *m = dap_message::read_message(*conn,true);
    if (m)
    {
	if (m->get_type() == dap_message::STATUS)
	    return check_status(rc, m);

	if (m->get_type() == dap_message::ACK)
	{
	    delete m;
	    return 0; // OK
	}
	if (m->get_type() == dap_message::ACCOMP)
	{
	    delete m;
	    return 0; // OK
	}
	if (m->get_type() == dap_message::ATTRIB)
	{
	    if (fab)
		fill_fab((dap_attrib_message *)m, fab);
	    delete m;
	    return 0; // OK
	}

	// Don't know what that was - let the caller deal with it.
	*msg = m;
	return -2;
    }
    rc->lasterror = conn->get_error();
    return -1;
}

char *rms_lasterror(RMSHANDLE h)
{
    rms_conn *rc = (rms_conn *)h;

    return rc->lasterror;
}

int rms_lasterrorcode(RMSHANDLE h)
{
    rms_conn *rc = (rms_conn *)h;

    return rc->lasterr;
}
