#
#    $Id$
#    $URL$
#

INCLUDE = -ITCPIPLibs: -IOSLibSupport: -IOSLib:
MODLIBS = -l TCPIPLIBS:o.unixlibzm -l TCPIPLIBS:o.inetlibzm -l TCPIPLibs:o.socklib5zm

LIBS = -l TCPIPLIBS:o.unixlib TCPIPLIBS:o.inetlib -l TCPIPLibs:o.socklib5 -l OSLibSupport:o.OSLibSupport32 -l OSLib:o.OSLib32

ifeq ($(BUILD),gcc)

CC = gcc
CFLAGS =  $(INCLUDE) -Wall -mthrowback -mlibscl -std=c99 -O3 -pedantic 

MODULE = -mmodule

LINK = gcc -mlibscl -mmodule

else

LIBS := $(LIBS) C:o.stubs
MODLIBS := $(MODLIBS) C:o.stubs

CC = cc
#CFLAGS = -Wp $(INCLUDE) -Otime -fah -throwback -zpq262144 -IC:
CFLAGS = -Wp $(INCLUDE) -fah -throwback -zpz0 -zpq262144 -IC:

LINK = link -m 

MODULE = -zM

endif


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
nfs3-calls.o \
portmapper-calls.o \
pcnfsd-calls.o


all: Sunfish !RunImage

!RunImage: frontend.c moduledefs.h
	$(CC) $(CFLAGS) frontend.c -o !RunImage  $(LIBS)

DEBUGSERVEROBJ = debug-server.o rpc-decode.o portmapper-procs.o mount1-procs.o mount3-procs.o portmapper-recv.o mount1-recv.o mount3-recv.o nfs2-recv.o nfs2-procs.o pools.o

debug-server: $(DEBUGSERVEROBJ)
	$(CC) $(CFLAGS) $(DEBUGSERVEROBJ) -o debug-server $(LIBS)

debug-server.o: debug-server.c $(GENHDRS)
	$(CC) $(CFLAGS) -c debug-server.c -o $@

rpc-decode.o: rpc-decode.c
	$(CC) $(CFLAGS) -c rpc-decode.c -o $@

pools.o: pools.c
	$(CC) $(CFLAGS) -c pools.c -o $@

portmapper-procs.o: portmapper-procs.c portmapper-procs.h
	$(CC) $(CFLAGS) -c portmapper-procs.c -o $@

mount3-procs.o: mount3-procs.c mount3-procs.h
	$(CC) $(CFLAGS) -c mount3-procs.c -o $@

mount1-procs.o: mount1-procs.c mount1-procs.h
	$(CC) $(CFLAGS) -c mount1-procs.c -o $@

nfs2-procs.o: nfs2-procs.c nfs2-procs.h
	$(CC) $(CFLAGS) -c nfs2-procs.c -o $@

portmapper-recv.o: portmapper-recv.c
	$(CC) $(CFLAGS) -c portmapper-recv.c -o $@
mount1-recv.o: mount1-recv.c
	$(CC) $(CFLAGS) -c mount1-recv.c -o $@
mount3-recv.o: mount3-recv.c
	$(CC) $(CFLAGS) -c mount3-recv.c -o $@
nfs2-recv.o: nfs2-recv.c
	$(CC) $(CFLAGS) -c nfs2-recv.c -o $@

rpc-calls.c rpc-calls.h: rpc-spec ProcessSpec
	perl ProcessSpec rpc rpc-spec

nfs2-calls.c nfs2-calls.h nfs2-recv.c nfs2-recv.h: nfs2-spec ProcessSpec
	perl ProcessSpec nfs2 nfs2-spec

mount1-calls.c mount1-calls.h mount1-recv.c mount1-recv.h: mount1-spec ProcessSpec
	perl ProcessSpec mount1 mount1-spec

nfs3-calls.c nfs3-calls.h nfs3-recv.c nfs3-recv.h: nfs3-spec ProcessSpec
	perl ProcessSpec nfs3 nfs3-spec

mount3-calls.c mount3-calls.h mount3-recv.c mount3-recv.h: mount3-spec ProcessSpec
	perl ProcessSpec mount3 mount3-spec

portmapper-calls.c portmapper-calls.h portmapper-recv.c portmapper-recv.h: portmapper-spec ProcessSpec
	perl ProcessSpec portmapper portmapper-spec

pcnfsd-calls.c pcnfsd-calls.h: pcnfsd-spec ProcessSpec
	perl ProcessSpec pcnfsd pcnfsd-spec




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
	$(LINK) -o Sunfish $(OBJS) $(GENOBJS) $(MODLIBS)

module.o: moduledefs.h

moduledefs.h modulehdr.o: @.cmhg.modulehdr
	cmhg  -throwback modulehdr.cmhg -o modulehdr.o moduledefs.h

imageentry_func2.o: imageentry_func.c
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_func.c
imageentry_file2.o: imageentry_file.c
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_file.c
imageentry_args2.o: imageentry_args.c
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_args.c
imageentry_bytes2.o: imageentry_bytes.c
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_bytes.c
imageentry_openclose2.o: imageentry_openclose.c
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_openclose.c
imageentry_common2.o: imageentry_common.c
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_common.c
imageentry_func3.o: imageentry_func.c
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_func.c
imageentry_file3.o: imageentry_file.c
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_file.c
imageentry_args3.o: imageentry_args.c
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_args.c
imageentry_bytes3.o: imageentry_bytes.c
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_bytes.c
imageentry_openclose3.o: imageentry_openclose.c
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_openclose.c
imageentry_common3.o: imageentry_common.c
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_common.c

.SUFFIXES: .o .s .c

.c.o:
	$(CC) -c  $(MODULE) $(CFLAGS) $(DEF) -o $@ $<

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
	-wipe h.*-procs ~CFR~V
	-wipe h.*-recv ~CFR~V
	-wipe c.*-recv ~CFR~V
