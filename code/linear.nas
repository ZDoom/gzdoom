;****************************************************************************
;*
;* Unwound vertical scaling code based loosing on linear.asm from the
;* Heretic/Hexen source. This is probably as optimal as you're going
;* to get on a 486. I've taken the liberty of bumping the screen height up
;* to 240 (so it will work with resolutions up to 320x240) and adding in
;* the use of dc_mask.
;*
;* Actually, as optimal as you're going to get would probably be to not
;* unwind but to combine separate adjacent framebuffer writes into one.
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
extern	_dc_texturefrac
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

	sub	ebp,[_dc_yl]		; ebp = pixel count
	or	ebp,ebp
	js	done

	mov	ebp,[scalecalls+4+ebp*4]
	mov	ebx,[_dc_iscale]
	mov	[calladdr],ebp

	mov	edx,[_dc_texturefrac]
	mov	ebp,ebx
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
