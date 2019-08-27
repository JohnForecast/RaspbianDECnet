/******************************************************************************
    (c) 1998-2006 Christine Caulfield               christine.caulfield@googlemail.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 ******************************************************************************
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <regex.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>

#include "logging.h"
#include "connection.h"
#include "protocol.h"
#include "dnetfile.h"


/*-------------------------------------------------------------------------*/
void dnetfile::dap_close_link()
{
    if (verbose > 2) DAPLOG((LOG_INFO, "in dap_close_link()\n"));
    conn.close();
}
/*-------------------------------------------------------------------------*/
int dnetfile::dap_get_reply(void)
{
    if (verbose > 2) DAPLOG((LOG_INFO, "in dap_get_reply()\n"));
    dap_message *m = dap_message::read_message(conn,true);
    if (m)
    {
	if (m->get_type() == dap_message::STATUS)
	    return dap_check_status(m,0);

	if (m->get_type() == dap_message::ACK)
	    return 0; // OK
	if (m->get_type() == dap_message::ACCOMP)
	    return 0; // OK

	DAPLOG((LOG_ERR, "dap_get_reply: got : %s(%d)\n", m->type_name(), m->get_type()));
	lasterror = "Unknown message type received";
	return -1;
    }
    DAPLOG((LOG_ERR, "dap_get_reply error: %s\n",conn.get_error()));

    return -1;
}
/*-------------------------------------------------------------------------*/
int dnetfile::dap_send_access()
{
    dap_access_message acc;
    acc.set_accfunc(dap_access_message::OPEN);

    conn.set_blocked(true);

    if (transfer_mode == MODE_BLOCK)
    {
	acc.set_fac( (1<<dap_access_message::FB$GET) |
		     (1<<dap_access_message::FB$BRO));
	acc.set_shr(1<<dap_access_message::FB$GET);
    }

    if (writing)
    {
	acc.set_accfunc(dap_access_message::CREATE);
	acc.set_fac(dap_access_message::FB$PUT);
	acc.set_shr(1<<dap_access_message::FB$BRO);
    }
    else // We build our own attributes message when writing.
    {
	dap_attrib_message att;

	att.set_fop_bit(dap_attrib_message::FB$SQO);
	att.set_org(dap_attrib_message::FB$SEQ);
	att.set_rfm(dap_attrib_message::FB$VAR);
	att.set_mrs(0);
	att.set_datatype(dap_attrib_message::IMAGE);
	if (user_flags & FILE_FLAGS_RRL) {
	    acc.set_shr(1<<dap_access_message::FB$PUT |
	                1<<dap_access_message::FB$UPD |
	                1<<dap_access_message::FB$GET |
	                1<<dap_access_message::FB$DEL);
	}

	att.write(conn);
    }
    acc.set_display(dap_access_message::DISPLAY_MAIN_MASK |
		    dap_access_message::DISPLAY_PROT_MASK |
		    dap_access_message::DISPLAY_NAME_MASK);
    acc.set_filespec(filname);
    acc.write(conn);
    return !conn.set_blocked(false);
}

/*-------------------------------------------------------------------------*/

// Receive the attributes of a file for download or a newly created file
int dnetfile::dap_get_file_entry(int *rfm, int *rat)
{
    char sentname[strlen(filname)+1];
    if (verbose > 2) DAPLOG((LOG_INFO, "in dap_get_file_entry()\n"));

    strcpy(sentname, filname); // Save in case of error
    dirname[0] = volname[0] = filname[0] = '\0';

    dap_message *m;
    while ( ((m = dap_message::read_message(conn, true))) )
    {
	switch (m->get_type())
	{
	case dap_message::NAME:
	    {
		dap_name_message *nm = (dap_name_message *)m;
		switch (nm->get_nametype())
		{
		case dap_name_message::VOLUME:
		    //strcpy(volname, nm->get_namespec());
		    break;

		case dap_name_message::DIRECTORY:
		    //strcpy(dirname, nm->get_namespec());
		    break;

		case dap_name_message::FILENAME:
		    strcpy(filname, nm->get_namespec());
		    break;

		case dap_name_message::FILESPEC:
		    strcpy(filname, nm->get_namespec());
		    break;
		}
	    }
	    break;

	case dap_message::ACCOMP:
	    return -2; // End of wildcard list

	case dap_message::ATTRIB:
	    {
		dap_attrib_message *am =(dap_attrib_message *)m;
		*rfm = am->get_rfm();
		*rat = am->get_rat();
		file_fsz = am->get_fsz();
	    }
	    break;
	case dap_message::PROTECT:
	    {
		dap_protect_message *pm =(dap_protect_message *)m;
		prot = pm->get_mode();
	    }
	    break;

	case dap_message::ACK:
	    return 0;

	case dap_message::STATUS:
	    {
		dap_status_message *sm = (dap_status_message *)m;
		if (wildcard && sm->get_code() == 0x4030) // Locked
		{
		    dap_send_skip();
		    break;
		}
#if 0
/* Don't know how to handle this yet (CONFUNC) */
		if (sm->get_code() == 0x2150) {
		}
#endif
		if (sm->get_code() == 0x4097) // Name syntax error
		{
			fprintf(stderr, "Can't send '%s' : Name is not legal at remote end\n", sentname);
			return -1;
		}
		return dap_check_status(m,-1);
	    }

	default:
	    return m->get_type();
	}
	if (m) delete m;
    }
    return -1;
}
/*-------------------------------------------------------------------------*/
int dnetfile::dap_send_connect()
{
    if (verbose > 2) DAPLOG((LOG_INFO, "in dap_send_connect()\n"));

    dap_control_message ctl;

    ctl.set_ctlfunc(dap_control_message::CONNECT);
    ctl.write(conn);

    return dap_get_reply();

}
/*-------------------------------------------------------------------------*/
// Sends a CONTROL message with $GET or $PUT as the action
// Also enables block mode for reading files with -mblock requested.
// Note we don't really write files in block mode only read them.
// "Block" writes just create fixed-length record binary files.
int dnetfile::dap_send_get_or_put()
{
    if (verbose > 2) DAPLOG((LOG_INFO, "in dap_send_get_or_put(%s)\n",
	writing?"$PUT":"$GET"));

    dap_control_message ctl;
    ctl.set_ctlfunc(dap_control_message::GET);
    ctl.set_rac(dap_control_message::SEQFT);

    if (transfer_mode == MODE_BLOCK && !writing)
    {
	ctl.set_rac(dap_control_message::BLOCKFT);
    }

    if (writing)
    {
	ctl.set_ctlfunc(dap_control_message::PUT);
    }
    if (user_flags & file::FILE_FLAGS_RRL)
    {
    	ctl.set_rop_bit(dap_control_message::RB$RRL);
    }
    return !ctl.write(conn);
}
/*-------------------------------------------------------------------------*/

// Send Access Complete.
int dnetfile::dap_send_accomp()
{
    dap_accomp_message accomp;
    accomp.set_cmpfunc(dap_accomp_message::CLOSE);

    /* Remote printing support */
    if (user_flags & FILE_FLAGS_SPOOL)
	    accomp.set_fop_bit(dap_attrib_message::FB$SPL);
    if (user_flags & FILE_FLAGS_DELETE)
	    accomp.set_fop_bit(dap_attrib_message::FB$DLT);
    if (!accomp.write(conn)) return -1;
    conn.set_blocked(false);

    if (writing)
	return dap_get_reply();
    else
	return 0;
}
/*-------------------------------------------------------------------------*/
int dnetfile::dap_get_record(char *rec, int reclen)
{
    if (verbose > 2) DAPLOG((LOG_INFO, "in dap_get_record()\n"));

    dap_message *m = dap_message::read_message(conn,true);
    if (m)
    {
	if (m->get_type() == dap_message::STATUS)
	{
	    dap_status_message *sm = (dap_status_message *)m;
	    if ( (sm->get_code() & 0xFF) == 047)
	    {
		ateof = TRUE;
		return -1;
	    }
	    // good STATUS here means we need to request the next record
	    if (dap_check_status(m, 0) == 0)
		dap_send_get_or_put(); 
	    return dap_check_status(m,0);
	}

	if (m->get_type() == dap_message::ACK)
	{
	    return 0; //CC Why need this ??
	}

	if (m->get_type() != dap_message::DATA)
	{
	    sprintf(errstring, "Wrong block type (%s) received", m->type_name());
	    lasterror = errstring;
	    return -1;
	}
	dap_data_message *dm = (dap_data_message *)m;
	unsigned int len = dm->get_datalen();
	rec[len] = 0;
	memcpy(rec, dm->get_dataptr(), len);

	return len;
    }
    lasterror = conn.get_error();
    return -1;
}

// Send a record to the other end
int dnetfile::dap_put_record(char *rec, int reclen)
{
    if (verbose > 2) DAPLOG((LOG_INFO, "in dap_put_record(%d bytes)\n", reclen));
    // Send blocked...it's *much* faster
    conn.set_blocked(true);

    dap_data_message data;
    data.set_data(rec, reclen);
    data.write_with_len(conn);

    // Check for out-of-band messages
    dap_message *d = dap_message::read_message(conn, false);
    if (d)
	return dap_check_status(d,0);
    return 0;
}

// Send the attributes for a newly created file
int dnetfile::dap_send_attributes()
{
    if (verbose > 2) DAPLOG((LOG_INFO, "in dap_send_attributes()\n"));

    dap_attrib_message att;
    dap_alloc_message  all;

    att.set_org(dap_attrib_message::FB$SEQ);

// Only VMS can cope with StreamLF files.
    if (conn.get_remote_os() == 7)
        att.set_rfm(dap_attrib_message::FB$STMLF);
    else
        att.set_rfm(dap_attrib_message::FB$VAR);
    att.set_bls(512);
    att.set_mrs(user_bufsize);

// CR attributes on all but BLOCK mode sends
    if (transfer_mode != MODE_BLOCK)
    {
	att.set_rat_bit(dap_attrib_message::FB$CR);
    }

// Set the attributes of the uploaded file
    if (user_rat != RAT_DEFAULT)
    {
	if (user_rat == RAT_FTN) att.set_rat_bit(dap_attrib_message::FB$FTN);
	if (user_rat == RAT_CR)  att.set_rat_bit(dap_attrib_message::FB$CR);
	if (user_rat == RAT_PRN) att.set_rat_bit(dap_attrib_message::FB$PRN);
    }

    if (user_rfm != RFM_DEFAULT)
	att.set_rfm(user_rfm);

    conn.set_blocked(true);
    att.write(conn);

// VMS likes an ALLOC message
    if (conn.get_remote_os() == 7)
	all.write(conn);

    if (protection)
    {
	dap_protect_message prot;
	if (prot.set_protection(protection) == -1)
	    fprintf(stderr, "Error in protection string '%s' - not sent\n", protection);
	else
	    prot.write(conn);
    }
    return conn.set_blocked(false);
}

int dnetfile::dap_send_name()
{
    if (verbose > 2) DAPLOG((LOG_INFO, "in dap_send_name() %s\n", name));

    dap_name_message nam;
    nam.set_nametype(dap_name_message::FILESPEC);
    nam.set_namespec(name);
    return !nam.write(conn);
}

// Skip to the next file. (used for skipping directories)
int dnetfile::dap_send_skip()
{
    dap_contran_message ct;
    ct.set_confunc(dap_contran_message::SKIP);
    return !ct.write(conn);
}

// Check a STATUS message. code 0225 is Success and 047 is EOF so we return a
// correct status code for those cases.
int dnetfile::dap_check_status(dap_message *m, int status)
{
    if (m->get_type() != dap_message::STATUS) return -1;

    dap_status_message *sm = (dap_status_message *)m;

    // Save this stuff so we can delete the message
    int code = sm->get_code() & 0xFF;
    int maccode = sm->get_code() >> 12;
    const char *err = sm->get_message();

    if (verbose > 1)
	DAPLOG((LOG_INFO, "dap_check_status. maccode=%d code: octal: %o (hex: %x)\n", maccode, sm->get_code(),sm->get_code()));

    delete m;

    if (maccode == 1) return status; // Success
    if (code == 0225) return status; // Success
    if (code == 047)  return code;   // EOF
    lasterror = err;
    return -1;
}
