;*
;* tmap.nas
;* The texture-mapping inner loops in pure assembly language.
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

; Segment/section definition macros. 

%ifdef M_TARGET_WATCOM
  SEGMENT DATA PUBLIC ALIGN=16 CLASS=DATA USE32
  SEGMENT DATA
%else
  SECTION .data
%endif

%define SPACEFILLER4 (0x44444444)

; If you change this in r_draw.c, be sure to change it here, too!
FUZZTABLE	equ	50

%ifdef M_TARGET_LINUX

EXTERN ylookup
EXTERN centery
EXTERN fuzzpos
EXTERN fuzzoffset
EXTERN NormalLight
EXTERN realviewheight

EXTERN dc_pitch
EXTERN dc_colormap
EXTERN dc_iscale
EXTERN dc_texturefrac
EXTERN dc_source
EXTERN dc_yl
EXTERN dc_yh
EXTERN dc_x

EXTERN dc_ctspan
EXTERN dc_temp

EXTERN ds_xstep
EXTERN ds_ystep
EXTERN ds_colormap
EXTERN ds_source
EXTERN ds_x1
EXTERN ds_x2
EXTERN ds_xfrac
EXTERN ds_yfrac
EXTERN ds_y

GLOBAL ds_cursource
GLOBAL ds_curcolormap

%else

EXTERN _ylookup
EXTERN _centery
EXTERN _fuzzpos
EXTERN _fuzzoffset
EXTERN _NormalLight
EXTERN _realviewheight

EXTERN _dc_pitch
EXTERN _dc_colormap
EXTERN _dc_iscale
EXTERN _dc_texturefrac
EXTERN _dc_source
EXTERN _dc_yl
EXTERN _dc_yh
EXTERN _dc_x

EXTERN _dc_ctspan
EXTERN _dc_temp

EXTERN _ds_xstep
EXTERN _ds_ystep
EXTERN _ds_colormap
EXTERN _ds_source
EXTERN _ds_x1
EXTERN _ds_x2
EXTERN _ds_xfrac
EXTERN _ds_yfrac
EXTERN _ds_y

GLOBAL _ds_cursource
GLOBAL _ds_curcolormap

%define ylookup		_ylookup
%define centery		_centery
%define fuzzpos		_fuzzpos
%define fuzzoffset	_fuzzoffset
%define NormalLight	_NormalLight
%define realviewheight	_realviewheight

%define dc_pitch	_dc_pitch
%define dc_colormap	_dc_colormap
%define dc_iscale	_dc_iscale
%define dc_texturefrac	_dc_texturefrac
%define dc_source	_dc_source
%define dc_yl		_dc_yl
%define dc_yh		_dc_yh
%define dc_x		_dc_x

%define dc_ctspan	_dc_ctspan
%define dc_temp		_dc_temp

%define ds_xstep	_ds_xstep
%define ds_ystep	_ds_ystep
%define ds_colormap	_ds_colormap
%define ds_source	_ds_source
%define ds_x1		_ds_x1
%define ds_x2		_ds_x2
%define ds_xfrac	_ds_xfrac
%define ds_yfrac	_ds_yfrac
%define ds_y		_ds_y

%endif

_ds_cursource:
ds_cursource:
	DD 0

_ds_curcolormap:
ds_curcolormap:
	DD 0


; Local stuff:
lastAddress	DD 0
pixelcount	DD 0

%ifdef M_TARGET_WATCOM
  SEGMENT DATA PUBLIC ALIGN=16 CLASS=CODE USE32
  SEGMENT DATA
%else
  SECTION .data
%endif


GLOBAL @R_SetSpanSource_ASM@4
GLOBAL _R_SetSpanSource_ASM
GLOBAL R_SetSpanSource_ASM

R_SetSpanSource_ASM:
_R_SetSpanSource_ASM:
	mov	ecx,[esp+4]

@R_SetSpanSource_ASM@4:
	mov	[spreada+2],ecx
	mov	[spreadb+2],ecx
	mov	[spreadc+2],ecx
	mov	[spreadd+2],ecx
	mov	[spreade+2],ecx
	mov	[spreadf+2],ecx
	mov	[spreadg+2],ecx
	mov	[ds_cursource],ecx
	ret

GLOBAL @R_SetSpanColormap_ASM@4
GLOBAL _R_SetSpanColormap_ASM
GLOBAL R_SetSpanColormap_ASM

R_SetSpanColormap_ASM:
_R_SetSpanColormap_ASM:
	mov ecx,[esp+4]

@R_SetSpanColormap_ASM@4:
	mov	[spmapa+2],ecx
	mov	[spmapb+2],ecx
	mov	[spmapc+2],ecx
	mov	[spmapd+2],ecx
	mov	[spmape+2],ecx
	mov	[spmapf+2],ecx
	mov	[spmapg+2],ecx
	mov	[ds_curcolormap],ecx
aret:	ret

GLOBAL @R_DrawSpanP_ASM@0
GLOBAL _R_DrawSpanP_ASM
GLOBAL R_DrawSpanP_ASM

; eax: scratch
; ebx: zero
; ecx: xfrac at top end, yfrac int part in low end3
; edx: yfrac frac part at top end
; edi: dest
; ebp: scratch
; esi: count

	align	16

@R_DrawSpanP_ASM@0:
_R_DrawSpanP_ASM:
R_DrawSpanP_ASM:
	mov	eax,[ds_x2]
	 mov	ecx,[ds_x1]
	sub	eax,ecx
	 jl	aret		; count < 0: nothing to do, so leave

	push	ebx
	push	edi
	push	ebp
	push	esi

	mov	edi,ecx
	 mov	ecx,[ds_y]
	add	edi,[ylookup+ecx*4]
	 mov	edx,[ds_ystep]
	shl	edx,6
	 mov	ebp,[ds_ystep]
	shr	ebp,26
	 xor	ebx,ebx
	lea	esi,[eax+1]
	 mov	[ds_ystep],edx
	mov	edx,[ds_xstep]
	 mov	ecx,[ds_yfrac]
	shr	ecx,26
	 and	edx,0xffffffc0
	or	ebp,edx
	 mov	[ds_xstep],ebp
	mov	ebp,[ds_xfrac]
	 mov	edx,[ds_yfrac]
	shl	edx,6
	 and	ebp,0xffffffc0
	or	ecx,ebp
	 shr	esi,1
	jnc	dseven1

; do odd pixel

		mov	ebp,ecx
		rol	ebp,6
		and	ebp,0xfff
		 add	edx,[ds_ystep]
		adc	ecx,[ds_xstep]
spreada		 mov	bl,[ebp+SPACEFILLER4]
spmapa		mov	bl,[ebx+SPACEFILLER4]
		mov	[edi],bl
		 inc	edi

dseven1		shr	esi,1
		 jnc	dsrest

; do two more pixels
		mov	ebp,ecx
		 add	edx,[ds_ystep]
		adc	ecx,[ds_xstep]
		 and	ebp,0xfc00003f
		rol	ebp,6
		mov	eax,ecx
		 add	edx,[ds_ystep]
		adc	ecx,[ds_xstep]
spreadb		 mov	bl,[ebp+SPACEFILLER4]	;read texel1
		rol	eax,6
		and	eax,0xfff
spmapb		 mov	bl,[ebx+SPACEFILLER4]	;map texel1
		mov	[edi],bl		;store texel1
		 add	edi,2
spreadc		mov	bl,[eax+SPACEFILLER4]	;read texel2
spmapc		mov	bl,[ebx+SPACEFILLER4]	;map texel2
		mov	[edi-1],bl		;store texel2

; do the rest

dsrest		test	esi,esi
		jz near	dsdone

		align 16

dsloop		mov	ebp,ecx
spstep1d	 add	edx,[ds_ystep]
spstep2d	adc	ecx,[ds_xstep]
		 and	ebp,0xfc00003f
		rol	ebp,6
		mov	eax,ecx
spstep1e	 add	edx,[ds_ystep]
spstep2e	adc	ecx,[ds_xstep]
spreadd		 mov	bl,[ebp+SPACEFILLER4]	;read texel1
		rol	eax,6
		and	eax,0xfff
spmapd		 mov	bl,[ebx+SPACEFILLER4]	;map texel1
		mov	[edi],bl		;store texel1
		 mov	ebp,ecx
spreade		mov	bl,[eax+SPACEFILLER4]	;read texel2
spstep1f	 add	edx,[ds_ystep]
spstep2f	adc	ecx,[ds_xstep]
		 and	ebp,0xfc00003f
		rol	ebp,6
spmape		mov	bl,[ebx+SPACEFILLER4]	;map texel2
		 mov	eax,ecx
		mov	[edi+1],bl		;store texel2
spreadf		 mov	bl,[ebp+SPACEFILLER4]	;read texel3
spmapf		mov	bl,[ebx+SPACEFILLER4]	;map texel3
		 add	edi,4
		rol	eax,6
		and	eax,0xfff
		 mov	[edi-2],bl		;store texel3
spreadg		mov	bl,[eax+SPACEFILLER4]	;read texel4
spstep1g	 add	edx,[ds_ystep]
spstep2g	adc	ecx,[ds_xstep]
spmapg		 mov	bl,[ebx+SPACEFILLER4]	;map texel4
		dec	esi
		 mov	[edi-1],bl		;store texel4
		jnz near dsloop

dsdone	pop	esi
	pop	ebp
	pop	edi
	pop	ebx

rdspret	ret


;************************

EXTERN setvlinebpl_
EXTERN setpitch3

GLOBAL	@ASM_PatchPitch@0
GLOBAL	_ASM_PatchPitch
GLOBAL	ASM_PatchPitch

ASM_PatchPitch:
_ASM_PatchPitch:
@ASM_PatchPitch@0:
	mov		eax,[dc_pitch]
	mov		[rdcp1+2],eax
	mov		[rdcp2+2],eax
	call	setpitch3
	jmp		setvlinebpl_


;*----------------------------------------------------------------------
;*
;* R_DrawColumnP
;*
;*----------------------------------------------------------------------

GLOBAL	@R_DrawColumnP_ASM@0
GLOBAL	_R_DrawColumnP_ASM
GLOBAL	R_DrawColumnP_ASM

	align 16

R_DrawColumnP_ASM:
_R_DrawColumnP_ASM:
@R_DrawColumnP_ASM@0:

; count = dc_yh - dc_yl;

	mov	ecx,[dc_yh]
	 mov	eax,[dc_yl]
	sub	ecx,eax
	 jl	near rdcpret		; count < 0: nothing to do, so leave

	push	ebp			; save registers
	 push	ebx
	push	edi
	 push	esi

; dest = ylookup[dc_yl] + dc_x;

	lea	ebp,[ecx+1]		; make 0 count mean 0 pixels
	 mov	esi,[dc_x]
	mov	edi,[ylookup+eax*4]
	 mov	ebx,[dc_texturefrac]	; ebx = frac
	add	edi,esi			; edi = top of destination column
	 mov	ecx,ebx
	shr	ecx,16
	 mov	esi,[dc_source]
	mov	edx,[dc_iscale]

rdcp1:	 sub	edi,SPACEFILLER4
	mov	eax,[dc_colormap]

; pad this out to a 16 byte boundary while retaining pairing on a Pentium
	 mov	edx,edx
	mov	ebp,ebp
	 nop

; The registers should now look like this:
;
;	[31  ..  16][15 .. 8][7 .. 0]
; eax	[colormap		    ]
; ebx	[yi	   ][yf		    ]
; ecx	[scratch		    ]
; edx	[dyi	   ][dyf	    ]
; esi	[source texture column	    ]
; edi	[destination screen pointer ]
; ebp	[counter		    ]
;
; Note the partial register stalls on anything better than a Pentium

rdcploop:
	mov	cl,[esi+ecx]		; Fetch texel
	 mov	ch,0
	add	ebx,edx			; increment frac
rdcp2:	 add	edi,SPACEFILLER4	; increment destination pointer
	mov	cl,[eax+ecx]		; colormap texel
	mov	[edi],cl		; Store texel
	 mov	ecx,ebx
	shr	ecx,16
	 dec	ebp
	jnz	rdcploop		; loop

	pop	esi
	 pop	edi
	pop	ebx
	 pop	ebp
rdcpret:
	ret

;*----------------------------------------------------------------------
;*
;* R_DrawFuzzColumnP
;*
;*----------------------------------------------------------------------

GLOBAL	@R_DrawFuzzColumnP_ASM@0
GLOBAL	_R_DrawFuzzColumnP_ASM
GLOBAL	R_DrawFuzzColumnP_ASM

	align 16

R_DrawFuzzColumnP_ASM:
_R_DrawFuzzColumnP_ASM:
@R_DrawFuzzColumnP_ASM@0:

; Adjust borders. Low...
	mov	eax,[dc_yl]
	 push	ebx
	push	esi
	 push	edi
	push	ebp

	 cmp	eax,0
	jg	.ylok

	mov	eax,1
	nop

; ...and high.
.ylok	mov	edx,[realviewheight]
	 mov	esi,[dc_yh]
	lea	ecx,[edx-2]
	cmp	esi,ecx
	 jle	.yhok

	mov	esi,ecx
	nop

.yhok	mov	edx,[dc_x]
	 sub	esi,eax			; esi = count
	js	near .dfcdone		; Zero length (or less)

	mov	edi,[ylookup+eax*4]
	 mov	ebx,edx
	mov	eax,[NormalLight]
	 mov	ecx,[fuzzpos]
	add	edi,ebx
	 add	eax,256*6
	inc	esi
	 mov	ebp,[dc_pitch]
	mov	edx,FUZZTABLE
	 test	ecx,ecx
	je	.fuzz0

;
; esi = count
; edi = dest
; ecx = fuzzpos
; eax = colormap 6
;

; first loop: end with fuzzpos or count 0, whichever happens first

	sub	edx,ecx			; edx = # of entries left in fuzzoffset
	 mov	ebx,esi
	cmp	esi,edx
	 jle	.enuf
	mov	esi,edx
.enuf	sub	ebx,esi
	 mov	edx,[fuzzoffset+ecx*4]
	push	ebx
	 xor	ebx,ebx

.loop1	inc	ecx
	 mov	bl,[edi+edx]
	dec	esi
	 mov	bl,[eax+ebx]
	mov	[edi],bl
	 lea	edi,[edi+ebp]
	mov	edx,[fuzzoffset+ecx*4]
	 jnz	.loop1

; second loop: Chunk it into groups of FUZZTABLE-sized spans and do those

	pop	esi
	 cmp	ecx,FUZZTABLE
	jl	.savefuzzpos
	xor	ecx,ecx
	 nop
.fuzz0	cmp	esi,FUZZTABLE
	 jl	.chunked

.oloop	lea	edx,[esi-FUZZTABLE]
	 mov	esi,FUZZTABLE
	push	edx
	 mov	edx,[fuzzoffset+ecx*4]

.iloop	inc	ecx
	 mov	bl,[edi+edx]
	dec	esi
	 mov	bl,[eax+ebx]
	mov	[edi],bl
	 lea	edi,[edi+ebp]
	mov	edx,[fuzzoffset+ecx*4]
	 jnz	.iloop

	pop	esi
	 xor	ecx,ecx
	cmp	esi,FUZZTABLE
	 jge	.oloop

; third loop: Do whatever is left

.chunked
	test	esi,esi
	 jle	.savefuzzpos
	mov	edx,[fuzzoffset+ecx*4]
	 nop

.loop3	inc	ecx
	 mov	bl,[edi+edx]
	dec	esi
	 mov	bl,[eax+ebx]
	mov	[edi],bl
	 lea	edi,[edi+ebp]
	mov	edx,[fuzzoffset+ecx*4]
	 jnz	.loop3

.savefuzzpos:
	mov	[fuzzpos],ecx
.dfcdone:
	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret


;*----------------------------------------------------------------------
;*
;* R_DrawColumnHorizP_ASM
;*
;*----------------------------------------------------------------------

GLOBAL	@R_DrawColumnHorizP_ASM@0
GLOBAL	_R_DrawColumnHorizP_ASM
GLOBAL	R_DrawColumnHorizP_ASM

	align 16

@R_DrawColumnHorizP_ASM@0:
R_DrawColumnHorizP_ASM:
_R_DrawColumnHorizP_ASM:

; count = dc_yh - dc_yl;

	mov	eax,[dc_yh]
	mov	ecx,[dc_yl]
	sub	eax,ecx
	mov	edx,[dc_x]

	jl	near .leave		; count < 0: nothing to do, so leave

	push	ebp			; save registers
	push	ebx
	push	edi
	push	esi

	inc	eax			; make 0 count mean 0 pixels
	 and	edx,3
	push	eax
	 mov	esi,[dc_ctspan+edx*4]
	lea	eax,[dc_temp+ecx*4+edx] ; eax = top of column in buffer
	 mov	ebp,[dc_yh]
	mov	[esi],ecx
	 mov	[esi+4],ebp
	add	esi,8
	 mov	edi,[dc_source]
	mov	[dc_ctspan+edx*4],esi
	 mov	esi,[dc_iscale]
	mov	ecx,[dc_texturefrac]	; ecx = frac
	 mov	dl,[edi]		; load cache
	mov	ebx,[esp]
	 and	ebx,0xfffffff8
	jnz	.mthan8

; Register usage in the following code is:
;
; eax: dest
; edi: source
; ecx: frac (16.16)
; esi: fracstep (16.16)
; ebx: add1
; ebp: add2
;  dl: texel1
;  dh: texel2
;[esp] count

; there are fewer than 8 pixels to draw

	mov	ebx,[esp]
.lthan8	shr	ebx,1
	 jnc	.even

; do one pixel before loop (little opportunity for pairing)

	mov	ebp,ecx			; copy frac to ebx
	 add	ecx,esi			; increment frac
	shr	ebp,16			; shift frac over to low end
	 add	eax,4
	mov	dl,[edi+ebp]
	mov	[eax-4],dl

.even	test	ebx,ebx
	jz	near .done

.loop2	mov	[esp],ebx		; save counter
	 mov	ebx,ecx			; copy frac for texel1 to ebx
	shr	ebx,16			; shift frac for texel1 to low end
	 add	ecx,esi			; increment frac
	mov	ebp,ecx			; copy frac for texel2 to ebp
	shr	ebp,16			; shift frac for texel2 to low end
	 add	ecx,esi			; increment frac
	 mov	dl,[edi+ebx]		; read texel1
	mov	ebx,[esp]		; fetch counter
	 mov	dh,[edi+ebp]		; read texel2
	mov	[eax],dl		; write texel1
	 mov	[eax+4],dh		; write texel2
	add	eax,8			; increment dest
	 dec	ebx			; decrement counter
	jnz	.loop2			; loop until it hits 0

	jmp	.done

; there are more than 8 pixels to draw. position eax as close to a 32 byte
; boundary as possible, then do whatever is left.

.mthan8 test	eax,4
	jz	.try2

	mov	ebp,ecx			; frac: in ebp
	 add	ecx,esi			; step
	shr	ebp,16			; frac: shift
	 add	eax,4			; increment dest
	 mov	ebx,[esp]		; fetch counter
	mov	dl,[edi+ebp]		; tex:  read
	 dec	ebx			; decrement counter
	mov	[eax-4],dl		; tex:  write
	 mov	[esp],ebx		; store counter

.try2	test	eax,8
	jz	.try4

	mov	ebx,ecx			; frac1: in ebx
	 add	ecx,esi			; step
	shr	ebx,16			; frac1: shift
	 mov	ebp,ecx			; frac2: in ebp
	shr	ebp,16			; frac2: shift
	 add	ecx,esi			; step
	mov	dl,[edi+ebx]		; tex1:  read
	 mov	ebx,[esp]		; fetch counter
	mov	dh,[edi+ebp]		; tex2:  read
	 mov	[eax],dl		; tex1:  write
	mov	[eax+4],dh		; tex2:  write
	 sub	ebx,2			; decrement counter
	add	eax,8			; increment dest
	 mov	[esp],ebx		; store counter

.try4	test	eax,16
	jz	.try8

	mov	ebx,ecx			; frac1: in ebx
	 add	ecx,esi			; step
	shr	ebx,16			; frac1: shift
	 mov	ebp,ecx			; frac2: in ebp
	shr	ebp,16			; frac2: shift
	 add	ecx,esi			; step
	mov	dl,[edi+ebx]		; tex1:  read
	 mov	ebx,ecx			; frac3: in ebx
	shr	ebx,16			; frac3: shift
	 mov	dh,[edi+ebp]		; tex2:  read
	add	ecx,esi			; step
	 mov	[eax],dl		; tex1:  write
	mov	[eax+4],dh		; tex2:  write
	 mov	ebp,ecx			; frac4: in ebp
	shr	ebp,16			; frac4: shift
	 add	ecx,esi			; step
	mov	dl,[edi+ebx]		; tex3:  read
	 mov	ebx,[esp]		; fetch counter
	mov	dh,[edi+ebp]		; tex4:  read
	 sub	ebx,4			; decrement counter
	mov	[esp],ebx		; store counter
	 mov	[eax+8],dl		; tex3:  write
	mov	[eax+12],dh		; tex4:  write
	 add	eax,16			; increment dest

.try8	mov	ebx,[esp]		; make counter count groups of 8
	sub	esp,4
	shr	ebx,3
	jmp	.tail8

	align	16

.loop8	mov	[esp],ebx		; save counter
	 mov	ebx,ecx			; frac1: in ebx
	shr	ebx,16			; frac1: shift
	 add	ecx,esi			; step
	mov	ebp,ecx			; frac2: in ebp
	shr	ebp,16			; frac2: shift
	 add	ecx,esi			; step
	 mov	dl,[edi+ebx]		; tex1:  read
	mov	ebx,ecx			; frac3: in ebx
	 mov	dh,[edi+ebp]		; tex2:  read
	shr	ebx,16			; frac3: shift
	 add	ecx,esi			; step
	mov	[eax],dl		; tex1:  write
	 mov	[eax+4],dh		; tex2:  write
	mov	ebp,ecx			; frac4: in ebp
	shr	ebp,16			; frac4: shift
	 add	ecx,esi			; step
	 mov	dl,[edi+ebx]		; tex3:  read
	mov	ebx,ecx			; frac5: in ebx
	 mov	dh,[edi+ebp]		; tex4:  read
	shr	ebx,16			; frac5: shift
	 mov	[eax+8],dl		; tex3:  write
	mov	[eax+12],dh		; tex4:  write
	 add	ecx,esi			; step
	mov	ebp,ecx			; frac6: in ebp
	shr	ebp,16			; frac6: shift
	 mov	dl,[edi+ebx]		; tex5:  read
	 add	ecx,esi			; step
	mov	ebx,ecx			; frac7: in ebx
	 mov	[eax+16],dl		; tex5:  write
	shr	ebx,16			; frac7: shift
	 mov	dh,[edi+ebp]		; tex6:  read
	 add	ecx,esi			; step
	mov	ebp,ecx			; frac8: in ebp
	 mov	[eax+20],dh		; tex6:  write
	shr	ebp,16			; frac8: shift
	 add	eax,32			; increment dest pointer
	 mov	dl,[edi+ebx]		; tex7:  read
	mov	ebx,[esp]		; fetch counter
	 mov	[eax-8],dl		; tex7:  write
	mov	dh,[edi+ebp]		; tex8:  read
	 add	ecx,esi			; step
	mov	[eax-4],dh		; tex8:  write
	 mov	dl,[eax]		; load cache
	dec	ebx			; decrement counter
.tail8	 jnz	near .loop8		; loop if more to do
	
	pop	ebp
	mov	ebx,[esp]
	and	ebx,7
	jnz	near .lthan8

.done	pop	eax
	pop	esi
	pop	edi
	pop	ebx
	pop	ebp
.leave	ret


;*----------------------------------------------------------------------
;*
;* rt_copy1col_asm
;*
;* ecx = hx
;* edx = sx
;* [esp+4] = yl
;* [esp+8] = yh
;*
;*----------------------------------------------------------------------

GLOBAL	@rt_copy1col_asm@16
GLOBAL	_rt_copy1col_asm
GLOBAL	rt_copy1col_asm

	align 16

rt_copy1col_asm:
_rt_copy1col_asm:
	pop	eax
	mov	edx,[esp+4*3]
	mov	ecx,[esp+4*2]
	push	edx
	push	ecx
	mov	ecx,[esp+4*2]
	mov	edx,[esp+4*3]
	push	eax

@rt_copy1col_asm@16:
	mov	eax, [esp+4]
	push	ebx
	mov	ebx, [esp+12]
	push	esi
	sub	ebx, eax
	push	edi
	js	.done

	lea	esi,[eax*4]
	inc	ebx			; ebx = count
	mov	eax,edx
	lea	ecx,[dc_temp+ecx+esi]	; ecx = source
	mov	edi,[ylookup+esi]
	mov	esi,[dc_pitch]		; esi = pitch
	add	eax,edi			; eax = dest

	shr	ebx,1
	jnc	.even

	mov	dl,[ecx]
	add	ecx,4
	mov	[eax],dl
	add	eax,esi

.even	and	ebx,ebx
	jz	.done

.loop	mov	dl,[ecx]
	mov	dh,[ecx+4]
	mov	[eax],dl
	mov	[eax+esi],dh
	add	ecx,8
	lea	eax,[eax+esi*2]
	dec	ebx
	jnz	.loop

.done	pop	edi
	pop	esi
	pop	ebx
	ret	8

;*----------------------------------------------------------------------
;*
;* rt_copy4cols_asm
;*
;* ecx = sx
;* edx = yl
;* [esp+4] = yh
;*
;*----------------------------------------------------------------------

GLOBAL	@rt_copy4cols_asm@12
GLOBAL	_rt_copy4cols_asm
GLOBAL	rt_copy4cols_asm

	align 16

rt_copy4cols_asm:
_rt_copy4cols_asm:
	pop	eax
	mov	ecx,[esp+8]
	mov	edx,[esp+4]
	push	ecx
	mov	ecx,[esp+4]
	push	eax

@rt_copy4cols_asm@12:
	push	ebx
	mov	ebx,[esp+8]
	push	esi
	sub	ebx,edx
	push	edi
	js	.done

	inc	ebx			; ebx = count
	mov	eax,ecx
	mov	esi,[ylookup+edx*4]
	lea	ecx,[dc_temp+edx*4]	; ecx = source
	mov	edx,[dc_pitch]		; edx = pitch
	add	eax,esi			; eax = dest

	shr	ebx,1
	jnc	.even

	mov	esi,[ecx]
	add	ecx,4
	mov	[eax],esi
	add	eax,edx

.even	and	ebx,ebx
	jz	.done

.loop	mov	esi,[ecx]
	mov	edi,[ecx+4]
	mov	[eax],esi
	mov	[eax+edx],edi
	add	ecx,8
	lea	eax,[eax+edx*2]
	dec	ebx
	jnz	.loop

.done	pop	edi
	pop	esi
	pop	ebx
	ret	4

;*----------------------------------------------------------------------
;*
;* rt_map1col_asm
;*
;* ecx = hx
;* edx = sx
;* [esp+4] = yl
;* [esp+8] = yh
;*
;*----------------------------------------------------------------------

GLOBAL	@rt_map1col_asm@16
GLOBAL	_rt_map1col_asm
GLOBAL	rt_map1col_asm

	align 16

rt_map1col_asm:
_rt_map1col_asm:
	pop	eax
	mov	edx,[esp+4*3]
	mov	ecx,[esp+4*2]
	push	edx
	push	ecx
	mov	ecx,[esp+4*2]
	mov	edx,[esp+4*3]
	push	eax

@rt_map1col_asm@16:	
	mov	eax,[esp+4]
	push	ebx
	mov	ebx,[esp+12]
	push	ebp
	push	esi
	sub	ebx, eax
	push	edi
	js	.done

	lea	edi,[eax*4]
	mov	esi,[dc_colormap]		; esi = colormap
	inc	ebx				; ebx = count
	mov	eax,edx
	lea	ebp,[dc_temp+ecx+edi]		; ebp = source
	mov	ecx,[ylookup+edi]
	mov	edi,[dc_pitch]			; edi = pitch
	add	eax,ecx				; eax = dest
	xor	ecx,ecx
	xor	edx,edx

	shr	ebx,1
	jnc	.even

	mov	dl,[ebp]
	add	ebp,4
	mov	dl,[esi+edx]
	mov	[eax],dl
	add	eax,edi

.even	and	ebx,ebx
	jz	.done

.loop	mov	dl,[ebp]
	mov	cl,[ebp+4]
	add	ebp,8
	mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	mov	[eax],dl
	mov	[eax+edi],cl
	dec	ebx
	lea	eax,[eax+edi*2]
	jnz	.loop

.done	pop	edi
	pop	esi
	pop	ebp
	pop	ebx
	ret	8

;*----------------------------------------------------------------------
;*
;* rt_map4cols_asm
;*
;* rt_map4cols_asm1 is for PPro and above
;* rt_map4cols_asm2 is for Pentium and below
;*
;* ecx = sx
;* edx = yl
;* [esp+4] = yh
;*
;*----------------------------------------------------------------------

GLOBAL	@rt_map4cols_asm1@12
GLOBAL	_rt_map4cols_asm1
GLOBAL	rt_map4cols_asm1

	align 16

rt_map4cols_asm1:
_rt_map4cols_asm1:
	pop	eax
	mov	ecx,[esp+8]
	mov	edx,[esp+4]
	push	ecx
	mov	ecx,[esp+4]
	push	eax

@rt_map4cols_asm1@12:
	push	ebx
	mov	ebx,[esp+8]
	push	ebp
	push	esi
	sub	ebx,edx
	push	edi
	js	near .done

	mov	esi,[dc_colormap]	; esi = colormap
	shl	edx,2
	mov	eax,ecx
	inc	ebx			; ebx = count
	mov	edi,[ylookup+edx]
	lea	ebp,[dc_temp+edx]	; ebp = source
	add	eax,edi			; eax = dest
	mov	edi,[dc_pitch]		; edi = pitch
	xor	ecx,ecx
	xor	edx,edx

	shr	ebx,1
	jnc	.even

	mov	dl,[ebp]
	 mov	cl,[ebp+1]
	add	ebp,4
	 mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	 mov	[eax],dl
	mov	[eax+1],cl
	 mov	dl,[ebp-2]
	mov	cl,[ebp-1]
	 mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	 mov	[eax+2],dl
	mov	[eax+3],cl
	 add	eax,edi

.even	and	ebx,ebx
	jz	.done

.loop
	mov	dl,[ebp]
	 mov	cl,[ebp+1]
	add	ebp,8
	 mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	 mov	[eax],dl
	mov	[eax+1],cl
	 mov	dl,[ebp-6]
	mov	cl,[ebp-5]
	 mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	 mov	[eax+2],dl
	mov	[eax+3],cl
	 mov	dl,[ebp-4]
	mov	cl,[ebp-3]
	 mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	 mov	[eax+edi],dl
	mov	[eax+edi+1],cl
	 mov	dl,[ebp-2]
	mov	cl,[ebp-1]
	 mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	 mov	[eax+edi+2],dl
	mov	[eax+edi+3],cl
	 lea	eax,[eax+edi*2]
	dec	ebx

	jnz	.loop

.done	pop	edi
	pop	esi
	pop	ebp
	pop	ebx
	ret	4

GLOBAL	@rt_map4cols_asm2@12
GLOBAL	_rt_map4cols_asm2
GLOBAL	rt_map4cols_asm2

	align 16

rt_map4cols_asm2:
_rt_map4cols_asm2:
	pop	eax
	mov	ecx,[esp+8]
	mov	edx,[esp+4]
	push	ecx
	mov	ecx,[esp+4]
	push	eax

@rt_map4cols_asm2@12:
	push	ebx
	mov	ebx,[esp+8]
	push	ebp
	push	esi
	sub	ebx,edx
	push	edi
	js	near .done

	mov	esi,[dc_colormap]	; esi = colormap
	shl	edx,2
	mov	eax,ecx
	inc	ebx			; ebx = count
	mov	edi,[ylookup+edx]
	lea	ebp,[dc_temp+edx]	; ebp = source
	add	eax,edi			; eax = dest
	mov	edi,[dc_pitch]		; edi = pitch
	xor	ecx,ecx
	xor	edx,edx

	shr	ebx,1
	jnc	.even

	mov	dl,[ebp]
	 mov	cl,[ebp+1]
	add	ebp,4
	 mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	 mov	[eax],dl
	mov	[eax+1],cl
	 mov	dl,[ebp-2]
	mov	cl,[ebp-1]
	 mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	 mov	[eax+2],dl
	mov	[eax+3],cl
	 add	eax,edi

.even	and	ebx,ebx
	jz	.done

.loop
	mov	dl,[ebp+3]
	mov	ch,[esi+edx]
	mov	dl,[ebp+2]
	mov	cl,[esi+edx]
	shl	ecx,16
	mov	dl,[ebp+1]
	mov	ch,[esi+edx]
	mov	dl,[ebp]
	mov	cl,[esi+edx]
	mov	[eax],ecx
	add	eax,edi

	mov	dl,[ebp+7]
	mov	ch,[esi+edx]
	mov	dl,[ebp+6]
	mov	cl,[esi+edx]
	shl	ecx,16
	mov	dl,[ebp+5]
	mov	ch,[esi+edx]
	mov	dl,[ebp+4]
	mov	cl,[esi+edx]
	mov	[eax],ecx
	add	eax,edi
	add	ebp,8
	dec	ebx

	jnz	.loop

.done	pop	edi
	pop	esi
	pop	ebp
	pop	ebx
	ret	4
