Three makefiles have been provided for Mac OS X:

   makefile.gcc    GNU C (gcc)
   makefile.icc    Intel C (icc)
   makefile.xlc    IBM XL C (xlc)

To create libvaxdata.a and the test program:

   # make -f makefile.xxx

substituting gcc, icc or xlc for xxx.  The default make target is all.

The library and all object files will be written to a subdirectory named
for the system architecture returned by "uname -p".

To link a C program with the library:

   # xxx -o program program.o -lvaxdata -Lhere/arch

substituting gcc, icc or xlc for xxx, and the path to the library for
here/arch (assuming it has not been moved or copied somewhere else).

To link a Fortran program with the library, substitute g95, gfortran, ifort,
xlf90, or xlf95 for xxx, plus the path to the library for here/arch.
