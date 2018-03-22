/*
** music_wavewriter_mididevice.cpp
** Dumps a MIDI to a wave file by using one of the other software synths.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
** Copyright 2018 Christoph Oelckers
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
// TimidityWaveWriterMIDIDevice Constructor
//
//==========================================================================

MIDIWaveWriter::MIDIWaveWriter(const char *filename, SoftSynthMIDIDevice *playdevice)
	: SoftSynthMIDIDevice(playdevice->GetSampleRate())
{
	File = FileWriter::Open(filename);
	playDevice = playdevice;
	if (File != nullptr)
	{ // Write wave header
		uint32_t work[3];
		FmtChunk fmt;

		work[0] = MAKE_ID('R','I','F','F');
		work[1] = 0;								// filled in later
		work[2] = MAKE_ID('W','A','V','E');
		if (4*3 != File->Write(work, 4 * 3)) goto fail;


		playDevice->CalcTickRate();
		fmt.ChunkID = MAKE_ID('f','m','t',' ');
		fmt.ChunkLen = LittleLong(uint32_t(sizeof(fmt) - 8));
		fmt.FormatTag = LittleShort((uint16_t)0xFFFE);		// WAVE_FORMAT_EXTENSIBLE
		fmt.Channels = LittleShort((uint16_t)2);
		fmt.SamplesPerSec = LittleLong(SampleRate);
		fmt.AvgBytesPerSec = LittleLong(SampleRate * 8);
		fmt.BlockAlign = LittleShort((uint16_t)8);
		fmt.BitsPerSample = LittleShort((uint16_t)32);
		fmt.ExtensionSize = LittleShort((uint16_t)(2 + 4 + 16));
		fmt.ValidBitsPerSample = LittleShort((uint16_t)32);
		fmt.ChannelMask = LittleLong(3);
		fmt.SubFormatA = LittleLong(0x00000003);	// Set subformat to KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
		fmt.SubFormatB = 0x0000;
		fmt.SubFormatC = LittleShort((uint16_t)0x0010);
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

MIDIWaveWriter::~MIDIWaveWriter()
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
					if (4 == File->Write(&size, 4))
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

int MIDIWaveWriter::Resume()
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

void MIDIWaveWriter::Stop()
{
}
