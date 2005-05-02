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


GENHDRS = \
rpc-calls.h \
nfs3-calls.h \
mount3-calls.h \
nfs2-calls.h \
mount1-calls.h \
portmapper-calls.h \
pcnfsd-calls.h

GENOBJS = \
rpc-calls.o \
mount3-calls.o \
mount1-calls.o \
nfs2-calls.o \
portmapper-calls.o \
pcnfsd-calls.o


OBJS = \
rpc.o \
base.o \
callback.o \
module.o \
modulehdr.o \
utils.o \
imageentry_newimage.o \
imageentry_func2.o \
imageentry_file2.o \
imageentry_args2.o \
imageentry_bytes2.o \
imageentry_openclose2.o \
imageentry_common2.o \
imageentry_func3.o \
imageentry_file3.o \
imageentry_args3.o \
imageentry_bytes3.o \
imageentry_openclose3.o \
imageentry_common3.o

$(OBJS) $(GENOBJS): $(GENHDRS) utils.h moduledefs.h

Sunfish: $(OBJS) $(GENOBJS)
	link -m -o Sunfish $(OBJS) $(GENOBJS) nfs3-calls.o $(LIBS)

module.o: moduledefs.h

moduledefs.h modulehdr.o: @.cmhg.modulehdr
	cmhg  -throwback modulehdr.cmhg -o modulehdr.o moduledefs.h

imageentry_func2.o: imageentry_func.c
	$(CC) -c  -zM $(CFLAGS) -o $@ imageentry_func.c
imageentry_file2.o: imageentry_file.c
	$(CC) -c  -zM $(CFLAGS) -o $@ imageentry_file.c
imageentry_args2.o: imageentry_args.c
	$(CC) -c  -zM $(CFLAGS) -o $@ imageentry_args.c
imageentry_bytes2.o: imageentry_bytes.c
	$(CC) -c  -zM $(CFLAGS) -o $@ imageentry_bytes.c
imageentry_openclose2.o: imageentry_openclose.c
	$(CC) -c  -zM $(CFLAGS) -o $@ imageentry_openclose.c
imageentry_common2.o: imageentry_common.c
	$(CC) -c  -zM $(CFLAGS) -o $@ imageentry_common.c
imageentry_func3.o: imageentry_func.c
	$(CC) -c  -zM $(CFLAGS) -DNFS3 -o $@ imageentry_func.c
imageentry_file3.o: imageentry_file.c
	$(CC) -c  -zM $(CFLAGS) -DNFS3 -o $@ imageentry_file.c
imageentry_args3.o: imageentry_args.c
	$(CC) -c  -zM $(CFLAGS) -DNFS3 -o $@ imageentry_args.c
imageentry_bytes3.o: imageentry_bytes.c
	$(CC) -c  -zM $(CFLAGS) -DNFS3 -o $@ imageentry_bytes.c
imageentry_openclose3.o: imageentry_openclose.c
	$(CC) -c  -zM $(CFLAGS) -DNFS3 -o $@ imageentry_openclose.c
imageentry_common3.o: imageentry_common.c
	$(CC) -c  -zM $(CFLAGS) -DNFS3 -o $@ imageentry_common.c

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
