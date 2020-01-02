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

#include <stdlib.h>
#include "mididevice.h"
#include "zmusic/zmusic_internal.h"

#ifdef HAVE_GUS

#include "timidity/timidity.h"
#include "timidity/playmidi.h"
#include "timidity/instrum.h"
#include "../../music_common/fileio.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

GUSConfig gusConfig;

//==========================================================================
//
// The actual device.
//
//==========================================================================

namespace Timidity { struct Renderer; }

class TimidityMIDIDevice : public SoftSynthMIDIDevice
{
	void LoadInstruments();
public:
	TimidityMIDIDevice(int samplerate);
	~TimidityMIDIDevice();
	
	int OpenRenderer() override;
	void PrecacheInstruments(const uint16_t *instruments, int count) override;
	int GetDeviceType() const override { return MDEV_GUS; }
	
protected:
	Timidity::Renderer *Renderer;
	
	void HandleEvent(int status, int parm1, int parm2) override;
	void HandleLongEvent(const uint8_t *data, int len) override;
	void ComputeOutput(float *buffer, int len) override;
};


// CODE --------------------------------------------------------------------


void TimidityMIDIDevice::LoadInstruments()
{
	if (gusConfig.dmxgus.size())
	{
		// Check if we got some GUS data before using it.
		std::string ultradir = getenv("ULTRADIR");
		if (ultradir.length() || gusConfig.gus_patchdir.length() != 0)
		{
			auto psreader = new MusicIO::FileSystemSoundFontReader("");
			
			// The GUS put its patches in %ULTRADIR%/MIDI so we can try that
			if (ultradir.length())
			{
				ultradir += "/midi";
				psreader->add_search_path(ultradir.c_str());
			}
			// Load DMXGUS lump and patches from gus_patchdir
			if (gusConfig.gus_patchdir.length() != 0) psreader->add_search_path(gusConfig.gus_patchdir.c_str());
			
			gusConfig.instruments.reset(new Timidity::Instruments(psreader));
			bool success = gusConfig.instruments->LoadDMXGUS(gusConfig.gus_memsize, (const char*)gusConfig.dmxgus.data(), gusConfig.dmxgus.size()) >= 0;
			
			gusConfig.dmxgus.clear();
			
			if (success)
			{
				gusConfig.loadedConfig = "DMXGUS";
				return;
			}
		}
		gusConfig.loadedConfig = "";
		gusConfig.instruments.reset();
		throw std::runtime_error("Unable to initialize DMXGUS for GUS MIDI device");
	}
	else if (gusConfig.reader)
	{
		gusConfig.loadedConfig = gusConfig.readerName;
		gusConfig.instruments.reset(new Timidity::Instruments(gusConfig.reader));
		bool err = gusConfig.instruments->LoadConfig() < 0;
		gusConfig.reader = nullptr;
		
		if (err)
		{
			gusConfig.instruments.reset();
			gusConfig.loadedConfig = "";
			throw std::runtime_error("Unable to initialize instruments for GUS MIDI device");
		}
	}
	else if (gusConfig.instruments == nullptr)
	{
		throw std::runtime_error("No instruments set for GUS device");
	}
}

//==========================================================================
//
// TimidityMIDIDevice Constructor
//
//==========================================================================

TimidityMIDIDevice::TimidityMIDIDevice(int samplerate)
	: SoftSynthMIDIDevice(samplerate, 11025, 65535)
{
	LoadInstruments();
	Renderer = new Timidity::Renderer((float)SampleRate, gusConfig.midi_voices, gusConfig.instruments.get());
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

int TimidityMIDIDevice::OpenRenderer()
{
	Renderer->Reset();
	return 0;
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
	for (int i = 0; i < len * 2; i++) buffer[i] *= 0.7f;
}

//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
// Sets up the date to load the instruments for the GUS device.
// The actual instrument loader is part of the device.
//
//==========================================================================

bool GUS_SetupConfig(const char* args)
{
	gusConfig.reader = nullptr;
	if ((gusConfig.gus_dmxgus && *args == 0) || !stricmp(args, "DMXGUS"))
	{
		if (stricmp(gusConfig.loadedConfig.c_str(), "DMXGUS") == 0) return false; // aleady loaded
		if (gusConfig.dmxgus.size() > 0)
		{
			gusConfig.readerName = "DMXGUS";
			return true;
		}
	}
	if (*args == 0) args = gusConfig.gus_config.c_str();
	if (stricmp(gusConfig.loadedConfig.c_str(), args) == 0) return false; // aleady loaded

	MusicIO::SoundFontReaderInterface* reader = MusicIO::ClientOpenSoundFont(args, SF_GUS | SF_SF2);
	if (!reader && MusicIO::fileExists(args))
	{
		auto f = MusicIO::utf8_fopen(args, "rb");
		if (f)
		{
			char test[12] = {};
			fread(test, 1, 12, f);
			fclose(f);
			// If the passed file is an SF2 sound font we need to use the special reader that fakes a config for it.
			if (memcmp(test, "RIFF", 4) == 0 && memcmp(test + 8, "sfbk", 4) == 0)
				reader = new MusicIO::SF2Reader(args);
		}
		if (!reader) reader = new MusicIO::FileSystemSoundFontReader(args, true);
	}

	if (reader == nullptr)
	{
		char error[80];
		snprintf(error, 80, "GUS: %s: Unable to load sound font\n", args);
		throw std::runtime_error(error);
	}
	gusConfig.reader = reader;
	gusConfig.readerName = args;
	return true;
}

#
MIDIDevice* CreateTimidityMIDIDevice(const char* Args, int samplerate)
{
	GUS_SetupConfig(Args);
	return new TimidityMIDIDevice(samplerate);
}

#else
MIDIDevice* CreateTimidityMIDIDevice(const char* Args, int samplerate)
{
	throw std::runtime_error("GUS device not supported in this configuration");
}
#endif
