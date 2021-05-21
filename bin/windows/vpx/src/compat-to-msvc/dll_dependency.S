/* Implementation for gcc's internal stack-allocation routines.  */
.global ___chkstk
.global __alloca

.global ___chkstk_ms
___chkstk_ms:
#ifdef _WIN64
	pushq	%rax
	pushq	%rcx
	cmpq	$0x1000, %rax
	leaq	24(%rsp), %rcx
	jb	.Lchkstk_ms_end
.Lchkstk_ms_loop:
	subq	$0x1000, %rcx
	subq	$0x1000, %rax
	orq	$0x0, (%rcx)
	cmpq	$0x1000, %rax
	ja	.Lchkstk_ms_loop
.Lchkstk_ms_end:
	subq	%rax, %rcx
	orq	$0x0, (%rcx)
	popq	%rcx
	popq	%rax
	ret
#else
	pushl	%eax
	pushl	%ecx
	cmpl	$0x1000, %eax
	leal	12(%esp), %ecx
	jb	chkstk_ms_end
chkstk_ms_loop:
	subl	$0x1000, %ecx
	subl	$0x1000, %eax
	orl	$0x0, (%ecx)
	cmpl	$0x1000, %eax
	ja	chkstk_ms_loop
chkstk_ms_end:
	subl	%eax, %ecx
	orl	$0x0, (%ecx)
	popl	%ecx
	popl	%eax
	ret
#endif

#ifdef _WIN64
__alloca:
	movq	%rcx, %rax
.align	4
___chkstk:
	popq	%r11
	movq	%rsp, %r10
	cmpq	$0x1000, %rax
	jb	.Lchkstk_end
.Lchkstk_loop:
	subq	$0x1000, %r10
	subq	$0x1000, %rax
	orl	$0x0, (%r10)
	cmpq	$0x1000, %rax
	ja	.Lchkstk_loop
.Lchkstk_end:
	subq	%rax, %r10
	movq	%rsp, %rax
	orl	$0x0, (%r10)
	movq	%r10, %rsp
	pushq	%r11
	ret
#else
___chkstk:
__alloca:
	pushl	%ecx
	leal	8(%esp), %ecx
	cmpl	$0x1000, %eax		/* > 4k ?*/
	jb	chkstk_end
chkstk_loop:
	subl	$0x1000, %ecx
	subl	$0x1000, %eax
	orl	$0x0, (%ecx)
	cmpl	$0x1000, %eax
	ja	chkstk_loop
chkstk_end:
	subl	%eax, %ecx
	orl	$0x0, (%ecx)
	movl	%esp, %eax
	movl	%ecx, %esp
	movl	(%eax), %ecx
	pushl	4(%eax)
	ret
#endif
