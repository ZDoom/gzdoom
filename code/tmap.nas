;* DrawSpan8Loop Written by Petteri Kangaslampi <pekangas@sci.fi>
;* and Donated to the public domain.
;*
;* ASM_R_DrawTranslucentColumn is from Doom Legacy
;* ASM_R_DrawColumn is from dosdoom

BITS 32

; Segment/section definition macros. 

%ifdef M_TARGET_WATCOM
  SEGMENT DATA PUBLIC ALIGN=16 CLASS=DATA USE32
  SEGMENT DATA
%else
  SECTION .data
%endif

; External variables:
EXTERN _columnofs
EXTERN _TransTable
EXTERN _ylookup
EXTERN _centery

EXTERN _dc_colormap
EXTERN _dc_iscale
EXTERN _dc_texturemid
EXTERN _dc_source
EXTERN _dc_yl
EXTERN _dc_yh
EXTERN _dc_x

EXTERN _ds_xstep
EXTERN _ds_ystep
EXTERN _ds_colormap
EXTERN _ds_source
EXTERN _ds_x1
EXTERN _ds_x2
EXTERN _ds_xfrac
EXTERN _ds_yfrac
EXTERN _ds_y




; Local stuff:
lastAddress	DD 0

rowbytes	DD 0
loopcount	DD 0
pixelcount	DD 0
tystep		DD 0


%ifdef M_TARGET_WATCOM
  SEGMENT CODE PUBLIC ALIGN=16 CLASS=CODE USE32
  SEGMENT CODE
%else
  SECTION .text
%endif


GLOBAL _ASM_DrawSpan8Loop

;void DrawSpan8Loop(fixed_t xfrac, fixed_t yfrac, int count, byte *dest);

_ASM_DrawSpan8Loop:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	ebx,[esp+20]	; xfrac
	mov	ecx,[esp+24]	; yfrac
	mov	eax,[esp+28]	; count
	mov	edi,[esp+32]	; dest
	add	eax,edi
	mov	[lastAddress],eax
	
	mov	ebp,ebx
	mov	edx,ecx
	shr	edx,10
	and	ebp,65536*63

	shr	ebp,16
	xor	eax,eax

.loo:
	and	edx,64*63
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
	mov	[edi+1],al

	shr	ebp,16
	mov	esi,[lastAddress]

	and	ebp,63
	add	edi,2

	cmp	edi,esi
	jne	near .loo

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp

	ret


;************************

GLOBAL	_ASM_PatchRowBytes

; void ASM_PatchRowBytes (int bytes);

_ASM_PatchRowBytes:
	mov	edx,[esp+4]
	mov	[rowbytes],edx

	; added:28-01-98:this is really crappy but I want to get it working...
	;		 I'll clean this later

	; 1 * rowbytes
	mov	[p5+2],edx
	mov	[rdc8width1+2],edx
	mov	[rdc8width2+2],edx

	; 2 * rowbytes
	add	edx,[esp+4]
	mov	[p6+2],edx
	mov	[p7+2],edx
	mov	[p8+2],edx
	mov	[p9+2],edx

	ret

;* GACK! Self-modifying code needs to go in the data
;* segment to avoid a segmentation fault. Very ugly.

	SECTION	.data

GLOBAL	_MutantCodeStart
GLOBAL	_MutantCodeEnd

_MutantCodeStart:


;*----------------------------------------------------------------------
;*
;* R_DrawColumn
;*
;*   "leet asm version" from dosdoom/r_draw1.c
;*
;*   Note: Doom Legacy unrolls this. This is probably of trivial
;*	performance gain on Pentiums and above due to branch prediction.
;*
;*----------------------------------------------------------------------

GLOBAL _ASM_R_DrawColumn

; void ASM_R_DrawColumn (void);

_ASM_R_DrawColumn:
	push	ebp
	push	esi
	push	edi

	mov	edx,[_dc_yl]
	mov	eax,[_dc_yh]
	sub	eax,edx
	lea	ebx,[eax+1]
	test	ebx,ebx
	jle	rdc8done

	mov	eax,[_dc_x]
	mov	esi,[_ylookup+edx*4]
	add	esi,[_columnofs+eax*4]
	mov	edi,[_dc_iscale]
	mov	eax,edx
	sub	eax,[_centery]
	imul	eax,edi
	mov	ecx,[_dc_texturemid]
	add	ecx,eax

	mov	ebp,[_dc_source]
rdc8width1:
	sub	esi,0x12345678

;	.align 4,0x90
	times ($$-$) & 3 nop
rdc8loop:
	mov	eax,ecx
	xor	edx,edx
	shr	eax,16
	add	ecx,edi
	and	eax,0x7f
rdc8width2:
	add	esi,0x12345678
	mov	dl,[eax+ebp]
	mov	eax,[_dc_colormap]
	mov	al,[eax+edx]
	mov	[esi],al
	dec	ebx
	jne	rdc8loop
rdc8done:
	pop	edi
	pop	esi
	pop	ebp
	ret


;*----------------------------------------------------------------------
;*
;* R_DrawTranslucentColumn
;*
;* Vertical column texture drawer, with translucency.
;* Known problem: This does not index into colormap.
;*	Implications: When using an invulnerability, sprites
;*	will still be colored. Also, they are always drawn
;*	fullbright.
;*
;*----------------------------------------------------------------------

GLOBAL	_ASM_R_DrawTranslucentColumn

; void R_DrawTranslucentColumn (void);

_ASM_R_DrawTranslucentColumn:
	push	ebp			; preserve caller's stack frame pointer
	push	esi			; preserve register variables
	push	edi
	push	ebx

;
; dest = ylookup[dc_yl] + columnofs[dc_x];
;
	mov	ebp,[_dc_yl]
	mov	ebx,ebp
	mov	edi,[_ylookup+ebx*4]
	mov	ebx,[_dc_x]
	add	edi,[_columnofs+ebx*4]	; edi = des

;
; pixelcount = yh - yl + 1
;
	mov	eax,[_dc_yh]
	inc	eax
	sub	eax,ebp			; pixel count
	mov	[pixelcount],eax	; save for final pixel
	jle	near vtdone		; nothing to scale

;
; frac = dc_texturemid - (centery-dc_yl)*fracstep;
;
	mov	ecx,[_dc_iscale]	; fracstep
	mov	eax,[_centery]
	sub	eax,ebp
	imul	eax,ecx
	mov	edx,[_dc_texturemid]
	sub	edx,eax
	mov	ebx,edx
	shr	ebx,16			; frac int.
	and	ebx,byte 0x7f
	shl	edx,16			; y frac up

	mov	ebp,ecx
	shl	ebp,16			; fracstep f. up
	shr	ecx,16			; fracstep i. ->cl
	and	cl,0x7f

	mov	esi,[_dc_source]

;
; lets rock :) !
;
	mov	eax,[pixelcount]
	mov	dh,al
	shr	eax,2
	mov	ch,al			; quad count
	mov	eax,[_TransTable]
	test	dh,0x03
	jz	vt4quadloop

;
; do un-even pixel
;
	test	dh,1
	jz	.do2

	mov	ah,[esi+ebx]		; fetch texel : colormap number
	add	edx,ebp
	adc	bl,cl
	mov	al,[edi]		; fetch dest  : index into colormap
	and	bl,0x7f
	mov	dl,[eax]
	mov	[edi],dl
	add	edi,[rowbytes]

;
; do two non-quad-aligned pixels
;
.do2:
	test	dh,2
	jz	.do3

	mov	ah,[esi+ebx]		; fetch texel : colormap number
	add	edx,ebp
	adc	bl,cl
	mov	al,[edi]		; fetch dest  : index into colormap
	and	bl,0x7f
	mov	dl,[eax]
	mov	[edi],dl
	add	edi,[rowbytes]

	mov	ah,[esi+ebx]		; fetch texel : colormap number
	add	edx,ebp
	adc	bl,cl
	mov	al,[edi]		; fetch dest  : index into colormap
	and	bl,0x7f
	mov	dl,[eax]
	mov	[edi],dl
	add	edi,[rowbytes]

;
; test if there were at least 4 pixels
;
.do3:
	test	ch,0xff			; test quad count
	jz	near vtdone

;
; ebp : ystep frac. upper 24 bits
; edx : y     frac. upper 24 bits
; ebx : y     i.    lower 7 bits,  masked for index
; ecx : ch = counter, cl = y step i.
; eax : translucency table aligned 65536
; esi : source texture column
; edi : dest screen
;
vt4quadloop:
	mov	dh,0x7f			; prep mask

	mov	ah,[esi+ebx]		; fetch texel : colormap number
p5:	mov	al,[edi+0x12345678]	; fetch dest : index into colormap

	mov	[tystep],ebp
	mov	ebp,edi
	sub	edi,[rowbytes]
	jmp	inloop
;	.align  4
vtquadloop:
	add	edx,[tystep]
	adc	bl,cl
	and	bl,dh
p6:	add	ebp,2*0x12345678
	mov	dl,[eax]
	mov	ah,[esi+ebx]		; fetch texel : colormap number
	mov	[edi],dl
	mov	al,[ebp]		; fetch dest  : index into colormap
inloop:
	add	edx,[tystep]
	adc	bl,cl
	and	bl,dh
p7:	add	edi,2*0x12345678
	mov	dl,[eax]
	mov	ah,[esi+ebx]		; fetch texel : colormap number
	mov	[ebp],dl
	mov	al,[edi]		; fetch dest  : index into colormap

	add	edx,[tystep]
	adc	bl,cl
	and	bl,dh
p8:	add	ebp,2*0x12345678
	mov	dl,[eax]
	mov	ah,[esi+ebx]		; fetch texel : colormap number
	mov	[edi],dl
	mov	al,[ebp]		; fetch dest  : index into colormap

	add	edx,[tystep]
	adc	bl,cl
	and	bl,dh
p9:	add	edi,2*0x12345678
	mov	dl,[eax]
	mov	ah,[esi+ebx]		; fetch texel : colormap number
	mov	[ebp],dl
	mov	al,[edi]		; fetch dest  : index into colormap

	dec	ch
	jnz	vtquadloop

vtdone:
	pop	ebx			; restore register variables
	pop	edi
	pop	esi
	pop	ebp			; restore caller's stack frame pointer
	ret

_MutantCodeEnd: