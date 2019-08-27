# Makefile for dnlogin

include ../Makefile.common

PROG1=dnlogin

MANPAGES=dnlogin.1

PROG1OBJS=dnlogin.o found.o cterm.o tty.o

CFLAGS:=$(filter-out -fsigned-char,$(CFLAGS))
CFLAGS+=-funsigned-char -Wall

CFLAGS+=-g
#CFLAGS+=-O2

all: $(PROG1)

$(PROG1): $(PROG1OBJS) tty.h dnlogin.h $(DEPLIBDNET)
	$(CC) $(LDFLAGS) -o $@ $(PROG1OBJS) $(LIBDNET)

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

