/**
 * @file svc.s
 * @brief SVC wrapper assembly.
 */

.macro SVC_BEGIN name
	.section .text.\name, "ax", %progbits
	.global \name
	.type \name, %function
	.align 2
	.cfi_startproc
\name:
.endm

.macro SVC_END
	.cfi_endproc
.endm

SVC_BEGIN svcSetHeapSize
	SVC 0x1
	RET
SVC_END

SVC_BEGIN svcSetMemoryPermission
	SVC 0x2
	RET
SVC_END

SVC_BEGIN svcSetMemoryAttribute
	SVC 0x3
	RET
SVC_END

SVC_BEGIN svcMapMemory
	SVC 0x4
	RET
SVC_END

SVC_BEGIN svcUnmapMemory
	SVC 0x5
	RET
SVC_END

SVC_BEGIN svcQueryMemory
	STR X1, [SP, #-16]!
	SVC 0x6
	LDR X2, [SP], #16
	STR W1, [X2]
	RET
SVC_END

SVC_BEGIN svcExitProcess
	SVC 0x7
	RET
SVC_END

SVC_BEGIN svcCreateThread
	STR X0, [SP, #-16]!
	SVC 0x8
	LDR X2, [SP], #16
	STR W1, [X2]
	RET
SVC_END

SVC_BEGIN svcStartThread
	SVC 0x9
	RET
SVC_END

SVC_BEGIN svcExitThread
	SVC 0xA
	RET
SVC_END

SVC_BEGIN svcSleepThread
	SVC 0xB
	RET
SVC_END

SVC_BEGIN svcCloseHandle
	SVC 0x16
	RET
SVC_END

SVC_BEGIN svcWaitSynchronization
	STR X0, [SP, #-16]!
	SVC 0x18
	LDR X2, [SP], #16
	STR W1, [X2]
	RET
SVC_END

SVC_BEGIN svcCreateSession
	STP X0, X1, [SP, #-16]!
	SVC 0x40
	LDP X3, X4, [SP], #16
	STR W1, [X3]
	STR W2, [X4]
	RET
SVC_END

SVC_BEGIN svcAcceptSession
	SVC 0x41
	RET
SVC_END

SVC_BEGIN svcReplyAndReceive
	STR X0, [SP, #-16]!
	SVC 0x43
	LDR X2, [SP], #16
	STR W1, [X2]
	RET
SVC_END

SVC_BEGIN svcSendSyncRequest
	SVC 0x21
	RET
SVC_END

SVC_BEGIN svcCreateEvent
	STR X0, [SP, #-16]!
	SVC 0x45
	LDR X2, [SP], #16
	STR W1, [X2]
	STR W0, [X2, #4]
	RET
SVC_END

SVC_BEGIN svcMapProcessMemory
	SVC 0x74
	RET
SVC_END

SVC_BEGIN svcUnmapProcessMemory
	SVC 0x75
	RET
SVC_END

SVC_BEGIN svcSetProcessMemoryPermission
	SVC 0x73
	RET
SVC_END

SVC_BEGIN svcCallSecureMonitor
	STR X0, [SP, #-16]!
	MOV X8, X0
	LDP X0, X1, [X8]
	LDP X2, X3, [X8, #0x10]
	LDP X4, X5, [X8, #0x20]
	LDP X6, X7, [X8, #0x30]
	SVC 0x7F
	LDR X8, [SP], #16
	STP X0, X1, [X8]
	STP X2, X3, [X8, #0x10]
	STP X4, X5, [X8, #0x20]
	STP X6, X7, [X8, #0x30]
	RET
SVC_END

SVC_BEGIN svcGetCurrentProcessorNumber
	MRS X0, TPIDRRO_EL0
	AND X0, X0, #3
	RET
SVC_END

SVC_BEGIN armGetTls
	MRS X0, TPIDRRO_EL0
	RET
SVC_END
