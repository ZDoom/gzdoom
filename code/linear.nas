;****************************************************************************
;*
;* Unwound vertical and horizontal scaling code based loosing on linear.asm
;* from the Heretic/Hexen source. This is probably as optimal as your going
;* to get on a 486. I've taken the liberty of bumping the screen height up
;* to 240 (so it will work with resolutions up to 320x240) and adding in
;* the use of dc_mask.
;*
;****************************************************************************

bits 32

%define SCREENWIDTH	320
%define SCREENHEIGHT	240

extern	_dc_colormap
extern	_dc_x
extern	_dc_yl
extern	_dc_yh
extern	_dc_iscale
extern	_dc_texturemid
extern	_dc_source
extern	_dc_mask
extern	_dc_pitch

extern	_ylookup
extern	_columnofs
extern	_centery

extern	_ds_y
extern	_ds_x1
extern	_ds_x2
extern	_ds_colormap
extern	_ds_xfrac
extern	_ds_yfrac
extern	_ds_xstep
extern	_ds_ystep
extern	_ds_source
extern	_ds_colsize

%define PUSHR	pushad
%define POPR	popad

;============================================================================
;
; unwound vertical scaling code
;
; eax	light table pointer, 0 lowbyte overwritten
; ebx	integral step value
; ecx	integral scale value
; edx	fractional scale value, lowbyte has mask
; esi	start of source pixels
; edi	bottom pixel in screenbuffer to blit into
; ebp	fractional step value
;
; ebx should be set to 0 0 0 dh to feed the pipeline
;
; The graphics wrap vertically at 128 pixels
;============================================================================

	section	.data

%macro	SCALEDEFINE 1
	dd	vscale%1
%endmacro

%define scalesize	(vscale2-vscale3)
%define scalepatchoffs	(11)

	align 4

scalecalls:
%assign	LINE	0
%rep	SCREENHEIGHT+1
	SCALEDEFINE	LINE
%assign	LINE	LINE+1
%endrep

calladdr dd	0

;=================================


	section	.text

GLOBAL	_PatchUnrolled
EXTERN	_detailyshift

_PatchUnrolled:
	push	ebx
	push	ebp
	mov	ebx,[_dc_pitch]
	mov	ecx,SCREENHEIGHT-1
	mov	ebp,vscale240+scalepatchoffs
	mov	eax,ecx
	neg	ebx
	imul	ebx
.loop	mov	[ebp],eax
	sub	eax,ebx
	dec	ecx
	lea	ebp,[ebp+scalesize]
	jnz	.loop
	pop	ebp
	pop	ebx
	ret

	align 16

;================
;
; R_DrawColumn
;
;================

GLOBAL	_R_DrawColumnP_Unrolled
GLOBAL	@R_DrawColumnP_Unrolled@0

@R_DrawColumnP_Unrolled@0:
_R_DrawColumnP_Unrolled:
	PUSHR

	mov	edx,[_ylookup]
	mov	ebp,[_dc_yh]
	mov	ebx,ebp
	mov	edi,[edx+ebx*4]
	mov	edx,[_columnofs]
	mov	ebx,[_dc_x]
	add	edi,[edx+ebx*4]

	mov	eax,[_dc_yl]
	sub	ebp,eax			; ebp = pixel count
	or	ebp,ebp
	js	done

	mov	ebp,[scalecalls+4+ebp*4]
	mov	ebx,[_dc_iscale]
	mov	[calladdr],ebp

	sub	eax,[_centery]
	imul	ebx
	mov	edx,[_dc_texturemid]
	mov	ebp,ebx
	add	edx,eax
	shl	ebp,16
	shr	ebx,16
	mov	ecx,edx
	shl	edx,16
	shr	ecx,16
	mov	dl,[_dc_mask]
	and	ebx,[_dc_mask]

	mov	esi,[_dc_source]
	mov	eax,[_dc_colormap]

	and	ecx,edx			; get address of first location
	jmp	[calladdr]

done:
	POPR
	ret

%macro	SCALELABEL 1
vscale%1:
%endmacro

%assign	LINE	SCREENHEIGHT
%rep	SCREENHEIGHT-1
	SCALELABEL	LINE
	mov	al,[esi+ecx]			; get source pixel
	add	edx,ebp				; calculate next location
	mov	al,[eax]			; translate the color
	adc	ecx,ebx
	mov	[edi-(LINE-1)*SCREENWIDTH],al	; draw a pixel to the buffer
	and	ecx,edx
%assign	LINE	LINE-1
%endrep

vscale1:
	mov	al,[esi+ecx]
	mov	al,[eax]
	mov	[edi],al
vscale0:
	POPR
	ret


;============================================================================
;
; unwound horizontal texture mapping code
;
; eax   lighttable
; ebx   scratch register
; ecx   position 6.10 bits x, 6.10 bits y
; edx   step 6.10 bits x, 6.10 bits y
; esi   start of block
; edi   dest
; ebp   fff to mask bx
;
; ebp should by preset from ebx / ecx before calling
;============================================================================

	section	.data


%macro	MAPDEFINE 1
	dd	hmap%1
%endmacro

	align 4
mapcalls:
%assign	LINE SCREENWIDTH-1
%rep	SCREENWIDTH
	MAPDEFINE LINE
%assign	LINE LINE-1
%endrep


callpoint	dd	0


	section .text

GLOBAL	_PatchUnrolledColSize

%define mapsize		(hmap1-hmap0)
%define mappatchoffs	(mapsize-4)

_PatchUnrolledColSize:
	mov	eax,[_ds_colsize]
	mov	ecx,hmap0+mappatchoffs
	mov	edx,-(SCREENWIDTH-1)
	imul	edx
	mov	edx,SCREENWIDTH-1
.loop	mov	[ecx],eax
	add	eax,[_ds_colsize]
	add	ecx,mapsize
	dec	edx
	jnz	.loop
	ret

;================
;
; R_DrawSpan
;
; Horizontal texture mapping
;
;================

GLOBAL	_R_DrawSpanP_Unrolled
GLOBAL	@R_DrawSpanP_Unrolled@0

	align 16

@R_DrawSpanP_Unrolled@0:
_R_DrawSpanP_Unrolled:
	PUSHR

	mov	edi,[_columnofs]
	mov	eax,[_ds_x1]
	mov	ebx,[_ds_x2]
	mov	edi,[edi+ebx*4]
	sub	ebx,eax
	mov	eax,[mapcalls+ebx*4]
	mov	[callpoint],eax		; spot to jump into unwound
	mov	eax,[_ylookup]
	mov	ebx,[_ds_y]
	add	edi,[eax+ebx*4]

;
; build composite position
;
	mov	ecx,[_ds_xfrac]
	shl	ecx,10
	and	ecx,0xffff0000
	mov	eax,[_ds_yfrac]
	shr	eax,6
	and	eax,0xffff
	or	ecx,eax

;
; build composite step
;
	mov	edx,[_ds_xstep]
	shl	edx,10
	and	edx,0xffff0000
	mov	eax,[_ds_ystep]
	shr	eax,6
	and	eax,0xffff
	or	edx,eax

	mov	esi,[_ds_source]
	mov	eax,[_ds_colormap]

;
; feed the pipeline and jump in
;
	mov	ebp,0xfff		; used to mask off slop high bits from position
	shld	ebx,ecx,22		; shift y units in
	shld	ebx,ecx,6		; shift x units in
	and	ebx,ebp			; mask off slop bits
	jmp	[callpoint]


%macro	MAPLABEL 1
hmap%1:
%endmacro

%assign	LINE 0
%assign	PCOL SCREENWIDTH-1
%rep	SCREENWIDTH-1

	MAPLABEL LINE
%assign	LINE LINE+1

	mov	al,[esi+ebx]		; get source pixel
	shld	ebx,ecx,22		; shift y units in
	shld	ebx,ecx,6		; shift x units in
	mov	al,[eax]		; translate color
	and	ebx,ebp			; mask off slop bits
	add	ecx,edx			; position += step
;	mov	[edi-PCOL],al		; write pixel
	db	0x88,0x87
	dd	-PCOL
%assign PCOL PCOL-1
%endrep

hmap319:
	mov	al,[esi+ebx]		; get source pixel
	shld	ebx,ecx,22		; shift y units in
	shld	ebx,ecx,6		; shift x units in
	mov	al,[eax]		; translate color
	and	ebx,ebp			; mask off slop bits
	add	ecx,edx			; position += step
	mov	[edi],al		; write pixel
hmap320:
	POPR
	ret
