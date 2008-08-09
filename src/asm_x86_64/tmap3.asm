%include "valgrind.inc"

BITS 64
DEFAULT REL

%ifnidn __OUTPUT_FORMAT__,win64

%macro PROC_FRAME 1
%1:
%endmacro

%macro rex_push_reg 1
	push %1
%endmacro

%macro push_reg 1
	push %1
%endmacro

%macro alloc_stack 1
	sub rsp,%1
%endmacro

%define parm1lo		dil

%else

%define parm1lo		cl

%endif

SECTION .data

EXTERN vplce
EXTERN vince
EXTERN palookupoffse
EXTERN bufplce

EXTERN dc_count
EXTERN dc_dest
EXTERN dc_pitch

SECTION .text

ALIGN 16
GLOBAL ASM_PatchPitch
ASM_PatchPitch:
	mov ecx, [dc_pitch]
	mov [pm+3], ecx
	mov	[vltpitch+3], ecx
	selfmod pm, vltpitch+6
	ret

ALIGN 16
GLOBAL setupvlinetallasm
setupvlinetallasm:
	mov	[shifter1+2], parm1lo
	mov	[shifter2+2], parm1lo
	mov	[shifter3+2], parm1lo
	mov	[shifter4+2], parm1lo
	selfmod shifter1, shifter4+3
	ret

%ifidn __OUTPUT_FORMAT__,win64
; Yasm can't do progbits alloc exec for win64?
; Hmm, looks like it's automatic. No worries, then.
	SECTION .rtext	write ;progbits alloc exec
%else
	SECTION .rtext	progbits alloc exec write
%endif

ALIGN 16

GLOBAL vlinetallasm4
PROC_FRAME vlinetallasm4
	rex_push_reg	rbx
	push_reg		rdi
	push_reg		r15
	push_reg		r14
	push_reg		r13
	push_reg		r12
	push_reg		rbp
	push_reg		rsi
	alloc_stack		8		; Stack must be 16-byte aligned
END_PROLOGUE
; rax =	bufplce base address
; rbx = 
; rcx = offset from rdi/count (negative)
; edx/rdx = scratch
; rdi = bottom of columns to write to
; r8d-r11d = column offsets
; r12-r15 = palookupoffse[0] - palookupoffse[4]

	mov		ecx, [dc_count]
	mov		rdi, [dc_dest]
	test	ecx, ecx
	jle		vltepilog		; count must be positive

	mov		rax, [bufplce]
	mov		r8,  [bufplce+8]
	sub		r8,  rax
	mov		r9,  [bufplce+16]
	sub		r9,  rax
	mov		r10, [bufplce+24]
	sub		r10, rax
	mov		[source2+4], r8d
	mov		[source3+4], r9d
	mov		[source4+4], r10d

pm:	imul	rcx, 320

	mov		r12, [palookupoffse]
	mov		r13, [palookupoffse+8]
	mov		r14, [palookupoffse+16]
	mov		r15, [palookupoffse+24]

	mov		r8d,  [vince]
	mov		r9d,  [vince+4]
	mov		r10d, [vince+8]
	mov		r11d, [vince+12]
	mov		[step1+3], r8d
	mov		[step2+3], r9d
	mov		[step3+3], r10d
	mov		[step4+3], r11d

	add		rdi, rcx
	neg		rcx

	mov		r8d,  [vplce]
	mov		r9d,  [vplce+4]
	mov		r10d, [vplce+8]
	mov		r11d, [vplce+12]
	selfmod loopit, vltepilog
	jmp		loopit

ALIGN	16
loopit:
			mov		edx, r8d
shifter1:	shr		edx, 24
step1:		add		r8d, 0x88888888
			movzx	rdx, BYTE [rax+rdx]
			mov		ebx, r9d
			mov		dl, [r12+rdx]
shifter2:	shr		ebx, 24
step2:		add		r9d, 0x88888888
source2:	movzx	ebx, BYTE [rax+rbx+0x88888888]
			mov		ebp, r10d
			mov		bl, [r13+rbx]
shifter3:	shr		ebp, 24
step3:		add		r10d, 0x88888888
source3:	movzx	ebp, BYTE [rax+rbp+0x88888888]
			mov		esi, r11d
			mov		bpl, BYTE [r14+rbp]
shifter4:	shr		esi, 24
step4:		add		r11d, 0x88888888
source4:	movzx	esi, BYTE [rax+rsi+0x88888888]
			mov		[rdi+rcx], dl
			mov		[rdi+rcx+1], bl
			mov		sil, BYTE [r15+rsi]
			mov		[rdi+rcx+2], bpl
			mov		[rdi+rcx+3], sil

vltpitch:	add		rcx, 320
			jl		loopit

	mov		[vplce], r8d
	mov		[vplce+4], r9d
	mov		[vplce+8], r10d
	mov		[vplce+12], r11d

vltepilog:
	add		rsp, 8
	pop		rsi
	pop		rbp
	pop		r12
	pop		r13
	pop		r14
	pop		r15
	pop		rdi
	pop		rbx
	ret
ENDPROC_FRAME
