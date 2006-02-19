#
#    $Id$
#    $URL$
#

INCLUDE = -ITCPIPLibs: -IOSLibSupport: -IOSLib: -Iauto -I.
MODLIBS = TCPIPLIBS:o.unixlibzm TCPIPLIBS:o.inetlibzm TCPIPLibs:o.socklib5zm

LIBS = -l TCPIPLIBS:o.unixlib TCPIPLIBS:o.inetlib -l TCPIPLibs:o.socklib5 -l OSLibSupport:o.OSLibSupport32 -l OSLib:o.OSLib32

ifneq (,$(findstring release,$(MAKECMDGOALS)))

CFLAGSN = -Otime
CFLAGSG = -O3
EXTRATARGET = release

else

CFLAGSN = -zpz0
CFLAGSG = -O0

endif


ifneq (,$(findstring gcc,$(MAKECMDGOALS)))

EXTRATARGET := $(EXTRATARGET) gcc

CC = gcc
CFLAGS =  $(INCLUDE) -Wall -mthrowback -mlibscl -std=c99 -pedantic $(CFLAGSG) 

MODULE = -mmodule

LINK = gcc -mlibscl -mmodule

CMHG = cmunge -32bit -tgcc
else

LIBS := $(LIBS) C:o.stubs
MODLIBS := $(MODLIBS) C:o.stubs

CC = cc
CFLAGS = -Wp $(INCLUDE) -fah -throwback -zpq262144 -IC: $(CFLAGSN)

LINK = link -m 

MODULE = -zM

CMHG = cmhg

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
mount3-procs.o \
nfs2-procs.o \
nfs3-procs.o \
portmapper-decode.o \
portmapper-decode2.o \
mount1-decode.o \
mount1-decode2.o \
mount3-decode.o \
mount3-decode2.o \
nfs2-decode.o \
nfs2-decode2.o \
nfs3-decode.o \
nfs3-decode2.o

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


all: sunfish-stage1 moonfish-stage1

sunfish: sunfish-stage1

moonfish: moonfish-stage1

debug: auto
	$(MAKE) debug-server $(EXTRATARGET)

release:

gcc:

ifeq (gcc,$(MAKECMDGOALS))
gcc: all
endif
ifeq (,$(findstring stage2,$(MAKECMDGOALS)))
release: all
endif

.PHONY: sunfish sunfish-stage1 sunfish-stage2 moonfish moonfish-stage1 moonfish-stage2 clean all release gcc debug

sunfish-stage1: auto
	$(MAKE) sunfish-stage2 $(EXTRATARGET)

moonfish-stage1: auto
	$(MAKE) moonfish-stage2 $(EXTRATARGET)

sunfish-stage2: ../!Sunfish/Sunfish ../!Sunfish/!RunImage

moonfish-stage2: ../!Moonfish/Moonfish ../!Moonfish/!RunImage

auto: ProcessSpec rpc-spec nfs2-spec nfs3-spec mount1-spec mount3-spec portmapper-spec pcnfsd-spec @.cmhg.sunfish @.cmhg.moonfish
	mkdir -p auto
	mkdir -p o
	perl ProcessSpec rpc rpc-spec
	perl ProcessSpec nfs2 nfs2-spec
	perl ProcessSpec mount1 mount1-spec
	perl ProcessSpec nfs3 nfs3-spec
	perl ProcessSpec mount3 mount3-spec
	perl ProcessSpec portmapper portmapper-spec
	perl ProcessSpec pcnfsd pcnfsd-spec
	$(CMHG) -throwback sunfish.cmhg -o sunfishhdr.o -d auto/sunfishdefs.h
	$(CMHG) -throwback moonfish.cmhg -o moonfishhdr.o -d auto/moonfishdefs.h
	touch auto



$(SUNFISHOBJS): auto $(SUNFISHOBJS:.o=.h)
$(SUNFISHGENOBJS): auto

$(MOONFISHOBJS): auto $(MOONFISHOBJS:.o=.h)
$(MOONFISHGENOBJS): auto 


../!Sunfish/!RunImage: sunfish-frontend.c auto
	$(CC) $(CFLAGS) sunfish-frontend.c -o ../!Sunfish/!RunImage $(LIBS)

../!Sunfish/Sunfish: $(SUNFISHOBJS) $(SUNFISHGENOBJS) $(SUNFISHDUALOBJS) $(ASMOBJS) $(COMMONOBJS)
	$(LINK) -o Sunfish $(SUNFISHOBJS) $(SUNFISHGENOBJS) $(SUNFISHDUALOBJS) $(ASMOBJS) $(COMMONOBJS) sunfishhdr.o $(MODLIBS)
	mv Sunfish ../!Sunfish/Sunfish

../!Moonfish/!RunImage: moonfish-frontend.c auto
	$(CC) $(CFLAGS) moonfish-frontend.c -o ../!Moonfish/!RunImage $(LIBS)


../!Moonfish/Moonfish: $(MOONFISHOBJS) $(MOONFISHGENOBJS) $(COMMONOBJS) moonfish.o
	$(LINK) -o Moonfish $(MOONFISHOBJS) $(MOONFISHGENOBJS) $(COMMONOBJS) moonfish.o moonfishhdr.o $(MODLIBS)
	mv Moonfish ../!Moonfish/Moonfish

debug-server: $(MOONFISHOBJS) $(MOONFISHGENOBJS) $(COMMONOBJS) debug-server.o
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


clean:
	-rm -f ../!Sunfish/Sunfish
	-rm -f ../!Sunfish/!RunImage
	-rm -f ../!Moonfish/Moonfish
	-rm -f ../!Moonfish/!RunImage
	-rm -f $(MOONFISHOBJS) $(MOONFISHGENOBJS)
	-rm -f $(SUNFISHOBJS) $(SUNFISHGENOBJS) $(SUNFISHDUALOBJS)
	-rm -f $(ASMOBJS) $(COMMONOBJS) 
	-rm -f moonfishhdr.o moonfish.o sunfishhdr.o
	-rm -f moonfish-frontend.o sunfish-frontend.o
	-rm -rf auto
	-rm -f debug-server debug-server.o
