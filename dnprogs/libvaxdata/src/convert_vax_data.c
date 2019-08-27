/******************************************************************************
 *                                                                            *
 * convert_vax_data.c - Convert VAX-format data to/from Unix (IEEE) format.   *
 *                                                                            *
 *    from_vax_i2()  - Byte swap Integer*2                                    *
 *    from_vax_i4()  - Byte reverse Integer*4                                 *
 *    from_vax_r4()  - 32-bit VAX F_floating to IEEE S_floating               *
 *    from_vax_d8()  - 64-bit VAX D_floating to IEEE T_floating               *
 *    from_vax_g8()  - 64-bit VAX G_floating to IEEE T_floating               *
 *    from_vax_h16() - 128-bit VAX H_floating to Alpha X_floating             *
 *                                                                            *
 *    to_vax_i2()    - Byte swap Integer*2                                    *
 *    to_vax_i4()    - Byte reverse Integer*4                                 *
 *    to_vax_r4()    - 32-bit IEEE S_floating to VAX F_floating               *
 *    to_vax_d8()    - 64-bit IEEE T_floating to VAX D_floating               *
 *    to_vax_g8()    - 64-bit IEEE T_floating to VAX G_floating               *
 *    to_vax_h16()   - 128-bit Alpha X_floating to VAX H_floating             *
 *                                                                            *
 * All calls take 3 arguments:                                                *
 *                                                                            *
 *    C declaration:  #include "convert_vax_data.h"                           *
 *            usage:  name( in_array, out_array, &count );                    *
 *                                                                            *
 *    Fortran usage:  Call NAME( in_array, out_array, count )                 *
 *                                                                            *
 * The in_array and out_array parameters may refer to the same object.        *
 *                                                                            *
 *                                                                            *
 * See  convert_vax_data.h  for a description of the VAX and Unix (IEEE) data *
 * formats and the compilation options available.                             *
 *                                                                            *
 *                                                                            *
 * References:  ANSI/IEEE Std 754-1985, IEEE Standard for Binary Floating-    *
 *                 Point Arithmetic, Institute of Electrical and Electronics  *
 *                 Engineers                                                  *
 *              Brunner, Richard A., Ed., 1991, VAX Architecture Reference    *
 *                 Manual, Second Edition, Digital Press                      *
 *              Sites, Richard L., and Witek, Richard T., 1995, Alpha AXP     *
 *                 Architecture Reference Manual, Second Edition, Digital     *
 *                 Press                                                      *
 *                                                                            *
 * Author:      Lawrence M. Baker                                             *
 *              U.S. Geological Survey                                        *
 *              345 Middlefield Road  MS977                                   *
 *              Menlo Park, CA  94025                                         *
 *              baker@usgs.gov                                                *
 *                                                                            *
 * Citation:    Baker, L.M., 2005, libvaxdata: VAX Data Format Conversion     *
 *                 Routines: U.S. Geological Survey Open-File Report 2005-    *
 *                 1424 (http://pubs.usgs.gov/of/2005/1424/).                 *
 *                                                                            *
 *                                                                            *
 *                                 Disclaimer                                 *
 *                                                                            *
 * Although this program has been used by the USGS, no warranty, expressed or *
 * implied, is made by the USGS or the United States  Government  as  to  the *
 * accuracy  and functioning of the program and related program material, nor *
 * shall the fact of  distribution  constitute  any  such  warranty,  and  no *
 * responsibility is assumed by the USGS in connection therewith.             *
 *                                                                            *
 *                                                                            *
 * Modification History:                                                      *
 *                                                                            *
 *  8-Sep-1992  L. M. Baker      Original version.                            *
 * 12-Jan-1993  L. M. Baker      Convert Fortran data conversion routines to  *
 *                                  C.                                        *
 *                               Force underflows to 0 (as the VAX hardware   *
 *                                  does) to avoid IEEE Not-a-Numbers (NaNs). *
 * 14-Jan-1993  L. M. Baker      Define forward and backward conversions for  *
 *                                  all numeric data types (not all implemen- *
 *                                  ted yet).                                 *
 * 20-Jan-1993  L. M. Baker      Convert VAX extrema to subnormal/infinities. *
 * 22-Jan-1993  L. M. Baker      Allow for little-endian and big-endian       *
 *                                  machines.                                 *
 *                               Define register variables for Microsoft C (2 *
 *                                  max).                                     *
 * 25-Jan-1993  L. M. Baker      Provide for Fortran naming with underscores. *
 *                               Provide for separate compilation for librar- *
 *                                  ies.                                      *
 * 27-Jan-1993  L. M. Baker      Swap (16-bit) words in floating-point for-   *
 *                                  mats for little-endian machines.          *
 * 16-May-2000  L. M. Baker      Add conditionals for DEC C, Microsoft Visual *
 *                                  C++.                                      *
 *                               Convert VAX dirty zero (s=e=0, m<>0) to true *
 *                                  zero.                                     *
 *                               raise( SIGFPE ) for VAX reserved operands.   *
 *                               Implement to_vax_r4(), to_vax_d8(),          *
 *                                  to_vax_g8().                              *
 *                               Add const specifier where appropriate.       *
 *  1-Feb-2001  L. M. Baker      Change long to int (long's are 64 bits on    *
 *                                  Compaq's Tru64 UNIX).                     *
 *                               Add __alpha to the list of predefined macros *
 *                                  that set IS_LITTLE_ENDIAN to 1.           *
 *  5-Feb-2001  L. M. Baker      Add prototypes for all functions (MatLab's   *
 *                                  MrC command on Macintosh requires them).  *
 *  9-Mar-2001  L. M. Baker      #include "convert_vax_data.h".               *
 * 12-Aug-2005  L. M. Baker      Add conditionals for GNU C and Portland      *
 *                                  Group C on AMD Opteron/Intel EM64T 64-bit *
 *                                  x86 machines.                             *
 * 16-Sep-2005  L. M. Baker      IEEE X_floating exponent is 15 bits, not 11  *
 *                                  bits (fix from_vax_h16()).                *
 *                               Correct recognition of +-0, +-Inf and +-NaN  *
 *                                  in to_vax_d8(), to_vax_g8().              *
 *                               Implement to_vax_h16().                      *
 * 17-Sep-2005  L. M. Baker      Add macro definitions for every float type.  *
 * 19-Sep-2005  L. M. Baker      Add fixups for IEEE-to-VAX conversion        *
 *                                  faults (+-infinity, +-NaN, overflow).     *
 * 22-Sep-2005  L. M. Baker      Change LITTLE_ENDIAN to IS_LITTLE_ENDIAN to  *
 *                                  avoid conflict with GCC/BSD predefined    *
 *                                  macro named LITTLE_ENDIAN.                *
 * 12-Oct-2005  L. M. Baker      Remove unreferenced variables.               *
 *  8-Nov-2005  L. M. Baker      Move #define const if not __STDC__ to        *
 *                                  convert_vax_data.h.                       *
 * 28-Jan-2010  L. M. Baker      Correct output order for d8/g8/h16 conver-   *
 *                                  sions on little endian machines (thanks   *
 *                                  to Neil Six at Raytheon).                 *
 *                               Correct typo (VAX_D_EXPONENT_BIAS should be  *
 *                                  VAX_G_EXPONENT_BIAS) in to_vax_g8().      *
 *                               Correct exponent positioning in to_vax_d8(). *
 * 15-Apr-2010  L. M. Baker      Correct f4/g8/h16 conversions to IEEE sub-   *
 *                                  normal form (thanks again to Neil Six).   *
 *                                                                            *
 ******************************************************************************/

#include "convert_vax_data.h"	/* UPCASE, APPEND_UNDERSCORE, FORTRAN_LINKAGE */

#ifndef IS_LITTLE_ENDIAN

/* VAX C, GNU C on a VAX or an Alpha, or DEC C */

#if defined( vax ) || defined( __vax ) || defined( vms ) || \
    defined( __vms ) || defined( __alpha )
#define IS_LITTLE_ENDIAN 1
#endif

/* Microsoft 80x86 C or Microsoft Visual C++ on an 80x86 or an Alpha */

#if defined( M_I86 ) || defined( _M_IX86 ) || defined( __M_ALPHA )
#define IS_LITTLE_ENDIAN 1
#endif

/* Sun C, GNU C, or Intel C on an 80x86 */

#if defined( i386 ) || defined( __i386 )
#define IS_LITTLE_ENDIAN 1
#endif

/* GNU C or Portland Group C on an AMD Opteron or Intel EM64T */

#if defined( __x86_64 ) || defined( __x86_64__ )
#define IS_LITTLE_ENDIAN 1
#endif

/* Otherwise, assume big-endian machine */

#ifndef IS_LITTLE_ENDIAN
#define IS_LITTLE_ENDIAN 0
#endif

#endif

#if !defined( MAKE_FROM_VAX_I2  ) && !defined( MAKE_FROM_VAX_I4  ) && \
    !defined( MAKE_FROM_VAX_R4  ) && !defined( MAKE_FROM_VAX_D8  ) && \
    !defined( MAKE_FROM_VAX_G8  ) && !defined( MAKE_FROM_VAX_H16 ) && \
    !defined( MAKE_TO_VAX_I2    ) && !defined( MAKE_TO_VAX_I4    ) && \
    !defined( MAKE_TO_VAX_R4    ) && !defined( MAKE_TO_VAX_D8    ) && \
    !defined( MAKE_TO_VAX_G8    ) && !defined( MAKE_TO_VAX_H16   )

#define MAKE_FROM_VAX_I2
#define MAKE_FROM_VAX_I4
#define MAKE_FROM_VAX_R4
#define MAKE_FROM_VAX_D8
#define MAKE_FROM_VAX_G8
#define MAKE_FROM_VAX_H16

#define MAKE_TO_VAX_I2
#define MAKE_TO_VAX_I4
#define MAKE_TO_VAX_R4
#define MAKE_TO_VAX_D8
#define MAKE_TO_VAX_G8
#define MAKE_TO_VAX_H16

#endif

#include <limits.h>             /* UCHAR_MAX, USHRT_MAX, UINT_MAX */

#if UCHAR_MAX != 255U || USHRT_MAX != 65535U || UINT_MAX != 4294967295U
#error convert_vax_data.c requires 8-bit chars, 16-bit shorts, and 32-bit ints
#endif

#if defined( MAKE_FROM_VAX_R4  ) || defined( MAKE_FROM_VAX_D8  ) || \
    defined( MAKE_FROM_VAX_G8  ) || defined( MAKE_FROM_VAX_H16 ) || \
    defined( MAKE_TO_VAX_R4    ) || defined( MAKE_TO_VAX_D8    ) || \
    defined( MAKE_TO_VAX_G8    ) || defined( MAKE_TO_VAX_H16   )
#include <signal.h>             /* SIGFPE, raise() */
#endif

/* Floating point data format invariants */

#define SIGN_BIT             0x80000000

/* VAX floating point data formats (see VAX Architecture Reference Manual) */

#define VAX_F_SIGN_BIT       SIGN_BIT
#define VAX_F_EXPONENT_MASK  0x7F800000
#define VAX_F_EXPONENT_SIZE  8
#define VAX_F_EXPONENT_BIAS  ( 1 << ( VAX_F_EXPONENT_SIZE - 1 ) )
#define VAX_F_MANTISSA_MASK  0x007FFFFF
#define VAX_F_MANTISSA_SIZE  23
#define VAX_F_HIDDEN_BIT     ( 1 << VAX_F_MANTISSA_SIZE )

#define VAX_D_SIGN_BIT       SIGN_BIT
#define VAX_D_EXPONENT_MASK  0x7F800000
#define VAX_D_EXPONENT_SIZE  8
#define VAX_D_EXPONENT_BIAS  ( 1 << ( VAX_D_EXPONENT_SIZE - 1 ) )
#define VAX_D_MANTISSA_MASK  0x007FFFFF
#define VAX_D_MANTISSA_SIZE  23
#define VAX_D_HIDDEN_BIT     ( 1 << VAX_D_MANTISSA_SIZE )

#define VAX_G_SIGN_BIT       SIGN_BIT
#define VAX_G_EXPONENT_MASK  0x7FF00000
#define VAX_G_EXPONENT_SIZE  11
#define VAX_G_EXPONENT_BIAS  ( 1 << ( VAX_G_EXPONENT_SIZE - 1 ) )
#define VAX_G_MANTISSA_MASK  0x000FFFFF
#define VAX_G_MANTISSA_SIZE  20
#define VAX_G_HIDDEN_BIT     ( 1 << VAX_G_MANTISSA_SIZE )

#define VAX_H_SIGN_BIT       SIGN_BIT
#define VAX_H_EXPONENT_MASK  0x7FFF0000
#define VAX_H_EXPONENT_SIZE  15
#define VAX_H_EXPONENT_BIAS  ( 1 << ( VAX_H_EXPONENT_SIZE - 1 ) )
#define VAX_H_MANTISSA_MASK  0x0000FFFF
#define VAX_H_MANTISSA_SIZE  16
#define VAX_H_HIDDEN_BIT     ( 1 << VAX_H_MANTISSA_SIZE )

/* IEEE floating point data formats (see Alpha Architecture Reference Manual) */

#define IEEE_S_SIGN_BIT      SIGN_BIT
#define IEEE_S_EXPONENT_MASK 0x7F800000
#define IEEE_S_EXPONENT_SIZE 8
#define IEEE_S_EXPONENT_BIAS ( ( 1 << ( IEEE_S_EXPONENT_SIZE - 1 ) ) - 1 )
#define IEEE_S_MANTISSA_MASK 0x007FFFFF
#define IEEE_S_MANTISSA_SIZE 23
#define IEEE_S_HIDDEN_BIT    ( 1 << IEEE_S_MANTISSA_SIZE )

#define IEEE_T_SIGN_BIT      SIGN_BIT
#define IEEE_T_EXPONENT_MASK 0x7FF00000
#define IEEE_T_EXPONENT_SIZE 11
#define IEEE_T_EXPONENT_BIAS ( ( 1 << ( IEEE_T_EXPONENT_SIZE - 1 ) ) - 1 )
#define IEEE_T_MANTISSA_MASK 0x000FFFFF
#define IEEE_T_MANTISSA_SIZE 20
#define IEEE_T_HIDDEN_BIT    ( 1 << IEEE_T_MANTISSA_SIZE )

#define IEEE_X_SIGN_BIT      SIGN_BIT
#define IEEE_X_EXPONENT_MASK 0x7FFF0000
#define IEEE_X_EXPONENT_SIZE 15
#define IEEE_X_EXPONENT_BIAS ( ( 1 << ( IEEE_X_EXPONENT_SIZE - 1 ) ) - 1 )
#define IEEE_X_MANTISSA_MASK 0x0000FFFF
#define IEEE_X_MANTISSA_SIZE 16
#define IEEE_X_HIDDEN_BIT    ( 1 << IEEE_X_MANTISSA_SIZE )

/************************************************************** from_vax_i2() */

#ifdef MAKE_FROM_VAX_I2

void FORTRAN_LINKAGE from_vax_i2( const void *inbuf, void *outbuf,
                                  const int *count ) {

#if IS_LITTLE_ENDIAN

   register const unsigned short *in;   /* Microsoft C: up to 2 register vars */
   register unsigned short *out;        /* Microsoft C: up to 2 register vars */
   int n;


   in  = (const unsigned short *) inbuf;
   out = (unsigned short *) outbuf;

   if ( in != out ) {
      for ( n = *count; n > 0; n-- ) {
         *out++ = *in++;
      }
   }

#else

   const unsigned char *in;
   unsigned char *out;
   int n;
   unsigned char c1;


   in  = (const unsigned char *) inbuf;
   out = (unsigned char *) outbuf;

   for ( n = *count; n > 0; n-- ) {
      c1     = *in++;
      *out++ = *in++;
      *out++ = c1;
   }

#endif

}

#endif /* #ifdef MAKE_FROM_VAX_I2 */

/************************************************************** from_vax_i4() */

#ifdef MAKE_FROM_VAX_I4

void FORTRAN_LINKAGE from_vax_i4( const void *inbuf, void *outbuf,
                                  const int *count ) {

#if IS_LITTLE_ENDIAN

   register const unsigned int *in;     /* Microsoft C: up to 2 register vars */
   register unsigned int *out;          /* Microsoft C: up to 2 register vars */
   int n;


   in  = (const unsigned int *) inbuf;
   out = (unsigned int *) outbuf;

   if ( in != out ) {
      for ( n = *count; n > 0; n-- ) {
         *out++ = *in++;
      }
   }

#else

   const unsigned char *in;
   unsigned char *out;
   int n;
   unsigned char c1, c2, c3;


   in  = (unsigned char *) inbuf;
   out = (unsigned char *) outbuf;

   for ( n = *count; n > 0; n-- ) {
      c1     = *in++;
      c2     = *in++;
      c3     = *in++;
      *out++ = *in++;
      *out++ = c3;
      *out++ = c2;
      *out++ = c1;
   }

#endif

}

#endif /* #ifdef MAKE_FROM_VAX_I4 */

/************************************************************** from_vax_r4() */

#ifdef MAKE_FROM_VAX_R4

/* Assert the mantissa in a VAX F_float is the same as in an IEEE S_float     */
#if VAX_F_MANTISSA_MASK != IEEE_S_MANTISSA_MASK
#error MANTISSA_MASK mismatch in from_vax_r4()
#endif
#define MANTISSA_MASK VAX_F_MANTISSA_MASK
/* If  the mantissas are the same, then so are the no. of bits and the hidden */
/* normalization bit                                                          */
#define MANTISSA_SIZE VAX_F_MANTISSA_SIZE
#define HIDDEN_BIT    VAX_F_HIDDEN_BIT

/* Assert  the  VAX  F_float  exponent  bias is greater than the IEEE S_float */
/* exponent bias (overflow is not possible)                                   */
#define EXPONENT_ADJUSTMENT ( 1 + VAX_F_EXPONENT_BIAS - IEEE_S_EXPONENT_BIAS )
#if EXPONENT_ADJUSTMENT < 2
#error EXPONENT_ADJUSTMENT assertion failure in from_vax_r4()
#endif
#define IN_PLACE_EXPONENT_ADJUSTMENT \
           ( EXPONENT_ADJUSTMENT << IEEE_S_MANTISSA_SIZE )

void FORTRAN_LINKAGE from_vax_r4( const void *inbuf, void *outbuf,
                                  const int *count ) {

#if IS_LITTLE_ENDIAN
   register const unsigned short *in;   /* Microsoft C: up to 2 register vars */
   union { unsigned short i[2]; unsigned int l; } vaxpart;
#else
   const unsigned char *in;
   union { unsigned char c[4]; unsigned int l; } vaxpart;
#endif
   register unsigned int *out;          /* Microsoft C: up to 2 register vars */
   unsigned int vaxpart1;
   int n;
   int e;


#if IS_LITTLE_ENDIAN
   in  = (const unsigned short *) inbuf;
#else
   in  = (const unsigned char *) inbuf;
#endif
   out = (unsigned int *) outbuf;

   for ( n = *count; n > 0; n-- ) {

#if IS_LITTLE_ENDIAN
      vaxpart.i[1] = *in++;
      vaxpart.i[0] = *in++;
      vaxpart1     = vaxpart.l;
#else
      vaxpart.c[1] = *in++;
      vaxpart.c[0] = *in++;
      vaxpart.c[3] = *in++;
      vaxpart.c[2] = *in++;
      vaxpart1     = vaxpart.l;
#endif

      if ( ( e = ( vaxpart1 & VAX_F_EXPONENT_MASK ) ) == 0 ) {
                                  /* If the biased VAX exponent is zero [e=0] */

         if ( ( vaxpart1 & SIGN_BIT ) == SIGN_BIT ) {    /* If negative [s=1] */
            raise( SIGFPE );/* VAX reserved operand fault; fixup to IEEE zero */
         }
           /* Set VAX dirty [m<>0] or true [m=0] zero to IEEE +zero [s=e=m=0] */
         *out++ = 0;

      } else {                  /* The biased VAX exponent is non-zero [e<>0] */

         e >>= MANTISSA_SIZE;               /* Obtain the biased VAX exponent */

         /* The  biased  VAX  exponent  has to be adjusted to account for the */
         /* right shift of the IEEE mantissa binary point and the  difference */
         /* between  the biases in their "excess n" exponent representations. */
         /* If the resulting biased IEEE exponent is less than  or  equal  to */
         /* zero, the converted IEEE S_float must use subnormal form.         */

         if ( ( e -= EXPONENT_ADJUSTMENT ) > 0 ) {
                                            /* Use IEEE normalized form [e>0] */

            /* Both mantissas are 23 bits; adjust the exponent field in place */
            *out++ = vaxpart1 - IN_PLACE_EXPONENT_ADJUSTMENT;

         } else {                       /* Use IEEE subnormal form [e=0, m>0] */

            /* In IEEE subnormal form, even though the biased exponent is 0   */
            /* [e=0], the effective biased exponent is 1.  The mantissa must  */
            /* be shifted right by the number of bits, n, required to adjust  */
            /* the biased exponent from its current value, e, to 1.  I.e.,    */
            /* e + n = 1, thus n = 1 - e.  n is guaranteed to be at least 1   */
            /* [e<=0], which guarantees that the hidden 1.m bit from the ori- */
            /* ginal mantissa will become visible, and the resulting subnor-  */
            /* mal mantissa will correctly be of the form 0.m.                */

            *out++ = ( vaxpart1 & SIGN_BIT ) |
                     ( ( HIDDEN_BIT | ( vaxpart1 & MANTISSA_MASK ) ) >>
                       ( 1 - e ) );

         }
      }
   }

}

#undef MANTISSA_MASK
#undef MANTISSA_SIZE
#undef HIDDEN_BIT
#undef EXPONENT_ADJUSTMENT
#undef IN_PLACE_EXPONENT_ADJUSTMENT

#endif /* #ifdef MAKE_FROM_VAX_R4 */

/************************************************************** from_vax_d8() */

#ifdef MAKE_FROM_VAX_D8

/* Assert  the  IEEE  T_float  exponent  bias  is so much larger than the VAX */
/* D_float exponent bias that it  is  not possible  for  this  conversion  to */
/* overflow or underflow                                                      */
#define EXPONENT_ADJUSTMENT ( 1 + VAX_D_EXPONENT_BIAS - IEEE_T_EXPONENT_BIAS )
#if ( EXPONENT_ADJUSTMENT + VAX_D_MANTISSA_SIZE + 1 ) > 0
#error EXPONENT_ADJUSTMENT assertion failure in from_vax_d8()
#endif
#define IN_PLACE_EXPONENT_ADJUSTMENT \
           ( EXPONENT_ADJUSTMENT << IEEE_T_MANTISSA_SIZE )

/* Assert the VAX D_float mantissa is wider than the IEEE T_float mantissa    */
#define EXPONENT_RIGHT_SHIFT ( VAX_D_MANTISSA_SIZE - IEEE_T_MANTISSA_SIZE )
#if EXPONENT_RIGHT_SHIFT <= 0
#error EXPONENT_RIGHT_SHIFT assertion failure in from_vax_d8()
#endif

void FORTRAN_LINKAGE from_vax_d8( const void *inbuf, void *outbuf,
                                  const int *count ) {

#if IS_LITTLE_ENDIAN
   register const unsigned short *in;   /* Microsoft C: up to 2 register vars */
   union { unsigned short i[2]; unsigned int l; } vaxpart;
#else
   const unsigned char *in;
   union { unsigned char c[4]; unsigned int l; } vaxpart;
#endif
   register unsigned int *out;          /* Microsoft C: up to 2 register vars */
   unsigned int vaxpart1, vaxpart2, ieeepart1, ieeepart2;
   int n;


#if IS_LITTLE_ENDIAN
   in  = (const unsigned short *) inbuf;
#else
   in  = (const unsigned char *) inbuf;
#endif
   out = (unsigned int *) outbuf;

   for ( n = *count; n > 0; n-- ) {

#if IS_LITTLE_ENDIAN
      vaxpart.i[1] = *in++;
      vaxpart.i[0] = *in++;
      vaxpart1     = vaxpart.l;
      vaxpart.i[1] = *in++;
      vaxpart.i[0] = *in++;
      vaxpart2     = vaxpart.l;
#else
      vaxpart.c[1] = *in++;
      vaxpart.c[0] = *in++;
      vaxpart.c[3] = *in++;
      vaxpart.c[2] = *in++;
      vaxpart1     = vaxpart.l;
      vaxpart.c[1] = *in++;
      vaxpart.c[0] = *in++;
      vaxpart.c[3] = *in++;
      vaxpart.c[2] = *in++;
      vaxpart2     = vaxpart.l;
#endif

      if ( ( vaxpart1 & VAX_D_EXPONENT_MASK ) == 0 ) {
                                  /* If the biased VAX exponent is zero [e=0] */

         if ( ( vaxpart1 & SIGN_BIT ) == SIGN_BIT ) {    /* If negative [s=1] */
            raise( SIGFPE );/* VAX reserved operand fault; fixup to IEEE zero */
         }
           /* Set VAX dirty [m<>0] or true [m=0] zero to IEEE +zero [s=e=m=0] */
         *out++ = 0;
         *out++ = 0;

      } else {                  /* The biased VAX exponent is non-zero [e<>0] */

         /* Because the range of an IEEE T_float is so much larger than a VAX */
         /* D_float, the converted IEEE T_float will never be subnormal form. */

         /* Use  IEEE normalized form [e>0]; truncate the mantissa from 55 to */
         /* 52 bits, then adjust the exponent field in place                  */
         ieeepart1 = ( ( vaxpart1 & SIGN_BIT ) |
                     ( ( vaxpart1 & ~SIGN_BIT ) >> EXPONENT_RIGHT_SHIFT ) ) -
                     IN_PLACE_EXPONENT_ADJUSTMENT;
         ieeepart2 = ( vaxpart1 << ( 32 - EXPONENT_RIGHT_SHIFT ) ) |
                     ( vaxpart2 >>        EXPONENT_RIGHT_SHIFT );

#if IS_LITTLE_ENDIAN
         *out++ = ieeepart2;
         *out++ = ieeepart1;
#else
         *out++ = ieeepart1;
         *out++ = ieeepart2;
#endif

      }
   }

}

#undef EXPONENT_ADJUSTMENT
#undef IN_PLACE_EXPONENT_ADJUSTMENT
#undef EXPONENT_RIGHT_SHIFT

#endif /* #ifdef MAKE_FROM_VAX_D8 */

/************************************************************** from_vax_g8() */

#ifdef MAKE_FROM_VAX_G8

/* Assert the mantissa in a VAX G_float is the same as in an IEEE T_float     */
#if VAX_G_MANTISSA_MASK != IEEE_T_MANTISSA_MASK
#error MANTISSA_MASK mismatch in from_vax_g8()
#endif
#define MANTISSA_MASK VAX_G_MANTISSA_MASK
/* If  the mantissas are the same, then so are the no. of bits and the hidden */
/* normalization bit                                                          */
#define MANTISSA_SIZE VAX_G_MANTISSA_SIZE
#define HIDDEN_BIT    VAX_G_HIDDEN_BIT

/* Assert  the  VAX  G_float  exponent  bias is greater than the IEEE T_float */
/* exponent bias (overflow is not possible)                                   */
#define EXPONENT_ADJUSTMENT ( 1 + VAX_G_EXPONENT_BIAS - IEEE_T_EXPONENT_BIAS )
#if EXPONENT_ADJUSTMENT < 2
#error EXPONENT_ADJUSTMENT assertion failure in from_vax_g8()
#endif
#define IN_PLACE_EXPONENT_ADJUSTMENT \
           ( EXPONENT_ADJUSTMENT << IEEE_T_MANTISSA_SIZE )

void FORTRAN_LINKAGE from_vax_g8( const void *inbuf, void *outbuf,
                                  const int *count ) {

#if IS_LITTLE_ENDIAN
   register const unsigned short *in;   /* Microsoft C: up to 2 register vars */
   union { unsigned short i[2]; unsigned int l; } vaxpart;
#else
   const unsigned char *in;
   union { unsigned char c[4]; unsigned int l; } vaxpart;
#endif
   register unsigned int *out;          /* Microsoft C: up to 2 register vars */
   unsigned int vaxpart1, vaxpart2, ieeepart1, ieeepart2;
   int n;
   int e;


#if IS_LITTLE_ENDIAN
   in  = (const unsigned short *) inbuf;
#else
   in  = (const unsigned char *) inbuf;
#endif
   out = (unsigned int *) outbuf;

   for ( n = *count; n > 0; n-- ) {

#if IS_LITTLE_ENDIAN
      vaxpart.i[1] = *in++;
      vaxpart.i[0] = *in++;
      vaxpart1     = vaxpart.l;
      vaxpart.i[1] = *in++;
      vaxpart.i[0] = *in++;
      vaxpart2     = vaxpart.l;
#else
      vaxpart.c[1] = *in++;
      vaxpart.c[0] = *in++;
      vaxpart.c[3] = *in++;
      vaxpart.c[2] = *in++;
      vaxpart1     = vaxpart.l;
      vaxpart.c[1] = *in++;
      vaxpart.c[0] = *in++;
      vaxpart.c[3] = *in++;
      vaxpart.c[2] = *in++;
      vaxpart2     = vaxpart.l;
#endif

      if ( ( e = ( vaxpart1 & VAX_G_EXPONENT_MASK ) ) == 0 ) {
                                  /* If the biased VAX exponent is zero [e=0] */

         if ( ( vaxpart1 & SIGN_BIT ) == SIGN_BIT ) {    /* If negative [s=1] */
            raise( SIGFPE );/* VAX reserved operand fault; fixup to IEEE zero */
         }
           /* Set VAX dirty [m<>0] or true [m=0] zero to IEEE +zero [s=e=m=0] */
         *out++ = 0;
         *out++ = 0;

      } else {                  /* The biased VAX exponent is non-zero [e<>0] */

         e >>= MANTISSA_SIZE;               /* Obtain the biased VAX exponent */
         
         /* The  biased  VAX  exponent  has to be adjusted to account for the */
         /* right shift of the IEEE mantissa binary point and the  difference */
         /* between  the biases in their "excess n" exponent representations. */
         /* If the resulting biased IEEE exponent is less than  or  equal  to */
         /* zero, the converted IEEE T_float must use subnormal form.         */

         if ( ( e -= EXPONENT_ADJUSTMENT ) > 0 ) {
                                            /* Use IEEE normalized form [e>0] */

            /* Both mantissas are 52 bits; adjust the exponent field in place */
            ieeepart1 = vaxpart1 - IN_PLACE_EXPONENT_ADJUSTMENT;
            ieeepart2 = vaxpart2;

         } else {                       /* Use IEEE subnormal form [e=0, m>0] */

            /* In IEEE subnormal form, even though the biased exponent is 0   */
            /* [e=0], the effective biased exponent is 1.  The mantissa must  */
            /* be shifted right by the number of bits, n, required to adjust  */
            /* the biased exponent from its current value, e, to 1.  I.e.,    */
            /* e + n = 1, thus n = 1 - e.  n is guaranteed to be at least 1   */
            /* [e<=0], which guarantees that the hidden 1.m bit from the ori- */
            /* ginal mantissa will become visible, and the resulting subnor-  */
            /* mal mantissa will correctly be of the form 0.m.                */

            vaxpart1  = ( vaxpart1 & ( SIGN_BIT | MANTISSA_MASK ) ) |
                        HIDDEN_BIT;
            ieeepart1 = ( vaxpart1 & SIGN_BIT ) |
                        ( ( vaxpart1 & ( HIDDEN_BIT | MANTISSA_MASK ) ) >>
                          ( 1 - e ) );
            ieeepart2 = ( vaxpart1 << ( 31 + e ) ) | ( vaxpart2 >> ( 1 - e ) );

         }

#if IS_LITTLE_ENDIAN
         *out++ = ieeepart2;
         *out++ = ieeepart1;
#else
         *out++ = ieeepart1;
         *out++ = ieeepart2;
#endif

      }
   }

}

#undef MANTISSA_MASK
#undef MANTISSA_SIZE
#undef HIDDEN_BIT
#undef EXPONENT_ADJUSTMENT
#undef IN_PLACE_EXPONENT_ADJUSTMENT

#endif /* #ifdef MAKE_FROM_VAX_G8 */

/************************************************************* from_vax_h16() */

#ifdef MAKE_FROM_VAX_H16

/* Assert the mantissa in a VAX H_float is the same as in an IEEE X_float     */
#if VAX_H_MANTISSA_MASK != IEEE_X_MANTISSA_MASK
#error MANTISSA_MASK mismatch in from_vax_h16()
#endif
#define MANTISSA_MASK VAX_H_MANTISSA_MASK
/* If  the mantissas are the same, then so are the no. of bits and the hidden */
/* normalization bit                                                          */
#define MANTISSA_SIZE VAX_H_MANTISSA_SIZE
#define HIDDEN_BIT    VAX_H_HIDDEN_BIT

/* Assert  the  VAX  H_float  exponent  bias is greater than the IEEE X_float */
/* exponent bias (overflow is not possible)                                   */
#define EXPONENT_ADJUSTMENT ( 1 + VAX_H_EXPONENT_BIAS - IEEE_X_EXPONENT_BIAS )
#if EXPONENT_ADJUSTMENT < 2
#error EXPONENT_ADJUSTMENT assertion failure in from_vax_H16()
#endif
#define IN_PLACE_EXPONENT_ADJUSTMENT \
           ( EXPONENT_ADJUSTMENT << IEEE_X_MANTISSA_SIZE )

void FORTRAN_LINKAGE from_vax_h16( const void *inbuf, void *outbuf,
                                   const int *count ) {

#if IS_LITTLE_ENDIAN
   register const unsigned short *in;   /* Microsoft C: up to 2 register vars */
   union { unsigned short i[2]; unsigned int l; } vaxpart;
#else
   const unsigned char *in;
   union { unsigned char c[4]; unsigned int l; } vaxpart;
#endif
   register unsigned int *out;          /* Microsoft C: up to 2 register vars */
   unsigned int vaxpart1, vaxpart2, vaxpart3, vaxpart4,
                ieeepart1, ieeepart2, ieeepart3, ieeepart4;
   int n;
   int e;


#if IS_LITTLE_ENDIAN
   in  = (const unsigned short *) inbuf;
#else
   in  = (const unsigned char *) inbuf;
#endif
   out = (unsigned int *) outbuf;

   for ( n = *count; n > 0; n-- ) {

#if IS_LITTLE_ENDIAN
      vaxpart.i[1] = *in++;
      vaxpart.i[0] = *in++;
      vaxpart1     = vaxpart.l;
      vaxpart.i[1] = *in++;
      vaxpart.i[0] = *in++;
      vaxpart2     = vaxpart.l;
      vaxpart.i[1] = *in++;
      vaxpart.i[0] = *in++;
      vaxpart3     = vaxpart.l;
      vaxpart.i[1] = *in++;
      vaxpart.i[0] = *in++;
      vaxpart4     = vaxpart.l;
#else
      vaxpart.c[1] = *in++;
      vaxpart.c[0] = *in++;
      vaxpart.c[3] = *in++;
      vaxpart.c[2] = *in++;
      vaxpart1     = vaxpart.l;
      vaxpart.c[1] = *in++;
      vaxpart.c[0] = *in++;
      vaxpart.c[3] = *in++;
      vaxpart.c[2] = *in++;
      vaxpart2     = vaxpart.l;
      vaxpart.c[1] = *in++;
      vaxpart.c[0] = *in++;
      vaxpart.c[3] = *in++;
      vaxpart.c[2] = *in++;
      vaxpart3     = vaxpart.l;
      vaxpart.c[1] = *in++;
      vaxpart.c[0] = *in++;
      vaxpart.c[3] = *in++;
      vaxpart.c[2] = *in++;
      vaxpart4     = vaxpart.l;
#endif

      if ( ( e = ( vaxpart1 & VAX_H_EXPONENT_MASK ) ) == 0 ) {
                                  /* If the biased VAX exponent is zero [e=0] */

         if ( ( vaxpart1 & SIGN_BIT ) == SIGN_BIT ) {    /* If negative [s=1] */
            raise( SIGFPE );/* VAX reserved operand fault; fixup to IEEE zero */
         }
           /* Set VAX dirty [m<>0] or true [m=0] zero to IEEE +zero [s=e=m=0] */
         *out++ = 0;
         *out++ = 0;
         *out++ = 0;
         *out++ = 0;

      } else {                  /* The biased VAX exponent is non-zero [e<>0] */

         e >>= MANTISSA_SIZE;               /* Obtain the biased VAX exponent */

         /* The  biased  VAX  exponent  has to be adjusted to account for the */
         /* right shift of the IEEE mantissa binary point and the  difference */
         /* between  the biases in their "excess n" exponent representations. */
         /* If the resulting biased IEEE exponent is less than  or  equal  to */
         /* zero, the converted IEEE X_float must use subnormal form.         */

         if ( ( e -= EXPONENT_ADJUSTMENT ) > 0 ) {
                                            /* Use IEEE normalized form [e>0] */

           /* Both mantissas are 112 bits; adjust the exponent field in place */
            ieeepart1 = vaxpart1 - IN_PLACE_EXPONENT_ADJUSTMENT;
            ieeepart2 = vaxpart2;
            ieeepart3 = vaxpart3;
            ieeepart4 = vaxpart4;

         } else {                       /* Use IEEE subnormal form [e=0, m>0] */

            /* In IEEE subnormal form, even though the biased exponent is 0   */
            /* [e=0], the effective biased exponent is 1.  The mantissa must  */
            /* be shifted right by the number of bits, n, required to adjust  */
            /* the biased exponent from its current value, e, to 1.  I.e.,    */
            /* e + n = 1, thus n = 1 - e.  n is guaranteed to be at least 1   */
            /* [e<=0], which guarantees that the hidden 1.m bit from the ori- */
            /* ginal mantissa will become visible, and the resulting subnor-  */
            /* mal mantissa will correctly be of the form 0.m.                */

            vaxpart1  = ( vaxpart1 & ( SIGN_BIT | MANTISSA_MASK ) ) |
                        HIDDEN_BIT;
            ieeepart1 = ( vaxpart1 & SIGN_BIT ) |
                        ( ( vaxpart1 & ( HIDDEN_BIT | MANTISSA_MASK ) ) >>
                          ( 1 - e ) );
            ieeepart2 = ( vaxpart1 << ( 31 + e ) ) | ( vaxpart2 >> ( 1 - e ) );
            ieeepart3 = ( vaxpart2 << ( 31 + e ) ) | ( vaxpart3 >> ( 1 - e ) );
            ieeepart4 = ( vaxpart3 << ( 31 + e ) ) | ( vaxpart4 >> ( 1 - e ) );

         }

#if IS_LITTLE_ENDIAN
         *out++ = ieeepart4;
         *out++ = ieeepart3;
         *out++ = ieeepart2;
         *out++ = ieeepart1;
#else
         *out++ = ieeepart1;
         *out++ = ieeepart2;
         *out++ = ieeepart3;
         *out++ = ieeepart4;
#endif

      }
   }

}

#undef MANTISSA_MASK
#undef MANTISSA_SIZE
#undef HIDDEN_BIT
#undef EXPONENT_ADJUSTMENT
#undef IN_PLACE_EXPONENT_ADJUSTMENT

#endif /* #ifdef MAKE_FROM_VAX_H16 */

/**************************************************************** to_vax_i2() */

#ifdef MAKE_TO_VAX_I2

void FORTRAN_LINKAGE to_vax_i2( const void *inbuf, void *outbuf,
                                const int *count ) {

#if IS_LITTLE_ENDIAN

   register const unsigned short *in;   /* Microsoft C: up to 2 register vars */
   register unsigned short *out;        /* Microsoft C: up to 2 register vars */
   int n;


   in  = (const unsigned short *) inbuf;
   out = (unsigned short *) outbuf;

   if ( in != out ) {
      for ( n = *count; n > 0; n-- ) {
         *out++ = *in++;
      }
   }

#else

   const unsigned char *in;
   unsigned char *out;
   int n;
   unsigned char c1;


   in  = (const unsigned char *) inbuf;
   out = (unsigned char *) outbuf;

   for ( n = *count; n > 0; n-- ) {
      c1     = *in++;
      *out++ = *in++;
      *out++ = c1;
   }

#endif

}

#endif /* #ifdef MAKE_TO_VAX_I2 */

/**************************************************************** to_vax_i4() */

#ifdef MAKE_TO_VAX_I4

void FORTRAN_LINKAGE to_vax_i4( const void *inbuf, void *outbuf,
                                const int *count ) {

#if IS_LITTLE_ENDIAN

   register const unsigned int *in;     /* Microsoft C: up to 2 register vars */
   register unsigned int *out;          /* Microsoft C: up to 2 register vars */
   int n;


   in  = (const unsigned int *) inbuf;
   out = (unsigned int *) outbuf;

   if ( in != out ) {
      for ( n = *count; n > 0; n-- ) {
         *out++ = *in++;
      }
   }

#else

   const unsigned char *in;
   unsigned char *out;
   int n;
   unsigned char c1, c2, c3;


   in  = (const unsigned char *) inbuf;
   out = (unsigned char *) outbuf;

   for ( n = *count; n > 0; n-- ) {
      c1     = *in++;
      c2     = *in++;
      c3     = *in++;
      *out++ = *in++;
      *out++ = c3;
      *out++ = c2;
      *out++ = c1;
   }

#endif

}

#endif /* #ifdef MAKE_TO_VAX_I4 */

/**************************************************************** to_vax_r4() */

#ifdef MAKE_TO_VAX_R4

/* Assert the mantissa in a VAX F_float is the same as in an IEEE S_float     */
#if VAX_F_MANTISSA_MASK != IEEE_S_MANTISSA_MASK
#error MANTISSA_MASK mismatch in to_vax_r4()
#endif
#define MANTISSA_MASK VAX_F_MANTISSA_MASK
/* If  the mantissas are the same, then so are the no. of bits and the hidden */
/* normalization bit                                                          */
#define MANTISSA_SIZE VAX_F_MANTISSA_SIZE
#define HIDDEN_BIT    VAX_F_HIDDEN_BIT

#define EXPONENT_ADJUSTMENT ( 1 + VAX_F_EXPONENT_BIAS - IEEE_S_EXPONENT_BIAS )

void FORTRAN_LINKAGE to_vax_r4( const void *inbuf, void *outbuf,
                                const int *count ) {

   register const unsigned int *in;     /* Microsoft C: up to 2 register vars */
#if IS_LITTLE_ENDIAN
   register unsigned short *out;        /* Microsoft C: up to 2 register vars */
   union { unsigned short i[2]; unsigned int l; } vaxpart;
#else
   unsigned char *out;
   union { unsigned char c[4]; unsigned int l; } vaxpart;
#endif
   unsigned int ieeepart1;
   unsigned int m;
   int n;
   int e;


   in = (const unsigned int *) inbuf;
#if IS_LITTLE_ENDIAN
   out = (unsigned short *) outbuf;
#else
   out = (unsigned char *) outbuf;
#endif

   for ( n = *count; n > 0; n-- ) {
      ieeepart1 = *in++;
      if ( ( ieeepart1 & ~SIGN_BIT ) == 0 ) {

         vaxpart.l = 0;      /* Set IEEE +-zero [e=m=0] to VAX zero [s=e=m=0] */

      } else if ( ( e = ( ieeepart1 & IEEE_S_EXPONENT_MASK ) ) ==
                  IEEE_S_EXPONENT_MASK ) {

       /* VAX's have no equivalents for IEEE +-Infinity and +-NaN [e=all-1's] */
         raise( SIGFPE );
               /* Fixup to VAX +-extrema [e=all-1's] with zero mantissa [m=0] */
         vaxpart.l = ( ieeepart1 & SIGN_BIT ) | VAX_F_EXPONENT_MASK;

      } else {

         e >>= MANTISSA_SIZE;              /* Obtain the biased IEEE exponent */
         m = ieeepart1 & MANTISSA_MASK;           /* Obtain the IEEE mantissa */

         if ( e == 0 ) {                          /* Denormalized [e=0, m<>0] */
            m <<= 1; /* Adjust representation from 2**(1-bias) to 2**(e-bias) */
            while ( ( m & HIDDEN_BIT ) == 0 ) {
               m <<= 1;
               e  -= 1;                                    /* Adjust exponent */
            }
            m &= MANTISSA_MASK;         /* Adjust mantissa to hidden-bit form */
         }

         if ( ( e += EXPONENT_ADJUSTMENT ) <= 0 ) {

            vaxpart.l = 0;                                /* Silent underflow */

         } else if ( e > ( 2 * VAX_F_EXPONENT_BIAS - 1 ) ) {

            raise( SIGFPE );/* Overflow; fixup to VAX +-extrema [e=m=all-1's] */
            vaxpart.l = ( ieeepart1 & SIGN_BIT ) | ~SIGN_BIT;

         } else {

                    /* VAX normalized form [e>0] (both mantissas are 23 bits) */
            vaxpart.l = ( ieeepart1 & SIGN_BIT ) | ( e << MANTISSA_SIZE ) | m;

         }

      }

#if IS_LITTLE_ENDIAN
      *out++ = vaxpart.i[1];
      *out++ = vaxpart.i[0];
#else
      *out++ = vaxpart.c[1];
      *out++ = vaxpart.c[0];
      *out++ = vaxpart.c[3];
      *out++ = vaxpart.c[2];
#endif

   }

}

#undef MANTISSA_MASK
#undef MANTISSA_SIZE
#undef HIDDEN_BIT
#undef EXPONENT_ADJUSTMENT

#endif /* #ifdef MAKE_TO_VAX_R4 */

/**************************************************************** to_vax_d8() */

#ifdef MAKE_TO_VAX_D8

#define EXPONENT_ADJUSTMENT ( 1 + VAX_D_EXPONENT_BIAS - IEEE_T_EXPONENT_BIAS )

/* Assert the VAX D_float mantissa is wider than the IEEE T_float mantissa    */
#define EXPONENT_LEFT_SHIFT ( VAX_D_MANTISSA_SIZE - IEEE_T_MANTISSA_SIZE )
#if EXPONENT_LEFT_SHIFT <= 0
#error EXPONENT_LEFT_SHIFT assertion failure in to_vax_d8()
#endif

void FORTRAN_LINKAGE to_vax_d8( const void *inbuf, void *outbuf,
                                const int *count ) {

   register const unsigned int *in;     /* Microsoft C: up to 2 register vars */
#if IS_LITTLE_ENDIAN
   register unsigned short *out;        /* Microsoft C: up to 2 register vars */
   union { unsigned short i[2]; unsigned int l; } vaxpart;
#else
   unsigned char *out;
   union { unsigned char c[4]; unsigned int l; } vaxpart;
#endif
   unsigned int ieeepart1, vaxpart2;
   unsigned int m;
   int n;
   int e;


   in = (const unsigned int *) inbuf;
#if IS_LITTLE_ENDIAN
   out = (unsigned short *) outbuf;
#else
   out = (unsigned char *) outbuf;
#endif

   for ( n = *count; n > 0; n-- ) {

#if IS_LITTLE_ENDIAN
      vaxpart2  = *in++;
      ieeepart1 = *in++;
#else
      ieeepart1 = *in++;
      vaxpart2  = *in++;
#endif

      if ( ( ( ieeepart1 & ~SIGN_BIT ) | vaxpart2 ) == 0 ) {

         vaxpart.l = 0;      /* Set IEEE +-zero [e=m=0] to VAX zero [s=e=m=0] */
      /* vaxpart2  = 0; */                        /* vaxpart2 is already zero */

      } else if ( ( e = ( ieeepart1 & IEEE_T_EXPONENT_MASK ) ) ==
                  IEEE_T_EXPONENT_MASK ) {

       /* VAX's have no equivalents for IEEE +-Infinity and +-NaN [e=all-1's] */
         raise( SIGFPE );
               /* Fixup to VAX +-extrema [e=all-1's] with zero mantissa [m=0] */
         vaxpart.l = ( ieeepart1 & SIGN_BIT ) | VAX_D_EXPONENT_MASK;
         vaxpart2  = 0;
         
      } else {

         e >>= IEEE_T_MANTISSA_SIZE;       /* Obtain the biased IEEE exponent */
         m = ieeepart1 & IEEE_T_MANTISSA_MASK;    /* Obtain the IEEE mantissa */

         if ( e == 0 ) {                          /* Denormalized [e=0, m<>0] */
                     /* Adjust representation from 2**(1-bias) to 2**(e-bias) */
            m = ( m << 1 ) | ( vaxpart2 >> 31 );
            vaxpart2 <<= 1;
            while ( ( m & IEEE_T_HIDDEN_BIT ) == 0 ) {
               m = ( m << 1 ) | ( vaxpart2 >> 31 );
               vaxpart2 <<= 1;
               e         -= 1;                             /* Adjust exponent */
            }
            m &= IEEE_T_MANTISSA_MASK;  /* Adjust mantissa to hidden-bit form */
         }

         if ( ( e += EXPONENT_ADJUSTMENT ) <= 0 ) {

            vaxpart.l = 0;                                /* Silent underflow */
            vaxpart2  = 0;

         } else if ( e > ( 2 * VAX_D_EXPONENT_BIAS - 1 ) ) {

            raise( SIGFPE );/* Overflow; fixup to VAX +-extrema [e=m=all-1's] */
            vaxpart.l = ( ieeepart1 & SIGN_BIT ) | ~SIGN_BIT;
            vaxpart2  = ~0;

         } else {

           /* VAX normalized form [e>0]; zero pad mantissa from 52 to 55 bits */
            vaxpart.l = ( ieeepart1 & SIGN_BIT ) |
                        ( e        <<        VAX_D_MANTISSA_SIZE ) |
                        ( m        <<        EXPONENT_LEFT_SHIFT ) |
                        ( vaxpart2 >> ( 32 - EXPONENT_LEFT_SHIFT ) );
            vaxpart2 <<= EXPONENT_LEFT_SHIFT;

         }

      }

#if IS_LITTLE_ENDIAN
      *out++    = vaxpart.i[1];
      *out++    = vaxpart.i[0];
      vaxpart.l = vaxpart2;
      *out++    = vaxpart.i[1];
      *out++    = vaxpart.i[0];
#else
      *out++    = vaxpart.c[1];
      *out++    = vaxpart.c[0];
      *out++    = vaxpart.c[3];
      *out++    = vaxpart.c[2];
      vaxpart.l = vaxpart2;
      *out++    = vaxpart.c[1];
      *out++    = vaxpart.c[0];
      *out++    = vaxpart.c[3];
      *out++    = vaxpart.c[2];
#endif

   }

}

#undef EXPONENT_ADJUSTMENT
#undef EXPONENT_RIGHT_SHIFT

#endif /* #ifdef MAKE_TO_VAX_D8 */

/**************************************************************** to_vax_g8() */

#ifdef MAKE_TO_VAX_G8

/* Assert the mantissa in a VAX G_float is the same as in an IEEE T_float     */
#if VAX_G_MANTISSA_MASK != IEEE_T_MANTISSA_MASK
#error MANTISSA_MASK mismatch in to_vax_g8()
#endif
#define MANTISSA_MASK VAX_G_MANTISSA_MASK
/* If  the mantissas are the same, then so are the no. of bits and the hidden */
/* normalization bit                                                          */
#define MANTISSA_SIZE VAX_G_MANTISSA_SIZE
#define HIDDEN_BIT    VAX_G_HIDDEN_BIT

#define EXPONENT_ADJUSTMENT ( 1 + VAX_G_EXPONENT_BIAS - IEEE_T_EXPONENT_BIAS )

void FORTRAN_LINKAGE to_vax_g8( const void *inbuf, void *outbuf,
                                const int *count ) {

   register const unsigned int *in;     /* Microsoft C: up to 2 register vars */
#if IS_LITTLE_ENDIAN
   register unsigned short *out;        /* Microsoft C: up to 2 register vars */
   union { unsigned short i[2]; unsigned int l; } vaxpart;
#else
   unsigned char *out;
   union { unsigned char c[4]; unsigned int l; } vaxpart;
#endif
   unsigned int ieeepart1, vaxpart2;
   unsigned int m;
   int n;
   int e;


   in = (const unsigned int *) inbuf;
#if IS_LITTLE_ENDIAN
   out = (unsigned short *) outbuf;
#else
   out = (unsigned char *) outbuf;
#endif

   for ( n = *count; n > 0; n-- ) {

#if IS_LITTLE_ENDIAN
      vaxpart2  = *in++;
      ieeepart1 = *in++;
#else
      ieeepart1 = *in++;
      vaxpart2  = *in++;
#endif

      if ( ( ( ieeepart1 & ~SIGN_BIT ) | vaxpart2 ) == 0 ) {

         vaxpart.l = 0;      /* Set IEEE +-zero [e=m=0] to VAX zero [s=e=m=0] */
      /* vaxpart2  = 0; */                        /* vaxpart2 is already zero */

      } else if ( ( e = ( ieeepart1 & IEEE_T_EXPONENT_MASK ) ) ==
                  IEEE_T_EXPONENT_MASK ) {

       /* VAX's have no equivalents for IEEE +-Infinity and +-NaN [e=all-1's] */
         raise( SIGFPE );
               /* Fixup to VAX +-extrema [e=all-1's] with zero mantissa [m=0] */
         vaxpart.l = ( ieeepart1 & SIGN_BIT ) | VAX_G_EXPONENT_MASK;
         vaxpart2  = 0;
         
      } else {

         e >>= MANTISSA_SIZE;              /* Obtain the biased IEEE exponent */
         m = ieeepart1 & MANTISSA_MASK;           /* Obtain the IEEE mantissa */

         if ( e == 0 ) {                          /* Denormalized [e=0, m<>0] */
                     /* Adjust representation from 2**(1-bias) to 2**(e-bias) */
            m = ( m << 1 ) | ( vaxpart2 >> 31 );
            vaxpart2 <<= 1;
            while ( ( m & HIDDEN_BIT ) == 0 ) {
               m = ( m << 1 ) | ( vaxpart2 >> 31 );
               vaxpart2 <<= 1;
               e         -= 1;                             /* Adjust exponent */
            }
            m &= MANTISSA_MASK;         /* Adjust mantissa to hidden-bit form */
         }

         if ( ( e += EXPONENT_ADJUSTMENT ) <= 0 ) {

            vaxpart.l = 0;                                /* Silent underflow */
            vaxpart2  = 0;

         } else if ( e > ( 2 * VAX_G_EXPONENT_BIAS - 1 ) ) {

            raise( SIGFPE );/* Overflow; fixup to VAX +-extrema [e=m=all-1's] */
            vaxpart.l = ( ieeepart1 & SIGN_BIT ) | ~SIGN_BIT;
            vaxpart2  = ~0;

         } else {

                    /* VAX normalized form [e>0] (both mantissas are 52 bits) */
            vaxpart.l = ( ieeepart1 & SIGN_BIT ) | ( e << MANTISSA_SIZE ) | m;
                                               /* vaxpart2 is already correct */

         }

      }

#if IS_LITTLE_ENDIAN
      *out++    = vaxpart.i[1];
      *out++    = vaxpart.i[0];
      vaxpart.l = vaxpart2;
      *out++    = vaxpart.i[1];
      *out++    = vaxpart.i[0];
#else
      *out++    = vaxpart.c[1];
      *out++    = vaxpart.c[0];
      *out++    = vaxpart.c[3];
      *out++    = vaxpart.c[2];
      vaxpart.l = vaxpart2;
      *out++    = vaxpart.c[1];
      *out++    = vaxpart.c[0];
      *out++    = vaxpart.c[3];
      *out++    = vaxpart.c[2];
#endif

   }

}

#undef MANTISSA_MASK
#undef MANTISSA_SIZE
#undef HIDDEN_BIT
#undef EXPONENT_ADJUSTMENT

#endif /* #ifdef MAKE_TO_VAX_G8 */

/*************************************************************** to_vax_h16() */

#ifdef MAKE_TO_VAX_H16

/* Assert the mantissa in a VAX H_float is the same as in an IEEE X_float     */
#if VAX_H_MANTISSA_MASK != IEEE_X_MANTISSA_MASK
#error MANTISSA_MASK mismatch in to_vax_h16()
#endif
#define MANTISSA_MASK VAX_H_MANTISSA_MASK
/* If  the mantissas are the same, then so are the no. of bits and the hidden */
/* normalization bit                                                          */
#define MANTISSA_SIZE VAX_H_MANTISSA_SIZE
#define HIDDEN_BIT    VAX_H_HIDDEN_BIT

#define EXPONENT_ADJUSTMENT ( 1 + VAX_H_EXPONENT_BIAS - IEEE_X_EXPONENT_BIAS )

void FORTRAN_LINKAGE to_vax_h16( const void *inbuf, void *outbuf,
                                 const int *count ) {

   register const unsigned int *in;     /* Microsoft C: up to 2 register vars */
#if IS_LITTLE_ENDIAN
   register unsigned short *out;        /* Microsoft C: up to 2 register vars */
   union { unsigned short i[2]; unsigned int l; } vaxpart;
#else
   unsigned char *out;
   union { unsigned char c[4]; unsigned int l; } vaxpart;
#endif
   unsigned int ieeepart1, vaxpart2, vaxpart3, vaxpart4;
   unsigned int m;
   int n;
   int e;


   in = (const unsigned int *) inbuf;
#if IS_LITTLE_ENDIAN
   out = (unsigned short *) outbuf;
#else
   out = (unsigned char *) outbuf;
#endif

   for ( n = *count; n > 0; n-- ) {

#if IS_LITTLE_ENDIAN
      vaxpart4  = *in++;
      vaxpart3  = *in++;
      vaxpart2  = *in++;
      ieeepart1 = *in++;
#else
      ieeepart1 = *in++;
      vaxpart2  = *in++;
      vaxpart3  = *in++;
      vaxpart4  = *in++;
#endif

      if ( ( ( ieeepart1 & ~SIGN_BIT ) | vaxpart2 | vaxpart3 | vaxpart4 ) == 0
         ) {

         vaxpart.l = 0;      /* Set IEEE +-zero [e=m=0] to VAX zero [s=e=m=0] */
      /* vaxpart2  = 0;     vaxpart2, vaxpart3, and vaxpart4 are already zero */
      /* vaxpart3  = 0; */
      /* vaxpart4  = 0; */

      } else if ( ( e = ( ieeepart1 & IEEE_X_EXPONENT_MASK ) ) ==
                  IEEE_X_EXPONENT_MASK ) {

       /* VAX's have no equivalents for IEEE +-Infinity and +-NaN [e=all-1's] */
         raise( SIGFPE );
               /* Fixup to VAX +-extrema [e=all-1's] with zero mantissa [m=0] */
         vaxpart.l = ( ieeepart1 & SIGN_BIT ) | VAX_H_EXPONENT_MASK;
         vaxpart2  = 0;
         vaxpart3  = 0;
         vaxpart4  = 0;
         
      } else {

         e >>= MANTISSA_SIZE;              /* Obtain the biased IEEE exponent */
         m = ieeepart1 & MANTISSA_MASK;           /* Obtain the IEEE mantissa */

         if ( e == 0 ) {                          /* Denormalized [e=0, m<>0] */
                     /* Adjust representation from 2**(1-bias) to 2**(e-bias) */
            m = ( m << 1 ) | ( vaxpart2 >> 31 );
            vaxpart2 <<= 1;
            while ( ( m & HIDDEN_BIT ) == 0 ) {
               m = ( m << 1 ) | ( vaxpart2 >> 31 );
               vaxpart2 = ( vaxpart2 << 1 ) | ( vaxpart3 >> 31 );
               vaxpart3 = ( vaxpart3 << 1 ) | ( vaxpart4 >> 31 );
               vaxpart4 <<= 1;
               e         -= 1;                             /* Adjust exponent */
            }
            m &= MANTISSA_MASK;         /* Adjust mantissa to hidden-bit form */
         }

         if ( ( e += EXPONENT_ADJUSTMENT ) <= 0 ) {

            vaxpart.l = 0;                                /* Silent underflow */
            vaxpart2  = 0;
            vaxpart3  = 0;
            vaxpart4  = 0;

         } else if ( e > ( 2 * VAX_H_EXPONENT_BIAS - 1 ) ) {

            raise( SIGFPE );/* Overflow; fixup to VAX +-extrema [e=m=all-1's] */
            vaxpart.l = ( ieeepart1 & SIGN_BIT ) | ~SIGN_BIT;
            vaxpart2  = ~0;
            vaxpart3  = ~0;
            vaxpart4  = ~0;

         } else {

                   /* VAX normalized form [e>0] (both mantissas are 112 bits) */
            vaxpart.l = ( ieeepart1 & SIGN_BIT ) | ( e << MANTISSA_SIZE ) | m;
                      /* vaxpart2, vaxpart3, and vaxpart4 are already correct */

         }

      }

#if IS_LITTLE_ENDIAN
      *out++    = vaxpart.i[1];
      *out++    = vaxpart.i[0];
      vaxpart.l = vaxpart2;
      *out++    = vaxpart.i[1];
      *out++    = vaxpart.i[0];
      vaxpart.l = vaxpart3;
      *out++    = vaxpart.i[1];
      *out++    = vaxpart.i[0];
      vaxpart.l = vaxpart4;
      *out++    = vaxpart.i[1];
      *out++    = vaxpart.i[0];
#else
      *out++    = vaxpart.c[1];
      *out++    = vaxpart.c[0];
      *out++    = vaxpart.c[3];
      *out++    = vaxpart.c[2];
      vaxpart.l = vaxpart2;
      *out++    = vaxpart.c[1];
      *out++    = vaxpart.c[0];
      *out++    = vaxpart.c[3];
      *out++    = vaxpart.c[2];
      vaxpart.l = vaxpart3;
      *out++    = vaxpart.c[1];
      *out++    = vaxpart.c[0];
      *out++    = vaxpart.c[3];
      *out++    = vaxpart.c[2];
      vaxpart.l = vaxpart4;
      *out++    = vaxpart.c[1];
      *out++    = vaxpart.c[0];
      *out++    = vaxpart.c[3];
      *out++    = vaxpart.c[2];
#endif

   }

}

#undef MANTISSA_MASK
#undef MANTISSA_SIZE
#undef HIDDEN_BIT
#undef EXPONENT_ADJUSTMENT

#endif /* #ifdef MAKE_TO_VAX_H16 */
