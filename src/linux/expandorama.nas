
		BITS 32

%macro gdword 1
		global	%1:data 4

%1:
		resd	1
%endmacro

%macro func 1
		global	%1:function
		align	16
%1:
%endmacro

%define SPACEFILLER4	0x44444444

extern UseMMX

		SECTION .bss

gdword	ExpCopyFunc
gdword	ExpPaletteFunc

gdword	ExpDepth; dest depth in bytes (set by ExpandSetMode)

gdword	ExpSrc	; source pixmap
gdword	ExpDst	; dest pixmap
gdword	ExpSW	; source width
gdword	ExpSH	; source height
gdword	ExpSG	; source gap
gdword	ExpDG	; dest gap
gdword	ExpDP	; dest pitch

RShift:		resd	1
GShift:		resd	1
BShift:		resd	1
RMask:		resd	1
GMask:		resd	1
BMask:		resd	1

resb	32-(($-$$)&31)
	
Palette:	resd	256
Palette2:	resd	256		; for byte expansion


		SECTION	.text

;*************************************************************************

CalcShifts:
	mov		eax,[esp+12]
	mov		[RMask],eax
	call	CalcShift
	mov		[RShift],ecx
	mov		eax,[esp+16]
	mov		[GMask],eax
	call	CalcShift
	mov		[GShift],ecx
	mov		eax,[esp+20]
	mov		[BMask],eax
	call	CalcShift
	mov		[BShift],ecx
	ret

CalcShift:
	xor		ecx,ecx
.loop
	inc		ecx
	shr		eax,1
	jnz		.loop
	sub		ecx,byte 8
	ret	

bits32
	mov		byte [ExpDepth],byte 4
	mov		[ExpCopyFunc],dword Expand8to32
	mov		[ExpPaletteFunc],dword Palette32
	mov		eax,[RMask]
	mov		ecx,[GMask]
	mov		[rmask32+2],eax
	mov		eax,[BMask]
	mov		[gmask32+2],ecx
	mov		[bmask32+2],eax
	mov		al,[RShift]
	cmp		al,0
	jge		.32al
	neg		al
	mov		[rshift32+1],byte 0xe9	;shr
	mov		[rshift32+2],al
	jmp		.32tg
.32al
	mov		[rshift32+1],byte 0xe1	;shl
	mov		[rshift32+2],al
.32tg
	mov		al,[GShift]
	cmp		al,0
	jge		.32gl
	neg		al
	mov		[gshift32+1],byte 0xeb	;shr
	mov		[gshift32+2],al
	jmp		.32tb
.32gl
	mov		[gshift32+1],byte 0xe3	;shl
	mov		[gshift32+2],al
.32tb
	mov		al,[BShift]
	cmp		al,0
	jge		.32bl
	neg		al
	mov		[bshift32+1],byte 0xea	;shr
	mov		[bshift32+2],al
	ret
.32bl
	mov		[bshift32+1],byte 0xe2	;shl
	mov		[bshift32+2],al
	ret

bits8
	mov		byte [ExpDepth], byte 1
	mov		[ExpPaletteFunc],dword .areturn
	cmp		[UseMMX],byte 0
	jz		.nommx
	mov		[ExpCopyFunc],dword Expand8to8MMX
	ret
.nommx
	mov		[ExpCopyFunc],dword Expand8to8NoMMX
.areturn
	ret

func	ExpandSetMode

; stack:
;	dword	depth
;	dword	redmask
;	dword	greenmask
;	dword	bluemask

	call	CalcShifts

	cmp		[esp+4],byte 8
	je		bits8
	cmp		[esp+4],byte 32
	je		near bits32

.bits16
	mov		byte [ExpDepth],byte 2
	mov		[ExpPaletteFunc],dword Palette16
	mov		[ExpCopyFunc],dword Expand8to16
	mov		eax,[RMask]
	mov		ecx,[GMask]
	mov		[rmask16+2],eax
	mov		eax,[BMask]
	mov		[gmask16+2],ecx
	mov		[bmask16+2],eax
	mov		al,[RShift]
	cmp		al,0
	jge		.16al
	neg		al
	mov		[rshift16+1],byte 0xe9	;shr
	mov		[rshift16+2],al
	jmp		.16tg
.16al
	mov		[rshift16+1],byte 0xe1	;shl
	mov		[rshift16+2],al
.16tg
	mov		al,[GShift]
	cmp		al,0
	jge		.16gl
	neg		al
	mov		[gshift16+1],byte 0xeb	;shr
	mov		[gshift16+2],al
	jmp		.16tb
.16gl
	mov		[gshift16+1],byte 0xe3	;shl
	mov		[gshift16+2],al
.16tb
	mov		al,[BShift]
	cmp		al,0
	jge		.16bl
	neg		al
	mov		[bshift16+1],byte 0xea	;shr
	mov		[bshift16+2],al
	ret
.16bl
	mov		[bshift16+1],byte 0xe2	;shl
	mov		[bshift16+2],al
	ret

;*************************************************************************

;-----------------------------------------
;
; 8 bit destination, non-MMX version
;
;-----------------------------------------

Expand8to8NoMMX:

	push	esi
	push	edi

	push	ebx
	mov		esi,[ExpSrc]

	mov		edi,[ExpDst]
	mov		edx,[ExpDP]

	mov		ecx,[ExpSW]

	align	16

.loop
	mov		al,[esi+1]
	mov		ah,[esi+1]

	shl		eax,16
	mov		bl,[esi+3]

	mov		bh,[esi+3]
	mov		al,[esi]

	shl		ebx,16
	mov		ah,[esi]
	
	mov		[edi],eax
	mov		[edi+edx],eax

	mov		bl,[esi+2]
	mov		bh,[esi+2]

	mov		[edi+4],ebx
	mov		[edi+edx+4],ebx

	add		esi,byte 4
	add		edi,byte 8

	sub		ecx,4
	jnz		.loop

	add		esi,[ExpSG]
	add		edi,[ExpDG]

	mov		ecx,[ExpSW]
	dec		dword [ExpSH]

	jnz		.loop

	pop		ebx
	pop		edi
	pop		esi
	ret

;-----------------------------------------
;
; 8 bit destination, MMX version
;
;-----------------------------------------

	SECTION	.text

	align	16

Expand8to8MMX:

	push	edi
	mov		ecx,[ExpSW]

	mov		eax,[ExpSrc]
	mov		edi,[ExpDst]

	mov		edx,[ExpDP]

	align	16

.loop
	movq		mm0,[eax]

	add			eax,byte 16

	movq		mm1,mm0
	punpcklbw	mm0,mm0

	movq		mm2,[eax-8]
	punpckhbw	mm1,mm1

	movq		[edi],mm0
	movq		mm3,mm2

	movq		[edi+edx],mm0
	punpcklbw	mm2,mm2

	movq		[edi+8],mm1
	punpckhbw	mm3,mm3

	movq		[edi+edx+8],mm1

	movq		[edi+16],mm2

	movq		[edi+24],mm3

	movq		[edi+edx+16],mm2

	movq		[edi+edx+24],mm3

	add			edi,byte 32
	sub			ecx,byte 16

	jg			.loop	

	add		eax,[ExpSG]
	add		edi,[ExpDG]

	mov		ecx,[ExpSW]
	dec		dword [ExpSH]

	jnz		.loop

	emms
	pop		edi
	ret

;*************************************************************************

	section .data

	align	16

Palette16:

; Stack:
;	palette

	mov		eax,[esp+4]
	push	ebx

	push	ebp
	xor		edx,edx

	mov		ebp,255

	align	16
p16loop

	mov		dl,[eax+ebp*4]
	xor		ebx,ebx

bshift16
	shr		edx,3
	mov		bl,[eax+ebp*4+1]

gshift16
	shl		ebx,3
	xor		ecx,ecx

bmask16
	and		edx,0x33333333
	mov		cl,[eax+ebp*4+2]

rshift16
	shl		ecx,8
gmask16
	and		ebx,0x22222222

	or		edx,ebx
rmask16
	and		ecx,0x11111111

	or		ecx,edx
	xor		edx,edx

	mov		[Palette+ebp*4],cx
	mov		[Palette+ebp*4+2],cx

	dec		ebp
	jge		p16loop

	pop		ebp
	pop		ebx
	ret

;-----------------------------------------
;
; 16 bit destination
;
;-----------------------------------------

	section .text

	align	16

Expand8to16:

	push	esi
	push	edi

	push	ebx
	mov		esi,[ExpSrc]

	mov		edi,[ExpDst]
	xor		eax,eax

	mov		ecx,[ExpSW]
	mov		edx,[ExpDP]

	sub		edi,byte 16

	align	16

.loop
	mov		al,[esi]
	add		edi,byte 16

	mov		ebx,[eax*4+Palette]
	mov		al,[esi+1]

	mov		[edi],ebx
	mov		[edi+edx],ebx

	mov		ebx,[eax*4+Palette]
	mov		al,[esi+2]

	mov		[edi+4],ebx
	mov		[edi+edx+4],ebx

	mov		ebx,[eax*4+Palette]
	mov		al,[esi+3]

	mov		[edi+8],ebx
	mov		[edi+edx+8],ebx

	mov		ebx,[eax*4+Palette]
	add		esi,byte 4

	mov		[edi+12],ebx
	mov		[edi+edx+12],ebx

	sub		ecx,4
	jg		.loop

	add		esi,[ExpSG]
	add		edi,[ExpDG]

	mov		ecx,[ExpSW]
	dec		dword [ExpSH]

	jnz		.loop

	pop		ebx
	pop		edi
	pop		esi
	ret

;*************************************************************************

	section	.data

	align	4

Palette32:
; Stack:
;	palette

	mov		eax,[esp+4]
	push	ebx

	push	ebp
	xor		edx,edx

	mov		ebp,255

	align	4
p32loop

	mov		dl,[eax+ebp*4]
	xor		ebx,ebx

bshift32
	shr		edx,3
	mov		bl,[eax+ebp*4+1]

gshift32
	shl		ebx,3
	xor		ecx,ecx

bmask32
	and		edx,0x33333333
	mov		cl,[eax+ebp*4+2]

rshift32
	shl		ecx,8
gmask32
	and		ebx,0x22222222

	or		edx,ebx
rmask32
	and		ecx,0x11111111

	or		ecx,edx
	xor		edx,edx

	mov		[Palette+ebp*4],ecx
	dec		ebp

	jge		p32loop

	pop		ebp
	pop		ebx
	ret

;-----------------------------------------
;
; 32 bit destination
;
;-----------------------------------------

	section	.text

	align	16

Expand8to32

	push	esi
	push	edi
	push	ebp
	push	ebx

	mov		esi,[ExpSrc]
	mov		edi,[ExpDst]

	xor		eax,eax
	mov		ecx,[ExpSW]

	mov		edx,[ExpDP]
	sub		edi,byte 32

	align	16

.loop
	mov		al,[esi]
	add		edi,byte 32

	mov		ebx,[eax*4+Palette]
	mov		al,[esi+1]

	mov		[edi],ebx
	mov		[edi+4],ebx

	mov		[edi+edx],ebx
	mov		[edi+edx+4],ebx

	mov		ebx,[eax*4+Palette]
	mov		al,[esi+2]

	mov		[edi+8],ebx
	mov		[edi+12],ebx

	mov		[edi+edx+8],ebx
	mov		[edi+edx+12],ebx

	mov		ebx,[eax*4+Palette]
	mov		al,[esi+3]

	mov		[edi+16],ebx
	mov		[edi+20],ebx

	mov		[edi+edx+16],ebx
	mov		[edi+edx+20],ebx

	mov		ebx,[eax*4+Palette]
	add		esi,byte 4

	mov		[edi+24],ebx
	mov		[edi+28],ebx

	mov		[edi+edx+24],ebx
	mov		[edi+edx+28],ebx

	sub		ecx,4
	jnz		.loop

	add		esi,[ExpSG]
	add		edi,[ExpDG]

	mov		ecx,[ExpSW]
	dec		dword [ExpSH]

	jnz		near .loop

	pop		ebx
	pop		ebp
	pop		edi
	pop		esi
	ret
