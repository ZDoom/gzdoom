; The Visual C++ CRT unconditionally links to IsDebuggerPresent.
; This function is not available under Windows 95, so here is a
; lowlevel replacement for it.

extern __imp__GetModuleHandleA@4
extern __imp__GetProcAddress@8

	SECTION .data

global __imp__IsDebuggerPresent@0
__imp__IsDebuggerPresent@0:
	dd	IsDebuggerPresent_Check

	SECTION	.rdata

StrKERNEL32:
	db "KERNEL32",0

StrIsDebuggerPresent:
	db "IsDebuggerPresent",0

	SECTION .text

IsDebuggerPresent_No:
	xor		eax,eax
	ret

IsDebuggerPresent_Check:
	push	StrKERNEL32
	call	[__imp__GetModuleHandleA@4]
	push	StrIsDebuggerPresent
	push	eax
	call	[__imp__GetProcAddress@8]
	test	eax,eax
	jne		near .itis
	mov		eax,IsDebuggerPresent_No
.itis:
	mov		[__imp__IsDebuggerPresent@0],eax
	jmp		eax
