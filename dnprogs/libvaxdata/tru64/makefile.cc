#
# makefile.cc - Make library of functions for reading and writing VAX format
#               data for Tru64 UNIX using DEC/Compaq/HP C (cc).
#
# Shell command syntax:
#
#    make -f makefile.cc [ all | libvaxdata | test | clean ]
#
# -fast (optimize for speed) -std1 (strict ANSI)
CC     = cc
CFLAGS = -fast -std1

include makefile.osf1
