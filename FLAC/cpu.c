/* libFLAC - Free Lossless Audio Codec library
 * Copyright (C) 2001,2002,2003,2004  Josh Coalson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Xiph.org Foundation nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "private/cpu.h"
#include<stdlib.h>
#include<stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined FLAC__CPU_PPC
#if !defined FLAC__NO_ASM
#if defined __APPLE__ && defined __MACH__
#include <sys/sysctl.h>
#endif /* __APPLE__ && __MACH__ */
#endif /* FLAC__NO_ASM */
#endif /* FLAC__CPU_PPC */

const unsigned FLAC__CPUINFO_IA32_CPUID_CMOV = 0x00008000;
const unsigned FLAC__CPUINFO_IA32_CPUID_MMX = 0x00800000;
const unsigned FLAC__CPUINFO_IA32_CPUID_FXSR = 0x01000000;
const unsigned FLAC__CPUINFO_IA32_CPUID_SSE = 0x02000000;
const unsigned FLAC__CPUINFO_IA32_CPUID_SSE2 = 0x04000000;

const unsigned FLAC__CPUINFO_IA32_CPUID_EXTENDED_AMD_3DNOW = 0x80000000;
const unsigned FLAC__CPUINFO_IA32_CPUID_EXTENDED_AMD_EXT3DNOW = 0x40000000;
const unsigned FLAC__CPUINFO_IA32_CPUID_EXTENDED_AMD_EXTMMX = 0x00400000;


void FLAC__cpu_info(FLAC__CPUInfo *info)
{
#ifdef FLAC__CPU_IA32
	info->type = FLAC__CPUINFO_TYPE_IA32;
#if !defined FLAC__NO_ASM && defined FLAC__HAS_NASM
	info->use_asm = true;
	{
		unsigned cpuid = FLAC__cpu_info_asm_ia32();
		info->data.ia32.cmov = (cpuid & FLAC__CPUINFO_IA32_CPUID_CMOV)? true : false;
		info->data.ia32.mmx = (cpuid & FLAC__CPUINFO_IA32_CPUID_MMX)? true : false;
		info->data.ia32.fxsr = (cpuid & FLAC__CPUINFO_IA32_CPUID_FXSR)? true : false;
		info->data.ia32.sse = (cpuid & FLAC__CPUINFO_IA32_CPUID_SSE)? true : false;
		info->data.ia32.sse2 = (cpuid & FLAC__CPUINFO_IA32_CPUID_SSE2)? true : false;

#ifndef FLAC__SSE_OS
		info->data.ia32.fxsr = info->data.ia32.sse = info->data.ia32.sse2 = false;
#endif

#ifdef FLAC__USE_3DNOW
		cpuid = FLAC__cpu_info_extended_amd_asm_ia32();
		info->data.ia32._3dnow = (cpuid & FLAC__CPUINFO_IA32_CPUID_EXTENDED_AMD_3DNOW)? true : false;
		info->data.ia32.ext3dnow = (cpuid & FLAC__CPUINFO_IA32_CPUID_EXTENDED_AMD_EXT3DNOW)? true : false;
		info->data.ia32.extmmx = (cpuid & FLAC__CPUINFO_IA32_CPUID_EXTENDED_AMD_EXTMMX)? true : false;
#else
		info->data.ia32._3dnow = info->data.ia32.ext3dnow = info->data.ia32.extmmx = false;
#endif
	}
#else
	info->use_asm = false;
#endif
#elif defined FLAC__CPU_PPC
	info->type = FLAC__CPUINFO_TYPE_PPC;
#if !defined FLAC__NO_ASM
	info->use_asm = true;
#ifdef FLAC__USE_ALTIVEC
#if defined __APPLE__ && defined __MACH__
	{
		int selectors[2] = { CTL_HW, HW_VECTORUNIT };
		int result = 0;
		size_t length = sizeof(result);
		int error = sysctl(selectors, 2, &result, &length, 0, 0);

		info->data.ppc.altivec = error==0 ? result!=0 : 0;
	}
#else /* __APPLE__ && __MACH__ */
	/* don't know of any other thread-safe way to check */
	info->data.ppc.altivec = 0;
#endif /* __APPLE__ && __MACH__ */
#else /* FLAC__USE_ALTIVEC */
	info->data.ppc.altivec = 0;
#endif /* FLAC__USE_ALTIVEC */
#else /* FLAC__NO_ASM */
	info->use_asm = false;
#endif /* FLAC__NO_ASM */
#else
	info->type = FLAC__CPUINFO_TYPE_UNKNOWN;
	info->use_asm = false;
#endif
}
