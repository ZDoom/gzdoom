%include "valgrind.inc"

%ifdef M_TARGET_WATCOM
  SEGMENT DATA PUBLIC ALIGN=16 CLASS=DATA USE32
  SEGMENT DATA
%else
  SECTION .data
%endif

%ifndef M_TARGET_LINUX
%define ylookup		_ylookup
%define vplce		_vplce
%define vince		_vince
%define palookupoffse	_palookupoffse
%define bufplce		_bufplce
%define dc_iscale	_dc_iscale
%define dc_colormap	_dc_colormap
%define dc_count	_dc_count
%define dc_dest		_dc_dest
%define dc_source	_dc_source
%define dc_texturefrac	_dc_texturefrac
%define dc_pitch	_dc_pitch

%define setupvlinetallasm	_setupvlinetallasm
%define vlinetallasm4		_vlinetallasm4
%define vlinetallasmathlon4	_vlinetallasmathlon4
%define vlinetallasm1		_vlinetallasm1
%define prevlinetallasm1	_prevlinetallasm1
%endif

EXTERN vplce
EXTERN vince
EXTERN palookupoffse
EXTERN bufplce

EXTERN ylookup
EXTERN dc_iscale
EXTERN dc_colormap
EXTERN dc_count
EXTERN dc_dest
EXTERN dc_source
EXTERN dc_texturefrac
EXTERN dc_pitch

GLOBAL vlt4pitch
GLOBAL vlt1pitch

%ifdef M_TARGET_WATCOM
  SEGMENT CODE PUBLIC ALIGN=16 CLASS=CODE USE32
  SEGMENT CODE
%else
  SECTION .text
%endif

ALIGN 16
GLOBAL setpitch3
setpitch3:
	mov	[vltpitch+2], eax
	mov	[vltpitcha+2],eax
	mov	[vlt1pitch1+2], eax
	mov	[vlt1pitch2+2], eax
	selfmod vltpitch, vlt1pitch2+6
	ret

ALIGN 16
GLOBAL setupvlinetallasm
setupvlinetallasm:
	mov	ecx, [esp+4]
	mov	[shifter1+2], cl
	mov	[shifter2+2], cl
	mov	[shifter3+2], cl
	mov	[shifter4+2], cl
	mov	[shifter1a+2], cl
	mov	[shifter2a+2], cl
	mov	[shifter3a+2], cl
	mov	[shifter4a+2], cl
	mov	[preshift+2], cl
	mov	[shift11+2], cl
	mov	[shift12+2], cl
	selfmod shifter1, shift12+6
	ret

%ifdef M_TARGET_MACHO
	SECTION .text align=64
GLOBAL _rtext_tmap3_start
_rtext_tmap3_start:
%else
	SECTION .rtext	progbits alloc exec write align=64
%endif

ALIGN 16

GLOBAL vlinetallasm4
vlinetallasm4:
	push	ebx
	mov	eax, [bufplce+0]
	mov	ebx, [bufplce+4]
	mov	ecx, [bufplce+8]
	mov	edx, [bufplce+12]
	mov	[source1+3], eax
	mov	[source2+3], ebx
	mov	[source3+3], ecx
	mov	[source4+3], edx
	mov	eax, [palookupoffse+0]
	mov	ebx, [palookupoffse+4]
	mov	ecx, [palookupoffse+8]
	mov	edx, [palookupoffse+12]
	mov	[lookup1+2], eax
	mov	[lookup2+2], ebx
	mov	[lookup3+2], ecx
	mov	[lookup4+2], edx
	mov	eax, [vince+0]
	mov	ebx, [vince+4]
	mov	ecx, [vince+8]
	mov	edx, [vince+12]
	mov	[step1+2], eax
	mov	[step2+2], ebx
	mov	[step3+2], ecx
	mov	[step4+1], edx
	push	ebp
	push	esi
	push	edi
	mov	ecx, [dc_count]
	mov	edi, [dc_dest]
	mov	eax, dword [ylookup+ecx*4-4]
	add	eax, edi
	sub	edi, eax
	mov	[write1+2],eax
	inc	eax
	mov	[write2+2],eax
	inc	eax
	mov	[write3+2],eax
	inc	eax
	mov	[write4+2],eax
	mov	ebx, [vplce]
	mov	ecx, [vplce+4]
	mov	esi, [vplce+8]
	mov	eax, [vplce+12]
	selfmod loopit, vltpitch
	jmp	loopit

ALIGN	16
loopit:
		mov	edx, ebx
shifter1:	shr	edx, 24
source1:	movzx	edx, BYTE [edx+0x88888888]
lookup1:	mov	dl, [edx+0x88888888]
write1:		mov	[edi+0x88888880], dl
step1:		add	ebx, 0x88888888
		mov	edx, ecx
shifter2:	shr	edx, 24
source2:	movzx	edx, BYTE [edx+0x88888888]
lookup2:	mov	dl, [edx+0x88888888]
write2:		mov	[edi+0x88888881], dl
step2:		add	ecx, 0x88888888
		mov	edx, esi
shifter3:	shr	edx, 24
source3:	movzx	edx, BYTE [edx+0x88888888]
lookup3:	mov	dl, BYTE [edx+0x88888888]
write3:		mov	[edi+0x88888882], dl
step3:		add	esi, 0x88888888
		mov	edx, eax
shifter4:	shr	edx, 24
source4:	movzx	edx, BYTE [edx+0x88888888]
lookup4:	mov	dl, [edx+0x88888888]
write4:		mov	[edi+0x88888883], dl
step4:		add	eax, 0x88888888
vltpitch:	add	edi, 320
		jle	near loopit

	mov	[vplce], ebx
	mov	[vplce+4], ecx
	mov	[vplce+8], esi
	mov	[vplce+12], eax

	pop	edi
	pop	esi
	pop	ebp
	pop	ebx

	ret

	ALIGN	16

GLOBAL vlinetallasmathlon4
vlinetallasmathlon4:
	push	ebx
	mov	eax, [bufplce+0]
	mov	ebx, [bufplce+4]
	mov	ecx, [bufplce+8]
	mov	edx, [bufplce+12]
	mov	[source1a+3], eax
	mov	[source2a+3], ebx
	mov	[source3a+3], ecx
	mov	[source4a+3], edx
	mov	eax, [palookupoffse+0]
	mov	ebx, [palookupoffse+4]
	mov	ecx, [palookupoffse+8]
	mov	edx, [palookupoffse+12]
	mov	[lookup1a+2], eax
	mov	[lookup2a+2], ebx
	mov	[lookup3a+2], ecx
	mov	[lookup4a+2], edx
	mov	eax, [vince+0]
	mov	ebx, [vince+4]
	mov	ecx, [vince+8]
	mov	edx, [vince+12]
	mov	[step1a+2], eax
	mov	[step2a+2], ebx
	mov	[step3a+2], ecx
	mov	[step4a+1], edx
	push	ebp
	push	esi
	push	edi
	mov	ecx, [dc_count]
	mov	edi, [dc_dest]
	mov	eax, dword [ylookup+ecx*4-4]
	add	eax, edi
	sub	edi, eax
	mov	[write1a+2],eax
	inc	eax
	mov	[write2a+2],eax
	inc	eax
	mov	[write3a+2],eax
	inc	eax
	mov	[write4a+2],eax
	mov	ebp, [vplce]
	mov	ecx, [vplce+4]
	mov	esi, [vplce+8]
	mov	eax, [vplce+12]
	selfmod loopita, vltpitcha
	jmp	loopita

; Unfortunately, this code has not been carefully analyzed to determine
; how well it utilizes the processor's instruction units. Instead, I just
; kept rearranging code, seeing what sped it up and what slowed it down
; until I arrived at this. The is the fastest version I was able to
; manage, but that does not mean it cannot be made faster with careful
; instructing shuffling.

		ALIGN	64
		
loopita:	mov	edx, ebp
		mov	ebx, ecx
shifter1a:	shr	edx, 24
shifter2a:	shr	ebx, 24
source1a:	movzx	edx, BYTE [edx+0x88888888]
source2a:	movzx	ebx, BYTE [ebx+0x88888888]
step1a:		add	ebp, 0x88888888
step2a:		add	ecx, 0x88888888
lookup1a:	mov	dl, [edx+0x88888888]
lookup2a:	mov	dh, [ebx+0x88888888]
		mov	ebx, esi
write1a:	mov	[edi+0x88888880], dl
write2a:	mov	[edi+0x88888881], dh
shifter3a:	shr	ebx, 24
		mov	edx, eax
source3a:	movzx	ebx, BYTE [ebx+0x88888888]
shifter4a:	shr	edx, 24
step3a:		add	esi, 0x88888888
source4a:	movzx	edx, BYTE [edx+0x88888888]
step4a:		add	eax, 0x88888888
lookup3a:	mov	bl, [ebx+0x88888888]
lookup4a:	mov	dl, [edx+0x88888888]
write3a:	mov	[edi+0x88888882], bl
write4a:	mov	[edi+0x88888883], dl
vltpitcha:	add	edi, 320
		jle	near loopita

	mov	[vplce], ebp
	mov	[vplce+4], ecx
	mov	[vplce+8], esi
	mov	[vplce+12], eax

	pop	edi
	pop	esi
	pop	ebp
	pop	ebx

	ret

ALIGN 16
GLOBAL prevlinetallasm1
prevlinetallasm1:
		mov	ecx, [dc_count]
		cmp	ecx, 1
		ja	vlinetallasm1

		mov	eax, [dc_iscale]
		mov	edx, [dc_texturefrac]
		add	eax, edx
		mov	ecx, [dc_source]
preshift: 	shr	edx, 16
		push	ebx
		push	edi
		mov	edi, [dc_colormap]
		movzx	ebx, byte [ecx+edx]
		mov	ecx, [dc_dest]
		mov	bl, byte [edi+ebx]
		pop	edi
		mov	byte [ecx], bl
		pop	ebx
		ret

ALIGN 16
GLOBAL vlinetallasm1
vlinetallasm1:
		push	ebp
		push	ebx
		push	edi
		push	esi

		mov	ebp, [dc_count]
		mov	ebx, [dc_texturefrac]	; ebx = frac
		mov	edi, [dc_dest]
		mov	ecx, ebx
shift11:	shr	ecx, 16
		mov	esi, [dc_source]
		mov	edx, [dc_iscale]
vlt1pitch1:	sub	edi, 0x88888888
		mov	eax, [dc_colormap]

loop2:
		movzx	ecx, BYTE [esi+ecx]
		add	ebx, edx
vlt1pitch2:	add	edi, 0x88888888
		mov	cl,[eax+ecx]
		mov	[edi],cl
		mov	ecx,ebx
shift12:	shr	ecx,16
		dec	ebp
		jnz	loop2

		mov	eax,ebx
		pop	esi
		pop	edi
		pop	ebx
		pop	ebp
		ret

%ifdef M_TARGET_MACHO
GLOBAL _rtext_tmap3_end
_rtext_tmap3_end:
%endif
