/*
** music_timiditypp_mididevice.cpp
** Provides access to timidity.exe
**
**---------------------------------------------------------------------------
** Copyright 2001-2017 Randy Heit
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

#include "mididevice.h"
#include "zmusic/zmusic_internal.h"

#ifdef HAVE_TIMIDITY

#include "timiditypp/timidity.h"
#include "timiditypp/instrum.h"
#include "timiditypp/playmidi.h"


TimidityConfig timidityConfig;

class TimidityPPMIDIDevice : public SoftSynthMIDIDevice
{
	std::shared_ptr<TimidityPlus::Instruments> instruments;
public:
	TimidityPPMIDIDevice(int samplerate);
	~TimidityPPMIDIDevice();

	int OpenRenderer() override;
	void PrecacheInstruments(const uint16_t *instruments, int count) override;
	//std::string GetStats();
	int GetDeviceType() const override { return MDEV_TIMIDITY; }

	double test[3] = { 0, 0, 0 };

protected:
	TimidityPlus::Player *Renderer;

	void HandleEvent(int status, int parm1, int parm2) override;
	void HandleLongEvent(const uint8_t *data, int len) override;
	void ComputeOutput(float *buffer, int len) override;
	void LoadInstruments();
};

//==========================================================================
//
//
//
//==========================================================================

void TimidityPPMIDIDevice::LoadInstruments()
{
	if (timidityConfig.reader)
	{
		timidityConfig.loadedConfig = timidityConfig.readerName;
		timidityConfig.instruments.reset(new TimidityPlus::Instruments());
		bool success = timidityConfig.instruments->load(timidityConfig.reader);
		timidityConfig.reader = nullptr;

		if (!success)
		{
			timidityConfig.instruments.reset();
			timidityConfig.loadedConfig = "";
			throw std::runtime_error("Unable to initialize instruments for Timidity++ MIDI device");
		}
	}
	else if (timidityConfig.instruments == nullptr)
	{
		throw std::runtime_error("No instruments set for Timidity++ device");
	}
	instruments = timidityConfig.instruments;
}

//==========================================================================
//
// TimidityPPMIDIDevice Constructor
//
//==========================================================================

TimidityPPMIDIDevice::TimidityPPMIDIDevice(int samplerate) 
	:SoftSynthMIDIDevice(samplerate, 4000, 65000)
{
	TimidityPlus::set_playback_rate(SampleRate);
	LoadInstruments();
	Renderer = new TimidityPlus::Player(instruments.get());
}

//==========================================================================
//
// TimidityPPMIDIDevice Destructor
//
//==========================================================================

TimidityPPMIDIDevice::~TimidityPPMIDIDevice ()
{
	Close();
	if (Renderer != nullptr)
	{
		delete Renderer;
	}
}

//==========================================================================
//
// TimidityPPMIDIDevice :: Open
//
//==========================================================================

int TimidityPPMIDIDevice::OpenRenderer()
{
	Renderer->playmidi_stream_init();
	return 0;
}

//==========================================================================
//
// TimidityPPMIDIDevice :: PrecacheInstruments
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
//==========================================================================

void TimidityPPMIDIDevice::PrecacheInstruments(const uint16_t *instrumentlist, int count)
{
	if (instruments != nullptr)
		instruments->PrecacheInstruments(instrumentlist, count);
}

//==========================================================================
//
// TimidityPPMIDIDevice :: HandleEvent
//
//==========================================================================

void TimidityPPMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	if (Renderer != nullptr)
		Renderer->send_event(status, parm1, parm2);
}

//==========================================================================
//
// TimidityPPMIDIDevice :: HandleLongEvent
//
//==========================================================================

void TimidityPPMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
	if (Renderer != nullptr)
		Renderer->send_long_event(data, len);
}

//==========================================================================
//
// TimidityPPMIDIDevice :: ComputeOutput
//
//==========================================================================

void TimidityPPMIDIDevice::ComputeOutput(float *buffer, int len)
{
	if (Renderer != nullptr)
		Renderer->compute_data(buffer, len);
}

//==========================================================================
//
//
//
//==========================================================================

bool Timidity_SetupConfig(const char* args)
{
	if (*args == 0) args = timidityConfig.timidity_config.c_str();
	if (stricmp(timidityConfig.loadedConfig.c_str(), args) == 0) return false; // aleady loaded

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
		snprintf(error, 80, "Timidity++: %s: Unable to load sound font\n", args);
		throw std::runtime_error(error);
	}
	timidityConfig.reader = reader;
	timidityConfig.readerName = args;
	return true;
}

MIDIDevice *CreateTimidityPPMIDIDevice(const char *Args, int samplerate)
{
	Timidity_SetupConfig(Args);
	return new TimidityPPMIDIDevice(samplerate);
}

#else
MIDIDevice* CreateTimidityPPMIDIDevice(const char* Args, int samplerate)
{
	throw std::runtime_error("Timidity++ device not supported in this configuration");
}
#endif