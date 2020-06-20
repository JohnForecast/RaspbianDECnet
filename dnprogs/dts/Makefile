# Makefile for dts

include ../Makefile.common

PROG1=dts
MANPAGES1=dts.1
PROG1OBJS=dts.o

all: $(PROG1)

.c.0:
	$(CC) $(CFLAGS) $(SYSCONF_PREFIX) -c -o $@ $<

$(PROG1): $(PROG1OBJS) $(DEPLIBDNET) $(DEPLIBDAEMON)
	$(CC) $(CFLAGS) -o $@ $(PROG1OBJS) $(LIBDNET) $(LIBDAEMON)

install:
	install -d $(prefix)/bin
	install -d $(manprefix)/man/man1
	install -m 0755 $(STRIPBIN) $(PROG1) $(prefix)/bin
	install -m 0644 $(MANPAGES1) $(manprefix)/man/man1

dep depend:
	$(CC) $(CFLAGS) -MM *.c >.depend 2>/dev/null

clean:
	rm -f $(PROG1) *.o *.bak .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
