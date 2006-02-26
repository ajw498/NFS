#
#    $Id$
#


all:
	perl CreatePackage
	$(MAKE) -C Sunfish
	$(MAKE) -C Moonfish


.PHONY: clean all


clean:
	-rm -f Sunfish/RiscPkg/Control Moonfish/RiscPkg/Control
	$(MAKE) -C Sunfish clean
	$(MAKE) -C Moonfish clean
