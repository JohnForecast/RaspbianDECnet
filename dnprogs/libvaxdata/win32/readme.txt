Two DOS Batch scripts have been provided for Windows:

   cwmake.bat    Metrowerks CodeWarrior C (MWCC)
   vcmake.bat    Microsoft Visual C (CL)

To create libvaxdata.lib and the test.exe program:

   For Metrowerks CodeWarrior:

      > "C:\Program Files\Metrowerks\CodeWarrior\Other Metrowerks Tools\
      Command Line Tools\CWEnv.bat" [on a single line]
      > cwmake.bat [ all | libvaxdata | test | clean ]

   For Microsoft Visual C++:

      > "C:\Program Files\Microsoft Visual Studio\VC98\Bin\VCVars32.bat"
      > vcmake.bat [ all | libvaxdata | test | clean ]

Substitute the correct path to set up the compiler command-line environment for
your compiler.  The default make target is all.

The library and all object files will be written here.

To link a C program with the library:

   For Metrowerks CodeWarrior:

      > MWLD program.obj path\libvaxdata.lib

   For Microsoft Visual C++:

      > LINK program.obj path\libvaxdata.lib

substituting the path to the library for here (assuming it has not been moved
or copied somewhere else).
