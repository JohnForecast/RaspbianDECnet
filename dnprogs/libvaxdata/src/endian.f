******************************************************************************
*                                                                            *
* Endian.f - VAX Fortran program to generate test data for the test.c        *
*            libvaxdata conversion routines test driver.                     *
*                                                                            *
* Author:      Lawrence M. Baker                                             *
*              U.S. Geological Survey                                        *
*              345 Middlefield Road  MS977                                   *
*              Menlo Park, CA  94025                                         *
*              baker@usgs.gov                                                *
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
* 28-Jan-2010  L. M. Baker      Original version.                            *
*                                                                            *
******************************************************************************

      Program Endian

      Integer*2  i2(6)
      Integer*4  i4(10)
      Real*4     f4(12)
      Real*8     d8(12)
      Real*8     g8(12)
      Real*16    h16(13)

      i2(1) =      1
      i2(2) =     -1
      i2(3) =    256
      i2(4) =   -256
      i2(5) =  12345
      i2(6) = -12345

      Write (*,601) ( i2(i), i2(i), i = 1,6 )
  601 Format ( / ' I2' / ( ' ', I, '  ', Z4.4 ) )

      i4( 1) =          1
      i4( 2) =         -1
      i4( 3) =        256
      i4( 4) =       -256
      i4( 5) =      65536
      i4( 6) =     -65536
      i4( 7) =   16777216
      i4( 8) =  -16777216
      i4( 9) =  123456789
      i4(10) = -123456789

      Write (*,602) ( i4(i), i4(i), i = 1,10 )
  602 Format ( / ' I4' / ( ' ', I, '  ', Z8.8 ) )

      f4( 1) =  1.0
      f4( 2) = -1.0
      f4( 3) =  3.5
      f4( 4) = -3.5
      f4( 5) =  3.14159
      f4( 6) = -3.14159
      f4( 7) =  1.E37
      f4( 8) = -1.E37
      f4( 9) =  1.E-37
      f4(10) = -1.E-37
      f4(11) =  1.23456789
      f4(12) = -1.23456789

      Write (*,603) ( f4(i), f4(i), i = 1,12 )
  603 Format ( / ' F4' / ( ' ', 1PG, '  ', Z8.8 ) )

      d8( 1) =  1.0
      d8( 2) = -1.0
      d8( 3) =  3.5
      d8( 4) = -3.5
      d8( 5) =  3.141592653589793D0
      d8( 6) = -3.141592653589793D0
      d8( 7) =  1.D37
      d8( 8) = -1.D37
      d8( 9) =  1.D-37
      d8(10) = -1.D-37
      d8(11) =  1.23456789012345D0
      d8(12) = -1.23456789012345D0

      Do i = 1,12
      End Do

      Write (*,604) ( d8(i), d8(i), i = 1,12 )
  604 Format ( / ' D8' / ( ' ', 1PG, '  ', Z16.16 ) )

      Call MTH$CVT_DA_GA( d8, g8, 12 )

      Write (*,605) ( d8(i), g8(i), i = 1,12 )
  605 Format ( / ' G8' / ( ' ', 1PG, '  ', Z16.16 ) )

      h16( 1) =  1.0
      h16( 2) = -1.0
      h16( 3) =  3.5
      h16( 4) = -3.5
      h16( 5) =  3.141592653589793238462643383279Q0
      h16( 6) = -3.141592653589793238462643383279Q0
      h16( 7) =  1.D37
      h16( 8) = -1.D37
      h16( 9) =  1.D-37
      h16(10) = -1.D-37
      h16(11) =  1.2345678901234567890123456789Q0
      h16(12) = -1.2345678901234567890123456789Q0

      Write (*,606) ( h16(i), h16(i), i = 1,12 )
  606 Format ( / ' H16' / ( ' ', 1PG, '  ', Z32.32 ) )

      Stop
      End
