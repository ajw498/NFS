#
#    $Id$
#

INCLUDE = -IC:,TCPIPLibs:
LIBS    = TCPIPLIBS:o.unixlibzm TCPIPLIBS:o.inetlibzm TCPIPLibs:o.socklib5zm C:o.stubs

CC = cc
CFLAGS = -Wp $(INCLUDE) -fah -throwback -zM -zpq262144

LINK = link

all: module


rpc-calls.c rpc-calls.h: rpc-spec ProcessSpec
	perl ProcessSpec rpc rpc-spec

nfs-calls.c nfs-calls.h: nfs-spec ProcessSpec
	perl ProcessSpec nfs nfs-spec

mount-calls.c mount-calls.h: mount-spec ProcessSpec
	perl ProcessSpec mount mount-spec

portmapper-calls.c portmapper-calls.h: portmapper-spec ProcessSpec
	perl ProcessSpec portmapper portmapper-spec

GENHDRS = \
rpc-calls.h \
nfs-calls.h \
mount-calls.h \
portmapper-calls.h

GENOBJS = \
rpc-calls.o \
nfs-calls.o \
mount-calls.o \
portmapper-calls.o

OBJS = \
rpc.o \
base.o \
module.o \
modulehdr.o \
imageentry_func.o \
imageentry_file.o \
imageentry_args.o \
imageentry_bytes.o \
imageentry_openclose.o \
imageentry_common.o

$(OBJS) $(GENOBJS): $(GENHDRS) imageentry_common.h

module: $(OBJS) $(GENOBJS)
	link -m -o module $(OBJS) $(GENOBJS) $(LIBS)

module.o: module.c moduledefs.h

moduledefs.h modulehdr.o: @.cmhg.modulehdr
	cmhg  -throwback modulehdr.cmhg -o modulehdr.o moduledefs.h

.SUFFIXES: .o .s .c

.c.o:
	$(CC) -c  $(CFLAGS) -o $@ $<

.s.o:
	$(CC) -c  $(CFLAGS) -o $@ $<


