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

#define IMF_RATE				700.0

EXTERN_CVAR (Bool, opl_onechip)

OPLmusicBlock *BlockForStats;

OPLmusicBlock::OPLmusicBlock()
{
	scoredata = NULL;
	NextTickIn = 0;
	TwoChips = !opl_onechip;
	Looping = false;
	io = NULL;
#ifdef _WIN32
	InitializeCriticalSection (&ChipAccess);
#else
	ChipAccess = SDL_CreateMutex ();
	if (ChipAccess == NULL)
	{
		return;
	}
#endif
	io = new OPLio;
}

OPLmusicBlock::~OPLmusicBlock()
{
	BlockForStats = NULL;
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

void OPLmusicBlock::Serialize()
{
#ifdef _WIN32
	EnterCriticalSection (&ChipAccess);
#else
	if (SDL_mutexP (ChipAccess) != 0)
		return;
#endif
}

void OPLmusicBlock::Unserialize()
{
#ifdef _WIN32
	LeaveCriticalSection (&ChipAccess);
#else
	SDL_mutexV (ChipAccess);
#endif
}

void OPLmusicBlock::ResetChips ()
{
	TwoChips = !opl_onechip;
	Serialize();
	io->OPLdeinit ();
	io->OPLinit (TwoChips + 1, uint(OPL_SAMPLE_RATE));
	Unserialize();
}

void OPLmusicBlock::Restart()
{
	OPLstopMusic ();
	OPLplayMusic (127);
	MLtime = 0;
	playingcount = 0;
}

OPLmusicFile::OPLmusicFile (FILE *file, char * musiccache, int len, int maxSamples)
	: ScoreLen (len)
{
	if (io == NULL)
	{
		return;
	}

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

	if (io->OPLinit (TwoChips + 1, uint(OPL_SAMPLE_RATE)))
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
		SamplesPerTick = OPL_SAMPLE_RATE / 140.0;
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
		SamplesPerTick = OPL_SAMPLE_RATE * LittleShort(*(WORD *)(scoredata + 8)) / 1193180.0;
	}
	// Check for modified IMF format (includes a header)
	else if (((DWORD *)scoredata)[0] == MAKE_ID('A','D','L','I') &&
		     scoredata[4] == 'B' && scoredata[5] == 1)
	{
		int songlen;
		BYTE *max = scoredata + ScoreLen;
		RawPlayer = IMF;
		SamplesPerTick = OPL_SAMPLE_RATE / IMF_RATE;

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

	Restart ();
}

OPLmusicFile::~OPLmusicFile ()
{
	if (scoredata != NULL)
	{
		io->OPLdeinit ();
		delete[] scoredata;
		scoredata = NULL;
	}
}

bool OPLmusicFile::IsValid () const
{
	return scoredata != NULL;
}

void OPLmusicFile::SetLooping (bool loop)
{
	Looping = loop;
}

void OPLmusicFile::Restart ()
{
	OPLmusicBlock::Restart();
	if (RawPlayer == NotRaw)
	{
		score = scoredata + ((MUSheader *)scoredata)->scoreStart;
	}
	else if (RawPlayer == RDosPlay)
	{
		score = scoredata + 10;
		SamplesPerTick = OPL_SAMPLE_RATE * LittleShort(*(WORD *)(scoredata + 8)) / 1193180.0;
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
	float *samples = (float *)buff;
	float *samples1;
	int numsamples = numbytes / sizeof(float);
	bool prevEnded = false;
	bool res = true;

	samples1 = samples;
	memset(buff, 0, numbytes);

	Serialize();
	while (numsamples > 0)
	{
		double ticky = NextTickIn;
		int tick_in = int(NextTickIn);
		int samplesleft = MIN(numsamples, tick_in);

		if (samplesleft > 0)
		{
			YM3812UpdateOne (0, samples1, samplesleft);
			if (TwoChips)
			{
				YM3812UpdateOne (1, samples1, samplesleft);
			}
			assert(NextTickIn == ticky);
			NextTickIn -= samplesleft;
			assert (NextTickIn >= 0);
			numsamples -= samplesleft;
			samples1 += samplesleft;
		}
		
		if (NextTickIn < 1)
		{
			int next = PlayTick();
			assert(next >= 0);
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
				NextTickIn += SamplesPerTick * next;
				assert (NextTickIn >= 0);
				MLtime += next;
			}
		}
	}
	Unserialize();
	return res;
}

int OPLmusicFile::PlayTick ()
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
					SamplesPerTick = OPL_SAMPLE_RATE * LittleShort(*(WORD *)(score)) / 1193180.0;
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
			if (*(DWORD *)score == 0xFFFFFFFF)
			{ // This is a special value that means to end the song.
				return 0;
			}
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
		OPLplayMusic (127);
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
