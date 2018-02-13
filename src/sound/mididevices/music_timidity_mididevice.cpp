/*
** music_timidity_mididevice.cpp
** Provides access to TiMidity as a generic MIDI device.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "w_wad.h"
#include "v_text.h"
#include "timidity/timidity.h"
#include <errno.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------
// MIDI file played with TiMidity++ and possibly streamed through the Sound System

struct FmtChunk
{
	uint32_t ChunkID;
	uint32_t ChunkLen;
	uint16_t  FormatTag;
	uint16_t  Channels;
	uint32_t SamplesPerSec;
	uint32_t AvgBytesPerSec;
	uint16_t  BlockAlign;
	uint16_t  BitsPerSample;
	uint16_t  ExtensionSize;
	uint16_t  ValidBitsPerSample;
	uint32_t ChannelMask;
	uint32_t SubFormatA;
	uint16_t  SubFormatB;
	uint16_t  SubFormatC;
	uint8_t  SubFormatD[8];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// TimidityMIDIDevice Constructor
//
//==========================================================================

TimidityMIDIDevice::TimidityMIDIDevice(const char *args)
{
	Renderer = nullptr;
	Renderer = new Timidity::Renderer((float)SampleRate, args);
}

//==========================================================================
//
// TimidityMIDIDevice Destructor
//
//==========================================================================

TimidityMIDIDevice::~TimidityMIDIDevice()
{
	Close();
	if (Renderer != nullptr)
	{
		delete Renderer;
	}
}

//==========================================================================
//
// TimidityMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int TimidityMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	int ret = OpenStream(2, 0, callback, userdata);
	if (ret == 0)
	{
		Renderer->Reset();
	}
	return ret;
}

//==========================================================================
//
// TimidityMIDIDevice :: PrecacheInstruments
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
//==========================================================================

void TimidityMIDIDevice::PrecacheInstruments(const uint16_t *instruments, int count)
{
	for (int i = 0; i < count; ++i)
	{
		Renderer->MarkInstrument((instruments[i] >> 7) & 127, instruments[i] >> 14, instruments[i] & 127);
	}
	Renderer->load_missing_instruments();
}

//==========================================================================
//
// TimidityMIDIDevice :: HandleEvent
//
//==========================================================================

void TimidityMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	Renderer->HandleEvent(status, parm1, parm2);
}

//==========================================================================
//
// TimidityMIDIDevice :: HandleLongEvent
//
//==========================================================================

void TimidityMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
	Renderer->HandleLongMessage(data, len);
}

//==========================================================================
//
// TimidityMIDIDevice :: ComputeOutput
//
//==========================================================================

void TimidityMIDIDevice::ComputeOutput(float *buffer, int len)
{
	Renderer->ComputeOutput(buffer, len);
}

//==========================================================================
//
// TimidityMIDIDevice :: GetStats
//
//==========================================================================

FString TimidityMIDIDevice::GetStats()
{
	FString dots;
	FString out;
	int i, used;

	CritSec.Enter();
	for (i = used = 0; i < Renderer->voices; ++i)
	{
		int status = Renderer->voice[i].status;

		if (!(status & Timidity::VOICE_RUNNING))
		{
			dots << TEXTCOLOR_PURPLE".";
		}
		else
		{
			used++;
			if (status & Timidity::VOICE_SUSTAINING)
			{
				dots << TEXTCOLOR_BLUE;
			}
			else if (status & Timidity::VOICE_RELEASING)
			{
				dots << TEXTCOLOR_ORANGE;
			}
			else if (status & Timidity::VOICE_STOPPING)
			{
				dots << TEXTCOLOR_RED;
			}
			else
			{
				dots << TEXTCOLOR_GREEN;
			}
			if (!Renderer->voice[i].eg1.env.bUpdating)
			{
				dots << "+";
			}
			else
			{
				dots << ('0' + Renderer->voice[i].eg1.gf1.stage);
			}
		}
	}
	CritSec.Leave();
	out.Format(TEXTCOLOR_YELLOW"%3d/%3d ", used, Renderer->voices);
	out += dots;
	if (Renderer->cut_notes | Renderer->lost_notes)
	{
		out.AppendFormat(TEXTCOLOR_RED" %d/%d", Renderer->cut_notes, Renderer->lost_notes);
	}
	return out;
}

//==========================================================================
//
// TimidityWaveWriterMIDIDevice Constructor
//
//==========================================================================

TimidityWaveWriterMIDIDevice::TimidityWaveWriterMIDIDevice(const char *filename, int rate)
	:TimidityMIDIDevice(nullptr)
{
	File = FileWriter::Open(filename);
	if (File != nullptr)
	{ // Write wave header
		uint32_t work[3];
		FmtChunk fmt;

		work[0] = MAKE_ID('R','I','F','F');
		work[1] = 0;								// filled in later
		work[2] = MAKE_ID('W','A','V','E');
		if (4*3 != File->Write(work, 4 * 3)) goto fail;

		fmt.ChunkID = MAKE_ID('f','m','t',' ');
		fmt.ChunkLen = LittleLong(uint32_t(sizeof(fmt) - 8));
		fmt.FormatTag = LittleShort(0xFFFE);		// WAVE_FORMAT_EXTENSIBLE
		fmt.Channels = LittleShort(2);
		fmt.SamplesPerSec = LittleLong((int)Renderer->rate);
		fmt.AvgBytesPerSec = LittleLong((int)Renderer->rate * 8);
		fmt.BlockAlign = LittleShort(8);
		fmt.BitsPerSample = LittleShort(32);
		fmt.ExtensionSize = LittleShort(2 + 4 + 16);
		fmt.ValidBitsPerSample = LittleShort(32);
		fmt.ChannelMask = LittleLong(3);
		fmt.SubFormatA = LittleLong(0x00000003);	// Set subformat to KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
		fmt.SubFormatB = LittleShort(0x0000);
		fmt.SubFormatC = LittleShort(0x0010);
		fmt.SubFormatD[0] = 0x80;
		fmt.SubFormatD[1] = 0x00;
		fmt.SubFormatD[2] = 0x00;
		fmt.SubFormatD[3] = 0xaa;
		fmt.SubFormatD[4] = 0x00;
		fmt.SubFormatD[5] = 0x38;
		fmt.SubFormatD[6] = 0x9b;
		fmt.SubFormatD[7] = 0x71;
		if (sizeof(fmt) != File->Write(&fmt, sizeof(fmt))) goto fail;

		work[0] = MAKE_ID('d','a','t','a');
		work[1] = 0;								// filled in later
		if (8 !=File->Write(work, 8)) goto fail;

		return;
fail:
		Printf("Failed to write %s: %s\n", filename, strerror(errno));
		delete File;
		File = nullptr;
	}
}

//==========================================================================
//
// TimidityWaveWriterMIDIDevice Destructor
//
//==========================================================================

TimidityWaveWriterMIDIDevice::~TimidityWaveWriterMIDIDevice()
{
	if (File != nullptr)
	{
		long pos = File->Tell();
		uint32_t size;

		// data chunk size
		size = LittleLong(uint32_t(pos - 8));
		if (0 == File->Seek(4, SEEK_SET))
		{
			if (4 == File->Write(&size, 4))
			{
				size = LittleLong(uint32_t(pos - 12 - sizeof(FmtChunk) - 8));
				if (0 == File->Seek(4 + sizeof(FmtChunk) + 4, SEEK_CUR))
				{
					if (1 == File->Write(&size, 4))
					{
						delete File;
						return;
					}
				}
			}
		}
		Printf("Could not finish writing wave file: %s\n", strerror(errno));
		delete File;
	}
}

//==========================================================================
//
// TimidityWaveWriterMIDIDevice :: Resume
//
//==========================================================================

int TimidityWaveWriterMIDIDevice::Resume()
{
	float writebuffer[4096];

	while (ServiceStream(writebuffer, sizeof(writebuffer)))
	{
		if (File->Write(writebuffer, sizeof(writebuffer)) != sizeof(writebuffer))
		{
			Printf("Could not write entire wave file: %s\n", strerror(errno));
			return 1;
		}
	}
	return 0;
}

//==========================================================================
//
// TimidityWaveWriterMIDIDevice Stop
//
//==========================================================================

void TimidityWaveWriterMIDIDevice::Stop()
{
}
