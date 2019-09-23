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

#include "i_musicinterns.h"
#include "v_text.h"
#include "doomerrors.h"
#include "i_soundfont.h"

#include "timiditypp/timidity.h"
#include "timiditypp/instrum.h"
#include "timiditypp/playmidi.h"



template<class T> void ChangeVarSync(T& var, T value)
{
	std::lock_guard<std::mutex> lock(TimidityPlus::CvarCritSec);
	var = value;
}

CUSTOM_CVAR(Bool, timidity_modulation_wheel, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_modulation_wheel, *self);
}

CUSTOM_CVAR(Bool, timidity_portamento, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_portamento, *self);
}
/*
* reverb=0     no reverb                 0
* reverb=1     old reverb                1
* reverb=1,n   set reverb level to n   (-1 to -127)
* reverb=2     "global" old reverb       2
* reverb=2,n   set reverb level to n   (-1 to -127) - 128
* reverb=3     new reverb                3
* reverb=3,n   set reverb level to n   (-1 to -127) - 256
* reverb=4     "global" new reverb       4
* reverb=4,n   set reverb level to n   (-1 to -127) - 384
*/
EXTERN_CVAR(Int, timidity_reverb_level)
EXTERN_CVAR(Int, timidity_reverb)

static void SetReverb()
{
	int value = 0;
	int mode = timidity_reverb;
	int level = timidity_reverb_level;

	if (mode == 0 || level == 0) value = mode;
	else value = (mode - 1) * -128 - level;
	ChangeVarSync(TimidityPlus::timidity_reverb, value);
}

CUSTOM_CVAR(Int, timidity_reverb, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 4) self = 0;
	else SetReverb();
}

CUSTOM_CVAR(Int, timidity_reverb_level, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 127) self = 0;
	else SetReverb();
}

CUSTOM_CVAR(Int, timidity_chorus, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_chorus, *self);
}

CUSTOM_CVAR(Bool, timidity_surround_chorus, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_TIMIDITY)
	{
		MIDIDeviceChanged(-1, true);
	}
	ChangeVarSync(TimidityPlus::timidity_surround_chorus, *self);
}

CUSTOM_CVAR(Bool, timidity_channel_pressure, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_channel_pressure, *self);
}

CUSTOM_CVAR(Int, timidity_lpf_def, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_lpf_def, *self);
}

CUSTOM_CVAR(Bool, timidity_temper_control, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_temper_control, *self);
}

CUSTOM_CVAR(Bool, timidity_modulation_envelope, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_modulation_envelope, *self);
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_TIMIDITY)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CUSTOM_CVAR(Bool, timidity_overlap_voice_allow, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_overlap_voice_allow, *self);
}

CUSTOM_CVAR(Bool, timidity_drum_effect, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_drum_effect, *self);
}

CUSTOM_CVAR(Bool, timidity_pan_delay, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	ChangeVarSync(TimidityPlus::timidity_pan_delay, *self);
}

CUSTOM_CVAR(Float, timidity_drum_power, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) /* coef. of drum amplitude */
{
	if (self < 0) self = 0;
	else if (self > MAX_AMPLIFICATION / 100.f) self = MAX_AMPLIFICATION / 100.f;
	ChangeVarSync(TimidityPlus::timidity_drum_power, *self);
}
CUSTOM_CVAR(Int, timidity_key_adjust, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < -24) self = -24;
	else if (self > 24) self = 24;
	ChangeVarSync(TimidityPlus::timidity_key_adjust, *self);
}
// For testing mainly.
CUSTOM_CVAR(Float, timidity_tempo_adjust, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.25) self = 0.25;
	else if (self > 10) self = 10;
	ChangeVarSync(TimidityPlus::timidity_tempo_adjust, *self);
}

CUSTOM_CVAR(Float, min_sustain_time, 5000, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0) self = 0;
	ChangeVarSync(TimidityPlus::min_sustain_time, *self);
}


class TimidityPPMIDIDevice : public SoftSynthMIDIDevice
{
	static TimidityPlus::Instruments *instruments;
	static FString configName;
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
FString TimidityPPMIDIDevice::configName;

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
	if (instruments != nullptr && configName.CompareNoCase(args))	// Only load instruments if they have changed from the last played song.
	{
		delete instruments;
		instruments = nullptr;
	}
	TimidityPlus::set_playback_rate(SampleRate);

	if (instruments == nullptr)
	{

		auto sfreader = sfmanager.OpenSoundFont(args, SF_SF2 | SF_GUS);
		if (sfreader != nullptr)
		{
			instruments = new TimidityPlus::Instruments;
			if (!instruments->load(sfreader))
			{
				delete instruments;
				instruments = nullptr;
			}
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