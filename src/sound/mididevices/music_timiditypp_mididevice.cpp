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

#include <string>
#include <vector>

#include "i_musicinterns.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "templates.h"
#include "version.h"
#include "m_misc.h"
#include "v_text.h"
#include "i_system.h"

#include "timiditypp/timidity.h"
#include "timiditypp/instrum.h"
#include "timiditypp/playmidi.h"

class TimidityPPMIDIDevice : public SoftSynthMIDIDevice
{
	static TimidityPlus::Instruments *instruments;
	int sampletime;
public:
	TimidityPPMIDIDevice(const char *args, int samplerate);
	~TimidityPPMIDIDevice();

	int Open(MidiCallback, void *userdata);
	void PrecacheInstruments(const uint16_t *instruments, int count);
	//FString GetStats();
	int GetDeviceType() const override { return MDEV_TIMIDITY; }
	static void ClearInstruments()
	{
		if (instruments != nullptr) delete instruments;
		instruments = nullptr;
	}

	double test[3] = { 0, 0, 0 };

protected:
	TimidityPlus::Player *Renderer;

	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
};
TimidityPlus::Instruments *TimidityPPMIDIDevice::instruments;

// Config file to use
CUSTOM_CVAR(String, timidity_config, "gzdoom", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_TIMIDITY)
	{
		MIDIDeviceChanged(-1, true);
	}
}


CVAR (Int, timidity_frequency, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

//==========================================================================
//
// TimidityPPMIDIDevice Constructor
//
//==========================================================================

TimidityPPMIDIDevice::TimidityPPMIDIDevice(const char *args, int samplerate) 
	:SoftSynthMIDIDevice(samplerate <= 0? timidity_frequency : samplerate, 4000, 65000)
{
	if (args == NULL || *args == 0) args = timidity_config;

	Renderer = nullptr;
	if (instruments != nullptr && !instruments->checkConfig(args))
	{
		delete instruments;
		instruments = nullptr;
	}
	TimidityPlus::set_playback_rate(SampleRate);

	if (instruments == nullptr)
	{
		instruments = new TimidityPlus::Instruments;
		if (!instruments->load(args))
		{
			delete instruments;
			instruments = nullptr;
		}
	}
	if (instruments != nullptr)
	{
		Renderer = new TimidityPlus::Player(instruments);
	}
	else
	{
		I_Error("Failed to load any MIDI patches");
	}
	sampletime = 0;
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

int TimidityPPMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	int ret = OpenStream(2, 0, callback, userdata);
	if (ret == 0 && Renderer != nullptr)
	{
		Renderer->playmidi_stream_init();
	}
	// No instruments loaded means we cannot play...
	if (instruments == nullptr) return 0;
	return ret;
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

MIDIDevice *CreateTimidityPPMIDIDevice(const char *args, int samplerate)
{
	return new TimidityPPMIDIDevice(args, samplerate);
}

void TimidityPP_Shutdown()
{
	TimidityPPMIDIDevice::ClearInstruments();
	TimidityPlus::free_gauss_table();
	TimidityPlus::free_global_mblock();
}


void TimidityPlus::ctl_cmsg(int type, int verbosity_level, const char *fmt, ...)
{
	if (verbosity_level >= VERB_DEBUG) return;	// Don't waste time on diagnostics.

	va_list args;
	va_start(args, fmt);
	FString msg;
	msg.VFormat(fmt, args);
	va_end(args);

	switch (type)
	{
	case CMSG_ERROR:
		Printf(TEXTCOLOR_RED "%s\n", msg.GetChars());
		break;

	case CMSG_WARNING:
		Printf(TEXTCOLOR_YELLOW "%s\n", msg.GetChars());
		break;

	case CMSG_INFO:
		DPrintf(DMSG_SPAMMY, "%s\n", msg.GetChars());
		break;
	}
}