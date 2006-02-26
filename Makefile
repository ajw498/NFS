#
#    $Id$
#


all sunfish moonfish:
	$(MAKE) -C src $(MAKECMDGOALS)

release: clean
	zip -r -9 -I source/zip !Sunfish !Moonfish COPYING Makefile Regression src pkg -x *CVS* *svn*
	$(MAKE) -C src
	zip -r -9 -I sunfish/zip !Sunfish COPYING -x *CVS* *svn*
	zip -r -9 -I moonfish/zip !Moonfish COPYING -x *CVS* *svn*
	$(MAKE) -C pkg

gcc:


.PHONY: sunfish moonfish clean all release gcc


clean:
	-rm -f sunfish.zip moonfish.zip source.zip
	-rm -f sunfish-pkg.zip moonfish-pkg.zip
	-rm -rf pkg/Sunfish/Apps pkg/Moonfish/Apps
	-rm -f pkg/Sunfish/RiscPkg/Control pkg/Moonfish/RiscPkg/Control
	$(MAKE) -C src clean
	$(MAKE) -C pkg clean
