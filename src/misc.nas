;* Miscellanoeus assembly functions. Many of these could have
;* easily been done using VC++ inline assembly, but that's not
;* nearly as portable as using NASM.

BITS 32

%ifdef M_TARGET_LINUX

extern	HaveRDTSC
extern	CPUFamily
extern	CPUModel
extern	CPUStepping

%else

extern	_HaveRDTSC
extern	_CPUFamily
extern	_CPUModel
extern	_CPUStepping

%define	HaveRDTSC	_HaveRDTSC
%define	CPUFamily	_CPUFamily
%define	CPUModel	_CPUModel
%define CPUStepping	_CPUStepping

%endif

%ifdef M_TARGET_WATCOM
  SEGMENT DATA PUBLIC ALIGN=16 CLASS=DATA USE32
  SEGMENT DATA
%else
  SECTION .data
%endif


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
GLOBAL	_FixedMul_ASM

	align	16

FixedMul_ASM:
_FixedMul_ASM:
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
GLOBAL	_FixedDiv_ASM

	align	16

FixedDiv_ASM:
_FixedDiv_ASM:
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
GLOBAL	_CheckMMX

; boolean CheckMMX (char *vendorid)

CheckMMX:
_CheckMMX:
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
GLOBAL	_EndMMX

EndMMX:
_EndMMX:
	emms
	ret


;-----------------------------------------------------------
;
; PrintChar1P
;
; Copies one conchar from source to the screen at dest.
;
;-----------------------------------------------------------

GLOBAL	PrintChar1P
GLOBAL	_PrintChar1P

; PrintChar1P (byte *char, byte *dest, int screenpitch);

PrintChar1P:
_PrintChar1P:
	push	esi
	 push	edi
	push	ebp

	 mov	ebp,[esp+6*4]
	mov	esi,[esp+4*4]
	 xor	ecx,ecx
	mov	edi,[esp+5*4]
	 mov	cl,8

.loop	mov	eax,[edi]		; Get first 4 pixels
	 mov	edx,[edi+4]		; Get last 4 pixels
	and	eax,[esi+8]		; Mask first 4 pixels
	 and	edx,[esi+12]		; Mask last 4 pixels
	db	0x33,0x46,0x00		; xor eax,[esi+0] : Draw first 4 pixels (K6 optimization)
	 xor	edx,[esi+4]		; Draw last 4 pixels
	mov	[edi],eax		; Save first 4 pixels
	 mov	[edi+4],edx		; Save last 4 pixels

	add	edi,ebp
	 add	esi,16
	dec	ecx
	 jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	ret

;-----------------------------------------------------------
;
; PrintChar2P_MMX
;
; Copies one conchar from source to the screen at dest,
; doubling it in size (MMX version).
;
;-----------------------------------------------------------

GLOBAL	PrintChar2P_MMX
GLOBAL	_PrintChar2P_MMX

; PrintChar2P_MMX (byte *char, byte *dest, int screenpitch);

PrintChar2P_MMX:
_PrintChar2P_MMX:
	push	esi
	push	edi

	xor	ecx,ecx
	mov	esi,[esp+3*4]
	mov	edx,[esp+5*4]
	mov	edi,[esp+4*4]
	mov	eax,edx
	mov	cl,8
	add	eax,eax

; Note that this doesn't pair as nicely as I thought it did.
; Oh well. I have a K6, so I don't really care. And since this
; code is never called, it doesn't much matter on a Pentium, either.

.loop	movq		mm0,[esi+8]	; Get mask
	db	0x0f,0x6f,0x56,0x00	; movq mm2,[esi+0] : Get conchar bits
	movq		mm1,mm0		; Copy mask
	movq		mm3,mm2		; Copy conchar bits
	punpckhbw	mm0,mm0		; Expand last 4 pixels of mask
	punpcklbw	mm1,mm1		; Expand first 4 pixels of mask
	punpckhbw	mm2,mm2		; Expand last 4 pixels of conchar bits
	punpcklbw	mm3,mm3		; Expand first 4 pixels of conchar bits
	movq		mm4,mm0		; Copy last part of mask
	movq		mm5,mm1		; Copy first part of mask
	pand		mm1,[edi]	; Mask out first 8 bytes of this line
	pand		mm5,[edi+edx]	; Mask out first 8 bytes of next line
	pand		mm0,[edi+8]	; Mask out last 8 bytes of this line
	pand		mm4,[edi+edx+8]	; Mask out last 8 bytes of next line
	pxor		mm1,mm3		; Plot first 8 bytes of this line
	pxor		mm5,mm3		; Plot first 8 bytes of next line
	pxor		mm0,mm2		; Plot last 8 bytes of this line
	pxor		mm4,mm2		; Plot last 8 bytes of next line
	add		esi,16		; Advance conchar pointer to next line
	dec		ecx
	movq		[edi],mm1	; Save first 8 bytes of this line
	movq		[edi+edx],mm5	; Save first 8 bytes of next line
	movq		[edi+8],mm0	; Save last 8 bytes of this line
	movq		[edi+edx+8],mm4	; Save last 8 bytes of next line

	lea	edi,[edi+eax]		; Advance dest pointer two lines
	jnz	.loop

	pop	edi
	pop	esi

	ret
