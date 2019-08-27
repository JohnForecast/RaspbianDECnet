#
# makefile.cc - Make library of functions for reading and writing VAX format
#               data for Solaris using Sun C (cc).
#
# Shell command syntax:
#
#    make -f makefile.cc [ all | libvaxdata | test | clean ]
#
# -O (standard optimization) -Xc (strict ANSI)
CC     = cc
CFLAGS = -O -Xc

include makefile.solaris
