;* Here are the texture-mapping inner loops in pure assembly
;* language. DrawSpan8Loop is from NTDOOM and
;* R_DrawColumn_ASM is from Doom Legacy. Everything else
;* was written by me.
;*
;* NEW- Discovered the Win32 Demos FAQ. Self-modifying code
;* no longer sits in the data segment.

BITS 32

; Segment/section definition macros. 

%ifdef M_TARGET_WATCOM
  SEGMENT DATA PUBLIC ALIGN=16 CLASS=DATA USE32
  SEGMENT DATA
%else
  SECTION .data
%endif

; External variables:
EXTERN	_columnofs
EXTERN	_TransTable
EXTERN	_ylookup
EXTERN	_centery
EXTERN	_fuzzpos
EXTERN	_fuzzoffset
EXTERN	_DefaultPalette
EXTERN	_realviewheight

EXTERN	_dc_pitch
EXTERN	_dc_colormap
EXTERN	_dc_transmap
EXTERN	_dc_iscale
EXTERN	_dc_texturemid
EXTERN	_dc_source
EXTERN	_dc_yl
EXTERN	_dc_yh
EXTERN	_dc_x
EXTERN	_dc_mask	; Tutti-Frutti fix

EXTERN	_ds_colsize
EXTERN	_ds_colshift
EXTERN	_ds_xstep
EXTERN	_ds_ystep
EXTERN	_ds_colormap
EXTERN	_ds_source
EXTERN	_ds_x1
EXTERN	_ds_x2
EXTERN	_ds_xfrac
EXTERN	_ds_yfrac
EXTERN	_ds_y

; If you change this in r_draw.c, be sure to change it here, too!
FUZZTABLE	equ	64


; Local stuff:
lastAddress	DD 0

pixelcount	DD 0

%ifdef M_TARGET_WATCOM
  SEGMENT CODE PUBLIC ALIGN=16 CLASS=CODE USE32
  SEGMENT CODE
%else
  SECTION .text
%endif


GLOBAL	_ASM_PatchColSize

_ASM_PatchColSize:
	mov	ecx,[_ds_colshift]
	mov	edx,[_ds_colsize]
	mov	[s0+2],cl
	mov	[s1+2],dl
	add	edx,edx
	mov	[s2+2],edx
	ret

;*----------------------------------------------------------------------
;*
;* DrawSpan8Loop Written by Petteri Kangaslampi <pekangas@sci.fi>
;* and Donated to the public domain.
;*
;*----------------------------------------------------------------------

GLOBAL	_DrawSpan8Loop

;void DrawSpan8Loop(fixed_t xfrac, fixed_t yfrac, int count, byte *dest);

_DrawSpan8Loop:
	push	ebp
	 push	edi
	push	esi
	 push	ebx

	mov	eax,[esp+28]	; count
	 mov	ebx,[esp+20]	; xfrac
s0:	sal	eax,8
	 mov	edi,[esp+32]	; dest
	add	eax,edi
	 mov	ecx,[esp+24]	; yfrac
	mov	[lastAddress],eax
	
	 mov	ebp,ebx
	mov	edx,ecx
	 and	ebp,65536*63
	shr	edx,10

	 shr	ebp,16
	xor	eax,eax
	 nop

ds8loo:	and	edx,64*63
	mov	esi,[_ds_source]

	or	ebp,edx
	mov	edx,[_ds_xstep]

	add	ebx,edx
	mov	edx,[_ds_ystep]

	mov	al,[esi+ebp]
	mov	esi,[_ds_colormap]

	add	ecx,edx
	mov	ebp,ebx

	mov	edx,ecx
	mov	al,[esi+eax]

	shr	edx,10
	mov	[edi],al

	shr	ebp,16
	and	edx,64*63

	and	ebp,63
	mov	esi,[_ds_source]

	or	ebp,edx
	mov	edx,[_ds_xstep]

	add	ebx,edx
	mov	edx,[_ds_ystep]

	mov	al,[esi+ebp]
	mov	esi,[_ds_colormap]

	add	ecx,edx
	mov	ebp,ebx

	mov	edx,ecx
	mov	al,[esi+eax]

	shr	edx,10
s1:	mov	[edi+1],al

	shr	ebp,16
	mov	esi,[lastAddress]

	and	ebp,63
s2:	add	edi,2

	cmp	edi,esi
	jne	near ds8loo

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp

	ret

;************************

GLOBAL	_ASM_PatchPitch

_ASM_PatchPitch:
	mov	edx,[_dc_pitch]

	; 1 * dc_pitch
	mov	[rdcp1+2],edx
	mov	[f1a+3],edx
	mov	[f1b+2],edx
	mov	[l1a+2],edx
	mov	[l1b+2],edx

	; 2 * dc_pitch
	add	edx,[_dc_pitch]
	mov	[f2a+3],edx
	mov	[f2b+2],edx
	mov	[l2a+2],edx
	mov	[l2b+2],edx

	; 3 * dc_pitch
	add	edx,[_dc_pitch]
	mov	[f3a+3],edx
	mov	[f3b+2],edx
	mov	[l3a+2],edx
	mov	[l3b+2],edx

	; 4 * dc_pitch
	add	edx,[_dc_pitch]
	mov	[f4+2],edx
	mov	[l4+2],edx

	ret


;*----------------------------------------------------------------------
;*
;* R_DrawColumnP
;*
;* I don't use Doom Legacy's assembly R_DrawColumn() anymore.
;* This routine avoids the AGI stalls of Legacy's routine, so on
;* Pentium's it really isn't that much slower. On PPros and above
;* it still suffers from just as many partial register stalls, though.
;*
;*----------------------------------------------------------------------

GLOBAL	_R_DrawColumnP_ASM

_R_DrawColumnP_ASM:

; count = dc_yh - dc_yl;

	mov	eax,[_dc_yh]
	 mov	ecx,[_dc_yl]
	sub	eax,ecx
	 mov	edx,[_ylookup]

	jl	near rdcpret		; count < 0: nothing to do, so leave

	push	ebp			; save registers
	 push	ebx
	push	edi
	 push	esi

; dest = ylookup[dc_yl] + columnofs[dc_x];

	mov	edi,[edx+ecx*4]
	 mov	edx,[_columnofs]
	mov	ebx,[_dc_x]
	 inc	eax			; make 0 count mean 0 pixels
	add	edi,[edx+ebx*4]		; edi = top of destination column
	 sub	ecx,[_centery]		; ecx = dc_yl - centery

	imul	eax,[_dc_pitch]		; Start turning the counter into an index

	add	edi,eax			; Point edi to the bottom of the destination column
	 mov	edx,[_dc_iscale]

; frac = dc_texturemid + (dc_yl - centery) * fracstep

	imul	ecx,edx

	shr	edx,16
	 add	ecx,[_dc_texturemid]	; ecx = frac (all 32 bits)
	mov	ebx,ecx
	 mov	ebp,0
	shl	ebx,16
	 sub	ebp,eax			; ebp = counter (counts up to 0 in pitch increments)
	shr	ecx,16
	 mov	eax,[_dc_mask]
	and	ecx,eax
	 or	ebx,edx			; Put fracstep integral part into bl
	shl	eax,8
	 mov	edx,[_dc_iscale]
	shl	edx,16
	 mov	esi,[_dc_source]
	or	ebx,eax			; Put mask byte into bh
	 mov	eax,[_dc_colormap]
	sub	edi,[_dc_pitch]

; The registers should now look like this:
;
;	[31  ..  16][15 .. 8][7 .. 0]
; eax	[colormap	    ][texel ]
; ebx	[yf	   ][mask   ][dyi   ]
; ecx	[0		    ][yi    ]
; edx	[dyf	   ][	    ][      ]
; esi	[source texture column	    ]
; edi	[destination screen pointer ]
; ebp	[counter (adds up)	    ]
;
; Unfortunately, this arrangement is going to produce
; lots of partial register stalls on anything better
; than a Pentium. :-(

	align	16
rdcploop:
	mov	al,[esi+ecx]		; Fetch texel
	 add	ebx,edx			; increment frac fractional part
	adc	cl,bl			; increment frac integral part
rdcp1:	 add	ebp,0x12345678		; increment counter
	mov	al,[eax]		; colormap texel
	 and	cl,bh
	mov	[edi+ebp],al		; Store texel
	 test	ebp,ebp
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
;* Based very loosely on the above R_DrawColumn code. This assumes
;* that the fuzztable is some 2^n size, which it is not in the
;* original DOOM.
;*
;*----------------------------------------------------------------------

GLOBAL _R_DrawFuzzColumnP_ASM

_R_DrawFuzzColumnP_ASM:
; Adjust borders. Low...
	mov	eax,[_dc_yl]
	 push	ebx
	push	esi
	 push	edi

	cmp	eax,0
	 jg	.ylok

	mov	eax,1

; ...and high.
.ylok	mov	edx,[_realviewheight]
	 mov	esi,[_dc_yh]
	lea	ecx,[edx-1]
	cmp	esi,ecx
	 jl	.yhok

	lea	esi,[edx-2]

.yhok	sub	esi,eax			; esi = count
	js	near dfcdone		; Zero length (or less)

	mov	ecx,[_ylookup]
	 mov	edx,[_dc_x]
	; AGI stall
	mov	edi,[ecx+eax*4]
	 mov	ecx,[_columnofs]
	; AGI stall
	mov	ebx,[ecx+edx*4]
	 mov	eax,[_DefaultPalette]
	mov	ecx,[_fuzzpos]
	 add	edi,ebx
	inc	esi
	 mov	eax,[eax+8]

	mov	ebx,esi
	 add	eax,6*256
	shr	esi,2
	 shl	ebx,8
	test	bh,3
	 jz	fquadloop
;
; esi = count
; edi = dest
; ecx = fuzzpos
; eax = colormap 6 (256-byte aligned)
;

; do odd pixel (if any)
	test	bh,1
	 jz	.oddid

	mov	edx,[_fuzzoffset+ecx*4]
	 inc	ecx
	mov	al,[edi+edx]
	 and	ecx,FUZZTABLE-1
	mov	bl,[eax]
	mov	[edi],bl
	 add	edi,[_dc_pitch]

; do two non-dword aligned pixels (if any)
.oddid	test	bh,2
	 jz	.undid

	mov	edx,[_fuzzoffset+ecx*4]
	 inc	ecx
	mov	al,[edi+edx]
	 and	ecx,FUZZTABLE-1
	mov	bl,[eax]
	 mov	edx,[_fuzzoffset+ecx*4]
	mov	[edi],bl
	 add	edi,[_dc_pitch]

	inc	ecx
	 mov	al,[edi+edx]
	and	ecx,FUZZTABLE-1
	 mov	bl,[eax]
	mov	[edi],bl
	 add	edi,[_dc_pitch]

; make sure we still have some pixels left to do
.undid	test	esi,esi
	 jz	savefuzzpos

fquadloop:
	mov	edx,[_fuzzoffset+ecx*4]
	 inc	ecx
	mov	al,[edi+edx]		; AGI stall
	 and	ecx,FUZZTABLE-1
	mov	bl,[eax]		; AGI stall
	 mov	edx,[_fuzzoffset+ecx*4]
	mov	[edi],bl

	 inc	ecx
f1a:	mov	al,[edi+edx+0x12345678]
	 and	ecx,FUZZTABLE-1
	mov	bl,[eax]
	 mov	edx,[_fuzzoffset+ecx*4]
f1b:	mov	[edi+0x12345678],bl

	 inc	ecx
f2a:	mov	al,[edi+edx+2*0x12345678]
	 and	ecx,FUZZTABLE-1
	mov	bl,[eax]
	 mov	edx,[_fuzzoffset+ecx*4]
f2b:	mov	[edi+2*0x12345678],bl

	 inc	ecx
f3a:	mov	al,[edi+edx+3*0x12345678]
	 and	ecx,FUZZTABLE-1
	mov	bl,[eax]
	 dec	esi
f3b:	mov	[edi+3*0x12345678],bl

f4:	 lea	edi,[edi+4*0x12345678]
	jnz	fquadloop

savefuzzpos:
	add	ecx,3
	and	ecx,FUZZTABLE-1
	mov	[_fuzzpos],ecx
dfcdone:
	pop	edi
	pop	esi
	pop	ebx
	ret

;*----------------------------------------------------------------------
;*
;* R_DrawTranslucentColumnP
;*
;* This is a modified version of R_DrawColumn above. The actual
;* inner loop code is mine. Significantly, unlike the version of
;* this function in Doom Legacy 1.11, this one properly indexes
;* into dc_colormap.
;*
;*----------------------------------------------------------------------

GLOBAL _R_DrawTranslucentColumnP_ASM

_R_DrawTranslucentColumnP_ASM:
;
; dest = ylookup[dc_yl] + columnofs[dc_x];
;
	push	ebp
	push	ebx
	push	edi
	push	esi

	mov	ebp,[_dc_yl]
;
; pixelcount = yh - yl + 1
;
	mov	eax,[_dc_yh]
	mov	ebx,ebp
	inc	eax
	sub	eax,ebp				; pixel count
	mov	[pixelcount],eax		; save for final pixel
	jle	near ldone			; nothing to scale

	mov	edi,[_ylookup]
	push	ebp				; make space for ystep frac. later
	mov	edi,[edi+ebx*4]

	mov	esi,[_columnofs]
	mov	ebx,[_dc_x]
	add	edi,[esi+ebx*4]			; edi = dest


;
; frac = dc_texturemid - (centery-dc_yl)*fracstep;
;
	mov	ecx,[_dc_iscale]		; fracstep
	mov	eax,[_centery]
	sub	eax,ebp
	imul	eax,ecx
	mov	edx,[_dc_texturemid]
	sub	edx,eax
	mov	ebx,edx
	shr	ebx,16				; frac int.
	and	ebx,[_dc_mask]
	shl	edx,16				; y frac up

	mov	ebp,ecx
	shl	ebp,16				; fracstep f. up
	shr	ecx,16				; fracstep i. ->cl
	and	cl,[_dc_mask]

	mov	esi,[_dc_source]

;
; lets rock :) !
;
	mov	eax,[pixelcount]
	mov	[esp],ebp
	mov	ebp,edx
	mov	ch,al
	shr	eax,2
	mov	edx,[_dc_transmap]
	test	ch,3
	jz	.l4quadloop
	mov	eax,[_dc_colormap]

;
; [esp] : ystep frac. upper 24 bits
;  ebp  : y     frac. upper 24 bits
;  ebx  : y     i.    lower 8 bits,  masked for index
;  ecx  : ch = counter, cl = y step i.
;  eax  : colormap aligned 256
;  edx  : transtable aligned 65536
;  esi  : source texture column
;  edi  : dest screen
;

;
;  do un-even pixel
;
	test	ch,1
	jz	.lc2

	mov	al,[esi+ebx]
	 mov	dh,[edi]
	mov	dl,[eax]
	 add	ebp,[esp]
	mov	al,[edx]
	 adc	bl,cl
	mov	[edi],al
	 and	ebx,[_dc_mask]
	add	edi,[_dc_pitch]


;
;  do two non-quad-aligned pixels
;
.lc2:
	test	ch,2
	jz	.lc3

	mov	al,[esi+ebx]
	 mov	dh,[edi]
	mov	dl,[eax]
	 add	ebp,[esp]
	mov	al,[edx]
	 adc	bl,cl
	mov	[edi],al
	 and	ebx,[_dc_mask]

	mov	al,[esi+ebx]
	 mov	dh,[edi]
	add	edi,[_dc_pitch]
	 mov	dl,[eax]
	add	ebp,[esp]
	 mov	al,[edx]
	adc	bl,cl
	 mov	[edi],al
	and	ebx,[_dc_mask]
	 add	edi,[_dc_pitch]

;
;  test if there were at least 4 pixels
;
.lc3:
	mov	eax,[pixelcount]
	shr	eax,2
	test	eax,eax				; test quad count
	jz	near ldone

.l4quadloop:
	mov	ch,al
	mov	eax,[_dc_colormap]

;	.align  4
lquadloop:
	mov	al,[esi+ebx]
	 mov	dh,[edi]
	mov	dl,[eax]
	 add	ebp,[esp]
	mov	al,[edx]
	 adc	bl,cl
	mov	[edi],al
	 and	ebx,[_dc_mask]

	mov	al,[esi+ebx]
l1a:	 mov	dh,[edi+0x12345678]
	mov	dl,[eax]
	 add	ebp,[esp]
	mov	al,[edx]
	 adc	bl,cl
l1b:	mov	[edi+0x12345678],al
	 and	ebx,[_dc_mask]

	mov	al,[esi+ebx]
l2a:	 mov	dh,[edi+2*0x12345678]
	mov	dl,[eax]
	 add	ebp,[esp]
	mov	al,[edx]
	 adc	bl,cl
l2b:	mov	[edi+2*0x12345678],al
	 and	ebx,[_dc_mask]

	mov	al,[esi+ebx]
l3a:	 mov	dh,[edi+3*0x12345678]
	mov	dl,[eax]
	 add	ebp,[esp]
	mov	al,[edx]
	 adc	bl,cl
l3b:	mov	[edi+3*0x12345678],al
	 and	ebx,[_dc_mask]

l4:	add	edi,4*0x12345678

	 dec	ch
	jnz	lquadloop

ldone:
	pop	ebp
	pop	esi
	pop	edi
	pop	ebx
	pop	ebp
	ret
