;* Miscellanoeus assembly functions. Many of these could have
;* easily been done using VC++ inline assembly, but that's not
;* nearly as portable as using NASM.

BITS 32

%ifndef M_TARGET_LINUX

%define	HaveRDTSC	_HaveRDTSC
%define HaveCMOV	_HaveCMOV
%define	CPUFamily	_CPUFamily
%define	CPUModel	_CPUModel
%define CPUStepping	_CPUStepping

%define FixedMul_ASM	_FixedMul_ASM
%define FixedDiv_ASM	_FixedDiv_ASM
%define CheckMMX	_CheckMMX
%define EndMMX		_EndMMX
%define DoBlending_MMX	_DoBlending_MMX
%define BestColor_MMX	_BestColor_MMX
%define DoubleHoriz_MMX _DoubleHoriz_MMX
%define DoubleHorizVert_MMX _DoubleHorizVert_MMX
%define DoubleVert_ASM	_DoubleVert_ASM

%endif

extern	HaveRDTSC
extern	HaveCMOV
extern	CPUFamily
extern	CPUModel
extern	CPUStepping

%ifdef M_TARGET_WATCOM
  SEGMENT DATA PUBLIC ALIGN=16 CLASS=DATA USE32
  SEGMENT DATA
%else
  SECTION .data
%endif

Blending256:
	dd	0x01000100,0x00000100

%ifdef M_TARGET_WATCOM
  SEGMENT CODE PUBLIC ALIGN=16 CLASS=CODE USE32
  SEGMENT CODE
%else
  SECTION .text
%endif

;-----------------------------------------------------------
;
; FixedMul_ASM
;
; Assembly version of FixedMul.
;
;-----------------------------------------------------------

GLOBAL	FixedMul_ASM

	align	16

FixedMul_ASM:
	mov	eax,[esp+4]
	imul	dword [esp+8]
	shrd	eax,edx,16
	ret

; Version of FixedMul_ASM for MSVC's __fastcall

GLOBAL @FixedMul_ASM@8

	align	16

@FixedMul_ASM@8:
	mov	eax,ecx
	imul	edx
	shrd	eax,edx,16
	ret

;-----------------------------------------------------------
;
; FixedDiv_ASM
;
; This is mostly what VC++ outputted for the original C
; routine (which used to contain inline assembler
; but doesn't anymore).
;
;-----------------------------------------------------------

GLOBAL	FixedDiv_ASM

	align	16

FixedDiv_ASM:
	push	ebp
	push	ebx

; 89   : 	if ((abs (a) >> 14) >= abs(b))

	mov	edx,[esp+12]		; get a
	 mov	eax,[esp+16]		; get b
	mov	ecx,edx
	 mov	ebx,eax
	sar	ecx,31
	 mov	ebp,edx
	sar	ebx,31
	 xor	ebp,ecx
	xor	eax,ebx
	 sub	ebp,ecx			; ebp is now abs(a)
	sub	eax,ebx			; eax is now abs(b)

	sar	ebp,14
	 pop	ebx
	cmp	ebp,eax
	 jl	.L206

; 90   : 		return (a^b)<0 ? MININT : MAXINT;

	xor	edx,[esp+12]
	 xor	eax,eax
	pop	ebp
	 test	edx,edx
	setl	al
	add	eax,0x7fffffff
	ret

	align	16

.L206:
	sar	edx,16			; (edx = ----aaaa)
	 mov	eax,[esp+8]		; (eax = aaaaAAAA)
	shl	eax,16			; (eax = AAAA0000)
	 pop	ebp
	idiv	dword [esp+8]

	ret

;-----------------------------------------------------------
;
; CheckMMX
;
; Checks for the presence of MMX instructions on the
; current processor. This code is adapted from the samples
; in AMD's document entitled "AMD-K6™ MMX Processor
; Multimedia Extensions." Also fills in the vendor
; information string.
;
;-----------------------------------------------------------

GLOBAL	CheckMMX

; boolean CheckMMX (char *vendorid)

CheckMMX:
	xor	eax,eax
	push	ebx
	push	eax		; Will be set true if MMX is present

	pushfd			; save EFLAGS
	pop	eax		; store EFLAGS in EAX
	mov	ebx,eax		; save in EBX for later testing
	xor	eax,0x00200000	; toggle bit 21
	push	eax		; put to stack
	popfd			; save changed EAX to EFLAGS
	pushfd			; push EFLAGS to TOS
	pop	eax		; store EFLAGS in EAX
	cmp	eax,ebx		; see if bit 21 has changed
	jz	.nommx		; if no change, no CPUID

	mov	eax,0		; setup function 0
	CPUID			; call the function
	mov	eax,[esp+12]	; fill in the vendorid string
	mov	[eax],ebx
	mov	[eax+4],edx
	mov	[eax+8],ecx

	xor	eax,eax
	mov	eax,1		; setup function 1
	CPUID			; call the function
	and	ah,0xf
	mov	[CPUFamily],ah
	mov	ah,al
	shr	ah,4
	and	al,0xf
	mov	[CPUModel],ah
	mov	[CPUStepping],al
	xor	eax,eax
	test	edx,0x00000010	; test for RDTSC
	setnz	al		; al=1 if RDTSC is available
	mov	[HaveRDTSC],eax
	test	edx,0x00001000	; test for CMOV
	setnz	al		; al=1 if CMOV is available
	mov	[HaveCMOV],eax

	test	edx,0x00800000	; test 23rd bit
	jz	.nommx

	inc	dword [esp]
.nommx	pop	eax
	pop	ebx
	ret

;-----------------------------------------------------------
;
; EndMMX
;
; Signal the end of MMX code
;
;-----------------------------------------------------------

GLOBAL	EndMMX

EndMMX:
	emms
	ret

;-----------------------------------------------------------
;
; DoBlending_MMX
;
; MMX version of DoBlending
;
; (DWORD *from, DWORD *to, count, tor, tog, tob, toa)
;-----------------------------------------------------------

GLOBAL	DoBlending_MMX

DoBlending_MMX:
	pxor		mm0,mm0		; mm0 = 0
	mov		eax,[esp+4*4]
	shl		eax,16
	mov		edx,[esp+4*5]
	shl		edx,8
	or		eax,[esp+4*6]
	or		eax,edx
	mov		ecx,[esp+4*3]	; ecx = count
	movd		mm1,eax		; mm1 = 00000000 00RRGGBB
	mov		eax,[esp+4*7]
	shl		eax,16
	mov		edx,[esp+4*7]
	shl		edx,8
	or		eax,[esp+4*7]
	or		eax,edx
	mov		edx,[esp+4*2]	; edx = dest
	movd		mm6,eax		; mm6 = 00000000 00AAAAAA
	punpcklbw	mm1,mm0		; mm1 = 000000RR 00GG00BB
	movq		mm7,[Blending256]
	punpcklbw	mm6,mm0		; mm6 = 000000AA 00AA00AA
	mov		eax,[esp+4*1]	; eax = source
	pmullw		mm1,mm6		; mm1 = 000000RR 00GG00BB (multiplied by alpha)
	psubusw		mm7,mm6		; mm7 = 000000aa 00aa00aa (one minus alpha)
	nop	; Does this actually pair on a Pentium?

; Do two colors per iteration: Count must be even.

.loop	movq		mm2,[eax]	; mm2 = 00r2g2b2 00r1g1b1
	add		eax,8
	movq		mm3,mm2		; mm3 = 00r2g2b2 00r1g1b1
	punpcklbw	mm2,mm0		; mm2 = 000000r1 00g100b1
	movq		mm4,mm1
	punpckhbw	mm3,mm0		; mm3 = 000000r2 00g200b2
	pmullw		mm2,mm7		; mm2 = 0000r1rr g1ggb1bb
	add		edx,8
	pmullw		mm3,mm7		; mm3 = 0000r2rr g2ggb2bb
	sub		ecx,2
	paddusw		mm2,mm1
	paddusw		mm3,mm1
	psrlw		mm2,8
	psrlw		mm3,8
	packuswb	mm2,mm3		; mm2 = 00r2g2b2 00r1g1b1
	movq		[edx-8],mm2
	jnz		.loop

	emms
	ret

;-----------------------------------------------------------
;
; BestColor_MMX
;
; Picks the closest matching color from a palette
;
; Passed FFRRGGBB and palette array in same format
; FF is the index of the first palette entry to consider
;
;-----------------------------------------------------------

GLOBAL	BestColor_MMX
GLOBAL	@BestColor_MMX@8

BestColor_MMX:
	mov		ecx,[esp+4]
	mov		edx,[esp+8]
@BestColor_MMX@8:
	pxor		mm0,mm0
	mov		eax,ecx
	shr		ecx,24		; ecx = count
	and		ebx,0xffffff
	movd		mm1,eax		; mm1 = color searching for
	mov		eax,257*257+257*257+257*257	;eax = bestdist
	push		ebx
	punpcklbw	mm1,mm0
	mov		ebx,ecx		; ebx = best color
	push		esi
	push		ebp

.loop	movd		mm2,[edx+ecx*4]	; mm2 = color considering now
	inc		ecx
	punpcklbw	mm2,mm0
	movq		mm3,mm1
	psubsw		mm3,mm2
	pmullw		mm3,mm3		; mm3 = color distance squared

	movd		ebp,mm3		; add the three components
	psrlq		mm3,32		; into ebp to get the real
	mov		esi,ebp		; (squared) distance
	shr		esi,16
	and		ebp,0xffff
	add		ebp,esi
	movd		esi,mm3
	add		ebp,esi

	jz		.perf		; found a perfect match
	cmp		eax,ebp
	jb		.skip
	mov		eax,ebp
	lea		ebx,[ecx-1]
.skip	cmp		ecx,256
	jne		.loop
	mov		eax,ebx
	pop		ebp
	pop		esi
	pop		ebx
	emms
	ret

.perf	lea		eax,[ecx-1]
	pop		ebp
	pop		esi
	pop		ebx
	emms
	ret

;-----------------------------------------------------------
;
; DoubleHoriz_MMX
;
; Stretches an image horizontally using MMX instructions.
; The source image is assumed to occupy the right half
; of the destination image.
;
;	height of source
;	width of source
;	dest pointer (at end of row)
;	pitch
;
;-----------------------------------------------------------

GLOBAL	DoubleHoriz_MMX

DoubleHoriz_MMX:
	mov	edx,[esp+8]	; edx = width
	push	edi

	neg	edx		; make edx negative so we can count up
	mov	edi,[esp+16]	; edi = dest pointer

	sar	edx,2		; and make edx count groups of 4 pixels
	push	ebp

	mov	ebp,edx		; ebp = # of columns remaining in this row
	push	ebx

	mov	ebx,[esp+28]	; ebx = pitch
	mov	ecx,[esp+16]	; ecx = # of rows remaining

.loop	movq		mm0,[edi+ebp*4]

.loop2	movq		mm1,mm0
	punpcklbw	mm0,mm0			; double left 4 pixels

	movq		mm2,[edi+ebp*4+8]
	punpckhbw	mm1,mm1			; double right 4 pixels

	movq		[edi+ebp*8],mm0		; write left pixels
	movq		mm0,mm2

	movq		[edi+ebp*8+8],mm1	; write right pixels

	add		ebp,2			; increment counter
	jnz		.loop2			; repeat until done with this row


	add	edi,ebx		; move edi to next row
	dec	ecx		; decrease row counter

	mov	ebp,edx		; prep ebp for next row
	jnz	.loop		; repeat until every row is done

	emms
	pop	ebx
	pop	ebp
	pop	edi
	ret

;-----------------------------------------------------------
;
; DoubleHorizVert_MMX
;
; Stretches an image horizontally and vertically using
; MMX instructions. The source image is assumed to occupy
; the right half of the destination image and to leave
; every other line unused for expansion.
;
;	height of source
;	width of source
;	dest pointer (at end of row)
;	pitch
;
;-----------------------------------------------------------

GLOBAL	DoubleHorizVert_MMX

DoubleHorizVert_MMX:
	mov	edx,[esp+8]	; edx = width
	push	edi

	neg	edx		; make edx negative so we can count up
	mov	edi,[esp+16]	; edi = dest pointer

	sar	edx,2		; and make edx count groups of 4 pixels
	push	ebp

	mov	ebp,edx		; ebp = # of columns remaining in this row
	push	ebx

	mov	ebx,[esp+28]	; ebx = pitch
	mov	ecx,[esp+16]	; ecx = # of rows remaining

	push	esi
	lea	esi,[edi+ebx]

.loop	movq		mm0,[edi+ebp*4]		; get 8 pixels

	movq		mm1,mm0
	punpcklbw	mm0,mm0			; double left 4

	punpckhbw	mm1,mm1			; double right 4
	add		ebp,2			; increment counter

	movq		[edi+ebp*8-16],mm0	; write them back out

	movq		[edi+ebp*8-8],mm1

	movq		[esi+ebp*8-16],mm0

	movq		[esi+ebp*8-8],mm1

	jnz		.loop			; repeat until done with this row

	lea	edi,[edi+ebx*2]	; move edi and esi to next row
	lea	esi,[esi+ebx*2]

	dec	ecx		; decrease row counter
	mov	ebp,edx		; prep ebp for next row

	jnz	.loop		; repeat until every row is done

	emms
	pop	esi
	pop	ebx
	pop	ebp
	pop	edi
	ret

;-----------------------------------------------------------
;
; DoubleVert_ASM
;
; Stretches an image vertically using regular x86
; instructions. The source image should be interleaved.
;
;	height of source
;	width of source
;	source/dest pointer
;	pitch
;
;-----------------------------------------------------------

GLOBAL	DoubleVert_ASM

DoubleVert_ASM:
	mov	edx,[esp+16]	; edx = pitch
	mov	eax,[esp+4]	; eax = # of rows left

	push	esi
	mov	esi,[esp+16]

	push	edi
	lea	edi,[esi+edx]

	shl	edx,1		; edx = pitch*2
	mov	ecx,[esp+16]

	sub	edx,ecx		; edx = dist from end of one line to start of next
	shr	ecx,2

.loop	rep movsd
	
	mov	ecx,[esp+16]
	add	esi,edx

	add	edi,edx
	shr	ecx,2

	dec	eax
	jnz	.loop

	pop	edi
	pop	esi
	ret
