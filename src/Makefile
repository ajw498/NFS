#
#    $Id$
#    $URL$
#

INCLUDE = -ITCPIPLibs: -IOSLibSupport: -IOSLib: -Iauto -I.
MODLIBS = TCPIPLIBS:o.unixlibzm TCPIPLIBS:o.inetlibzm TCPIPLibs:o.socklib5zm

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

VPATH = auto:.

SUNFISHGENOBJS = \
portmapper-calls.o \
pcnfsd-calls.o \
mount1-calls.o \
mount3-calls.o \
nfs2-calls.o \
nfs3-calls.o

MOONFISHGENOBJS =  \
portmapper-procs.o \
mount1-procs.o \
nfs2-procs.o \
portmapper-decode.o \
mount1-decode.o \
nfs2-decode.o

SUNFISHOBJS = \
rpc.o \
sunfish.o

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
serverconn.o \
filecache.o \
exports.o

COMMONOBJS = \
pools.o \
utils.o

ifneq (,$(findstring debug-server,$(MAKECMDGOALS)))

MODULE =

endif

all: sunfish Moonfish


sunfish: ../!Sunfish/Sunfish ../!Sunfish/!RunImage

.PHONY: auto

auto: auto/stamp

auto/stamp: ProcessSpec rpc-spec nfs2-spec nfs3-spec mount1-spec mount3-spec portmapper-spec pcnfsd-spec @.cmhg.sunfish @.cmhg.moonfish
	mkdir -p auto
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
	touch auto/stamp



$(SUNFISHOBJS): auto $(SUNFISHOBJS:.o=.h)
$(SUNFISHGENOBJS): auto

$(MOONFISHOBJS): auto $(MOONFISHOBJS:.o=.h)
$(MOONFISHGENOBJS): auto 


../!Sunfish/!RunImage: frontend.c auto
	$(CC) $(CFLAGS) frontend.c -o ../!Sunfish/!RunImage $(LIBS)

../!Sunfish/Sunfish: $(SUNFISHOBJS) $(SUNFISHGENOBJS) $(SUNFISHDUALOBJS) $(ASMOBJS) $(COMMONOBJS)
	$(LINK) -o Sunfish $(SUNFISHOBJS) $(SUNFISHGENOBJS) $(SUNFISHDUALOBJS) $(ASMOBJS) $(COMMONOBJS) sunfishhdr.o $(MODLIBS)
	mv Sunfish ../!Sunfish/Sunfish



Moonfish: $(MOONFISHOBJS) $(MOONFISHGENOBJS) $(COMMONOBJS) moonfish.o
	$(LINK) -o Moonfish $(MOONFISHOBJS) $(MOONFISHGENOBJS) $(COMMONOBJS) moonfish.o moonfishhdr.o $(MODLIBS)

debug-server: $(MOONFISHOBJS) $(MOONFISHGENOBJS) debug-server.o
	$(CC) $(CFLAGS) $(MOONFISHOBJS) $(MOONFISHGENOBJS) $(COMMONOBJS) debug-server.o -o debug-server $(LIBS)



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

.PHONY: sunfish clean all

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
