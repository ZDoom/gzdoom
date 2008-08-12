#%include "valgrind.inc"

		.section	.text

.globl ASM_PatchPitch
ASM_PatchPitch:
		movl 		dc_pitch(%rip), %ecx
		movl 		%ecx, pm+3(%rip)
		movl 		%ecx, vltpitch+3(%rip)
#		selfmod pm, vltpitch+6
		ret
		.align 16

.globl setupvlinetallasm
setupvlinetallasm:
		movb		%dil, shifter1+2(%rip)
		movb		%dil, shifter2+2(%rip)
		movb		%dil, shifter3+2(%rip)
		movb		%dil, shifter4+2(%rip)
#		selfmod shifter1, shifter4+3
		ret
		.align 16

		.section .rtext,"awx"

.globl vlinetallasm4
		.type		vlinetallasm4,@function
vlinetallasm4:
		.cfi_startproc
		push		%rbx
		push		%rdi
		push		%r15
		push		%r14
		push		%r13
		push		%r12
		push		%rbp
		push		%rsi
		subq		$8, %rsp	# Does the stack need to be 16-byte aligned for Linux?
		.cfi_adjust_cfa_offset	8

# rax =	bufplce base address
# rbx = 
# rcx = offset from rdi/count (negative)
# edx/rdx = scratch
# rdi = bottom of columns to write to
# r8d-r11d = column offsets
# r12-r15 = palookupoffse[0] - palookupoffse[4]

		movl		dc_count(%rip), %ecx
		movq		dc_dest(%rip), %rdi
		testl		%ecx, %ecx
		jle			vltepilog	# count must be positive

		movq		bufplce(%rip), %rax
		movq		bufplce+8(%rip), %r8
		subq		%rax, %r8
		movq		bufplce+16(%rip), %r9
		subq		%rax, %r9
		movq		bufplce+24(%rip), %r10
		subq		%rax, %r10
		movl		%r8d, source2+4(%rip)
		movl		%r9d, source3+4(%rip)
		movl		%r10d, source4+4(%rip)

pm:		imulq		$320, %rcx

		movq		palookupoffse(%rip), %r12
		movq		palookupoffse+8(%rip), %r13
		movq		palookupoffse+16(%rip), %r14
		movq		palookupoffse+24(%rip), %r15

		movl		vince(%rip), %r8d
		movl		vince+4(%rip), %r9d
		movl		vince+8(%rip), %r10d
		movl		vince+12(%rip), %r11d
		movl		%r8d, step1+3(%rip)
		movl		%r9d, step2+3(%rip)
		movl		%r10d, step3+3(%rip)
		movl		%r11d, step4+3(%rip)

		addq		%rcx, %rdi
		negq		%rcx

		movl		vplce(%rip), %r8d
		movl		vplce+4(%rip), %r9d
		movl		vplce+8(%rip), %r10d
		movl		vplce+12(%rip), %r11d
#		selfmod loopit, vltepilog
		jmp			loopit

		.align 16
loopit:
			movl	%r8d, %edx
shifter1:	shrl	$24, %edx
step1:		addl	$0x88888888, %r8d
			movzbl	(%rax,%rdx), %edx
			movl	%r9d, %ebx
			movb	(%r12,%rdx), %dl
shifter2:	shrl	$24, %ebx
step2:		addl	$0x88888888, %r9d
source2:	movzbl	0x88888888(%rax,%rbx), %ebx
			movl	%r10d, %ebp
			movb	(%r13,%rbx), %bl
shifter3:	shr		$24, %ebp
step3:		addl	$0x88888888, %r10d
source3:	movzbl	0x88888888(%rax,%rbp), %ebp
			movl	%r11d, %esi
			movb	(%r14,%rbp), %bpl
shifter4:	shr		$24, %esi
step4:		add		$0x88888888, %r11d
source4:	movzbl	0x88888888(%rax,%rsi), %esi
			movb	%dl, (%rdi,%rcx)
			movb	%bl, 1(%rdi,%rcx)
			movb	(%r15,%rsi), %sil
			movb	%bpl, 2(%rdi,%rcx)
			movb	%sil, 3(%rdi,%rcx)

vltpitch:	addq	$320, %rcx
			jl		loopit

		movl		%r8d, vplce(%rip)
		movl		%r9d, vplce+4(%rip)
		movl		%r10d, vplce+8(%rip)
		movl		%r11d, vplce+12(%rip)

vltepilog:
		addq		$8, %rsp
		.cfi_adjust_cfa_offset	-8
		pop			%rsi
		pop			%rbp
		pop			%r12
		pop			%r13
		pop			%r14
		pop			%r15
		pop			%rdi
		pop			%rbx
		ret
		.cfi_endproc
		.align 16


