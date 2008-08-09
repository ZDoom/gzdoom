;*
;* misc.nas
;* Miscellaneous assembly functions
;*
;*---------------------------------------------------------------------------
;* Copyright 1998-2006 Randy Heit
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

%ifndef M_TARGET_LINUX

%define DoBlending_MMX	_DoBlending_MMX
%define BestColor_MMX	_BestColor_MMX

%endif

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
; DoBlending_MMX
;
; MMX version of DoBlending
;
; (DWORD *from, DWORD *to, count, tor, tog, tob, toa)
;-----------------------------------------------------------

GLOBAL	DoBlending_MMX

DoBlending_MMX:
		pxor		mm0,mm0			; mm0 = 0
		mov			eax,[esp+4*4]
		shl			eax,16
		mov			edx,[esp+4*5]
		shl			edx,8
		or			eax,[esp+4*6]
		or			eax,edx
		mov			ecx,[esp+4*3]	; ecx = count
		movd		mm1,eax			; mm1 = 00000000 00RRGGBB
		mov			eax,[esp+4*7]
		shl			eax,16
		mov			edx,[esp+4*7]
		shl			edx,8
		or			eax,[esp+4*7]
		or			eax,edx
		mov			edx,[esp+4*2]	; edx = dest
		movd		mm6,eax			; mm6 = 00000000 00AAAAAA
		punpcklbw	mm1,mm0			; mm1 = 000000RR 00GG00BB
		movq		mm7,[Blending256]
		punpcklbw	mm6,mm0			; mm6 = 000000AA 00AA00AA
		mov			eax,[esp+4*1]	; eax = source
		pmullw		mm1,mm6			; mm1 = 000000RR 00GG00BB (multiplied by alpha)
		psubusw		mm7,mm6			; mm7 = 000000aa 00aa00aa (one minus alpha)
		nop							; Does this actually pair on a Pentium?

; Do four colors per iteration: Count must be a multiple of four.

.loop	movq		mm2,[eax]	; mm2 = 00r2g2b2 00r1g1b1
		add			eax,8
		movq		mm3,mm2		; mm3 = 00r2g2b2 00r1g1b1
		punpcklbw	mm2,mm0		; mm2 = 000000r1 00g100b1
		punpckhbw	mm3,mm0		; mm3 = 000000r2 00g200b2
		pmullw		mm2,mm7		; mm2 = 0000r1rr g1ggb1bb
		add			edx,8
		pmullw		mm3,mm7		; mm3 = 0000r2rr g2ggb2bb
		sub			ecx,2
		paddusw		mm2,mm1
		psrlw		mm2,8
		paddusw		mm3,mm1
		psrlw		mm3,8
		packuswb	mm2,mm3		; mm2 = 00r2g2b2 00r1g1b1
		movq		[edx-8],mm2

		movq		mm2,[eax]	; mm2 = 00r2g2b2 00r1g1b1
		add			eax,8
		movq		mm3,mm2		; mm3 = 00r2g2b2 00r1g1b1
		punpcklbw	mm2,mm0		; mm2 = 000000r1 00g100b1
		punpckhbw	mm3,mm0		; mm3 = 000000r2 00g200b2
		pmullw		mm2,mm7		; mm2 = 0000r1rr g1ggb1bb
		add			edx,8
		pmullw		mm3,mm7		; mm3 = 0000r2rr g2ggb2bb
		sub			ecx,2
		paddusw		mm2,mm1
		psrlw		mm2,8
		paddusw		mm3,mm1
		psrlw		mm3,8
		packuswb	mm2,mm3		; mm2 = 00r2g2b2 00r1g1b1
		movq		[edx-8],mm2
		
		jnz			.loop

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
		mov			ecx,[esp+4]
		mov			edx,[esp+8]
@BestColor_MMX@8:
		pxor		mm0,mm0
		movd		mm1,ecx			; mm1 = color searching for
		mov			eax,257*257+257*257+257*257	;eax = bestdist
		push		ebx
		punpcklbw	mm1,mm0
		mov			ebx,ecx			; ebx = best color
		shr			ecx,24			; ecx = count
		and			ebx,0xffffff
		push		esi
		push		ebp

.loop	movd		mm2,[edx+ecx*4]	; mm2 = color considering now
		inc			ecx
		punpcklbw	mm2,mm0
		movq		mm3,mm1
		psubsw		mm3,mm2
		pmullw		mm3,mm3			; mm3 = color distance squared

		movd		ebp,mm3			; add the three components
		psrlq		mm3,32			; into ebp to get the real
		mov			esi,ebp			; (squared) distance
		shr			esi,16
		and			ebp,0xffff
		add			ebp,esi
		movd		esi,mm3
		add			ebp,esi

		jz			.perf			; found a perfect match
		cmp			eax,ebp
		jb			.skip
		mov			eax,ebp
		lea			ebx,[ecx-1]
.skip	cmp			ecx,256
		jne			.loop
		mov			eax,ebx
		pop			ebp
		pop			esi
		pop			ebx
		emms
		ret

.perf	lea		eax,[ecx-1]
		pop		ebp
		pop		esi
		pop		ebx
		emms
		ret
