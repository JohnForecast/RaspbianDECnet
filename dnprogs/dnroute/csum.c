/*
 * csum.c       Checksum calculation for DECnet routing
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:     Christine Caulfield <christine.caulfield@googlemail.com>
 *
 */
#include <stdio.h>

unsigned short route_csum(unsigned char *buf, int start, int end)
{
    unsigned int  sum = 1; /* Starting value for Phase IV */
    int i;

    for (i=start; i<end; i++, i++)
    {
	sum += buf[i] + (buf[i+1]<<8);
    }
    sum = (sum>>16) + (sum&0xFFFF);
    sum = (sum>>16) + (sum&0xFFFF);

    return (unsigned short)sum;
}
