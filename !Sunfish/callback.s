;
;    $Id$
;    $URL$
;
;    Allow any pending callbacks to trigger by calling a SWI from user mode
;    This is unfortunatly necessary because the internet stack will not always
;    notice that new data has arrived otherwise.
;

XBit			EQU	&20000
XOS_EnterOS		EQU	&16 + XBit
XOS_GenerateError	EQU	&2B + XBit

	AREA	|C$$Code|, CODE, READONLY, REL
	EXPORT	|trigger_callback|
|trigger_callback|
	STMFD sp!,{lr}

	TEQ	a1, a1	; Set Z
	TEQ	pc, pc	; EQ if 32-bit mode
	TEQNEP	pc, #0
	MRSEQ	a1, CPSR	; Acts a NOP for TEQP
	BICEQ	a1, a1, #&f	; Preserve 32bit mode bit
	MSREQ	CPSR_c, a1
	MOV	a1, a1		; Avoid StrongARM MSR bug

	SWI	XOS_GenerateError	; Force callback
	SWI	XOS_EnterOS		; Back from whence we came
	LDMFD	sp!, {pc}

	END
