/*
    rms.h.h from librms

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

// Get the definitions of RABs and FABs
#include "fabdef.h"
#include "rabdef.h"

typedef void * RMSHANDLE;


#ifdef __cplusplus
extern "C" {
#endif

RMSHANDLE rms_open(char *name, int mode, struct FAB *);
int   rms_close(RMSHANDLE h);
int   rms_read(RMSHANDLE h, char *buf, int maxlen, struct RAB*);
int   rms_write(RMSHANDLE h, char *buf, int maxlen, struct RAB*);
int   rms_update(RMSHANDLE h, char *buf, int maxlen, struct RAB*);
int   rms_find(RMSHANDLE h, struct RAB *);
int   rms_delete(RMSHANDLE h, struct RAB *);
int   rms_rewind(RMSHANDLE h, struct RAB *);
int   rms_truncate(RMSHANDLE h, struct RAB *);
char *rms_lasterror(RMSHANDLE h);
int   rms_lasterrorcode(RMSHANDLE h);
char *rms_openerror(void);

RMSHANDLE rms_t_open(char *name, int mode, char *options, ...);
int   rms_t_close(RMSHANDLE h);
int   rms_t_read(RMSHANDLE h, char *buf, int maxlen, char *options, ...);
int   rms_t_write(RMSHANDLE h, char *buf, int maxlen, char *options, ...);
int   rms_t_update(RMSHANDLE h, char *buf, int maxlen, char *options, ...);
int   rms_t_find(RMSHANDLE h, char *options, ...);
int   rms_t_delete(RMSHANDLE h, char *options, ...);
int   rms_t_rewind(RMSHANDLE h, char *options, ...);
int   rms_t_truncate(RMSHANDLE h, char *options, ...);

#ifdef __cplusplus
}
#endif
