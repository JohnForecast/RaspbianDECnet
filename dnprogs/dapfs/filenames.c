/******************************************************************************
    (c) 2005-2008 Christine Caulfield             christine.caulfield@gmail.com

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

/* This code is lifted from FAL. */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <glob.h>
#include <regex.h>
#include <string.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "filenames.h"

#define true 1
#define false 0

const char *sysdisk_name = "SYS$SYSDEVICE";

// A couple of general utility methods:
static void makelower(char *s)
{
	unsigned int i;
	for (i=0; s[i]; i++) s[i] = tolower(s[i]);
}

void makeupper(char *s)
{
	unsigned int i;
	for (i=0; s[i]; i++) s[i] = toupper(s[i]);
}

// Convert a Unix-style filename to a VMS-style name
// No return code because this routine cannot fail :-)
void make_vms_filespec(const char *unixname, char *vmsname, int isdir)
{
    char        fullname[PATH_MAX];
    int         i;
    char       *lastslash;

    // Take a copy wwe can muck about with
    strcpy(fullname, unixname);

    // Find the last slash in the name
    lastslash = fullname + strlen(fullname);
    while (*(--lastslash) != '/') ;

    // If the filename has no extension then add one. VMS seems to
    // expect one as does dapfs.
    if (!strchr(lastslash, '.' && !isdir))
        strcat(fullname, ".");

    int slashes = 0;

    // Oh, also make it all upper case for VMS's benefit.
    for (i=0; i<(int)strlen(fullname); i++)
    {
	if (islower(fullname[i])) fullname[i] = toupper(fullname[i]);
	if (fullname[i] == '/')
	{
	    slashes++;
	}
    }

    if (slashes == 1)
    {
	    sprintf(vmsname, "%s", unixname+1);
	    return;
    }

    int thisslash = 0;
    int v=0;
    for (i=0; i<=(int)strlen(fullname); i++)
    {
	    if (i==0)
	    {
		    vmsname[v++] = '[';
		    vmsname[v++] = '.';
		    thisslash++;
	    }
	    else
	    {
		    if (fullname[i] == '/')
		    {
			    thisslash++;
			    if (thisslash == slashes)
				    vmsname[v++] = ']';
			    else
				    vmsname[v++] = '.';
		    }
		    else
			    vmsname[v++] = fullname[i];
	    }
    }
}

// Split out the volume, directory and file portions of a VMS file spec
// We assume that the VMS name is (quite) well formed.
static void parse_vms_filespec(char *volume, char *directory, char *file)
{
    char *colon = strchr(file, ':');
    char *ptr = file;

    volume[0] = '\0';
    directory[0] = '\0';

    if (colon) // We have a volume name
    {
	char saved = *(colon+1);
	*(colon+1) = '\0';
	strcpy(volume, file);
	ptr = colon+1;
	*ptr = saved;
    }

    char *enddir = strchr(ptr, ']');

    // Don't get caught out by concatenated filespecs
    // like dua0:[home.chrissie.][test]
    if (enddir && enddir[1] == '[')
	enddir=strchr(enddir+1, ']');

    if (*ptr == '[' && enddir) // we have a directory
    {
	char saved = *(enddir+1);

	*(enddir+1) = '\0';
	strcpy(directory, ptr);
	ptr = enddir+1;
	*ptr = saved;
    }

    // Copy the rest of the filename using memmove 'cos it might overlap
    if (ptr != file)
	memmove(file, ptr, strlen(ptr)+1);

}

// Convert a VMS filespec into a Unix filespec
// volume names are turned into directories in the root directory
// (unless they are SYSDISK which is our pseudo name)
void make_unix_filespec(char *unixname, char *vmsname)
{
    char volume[PATH_MAX];
    char dir[PATH_MAX];
    char file[PATH_MAX];
    int  ptr;
    int  i;

    strcpy(file, vmsname);

    // Remove the trailing version number
    char *semi = strchr(file, ';');
    if (semi) *semi = '\0';

    // If the filename has a trailing dot them remove that too
    if (file[strlen(file)-1] == '.')
        file[strlen(file)-1] = '\0';

    unixname[0] = '\0';

    // Split it into its component parts
    parse_vms_filespec(volume, dir, file);

    // Remove the trailing colon from the volume name
    if (volume[strlen(volume)-1] == ':')
	volume[strlen(volume)-1] = '\0';

    // If the filename has the dummy SYSDISK volume then start from the
    // filesystem root
    if (strcasecmp(volume, sysdisk_name) == 0)
    {
	strcpy(unixname, "/");
    }
    else
    {
	if (volume[0] != '\0')
	{
	    strcpy(unixname, "/");
	    strcat(unixname, volume);
	}
    }
    ptr = strlen(unixname);

    // Copy the directory
    for (i=0; i< (int)strlen(dir); i++)
    {
	// If the directory name starts [. then it is relative to the
	// user's home directory and we lose the starting slash
	// If there is also a volume name present then it all falls
	// to bits but then it's pretty dodgy on VMS too.
	if (dir[i] == '[' &&
	    dir[i+1] == '.')
	{
	    i++;
	    ptr = 0;
	    continue;
	}

	if (dir[i] == '[' ||
	    dir[i] == ']' ||
	    dir[i] == '.')
	{
	    unixname[ptr++] = '/';
	}
	else
	{
	    // Skip root directory specs
	    if (dir[i] == '0' && (strncmp(&dir[i], "000000", 6) == 0))
	    {
		i += 5;
		continue;
	    }
	    if (dir[i] == '0' && (strncmp(&dir[i], "0,0", 3) == 0))
	    {
		i += 2;
		continue;
	    }
	    unixname[ptr++] = dir[i];
	}
    }
    unixname[ptr++] = '\0'; // so that strcat will work properly

    // A special case (ugh!), if VMS sent us '*.*' (maybe as part of *.*;*)
    // then change it to just '*' so we get all the files.
    if (strcmp(file, "*.*") == 0) strcpy(file, "*");

    strcat(unixname, file);

    // Finally convert it all to lower case. This is not the greatest way to
    // cope with it but because VMS will upper-case everything anyway we
    // can't really distinguish case. I know samba does fancy stuff with
    // matching various combinations of case but I really can't be bothered.
    makelower(unixname);

    // If the name ends in .dir and there is a directory of that name without
    // the .dir then remove it (the .dir, not the directory!)
    if (strstr(unixname, ".dir") == unixname+strlen(unixname)-4)
    {
	char dirname[strlen(unixname)+1];
	struct stat st;

	strcpy(dirname, unixname);
	char *ext = strstr(dirname, ".dir");
	if (ext) *ext = '\0';
	if (stat(dirname, &st) == 0 &&
	    S_ISDIR(st.st_mode))
	{
	    char *ext = strstr(unixname, ".dir");
	    if (ext) *ext = '\0';
	}
    }
}
