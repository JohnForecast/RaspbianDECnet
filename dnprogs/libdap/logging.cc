/******************************************************************************
    logging.cc from libdap

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

// Logging module for libdap
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include "logging.h"

static enum {DAPLOG_MONO, DAPLOG_STDERR, DAPLOG_SYSLOG} 
       log_type = DAPLOG_SYSLOG;

static bool show_pid = false;

void init_logging(const char *progname, char type, bool pid)
{
    switch (type)
    {
    case 'm': 
	log_type = DAPLOG_MONO;
	break;
    case 'e': 
	log_type = DAPLOG_STDERR;
	break;
    default:
	log_type = DAPLOG_SYSLOG;
	openlog(progname, LOG_PID, LOG_DAEMON);
	break;
    }
#ifndef NO_FORK
    show_pid = pid;
#endif
}

static void daplog_stderr(int level, const char *fmt, va_list ap)
{

    if (show_pid) fprintf(stderr, "[%d] ", getpid());

    vfprintf(stderr, fmt, ap);
}


// This will output to the /dev/mono device (using my mono driver - see
// web page for details) or tty13 which is usually the mono monitor
// in a framebuffer system.
static void daplog_mono(int level, const char *fmt, va_list ap)
{
    char        outbuf[4096];
    static int  fd = 0;

    if (!fd) fd = open("/dev/mono", O_WRONLY);
    if (!fd) fd = open("/dev/tty13", O_WRONLY);

#ifndef NO_FORK
    sprintf(outbuf, "[%d] ", getpid());
    write(fd, outbuf, strlen(outbuf));
#endif

    vsprintf(outbuf, fmt, ap);
    write(fd, outbuf, strlen(outbuf));
}

void daplog(int level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    switch(log_type)
    {
    case DAPLOG_MONO:
	daplog_mono(level, fmt, ap);
	break;
    case DAPLOG_STDERR:
	daplog_stderr(level, fmt, ap);
	break;
    case DAPLOG_SYSLOG:
	vsyslog(level, fmt, ap);
	break;
    }
    va_end(ap);
}
