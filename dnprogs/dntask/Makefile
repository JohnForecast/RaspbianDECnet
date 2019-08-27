# Makefile for dntask

include ../Makefile.common

PROG1=dntask

MANPAGES=dntask.1

PROG1OBJS=dntask.o

all: $(PROG1)

$(PROG1): $(PROG1OBJS) $(DEPLIBDNET)
	$(CC) $(CFLAGS) -o $@ $(PROG1OBJS) $(LIBDNET)

install:
	install -d $(prefix)/bin
	install -d $(manprefix)/man/man1
	install -m 0755 $(STRIPBIN) $(PROG1) $(prefix)/bin
	install -m 0644 $(MANPAGES) $(manprefix)/man/man1

dep depend:	
	$(CC) $(CFLAGS) -MM *.c >.depend 2>/dev/null

clean:
	rm -f $(PROG1) *.o *.bak .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif

