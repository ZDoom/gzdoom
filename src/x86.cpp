#include "doomtype.h"
#include "doomdef.h"
#include "x86.h"

extern "C"
{
	CPUInfo CPU;
}

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
#include <mmintrin.h>
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
		if (cpu->bMMX)			Printf(" MMX");
		if (cpu->bMMXPlus)		Printf(" MMX+");
		if (cpu->bSSE)			Printf(" SSE");
		if (cpu->bSSE2)			Printf(" SSE2");
		if (cpu->bSSE3)			Printf(" SSE3");
		if (cpu->bSSSE3)		Printf(" SSSE3");
		if (cpu->bSSE41)		Printf(" SSE4.1");
		if (cpu->bSSE42)		Printf(" SSE4.2");
		if (cpu->b3DNow)		Printf(" 3DNow!");
		if (cpu->b3DNowPlus)	Printf(" 3DNow!+");
		Printf ("\n");
	}
}

#if 0
// Compiler output for this function is crap compared to the assembly
// version, which is why it isn't used.
void DoBlending_MMX2(const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a)
{
	__m64 blendcolor;
	__m64 blendalpha;
	__m64 zero;
	__m64 blending256;
	__m64 color1;
	__m64 color2;

	zero = _mm_setzero_si64();
#ifndef __GNUC__
	blending256.m64_i64 = 0x10001000100;
#else
	blending256 = (__m64)0x10001000100ll;
#endif

	blendcolor = _mm_unpacklo_pi8(_m_from_int((r << 16) | (g << 8) | b), zero);	// 000000RR 00GG00BB
	blendalpha = _mm_unpacklo_pi8(_m_from_int((a << 16) | (a << 8) | a), zero);	// 000000AA 00AA00AA

	blendcolor = _mm_mullo_pi16(blendcolor, blendalpha);	// premultiply blend by alpha
	blendalpha = _mm_subs_pu16(blending256, blendalpha);	// one minus alpha

	// Do two colors per iteration: Count must be even
	for (count >>= 1; count > 0; --count)
	{
		color1 = *(__m64 *)from;						// 00r2g2b2 00r1g1b1
		from += 2;
		color2 = _mm_unpackhi_pi8(color1, zero);		// 000000r2 00g200b2
		color1 = _mm_unpacklo_pi8(color1, zero);		// 000000r1 00g100b1
		color1 = _mm_mullo_pi16(blendalpha, color1);	// 0000r1rr g1ggb1bb
		color2 = _mm_mullo_pi16(blendalpha, color2);	// 0000r2rr g2ggb2bb
		color1 = _mm_adds_pu16(blendcolor, color1);
		color2 = _mm_adds_pu16(blendcolor, color2);
		color1 = _mm_srli_pi16(color1, 8);
		color2 = _mm_srli_pi16(color2, 8);
		*(__m64 *)to = _mm_packs_pu16(color1, color2);	// 00r2g2b2 00r1g1b1
		to += 2;
	}
	_mm_empty();
}
#endif

#ifdef X86_ASM
extern "C" void DoBlending_MMX(const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);
#endif

void DoBlending_SSE2(const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a)
{
	__m128i blendcolor;
	__m128i blendalpha;
	__m128i zero;
	__m128i blending256;
	__m128i color1;
	__m128i color2;
	size_t unaligned;

	unaligned = ((size_t)from | (size_t)to) & 0xF;

#ifdef X86_ASM
	// For unaligned accesses, the assembly MMX version is slightly faster.
	// Note that using unaligned SSE loads and stores is still faster than
	// the compiler-generated MMX version.
	if (unaligned)
	{
		DoBlending_MMX(from, to, count, r, g, b, a);
		return;
	}
#endif

#if defined(__amd64__) || defined(_M_X64)
	long long color;

	blending256 = _mm_set_epi64x(0x10001000100ll, 0x10001000100ll);

	color = ((long long)r << 32) | (g << 16) | b;
	blendcolor = _mm_set_epi64x(color, color);

	color = ((long long)a << 32) | (a << 16) | a;
	blendalpha = _mm_set_epi64x(color, color);
#else
	int color;

	blending256 = _mm_set_epi32(0x100, 0x1000100, 0x100, 0x1000100);

	color = (g << 16) | b;
	blendcolor = _mm_set_epi32(r, color, r, color);

	color = (a << 16) | a;
	blendalpha = _mm_set_epi32(a, color, a, color);
#endif

	blendcolor = _mm_mullo_epi16(blendcolor, blendalpha);	// premultiply blend by alpha
	blendalpha = _mm_subs_epu16(blending256, blendalpha);	// one minus alpha

	zero = _mm_setzero_si128();

#ifndef X86_ASM
	if (unaligned)
	{
		for (count >>= 2; count > 0; --count)
		{
			color1 = _mm_loadu_si128((__m128i *)from);
			from += 4;
			color2 = _mm_unpackhi_epi8(color1, zero);
			color1 = _mm_unpacklo_epi8(color1, zero);
			color1 = _mm_mullo_epi16(blendalpha, color1);
			color2 = _mm_mullo_epi16(blendalpha, color2);
			color1 = _mm_adds_epu16(blendcolor, color1);
			color2 = _mm_adds_epu16(blendcolor, color2);
			color1 = _mm_srli_epi16(color1, 8);
			color2 = _mm_srli_epi16(color2, 8);
			_mm_storeu_si128((__m128i *)to, _mm_packus_epi16(color1, color2));
			to += 4;
		}
	}
	else
#endif
	{
		for (count >>= 2; count > 0; --count)
		{
			color1 = _mm_load_si128((__m128i *)from);
			from += 4;
			color2 = _mm_unpackhi_epi8(color1, zero);
			color1 = _mm_unpacklo_epi8(color1, zero);
			color1 = _mm_mullo_epi16(blendalpha, color1);
			color2 = _mm_mullo_epi16(blendalpha, color2);
			color1 = _mm_adds_epu16(blendcolor, color1);
			color2 = _mm_adds_epu16(blendcolor, color2);
			color1 = _mm_srli_epi16(color1, 8);
			color2 = _mm_srli_epi16(color2, 8);
			_mm_store_si128((__m128i *)to, _mm_packus_epi16(color1, color2));
			to += 4;
		}
	}
}
#endif
