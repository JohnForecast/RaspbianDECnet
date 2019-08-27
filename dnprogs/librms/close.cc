/*
    close.cc from librms

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

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "connection.h"
#include "protocol.h"
#include "rms.h"
#include "rmsp.h"

int rms_close(RMSHANDLE h)
{
    if (!h) return -1;

    rms_conn *rc = (rms_conn *)h;
    dap_connection *conn = (dap_connection *)rc->conn;

    dap_accomp_message ac;
    ac.set_cmpfunc(dap_accomp_message::CLOSE);
    ac.write(*conn);

    dap_message *m;
    int r = rms_getreply(h, 1, NULL, &m);

    conn->close();
    
    delete conn;
    delete rc;

    return r;
}


// Just for consistancy
int rms_t_close(RMSHANDLE h)
{
    return rms_close(h);
}
