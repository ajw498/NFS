;
;    $Id$
;
;    Wrapper for calls from the free module
;    This is needed because the routine is called in user mode with
;    no usable stack.
;

XBit			EQU	&20000
XSunfish_Free		EQU	&58bc3 + XBit

	AREA	|C$$Code|, CODE, READONLY, REL
	EXPORT	|free_wrapper|
|free_wrapper|
	CMP	a1, #3
	TEQEQ	a1, a1	; Set Z
	SWINE	XSunfish_Free
	LDMIA	sp!, {pc}

	END
