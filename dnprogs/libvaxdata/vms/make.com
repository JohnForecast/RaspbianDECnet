$ verify = 'F$Verify( 0 )'
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!                                                                             !
$! Make.com - Make library of functions for reading and writing VAX format     !
$!            data for OpenVMS using DEC/Compaq/HP VAX C or DEC C (CC).        !
$!                                                                             !
$! DCL command syntax: @Make [ all | libvaxdata | test | clean ]               !
$!                                                                             !
$!                                                                             !
$! Author:      Lawrence M. Baker                                              !
$!              U.S. Geological Survey                                         !
$!              345 Middlefield Road  MS977                                    !
$!              Menlo Park, CA  94025                                          !
$!              baker@usgs.gov                                                 !
$!                                                                             !
$! Citation:    Baker, L.M., 2005, libvaxdata: VAX Data Format Conversion      !
$!                 Routines: U.S. Geological Survey Open-File Report 2005-     !
$!                 1424, v1.1 (http://pubs.usgs.gov/of/2005/1424/).            !
$!                                                                             !
$!                                                                             !
$!                                 Disclaimer                                  !
$!                                                                             !
$! Although  this program has been used by the USGS, no warranty, expressed or !
$! implied, is made by the USGS or the United  States  Government  as  to  the !
$! accuracy  and  functioning of the program and related program material, nor !
$! shall the fact  of  distribution  constitute  any  such  warranty,  and  no !
$! responsibility is assumed by the USGS in connection therewith.              !
$!                                                                             !
$!                                                                             !
$! Modification History:                                                       !
$!                                                                             !
$!  2-Sep-2005  L. M. Baker      Original version (from make.libvfbb).         !
$! 30-Jan-2010  L. M. Baker      Add test program.                             !
$!                                                                             !
$!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
$!
$ lib_name = "LibVAXData"
$ args = "|all|libvaxdata|test|clean|"
$
$ P1 = F$Edit( P1, "TRIM,LOWERCASE" )
$ If ( P1 .eqs. "" ) Then $ P1 = "all"
$ If ( F$Locate( "|''P1'|", args ) .eq. F$Length( args ) )
$ Then
$    Write Sys$Error -
        "DCL command syntax: @Make [ all | libvaxdata | test | clean ]"
$    Goto EXIT
$ EndIf
$!
$ arch = F$GetSYI( "ARCH_NAME" )
$!
$ If ( F$Search( "'arch'.DIR;1" ) .eqs. "" )
$ Then
$    Set Verify
$ Create /Directory [.'arch']
$    junk = 'F$Verify( 0 )'
$ EndIf
$!
$ cflags = ""
$!
$ Set Verify
$ Set Default [.'arch']
$ junk = 'F$Verify( 0 )'
$ Goto 'P1'
$!
$ALL:
$LIBVAXDATA:
$!
$! VAX Data Conversion Routines (upper case)
$!
$ Set Verify
$ CC 'cflags' [--.Src]From_VAX_I2
$ CC 'cflags' [--.Src]From_VAX_I4
$ CC 'cflags' [--.Src]From_VAX_R4
$ CC 'cflags' [--.Src]From_VAX_D8
$ CC 'cflags' [--.Src]From_VAX_G8
$ CC 'cflags' [--.Src]From_VAX_H16
$ CC 'cflags' [--.Src]To_VAX_I2
$ CC 'cflags' [--.Src]To_VAX_I4
$ CC 'cflags' [--.Src]To_VAX_R4
$ CC 'cflags' [--.Src]To_VAX_D8
$ CC 'cflags' [--.Src]To_VAX_G8
$ CC 'cflags' [--.Src]To_VAX_H16
$ CC 'cflags' [--.Src]Is_Little_Endian
$ junk = 'F$Verify( 0 )'
$!
$ If ( arch .eqs. "VAX" )
$ Then
$    blocks  = 47
$    modules = 13
$ Else
$    blocks  = 97
$    modules = 26
$ EndIf
$!
$! Create a static library
$!
$ Set Verify
$ Library /Create=(Blocks:'blocks',Modules:'modules',Globals:'modules',-
                   KeySize:16,History:0) 'lib_name'
$ Library /Insert 'lib_name' -
          From_VAX_I2  , From_VAX_I4  , From_VAX_R4  , From_VAX_D8  , -
          From_VAX_G8  , From_VAX_H16 , To_VAX_I2    , To_VAX_I4    , -
          To_VAX_R4    , To_VAX_D8    , To_VAX_G8    , To_VAX_H16   , -
          Is_Little_Endian
$ Purge 'lib_name'.olb
$ Delete From_VAX_I2.obj;*  , From_VAX_I4.obj;*  , From_VAX_R4.obj;*  , -
         From_VAX_D8.obj;*  , From_VAX_G8.obj;*  , From_VAX_H16.obj;* , -
         To_VAX_I2.obj;*    , To_VAX_I4.obj;*    , To_VAX_R4.obj;*    , -
         To_VAX_D8.obj;*    , To_VAX_G8.obj;*    , To_VAX_H16.obj;*   , -
         Is_Little_Endian.obj;*
$ junk = 'F$Verify( 0 )'
$!
$ If ( arch .nes. "VAX" )
$ Then
$!
$    cflags = cflags + " /Names=As_Is"
$!
$!   VAX Data Conversion Routines (lower case)
$!
$    Set Verify
$ CC 'cflags' [--.Src]From_VAX_I2
$ CC 'cflags' [--.Src]From_VAX_I4
$ CC 'cflags' [--.Src]From_VAX_R4
$ CC 'cflags' [--.Src]From_VAX_D8
$ CC 'cflags' [--.Src]From_VAX_G8
$ CC 'cflags' [--.Src]From_VAX_H16
$ CC 'cflags' [--.Src]To_VAX_I2
$ CC 'cflags' [--.Src]To_VAX_I4
$ CC 'cflags' [--.Src]To_VAX_R4
$ CC 'cflags' [--.Src]To_VAX_D8
$ CC 'cflags' [--.Src]To_VAX_G8
$ CC 'cflags' [--.Src]To_VAX_H16
$ CC 'cflags' [--.Src]Is_Little_Endian
$    junk = 'F$Verify( 0 )'
$!
$    Set Verify
$ Library /Insert 'lib_name' -
          From_VAX_I2  , From_VAX_I4  , From_VAX_R4  , From_VAX_D8  , -
          From_VAX_G8  , From_VAX_H16 , To_VAX_I2    , To_VAX_I4    , -
          To_VAX_R4    , To_VAX_D8    , To_VAX_G8    , To_VAX_H16   , -
	  Is_Little_Endian
$ Delete From_VAX_I2.obj;*  , From_VAX_I4.obj;*  , From_VAX_R4.obj;*  , -
         From_VAX_D8.obj;*  , From_VAX_G8.obj;*  , From_VAX_H16.obj;* , -
         To_VAX_I2.obj;*    , To_VAX_I4.obj;*    , To_VAX_R4.obj;*    , -
         To_VAX_D8.obj;*    , To_VAX_G8.obj;*    , To_VAX_H16.obj;*   , -
	 Is_Little_Endian.obj;*
$    junk = 'F$Verify( 0 )'
$ EndIf
$!
$ If ( P1 .nes. "all" ) Then $ Goto DONE
$!
$TEST:
$!
$ Set NoOn
$ Set Verify
$ CC /Float=IEEE_Float [--.Src]Test
$ Link Test, 'lib_name'/Library
$ Delete Test.obj;*
$ junk = 'F$Verify( 0 )'
$ Set On
$!
$ Goto DONE
$!
$CLEAN:
$!
$ Set NoOn
$ Set Verify
$ Delete 'lib_name'.olb;*   , Test.exe;*         , Test.obj;*
$ Delete From_VAX_I2.obj;*  , From_VAX_I4.obj;*  , From_VAX_R4.obj;*  , -
         From_VAX_D8.obj;*  , From_VAX_G8.obj;*  , From_VAX_H16.obj;* , -
         To_VAX_I2.obj;*    , To_VAX_I4.obj;*    , To_VAX_R4.obj;*    , -
         To_VAX_D8.obj;*    , To_VAX_G8.obj;*    , To_VAX_H16.obj;*   , -
	 Is_Little_Endian.obj;*
$ junk = 'F$Verify( 0 )'
$ Set On
$!
$DONE:
$!
$ Set Verify
$ Set Default [-]
$ junk = 'F$Verify( 0 )'
$!
$EXIT:
$ Exit 1 + ( 0 * F$Verify( verify ) )
