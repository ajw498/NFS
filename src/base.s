;
;    $Id$
;    $URL$
;
;    Export the base address of the module in a format that can be used from C
;

	AREA	|C$$Data|, DATA, READONLY, REL
	IMPORT	|Image$$RO$$Base|
	EXPORT	|module_base_address|
|module_base_address|	DCD	|Image$$RO$$Base|

	END
