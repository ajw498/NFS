#
#    $Id$
#    $URL$
#

INCLUDE = -ITCPIPLibs: -IOSLibSupport: -IOSLib: -Iauto
MODLIBS = TCPIPLIBS:o.unixlibzm TCPIPLIBS:o.inetlibzm TCPIPLibs:o.socklib5zm

LIBS = -l TCPIPLIBS:o.unixlib TCPIPLIBS:o.inetlib -l TCPIPLibs:o.socklib5 -l OSLibSupport:o.OSLibSupport32 -l OSLib:o.OSLib32

ifeq ($(BUILD),gcc)

CC = gcc
CFLAGS =  $(INCLUDE) -Wall -mthrowback -mlibscl -std=c99 -O3 -pedantic 

MODULEFLAG = -mmodule

LINK = gcc -mlibscl -mmodule

else

LIBS := $(LIBS) C:o.stubs
MODLIBS := $(MODLIBS) C:o.stubs

CC = cc
#CFLAGS = -Wp $(INCLUDE) -Otime -fah -throwback -zpq262144 -IC:
CFLAGS = -Wp $(INCLUDE) -fah -throwback -zpz0 -zpq262144 -IC:

LINK = drlink -m 

MODULEFLAG = -zM

endif


SUNFISHGENOBJS = \
auto/portmapper-calls.o \
auto/pcnfsd-calls.o \
auto/mount1-calls.o \
auto/mount3-calls.o \
auto/nfs2-calls.o \
auto/nfs3-calls.o

MOONFISHGENOBJS =  \
portmapper-procs.o \
mount1-procs.o \
nfs2-procs.o \
auto/portmapper-decode.o \
auto/mount1-decode.o \
auto/nfs2-decode.o

SUNFISHOBJS = \
rpc.o \
sunfish.o \
utils.o

SUNFISHDUALOBJS = \
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

ASMOBJS = \
base.o \
callback.o

MOONFISHOBJS = \
request-decode.o \
pools.o \
serverconn.o \
exports.o

COMMONOBJ =

ifeq ($(SERVER),debug)

MODULE =

all: debug-server

else

MODULE = $(MODULEFLAG)

all: sunfish Moonfish

endif

sunfish: ../!Sunfish/Sunfish ../!Sunfish/!RunImage


auto: ProcessSpec rpc-spec nfs2-spec nfs3-spec mount1-spec mount3-spec portmapper-spec pcnfsd-spec @.cmhg.sunfish @.cmhg.moonfish
	mkdir -p auto/o
	mkdir -p o
	perl ProcessSpec rpc rpc-spec
	perl ProcessSpec nfs2 nfs2-spec
	perl ProcessSpec mount1 mount1-spec
	perl ProcessSpec nfs3 nfs3-spec
	perl ProcessSpec mount3 mount3-spec
	perl ProcessSpec portmapper portmapper-spec
	perl ProcessSpec pcnfsd pcnfsd-spec
	cmhg  -throwback sunfish.cmhg -o sunfishhdr.o auto/sunfishdefs.h
	cmhg  -throwback moonfish.cmhg -o moonfishhdr.o auto/moonfishdefs.h
	touch auto




$(SUNFISHOBJS): auto $(SUNFISHOBJS:.o=.c) $(SUNFISHOBJS:.o=.h)
$(SUNFISHGENOBJS): auto

$(MOONFISHOBJS): auto $(MOONFISHOBJS:.o=.c) $(MOONFISHOBJS:.o=.h)
$(MOONFISHGENOBJS): auto

../!Sunfish/!RunImage: frontend.c auto
	$(CC) $(CFLAGS) frontend.c -o ../!Sunfish/!RunImage $(LIBS)

../!Sunfish/Sunfish: $(SUNFISHOBJS) $(SUNFISHGENOBJS) $(SUNFISHDUALOBJS) $(ASMOBJS)
	$(LINK) -o ../!Sunfish/Sunfish $(SUNFISHOBJS) $(SUNFISHGENOBJS) $(SUNFISHDUALOBJS) $(ASMOBJS) sunfishhdr.o $(MODLIBS)

Moonfish: $(MOONFISHOBJS) $(MOONFISHGENOBJS) moonfish.o
	$(LINK) -o Moonfish $(MOONFISHOBJS) $(MOONFISHGENOBJS) moonfish.o moonfishhdr.o $(MODLIBS)

debug-server: $(MOONFISHOBJS) $(MOONFISHGENOBJS) debug-server.o
	$(CC) $(CFLAGS) $(MOONFISHOBJS) $(MOONFISHGENOBJS) debug-server.o -o debug-server $(LIBS)



imageentry_func2.o: imageentry_func.c imageentry_func.h
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_func.c
imageentry_file2.o: imageentry_file.c imageentry_file.h
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_file.c
imageentry_args2.o: imageentry_args.c imageentry_args.h
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_args.c
imageentry_bytes2.o: imageentry_bytes.c imageentry_bytes.h
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_bytes.c
imageentry_openclose2.o: imageentry_openclose.c imageentry_openclose.h
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_openclose.c
imageentry_common2.o: imageentry_common.c imageentry_common.h
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ imageentry_common.c
imageentry_func3.o: imageentry_func.c imageentry_func.h
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_func.c
imageentry_file3.o: imageentry_file.c imageentry_file.h
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_file.c
imageentry_args3.o: imageentry_args.c imageentry_args.h
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_args.c
imageentry_bytes3.o: imageentry_bytes.c imageentry_bytes.h
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_bytes.c
imageentry_openclose3.o: imageentry_openclose.c imageentry_openclose.h
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_openclose.c
imageentry_common3.o: imageentry_common.c imageentry_common.h
	$(CC) -c  $(MODULE) $(CFLAGS) -DNFS3 -o $@ imageentry_common.c

.SUFFIXES: .o .s .c

.c.o:
	$(CC) -c  $(MODULE) $(CFLAGS) -o $@ $<

.s.o:
	$(CC) -c  $(CFLAGS) -o $@ $<

.PHONY: sunfish

clean:
	-rm -f ../!Sunfish/Sunfish
	-rm -f ../!Sunfish/!RunImage
	-rm -f $(MOONFISHOBJS) $(MOONFISHGENOBJS)
	-rm -f debug-server.o moonfish.o moonfishhdr.o
	-rm -f $(SUNFISHOBJS) $(SUNFISHGENOBJS) $(SUNFISHDUALOBJS)
	-rm -f $(ASMOBJS) sunfishhdr.o frontend.o
	-rm -rf auto
	-rm -f Moonfish
	-rm -f debug-server
