#
#    $Id$
#

all:
	mkdir -p Apps/Disc
	cp -r ../../!Sunfish Apps/Disc
	zip -r -9 -I ^.^.sunfish-pkg/zip Apps RiscPkg Sprites SysVars -x *CVS* *svn*



.PHONY: clean all


clean:
	-rm -rf ../../sunfish-pkg.zip Apps
