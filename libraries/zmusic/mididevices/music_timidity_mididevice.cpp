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

#include "mididevice.h"
#include "timidity/timidity.h"
#include "timidity/playmidi.h"
#include "timidity/instrum.h"
#define USE_BASE_INTERFACE
#include "timidity/timidity_file.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------


//==========================================================================
//
// The actual device.
//
//==========================================================================

namespace Timidity { struct Renderer; }

class TimidityMIDIDevice : public SoftSynthMIDIDevice
{
	void LoadInstruments(GUSConfig *config);
public:
	TimidityMIDIDevice(GUSConfig *config, int samplerate);
	~TimidityMIDIDevice();
	
	int OpenRenderer();
	void PrecacheInstruments(const uint16_t *instruments, int count);
	int GetDeviceType() const override { return MDEV_GUS; }
	
protected:
	Timidity::Renderer *Renderer;
	std::shared_ptr<Timidity::Instruments> instruments;	// The device needs to hold a reference to this while the renderer is in use.
	
	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
};


// CODE --------------------------------------------------------------------


void TimidityMIDIDevice::LoadInstruments(GUSConfig *config)
{
	if (config->dmxgus.size())
	{
		// Check if we got some GUS data before using it.
		std::string ultradir = getenv("ULTRADIR");
		if (ultradir.length() || config->gus_patchdir.length() != 0)
		{
			auto psreader = new Timidity::BaseSoundFontReader;
			
			// The GUS put its patches in %ULTRADIR%/MIDI so we can try that
			if (ultradir.length())
			{
				ultradir += "/midi";
				psreader->timidity_add_path(ultradir.c_str());
			}
			// Load DMXGUS lump and patches from gus_patchdir
			if (config->gus_patchdir.length() != 0) psreader->timidity_add_path(config->gus_patchdir.c_str());
			
			config->instruments.reset(new Timidity::Instruments(psreader));
			bool success = config->instruments->LoadDMXGUS(config->gus_memsize, (const char*)config->dmxgus.data(), config->dmxgus.size()) >= 0;
			
			config->dmxgus.clear();
			
			if (success)
			{
				config->loadedConfig = "DMXGUS";
				return;
			}
		}
		config->loadedConfig = "";
		config->instruments.reset();
		throw std::runtime_error("Unable to initialize DMXGUS for GUS MIDI device");
	}
	else if (config->reader)
	{
		config->loadedConfig = config->readerName;
		config->instruments.reset(new Timidity::Instruments(config->reader));
		bool err = config->instruments->LoadConfig() < 0;
		config->reader = nullptr;
		
		if (err)
		{
			config->instruments.reset();
			config->loadedConfig = "";
			throw std::runtime_error("Unable to initialize instruments for GUS MIDI device");
		}
	}
	else if (config->instruments == nullptr)
	{
		throw std::runtime_error("No instruments set for GUS device");
	}
	instruments = config->instruments;
}

//==========================================================================
//
// TimidityMIDIDevice Constructor
//
//==========================================================================

TimidityMIDIDevice::TimidityMIDIDevice(GUSConfig *config, int samplerate)
	: SoftSynthMIDIDevice(samplerate, 11025, 65535)
{
	LoadInstruments(config);
	Renderer = new Timidity::Renderer((float)SampleRate, config->midi_voices, instruments.get());
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

MIDIDevice *CreateTimidityMIDIDevice(GUSConfig *config, int samplerate)
{
	return new TimidityMIDIDevice(config, samplerate);
}
