/******************************************************************************
    (c) 1998-2000 Christine Caulfield               christine.caulfield@googlemail.com
    
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
#include <string.h>

char config_hostname[1024];
char config_vmsmailuser[1024];
char config_smtphost[1024];

//
// Read the VMSmail config file ($SYSCONF_PREFIX/etc/vmsmail.conf) to determine parameters
// for the linux/VMSmail gateway programs.
//

void read_configfile(void)
{
    FILE *cf;
    char cfgline[1024];

    // make defaults.
    strcpy(config_vmsmailuser, "vmsmail");
    gethostname(config_hostname, sizeof(config_hostname));
    config_smtphost[0] = '\0'; // Use sendmail.
    
    cf = fopen(SYSCONF_PREFIX "/etc/vmsmail.conf", "r");
    if (cf)
    {
	fgets(cfgline, sizeof(cfgline), cf);
	while (!feof(cf))
	{
	    char *eq;
	    
	    if (cfgline[strlen(cfgline)-1] == '\n')
		cfgline[strlen(cfgline)-1] = '\0';
	    eq = strchr(cfgline, '=');
	    if (eq)
	    {
		*eq = '\0';
		
		// Add more config variables here:
		
		if (strcasecmp(cfgline, "hostname") == 0)
		    strcpy(config_hostname, eq+1);
		if (strcasecmp(cfgline, "username") == 0)
		    strcpy(config_vmsmailuser, eq+1);
		if (strcasecmp(cfgline, "smtphost") == 0)
		    strcpy(config_smtphost, eq+1);
	    }
	    fgets(cfgline, sizeof(cfgline), cf);
	}
	fclose(cf);
    }
}
