/*
** opl_mus_player.cpp
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
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

#ifdef _WIN32
#include <io.h>
#endif
#include <string.h>
#include <assert.h>
#include <math.h>

#include "opl_mus_player.h"
#include "opl.h"
#include "w_wad.h"
#include "templates.h"
#include "c_cvars.h"

#define IMF_RATE				700.0

EXTERN_CVAR (Int, opl_numchips)

OPLmusicBlock::OPLmusicBlock()
{
	scoredata = NULL;
	NextTickIn = 0;
	LastOffset = 0;
	NumChips = MIN(*opl_numchips, 2);
	Looping = false;
	FullPan = false;
	io = NULL;
	io = new OPLio;
}

OPLmusicBlock::~OPLmusicBlock()
{
	delete io;
}

void OPLmusicBlock::ResetChips ()
{
	ChipAccess.Enter();
	io->Reset ();
	NumChips = io->Init(MIN(*opl_numchips, 2), FullPan);
	ChipAccess.Leave();
}

void OPLmusicBlock::Restart()
{
	stopAllVoices ();
	resetAllControllers (127);
	playingcount = 0;
	LastOffset = 0;
}

OPLmusicFile::OPLmusicFile (FileReader &reader)
	: ScoreLen ((int)reader.GetLength())
{
	if (io == NULL)
	{
		return;
	}

	scoredata = new uint8_t[ScoreLen];

    if (reader.Read(scoredata, ScoreLen) != ScoreLen)
    {
fail:	delete[] scoredata;
        scoredata = NULL;
        return;
    }

	if (0 == (NumChips = io->Init(NumChips)))
	{
		goto fail;
	}

	// Check for RDosPlay raw OPL format
	if (((uint32_t *)scoredata)[0] == MAKE_ID('R','A','W','A') &&
			 ((uint32_t *)scoredata)[1] == MAKE_ID('D','A','T','A'))
	{
		RawPlayer = RDosPlay;
		if (*(uint16_t *)(scoredata + 8) == 0)
		{ // A clock speed of 0 is bad
			*(uint16_t *)(scoredata + 8) = 0xFFFF; 
		}
		SamplesPerTick = LittleShort(*(uint16_t *)(scoredata + 8)) / ADLIB_CLOCK_MUL;
	}
	// Check for DosBox OPL dump
	else if (((uint32_t *)scoredata)[0] == MAKE_ID('D','B','R','A') &&
		((uint32_t *)scoredata)[1] == MAKE_ID('W','O','P','L'))
	{
		if (LittleShort(((uint16_t *)scoredata)[5]) == 1)
		{
			RawPlayer = DosBox1;
			SamplesPerTick = OPL_SAMPLE_RATE / 1000;
			ScoreLen = MIN<int>(ScoreLen - 24, LittleLong(((uint32_t *)scoredata)[4])) + 24;
		}
		else if (((uint32_t *)scoredata)[2] == MAKE_ID(2,0,0,0))
		{
			bool okay = true;
			if (scoredata[21] != 0)
			{
				Printf("Unsupported DOSBox Raw OPL format %d\n", scoredata[20]);
				okay = false;
			}
			if (scoredata[22] != 0)
			{
				Printf("Unsupported DOSBox Raw OPL compression %d\n", scoredata[21]);
				okay = false;
			}
			if (!okay)
				goto fail;
			RawPlayer = DosBox2;
			SamplesPerTick = OPL_SAMPLE_RATE / 1000;
			int headersize = 0x1A + scoredata[0x19];
			ScoreLen = MIN<int>(ScoreLen - headersize, LittleLong(((uint32_t *)scoredata)[3]) * 2) + headersize;
		}
		else
		{
			Printf("Unsupported DOSBox Raw OPL version %d.%d\n", LittleShort(((uint16_t *)scoredata)[4]), LittleShort(((uint16_t *)scoredata)[5]));
			goto fail;
		}
	}
	// Check for modified IMF format (includes a header)
	else if (((uint32_t *)scoredata)[0] == MAKE_ID('A','D','L','I') &&
		     scoredata[4] == 'B' && scoredata[5] == 1)
	{
		int songlen;
		uint8_t *max = scoredata + ScoreLen;
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
		songlen = LittleLong(*(uint32_t *)score);
		if (songlen != 0 && (songlen +=4) < ScoreLen - (score - scoredata))
		{
			ScoreLen = songlen + int(score - scoredata);
		}
	}
	else
	{
		goto fail;
	}

	Restart ();
}

OPLmusicFile::~OPLmusicFile ()
{
	if (scoredata != NULL)
	{
		io->Reset ();
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
	WhichChip = 0;
	switch (RawPlayer)
	{
	case RDosPlay:
		score = scoredata + 10;
		SamplesPerTick = LittleShort(*(uint16_t *)(scoredata + 8)) / ADLIB_CLOCK_MUL;
		break;

	case DosBox1:
		score = scoredata + 24;
		SamplesPerTick = OPL_SAMPLE_RATE / 1000;
		break;

	case DosBox2:
		score = scoredata + 0x1A + scoredata[0x19];
		SamplesPerTick = OPL_SAMPLE_RATE / 1000;
		break;

	case IMF:
		score = scoredata + 6;

		// Skip track and game name
		for (int i = 2; i != 0; --i)
		{
			while (*score++ != '\0') {}
		}
		score++;	// Skip unknown byte
		if (*(uint32_t *)score != 0)
		{
			score += 4;		// Skip song length
		}
		break;
	}
	io->SetClockRate(SamplesPerTick);
}

bool OPLmusicBlock::ServiceStream (void *buff, int numbytes)
{
	float *samples1 = (float *)buff;
	int stereoshift = (int)(FullPan | io->IsOPL3);
	int numsamples = numbytes / (sizeof(float) << stereoshift);
	bool prevEnded = false;
	bool res = true;

	memset(buff, 0, numbytes);

	ChipAccess.Enter();
	while (numsamples > 0)
	{
		double ticky = NextTickIn;
		int tick_in = int(NextTickIn);
		int samplesleft = MIN(numsamples, tick_in);
		size_t i;

		if (samplesleft > 0)
		{
			for (i = 0; i < io->NumChips; ++i)
			{
				io->chips[i]->Update(samples1, samplesleft);
			}
			OffsetSamples(samples1, samplesleft << stereoshift);
			assert(NextTickIn == ticky);
			NextTickIn -= samplesleft;
			assert (NextTickIn >= 0);
			numsamples -= samplesleft;
			samples1 += samplesleft << stereoshift;
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
						for (i = 0; i < io->NumChips; ++i)
						{
							io->chips[i]->Update(samples1, numsamples);
						}
						OffsetSamples(samples1, numsamples << stereoshift);
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
				io->WriteDelay(next);
				NextTickIn += SamplesPerTick * next;
				assert (NextTickIn >= 0);
			}
		}
	}
	ChipAccess.Leave();
	return res;
}

void OPLmusicBlock::OffsetSamples(float *buff, int count)
{
	// Three out of four of the OPL waveforms are non-negative. Depending on
	// timbre selection, this can cause the output waveform to tend toward
	// very large positive values. Heretic's music is particularly bad for
	// this. This function attempts to compensate by offseting the sample
	// data back to around the [-1.0, 1.0] range.

	double max = -1e10, min = 1e10, offset, step;
	int i, ramp, largest_at = 0;

	// Find max and min values for this segment of the waveform.
	for (i = 0; i < count; ++i)
	{
		if (buff[i] > max)
		{
			max = buff[i];
			largest_at = i;
		}
		if (buff[i] < min)
		{
			min = buff[i];
			largest_at = i;
		}
	}
	// Prefer to keep the offset at 0, even if it means a little clipping.
	if (LastOffset == 0 && min >= -1.1 && max <= 1.1)
	{
		offset = 0;
	}
	else
	{
		offset = (max + min) / 2;
		// If the new offset is close to 0, make it 0 to avoid making another
		// full loop through the sample data.
		if (fabs(offset) < 1/256.0)
		{
			offset = 0;
		}
	}
	// Ramp the offset change so there aren't any abrupt clicks in the output.
	// If the ramp is too short, it can sound scratchy. cblood2.mid is
	// particularly unforgiving of short ramps.
	if (count >= 512)
	{
		ramp = 512;
		step = (offset - LastOffset) / 512;
	}
	else
	{
		ramp = MIN(count, MAX(196, largest_at));
		step = (offset - LastOffset) / ramp;
	}
	offset = LastOffset;
	i = 0;
	if (step != 0)
	{
		for (; i < ramp; ++i)
		{
			buff[i] = float(buff[i] - offset);
			offset += step;
		}
	}
	if (offset != 0)
	{
		for (; i < count; ++i)
		{
			buff[i] = float(buff[i] - offset);
		}
	}
	LastOffset = float(offset);
}

int OPLmusicFile::PlayTick ()
{
	uint8_t reg, data;
	uint16_t delay;

	switch (RawPlayer)
	{
	case RDosPlay:
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
					SamplesPerTick = LittleShort(*(uint16_t *)(score)) / ADLIB_CLOCK_MUL;
					io->SetClockRate(SamplesPerTick);
					score += 2;
				}
				else if (data == 1)
				{
					WhichChip = 0;
				}
				else if (data == 2)
				{
					WhichChip = 1;
				}
				break;

			case 0xFF:	// End of song
				if (data == 0xFF)
				{
					return 0;
				}
				break;

			default:	// It's something to stuff into the OPL chip
				io->WriteRegister(WhichChip, reg, data);
				break;
			}
		}
		break;

	case DosBox1:
		while (score < scoredata + ScoreLen)
		{
			reg = *score++;

			if (reg == 4)
			{
				reg = *score++;
				data = *score++;
			}
			else if (reg == 0)
			{ // One-byte delay
				return *score++ + 1;
			}
			else if (reg == 1)
			{ // Two-byte delay
				int delay = score[0] + (score[1] << 8) + 1;
				score += 2;
				return delay;
			}
			else if (reg == 2)
			{ // Select OPL chip 0
				WhichChip = 0;
				continue;
			}
			else if (reg == 3)
			{ // Select OPL chip 1
				WhichChip = 1;
				continue;
			}
			else
			{
				data = *score++;
			}
			io->WriteRegister(WhichChip, reg, data);
		}
		break;

	case DosBox2:
		{
			uint8_t *to_reg = scoredata + 0x1A;
			uint8_t to_reg_size = scoredata[0x19];
			uint8_t short_delay_code = scoredata[0x17];
			uint8_t long_delay_code = scoredata[0x18];

			while (score < scoredata + ScoreLen)
			{
				uint8_t code = *score++;
				data = *score++;

				// Which OPL chip to write to is encoded in the high bit of the code value.
				int which = !!(code & 0x80);
				code &= 0x7F;

				if (code == short_delay_code)
				{
					return data + 1;
				}
				else if (code == long_delay_code)
				{
					return (data + 1) << 8;
				}
				else if (code < to_reg_size)
				{
					io->WriteRegister(which, to_reg[code], data);
				}
			}
		}
		break;

	case IMF:
		delay = 0;
		while (delay == 0 && score + 4 - scoredata <= ScoreLen)
		{
			if (*(uint32_t *)score == 0xFFFFFFFF)
			{ // This is a special value that means to end the song.
				return 0;
			}
			reg = score[0];
			data = score[1];
			delay = LittleShort(((uint16_t *)score)[1]);
			score += 4;
			io->WriteRegister (0, reg, data);
		}
		return delay;
	}
	return 0;
}

/*
ADD_STAT (opl)
{
	return YM3812GetVoiceString ();
}
*/

OPLmusicFile::OPLmusicFile(const OPLmusicFile *source, const char *filename)
{
	ScoreLen = source->ScoreLen;
	scoredata = new uint8_t[ScoreLen];
	memcpy(scoredata, source->scoredata, ScoreLen);
	SamplesPerTick = source->SamplesPerTick;
	RawPlayer = source->RawPlayer;
	score = source->score;
	NumChips = source->NumChips;
	WhichChip = 0;
	if (io != NULL)
	{
		delete io;
	}
	io = new DiskWriterIO(filename);
	NumChips = io->Init(NumChips);
	Restart();
}

void OPLmusicFile::Dump()
{
	int time;

	time = PlayTick();
	while (time != 0)
	{
		io->WriteDelay(time);
		time = PlayTick();
	}
}
