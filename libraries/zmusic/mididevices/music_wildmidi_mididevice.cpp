/*
** music_wildmidi_mididevice.cpp
** Provides access to WildMidi as a generic MIDI device.
**
**---------------------------------------------------------------------------
** Copyright 2015 Randy Heit
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
#include "wildmidi/wildmidi_lib.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// WildMidi implementation of a MIDI device ---------------------------------

class WildMIDIDevice : public SoftSynthMIDIDevice
{
public:
	WildMIDIDevice(WildMidiConfig* config, int samplerate);
	~WildMIDIDevice();
	
	int OpenRenderer();
	void PrecacheInstruments(const uint16_t *instruments, int count);
	std::string GetStats();
	int GetDeviceType() const override { return MDEV_WILDMIDI; }
	
protected:
	WildMidi::Renderer *Renderer;
	std::shared_ptr<WildMidi::Instruments> instruments;
	
	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
	void ChangeSettingInt(const char *opt, int set);
	void LoadInstruments(WildMidiConfig* config);

};


// CODE --------------------------------------------------------------------

//==========================================================================
//
//
//
//==========================================================================

void WildMIDIDevice::LoadInstruments(WildMidiConfig* config)
{
	if (config->reader)
	{
		config->loadedConfig = config->readerName;
		config->instruments.reset(new WildMidi::Instruments(config->reader, SampleRate));
		bool success = config->instruments->LoadConfig(config->readerName.c_str());
		config->reader = nullptr;

		if (!success)
		{
			config->instruments.reset();
			config->loadedConfig = "";
			throw std::runtime_error("Unable to initialize instruments for WildMidi device");
		}
	}
	else if (config->instruments == nullptr)
	{
		throw std::runtime_error("No instruments set for WildMidi device");
	}
	instruments = config->instruments;
	if (instruments->LoadConfig(nullptr) < 0)
	{
		throw std::runtime_error("Unable to load instruments set for WildMidi device");
	}
}

//==========================================================================
//
// WildMIDIDevice Constructor
//
//==========================================================================

WildMIDIDevice::WildMIDIDevice(WildMidiConfig *config, int samplerate)
	:SoftSynthMIDIDevice(samplerate, 11025, 65535)
{
	Renderer = NULL;
	LoadInstruments(config);

	Renderer = new WildMidi::Renderer(instruments.get());
	int flags = 0;
	if (config->enhanced_resampling) flags |= WildMidi::WM_MO_ENHANCED_RESAMPLING;
	if (config->reverb) flags |= WildMidi::WM_MO_REVERB;
	Renderer->SetOption(WildMidi::WM_MO_ENHANCED_RESAMPLING | WildMidi::WM_MO_REVERB, flags);
}

//==========================================================================
//
// WildMIDIDevice Destructor
//
//==========================================================================

WildMIDIDevice::~WildMIDIDevice()
{
	Close();
	if (Renderer != NULL)
	{
		delete Renderer;
	}
}

//==========================================================================
//
// WildMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int WildMIDIDevice::OpenRenderer()
{
	return 0;	// This one's a no-op
}

//==========================================================================
//
// WildMIDIDevice :: PrecacheInstruments
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
//==========================================================================

void WildMIDIDevice::PrecacheInstruments(const uint16_t *instruments, int count)
{
	for (int i = 0; i < count; ++i)
	{
		Renderer->LoadInstrument((instruments[i] >> 7) & 127, instruments[i] >> 14, instruments[i] & 127);
	}
}


//==========================================================================
//
// WildMIDIDevice :: HandleEvent
//
//==========================================================================

void WildMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	Renderer->ShortEvent(status, parm1, parm2);
}

//==========================================================================
//
// WildMIDIDevice :: HandleLongEvent
//
//==========================================================================

void WildMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
	Renderer->LongEvent(data, len);
}

//==========================================================================
//
// WildMIDIDevice :: ComputeOutput
//
//==========================================================================

void WildMIDIDevice::ComputeOutput(float *buffer, int len)
{
	Renderer->ComputeOutput(buffer, len);
}

//==========================================================================
//
// WildMIDIDevice :: GetStats
//
//==========================================================================

std::string WildMIDIDevice::GetStats()
{
	char out[20];
	snprintf(out, 20, "%3d voices", Renderer->GetVoiceCount());
	return out;
}

//==========================================================================
//
// WildMIDIDevice :: ChangeSettingInt
//
//==========================================================================

void WildMIDIDevice::ChangeSettingInt(const char *opt, int set)
{
	int option;
	if (!stricmp(opt, "wildmidi.reverb")) option = WildMidi::WM_MO_REVERB;
	else if (!stricmp(opt, "wildmidi.resampling")) option = WildMidi::WM_MO_ENHANCED_RESAMPLING;
	else return;
	int setit = option * int(set);
	Renderer->SetOption(option, setit);
}

//==========================================================================
//
//
//
//==========================================================================

MIDIDevice *CreateWildMIDIDevice(WildMidiConfig *config, int samplerate)
{
	return new WildMIDIDevice(config, samplerate);
}

