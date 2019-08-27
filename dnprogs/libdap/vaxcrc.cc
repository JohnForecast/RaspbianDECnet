/******************************************************************************
    (c) Eduardo Marcelo Serrat		  emserrat@geocities.com 
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Cyclic Redundancy Check
    -----------------------

    Calculates a 16-bit CRC for use on DECnet for Linux.
    The DAP protocol uses the following polynomial:

    DAPPOLY(0xE905):

    X^16 + X^15 + X^13 + X^7 + X^4 + X^2 + X^1 + X^0 

    The methods "calc1shift", "calc2shift" and "calc4shift" implements the
    one-shift, two-shift and four-shift algorithm options correspondingly.
    The "calc4shift" method should be the less CPU Intensive.

    Does anybody know which one implements the VAX CRC hardware instruction???
******************************************************************************/

#include "vaxcrc.h"

/*-------------------------------------------------------------------------*/
vaxcrc::vaxcrc(unsigned short poly, unsigned short inicrc)
{
	for (int i=0; i < 16; i++)
	{
		unsigned short tmp=i;
		for (int k=0; k < 4; k++)
		{
			int C = tmp & 1;
			tmp = tmp >> 1;
			if (C) tmp ^= poly;
		}
		crc_table[i]=tmp;
	}
	crc=inicrc;
}
/*-------------------------------------------------------------------------*/
void vaxcrc::setcrc(unsigned short newcrc)
{
	crc=newcrc;
}
/*-------------------------------------------------------------------------*/
unsigned short vaxcrc::getcrc()
{
	return crc;
}
/*-------------------------------------------------------------------------*/
void vaxcrc::calc1shift(unsigned char *stream, int len)
{
	while (len--)
	{
		crc ^= *stream++;
		for (int k=0; k < 8; k++)
			crc = (crc >> 1) ^ crc_table[8*(crc & 1)];
	}	
}
/*-------------------------------------------------------------------------*/
void vaxcrc::calc2shift(unsigned char *stream, int len)
{
	while (len--)
	{
		crc ^= *stream++;
		for (int k=0; k < 4; k++)
			crc = (crc >> 2) ^ crc_table[4*(crc & 3)];
	}	
}
/*-------------------------------------------------------------------------*/
void vaxcrc::calc4shift(unsigned char *stream, int len)
{
	while (len--)
	{
		crc ^= *stream++;
		for (int k=0; k < 2; k++)
			crc = (crc >> 4) ^ crc_table[crc & 15];
	}	
}
/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
