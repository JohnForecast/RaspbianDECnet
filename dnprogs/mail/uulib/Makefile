# Generated automatically from Makefile.in by configure.
#
# ============================================================================
#
# This is the Makefile for the uu library, part of the uudeview package.
# The values here were guessed by ./configure and are probably correct.
#                                        fp@informatik.uni-frankfurt.de
#
# Usefull targets
#	all		Compile the package
#	clean		Deletes the binaries and objects and all the
#			other dirty stuff.
#
# ============================================================================
#
# $Id: Makefile,v 1.3 2002/04/17 09:47:38 patrick Exp $
#
# your make might need this
#
SHELL	=	/bin/sh
#
# If you don't have the GNU C compiler installed, set CC=cc here
#
CC	=	gcc
#
# C Compiler Options
#
CFLAGS	=	-Wall -fsigned-char -O -I.  -DHAVE_CONFIG_H
#
# the ranlib program
#
RANLIB =	ranlib
#
###############################################################################
# You shouldn't have to change anything below.
###############################################################################
#
# Programs to compile, Manpages to install and Versions
#
VERSION	=	0.5
PATCH	=	13
VDEF	=	-DVERSION=\"$(VERSION)\" -DPATCH=\"$(PATCH)\"
#

UULIB_SOURCE	=	uulib.c uucheck.c uunconc.c uuutil.c uuencode.c \
			uuscan.c uustring.c fptools.c
UULIB_OBJ	=	${UULIB_SOURCE:.c=.o}

#
# make stuff
#

.SUFFIXES:
.SUFFIXES: .c .o

all:		libuu.a

clean:
	rm -f [Xx]deview gif2gfp
	rm -f *.o *.a *.so core *~ TAGS

distclean:	clean
	rm -f config.status config.cache config.log Makefile config.h
	rm -f uudeview-*tar* uudeview-sfx

realclean:	distclean

new:		clean
	rm -f libuu.a
	$(MAKE) all

libuu.a:	$(UULIB_OBJ)
	rm -f $@
	ar r $@ $(UULIB_OBJ)
	-$(RANLIB) $@

.c.o:
	$(CC) -c $(CFLAGS) $(VDEF) $<

uuencode.o:	uuencode.c uudeview.h uuint.h uustring.h fptools.h config.h
uulib.o:	uulib.c uudeview.h uuint.h uustring.h fptools.h config.h
uunconc.o:	uunconc.c uudeview.h uuint.h uustring.h fptools.h config.h
uucheck.o:	uucheck.c uudeview.h uuint.h uustring.h fptools.h config.h
uuutil.o:	uuutil.c  uudeview.h uuint.h uustring.h fptools.h config.h
uuscan.o:	uuutil.c  uudeview.h uuint.h uustring.h fptools.h config.h
uustring.o:	uustring.c uudeview.h uuint.h uustring.h config.h
fptools.o:	fptools.c fptools.h config.h

uustring.h:	uustring.c
	awk -f uustring.awk < uustring.c > uustring.h
