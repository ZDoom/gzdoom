;  libFLAC - Free Lossless Audio Codec library
;  Copyright (C) 2001,2002,2003  Josh Coalson
;
;  Redistribution and use in source and binary forms, with or without
;  modification, are permitted provided that the following conditions
;  are met:
;
;  - Redistributions of source code must retain the above copyright
;  notice, this list of conditions and the following disclaimer.
;
;  - Redistributions in binary form must reproduce the above copyright
;  notice, this list of conditions and the following disclaimer in the
;  documentation and/or other materials provided with the distribution.
;
;  - Neither the name of the Xiph.org Foundation nor the names of its
;  contributors may be used to endorse or promote products derived from
;  this software without specific prior written permission.
;
;  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
;  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
;  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
;  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
;  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
;  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
;  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
;  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

%include "ia32/nasm.h"

	data_section

cglobal FLAC__cpu_info_asm_ia32
cglobal FLAC__cpu_info_extended_amd_asm_ia32
cglobal FLAC__cpu_info_sse_test_asm_ia32

	code_section

; **********************************************************************
;

have_cpuid:
	pushfd
	pop	eax
	mov	edx, eax
	xor	eax, 0x00200000
	push	eax
	popfd
	pushfd
	pop	eax
	cmp	eax, edx
	jz	.no_cpuid
	mov	eax, 1
	jmp	.end
.no_cpuid:
	xor	eax, eax
.end:
	ret

cident FLAC__cpu_info_asm_ia32
	push	ebx
	call	have_cpuid
	test	eax, eax
	jz	.no_cpuid
	mov	eax, 1
	cpuid
	mov	eax, edx
	jmp	.end
.no_cpuid:
	xor	eax, eax
.end
	pop	ebx
	ret

cident FLAC__cpu_info_extended_amd_asm_ia32
	push	ebx
	call	have_cpuid
	test	eax, eax
	jz	.no_cpuid
	mov	eax, 0x80000000
	cpuid
	cmp	eax, 0x80000001
	jb	.no_cpuid
	mov	eax, 0x80000001
	cpuid
	mov	eax, edx
	jmp	.end
.no_cpuid
	xor	eax, eax
.end
	pop	ebx
	ret

cident FLAC__cpu_info_sse_test_asm_ia32
	xorps	xmm0, xmm0
	pop	ebx

end
