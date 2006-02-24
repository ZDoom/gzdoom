;*
;* blocks.nas
;* Draws simple blocks to the screen, possibly with masking
;*
;*---------------------------------------------------------------------------
;* Copyright 1998-2001 Randy Heit
;* All rights reserved.
;*
;* Redistribution and use in source and binary forms, with or without
;* modification, are permitted provided that the following conditions
;* are met:
;*
;* 1. Redistributions of source code must retain the above copyright
;*    notice, this list of conditions and the following disclaimer.
;* 2. Redistributions in binary form must reproduce the above copyright
;*    notice, this list of conditions and the following disclaimer in the
;*    documentation and/or other materials provided with the distribution.
;* 3. The name of the author may not be used to endorse or promote products
;*    derived from this software without specific prior written permission.
;*
;* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
;* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
;* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
;* IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
;* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
;* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
;* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
;* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
;* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
;* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;*---------------------------------------------------------------------------
;*

BITS 32

%define SPACEFILLER4 (0x44444444)

%ifndef M_TARGET_LINUX

%define RGB32k _RGB32k
%define Col2RGB8 _Col2RGB8

; Key to naming (because more descriptive names mess up the linker, sadly):
; 1st char: D = Draw, S = Scale
; 2nd char: M = Masked
; 3rd char: P = Plain, T = Translucent, S = Shadowed, A = Alpha
; 4th char: R = Remapped, U = Unmapped
; 5th char: P = Palettized output

%define DMPRP _DMPRP
%define DMPUP _DMPUP
%define DMTRP _DMTRP
%define DMTUP _DMTUP
%define DMSRP _DMSRP
%define DMSUP _DMSUP
%define DMAUP _DMAUP

%define SMPUP _SMPUP
%define SMPRP _SMPRP
%define SMTRP _SMTRP
%define SMTUP _SMTUP
%define SMSRP _SMSRP
%define SMSUP _SMSUP
%define SMAUP _SMAUP

%define MaskedBlockFunctions _MaskedBlockFunctions

%endif

%ifdef M_TARGET_WATCOM
  SEGMENT DATA PUBLIC ALIGN=16 CLASS=DATA USE32
  SEGMENT DATA
%else
  SECTION .data
%endif

GLOBAL	MaskedBlockFunctions

; Get around linker problems
MaskedBlockFunctions:
	dd	DMPRP
	dd	DMPUP
	dd	SMPRP
	dd	SMPUP
	dd	DMTRP
	dd	DMTUP
	dd	SMTRP
	dd	SMTUP
	dd	DMSRP
	dd	DMSUP
	dd	SMSRP
	dd	SMSUP
	dd	DMAUP
	dd	SMAUP

EXTERN	RGB32k
EXTERN	Col2RGB8

%ifdef M_TARGET_WATCOM
  SEGMENT CODE PUBLIC ALIGN=16 CLASS=CODE USE32
  SEGMENT CODE
%else
  SECTION .text
%endif

;__declspec(naked) void STACK_ARGS DMPRP (
;	const byte *src, byte *dest, const byte *remap, int srcpitch, int destpitch, int width, int height)

GLOBAL	DMPRP

	align 16

DMPRP:
	push	ebx
	push	edi
	push	ebp

%assign STACKBASE 4*4
%assign src	(4*0+STACKBASE)
%assign dest	(4*1+STACKBASE)
%assign remap	(4*2+STACKBASE)
%assign srcp	(4*3+STACKBASE)
%assign destp	(4*4+STACKBASE)
%assign width	(4*5+STACKBASE)
%assign height	(4*6+STACKBASE)

	mov	ebx,[esp+width]
	mov	ecx,[esp+src]
	xor	eax,eax
	mov	edx,[esp+dest]
	add	ecx,ebx
	add	edx,ebx
	neg	ebx
	mov	ebp,[esp+remap]
	mov	[esp+width],ebx
	mov	edi,[esp+height]

.loop	mov	al,[ecx+ebx]
	cmp	eax,255
	jz	.skip

	mov	al,[eax+ebp]
	mov	[edx+ebx],al

.skip	inc	ebx
	jnz	.loop

	add	ecx,[esp+srcp]
	add	edx,[esp+destp]
	mov	ebx,[esp+width]
	dec	edi
	jnz	.loop
	pop	ebp
	pop	edi
	pop	ebx
	ret

;__declspec(naked) void STACK_ARGS DMPUP (
;	const byte *src, byte *dest, int srcpitch, int destpitch, int _width, int _height)

	align 16

GLOBAL	DMPUP

DMPUP:
	mov	ecx,[esp+4*0+4]
	mov	edx,[esp+4*1+4]
	push	ebx
	mov	ebx,[esp+4*4+8]
	add	ecx,ebx
	add	edx,ebx
	xor	eax,eax
	neg	ebx
	mov	[esp+4*4+8],ebx
	push	ebp
	mov	ebp,[esp+4*5+12]

.loop	mov	al,[ecx+ebx]
	cmp	al,255
	jz	.skip

	mov	[edx+ebx],al

.skip	inc	ebx
	jnz	.loop

	add	ecx,[esp+4*2+12]
	add	edx,[esp+4*3+12]
	mov	ebx,[esp+4*4+12]
	dec	ebp
	jnz	.loop
	pop	ebp
	pop	ebx
	ret

;__declspec(naked) void STACK_ARGS DMTRP (
;	src, dest, remap, srcpitch, destpitch, _width, _height, fg2rgb, bg2rgb);

GLOBAL	DMTRP

	align 16

DMTRP:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	esi,[esp+0*4+20]	; esi = source
	mov	ebp,[esp+5*4+20]	; ebp = index
	mov	edi,[esp+1*4+20]	; edi = dest
	add	esi,ebp
	add	edi,ebp
	neg	ebp
	mov	[esp+5*4+20],ebp
	xor	eax,eax
	mov	ebx,[esp+2*4+20]	; ebx = remap
	mov	ecx,[esp+7*4+20]	; ecx = fg2rgb and scratch
	mov	edx,[esp+8*4+20]	; edx = bg2rgb and scratch

.loop	mov	al,[esi+ebp]
	cmp	eax,255
	jz	.skip

	mov	al,[ebx+eax]		; remap foreground
	mov	ecx,[ecx+eax*4]		; get foreground RGB
	mov	al,[edi+ebp]		; get background
	add	ecx,[edx+eax*4]		; add background RGB
	or	ecx,0x1f07c1f
	mov	edx,ecx
	shr	edx,15
	and	edx,ecx
	mov	ecx,[esp+7*4+20]	; set ecx back to fg2rgb
	mov	al,[RGB32k+edx]
	mov	edx,[esp+8*4+20]	; set edx back to bg2rgb
	mov	[edi+ebp],al

.skip	inc	ebp
	jnz	.loop

	add	esi,[esp+3*4+20]
	add	edi,[esp+4*4+20]
	mov	ebp,[esp+5*4+20]
	dec	dword [esp+6*4+20]
	jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

;__declspec(naked) void STACK_ARGS DMTUP (
;	src, dest, srcpitch, destpitch, _width, _height, fg2rgb, bg2rgb);

GLOBAL	DMTUP

	align 16

DMTUP:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	esi,[esp+0*4+20]	; esi = source
	mov	ebp,[esp+4*4+20]	; ebp = index
	mov	edi,[esp+1*4+20]	; edi = dest
	add	esi,ebp
	add	edi,ebp
	neg	ebp
	mov	[esp+4*4+20],ebp
	xor	eax,eax
	mov	ebx,[esp+5*4+20]	; ebx = height remaining
	mov	ecx,[esp+6*4+20]	; ecx = fg2rgb and scratch
	mov	edx,[esp+7*4+20]	; edx = bg2rgb and scratch

.loop	mov	al,[esi+ebp]
	cmp	eax,255
	jz	.skip

	mov	ecx,[ecx+eax*4]		; get foreground RGB
	mov	al,[edi+ebp]
	add	ecx,[edx+eax*4]		; add background RGB
	or	ecx,0x1f07c1f
	mov	edx,ecx
	shr	edx,15
	and	edx,ecx
	mov	ecx,[esp+6*4+20]	; set ecx back to fg2rgb
	mov	al,[RGB32k+edx]
	mov	edx,[esp+7*4+20]	; set edx back to bg2rgb
	mov	[edi+ebp],al

.skip	inc	ebp
	jnz	.loop

	add	esi,[esp+2*4+20]
	add	edi,[esp+3*4+20]
	mov	ebp,[esp+4*4+20]
	dec	ebx
	jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

;__declspec(naked) void STACK_ARGS DMSRP (
;	const byte *src, byte *dest, const byte *remap, int srcpitch, int destpitch, int w, int h, DWORD fg, DWORD *bg2rgb);

GLOBAL	DMSRP

	align 16

DMSRP:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+5*4+20]	; ebp = index
	mov	esi,[esp+0*4+20]	; esi = source
	mov	edi,[esp+1*4+20]	; edi = dest
	add	esi,ebp
	add	edi,ebp
	neg	ebp
	mov	ebx,[esp+4*4+20]	; ebx = dest+destpitch*2+2
	mov	[esp+5*4+20],ebp
	lea	ebx,[ebx*2+edi+2]
	xor	eax,eax
	mov	edx,[esp+2*4+20]	; edx = remap and scratch
	mov	ecx,[esp+8*4+20]	; ecx = bg2rgb and scratch

.loop	mov	al,[esi+ebp]
	cmp	eax,255
	jz	.skip

	mov	al,[edx+eax]		; remap color
	mov	edx,[esp+7*4+20]	; edx = fg
	mov	[edi+ebp],al		; store color
	mov	al,[ebx+ebp]		; get background color
	mov	ecx,[ecx+eax*4]		; get background RGB
	add	ecx,edx			; add foreground RGB
	or	ecx,0x1f07c1f
	mov	edx,ecx
	shr	edx,15
	and	edx,ecx
	mov	ecx,[esp+8*4+20]	; set ecx back to bg2rgb
	mov	al,[RGB32k+edx]		; convert RGB back to palette index
	mov	edx,[esp+2*4+20]	; set edx back to remap
	mov	[ebx+ebp],al		; store shadow pixel

.skip	inc	ebp
	jnz	.loop

	add	esi,[esp+3*4+20]
	add	edi,[esp+4*4+20]
	add	ebx,[esp+4*4+20]
	dec	dword [esp+6*4+20]
	mov	ebp,[esp+5*4+20]
	jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

;__declspec(naked) void STACK_ARGS DMSUP (
;	const byte *src, byte *dest, int srcpitch, int destpitch, int w, int h, DWORD fg, DWORD *bg2rgb);

GLOBAL	DMSUP

	align 16

DMSUP:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+4*4+20]	; ebp = index
	mov	esi,[esp+0*4+20]	; esi = source
	mov	edi,[esp+1*4+20]	; edi = dest
	add	esi,ebp
	add	edi,ebp
	neg	ebp
	mov	ebx,[esp+3*4+20]	; ebx = dest+destpitch*2+2
	mov	[esp+4*4+20],ebp
	lea	ebx,[ebx*2+edi+2]
	xor	eax,eax
	mov	ecx,[esp+7*4+20]	; ecx = bg2rgb

.loop	mov	al,[esi+ebp]
	cmp	al,255
	jz	.skip

	mov	[edi+ebp],al
	mov	al,[ebx+ebp]
	mov	edx,[esp+6*4+20]
	mov	ecx,[ecx+eax*4]
	add	ecx,edx
	or	ecx,0x1f07c1f
	mov	edx,ecx
	shr	edx,15
	and	edx,ecx
	mov	ecx,[esp+7*4+20]	; set ecx back to bg2rgb
	mov	al,[RGB32k+edx]
	mov	[ebx+ebp],al

.skip	inc	ebp
	jnz	.loop

	add	esi,[esp+2*4+20]
	add	edi,[esp+3*4+20]
	add	ebx,[esp+3*4+20]
	dec	dword [esp+5*4+20]
	mov	ebp,[esp+4*4+20]
	jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

;__declspec(naked) void STACK_ARGS DMAUP (
;	const byte *src, byte *dest, int srcpitch, int destpitch, int w, int h, DWORD *fgstart);

GLOBAL DMAUP

	align 16

DMAUP:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+4*4+20]	; ebp = index
	mov	esi,[esp+0*4+20]	; esi = source
	mov	edi,[esp+1*4+20]	; edi = dest
	add	esi,ebp
	add	edi,ebp
	neg	ebp
	mov	[esp+4*4+20],ebp
	mov	ecx,[esp+6*4+20]	; ecx = fgstart and scratch
	mov	edx,[esp+5*4+20]	; edx = height remaining
	xor	eax,eax
	xor	ebx,ebx

.loop	mov	al,[esi+ebp]
	cmp	eax,255
	jz	.skip

	add	eax,1
	mov	bl,[edi+ebp]
	shl	eax,6
	and	eax,0x7f00
	sub	ebx,eax
	mov	ecx,[ecx+eax*4]		; get fg RGB
	add	ecx,[Col2RGB8+ebx*4+0x10000] ; add 1/fg RGB
	xor	eax,eax
	or	ecx,0x1f07c1f
	mov	ebx,ecx
	shr	ebx,15
	and	ebx,ecx
	mov	ecx,[esp+6*4+20]	; set ecx back to fgstart
	mov	al,[RGB32k+ebx]
	xor	ebx,ebx
	mov	[edi+ebp],al

.skip	inc	ebp
	jnz	.loop

	add	esi,[esp+2*4+20]
	add	edi,[esp+3*4+20]
	mov	ebp,[esp+4*4+20]
	dec	edx
	jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret


; Learn from my mistake. In the following routines, I used this bit of code
; to get two cycle pairing on a Pentium. Unfurtunately, the shr instruction
; can clobber the OF flag even though it doesn't on a Pentium II (which is
; what I have ATM). So I had to swap the middle two instructions, so now it
; only gets three cycles on a Pentium. (Other processors should be smart
; enough to reorder this to run in two or fewer cycles, I would hope.)
;
;.skip	mov	ebx,edx
;	inc	ebp
;	shr	ebx,16
;	jno	.loop

;__declspec(naked) void STACK_ARGS SMPRP (
;	src, dest, remap, srcpitch, destpitch, xinc, yinc, xstart, yerr, width, height)

GLOBAL	SMPRP

	align 16

SMPRP:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+9*4+20]	; ebp = dest index and counter
	mov	edi,[esp+1*4+20]	; edi = dest
	mov	esi,[esp+0*4+20]	; esi = source
	xor	eax,eax
	lea	edi,[edi+ebp+0x80000000]
	neg	ebp
	add	ebp,0x80000000
	mov	ebx,[esp+7*4+20]	; ebx = source x index
	mov	[esp+9*4+20],ebp
	mov	edx,[esp+7*4+20]	; edx = xplace
	shr	ebx,16
	mov	ecx,[esp+5*4+20]	; ecx = xinc

.loop	mov	al,[esi+ebx]
	mov	ebx,[esp+2*4+20]
	add	edx,ecx
	cmp	eax,255
	mov	al,[ebx+eax]
	jz	.skip

	mov	[edi+ebp],al

.skip	mov	ebx,edx
	shr	ebx,16
	inc	ebp
	jno	.loop

	mov	ebp,[esp+8*4+20]
	mov	edx,[esp+7*4+20]
	add	ebp,[esp+6*4+20]
	add	edi,[esp+4*4+20]
	cmp	ebp,0x10000
	jb	.noadv

.adv	add	esi,[esp+3*4+20]
	sub	ebp,0x10000
	cmp	ebp,0x10000
	jae	.adv

.noadv	mov	ebx,edx
	mov	[esp+8*4+20],ebp
	shr	ebx,16
	mov	ebp,[esp+9*4+20]
	dec	dword [esp+10*4+20]
	jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

;__declspec(naked) void STACK_ARGS SMPUP (
;	src, dest, srcpitch, destpitch, xinc, yinc, xstart, yerr, width, height)

GLOBAL	SMPUP

	align 16

SMPUP:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+8*4+20]	; ebp = dest index and counter
	mov	edi,[esp+1*4+20]	; edi = dest
	mov	esi,[esp+0*4+20]	; esi = source
	xor	eax,eax
	lea	edi,[edi+ebp+0x80000000]
	neg	ebp
	add	ebp,0x80000000
	mov	ebx,[esp+6*4+20]	; ebx = source x index
	mov	[esp+8*4+20],ebp
	mov	edx,[esp+6*4+20]	; edx = xplace
	shr	ebx,16
	mov	ecx,[esp+4*4+20]	; ecx = xinc


.loop	mov	al,[esi+ebx]
	add	edx,ecx
	cmp	al,255
	jz	.skip

	mov	[edi+ebp],al

.skip	mov	ebx,edx
	shr	ebx,16
	inc	ebp
	jno	.loop

	mov	ebp,[esp+7*4+20]
	mov	edx,[esp+6*4+20]
	add	ebp,[esp+5*4+20]
	add	edi,[esp+3*4+20]
	cmp	ebp,0x10000
	jb	.noadv

.adv	add	esi,[esp+2*4+20]
	sub	ebp,0x10000
	cmp	ebp,0x10000
	jae	.adv

.noadv	mov	ebx,edx
	mov	[esp+7*4+20],ebp
	shr	ebx,16
	mov	ebp,[esp+8*4+20]
	dec	dword [esp+9*4+20]
	jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

;SMAUP (src, dest, srcpitch, destpitch, xinc, yinc, xstart, yerr, dwidth, dheight, fgstart);

GLOBAL	SMAUP

	align 16

SMAUP:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+8*4+20]	; ebp = dest index/counter
	mov	edi,[esp+1*4+20]	; edi = dest
	mov	esi,[esp+0*4+20]	; esi = source
	xor	eax,eax
	lea	edi,[edi+ebp+0x80000000]
	neg	ebp
	add	ebp,0x80000000
	mov	ebx,[esp+6*4+20]
	shr	ebx,16
	mov	[esp+8*4+20],ebp
	mov	edx,[esp+6*4+20]	; edx = xplace
	mov	ecx,[esp+4*4+20]	; ecx = xinc

.loop	mov	al,[esi+ebx]
	add	edx,ecx
	cmp	eax,255
	jz	.skip

	add	eax,2
	xor	ebx,ebx
	shl	eax,6
	mov	ecx,[esp+10*4+20]	; ecx = fgstart
	mov	bl,[edi+ebp]
	and	eax,0x7f00
	sub	ebx,eax
	mov	ecx,[ecx+eax*4]		; get fg RGB
	add	ecx,[Col2RGB8+ebx*4+0x10000] ; add 1/fg RGB
	xor	eax,eax
	or	ecx,0x1f07c1f
	mov	ebx,ecx
	shr	ecx,15
	and	ebx,ecx
	mov	ecx,[esp+4*4+20]	; ecx = xinc
	mov	al,[RGB32k+ebx]
	mov	[edi+ebp],al

.skip	mov	ebx,edx
	shr	ebx,16
	inc	ebp
	jno	.loop

	mov	ebp,[esp+8*4+20]
	mov	ebx,[esp+7*4+20]	; get yerr
	add	edi,[esp+3*4+20]	; advance dest to next line
	add	ebx,[esp+5*4+20]	; advance yerr
	cmp	ebx,0x10000
	jb	.noadv
.adv	add	esi,[esp+2*4+20]	; advance src to next line
	sub	ebx,0x10000
	cmp	ebx,0x10000
	jae	.adv
.noadv	mov	[esp+7*4+20],ebx
	mov	ebx,[esp+6*4+20]
	shr	ebx,16
	mov	edx,[esp+6*4+20]
	dec	dword [esp+9*4+20]
	jnz	near .loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

; The rest of this code is all self-modifying, so it lives in the data segment

%ifdef M_TARGET_WATCOM
  SEGMENT DATA PUBLIC ALIGN=16 CLASS=CODE USE32
  SEGMENT DATA
%else
  SECTION .data
%endif

;SMTRP (src, dest, remap, srcpitch, destpitch, xinc, yinc, xstart, yerr, dwidth, dheight, fg2rgb, bg2rgb);

GLOBAL	SMTRP

	align 16

SMTRP:
; self-modify with fg2rgb, bg2rgb, and remap
	mov	eax,[esp+11*4+4]	; fg2rgb
	mov	ecx,[esp+12*4+4]	; bg2rgb
	cmp	[.fg+3],eax		; fg2rgb and bg2rgb always come in unique pairs
	je	.tgood
	mov	[.fg+3],eax
	mov	[.bg+3],ecx
.tgood	mov	eax,[esp+2*4+4]		; remap
	cmp	[.map+2],eax
	je	.mgood
	mov	[.map+2],eax
.mgood
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+9*4+20]	; ebp = dest index/counter
	mov	edi,[esp+1*4+20]	; edi = dest
	mov	esi,[esp+0*4+20]	; esi = source
	xor	eax,eax
	lea	edi,[edi+ebp+0x80000000]
	neg	ebp
	add	ebp,0x80000000
	mov	ebx,[esp+7*4+20]
	shr	ebx,16
	mov	[esp+9*4+20],ebp
	mov	edx,[esp+7*4+20]
	mov	ecx,[esp+5*4+20]

.loop	mov	al,[esi+ebx]
	add	edx,ecx
	cmp	eax,255
	jz	.skip

.map	mov	al,[SPACEFILLER4+eax]		; remap
.fg	mov	ebx,[SPACEFILLER4+eax*4]	; fg2rgb
	mov	al,[edi+ebp]
.bg	add	ebx,[SPACEFILLER4+eax*4]	; bg2rgb
	or	ebx,0x1f07c1f
	mov	eax,ebx
	shr	ebx,15
	and	ebx,eax
	xor	eax,eax
	mov	al,[RGB32k+ebx]
	mov	[edi+ebp],al

.skip	mov	ebx,edx
	shr	ebx,16
	inc	ebp
	jno	.loop			; inc modifies OF; shr does not
	
	mov	ebp,[esp+9*4+20]
	mov	ebx,[esp+8*4+20]	; get yerr
	add	edi,[esp+4*4+20]	; advance dest to next line
	add	ebx,[esp+6*4+20]	; advance yerr
	cmp	ebx,0x10000
	jb	.noadv
.adv	add	esi,[esp+3*4+20]	; advance src to next line
	sub	ebx,0x10000
	cmp	ebx,0x10000
	jae	.adv
.noadv	mov	[esp+8*4+20],ebx
	mov	ebx,[esp+7*4+20]
	shr	ebx,16
	mov	edx,[esp+7*4+20]
	dec	dword [esp+10*4+20]
	jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

;SMTUP (src, dest, srcpitch, destpitch, xinc, yinc, xstart, yerr, dwidth, dheight, fg2rgb, bg2rgb);

GLOBAL	SMTUP

	align 16

SMTUP:
; self-modify with fg2rgb and bg2rgb
	mov	eax,[esp+10*4+4]	; fg2rgb
	mov	ecx,[esp+11*4+4]	; bg2rgb
	cmp	[.fg+3],eax
	je	.good
	mov	[.fg+3],eax
	mov	[.bg+3],ecx

.good	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+8*4+20]	; ebp = index
	mov	edi,[esp+1*4+20]	; edi = dest
	mov	esi,[esp+0*4+20]	; esi = source
	xor	eax,eax
	lea	edi,[edi+ebp+0x80000000]
	neg	ebp
	add	ebp,0x80000000
	mov	ebx,[esp+6*4+20]
	shr	ebx,16
	mov	[esp+8*4+20],ebp
	mov	edx,[esp+6*4+20]	; edx = xplace
	mov	ecx,[esp+4*4+20]	; ecx = xinc

.loop	mov	al,[esi+ebx]
	add	edx,ecx
	cmp	eax,255
	jz	.skip

.fg	mov	ebx,[SPACEFILLER4+eax*4]	; fg2rgb
	mov	al,[edi+ebp]
.bg	add	ebx,[SPACEFILLER4+eax*4]	; bg2rgb
	or	ebx,0x1f07c1f
	mov	eax,ebx
	shr	ebx,15
	and	ebx,eax
	xor	eax,eax
	mov	al,[RGB32k+ebx]
	mov	[edi+ebp],al

.skip	mov	ebx,edx
	shr	ebx,16
	inc	ebp
	jno	.loop			; inc modifies OF; shr does not
	
	mov	ebp,[esp+8*4+20]
	mov	ebx,[esp+7*4+20]	; get yerr
	add	edi,[esp+3*4+20]	; advance dest to next line
	add	ebx,[esp+5*4+20]	; advance yerr
	cmp	ebx,0x10000
	jb	.noadv
.adv	add	esi,[esp+2*4+20]	; advance src to next line
	sub	ebx,0x10000
	cmp	ebx,0x10000
	jae	.adv
.noadv	mov	[esp+7*4+20],ebx
	mov	ebx,[esp+6*4+20]
	shr	ebx,16
	mov	edx,[esp+6*4+20]
	dec	dword [esp+9*4+20]
	jnz	.loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

;SMSRP (src, dest, remap, srcpitch, destpitch, xinc, yinc, xstart, err, dwidth, dheight, fg, bg2rgb);

GLOBAL	SMSRP

	align 16

SMSRP:
; self-modify with remap, bg2rgb and destpitch
	mov	eax,[esp+12*4+4]	; bg2rgb
	mov	ecx,[esp+4*4+4]		; destpitch
	cmp	[.bg+3],eax
	je	.bgood
	mov	[.bg+3],eax
.bgood	lea	ecx,[ecx*2+2]
	mov	eax,[esp+2*4+4]		; remap
	cmp	[.sadv1+3],ecx
	je	.pgood
	mov	[.sadv1+3],ecx
	mov	[.sadv2+3],ecx
.pgood	cmp	eax,[.map+2]
	je	.mgood
	mov	[.map+2],eax
.mgood
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+9*4+20]	; ebp = dest index/counter
	mov	edi,[esp+1*4+20]	; edi = dest
	mov	esi,[esp+0*4+20]	; esi = source
	xor	eax,eax
	lea	edi,[edi+ebp+0x80000000]
	neg	ebp
	add	ebp,0x80000000
	mov	ebx,[esp+7*4+20]	; ebx = src index
	shr	ebx,16
	mov	[esp+9*4+20],ebp
	mov	edx,[esp+7*4+20]	; edx = xplace
	mov	ecx,[esp+5*4+20]	; ecx = xinc

.loop	mov	al,[esi+ebx]
	add	edx,ecx
	cmp	eax,255
	jz	.skip

.map	mov	al,[SPACEFILLER4+eax]
	mov	[edi+ebp],al
.sadv1	mov	al,[edi+ebp+SPACEFILLER4]	; get pixel under shadow
.bg	mov	ebx,[SPACEFILLER4+eax*4]	; and convert to RGB
	mov	eax,[esp+11*4+20]		; get fg RGB
	add	ebx,eax				; and add to bg RGB
	or	ebx,0x1f07c1f
	mov	eax,ebx
	shr	ebx,15
	and	ebx,eax
	xor	eax,eax
	mov	al,[RGB32k+ebx]
.sadv2	mov	[edi+ebp+SPACEFILLER4],al	; write shaded shadow

.skip	mov	ebx,edx
	shr	ebx,16
	inc	ebp
	jno	.loop
	
	mov	ebp,[esp+9*4+20]
	mov	ebx,[esp+8*4+20]	; get yerr
	add	edi,[esp+4*4+20]	; advance dest to next line
	add	ebx,[esp+6*4+20]	; advance yerr
	cmp	ebx,0x10000
	jb	.noadv
.adv	add	esi,[esp+3*4+20]	; advance src to next line
	sub	ebx,0x10000
	cmp	ebx,0x10000
	jae	.adv
.noadv	mov	[esp+8*4+20],ebx
	mov	ebx,[esp+7*4+20]
	shr	ebx,16
	mov	edx,[esp+7*4+20]
	dec	dword [esp+10*4+20]
	jnz	near .loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret

;SMSUP (src, dest, srcpitch, destpitch, xinc, yinc, xstart, err, dwidth, dheight, fg, bg2rgb);

GLOBAL	SMSUP

	align 16

SMSUP:
; self-modify with bg2rgb and destpitch
	mov	eax,[esp+11*4+4]	; bg2rgb
	mov	ecx,[esp+3*4+4]		; destpitch
	cmp	[.bg+3],eax
	je	.bgood
	mov	[.bg+3],eax
.bgood	lea	ecx,[ecx*2+2]
	cmp	[.sadv1+3],ecx
	je	.pgood
	mov	[.sadv1+3],ecx
	mov	[.sadv2+3],ecx
.pgood
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp+8*4+20]	; ebp = dest index/counter
	mov	edi,[esp+1*4+20]	; edi = dest
	mov	esi,[esp+0*4+20]	; esi = source
	xor	eax,eax
	lea	edi,[edi+ebp+0x80000000]
	neg	ebp
	add	ebp,0x80000000
	mov	ebx,[esp+6*4+20]	; ebx = src index
	shr	ebx,16
	mov	[esp+8*4+20],ebp
	mov	edx,[esp+6*4+20]	; edx = xplace
	mov	ecx,[esp+4*4+20]	; ecx = xinc

.loop	mov	al,[esi+ebx]
	add	edx,ecx
	cmp	eax,255
	jz	.skip

	mov	[edi+ebp],al
.sadv1	mov	al,[edi+ebp+SPACEFILLER4]	; get pixel under shadow
.bg	mov	ebx,[SPACEFILLER4+eax*4]	; and convert to RGB
	mov	eax,[esp+10*4+20]		; get fg RGB
	add	ebx,eax				; and add to bg RGB
	or	ebx,0x1f07c1f
	mov	eax,ebx
	shr	ebx,15
	and	ebx,eax
	xor	eax,eax
	mov	al,[RGB32k+ebx]
.sadv2	mov	[edi+ebp+SPACEFILLER4],al	; write shaded shadow

.skip	mov	ebx,edx
	shr	ebx,16
	inc	ebp
	jno	.loop
	
	mov	ebp,[esp+8*4+20]
	mov	ebx,[esp+7*4+20]	; get yerr
	add	edi,[esp+3*4+20]	; advance dest to next line
	add	ebx,[esp+5*4+20]	; advance yerr
	cmp	ebx,0x10000
	jb	.noadv
.adv	add	esi,[esp+2*4+20]	; advance src to next line
	sub	ebx,0x10000
	cmp	ebx,0x10000
	jae	.adv
.noadv	mov	[esp+7*4+20],ebx
	mov	ebx,[esp+6*4+20]
	shr	ebx,16
	mov	edx,[esp+6*4+20]
	dec	dword [esp+9*4+20]
	jnz	near .loop

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret
