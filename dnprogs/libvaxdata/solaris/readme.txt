Two makefiles have been provided for Solaris:

   makefile.gcc    GNU C (gcc)
   makefile.cc     Sun C (cc)

To create libvaxdata.a and the test program:

   # make -f makefile.xxx

substituting gcc or cc for xxx.  The default make target is all

The library and all object files will be written to a subdirectory named
for the system architecture returned by "uname -p".

To link a C program program.o with the library:

   # xxx -o program program.o -lvaxdata -Lhere/arch

substituting gcc or cc for xxx, and the path to the library for here/arch
(assuming it has not been moved or copied somewhere else).

To link a Fortran program with the library, substitute g95, gfortran, f90,
or f95 for xxx, plus the path to the library for here/arch.
