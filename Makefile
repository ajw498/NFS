
NETINCLUDE = TCPIPLibs:
NETLIBS    = TCPIPLibs:o.socklib TCPIPLIBS:o.inetlib

THROWBACK = -throwback

CC = cc
CFLAGS = -Wp -IC: -I$(NETINCLUDE) -fah $(THROWBACK)

LINK = link
LINKFLAGS = #-d

all: main test

RPCFILES = rpc-calls.c rpccalls.h rpc-structs.h rpc-process1.h rpc-process2.h
NFSFILES = nfs-calls.c nfscalls.h nfs-structs.h nfs-process1.h nfs-process2.h
MOUNTFILES = mount-calls.c mountcalls.h mount-structs.h mount-process1.h mount-process2.h

$(RPCFILES): rpc-spec ProcessSpec
	perl ProcessSpec rpc rpc-spec

$(NFSFILES): nfs-spec ProcessSpec
	perl ProcessSpec nfs nfs-spec

$(MOUNTFILES): mount-spec ProcessSpec
	perl ProcessSpec mount mount-spec

GENOBJS = rpc-calls.o nfs-calls.o mount-calls.o

OBJS = rpc.o test.o

$(OBJS) $(GENOBJS): $(RPCFILES) $(NFSFILES) $(MOUNTFILES)

main: main.o
	$(LINK) -o $@ $(LINKFLAGS) main.o C:o.stubs $(NETLIBS)

test: $(OBJS) $(GENOBJS)
	$(LINK) -o $@ $(LINKFLAGS) $(OBJS) $(GENOBJS) C:o.stubs $(NETLIBS)

.SUFFIXES: .o .s .c

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

