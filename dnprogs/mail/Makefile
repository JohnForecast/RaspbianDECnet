# Makefile for mail progs

include ../Makefile.common

PROG1=vmsmaild
PROG2=sendvmsmail

UUDIR=uulib
UULIB=$(UUDIR)/libuu.a
LIBUU=$(UULIB)

MANPAGES5=vmsmail.conf.5
MANPAGES8=vmsmaild.8 sendvmsmail.8

PROG1OBJS=vmsmaild.o receive.o configfile.o
PROG2OBJS=sendvmsmail.o configfile.o

CDEFS+=-I$(UUDIR) -DPROTOTYPES
CFLAGS+=-fdollars-in-identifiers

all: $(UULIB) $(PROG1) $(PROG2)

$(UULIB): 
	$(MAKE) -C $(UUDIR)

.c.o:
	$(CC) $(CFLAGS) $(SYSCONF_PREFIX) -c -o $@ $<

$(PROG1): $(PROG1OBJS) $(DEPLIBDNET) $(DEPLIBDAEMON) $(UULIB)
	$(CC) $(CFLAGS) -o $@ $(PROG1OBJS) $(LIBDNET) $(LIBUU) $(LIBDAEMON)

$(PROG2): $(PROG2OBJS) $(DEPLIBDNET) $(UULIB)
	$(CC) $(CFLAGS) -o $@ $(PROG2OBJS) $(LIBDNET) $(LIBUU)

install:
	install -d $(prefix)/bin
	install -d $(manprefix)/man/man5
	install -d $(manprefix)/man/man8
	install -m 0755 $(STRIPBIN) $(PROG1) $(prefix)/sbin
	install -m 0755 $(STRIPBIN) $(PROG2) $(prefix)/sbin
	install -m 0644 $(MANPAGES5) $(manprefix)/man/man5
	install -m 0644 $(MANPAGES8) $(manprefix)/man/man8

dep depend:	
	$(CC) $(CFLAGS) -MM *.c >.depend 2>/dev/null

clean:
	rm -f $(PROG1) $(PROG2) *.o *.bak .depend
	$(MAKE) -C $(UUDIR) clean


ifeq (.depend,$(wildcard .depend))
include .depend
endif
