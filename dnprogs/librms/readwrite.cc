/*
    readwrite.cc from librms

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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "connection.h"
#include "protocol.h"
#include "rms.h"
#include "rmsp.h"

// Populate the control message from the input parameters...
static void build_control_message(rms_conn *rc, dap_control_message *ctl, struct RAB *rab)
{
	if (rab->rab$b_rac) ctl->set_rac(rab->rab$b_rac);
	if (rab->rab$l_rop) ctl->set_rop(rab->rab$l_rop);
	if (rab->rab$l_kbf)
	{
// To be helpful - if the ksz is zero then assume it's a string
		if (rab->rab$b_ksz)
		{
			ctl->set_key((char*)rab->rab$l_kbf, (int)rab->rab$b_ksz);
		}
		else
		{
			ctl->set_key((char *)rab->rab$l_kbf);
		}
		ctl->set_krf(rab->rab$b_krf);
	}
	if (rab->rab$w_usz)
		ctl->set_usz(rab->rab$w_usz);
}

int rms_read(RMSHANDLE h, char *buf, int maxlen, struct RAB *rab)
{
    if (!h) return -1;

    rms_conn *rc = (rms_conn *)h;
    dap_connection *conn = (dap_connection *)rc->conn;

    rc->lasterror = NULL;

// If there is an outstanding record then return that if we can
    if (rc->record)
    {
	if (maxlen >= rc->dlen)
	{
	    memcpy(buf, rc->record, rc->dlen);
	    delete rc->record;
	    rc->record = NULL;
	    return rc->dlen;
	}
	// Stupid user - still not enough space;
	rc->lasterror = NULL;
	return -rc->dlen;
    }

    // Send GET - these are the defaults if no RAB
    dap_control_message ctl;
    ctl.set_ctlfunc(dap_control_message::GET);
    ctl.set_rac(dap_control_message::RB$SEQ);

    if (rab) build_control_message(rc, &ctl, rab);

    if (!ctl.write(*conn))
    {
	rc->lasterror = conn->get_error();
	return -1;
    }

    // Check the reply
    dap_message *m;
    int r = rms_getreply(h, 0, NULL, &m);
    if (r == -1) return -1;
    if (r == 047) {
	rc->lasterror = (char *)"EOF";
	return 0; // EOF
    }
    // if r == -2 then we have a message to process.

    // Get record
    if (!m) m = dap_message::read_message(*conn,true);
    if (m && m->get_type() == dap_message::DATA)
    {
	dap_data_message *dm=(dap_data_message *)m;
	int dlen;
	dlen = dm->get_datalen();
	if (dlen > maxlen)
	{
	    // Take a temporary copy and return the actual length
	    rc->record = new char[dlen];
	    rc->dlen = dlen;
	    rc->lasterror = NULL;
	    dm->get_data(rc->record, &dlen);

	    delete m;
	    return -dlen;
	}
	dm->get_data(buf, &dlen);
	delete m;
	return dlen;
    }

    // It was a STATUS message instead of DATA, argh!
    if (m && m->get_type() == dap_message::STATUS)
    {
	int status = check_status(rc, m);

	// If that was an error then send ACCOMP to keep the remote end sweet
	if (status == -1)
	{
	    dap_accomp_message acc;
	    acc.set_cmpfunc(dap_accomp_message::SKIP);// CLOSE? END_OF_STREAM?
	    acc.write(*conn);
	    return -1;
	}
	if (status == 047) // EOF
		status = 0;
	return status;
    }

    if (m)  // WTF was that?
    {
	static char err[1024];
	sprintf(err, "got unexpected DAP message: %s\n", m->type_name());
	rc->lasterror = err;

	delete m;
	return -1;
    }
    rc->lasterror = conn->get_error();
    return -1;
}

// Similar to rms_read but don't fetch data.
int rms_find(RMSHANDLE h, struct RAB *rab)
{
    if (!h) return -1;

    rms_conn *rc = (rms_conn *)h;
    dap_connection *conn = (dap_connection *)rc->conn;

    dap_control_message ctl;
    ctl.set_ctlfunc(dap_control_message::FIND);

    if (rab) build_control_message(rc, &ctl, rab);

    if (!ctl.write(*conn))
    {
	rc->lasterror = conn->get_error();
	return -1;
    }

    dap_message *m;
    int r = rms_getreply(h, 1, NULL, &m);
    if (r == -2) delete m;
    if (r < 0) return -1;
    if (r == 047) return 0; // EOF

    return 0;
}

int rms_write(RMSHANDLE h, char *buf, int len, struct RAB *rab)
{
    if (!h) return -1;

    rms_conn *rc = (rms_conn *)h;
    dap_connection *conn = (dap_connection *)rc->conn;

    dap_control_message ctl;
    ctl.set_ctlfunc(dap_control_message::PUT);

    if (rab) build_control_message(rc, &ctl, rab);

    if (!ctl.write(*conn))
    {
	rc->lasterror = conn->get_error();
	return -1;
    }

    dap_data_message data;
    data.set_data(buf, len);
    bool status;

    if (len >= 256)
	status = data.write_with_len256(*conn);
    else
	status = data.write_with_len(*conn);
    if (!status)
    {
	rc->lasterror = conn->get_error();
	return -1;
    }

    dap_message *m;
    int r = rms_getreply(h, 1, NULL, &m);
    if (r == -2) delete m;
    if (r < 0) return -1;

    return 0;
}

int rms_update(RMSHANDLE h, char *buf, int len, struct RAB *rab)
{
    if (!h) return -1;

    rms_conn *rc = (rms_conn *)h;
    dap_connection *conn = (dap_connection *)rc->conn;

    dap_control_message ctl;
    ctl.set_ctlfunc(dap_control_message::UPDATE);

    if (rab) build_control_message(rc, &ctl, rab);

    if (!ctl.write(*conn))
    {
	rc->lasterror = conn->get_error();
	return -1;
    }

    dap_data_message data;
    data.set_data(buf, len);
    bool status;
    if (len >= 256)
	status = data.write_with_len256(*conn);
    else
	status = data.write_with_len(*conn);
    if (!status)
    {
	rc->lasterror = conn->get_error();
	return -1;
    }

    dap_message *m;
    int r = rms_getreply(h, 1, NULL, &m);
    if (r == -2) delete m;
    if (r < 0 ) return -1;

    return 0;
}

int rms_delete(RMSHANDLE h, struct RAB *rab)
{
    if (!h) return -1;

    rms_conn *rc = (rms_conn *)h;
    dap_connection *conn = (dap_connection *)rc->conn;

    dap_control_message ctl;
    ctl.set_ctlfunc(dap_control_message::DELETE);

    if (rab) build_control_message(rc, &ctl, rab);

    if (!ctl.write(*conn))
    {
	rc->lasterror = conn->get_error();
	return -1;
    }

    dap_message *m;
    int r = rms_getreply(h, 1, NULL, &m);
    if (r == -2) delete m;
    if (r < 0) return -1;

    return 0;
}

int rms_truncate(RMSHANDLE h, struct RAB *rab)
{
    if (!h) return -1;

    rms_conn *rc = (rms_conn *)h;
    dap_connection *conn = (dap_connection *)rc->conn;

    dap_control_message ctl;
    ctl.set_ctlfunc(dap_control_message::TRUNCATE);

    if (rab) build_control_message(rc, &ctl, rab);

    if (!ctl.write(*conn))
    {
	rc->lasterror = conn->get_error();
	return -1;
    }

    dap_message *m;
    int r = rms_getreply(h, 1, NULL, &m);
    if (r == -2) delete m;
    if (r < 0) return -1;

    return 0;
}

int rms_rewind(RMSHANDLE h, struct RAB *rab)
{
    if (!h) return -1;

    rms_conn *rc = (rms_conn *)h;
    dap_connection *conn = (dap_connection *)rc->conn;

    dap_control_message ctl;
    ctl.set_ctlfunc(dap_control_message::REWIND);

    if (rab) build_control_message(rc,&ctl, rab);

    if (!ctl.write(*conn))
    {
	rc->lasterror = conn->get_error();
	return -1;
    }

    dap_message *m;
    int r = rms_getreply(h, 1, NULL, &m);
    if (r == -2) delete m;
    if (r < 0) return -1;

    return 0;
}

int rms_t_read(RMSHANDLE h, char *buf, int maxlen, char *options, ...)
{
    struct FAB fab;
    struct RAB rab;
    va_list ap;

    va_start(ap, options);
    if (!parse_options(h, options, &fab, &rab, ap)) return -1;
    return rms_read(h, buf, maxlen, &rab);
}

int rms_t_find(RMSHANDLE h, char *options, ...)
{
    struct FAB fab;
    struct RAB rab;
    va_list ap;

    va_start(ap, options);
    if (!parse_options(h, options, &fab, &rab, ap)) return -1;
    return rms_find(h, &rab);
}

int rms_t_write(RMSHANDLE h, char *buf, int len, char *options, ...)
{
    struct FAB fab;
    struct RAB rab;
    va_list ap;

    va_start(ap, options);
    if (!parse_options(h, options, &fab, &rab, ap)) return -1;
    return rms_write(h, buf, len, &rab);
}

int rms_t_update(RMSHANDLE h, char *buf, int len, char *options, ...)
{
    struct FAB fab;
    struct RAB rab;
    va_list ap;

    va_start(ap, options);
    if (!parse_options(h, options, &fab, &rab, ap)) return -1;
    return rms_update(h, buf, len, &rab);
}

int rms_t_delete(RMSHANDLE h, char *options, ...)
{
    struct FAB fab;
    struct RAB rab;
    va_list ap;

    va_start(ap, options);
    if (!parse_options(h, options, &fab, &rab, ap)) return -1;
    return rms_delete(h, &rab);
}

int rms_t_truncate(RMSHANDLE h, char *options, ...)
{
    struct FAB fab;
    struct RAB rab;
    va_list ap;

    va_start(ap, options);
    if (!parse_options(h, options, &fab, &rab, ap)) return -1;
    return rms_truncate(h, &rab);
}


int rms_t_rewind(RMSHANDLE h, char *options, ...)
{
    struct FAB fab;
    struct RAB rab;
    va_list ap;

    va_start(ap, options);
    if (!parse_options(h, options, &fab, &rab, ap)) return -1;
    return rms_rewind(h, &rab);
}
