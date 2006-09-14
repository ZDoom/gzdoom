#ifdef _WIN32

#define USE_WINDOWS_DWORD
#include "altsound.h"
#include "c_cvars.h"

EXTERN_CVAR (Int, snd_interpolate)

// These macros implement linear interpolation

__forceinline SDWORD lininterp (SDWORD sample1, SDWORD sample2, DWORD frac)
{
	return sample1 + MulScale30 (frac >> 2, sample2 - sample1);
}

#define INTERPOLATEL \
	SDWORD sample = lininterp ((SDWORD)src[pos.HighPart], (SDWORD)src[pos.HighPart+1], pos.LowPart);

#define INTERPOLATEL2 \
	SDWORD samplel = lininterp ((SDWORD)src[pos.HighPart*2+0], (SDWORD)src[pos.HighPart*2+2], pos.LowPart); \
	SDWORD sampler = lininterp ((SDWORD)src[pos.HighPart*2+1], (SDWORD)src[pos.HighPart*2+3], pos.LowPart);

// These macros implement quadratic interpolation

__forceinline SDWORD quadraticinterp (SDWORD sample1, SDWORD sample2, SDWORD sample3, DWORD frac)
{

	SDWORD a =    sample1 - 2*sample2 + sample3;
	SDWORD b = -3*sample1 + 4*sample2 - sample3;
	frac >>= 17;
	return MulScale31 (a, frac*frac) + MulScale16 (b, frac) + sample1;
}

#define INTERPOLATEQ \
	SDWORD sample = quadraticinterp ((SDWORD)src[pos.HighPart], \
		(SDWORD)src[pos.HighPart + 1], \
		(SDWORD)src[pos.HighPart + 2], \
		pos.LowPart);

#define INTERPOLATEQ2 \
	SDWORD samplel = quadraticinterp ((SDWORD)src[pos.HighPart*2], \
		(SDWORD)src[pos.HighPart*2 + 2], \
		(SDWORD)src[pos.HighPart*2 + 4], \
		pos.LowPart); \
	SDWORD sampler = quadraticinterp ((SDWORD)src[pos.HighPart*2 + 1], \
		(SDWORD)src[pos.HighPart*2 + 3], \
		(SDWORD)src[pos.HighPart*2 + 5], \
		pos.LowPart);

/* Possible snd_interpolate values:
	0: No interpolation
	1: Linear interpolation
	2: Quadratic interpolation
*/
#define MONO_MIXER \
	LARGE_INTEGER pos; \
 \
	switch (snd_interpolate) \
	{ \
	default: \
		for (pos.QuadPart = inpos; count; --count) \
		{ \
			SDWORD sample = (SDWORD)src[pos.HighPart]; \
			pos.QuadPart += step; \
			dest[0] += sample * leftvol; \
			dest[1] += sample * rightvol; \
			dest += 2; \
		} \
		break; \
 \
	case 1: \
		for (pos.QuadPart = inpos; count; --count) \
		{ \
			INTERPOLATEL \
			pos.QuadPart += step; \
			dest[0] += sample * leftvol; \
			dest[1] += sample * rightvol; \
			dest += 2; \
		} \
		break; \
 \
	case 2:	\
		for (pos.QuadPart = inpos; count; --count) \
		{ \
			INTERPOLATEQ \
			pos.QuadPart += step; \
			dest[0] += sample * leftvol; \
			dest[1] += sample * rightvol; \
			dest += 2; \
		} \
		break; \
	} \
	return pos.QuadPart;

#define STEREO_MIXER \
	LARGE_INTEGER pos; \
 \
	switch (snd_interpolate) \
	{ \
	default: \
		for (pos.QuadPart = inpos; count; --count) \
		{ \
			SDWORD samplel = (SDWORD)src[pos.HighPart*2]; \
			SDWORD sampler = (SDWORD)src[pos.HighPart*2+1]; \
			pos.QuadPart += step; \
			dest[0] += samplel * vol; \
			dest[1] += sampler * vol; \
			dest += 2; \
		} \
		break; \
 \
	case 1: \
		for (pos.QuadPart = inpos; count; --count) \
		{ \
			INTERPOLATEL2 \
			pos.QuadPart += step; \
			dest[0] += samplel * vol; \
			dest[1] += sampler * vol; \
			dest += 2; \
		} \
		break; \
 \
	case 2:	\
		for (pos.QuadPart = inpos; count; --count) \
		{ \
			INTERPOLATEQ2 \
			pos.QuadPart += step; \
			dest[0] += samplel * vol; \
			dest[1] += sampler * vol; \
			dest += 2; \
		} \
		break; \
	} \
	return pos.QuadPart;

SQWORD AltSoundRenderer::MixMono8 (SDWORD *dest, const SBYTE *src, DWORD count, SQWORD inpos, SQWORD step, int leftvol, int rightvol)
{
	MONO_MIXER
}

SQWORD AltSoundRenderer::MixMono16 (SDWORD *dest, const SWORD *src, DWORD count, SQWORD inpos, SQWORD step, int leftvol, int rightvol)
{
	MONO_MIXER
}

SQWORD AltSoundRenderer::MixStereo8 (SDWORD *dest, const SBYTE *src, DWORD count, SQWORD inpos, SQWORD step, int vol)
{
	STEREO_MIXER
}

SQWORD AltSoundRenderer::MixStereo16 (SDWORD *dest, const SWORD *src, DWORD count, SQWORD inpos, SQWORD step, int vol)
{
	STEREO_MIXER
}

#endif
