; "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
; Ken Silverman's official web site: "http://www.advsys.net/ken"
; See the included license file "BUILDLIC.TXT" for license info.
; This file has been modified from Ken Silverman's original release

	SECTION .data

%ifndef M_TARGET_LINUX
%define ylookup			_ylookup
%define vplce			_vplce
%define vince			_vince
%define palookupoffse	_palookupoffse
%define bufplce			_bufplce
%define dc_iscale		_dc_iscale
%define dc_colormap		_dc_colormap
%define dc_count		_dc_count
%define dc_dest			_dc_dest
%define dc_source		_dc_source
%define dc_texturefrac	_dc_texturefrac

%define setupvlineasm	_setupvlineasm
%define prevlineasm1	_prevlineasm1
%define vlineasm1		_vlineasm1
%define vlineasm4		_vlineasm4
%endif

EXTERN ylookup ; near

EXTERN vplce ; near
EXTERN vince ; near
EXTERN palookupoffse ; near
EXTERN bufplce ; near

EXTERN dc_iscale
EXTERN dc_colormap
EXTERN dc_count
EXTERN dc_dest
EXTERN dc_source
EXTERN dc_texturefrac

	SECTION .text

ALIGN 16
GLOBAL setvlinebpl_
setvlinebpl_:
	mov dword [fixchain1a+2], eax
	mov dword [fixchain1b+2], eax
	mov dword [fixchain2a+2], eax
	ret

; pass it log2(texheight)

ALIGN 16
GLOBAL setupvlineasm
setupvlineasm:
	mov ecx, [esp+4]

		;First 2 lines for VLINEASM1, rest for VLINEASM4
	mov byte [premach3a+2], cl
	mov byte [mach3a+2], cl

	mov byte [machvsh1+2], cl      ;32-shy
	mov byte [machvsh3+2], cl      ;32-shy
	mov byte [machvsh5+2], cl      ;32-shy
	mov byte [machvsh6+2], cl      ;32-shy
	mov ch, cl
	sub ch, 16
	mov byte [machvsh8+2], ch      ;16-shy
	neg cl
	mov byte [machvsh7+2], cl      ;shy
	mov byte [machvsh9+2], cl      ;shy
	mov byte [machvsh10+2], cl     ;shy
	mov byte [machvsh11+2], cl     ;shy
	mov byte [machvsh12+2], cl     ;shy
	mov eax, 1
	shl eax, cl
	dec eax
	mov dword [machvsh2+2], eax    ;(1<<shy)-1
	mov dword [machvsh4+2], eax    ;(1<<shy)-1
	ret

	SECTION .rtext	progbits alloc exec write align=64

;eax = xscale
;ebx = palookupoffse
;ecx = # pixels to draw-1
;edx = texturefrac
;esi = texturecolumn
;edi = buffer pointer

ALIGN 16
GLOBAL prevlineasm1
prevlineasm1:
		mov ecx, [dc_count]
		cmp ecx, 1
		ja vlineasm1

		mov eax, [dc_iscale]
		mov edx, [dc_texturefrac]
		add eax, edx
		mov ecx, [dc_source]
premach3a: 	shr edx, 32
		push ebx
		push edi
		mov edi, [dc_colormap]
		xor ebx, ebx
		mov bl, byte [ecx+edx]
		mov ecx, [dc_dest]
		mov bl, byte [edi+ebx]
		pop edi
		mov byte [ecx], bl
		pop ebx
		ret

GLOBAL vlineasm1
ALIGN 16
vlineasm1:
		push ebx
		push edi
		push esi
		push ebp
		mov ecx, [dc_count]
		mov ebp, [dc_colormap]
		mov edi, [dc_dest]
		mov eax, [dc_iscale]
		mov edx, [dc_texturefrac]
		mov esi, [dc_source]
fixchain1a:	sub edi, 320
		nop
		nop
		nop
beginvline:
		mov ebx, edx
mach3a:		shr ebx, 32
fixchain1b:	add edi, 320
		mov bl, byte [esi+ebx]
		add edx, eax
		dec ecx
		mov bl, byte [ebp+ebx]
		mov byte [edi], bl
		jnz short beginvline
		pop ebp
		pop esi
		pop edi
		pop ebx
		mov eax, edx
		ret

	;eax: -------temp1-------
	;ebx: -------temp2-------
	;ecx:  dat  dat  dat  dat
	;edx: ylo2           ylo4
	;esi: yhi1           yhi2
	;edi: ---videoplc/cnt----
	;ebp: yhi3           yhi4
	;esp:
ALIGN 16
GLOBAL vlineasm4
vlineasm4:
		mov ecx, [dc_count]
		push ebp
		push ebx
		push esi
		push edi
		mov edi, [dc_dest]

		mov eax, dword [ylookup+ecx*4-4]
		add eax, edi
		mov dword [machvline4end+2], eax
		sub edi, eax

		mov eax, dword [bufplce+0]
		mov ebx, dword [bufplce+4]
		mov ecx, dword [bufplce+8]
		mov edx, dword [bufplce+12]
		mov dword [machvbuf1+2], ecx
		mov dword [machvbuf2+2], edx
		mov dword [machvbuf3+2], eax
		mov dword [machvbuf4+2], ebx

		mov eax, dword [palookupoffse+0]
		mov ebx, dword [palookupoffse+4]
		mov ecx, dword [palookupoffse+8]
		mov edx, dword [palookupoffse+12]
		mov dword [machvpal1+2], ecx
		mov dword [machvpal2+2], edx
		mov dword [machvpal3+2], eax
		mov dword [machvpal4+2], ebx

;     ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
;edx: ³v3lo           ³v1lo           ³
;     ÃÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄ´
;esi: ³v2hi  v2lo             ³   v3hi³
;     ÃÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄ´
;ebp: ³v0hi  v0lo             ³   v1hi³
;     ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÙ

		mov ebp, dword [vince+0]
		mov ebx, dword [vince+4]
		mov esi, dword [vince+8]
		mov eax, dword [vince+12]
		and esi, 0fffffe00h
		and ebp, 0fffffe00h
machvsh9:	rol eax, 88h                ;sh
machvsh10:	rol ebx, 88h                ;sh
		mov edx, eax
		mov ecx, ebx
		shr ecx, 16
		and edx, 0ffff0000h
		add edx, ecx
		and eax, 000001ffh
		and ebx, 000001ffh
		add esi, eax
		add ebp, ebx
		;
		mov eax, edx
		and eax, 0ffff0000h
		mov dword [machvinc1+2], eax
		mov dword [machvinc2+2], esi
		mov byte [machvinc3+2], dl
		mov byte [machvinc4+2], dh
		mov dword [machvinc5+2], ebp

		mov ebp, dword [vplce+0]
		mov ebx, dword [vplce+4]
		mov esi, dword [vplce+8]
		mov eax, dword [vplce+12]
		and esi, 0fffffe00h
		and ebp, 0fffffe00h
machvsh11:	rol eax, 88h                ;sh
machvsh12:	rol ebx, 88h                ;sh
		mov edx, eax
		mov ecx, ebx
		shr ecx, 16
		and edx, 0ffff0000h
		add edx, ecx
		and eax, 000001ffh
		and ebx, 000001ffh
		add esi, eax
		add ebp, ebx

		mov ecx, esi
		jmp short beginvlineasm4
ALIGN 16
		nop
		nop
		nop
beginvlineasm4:
machvsh1:	shr ecx, 88h          ;32-sh
		mov ebx, esi
machvsh2:	and ebx, 00000088h    ;(1<<sh)-1
machvinc1:	add edx, 88880000h
machvinc2:	adc esi, 88888088h
machvbuf1:	mov cl, byte [ecx+88888888h]
machvbuf2:	mov bl, byte [ebx+88888888h]
		mov eax, ebp
machvsh3:	shr eax, 88h          ;32-sh
machvpal1:	mov cl, byte [ecx+88888888h]
machvpal2:	mov ch, byte [ebx+88888888h]
		mov ebx, ebp
		shl ecx, 16
machvsh4:	and ebx, 00000088h    ;(1<<sh)-1
machvinc3:	add dl, 88h
machvbuf3:	mov al, byte [eax+88888888h]
machvinc4:	adc dh, 88h
machvbuf4:	mov bl, byte [ebx+88888888h]
machvinc5:	adc ebp, 88888088h
machvpal3:	mov cl, byte [eax+88888888h]
machvpal4:	mov ch, byte [ebx+88888888h]
machvline4end:	mov dword [edi+88888888h], ecx
fixchain2a:	add edi, 88888888h
		mov ecx, esi
		jle short beginvlineasm4

;     ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
;edx: ³v3lo           ³v1lo           ³
;     ÃÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄ´
;esi: ³v2hi  v2lo             ³   v3hi³
;     ÃÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄ´
;ebp: ³v0hi  v0lo             ³   v1hi³
;     ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÙ

		mov dword [vplce+8], esi
		mov dword [vplce+0], ebp
;vplc2 = (esi<<(32-sh))+(edx>>sh)
;vplc3 = (ebp<<(32-sh))+((edx&65535)<<(16-sh))
machvsh5:	shl esi, 88h     ;32-sh
		mov eax, edx
machvsh6:	shl ebp, 88h     ;32-sh
		and edx, 0000ffffh
machvsh7:	shr eax, 88h     ;sh
		add esi, eax
machvsh8:	shl edx, 88h     ;16-sh
		add ebp, edx
		mov dword [vplce+12], esi
		mov dword [vplce+4], ebp

		pop edi
		pop esi
		pop ebx
		pop ebp
		ret
