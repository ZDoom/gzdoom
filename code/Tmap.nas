;* Here are the texture-mapping inner loops in pure assembly
;* language. DrawSpan8Loop is from NTDOOM.
;*
;* Since discovering the Win32 Demos FAQ, self-modifying code
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

EXTERN	_dc_ctspan
EXTERN	_dc_temp

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
GLOBAL	@ASM_PatchColSize@0

EXTERN _PatchUnrolledColSize

_ASM_PatchColSize:
@ASM_PatchColSize@0:
	mov	ecx,[_ds_colshift]
	mov	edx,[_ds_colsize]
	mov	[s0+2],cl
	mov	[s1+2],dl
	add	edx,edx
	mov	[s2+2],edx
	jmp	_PatchUnrolledColSize

;*----------------------------------------------------------------------
;*
;* DrawSpan8Loop Written by Petteri Kangaslampi <pekangas@sci.fi>
;* and Donated to the public domain.
;*
;*----------------------------------------------------------------------

GLOBAL	_DrawSpan8Loop

;void DrawSpan8Loop(fixed_t xfrac, fixed_t yfrac, int count, byte *dest);
	align 16

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
GLOBAL	@ASM_PatchPitch@0

EXTERN	_PatchUnrolled

_ASM_PatchPitch:
@ASM_PatchPitch@0:
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

	jmp	_PatchUnrolled


;*----------------------------------------------------------------------
;*
;* R_DrawColumnP
;*
;*----------------------------------------------------------------------

GLOBAL	_R_DrawColumnP_ASM
GLOBAL	@R_DrawColumnP_ASM@0

	align 16

_R_DrawColumnP_ASM:
@R_DrawColumnP_ASM@0:

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
;* This code assumes that the fuzztable is some 2^n size, which it is
;* not in the original DOOM.
;*
;*----------------------------------------------------------------------

GLOBAL	_R_DrawFuzzColumnP_ASM
GLOBAL	@R_DrawFuzzColumnP_ASM@0

	align 16

_R_DrawFuzzColumnP_ASM:
@R_DrawFuzzColumnP_ASM@0:

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
;*----------------------------------------------------------------------

GLOBAL	_R_DrawTranslucentColumnP_ASM
GLOBAL	@R_DrawTranslucentColumnP_ASM@0

	align 16

_R_DrawTranslucentColumnP_ASM:
@R_DrawTranslucentColumnP_ASM@0:
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


;*----------------------------------------------------------------------
;*
;* R_DrawColumnHorizP_ASM
;*
;*----------------------------------------------------------------------

GLOBAL	_R_DrawColumnHorizP_ASM
GLOBAL	@R_DrawColumnHorizP_ASM@0

	align 16

@R_DrawColumnHorizP_ASM@0:
_R_DrawColumnHorizP_ASM:

; count = dc_yh - dc_yl;

	mov	eax,[_dc_yh]
	mov	ecx,[_dc_yl]
	sub	eax,ecx
	mov	edx,[_dc_x]

	jl	near .leave		; count < 0: nothing to do, so leave

	push	ebp			; save registers
	push	ebx
	push	edi
	push	esi

; dest = ylookup[dc_yl] + columnofs[dc_x];

	inc	eax			; make 0 count mean 0 pixels
	 and	edx,3
	push	eax
	 mov	esi,[_dc_ctspan+edx*4]
	lea	eax,[_dc_temp+ecx*4+edx] ; eax = top of column in buffer
	 mov	ebp,[_dc_yh]
	mov	ebx,[_centery]
	 mov	[esi],ecx
	sub	ecx,ebx			; ecx = dc_yl - centery
	 mov	[esi+4],ebp
	add	esi,8
	 mov	[_dc_ctspan+edx*4],esi

; frac = dc_texturemid + (dc_yl - centery) * fracstep

	mov	esi,[_dc_iscale]
	imul	ecx,esi
	add	ecx,[_dc_texturemid]	; ecx = frac (all 32 bits)
	 mov	ebx,[_dc_mask]
	shl	ecx,8
	 mov	edi,[_dc_source]
	shl	esi,8
	 or	ecx,ebx
	mov	dl,[edi]		; load cache
	 mov	ebx,[esp]
	and	ebx,0xfffffff8
	 jnz	.mthan8

; Register usage in the following code is:
;
; eax: dest
; edi: source
; ecx: frac (8.16)/mask (..8)
; esi: fracstep (8.24)
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
	shr	ebp,24			; shift frac over to low byte
	 add	eax,4
	and	ebp,ecx			; mask it
	mov	dl,[edi+ebp]
	mov	[eax-4],dl

.even	test	ebx,ebx
	jz	near .done

.loop2	mov	[esp],ebx		; save counter
	 mov	ebx,ecx			; copy frac for texel1 to ebx
	shr	ebx,24			; shift frac for texel1 to low byte
	 add	ecx,esi			; increment frac
	mov	ebp,ecx			; copy frac for texel2 to ebp
	 and	ebx,ecx			; mask frac for texel1
	shr	ebp,24			; shift frac for texel2 to low byte
	 add	ecx,esi			; increment frac
	and	ebp,ecx			; mask frac for texel2
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
	shr	ebp,24			; frac: shift
	 add	eax,4			; increment dest
	and	ebp,ecx			; frac: mask
	 mov	ebx,[esp]		; fetch counter
	mov	dl,[edi+ebp]		; tex:  read
	 dec	ebx			; decrement counter
	mov	[eax-4],dl		; tex:  write
	 mov	[esp],ebx		; store counter

.try2	test	eax,8
	jz	.try4

	mov	ebx,ecx			; frac1: in ebx
	 add	ecx,esi			; step
	shr	ebx,24			; frac1: shift
	 mov	ebp,ecx			; frac2: in ebp
	shr	ebp,24			; frac2: shift
	 and	ebx,ecx			; frac1: mask
	and	ebp,ecx			; frac2: mask
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
	shr	ebx,24			; frac1: shift
	 mov	ebp,ecx			; frac2: in ebp
	shr	ebp,24			; frac2: shift
	 and	ebx,ecx			; frac1: mask
	and	ebp,ecx			; frac2: mask
	 add	ecx,esi			; step
	mov	dl,[edi+ebx]		; tex1:  read
	 mov	ebx,ecx			; frac3: in ebx
	shr	ebx,24			; frac3: shift
	 mov	dh,[edi+ebp]		; tex2:  read
	add	ecx,esi			; step
	 mov	[eax],dl		; tex1:  write
	mov	[eax+4],dh		; tex2:  write
	 mov	ebp,ecx			; frac4: in ebp
	shr	ebp,24			; frac4: shift
	 and	ebx,ecx			; frac3: mask
	and	ebp,ecx			; frac4: mask
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
	shr	ebx,24			; frac1: shift
	 add	ecx,esi			; step
	mov	ebp,ecx			; frac2: in ebp
	 and	ebx,ecx			; frac1: mask
	shr	ebp,24			; frac2: shift
	 add	ecx,esi			; step
	and	ebp,ecx			; frac2: mask
	 mov	dl,[edi+ebx]		; tex1:  read
	mov	ebx,ecx			; frac3: in ebx
	 mov	dh,[edi+ebp]		; tex2:  read
	shr	ebx,24			; frac3: shift
	 add	ecx,esi			; step
	mov	[eax],dl		; tex1:  write
	 mov	[eax+4],dh		; tex2:  write
	mov	ebp,ecx			; frac4: in ebp
	 and	ebx,ecx			; frac3: mask
	shr	ebp,24			; frac4: shift
	 add	ecx,esi			; step
	and	ebp,ecx			; frac4: mask
	 mov	dl,[edi+ebx]		; tex3:  read
	mov	ebx,ecx			; frac5: in ebx
	 mov	dh,[edi+ebp]		; tex4:  read
	shr	ebx,24			; frac5: shift
	 mov	[eax+8],dl		; tex3:  write
	mov	[eax+12],dh		; tex4:  write
	 add	ecx,esi			; step
	mov	ebp,ecx			; frac6: in ebp
	 and	ebx,ecx			; frac5: mask
	shr	ebp,24			; frac6: shift
	 mov	dl,[edi+ebx]		; tex5:  read
	and	ebp,ecx			; frac6: mask
	 add	ecx,esi			; step
	mov	ebx,ecx			; frac7: in ebx
	 mov	[eax+16],dl		; tex5:  write
	shr	ebx,24			; frac7: shift
	 mov	dh,[edi+ebp]		; tex6:  read
	and	ebx,ecx			; frac7: mask
	 add	ecx,esi			; step
	mov	ebp,ecx			; frac8: in ebp
	 mov	[eax+20],dh		; tex6:  write
	shr	ebp,24			; frac8: shift
	 add	eax,32			; increment dest pointer
	and	ebp,ecx			; frac8: mask
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

GLOBAL	_rt_copy1col_asm
GLOBAL	@rt_copy1col_asm@16

	align 16

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
	mov	eax,[_columnofs]
	inc	ebx			; ebx = count
	mov	eax,[eax+edx*4]
	mov	edx,[_ylookup]
	lea	ecx,[_dc_temp+ecx+esi]	; ecx = source
	mov	edi,[edx+esi]
	mov	esi,[_dc_pitch]		; esi = pitch
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
;* rt_copy2cols_asm
;*
;* ecx = hx
;* edx = sx
;* [esp+4] = yl
;* [esp+8] = yh
;*
;*----------------------------------------------------------------------

GLOBAL	_rt_copy2cols_asm
GLOBAL	@rt_copy2cols_asm@16

	align 16

_rt_copy2cols_asm:
	pop	eax
	mov	edx,[esp+4*3]
	mov	ecx,[esp+4*2]
	push	edx
	push	ecx
	mov	ecx,[esp+4*2]
	mov	edx,[esp+4*3]
	push	eax

@rt_copy2cols_asm@16:
	mov	eax, [esp+4]
	push	ebx
	mov	ebx, [esp+12]
	push	esi
	sub	ebx, eax
	push	edi
	js	.done

	lea	esi,[eax*4]
	mov	eax,[_columnofs]
	inc	ebx			; ebx = count
	mov	eax,[eax+edx*4]
	mov	edx,[_ylookup]
	lea	ecx,[_dc_temp+ecx+esi]	; ecx = source
	mov	edi,[edx+esi]
	mov	edx,[_dc_pitch]		; edx = pitch
	add	eax,edi			; eax = dest

	shr	ebx,1
	jnc	.even

	mov	si,[ecx]
	add	ecx,4
	mov	[eax],si
	add	eax,edx

.even	and	ebx,ebx
	jz	.done

.loop	mov	si,[ecx]
	mov	di,[ecx+4]
	mov	[eax],si
	mov	[eax+edx],di
	add	ecx,8
	lea	eax,[eax+edx*2]
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

GLOBAL	_rt_copy4cols_asm
GLOBAL	@rt_copy4cols_asm@12

	align 16

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

	mov	eax,[_columnofs]
	inc	ebx			; ebx = count
	mov	eax,[eax+ecx*4]
	mov	ecx,[_ylookup]
	mov	esi,[ecx+edx*4]
	lea	ecx,[_dc_temp+edx*4]	; ecx = source
	mov	edx,[_dc_pitch]		; edx = pitch
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

GLOBAL	_rt_map1col_asm
GLOBAL	@rt_map1col_asm@16

	align 16

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
	mov	eax,[_columnofs]
	mov	esi,[_dc_colormap]		; esi = colormap
	inc	ebx				; ebx = count
	mov	eax,[eax+edx*4]
	mov	edx,[_ylookup]
	lea	ebp,[_dc_temp+ecx+edi]		; ebp = source
	mov	ecx,[edx+edi]
	mov	edi,[_dc_pitch]			; edi = pitch
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
;* rt_map2cols_asm
;*
;* ecx = hx
;* edx = sx
;* [esp+4] = yl
;* [esp+8] = yh
;*
;*----------------------------------------------------------------------

GLOBAL	_rt_map2cols_asm
GLOBAL	@rt_map2cols_asm@16

	align 16

_rt_map2cols_asm:
	pop	eax
	mov	edx,[esp+4*3]
	mov	ecx,[esp+4*2]
	push	edx
	push	ecx
	mov	ecx,[esp+4*2]
	mov	edx,[esp+4*3]
	push	eax

@rt_map2cols_asm@16:
	mov	eax,[esp+4]
	push	ebx
	mov	ebx,[esp+12]
	push	ebp
	push	esi
	sub	ebx, eax
	push	edi
	js	near .done

	lea	edi,[eax*4]
	mov	eax,[_columnofs]
	mov	esi,[_dc_colormap]		; esi = colormap
	inc	ebx				; ebx = count
	mov	eax,[eax+edx*4]
	mov	edx,[_ylookup]
	lea	ebp,[_dc_temp+ecx+edi]		; ebp = source
	mov	ecx,[edx+edi]
	mov	edi,[_dc_pitch]			; edi = pitch
	add	eax,ecx				; eax = dest
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
	add	eax,edi

.even	and	ebx,ebx
	jz	.done

.loop	mov	dl,[ebp]
	mov	cl,[ebp+1]
	mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	mov	[eax],dl
	mov	dl,[ebp+4]
	mov	[eax+1],cl
	mov	cl,[ebp+5]
	add	ebp,8
	mov	dl,[esi+edx]
	mov	cl,[esi+ecx]
	mov	[eax+edi],dl
	mov	[eax+edi+1],cl
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
;* ecx = sx
;* edx = yl
;* [esp+4] = yh
;*
;*----------------------------------------------------------------------

GLOBAL	_rt_map4cols_asm
GLOBAL	@rt_map4cols_asm@12

	align 16

_rt_map4cols_asm:
	pop	eax
	mov	ecx,[esp+8]
	mov	edx,[esp+4]
	push	ecx
	mov	ecx,[esp+4]
	push	eax

@rt_map4cols_asm@12:
	push	ebx
	mov	ebx,[esp+8]
	push	ebp
	push	esi
	sub	ebx,edx
	push	edi
	js	near .done

	mov	eax,[_columnofs]
	mov	esi,[_dc_colormap]	; esi = colormap
	shl	edx,2
	mov	eax,[eax+ecx*4]
	mov	ecx,[_ylookup]
	inc	ebx			; ebx = count
	mov	edi,[ecx+edx]
	lea	ebp,[_dc_temp+edx]	; ebp = source
	add	eax,edi			; eax = dest
	mov	edi,[_dc_pitch]		; edi = pitch
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
	 mov	dl,[ebp-1]
	mov	cl,[ebp-2]
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
