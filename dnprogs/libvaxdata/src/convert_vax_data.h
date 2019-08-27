/******************************************************************************
 *                                                                            *
 * convert_vax_data.h - Convert VAX-format data to/from Unix (IEEE) format.   *
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
 * VAXes (as well as the Intel 80x86 family) store integers in 2's complement *
 * format, ordering the bytes in memory from low-order to high-order  (collo- *
 * quially  called  little-endian format).  Most Unix machines (Sun, IBM, HP) *
 * also store integers in 2's complement format, but  use  the  opposite  (so *
 * called big-endian) byte ordering.                                          *
 *                                                                            *
 * A  VAX  integer  is converted to (big-endian) Unix format by reversing the *
 * byte order.                                                                *
 *                                                                            *
 * Most  Unix machines implement the ANSI/IEEE 754-1985 floating-point arith- *
 * metic standard.  VAX and IEEE formats are similar  (after  byte-swapping). *
 * The  high-order bit is a sign bit (s).  This is followed by a biased expo- *
 * nent (e), and a (usually) hidden-bit normalized mantissa (m).  They differ *
 * in  the number used to bias the exponent, the location of the implicit bi- *
 * nary point for the mantissa, and the representation of exceptional numbers *
 * (e.g., +/-infinity).                                                       *
 *                                                                            *
 * VAX floating-point formats:  (-1)^s * 2^(e-bias) * 0.1m                    *
 *                                                                            *
 *                  31              15              0                         *
 *                   |               |              |                         *
 * F_floating        mmmmmmmmmmmmmmmmseeeeeeeemmmmmmm  bias = 128             *
 * D_floating        mmmmmmmmmmmmmmmmseeeeeeeemmmmmmm  bias = 128             *
 *                   mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm                         *
 * G_floating        mmmmmmmmmmmmmmmmseeeeeeeeeeemmmm  bias = 1024            *
 *                   mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm                         *
 * H_floating        mmmmmmmmmmmmmmmmseeeeeeeeeeeeeee  bias = 16384           *
 *                   mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm                         *
 *                   mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm                         *
 *                   mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm                         *
 *                                                                            *
 * IEEE floating-point formats:  (-1)^s * 2^(e-bias) * 1.m                    *
 *                                                                            *
 *                  31              15              0                         *
 *                   |               |              |                         *
 * S_floating        seeeeeeeemmmmmmmmmmmmmmmmmmmmmmm  bias = 127             *
 * T_floating        seeeeeeeeeeemmmmmmmmmmmmmmmmmmmm  bias = 1023            *
 *                   mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm                         *
 * X_floating        seeeeeeeeeeeeeeemmmmmmmmmmmmmmmm  bias = 16383           *
 *                   mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm                         *
 *                   mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm                         *
 *                   mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm                         *
 *                                                                            *
 * A  VAX floating-point number is converted to IEEE floating-point format by *
 * subtracting (1+VAX_bias-IEEE_bias) from the exponent field to  (1)  adjust *
 * from  VAX  0.1m hidden-bit normalization to IEEE 1.m hidden-bit normaliza- *
 * tion and (2) adjust the bias from VAX format to IEEE  format.   True  zero *
 * [s=e=m=0]  and  dirty  zero  [s=e=0, m<>0] are special cases which must be *
 * recognized and handled separately.  Both VAX zeros are converted  to  IEEE *
 * +zero [s=e=m=0].                                                           *
 *                                                                            *
 * Numbers  whose  absolute value is too small to represent in the normalized *
 * IEEE format illustrated above are converted to subnormal form [e=0,  m>0]: *
 * (-1)^s * 2^(1-bias) * 0.m.   Numbers  whose absolute value is too small to *
 * represent in subnormal form are set to 0.0 (silent underflow).             *
 *                                                                            *
 * Note: If the fractional part of the VAX floating-point number is too large *
 *       for the corresponding IEEE floating-point format,  bits  are  simply *
 *       discarded  from  the  right.  Thus, the remaining fractional part is *
 *       chopped, not rounded to the lowest-order bit.  This can  only  occur *
 *       when the conversion requires IEEE subnormal form.                    *
 *                                                                            *
 * A  VAX  floating-point  reserved operand [s=1, e=0, m=any] causes a SIGFPE *
 * exception to be raised.  The converted result is set to zero.              *
 *                                                                            *
 * Conversely,  an  IEEE  floating-point number is converted to VAX floating- *
 * point format by  adding  (1+VAX_bias-IEEE_bias)  to  the  exponent  field. *
 * +zero [s=e=m=0], -zero [s=1, e=m=0], infinities [s=X, e=all-1's, m=0], and *
 * NaNs [s=X, e=all-1's, m<>0] are special cases which must be recognized and *
 * handled  separately.   Both  IEEE  zeros  are  converted  to VAX true zero *
 * [s=e=m=0].  Infinities and NaNs cause a SIGFPE  exception  to  be  raised. *
 * The  result  returned  has  the  largest VAX exponent [e=all-1's] and zero *
 * mantissa [m=0] with the same sign as the original.                         *
 *                                                                            *
 * Numbers  whose  absolute value is too small to represent in the normalized *
 * VAX format illustrated above are set  to  0.0  (silent  underflow).   (VAX *
 * floating-point  format does not support subnormal numbers.)  Numbers whose *
 * absolute value exceeds the largest representable VAX-format number cause a *
 * SIGFPE exception to be raised (overflow).  (VAX floating-point format does *
 * not have reserved bit patterns for infinities and  not-a-numbers  [NaNs].) *
 * The  result  returned  has  the  largest  VAX  exponent and mantissa [e=m= *
 * all-1's] with the same sign as the original.                               *
 *                                                                            *
 *                                                                            *
 * Two  variants  of  convert_vax_data.c are available using IS_LITTLE_ENDIAN *
 * and APPEND_UNDERSCORE.  If IS_LITTLE_ENDIAN is defined as 0 (false),  then *
 * the  conversions  are performed for a big-endian machine, i.e., byte reor- *
 * dering is performed for all data types.  If IS_LITTLE_ENDIAN is defined as *
 * 1 (true), then no reordering is performed.  (Integers are identical to VAX *
 * format.)                                                                   *
 *                                                                            *
 * If  IS_LITTLE_ENDIAN is not defined, then it is defined as 1 (true) if any *
 * of the following macros are defined:                                       *
 *                                                                            *
 *    vax __vax vms __vms __alpha  DEC VAX C, GNU C on a DEC VAX or a DEC     *
 *                                    Alpha, DEC C                            *
 *    M_I86 _M_IX86 __M_ALPHA      Microsoft 80x86 C or Microsoft Visual C++  *
 *                                    on an Intel x86 or a DEC Alpha          *
 *    i386 __i386                  Sun C, GNU C, or Intel C on an Intel x86   *
 *    __x86_64 __x86_64__          GNU C or Portland Group C on an AMD        *
 *                                    Opteron or an Intel EM64T               *
 *                                                                            *
 * If  APPEND_UNDERSCORE  is defined, the entry point names are compiled with *
 * an underscore appended.  This is required so that they can  be  called  by *
 * Fortran  in  cases  where  the  Fortran  compiler appends an underscore to *
 * externally called routines (e.g., Sun Fortran).                            *
 *                                                                            *
 * Normally,  all routines are compiled into a single object module.  To com- *
 * pile a single routine into its own module, define MAKE_routine_name,  sub- *
 * stituting  the upper-case name of the routine for routine_name.  For exam- *
 * ple, MAKE_TO_VAX_I2.  (This is useful, for example, to insert the routines *
 * into  a  library such that a linker may extract only the routines actually *
 * needed by a particular program.)                                           *
 *                                                                            *
 * convert_vax_data.c assumes an ANSI C compiler, 8-bit chars, 16-bit shorts, *
 * and 32-bit ints.                                                           *
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
 *  8-Mar-2001  L. M. Baker      Original version.                            *
 *                               Define upcased and underscore-appended       *
 *                                  variants.                                 *
 *  9-Mar-2001  L. M. Baker      Add #ifndef _CONVERT_VAX_DATA guard.         *
 * 16-Sep-2005  L. M. Baker      Correct the diagram of IEEE X_floating       *
 *                                  (exponent is 15 bits, not 11 bits).       *
 * 19-Sep-2005  L. M. Baker      Add fixups for IEEE-to-VAX conversion        *
 *                                  faults (+-infinity, +-NaN, overflow).     *
 *  8-Nov-2005  L. M. Baker      Move #define const if not __STDC__ from      *
 *                                  convert_vax_data.c.                       *
 * 27-Jan-2010  L. M. Baker      Change guard to #ifndef _CONVERT_VAX_DATA_H. *
 *                                                                            *
 ******************************************************************************/

#ifndef _CONVERT_VAX_DATA_H
#define _CONVERT_VAX_DATA_H

#ifndef FORTRAN_LINKAGE
#define FORTRAN_LINKAGE
#endif

#ifdef UPCASE

#define from_vax_i2  FROM_VAX_I2
#define from_vax_i4  FROM_VAX_I4
#define from_vax_r4  FROM_VAX_R4
#define from_vax_d8  FROM_VAX_D8
#define from_vax_g8  FROM_VAX_G8
#define from_vax_h16 FROM_VAX_H16

#define to_vax_i2    TO_VAX_I2
#define to_vax_i4    TO_VAX_I4
#define to_vax_r4    TO_VAX_R4
#define to_vax_d8    TO_VAX_D8
#define to_vax_g8    TO_VAX_G8
#define to_vax_h16   TO_VAX_H16

#endif

#ifdef APPEND_UNDERSCORE

#define from_vax_i2  from_vax_i2##_
#define from_vax_i4  from_vax_i4##_
#define from_vax_r4  from_vax_r4##_
#define from_vax_d8  from_vax_d8##_
#define from_vax_g8  from_vax_g8##_
#define from_vax_h16 from_vax_h16##_

#define to_vax_i2    to_vax_i2##_
#define to_vax_i4    to_vax_i4##_
#define to_vax_r4    to_vax_r4##_
#define to_vax_d8    to_vax_d8##_
#define to_vax_g8    to_vax_g8##_
#define to_vax_h16   to_vax_h16##_

#endif

/* const is ANSI C, C++ only */
#if !defined( __STDC__ ) && !defined( __cplusplus )
#define const
#endif

void FORTRAN_LINKAGE from_vax_i2(  const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE from_vax_i4(  const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE from_vax_r4(  const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE from_vax_d8(  const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE from_vax_g8(  const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE from_vax_h16( const void *inbuf, void *outbuf,
                                   const int *count );

void FORTRAN_LINKAGE to_vax_i2(    const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE to_vax_i4(    const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE to_vax_r4(    const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE to_vax_d8(    const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE to_vax_g8(    const void *inbuf, void *outbuf,
                                   const int *count );
void FORTRAN_LINKAGE to_vax_h16(   const void *inbuf, void *outbuf,
                                   const int *count );

#endif /* _CONVERT_VAX_DATA_H */
