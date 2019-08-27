/******************************************************************************
    (c) 1998-2009 Christine Caulfield         christine.caulfield@googlemail.com

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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <string.h>
#include "file.h"
#include "unixfile.h"

#ifdef __FreeBSD__
#include <libgen.h>
#endif

// basename() is in libc but not in my header files
//extern "C" char *basename(const char *);

int unixfile::open(const char *mode)
{

// if the filename was "-" then open standard in/output
    if (!strcmp(filename, "-"))
    {
	if (mode[0] == 'w' || mode[0] == 'a')
	    stream = fdopen(STDOUT_FILENO, "w");
	else
	    stream = fdopen(STDIN_FILENO, "r");
    }
    else
    {
	stream = fopen(filename, mode);
    }
    strcpy(printname, filename);

// Check it was opened OK
    if (stream)
	return 0;
    else
	return -1;
}

// This open routine is called when the output file is a directory (ie a
// copy from multiple sources) so we don't need to check for stdin/out.
int unixfile::open(const char *basename, const char *mode)
{
    strcpy(printname, filename);
    strcat(printname, "/");
    strcat(printname, basename);

    stream = fopen(printname, mode);

    if (stream)
	return 0;
    else
	return -1;
}

// We honour the transfer mode here mainly for consistancy.
int unixfile::read(char *buf, int len)
{
    if (transfer_mode == MODE_RECORD)
    {
	char *out = buf;
	char *in  = record_buffer + record_ptr;
	char *endin  = record_buffer+record_buflen;
	int   reclen = 0;

	do
	{
	    // Refill our buffer
	    if (record_ptr == record_buflen)
	    {
		record_buflen = ::fread(record_buffer, 1, RECORD_BUFSIZE, stream);
		record_ptr = 0;
		if (record_buflen <= 0)
		{
		    if (reclen)
			return reclen;
		    else
			return -1;
		}
	    }
	    in    = record_buffer + record_ptr;
	    endin = record_buffer + record_buflen;
	    int copied = 0;

	    // Copy up to the next LF *OR* the callers buffer length
	    do
	    {
		*(out++) = *(in++);
		copied++;
	    }
            while (*in != '\n' && in < endin && copied+reclen < len);
	    record_ptr += copied;
	    reclen     += copied;
	    // Maybe just ran out of input buffer
	}
	while (*in != '\n' && reclen < len);

// If we finished on an LF then include it in the send buffer
	if (*in == '\n')
	{
	    *(out++) = '\n';
	    record_ptr++;
	    reclen++;
	}
	return reclen;
    }
    else // Just read a block
    {
        int reclen;
	reclen = ::fread(buf, 1, len, stream);
	if (reclen <= 0)
	    return -1;

        // For block-mode we pad out the block to the
        // full block size. We never return a partial block.
        if (reclen != (int)block_size)
	{
	    memset(buf+reclen, 0, block_size - reclen);
	}
	return block_size;
    }
    return -1; // Duh?
}

int unixfile::write(char *buf, int len)
{
    return ::fwrite(buf, 1, len, stream);
}

bool unixfile::eof()
{
// Don't know why this is necessary
#if defined(__NetBSD__) || defined(__FreeBSD__)
    return feof(stream);
#else
    return ::feof(stream);
#endif
}

int unixfile::close()
{
    int status;

    status =  ::fclose(stream);
    stream = NULL;

    return status;
}

void unixfile::perror(const char *msg)
{
    ::perror(msg);
}

bool unixfile::isdirectory()
{
    struct stat s;

    if (::stat(filename, &s)) return FALSE; // Doesn't exist
    return S_ISDIR(s.st_mode);
}

int unixfile::setup_link(unsigned int bufsize, int rfm, int rat, int xfer_mode, int flags, int timeout)
{
// Save these for later
    user_rfm = rfm;
    user_rat = rat;
    transfer_mode = xfer_mode;
    block_size = bufsize;

// Allocate a buffer for copying records.
    if (transfer_mode == MODE_RECORD && !record_buffer)
    {
	record_buffer = (char *)malloc(RECORD_BUFSIZE);
    }

    return 0;
};

int unixfile::next()
{
    return FALSE;
};

char *unixfile::get_printname()
{
    static char realname[PATH_MAX];

    realpath(printname, realname);
    return realname;
}

char *unixfile::get_printname(char *filename)
{
    static char realname[PATH_MAX];
    static char tmpname[PATH_MAX];

    strcpy(tmpname, this->filename);
    strcat(tmpname, "/");
    strcat(tmpname, filename);

    realpath(tmpname, realname);

    return realname;
}

char *unixfile::get_basename(int keep_version)
{
    return ::basename(filename);
}

bool unixfile::iswildcard()
{
    return FALSE; // Wildcards are expanded by the shell
}


// Return a string telling the user whether we transferred blocks or
// records or what.
const char *unixfile::get_format_name()
{

    switch (transfer_mode)
    {
    case MODE_RECORD:
	return "records";
    case MODE_BLOCK:
    default:
	return "blocks";
    }
    return "things";
}


// Set the file protection. This assumes the file is open
int unixfile::get_umask()
{
    struct stat s;

    ::fstat(fileno(stream), &s);
    return s.st_mode;
}

// Set the file protection. This assumes the file is open
int unixfile::set_umask(int mask)
{
    return ::fchmod(fileno(stream), mask);
}

int unixfile::max_buffersize(int biggest)
{
    return biggest;
}

unixfile::unixfile()
{
    record_buffer = NULL;
    record_ptr = record_buflen = 0;
}
unixfile::~unixfile()
{
    if (record_buffer)
	free(record_buffer);
}

unixfile::unixfile(const char *name)
{
    strcpy(filename, name);
    record_buffer = NULL;
    record_ptr = record_buflen = 0;
}
