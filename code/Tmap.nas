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
EXTERN	_colormaps
EXTERN	_viewheight

EXTERN	_dc_colormap
EXTERN	_dc_transmap
EXTERN	_dc_iscale
EXTERN	_dc_texturemid
EXTERN	_dc_source
EXTERN	_dc_yl
EXTERN	_dc_yh
EXTERN	_dc_x

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

rowbytes	DD 0
pixelcount	DD 0

%ifdef M_TARGET_WATCOM
  SEGMENT CODE PUBLIC ALIGN=16 CLASS=CODE USE32
  SEGMENT CODE
%else
  SECTION .text
%endif


;*----------------------------------------------------------------------
;*
;* DrawSpan8Loop Written by Petteri Kangaslampi <pekangas@sci.fi>
;* and Donated to the public domain.
;*
;*----------------------------------------------------------------------

GLOBAL _DrawSpan8Loop

;void DrawSpan8Loop(fixed_t xfrac, fixed_t yfrac, int count, byte *dest);

_DrawSpan8Loop:
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

	; 1 * rowbytes
	mov	[p1+2],edx
	mov	[f1a+3],edx
	mov	[f1b+2],edx
	mov	[l1a+2],edx
	mov	[l1b+2],edx

	; 2 * rowbytes
	add	edx,[esp+4]
	mov	[p2+2],edx
	mov	[f2a+3],edx
	mov	[f2b+2],edx
	mov	[l2a+2],edx
	mov	[l2b+2],edx

	; 3 * rowbytes
	add	edx,[esp+4]
	mov	[p3+2],edx
	mov	[f3a+3],edx
	mov	[f3b+2],edx
	mov	[l3a+2],edx
	mov	[l3b+2],edx

	; 4 * rowbytes
	add	edx,[esp+4]
	mov	[p4+2],edx
	mov	[f4+2],edx
	mov	[l4+2],edx

	ret


;*----------------------------------------------------------------------
;*
;* R_DrawColumn
;*
;* Taken from Doom Legacy. Here's their info:
;*
;*   New optimised version 10-01-1998 by D.Fabrice and P.Boris
;*   TO DO: optimise it much farther... should take at most 3 cycles/pix
;*	    once it's fixed, add code to patch the offsets so that it
;*	    works in every screen width.
;*
;*----------------------------------------------------------------------

GLOBAL _R_DrawColumn_ASM

_R_DrawColumn_ASM:
;
; dest = ylookup[dc_yl] + columnofs[dc_x];
;
; [RH] I interleaved this with the register saving. Pairs on a Pentium. (Woohoo!)
	push	ebp
	mov	ebp,[_dc_yl]
	push	ebx
	mov	ebx,ebp
	push	edi
	mov	edi,[_ylookup]
	push	esi
	mov	edi,[edi+ebx*4]

	mov	esi,[_columnofs]
	mov	ebx,[_dc_x]
	add	edi,[esi+ebx*4]			; edi = dest

;
; pixelcount = yh - yl + 1
;
	mov	eax,[_dc_yh]
	inc	eax
	sub	eax,ebp				; pixel count
	mov	[pixelcount],eax		; save for final pixel
	jle	near vdone			; nothing to scale

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
	and	ebx,0x7f
	shl	edx,16				; y frac up

	mov	ebp,ecx
	shl	ebp,16				; fracstep f. up
	shr	ecx,16				; fracstep i. ->cl
	and	cl,0x7f

	mov	esi,[_dc_source]

;
; lets rock :) !
;
	mov	eax,[pixelcount]
	mov	dh,al
	shr	eax,2
	mov	ch,al				; quad count
	mov	eax,[_dc_colormap]
	test	dh,3
	jz	v4quadloop

;
;  do un-even pixel
;
	test	dh,1
	jz	dc2

	mov	al,[esi+ebx]			; prep un-even loops
	 add	edx,ebp				; ypos f += ystep f
	adc	bl,cl				; ypos i += ystep i
	 mov	dl,[eax]			; colormap texel
	and	bl,0x7f				; mask 0-127 texture index
	 mov	[edi],dl			; output pixel
	add	edi,[rowbytes]

;
;  do two non-quad-aligned pixels
;
dc2:
	test	dh,2
	jz	dc3

	mov	al,[esi+ebx]			; prep un-even loops
	 add	edx,ebp				; ypos f += ystep f
	adc	bl,cl				; ypos i += ystep i
	 mov	dl,[eax]			; colormap texel
	and	bl,0x7f				; mask 0-127 texture index
	 mov	[edi],dl			; output pixel

	mov	al,[esi+ebx]			; prep un-even loops
	 add	edx,ebp				; ypos f += ystep f
	adc	bl,cl				; ypos i += ystep i
	 mov	dl,[eax]			; colormap texel
	and	bl,0x7f				; mask 0-127 texture index
	 add	edi,[rowbytes]
	mov	[edi],dl			; output pixel

	add	edi,[rowbytes]

;
;  test if there was at least 4 pixels
;
dc3:
	test	ch,0xff				; test quad count
	jz	vdone

;
; ebp : ystep frac. upper 24 bits
; edx : y     frac. upper 24 bits
; ebx : y     i.    lower 7 bits,  masked for index
; ecx : ch = counter, cl = y step i.
; eax : colormap aligned 256
; esi : source texture column
; edi : dest screen
;
v4quadloop:
	mov	dh,0x7f				; prep mask

;	.align  4
vquadloop:
	mov	al,[esi+ebx]			; prep loop
	 add	edx,ebp				; ypos f += ystep f
	adc	bl,cl				; ypos i += ystep i
	 mov	dl,[eax]			; colormap texel
	mov	[edi],dl			; output pixel
	 and	bl,dh				; mask 0-127 texture index

	mov	al,[esi+ebx]			; fetch source texel
	 add	edx,ebp				; ypos f += ystep f
	adc	bl,cl				; ypos i += ystep i
	 mov	dl,[eax]			; colormap texel
p1:	mov	[edi+0x12345678],dl		; output pixel
	 and	bl,dh				; mask 0-127 texture index

	mov	al,[esi+ebx]			; fetch source texel
	 add	edx,ebp
	adc	bl,cl
	 mov	dl,[eax]
p2:	mov	[edi+2*0x12345678],dl
	 and	bl,dh

	mov	al,[esi+ebx]			; fetch source texel
	 add	edx,ebp
	adc	bl,cl
	 mov	dl,[eax]
p3:	mov	[edi+3*0x12345678],dl
	 and	bl,dh

p4:	add	edi,4*0x12345678

	 dec	ch
	jnz	vquadloop

vdone:
	pop	esi
	pop	edi
	pop	ebx
	pop	ebp
	ret

;*----------------------------------------------------------------------
;*
;* R_DrawFuzzColumn
;*
;* Based very loosely on the above R_DrawColumn code. This assumes
;* that the fuzztable is some 2^n size, which it is not in the
;* original DOOM.
;*
;*----------------------------------------------------------------------

GLOBAL _R_DrawFuzzColumn_ASM

_R_DrawFuzzColumn_ASM:
; Adjust borders. Low...
	mov	eax,[_dc_yl]
	push	ebx
	push	esi
	push	edi

	test	eax,eax
	jne	.ylok

	mov	eax,1
	mov	[_dc_yl],eax		; Necessary?

; ...and high.
.ylok	mov	edx,[_viewheight]
	mov	ecx,[_dc_yh]
	lea	esi,[edx-1]
	cmp	ecx,esi
	jne	.yhok

	lea	ecx,[edx-2]
	mov	[_dc_yh],ecx		; Necessary?

.yhok	mov	esi,ecx
	sub	esi,eax			; esi = count
	js	near dfcdone		; Zero length (or less)

	mov	ecx,[_ylookup]
	mov	edx,[_dc_yl]
	mov	edi,[ecx+edx*4]
	mov	edx,[_dc_x]
	mov	ecx,[_columnofs]
	mov	ebx,[ecx+edx*4]
	mov	ecx,[_fuzzpos]
	mov	eax,[_colormaps]
	add	edi,ebx
	inc	esi
	add	eax,6*256

	mov	ebx,esi
	shr	esi,2
	test	ebx,3
	jz	fquadloop
;
; esi = count
; edi = dest
; ecx = fuzzpos
; eax = colormap 6 (256-byte aligned)
;

; do odd pixel (if any)
	test	ebx,1
	jz	.oddid

	mov	edx,[_fuzzoffset+ecx*4]
	 inc	ecx
	mov	al,[edi+edx]
	 and	ecx,FUZZTABLE-1
	mov	bl,[eax]
	mov	[edi],bl
	 add	edi,[rowbytes]

; do two non-dword aligned pixels (if any)
.oddid	test	ebx,2
	jz	.undid

	mov	edx,[_fuzzoffset+ecx*4]
	 inc	ecx
	mov	al,[edi+edx]
	 and	ecx,FUZZTABLE-1
	mov	bl,[eax]
	 mov	edx,[_fuzzoffset+ecx*4]
	mov	[edi],bl
	 add	edi,[rowbytes]

	inc	ecx
	 mov	al,[edi+edx]
	and	ecx,FUZZTABLE-1
	 mov	bl,[eax]
	mov	[edi],bl
	 add	edi,[rowbytes]

; make sure we still have some pixels left to do
.undid	test	esi,esi
	jz	savefuzzpos

fquadloop:
	mov	edx,[_fuzzoffset+ecx*4]
	 inc	ecx
	mov	al,[edi+edx]
	 and	ecx,FUZZTABLE-1
	mov	bl,[eax]
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
;* R_DrawTranslucentColumn
;*
;* This is a modified version of R_DrawColumn above. The actual
;* inner loop code is mine. Significantly, unlike the version of
;* this function in Doom Legacy 1.11, this one properly indexes
;* into dc_colormap.
;*
;*----------------------------------------------------------------------

GLOBAL _R_DrawTranslucentColumn_ASM

_R_DrawTranslucentColumn_ASM:
;
; dest = ylookup[dc_yl] + columnofs[dc_x];
;
	push	ebp
	mov	ebp,[_dc_yl]
	push	ebx
	mov	ebx,ebp
	push	edi
	mov	edi,[_ylookup]
	push	esi
	mov	edi,[edi+ebx*4]

	push	ebp				; make space for ystep frac. later

	mov	esi,[_columnofs]
	mov	ebx,[_dc_x]
	add	edi,[esi+ebx*4]			; edi = dest

;
; pixelcount = yh - yl + 1
;
	mov	eax,[_dc_yh]
	inc	eax
	sub	eax,ebp				; pixel count
	mov	[pixelcount],eax		; save for final pixel
	jle	near ldone			; nothing to scale

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
	and	ebx,0x7f
	shl	edx,16				; y frac up

	mov	ebp,ecx
	shl	ebp,16				; fracstep f. up
	shr	ecx,16				; fracstep i. ->cl
	and	cl,0x7f

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
;  ebx  : y     i.    lower 7 bits,  masked for index
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
	 and	ebx,byte 0x7f
	add	edi,[rowbytes]


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
	 and	ebx,byte 0x7f

	mov	al,[esi+ebx]
	 mov	dh,[edi]
	add	edi,[rowbytes]
	 mov	dl,[eax]
	add	ebp,[esp]
	 mov	al,[edx]
	adc	bl,cl
	 mov	[edi],al
	and	ebx,byte 0x7f
	 add	edi,[rowbytes]

;
;  test if there were at least 4 pixels
;
.lc3:
	mov	eax,[pixelcount]
	shr	eax,2
	test	eax,eax				; test quad count
	jz	ldone

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
	 and	ebx,byte 0x7f

	mov	al,[esi+ebx]
l1a:	 mov	dh,[edi+0x12345678]
	mov	dl,[eax]
	 add	ebp,[esp]
	mov	al,[edx]
	 adc	bl,cl
l1b:	mov	[edi+0x12345678],al
	 and	ebx,byte 0x7f

	mov	al,[esi+ebx]
l2a:	 mov	dh,[edi+2*0x12345678]
	mov	dl,[eax]
	 add	ebp,[esp]
	mov	al,[edx]
	 adc	bl,cl
l2b:	mov	[edi+2*0x12345678],al
	 and	ebx,byte 0x7f

	mov	al,[esi+ebx]
l3a:	 mov	dh,[edi+3*0x12345678]
	mov	dl,[eax]
	 add	ebp,[esp]
	mov	al,[edx]
	 adc	bl,cl
l3b:	mov	[edi+3*0x12345678],al
	 and	ebx,byte 0x7f

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
