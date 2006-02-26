#
#    $Id$
#

all:
	mkdir -p Apps/Misc
	cp -r ../../!Moonfish Apps/Misc
	zip -r -9 -I ^.^.moonfish-pkg/zip Apps RiscPkg Sprites SysVars -x *CVS* *svn*



.PHONY: clean all


clean:
	-rm -rf ../../moonfish-pkg.zip Apps
