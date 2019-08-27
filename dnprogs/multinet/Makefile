include ../Makefile.common

all:	multinet

multinet: multinet.c	
	$(CC) $(CFLAGS) $(LDFLAGS) -o multinet multinet.c 


install:
	install -d $(prefix)/sbin
	install -d $(manprefix)/man/man8
	install -m 0755 $(STRIPBIN) multinet $(prefix)/sbin
	install -m 0644 multinet.8 $(manprefix)/man/man8

dep depend:	
	$(CC) $(CFLAGS) -MM *.c >.depend 2>/dev/null

clean:	
	rm -f *.o *~ multinet
