/*
    parse.cc from librms

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

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#include "connection.h"
#include "protocol.h"
#include "logging.h"
#include "rms.h"
#include "rmsp.h"

#define SPAN_BLANKS(string, index) \
while (string[index] == ' ' && string[index] != '\0') index++;

// These are the routines that fill in the RAB/FAB fields from the value.
// Declared forward so we can put them in the struct
static void set_key(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab);
static void set_val(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab);
static void set_facshr(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab);
static void set_rfm(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab);
static void set_rat(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab);
static void set_rac(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab);
static void set_rop(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab);

struct types
{
    const char  *key;
    size_t offset;
    enum {RAB, FAB} rabfab;
    enum {BYTE, WORD, LONG, PTR} type;
    void (*proc)(RMSHANDLE h, int, char *, struct RAB *, struct FAB *);
} field_types[] =
{
    {"fac", offsetof(struct FAB, fab$b_fac), types::FAB, types::BYTE, set_facshr},
    {"shr", offsetof(struct FAB, fab$b_shr), types::FAB, types::BYTE, set_facshr},
    {"rfm", offsetof(struct FAB, fab$b_rfm), types::FAB, types::BYTE, set_rfm},
    {"rat", offsetof(struct FAB, fab$b_rat), types::FAB, types::BYTE, set_rat},
    {"rac", offsetof(struct RAB, rab$b_rac), types::RAB, types::BYTE, set_rac},
    {"rop", offsetof(struct RAB, rab$l_rop), types::RAB, types::LONG, set_rop},
    {"kop", offsetof(struct RAB, rab$l_rop), types::RAB, types::LONG, set_rop},
    {"kbf", offsetof(struct RAB, rab$l_kbf), types::RAB, types::PTR,  set_key},
    {"key", offsetof(struct RAB, rab$l_kbf), types::RAB, types::PTR,  set_key},
    {"ksz", offsetof(struct RAB, rab$b_ksz), types::RAB, types::BYTE, set_val},
    {"krf", offsetof(struct RAB, rab$b_krf), types::RAB, types::BYTE, set_val},

    {NULL, 0} // End of the list
};

// This is a bit gross really.
// What I need to do is pass the va_list variable into get_item by reference
// so that it can increment the pointer for each arg it finds. 
// HOWEVER, some machines (PPC that I know of) have a definition of va_list
// that prevents this passing by reference, so what I am doing now is to 
// enclose the va_list in a structure and pass /that/ by reference.
// The real horror is that I need to use __va_copy() to copy the va_list
// into the structure. I know: you're not supposed to use double-underscored
// names. If anyone has a better way around this then please tell me.
struct va_list_passer
{
    va_list ap;
};

static bool get_item(char *options, unsigned int &option_ptr,
		     char *key, char *value, struct va_list_passer &vlp)
{
    // We've reached the end.
    if (option_ptr+4 >= strlen(options)) return false;

    // All options are 3 characters long...
    strncpy(key, &options[option_ptr], 3);
    key[3] = '\0';
    option_ptr += 3;

    SPAN_BLANKS(options, option_ptr);
    if (options[option_ptr] != '=')
    {
	fprintf(stderr, "Error in options string, '=' missing after %s :\n%s\n", key, options);
	return false;
    }
    option_ptr++;
    SPAN_BLANKS(options, option_ptr);

    // Is the value quoted ?
    bool quoted = false;
    char end_char = ',';
    if (options[option_ptr] == '"')
    {
	option_ptr++;
	quoted = true;
	end_char = '"';
    }
    if (options[option_ptr] == '\'')
    {
	option_ptr++;
	quoted = true;
	end_char = '\'';
    }

    int value_ptr = 0;

    // Find the end of the value
    while (option_ptr < strlen(options))
    {
	// Check for doubled quotes
	if (options[option_ptr] == '"' &&
	    options[option_ptr+1] == '"')
	{
	    option_ptr++;
	    value[value_ptr++] = '"';
	    continue;
	}

	// Check for the end of the value
	if (options[option_ptr] == end_char)
	{
	    option_ptr++;
	    break;
	}

	// Just copy the character
	value[value_ptr++] = options[option_ptr++];
    }

    // Terminate the string
    value[value_ptr] = '\0';


    // Check for % options which indicate items in the variable argument list
    if (value[0] == '%')
    {
	int len = 0;
	int ptr = 1;
	int intval;
	char *charval;

	if (value[1] == '*') // Length is also an argument
	{
	    ptr++;
	    len = va_arg(vlp.ap, int);
	}
	switch (value[ptr])
	{
	case 'd':
	    // Rather depressingly we have to convert this to a string before 
	    // converting it back to a number :-(
	    intval = va_arg(vlp.ap, int);
	    sprintf(value, "%d", intval);
	    break;

	case 's':
	    charval = va_arg(vlp.ap, char *);
	    if (len)
	    {
		memcpy(value, charval, len);
	    }
	    else
	    {
		strcpy(value, charval);
	    }
	    break;
	}
    }


    // If there's a comma then skip past it
    SPAN_BLANKS(options, option_ptr);
    if (options[option_ptr] == ',')
	option_ptr++;
    SPAN_BLANKS(options, option_ptr);

    return true;
}

// Parse string options into a RAB and a FAB.
// Options consist of a set of comma separated key=value pairs. If the
// value contains a space it must be surrounded by double quotes. If it
// contains a double quote then it must be doubled up or in single quotes.
bool parse_options(RMSHANDLE h, char *options, struct FAB *fab, 
		   struct RAB *rab, va_list ap)
{
    unsigned int option_ptr = 0;
    char key[4];     // All option names are 3 letters
    char value[256]; // All values are shorter than 256 bytes
    struct va_list_passer vlp;

    memset(fab, 0, sizeof(struct FAB));
    memset(rab, 0, sizeof(struct RAB));

    // a NULL option string is legal
    if (!options) return true;

// Oh dear, oh dear, oh dear.
#ifdef __va_copy
    __va_copy(vlp.ap, ap);
#else
    vlp.ap = ap;
#endif

    while (get_item(options, option_ptr, key, value, vlp))
    {
	int i=0;
	while (field_types[i].key)
	{
	    if (strcasecmp(key, field_types[i].key) == 0)
	    {
		// Do it!
		field_types[i].proc(h, i, value, rab, fab);
		break;
	    }
	    i++;
	}
	if (!field_types[i].key)
	{
	    fprintf(stderr,"Unrecognised key name: %s\n", options);
	    return false;
	}
    }
    return true;
}


static void set_key(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab)
{
    rms_conn *rc = (rms_conn *)h;

// Save the key for this RAB
    if (rab->rab$b_ksz)
    {
	memcpy(rc->key, string, rab->rab$b_ksz);
    }
    else
    {
        strcpy(rc->key, string);
        rab->rab$b_ksz = 0;
    }
    rab->rab$l_kbf = rc->key;

// As this is set_key - add rac=key and set the key size
    rab->rab$b_rac |= RAB$C_KEY;
}

static void set_val(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab)
{
    void *ptr;

    if (field_types[entry].rabfab == types::FAB)
	ptr = (char *)fab+field_types[entry].offset;
    else
	ptr = (char *)rab+field_types[entry].offset;

    switch (field_types[entry].type)
    {
    case types::BYTE:
	*(char *)ptr = atoi(string);
	break;
    case types::WORD:
	*(short *)ptr = atoi(string);
	break;
    case types::LONG:
	*(int *)ptr = atoi(string);
	break;
    default:
	fprintf(stderr, "Unknown type in array. This is a stupid bug.\n");
	exit(1);
    }
}

static void makelower(char *s)
{
    for (unsigned int i=0; s[i]; i++) s[i] = tolower(s[i]);
}

static void set_facshr(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab)
{
    char *ptr = (char*)fab+field_types[entry].offset;

    makelower(string);
    char *p = strtok(string,",");
    while (p)
    {
	if (strcmp(p, "put") == 0) {*ptr |= FAB$M_PUT; goto shrdone;}
	if (strcmp(p, "get") == 0) {*ptr |= FAB$M_GET; goto shrdone;}
	if (strcmp(p, "upd") == 0) {*ptr |= FAB$M_UPD; goto shrdone;}
	if (strcmp(p, "del") == 0) {*ptr |= FAB$M_DEL; goto shrdone;}
	fprintf(stderr, "Error: unknown access/share type: %s\n", p);
	return;

    shrdone:
	p = strtok(NULL, ",");
    }
}

static void set_rfm(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab)
{
    unsigned char *ptr = &fab->fab$b_rfm;

    makelower(string);
    char *p = strtok(string,",");
    while (p)
    {
	if (strcmp(p, "udf") == 0)   {*ptr |= FAB$C_UDF; goto rfmdone;}
	if (strcmp(p, "fix") == 0)   {*ptr |= FAB$C_FIX; goto rfmdone;}
	if (strcmp(p, "var") == 0)   {*ptr |= FAB$C_VAR; goto rfmdone;}
	if (strcmp(p, "vfc") == 0)   {*ptr |= FAB$C_VFC; goto rfmdone;}
	if (strcmp(p, "stm") == 0)   {*ptr |= FAB$C_STM; goto rfmdone;}
	if (strcmp(p, "stmlf") == 0) {*ptr |= FAB$C_STMLF; goto rfmdone;}
	if (strcmp(p, "stmcr") == 0) {*ptr |= FAB$C_STMCR; goto rfmdone;}

	fprintf(stderr, "Error: unknown record format type: %s\n", p);
	return;

    rfmdone:
	p = strtok(NULL, ",");
    }
}

static void set_rat(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab)
{
    unsigned char *ptr = &fab->fab$b_rat;

    makelower(string);
    char *p = strtok(string,",");
    while (p)
    {
	if (strcmp(p, "ftn") == 0) {*ptr |= FAB$M_FTN; goto ratdone;}
	if (strcmp(p, "cr" ) == 0) {*ptr |= FAB$M_CR;  goto ratdone;}
	if (strcmp(p, "prn") == 0) {*ptr |= FAB$M_PRN; goto ratdone;}
	if (strcmp(p, "blk") == 0) {*ptr |= FAB$M_BLK; goto ratdone;}
	fprintf(stderr, "Error: unknown record attribute type: %s\n", p);
	return;

    ratdone:
	p = strtok(NULL, ",");
    }
}

static void set_rac(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab)
{
    unsigned char *ptr = &rab->rab$b_rac;

    makelower(string);
    char *p = strtok(string,",");
    while (p)
    {
	if (strcmp(p, "seq") == 0) {*ptr |= RAB$C_SEQ; goto racdone;}
	if (strcmp(p, "key") == 0) {*ptr |= RAB$C_KEY; goto racdone;}
	if (strcmp(p, "rfa") == 0) {*ptr |= RAB$C_RFA; goto racdone;}

	fprintf(stderr, "Error: unknown record access mode: %s\n", p);
	return;

    racdone:
	p = strtok(NULL, ",");
    }
}

static void set_rop(RMSHANDLE h, int entry, char *string, RAB *rab, FAB *fab)
{
    unsigned long *ptr = &rab->rab$l_rop;

    makelower(string);
    char *p = strtok(string,",");
    while (p)
    {
	if (strcmp(p, "eof") == 0) {*ptr |= RAB$M_EOF; goto ropdone;}
	if (strcmp(p, "fdl") == 0) {*ptr |= RAB$M_FDL; goto ropdone;}
	if (strcmp(p, "uif") == 0) {*ptr |= RAB$M_UIF; goto ropdone;}
	if (strcmp(p, "hsh") == 0) {*ptr |= RAB$M_HSH; goto ropdone;}
	if (strcmp(p, "loa") == 0) {*ptr |= RAB$M_LOA; goto ropdone;}
	if (strcmp(p, "ulk") == 0) {*ptr |= RAB$M_ULK; goto ropdone;}
	if (strcmp(p, "tpt") == 0) {*ptr |= RAB$M_TPT; goto ropdone;}
	if (strcmp(p, "rah") == 0) {*ptr |= RAB$M_RAH; goto ropdone;}
	if (strcmp(p, "wbh") == 0) {*ptr |= RAB$M_WBH; goto ropdone;}
	if (strcmp(p, "kge") == 0) {*ptr |= RAB$M_KGE; goto ropdone;}
	if (strcmp(p, "kgt") == 0) {*ptr |= RAB$M_KGT; goto ropdone;}
	if (strcmp(p, "nlk") == 0) {*ptr |= RAB$M_NLK; goto ropdone;}
	if (strcmp(p, "rlk") == 0) {*ptr |= RAB$M_RLK; goto ropdone;}
	if (strcmp(p, "bio") == 0) {*ptr |= RAB$M_BIO; goto ropdone;}
	if (strcmp(p, "nxr") == 0) {*ptr |= RAB$M_NXR; goto ropdone;}
	if (strcmp(p, "wat") == 0) {*ptr |= RAB$M_NXR; goto ropdone;}
	if (strcmp(p, "rrl") == 0) {*ptr |= RAB$M_NXR; goto ropdone;}
	if (strcmp(p, "rea") == 0) {*ptr |= RAB$M_NXR; goto ropdone;}
	if (strcmp(p, "kle") == 0) {*ptr |= RAB$M_NXR; goto ropdone;}
	if (strcmp(p, "kgt") == 0) {*ptr |= RAB$M_NXR; goto ropdone;}


	fprintf(stderr, "Error: unknown record option: %s\n", p);
	return;

    ropdone:
	p = strtok(NULL, ",");
    }
}
