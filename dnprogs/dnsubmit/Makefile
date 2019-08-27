# Makefile for dnsubmit

include ../Makefile.common

PROG1=dnsubmit
PROG2=dnprint

MANPAGES=dnsubmit.1

PROG1OBJS=dnsubmit.o

all: $(PROG1) $(PROG2)

$(PROG1): $(PROG1OBJS) $(DEPLIBS)
	$(CXX) $(CXXFLAGS) -o $@ $(PROG1OBJS) $(LIBS)

$(PROG2): $(PROG1)
	ln -sf $< $@

install:
	install -d $(prefix)/bin
	install -d $(manprefix)/man/man1
	install -m 0755 $(STRIPBIN) $(PROG1) $(prefix)/bin
	ln -sf $(PROG1) $(prefix)/bin/$(PROG2)
	install -m 0644 $(MANPAGES) $(manprefix)/man/man1
	ln -sf $(PROG1).1 $(manprefix)/man/man1/$(PROG2).1


dep depend:	
	$(CXX) $(CXXFLAGS) -MM *.cc >.depend 2>/dev/null

clean:
	rm -f $(PROG2) $(PROG1) *.o *.bak .depend


ifeq (.depend,$(wildcard .depend))
include .depend
endif
