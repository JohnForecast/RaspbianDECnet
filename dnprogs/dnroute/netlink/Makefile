include ../../Makefile.common

NLOBJ=ll_map.o libnetlink.o

CFLAGS+=-Iinclude

all: libnetlink.a # libutil.a

libnetlink.a: $(NLOBJ)
	ar rcs $@ $(NLOBJ)

clean:
	rm -f $(NLOBJ) $(ADDLIB) libnetlink.a

