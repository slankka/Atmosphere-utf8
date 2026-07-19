/**
 * @file cache.s
 * @brief AArch64 cache operations.
 */

.macro CACHE_BEGIN name
	.section .text.\name, "ax", %progbits
	.global \name
	.type \name, %function
	.align 2
	.cfi_startproc
\name:
.endm

.macro CACHE_END
	.cfi_endproc
.endm

CACHE_BEGIN armDCacheFlush
	ADD X1, X1, X0
	MRS X8, CTR_EL0
	LSR X8, X8, #16
	AND X8, X8, #0xF
	MOV X9, #4
	LSL X9, X9, X8
	SUB X10, X9, #1
	BIC X8, X0, X10
	MOV X10, X1
	MOV W1, #1
	MRS X0, TPIDRRO_EL0
	STRB W1, [X0, #0x104]
1:	DC CIVAC, X8
	ADD X8, X8, X9
	CMP X8, X10
	B.CC 1b
	DSB SY
	STRB WZR, [X0, #0x104]
	RET
CACHE_END

CACHE_BEGIN armDCacheClean
	ADD X1, X1, X0
	MRS X8, CTR_EL0
	LSR X8, X8, #16
	AND X8, X8, #0xF
	MOV X9, #4
	LSL X9, X9, X8
	SUB X10, X9, #1
	BIC X8, X0, X10
	MOV X10, X1
	MOV W1, #1
	MRS X0, TPIDRRO_EL0
	STRB W1, [X0, #0x104]
1:	DC CVAC, X8
	ADD X8, X8, X9
	CMP X8, X10
	B.CC 1b
	DSB SY
	STRB WZR, [X0, #0x104]
	RET
CACHE_END

CACHE_BEGIN armICacheInvalidate
	ADD X1, X1, X0
	MRS X8, CTR_EL0
	AND X8, X8, #0xF
	MOV X9, #4
	LSL X9, X9, X8
	SUB X10, X9, #1
	BIC X8, X0, X10
	MOV X10, X1
	MOV W1, #1
	MRS X0, TPIDRRO_EL0
	STRB W1, [X0, #0x104]
1:	IC IVAU, X8
	ADD X8, X8, X9
	CMP X8, X10
	B.CC 1b
	DSB SY
	STRB WZR, [X0, #0x104]
	RET
CACHE_END

CACHE_BEGIN armDCacheZero
	ADD X1, X1, X0
	MRS X8, CTR_EL0
	LSR X8, X8, #16
	AND X8, X8, #0xF
	MOV X9, #4
	LSL X9, X9, X8
	SUB X10, X9, #1
	BIC X8, X0, X10
	MOV X10, X1
	MOV W1, #1
	MRS X0, TPIDRRO_EL0
	STRB W1, [X0, #0x104]
1:	DC ZVA, X8
	ADD X8, X8, X9
	CMP X8, X10
	B.CC 1b
	DSB SY
	STRB WZR, [X0, #0x104]
	RET
CACHE_END
