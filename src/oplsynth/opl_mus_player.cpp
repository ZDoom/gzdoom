#ifdef _WIN32
#include <io.h>
#endif
#include <string.h>
#include <assert.h>

#include "opl_mus_player.h"
#include "doomtype.h"
#include "fmopl.h"
#include "w_wad.h"
#include "templates.h"
#include "c_cvars.h"
#include "i_system.h"
#include "stats.h"
#include "v_text.h"
#include "c_dispatch.h"

#define IMF_RATE	700

EXTERN_CVAR (Bool, opl_onechip)

static OPLmusicBlock *BlockForStats;

OPLmusicBlock::OPLmusicBlock (FILE *file, char * musiccache, int len, int rate, int maxSamples)
	: SampleRate (rate), NextTickIn (0), Looping (false), ScoreLen (len)
{
	scoredata = NULL;
	SampleBuff = NULL;
	TwoChips = !opl_onechip;
	io = new OPLio;

#ifdef _WIN32
	InitializeCriticalSection (&ChipAccess);
#else
	ChipAccess = SDL_CreateMutex ();
	if (ChipAccess == NULL)
	{
		return;
	}
#endif

	scoredata = new BYTE[len];

	if (file)
	{
		if (fread (scoredata, 1, len, file) != (size_t)len)
		{
			delete[] scoredata;
			scoredata = NULL;
			return;
		}
	}
	else
	{
		memcpy(scoredata, &musiccache[0], len);
	}

	if (io->OPLinit (TwoChips + 1, rate))
	{
		delete[] scoredata;
		scoredata = NULL;
		return;
	}

	// Check for MUS format
	if (*(DWORD *)scoredata == MAKE_ID('M','U','S',0x1a))
	{
		FWadLump data = Wads.OpenLumpName ("GENMIDI");
		if (0 != OPLloadBank (data))
		{
			delete[] scoredata;
			scoredata = NULL;
			return;
		}
		BlockForStats = this;
		SamplesPerTick = ((rate/14)+5)/10;	// round to nearest, not lowest
		RawPlayer = NotRaw;
	}
	// Check for RDosPlay raw OPL format
	else if (((DWORD *)scoredata)[0] == MAKE_ID('R','A','W','A') &&
			 ((DWORD *)scoredata)[1] == MAKE_ID('D','A','T','A'))
	{
		RawPlayer = RDosPlay;
		if (*(WORD *)(scoredata + 8) == 0)
		{ // A clock speed of 0 is bad
			*(WORD *)(scoredata + 8) = 0xFFFF; 
		}
		SamplesPerTick = Scale (rate, LittleShort(*(WORD *)(scoredata + 8)), 1193180);
	}
	// Check for modified IMF format (includes a header)
	else if (((DWORD *)scoredata)[0] == MAKE_ID('A','D','L','I') &&
		     scoredata[4] == 'B' && scoredata[5] == 1)
	{
		int songlen;
		BYTE *max = scoredata + ScoreLen;
		RawPlayer = IMF;
		SamplesPerTick = rate / IMF_RATE;

		score = scoredata + 6;
		// Skip track and game name
		for (int i = 2; i != 0; --i)
		{
			while (score < max && *score++ != '\0') {}
		}
		if (score < max) score++;	// Skip unknown byte
		if (score + 8 > max)
		{ // Not enough room left for song data
			delete[] scoredata;
			scoredata = NULL;
			return;
		}
		songlen = LittleLong(*(DWORD *)score);
		if (songlen != 0 && (songlen +=4) < ScoreLen - (score - scoredata))
		{
			ScoreLen = songlen + int(score - scoredata);
		}
	}

	SampleBuff = new int[maxSamples];
	Restart ();
}

OPLmusicBlock::~OPLmusicBlock ()
{
	BlockForStats = NULL;
	if (scoredata != NULL)
	{
		io->OPLdeinit ();
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
	delete io;
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
	io->OPLdeinit ();
	io->OPLinit (TwoChips + 1, SampleRate);
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
	OPLstopMusic ();
	OPLplayMusic ();
	MLtime = 0;
	playingcount = 0;
	if (RawPlayer == NotRaw)
	{
		score = scoredata + ((MUSheader *)scoredata)->scoreStart;
	}
	else if (RawPlayer == RDosPlay)
	{
		score = scoredata + 10;
		SamplesPerTick = Scale (SampleRate, LittleShort(*(WORD *)(scoredata + 8)), 1193180);
	}
	else if (RawPlayer == IMF)
	{
		score = scoredata + 6;

		// Skip track and game name
		for (int i = 2; i != 0; --i)
		{
			while (*score++ != '\0') {}
		}
		score++;	// Skip unknown byte
		if (*(DWORD *)score != 0)
		{
			score += 4;		// Skip song length
		}
	}
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
			int next = PlayTick ();
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

int OPLmusicBlock::PlayTick ()
{
	BYTE reg, data;

	if (RawPlayer == NotRaw)
	{
		return playTick ();
	}
	else if (RawPlayer == RDosPlay)
	{
		while (score < scoredata + ScoreLen)
		{
			data = *score++;
			reg = *score++;
			switch (reg)
			{
			case 0:		// Delay
				if (data != 0)
				{
					return data;
				}
				break;

			case 2:		// Speed change or OPL3 switch
				if (data == 0)
				{
					SamplesPerTick = Scale (SampleRate, LittleShort(*(WORD *)(score)), 1193180);
					score += 2;
				}
				break;

			case 0xFF:	// End of song
				if (data == 0xFF)
				{
					return 0;
				}
				break;

			default:	// It's something to stuff into the OPL chip
				io->OPLwriteReg (0, reg, data);
				break;
			}
		}
	}
	else if (RawPlayer == IMF)
	{
		WORD delay = 0;

		while (delay == 0 && score + 4 - scoredata <= ScoreLen)
		{
			reg = score[0];
			data = score[1];
			delay = LittleShort(((WORD *)score)[1]);
			score += 4;
			io->OPLwriteReg (0, reg, data);
		}
		return delay;
	}
	return 0;
}

ADD_STAT (opl)
{
	if (BlockForStats != NULL)
	{
		FString out;
		char star[3] = { TEXTCOLOR_ESCAPE, 'A', '*' };
		for (uint i = 0; i < BlockForStats->io->OPLchannels; ++i)
		{
			if (BlockForStats->channels[i].flags & CH_FREE)
			{
				star[1] = CR_BRICK + 'A';
			}
			else if (BlockForStats->channels[i].flags & CH_SUSTAIN)
			{
				star[1] = CR_ORANGE + 'A';
			}
			else if (BlockForStats->channels[i].flags & CH_SECONDARY)
			{
				star[1] = CR_BLUE + 'A';
			}
			else
			{
				star[1] = CR_GREEN + 'A';
			}
			out.AppendCStrPart (star, 3);
		}
		return out;
	}
	else
	{
		return YM3812GetVoiceString ();
	}
}

struct DiskWriterIO : public OPLio
{
	DiskWriterIO () : File(NULL) {}
	virtual ~DiskWriterIO () { if (File != NULL) fclose (File); }
	int OPLinit(const char *filename);
	virtual void OPLwriteReg(int which, uint reg, uchar data);

	FILE *File;
	bool RawFormat;
};

class OPLmusicWriter : public musicBlock
{
public:
	OPLmusicWriter (const char *songname, const char *filename);
	~OPLmusicWriter ();
	void Go ();

	bool SharingData;
	FILE *File;
};

OPLmusicWriter::OPLmusicWriter (const char *songname, const char *filename)
{
	io = NULL;
	SharingData = true;
	if (songname == NULL)
	{
		if (BlockForStats == NULL)
		{
			Printf ("Not currently playing an OPL song.\n");
			return;
		}
		scoredata = BlockForStats->scoredata;
		OPLinstruments = BlockForStats->OPLinstruments;
	}
	else
	{
		SharingData = false;
		int lumpnum = Wads.CheckNumForName (songname, ns_music);
		if (lumpnum == -1)
		{
			Printf ("Song %s is unknown.\n", songname);
			return;
		}
		FWadLump song = Wads.OpenLumpNum (lumpnum);
		scoredata = new BYTE [song.GetLength ()];
		song.Read (scoredata, song.GetLength());
		FWadLump genmidi = Wads.OpenLumpName ("GENMIDI");
		OPLloadBank (genmidi);
	}
	io = new DiskWriterIO ();
	if (((DiskWriterIO *)io)->OPLinit (filename) == 0)
	{
		OPLplayMusic ();
		score = scoredata + ((MUSheader *)scoredata)->scoreStart;
		Go ();
	}
}

OPLmusicWriter::~OPLmusicWriter ()
{
	if (io != NULL) delete io;
	if (!SharingData)
	{
		delete scoredata;
	}
	else
	{
		OPLinstruments = NULL;
	}
}

void OPLmusicWriter::Go ()
{
	int next;

	while ((next = playTick()) != 0)
	{
		MLtime += next;
		while (next > 255)
		{
			io->OPLwriteReg (10, 0, 255);
			next -= 255;
		}
		io->OPLwriteReg (10, 0, next);
	}
	io->OPLwriteReg (10, 0xFF, 0xFF);
}

int DiskWriterIO::OPLinit (const char *filename)
{
	int numchips;
	//size_t namelen;

	// If the file extension is unknown or not present, the default format
	// is RAW. Otherwise, you can use DRO. But not right now. The DRO format
	// is still in a state of flux, so I don't want the hassle.
	//namelen = strlen (filename);
	RawFormat = 1; //(namelen < 5 || stricmp (filename + namelen - 4, ".dro") != 0);
	File = fopen (filename, "wb");
	if (File == NULL)
	{
		return -1;
	}

	if (RawFormat)
	{
		fwrite ("RAWADATA", 1, 8, File);
		WORD clock = LittleShort(17045/2);
		fwrite (&clock, 2, 1, File);
		numchips = 1;
	}
	else
	{
		numchips = 2;
	}

	OPLchannels = OPL2CHANNELS*numchips;
	for (int i = 0; i < numchips; ++i)
	{
		OPLwriteReg (i, 0x01, 0x20);	// enable Waveform Select
		OPLwriteReg (i, 0x0B, 0x40);	// turn off CSW mode
		OPLwriteReg (i, 0xBD, 0x00);	// set vibrato/tremolo depth to low, set melodic mode
	}
	OPLshutup();
	return 0;
}

void DiskWriterIO::OPLwriteReg(int which, uint reg, uchar data)
{
	if (which == 10 || (reg != 0 && reg != 2 && reg != 0xFF))
	{
		struct { BYTE data, reg; } out = { data, reg };
		fwrite (&out, 2, 1, File);
	}
	else
	{
		reg = reg;
	}
}

CCMD (writeopl)
{
	if (argv.argc() == 2)
	{
		OPLmusicWriter writer (NULL, argv[1]);
	}
	else if (argv.argc() == 3)
	{
		OPLmusicWriter writer (argv[1], argv[2]);
	}
	else
	{
		Printf ("Usage: writeopl [songname] <filename>");
	}

}
