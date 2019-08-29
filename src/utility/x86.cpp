/*
**
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "doomtype.h"
#include "doomdef.h"
#include "x86.h"

CPUInfo CPU;

#if !defined(__amd64__) && !defined(__i386__) && !defined(_M_IX86) && !defined(_M_X64)
void CheckCPUID(CPUInfo *cpu)
{
	memset(cpu, 0, sizeof(*cpu));
	cpu->DataL1LineSize = 32;	// Assume a 32-byte cache line
}

void DumpCPUInfo(const CPUInfo *cpu)
{
}
#else

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <emmintrin.h>


#ifdef __GNUC__
#if defined(__i386__) && defined(__PIC__)
// %ebx may by the PIC register. */
#define __cpuid(output, func) \
	__asm__ __volatile__("xchgl\t%%ebx, %1\n\t" \
						 "cpuid\n\t" \
						 "xchgl\t%%ebx, %1\n\t" \
		: "=a" ((output)[0]), "=r" ((output)[1]), "=c" ((output)[2]), "=d" ((output)[3]) \
		: "a" (func));
#else
#define __cpuid(output, func) __asm__ __volatile__("cpuid" : "=a" ((output)[0]),\
	"=b" ((output)[1]), "=c" ((output)[2]), "=d" ((output)[3]) : "a" (func));
#endif
#endif

void CheckCPUID(CPUInfo *cpu)
{
	int foo[4];
	unsigned int maxext;

	memset(cpu, 0, sizeof(*cpu));

	cpu->DataL1LineSize = 32;	// Assume a 32-byte cache line

#if !defined(_M_IX86) && !defined(__i386__) && !defined(_M_X64) && !defined(__amd64__)
	return;
#else

#if defined(_M_IX86) || defined(__i386__)
	// Old 486s do not have CPUID, so we must test for its presence.
	// This code is adapted from the samples in AMD's document
	// entitled "AMD-K6 MMX Processor Multimedia Extensions."
#ifndef __GNUC__
	__asm
	{
		pushfd				// save EFLAGS
		pop eax				// store EFLAGS in EAX
		mov ecx,eax			// save in ECX for later testing
		xor eax,0x00200000	// toggle bit 21
		push eax			// put to stack
		popfd				// save changed EAX to EFLAGS
		pushfd				// push EFLAGS to TOS
		pop eax				// store EFLAGS in EAX
		cmp eax,ecx			// see if bit 21 has changed
		jne haveid			// if no change, then no CPUID
	}
	return;
haveid:
#else
	int oldfd, newfd;

	__asm__ __volatile__("\t"
		"pushf\n\t"
		"popl %0\n\t"
		"movl %0,%1\n\t"
		"xorl $0x200000,%0\n\t"
		"pushl %0\n\t"
		"popf\n\t"
		"pushf\n\t"
		"popl %0\n\t"
		: "=r" (newfd), "=r" (oldfd));
	if (oldfd == newfd)
	{
		return;
	}
#endif
#endif

	// Get vendor ID
	__cpuid(foo, 0);
	cpu->dwVendorID[0] = foo[1];
	cpu->dwVendorID[1] = foo[3];
	cpu->dwVendorID[2] = foo[2];
	if (foo[1] == MAKE_ID('A','u','t','h') &&
		foo[3] == MAKE_ID('e','n','t','i') &&
		foo[2] == MAKE_ID('c','A','M','D'))
	{
		cpu->bIsAMD = true;
	}

	// Get features flags and other info
	__cpuid(foo, 1);
	cpu->FeatureFlags[0] = foo[1];	// Store brand index and other stuff
	cpu->FeatureFlags[1] = foo[2];	// Store extended feature flags
	cpu->FeatureFlags[2] = foo[3];	// Store feature flags

	cpu->HyperThreading = (foo[3] & (1 << 28)) > 0;

	// If CLFLUSH instruction is supported, get the real cache line size.
	if (foo[3] & (1 << 19))
	{
		cpu->DataL1LineSize = (foo[1] & 0xFF00) >> (8 - 3);
	}

	cpu->Stepping = foo[0] & 0x0F;
	cpu->Type = (foo[0] & 0x3000) >> 12;	// valid on Intel only
	cpu->Model = (foo[0] & 0xF0) >> 4;
	cpu->Family = (foo[0] & 0xF00) >> 8;

	if (cpu->Family == 15)
	{ // Add extended family.
		cpu->Family += (foo[0] >> 20) & 0xFF;
	}
	if (cpu->Family == 6 || cpu->Family == 15)
	{ // Add extended model ID.
		cpu->Model |= (foo[0] >> 12) & 0xF0;
	}

	// Check for extended functions.
	__cpuid(foo, 0x80000000);
	maxext = (unsigned int)foo[0];

	if (maxext >= 0x80000004)
	{ // Get processor brand string.
		__cpuid((int *)&cpu->dwCPUString[0], 0x80000002);
		__cpuid((int *)&cpu->dwCPUString[4], 0x80000003);
		__cpuid((int *)&cpu->dwCPUString[8], 0x80000004);
	}

	if (cpu->bIsAMD)
	{
		if (maxext >= 0x80000005)
		{ // Get data L1 cache info.
			__cpuid(foo, 0x80000005);
			cpu->AMD_DataL1Info = foo[2];
		}
		if (maxext >= 0x80000001)
		{ // Get AMD-specific feature flags.
			__cpuid(foo, 0x80000001);
			cpu->AMDStepping = foo[0] & 0x0F;
			cpu->AMDModel = (foo[0] & 0xF0) >> 4;
			cpu->AMDFamily = (foo[0] & 0xF00) >> 8;

			if (cpu->AMDFamily == 15)
			{ // Add extended model and family.
				cpu->AMDFamily += (foo[0] >> 20) & 0xFF;
				cpu->AMDModel |= (foo[0] >> 12) & 0xF0;
			}
			cpu->FeatureFlags[3] = foo[3];	// AMD feature flags
		}
	}
#endif
}

void DumpCPUInfo(const CPUInfo *cpu)
{
	char cpustring[4*4*3+1];
	
	// Why does Intel right-justify this string (on P4s)
	// or add extra spaces (on Cores)?
	const char *f = cpu->CPUString;
	char *t;

	// Skip extra whitespace at the beginning.
	while (*f == ' ')
	{
		++f;
	}

	// Copy string to temp buffer, but condense consecutive
	// spaces to a single space character.
	for (t = cpustring; *f != '\0'; ++f)
	{
		if (*f == ' ' && *(f - 1) == ' ')
		{
			continue;
		}
		*t++ = *f;
	}
	*t = '\0';

	if (cpu->VendorID[0] && !batchrun)
	{
		Printf("CPU Vendor ID: %s\n", cpu->VendorID);
		if (cpustring[0])
		{
			Printf("  Name: %s\n", cpustring);
		}
		if (cpu->bIsAMD)
		{
			Printf("  Family %d (%d), Model %d, Stepping %d\n",
				cpu->Family, cpu->AMDFamily, cpu->AMDModel, cpu->AMDStepping);
		}
		else
		{
			Printf("  Family %d, Model %d, Stepping %d\n",
				cpu->Family, cpu->Model, cpu->Stepping);
		}
		Printf("  Features:");
		if (cpu->bSSE2)			Printf(" SSE2");
		if (cpu->bSSE3)			Printf(" SSE3");
		if (cpu->bSSSE3)		Printf(" SSSE3");
		if (cpu->bSSE41)		Printf(" SSE4.1");
		if (cpu->bSSE42)		Printf(" SSE4.2");
		if (cpu->b3DNow)		Printf(" 3DNow!");
		if (cpu->b3DNowPlus)	Printf(" 3DNow!+");
		if (cpu->HyperThreading)	Printf(" HyperThreading");
		Printf ("\n");
	}
}

#endif
