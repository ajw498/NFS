
NETINCLUDE = TCPIPLibs:
#NETLIBS    = TCPIPLibs:o.socklib5 TCPIPLIBS:o.inetlib
NETLIBS    = TCPIPLibs:o.socklib5zm TCPIPLIBS:o.inetlibzm

THROWBACK = -throwback

CC = cc
CFLAGS = -Wp -IC: -I$(NETINCLUDE) -fah $(THROWBACK) -zM

LINK = link
LINKFLAGS = #-d

all2: module

all: main test


RPCFILES = rpc-calls.c rpccalls.h rpc-structs.h rpc-process1.h rpc-process2.h
NFSFILES = nfs-calls.c nfscalls.h nfs-structs.h nfs-process1.h nfs-process2.h
MOUNTFILES = mount-calls.c mountcalls.h mount-structs.h mount-process1.h mount-process2.h
PMAPFILES = portmapper-calls.c portmappercalls.h portmapper-structs.h portmapper-process1.h portmapper-process2.h

$(RPCFILES): rpc-spec ProcessSpec
	perl ProcessSpec rpc rpc-spec

$(NFSFILES): nfs-spec ProcessSpec
	perl ProcessSpec nfs nfs-spec

$(MOUNTFILES): mount-spec ProcessSpec
	perl ProcessSpec mount mount-spec

$(PMAPFILES): portmapper-spec ProcessSpec
	perl ProcessSpec portmapper portmapper-spec

GENOBJS = rpc-calls.o nfs-calls.o mount-calls.o portmapper-calls.o

OBJS = rpc.o test.o readdir.o

$(OBJS) $(GENOBJS): $(RPCFILES) $(NFSFILES) $(MOUNTFILES) $(PMAPFILES)

module: module.o modulehdr.o base.o $(OBJS) $(GENOBJS)
	link -m -o module module.o modulehdr.o base.o $(OBJS) $(GENOBJS) C:o.stubs $(NETLIBS)

module.o: module.c moduledefs.h
	$(CC) -throwback -c $(CFLAGS) -o module.o module.c

base.o: base.s
	$(CC) -throwback -c $(CFLAGS) -o base.o base.s

moduledefs.h modulehdr.o: @.cmhg.modulehdr
	cmhg  -throwback modulehdr.cmhg -o modulehdr.o moduledefs.h

main: main.o
	$(LINK) -o $@ $(LINKFLAGS) main.o C:o.stubs $(NETLIBS)

test: $(OBJS) $(GENOBJS)
	$(LINK) -o $@ $(LINKFLAGS) $(OBJS) $(GENOBJS) C:o.stubs $(NETLIBS)

.SUFFIXES: .o .s .c

.c.o:
	$(CC) -c  $(CFLAGS) -o $@ $<

