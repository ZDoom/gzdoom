#include <io.h>
#include <string.h>

#include "doomtype.h"
#include "fmopl.h"
#include "opl_mus_player.h"
#include "w_wad.h"
#include "z_zone.h"
#include "templates.h"
#include "c_cvars.h"
#include "i_system.h"

EXTERN_CVAR (Bool, opl_onechip)

OPLmusicBlock::OPLmusicBlock (int handle, int pos, int len, int rate, int maxSamples)
	: SampleRate (rate), NextTickIn (0), Looping (false)
{
	static bool gotInstrs;

	InitializeCriticalSection (&ChipAccess);

	scoredata = NULL;
	SampleBuff = NULL;
	TwoChips = !opl_onechip;

	if (!gotInstrs)
	{
		if (OPLloadBank (W_CacheLumpName ("GENMIDI", PU_CACHE)))
		{
			return;
		}
	}

	SampleBuff = new int[maxSamples];

	scoredata = new BYTE[len];
	if (lseek (handle, pos, SEEK_SET) < 0 ||
		read (handle, scoredata, len) != len ||
		OPLinit (TwoChips + 1, rate))
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
	DeleteCriticalSection (&ChipAccess);
}

void OPLmusicBlock::ResetChips ()
{
	TwoChips = !opl_onechip;
	EnterCriticalSection (&ChipAccess);
	OPLdeinit ();
	OPLinit (TwoChips + 1, SampleRate);
	LeaveCriticalSection (&ChipAccess);
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

	EnterCriticalSection (&ChipAccess);
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
				MLtime += next;
			}
		}
	}
	LeaveCriticalSection (&ChipAccess);
	numsamples = numbytes / 2;
	samples1 = SampleBuff;

#if defined(_MSC_VER) && defined(USEASM)
	if (HaveCMOV && numsamples > 1)
	{
		__asm
		{
			mov ecx, numsamples
			mov esi, samples1
			mov edi, samples
			lea esi, [esi+ecx*4]
			lea edi, [edi+ecx*2]
			shr ecx, 1
			neg ecx
			mov edx, 0x00007fff
looper:		mov eax, [esi+ecx*8]
			mov ebx, [esi+ecx*8+4]

			// It's a might quiet, so amplify the sound a little
			add eax, eax
			add ebx, ebx

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
		}
		if (numsamples & 1)
		{
			int v = clamp (samples1[numsamples-1]<<1, -32768, 32767);;
			samples[numsamples-1] = v;
		}
		return true;
	}
#endif

	for (int i = 0; i < numsamples; ++i)
	{
		int v = samples1[i]<<1;
		if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
		samples[i] = v;
	}
	return true;
}
