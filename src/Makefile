#
#    $Id$
#

INCLUDE = -ITCPIPLibs: -IOSLibSupport: -IOSLib: -Ilibiconv:src -Iauto -I.

MODLIBS = TCPIPLIBS:o.unixlibzm TCPIPLIBS:o.inetlibzm TCPIPLibs:o.socklib5zm -l libiconv:libiconv

APPLIBS = -l TCPIPLIBS:o.unixlib TCPIPLIBS:o.inetlib -l TCPIPLibs:o.socklib5 -l OSLibSupport:o.OSLibSupport32 -l OSLib:o.OSLib32 -l libiconv:libiconv


ifneq (,$(findstring release,$(MAKECMDGOALS)))

CFLAGSN = -Otime
CFLAGSG = -O3
EXTRATARGET = release

else

CFLAGSN = -zpz0
CFLAGSG = -O0 -mpoke-function-name

endif

CXX = g++
CC = gcc
CFLAGS = $(INCLUDE) -Wall -mthrowback -std=c99 -pedantic  -mlibscl $(CFLAGSG)
CXXFLAGS = $(INCLUDE) -Irtk: -Wall -mthrowback -mlibscl $(CFLAGSG)

ifneq (,$(findstring gcc,$(MAKECMDGOALS)))

EXTRATARGET := $(EXTRATARGET) gcc

MODCC = gcc
MODCFLAGS =  $(INCLUDE) -DMODULE -Wall -mthrowback -mlibscl -std=c99 -pedantic $(CFLAGSG) 

MODULE = -mmodule

MODLINK = gcc -mlibscl -mmodule

CMHG = cmunge -32bit -tgcc

else

#LIBS := $(LIBS) C:o.stubs
MODLIBS := $(MODLIBS) C:o.stubs

MODCC = cc
MODCFLAGS = -Wp $(INCLUDE) -DMODULE -fah -throwback -zpq262144 -IC: $(CFLAGSN)

MODLINK = link -m 

MODULE = -zM

CMHG = cmhg

endif


VPATH = auto:.

ifneq (,$(findstring debug-server,$(MAKECMDGOALS)))

MODULE =
CFLAGS := $(CFLAGS) -DDEBUGSERVER

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

auto: ProcessSpec rpc-spec nfs2-spec nfs3-spec nfs4-spec mount1-spec mount3-spec portmapper-spec pcnfsd-spec @.cmhg.sunfish @.cmhg.moonfish
	mkdir -p auto
	mkdir -p o
	perl ProcessSpec rpc rpc-spec
	perl ProcessSpec mount1 mount1-spec
	perl ProcessSpec mount3 mount3-spec
	perl ProcessSpec nfs2 nfs2-spec
	perl ProcessSpec nfs3 nfs3-spec
	perl ProcessSpec nfs4 nfs4-spec
	perl ProcessSpec portmapper portmapper-spec
	perl ProcessSpec pcnfsd pcnfsd-spec
	$(CMHG) -throwback sunfish.cmhg -o sunfishhdr_m.o -d auto/sunfishdefs.h
	$(CMHG) -throwback moonfish.cmhg -o moonfishhdr_m.o -d auto/moonfishdefs.h
	touch auto



SUNFISHGENOBJS = \
pcnfsd-process1.o \
pcnfsd-process2.o \
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
nfs4-procs.o \
portmapper-decode.o \
portmapper-decode2.o \
mount1-decode.o \
mount1-decode2.o \
mount3-decode.o \
mount3-decode2.o \
nfs2-decode.o \
nfs2-decode2.o \
nfs3-decode.o \
nfs3-decode2.o \
nfs4-decode.o \
nfs4-decode2.o

COMMONGENOBJS = \
rpc-process1.o \
rpc-process2.o \
portmapper-process1.o \
portmapper-process2.o \
mount1-process1.o \
mount1-process2.o \
mount3-process1.o \
mount3-process2.o \
nfs2-process1.o \
nfs2-process2.o \
nfs3-process1.o \
nfs3-process2.o \
nfs4-process1.o \
nfs4-process2.o

SUNFISHDUALOBJS = \
imageentry_func.o \
imageentry_file.o \
imageentry_args.o \
imageentry_bytes.o \
imageentry_openclose.o \
imageentry_common.o 

ASMOBJS = \
base.o \
callback.o \
memcpy.o

COMMONOBJS = \
pools.o \
utils.o \
primitives.o

SUNFISHMODOBJS = \
$(SUNFISHGENOBJS:.o=_m.o) \
$(COMMONGENOBJS:.o=_m.o) \
$(SUNFISHDUALOBJS:.o=2_m.o) \
$(SUNFISHDUALOBJS:.o=3_m.o) \
$(ASMOBJS) \
$(COMMONOBJS:.o=_m.o) \
rpc_m.o \
sunfish_m.o \
imageentry_newimage_m.o \
sunfishhdr_m.o

SUNFISHAPPOBJS = \
$(SUNFISHGENOBJS) \
$(COMMONGENOBJS) \
$(ASMOBJS) \
$(COMMONOBJS) \
rpc.o \
browse.o \
tbelib.o \
sunfish-frontend.o

NEWFEAPPOBJS = \
$(SUNFISHGENOBJS) \
$(COMMONGENOBJS) \
$(ASMOBJS) \
$(COMMONOBJS) \
rpc.o \
browse.o \
newfe.o \
exportbrowser.o \
hostbrowser.o \
getuid.o \
mountchoices.o

MOONFISHOBJS = \
request-decode.o \
serverconn.o \
filecache.o \
clientid.o \
state.o \
exports.o

MOONFISHMODOBJS = \
$(MOONFISHGENOBJS:.o=_m.o) \
$(COMMONGENOBJS:.o=_m.o) \
$(ASMOBJS) \
$(COMMONOBJS:.o=_m.o) \
$(MOONFISHOBJS:.o=_m.o) \
moonfish_m.o \
moonfishhdr_m.o

MOONFISHAPPOBJS = \
moonfish-frontend.o

MOONFISHDEBUGOBJS = \
$(MOONFISHGENOBJS) \
$(COMMONGENOBJS) \
$(COMMONOBJS) \
$(MOONFISHOBJS) \
debug-server.o

$(SUNFISHMODOBJS) $(SUNFISHAPPOBJS): auto
$(MOONFISHMODOBJS) $(MOONFISHAPPOBJS): auto



../!Sunfish/!RunImage: $(SUNFISHAPPOBJS)
	$(CXX) $(CXXFLAGS) $(SUNFISHAPPOBJS) -o $@ $(APPLIBS)

newfe: $(NEWFEAPPOBJS)
	$(CXX) $(CXXFLAGS) $(NEWFEAPPOBJS)  -o $@ -lrtk:a.rtk $(APPLIBS)

../!Moonfish/!RunImage: $(MOONFISHAPPOBJS)
	$(CC) $(CFLAGS) $(MOONFISHAPPOBJS) -o $@ $(APPLIBS)

../!Sunfish/Sunfish: $(SUNFISHMODOBJS)
	$(MODLINK) -o Sunfish $(SUNFISHMODOBJS) $(MODLIBS)
	mv Sunfish ../!Sunfish/Sunfish

../!Moonfish/Moonfish: $(MOONFISHMODOBJS) 
	$(MODLINK) -o Moonfish $(MOONFISHMODOBJS) $(MODLIBS)
	mv Moonfish ../!Moonfish/Moonfish

debug-server: $(MOONFISHDEBUGOBJS)
	$(CC) $(CFLAGS) $(MOONFISHDEBUGOBJS) -o $@ $(APPLIBS)


%.o: %.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $<

%.o: %.s
	$(MODCC) -c  -o $@ $<

%_m.o: %.c
	$(MODCC) -c $(MODULE) $(MODCFLAGS) -o $@ $<

$(SUNFISHDUALOBJS:.o=2_m.o): %2_m.o: %.c
	$(MODCC) -c $(MODULE) $(MODCFLAGS) -o $@ $<

$(SUNFISHDUALOBJS:.o=3_m.o): %3_m.o: %.c
	$(MODCC) -c $(MODULE) $(MODCFLAGS) -DNFS3 -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	-rm -f ../!Sunfish/Sunfish
	-rm -f ../!Sunfish/!RunImage
	-rm -f ../!Moonfish/Moonfish
	-rm -f ../!Moonfish/!RunImage
	-rm -f debug-server
	-rm -f $(MOONFISHAPPOBJS) $(MOONFISHMODOBJS)
	-rm -f $(SUNFISHAPPOBJS) $(SUNFISHMODOBJS)
	-rm -f $(MOONFISHDEBUGOBJS)
	-rm -rf auto
