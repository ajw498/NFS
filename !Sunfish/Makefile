#
#    $Id$
#    $URL$
#

INCLUDE = -IC:,TCPIPLibs:,OSLibSupport:,OSLib:
LIBS    = TCPIPLIBS:o.unixlibzm TCPIPLIBS:o.inetlibzm TCPIPLibs:o.socklib5zm C:o.stubs

CC = cc
CFLAGS = -Wp $(INCLUDE) -Otime -fah -throwback -zpq262144

LINK = link

all: Sunfish !RunImage

!RunImage: frontend.c moduledefs.h
	$(CC) $(CFLAGS) frontend.c -o !RunImage  -l TCPIPLIBS:o.unixlib,OSLibSupport:o.OSLibSupport32,OSLib:o.OSLib32,C:o.stubs

rpc-calls.c rpc-calls.h: rpc-spec ProcessSpec
	perl ProcessSpec rpc rpc-spec

nfs-calls.c nfs-calls.h: nfs-spec ProcessSpec
	perl ProcessSpec nfs nfs-spec

mount-calls.c mount-calls.h: mount-spec ProcessSpec
	perl ProcessSpec mount mount-spec

portmapper-calls.c portmapper-calls.h: portmapper-spec ProcessSpec
	perl ProcessSpec portmapper portmapper-spec

pcnfsd-calls.c pcnfsd-calls.h: pcnfsd-spec ProcessSpec
	perl ProcessSpec pcnfsd pcnfsd-spec

GENHDRS = \
rpc-calls.h \
nfs-calls.h \
mount-calls.h \
portmapper-calls.h \
pcnfsd-calls.h

GENOBJS = \
rpc-calls.o \
nfs-calls.o \
mount-calls.o \
portmapper-calls.o \
pcnfsd-calls.o

OBJS = \
rpc.o \
base.o \
callback.o \
module.o \
modulehdr.o \
imageentry_func.o \
imageentry_file.o \
imageentry_args.o \
imageentry_bytes.o \
imageentry_openclose.o \
imageentry_common.o

$(OBJS) $(GENOBJS): $(GENHDRS) imageentry_common.h moduledefs.h

Sunfish: $(OBJS) $(GENOBJS)
	link -m -o Sunfish $(OBJS) $(GENOBJS) $(LIBS)

module.o: moduledefs.h

moduledefs.h modulehdr.o: @.cmhg.modulehdr
	cmhg  -throwback modulehdr.cmhg -o modulehdr.o moduledefs.h

.SUFFIXES: .o .s .c

.c.o:
	$(CC) -c  -zM $(CFLAGS) -o $@ $<

.s.o:
	$(CC) -c  $(CFLAGS) -o $@ $<


clean:
	-remove Sunfish
	-remove !RunImage
	-remove h.moduledefs
	-wipe o.* ~CFR~V
	-wipe c.*-calls ~CFR~V
	-wipe h.*-calls ~CFR~V
	-wipe h.*-process* ~CFR~V
	-wipe h.*-structs ~CFR~V
