#ifdef _WIN32
#include <io.h>
#endif
#include <string.h>
#include <assert.h>

#include "doomtype.h"
#include "fmopl.h"
#include "opl_mus_player.h"
#include "w_wad.h"
#include "templates.h"
#include "c_cvars.h"
#include "i_system.h"

EXTERN_CVAR (Bool, opl_onechip)

OPLmusicBlock::OPLmusicBlock (FILE *file, int len, int rate, int maxSamples)
	: SampleRate (rate), NextTickIn (0), Looping (false)
{
	static bool gotInstrs;

	scoredata = NULL;
	SampleBuff = NULL;
	TwoChips = !opl_onechip;

#ifdef _WIN32
	InitializeCriticalSection (&ChipAccess);
#else
	ChipAccess = SDL_CreateMutex ();
	if (ChipAccess == NULL)
	{
		return;
	}
#endif

	if (!gotInstrs)
	{
		FWadLump data = Wads.OpenLumpName ("GENMIDI");
		int failed = OPLloadBank (data);
		if (failed)
		{
			return;
		}
	}

	SampleBuff = new int[maxSamples];

	scoredata = new BYTE[len];
	if (fread (scoredata, 1, len, file) != (size_t)len || OPLinit (TwoChips + 1, rate))
	{
		delete[] scoredata;
		scoredata = NULL;
		return;
	}

	SamplesPerTick = ((rate/14)+5)/10;	// round to nearest, not lowest
	Restart ();
}

OPLmusicBlock::~OPLmusicBlock ()
{
	if (scoredata != NULL)
	{
		OPLdeinit ();
		delete[] scoredata;
		scoredata = NULL;
	}
	if (SampleBuff != NULL)
	{
		delete[] SampleBuff;
		SampleBuff = NULL;
	}
#ifdef _WIN32
	DeleteCriticalSection (&ChipAccess);
#else
	if (ChipAccess != NULL)
	{
		SDL_DestroyMutex (ChipAccess);
		ChipAccess = NULL;
	}
#endif
}

void OPLmusicBlock::ResetChips ()
{
	TwoChips = !opl_onechip;
#ifdef _WIN32
	EnterCriticalSection (&ChipAccess);
#else
	if (SDL_mutexP (ChipAccess) != 0)
		return;
#endif
	OPLdeinit ();
	OPLinit (TwoChips + 1, SampleRate);
#ifdef _WIN32
	LeaveCriticalSection (&ChipAccess);
#else
	SDL_mutexV (ChipAccess);
#endif
}

bool OPLmusicBlock::IsValid () const
{
	return scoredata != NULL;
}

void OPLmusicBlock::SetLooping (bool loop)
{
	Looping = loop;
}

void OPLmusicBlock::Restart ()
{
	OPLstopMusic (this);
	OPLplayMusic (this);
	MLtime = 0;
	playingcount = 0;
	score = scoredata + ((MUSheader *)scoredata)->scoreStart;
}

bool OPLmusicBlock::ServiceStream (void *buff, int numbytes)
{
	short *samples = (short *)buff;
	int *samples1 = SampleBuff;
	int numsamples = numbytes / 2;
	bool prevEnded = false;
	bool res = true;

	memset (SampleBuff, 0, numsamples * 4);

#ifdef _WIN32
	EnterCriticalSection (&ChipAccess);
#else
	if (SDL_mutexP (ChipAccess) != 0)
		return true;
#endif
	while (numsamples > 0)
	{
		int samplesleft = MIN (numsamples, NextTickIn);

		if (samplesleft > 0)
		{
			YM3812UpdateOne (0, samples1, samplesleft);
			if (TwoChips)
			{
				YM3812UpdateOne (1, samples1, samplesleft);
			}
			NextTickIn -= samplesleft;
			assert (NextTickIn >= 0);
			numsamples -= samplesleft;
			samples1 += samplesleft;
		}
		
		if (NextTickIn == 0)
		{
			int next = playTick (this);
			if (next == 0)
			{ // end of song
				if (!Looping || prevEnded)
				{
					if (numsamples > 0)
					{
						YM3812UpdateOne (0, samples1, numsamples);
						if (TwoChips)
						{
							YM3812UpdateOne (1, samples1, numsamples);
						}
					}
					res = false;
					break;
				}
				else
				{
					// Avoid infinite loops from songs that do nothing but end
					prevEnded = true;
					Restart ();
				}
			}
			else
			{
				prevEnded = false;
				NextTickIn = SamplesPerTick * next;
				assert (NextTickIn >= 0);
				MLtime += next;
			}
		}
	}
#ifdef _WIN32
	LeaveCriticalSection (&ChipAccess);
#else
	SDL_mutexV (ChipAccess);
#endif
	numsamples = numbytes / 2;
	samples1 = SampleBuff;

#if defined(_MSC_VER) && defined(USEASM)
	if (CPU.bCMOV && numsamples > 1)
	{
		__asm
		{
			mov ecx, numsamples
			mov esi, samples1
			mov edi, samples
			shr ecx, 1
			lea esi, [esi+ecx*8]
			lea edi, [edi+ecx*4]
			neg ecx
			mov edx, 0x00007fff
looper:		mov eax, [esi+ecx*8]
			mov ebx, [esi+ecx*8+4]

			// Shift the samples down to reduce the chance of clipping at the high end.
			// Since most of the waveforms we produce only use the upper half of the
			// sine wave, this is a good thing. Although it does leave less room at
			// the bottom of the sine before clipping, almost none of the songs I tested
			// went below -9000.
			sub eax, 18000
			sub ebx, 18000

			// Clamp high
			cmp eax, edx
			cmovg eax, edx
			cmp ebx, edx
			cmovg ebx, edx

			// Clamp low
			not edx
			cmp eax, edx
			cmovl eax, edx
			cmp ebx, edx
			cmovl ebx, edx

			not edx
			mov [edi+ecx*4], ax
			mov [edi+ecx*4+2], bx

			inc ecx
			jnz looper

			test numsamples, 1
			jz done

			mov eax, [esi+ecx*8]
			sub eax, 18000
			cmp eax, edx
			cmovg eax, edx
			not edx
			cmp eax, edx
			cmovl eax, edx
			mov [edi+ecx*2], ax
done:
		}
	}
	else
#endif
	{
		for (int i = 1; i < numsamples; i+=2)
		{
			int u = samples1[i-1] - 18000;
			int v = samples1[i  ] - 18000;
			if (u < -32768) u = -32768; else if (u > 32767) u = 32767;
			if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
			samples[i-1] = u;
			samples[i  ] = v;
		}
		if (numsamples & 1)
		{
 			int v = samples1[numsamples-1] - 18000;
			if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
			samples[numsamples-1] = v;
		}
	}
	return res;
}
