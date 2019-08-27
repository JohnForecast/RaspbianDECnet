/******************************************************************************
    (c) 1998-2008 C.H Caulfield              Christine.Caulfield@googlemail.com

    
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
/* Header file to cope with endian issues */

#if defined(__NetBSD__) || defined(__FreeBSD__)
#include <machine/endian.h>
#define __BYTE_ORDER BYTE_ORDER
#endif


#ifndef __BYTE_ORDER
    #error "Can't determine endianness - please inform Christine.Caulfield@googlemail.com with your distribution and hardware type."
#endif

#if (__BYTE_ORDER == 1234)

  /* It's a VAX or Intel, or some other obscure make :-) */

  /* DECnet is little-endian so these are no-ops */
  #define dn_ntohs(x) (x)
  #define dn_htons(x) (x)

  #define dn_ntohl(x) (x)
  #define dn_htonl(x) (x)

#else
  #if (__BYTE_ORDER == 4321)

    /* It's a SPARC - most likely than not */
 
    #define dn_ntohs(x) ((((x)&0x0ff)<<8) | (((x)&0xff00)>>8))

    #define dn_ntohl(x) ( ((dn_ntohs((x)&0xffff))<<16) |\
			  ((dn_ntohs(((x)>>16)))) )

    #define dn_htonl(x) dn_ntohl(x)
    #define dn_htons(x) dn_ntohs(x)
  #else

    /* it's a PDP??? */
    #error "Unsupported endianness - please inform Christine.Caulfield@googlemail.com with your distribution and hardware type."
  #endif
#endif
