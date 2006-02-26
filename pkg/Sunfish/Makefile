#
#    $Id$
#

all:
	mkdir -p Apps/Misc
	cp -r ../../!Sunfish Apps/Misc
	zip -r -9 -I ^.^.sunfish-pkg/zip Apps RiscPkg Sprites SysVars -x *CVS* *svn*



.PHONY: clean all


clean:
	-rm -rf ../../sunfish-pkg.zip Apps
