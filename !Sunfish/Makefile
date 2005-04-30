#
#    $Id$
#    $URL$
#

INCLUDE = -IC:,TCPIPLibs:,OSLibSupport:,OSLib:
LIBS    = TCPIPLIBS:o.unixlibzm TCPIPLIBS:o.inetlibzm TCPIPLibs:o.socklib5zm C:o.stubs

CC = cc
#CFLAGS = -Wp $(INCLUDE) -Otime -fah -throwback -zpq262144
CFLAGS = -Wp $(INCLUDE) -fah -throwback -zpq262144

LINK = link

all: Sunfish !RunImage

!RunImage: frontend.c moduledefs.h
	$(CC) $(CFLAGS) frontend.c -o !RunImage  -l TCPIPLIBS:o.unixlib,OSLibSupport:o.OSLibSupport32,OSLib:o.OSLib32,C:o.stubs

NFSVERSION = 3
MOUNTVERSION = 3
DEF = -DNFS3

rpc-calls.c rpc-calls.h: rpc-spec ProcessSpec
	perl ProcessSpec rpc rpc-spec

nfs2-calls.c nfs2-calls.h: nfs2-spec ProcessSpec
	perl ProcessSpec nfs2 nfs2-spec

mount1-calls.c mount1-calls.h: mount1-spec ProcessSpec
	perl ProcessSpec mount1 mount1-spec

nfs3-calls.c nfs3-calls.h: nfs3-spec ProcessSpec
	perl ProcessSpec nfs3 nfs3-spec

mount3-calls.c mount3-calls.h: mount3-spec ProcessSpec
	perl ProcessSpec mount3 mount3-spec

portmapper-calls.c portmapper-calls.h: portmapper-spec ProcessSpec
	perl ProcessSpec portmapper portmapper-spec

pcnfsd-calls.c pcnfsd-calls.h: pcnfsd-spec ProcessSpec
	perl ProcessSpec pcnfsd pcnfsd-spec

# ${VAR:search=replace}

GENHDRS = \
rpc-calls.h \
nfs$(NFSVERSION)-calls.h \
mount$(MOUNTVERSION)-calls.h \
portmapper-calls.h \
pcnfsd-calls.h

GENOBJS = \
rpc-calls.o \
nfs$(NFSVERSION)-calls.o \
mount$(MOUNTVERSION)-calls.o \
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
	$(CC) -c  -zM $(CFLAGS) $(DEF) -o $@ $<

.s.o:
	$(CC) -c  $(CFLAGS) -o $@ $<

.INIT:
	cdir o

clean:
	-remove Sunfish
	-remove !RunImage
	-remove h.moduledefs
	-wipe o.* ~CFR~V
	-wipe c.*-calls ~CFR~V
	-wipe h.*-calls ~CFR~V
	-wipe h.*-process* ~CFR~V
	-wipe h.*-structs ~CFR~V
