# Makefile for dapfs

include ../Makefile.common

PROG1=dapfs

MANPAGES=mount.dapfs.8

PROG1OBJS=dapfs.o dapfs_dap.o filenames.o kfifo.o

all: $(PROG1)

CFLAGS=-I../include -I ../librms -Wall $(DFLAGS) -fdollars-in-identifiers

$(PROG1): $(PROG1OBJS) $(DEPLIBS)
	g++ -o$(PROG1) $(LDFLAGS) $(PROG1OBJS) $(LIBDAP) -L../librms -lrms $(LIBDNET) -lfuse -lpthread

install:
	install -d $(rootprefix)/sbin
	install -d $(manprefix)/man/man8
	install -m 0755 $(STRIPBIN) $(PROG1) $(rootprefix)/sbin/mount.dapfs
	install -m 0644 $(MANPAGES) $(manprefix)/man/man8

dep depend:	
	$(CXX) $(CXXFLAGS) -MM *.cc >.depend 2>/dev/null

clean:
	rm -f $(PROG1) *.o *.bak .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif

