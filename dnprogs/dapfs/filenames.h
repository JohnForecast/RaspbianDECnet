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
/* filenames.c */
void make_vms_filespec(const char *unixname, char *vmsname, int full);
void make_unix_filespec(char *unixname, char *vmsname);
void makeupper(char *s);
