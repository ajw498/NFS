
NETINCLUDE = TCPIPLibs:
NETLIBS    = TCPIPLibs:o.socklib TCPIPLIBS:o.inetlib

THROWBACK = -throwback

CC = cc
CFLAGS = -Wp -IC: -I$(NETINCLUDE) -fah $(THROWBACK)

LINK = link
LINKFLAGS = #-d


OBJS = main.o

main:	$(OBJS)
	$(LINK) -o $@ $(LINKFLAGS) $(OBJS) C:o.stubs $(NETLIBS)

.SUFFIXES: .o .s .c

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

