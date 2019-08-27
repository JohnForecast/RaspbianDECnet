/*
    rmsp.h from librms

    Copyright (C) 1999 Christine Caulfield       christine.caulfield@googlemail.com

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

// Private definitions for librms
// If you rely on the contents of this file in your application
// IT WILL BREAK when I release future versions of librms.
// You have been warned!

class rms_conn
{
 public:
    dap_connection *conn;
    int lasterr;           // DAP code of last error
    char *lasterror;       // Text of last error
    char *record;          // Temp storage for records that are too long
    int  dlen;             // Size of message
    char key[256];

    rms_conn(dap_connection *c)
	{
	    conn = c;
	    lasterr = 0;
	    lasterror = NULL;
	    record = NULL;
	}

};

int   rms_getreply(RMSHANDLE h, int wait, struct FAB *fab, dap_message **msg);
int   check_status(rms_conn *c, dap_message *m);
bool  parse_options(RMSHANDLE h, char *options, struct FAB *fab, struct RAB *rab, va_list ap);

