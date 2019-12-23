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

#include "mididevice.h"
#include "zmusic/m_swap.h"
#include "../music_common/fileio.h"
#include <errno.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct FmtChunk
{
	//uint32_t ChunkID;
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
// MIDIWaveWriter Constructor
//
//==========================================================================

MIDIWaveWriter::MIDIWaveWriter(const char *filename, SoftSynthMIDIDevice *playdevice)
	: SoftSynthMIDIDevice(playdevice->GetSampleRate())
{
	File = MusicIO::utf8_fopen(filename, "wt");
	playDevice = playdevice;
	if (File != nullptr)
	{ // Write wave header
		FmtChunk fmt;

		if (fwrite("RIFF\0\0\0\0WAVEfmt ", 1, 16, File) != 16) goto fail;

		playDevice->CalcTickRate();
		fmt.ChunkLen = LittleLong(uint32_t(sizeof(fmt) - 4));
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
		if (sizeof(fmt) != fwrite(&fmt, 1, sizeof(fmt), File)) goto fail;

		if (fwrite("data\0\0\0\0", 1, 8, File) != 8) goto fail;

		return;
	fail:
		char buffer[80];
		fclose(File);
		File = nullptr;
		snprintf(buffer, 80, "Failed to write %s: %s\n", filename, strerror(errno));
		throw std::runtime_error(buffer);
	}
}

//==========================================================================
//
// MIDIWaveWriter Destructor
//
//==========================================================================

bool MIDIWaveWriter::CloseFile()
{
	if (File != nullptr)
	{
		auto pos = ftell(File);
		uint32_t size;

		// data chunk size
		size = LittleLong(uint32_t(pos - 8));
		if (0 == fseek(File, 4, SEEK_SET))
		{
			if (4 == fwrite(&size, 1, 4, File))
			{
				size = LittleLong(uint32_t(pos - 12 - sizeof(FmtChunk) - 8));
				if (0 == fseek(File, 4 + sizeof(FmtChunk) + 4, SEEK_CUR))
				{
					if (4 == fwrite(&size, 1, 5, File))
					{
						fclose(File);
						File = nullptr;
						return true;
					}
				}
			}
		}
		fclose(File);
		File = nullptr;
	}
	return false;
}

//==========================================================================
//
// MIDIWaveWriter :: Resume
//
//==========================================================================

int MIDIWaveWriter::Resume()
{
	float writebuffer[4096];

	while (ServiceStream(writebuffer, sizeof(writebuffer)))
	{
		if (fwrite(writebuffer, 1, sizeof(writebuffer), File) != sizeof(writebuffer))
		{
			fclose(File);
			File = nullptr;
			char buffer[80];
			snprintf(buffer, 80, "Could not write entire wave file: %s\n", strerror(errno));
			throw std::runtime_error(buffer);
		}
	}
	return 0;
}

//==========================================================================
//
// MIDIWaveWriter Stop
//
//==========================================================================

void MIDIWaveWriter::Stop()
{
}
