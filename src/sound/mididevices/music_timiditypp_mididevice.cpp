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

#include "i_midi_win32.h"

#include <string>
#include <vector>

#include "i_musicinterns.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "templates.h"
#include "version.h"
#include "tmpfileplus.h"
#include "m_misc.h"

class TimidityPPMIDIDevice : public SoftSynthMIDIDevice
{
	TArray<uint8_t> midi;
	FString CurrentConfig;
public:
	TimidityPPMIDIDevice(const char *args);
	~TimidityPPMIDIDevice();

	int Open(MidiCallback, void *userdata);
	void PrecacheInstruments(const uint16_t *instruments, int count);
	//FString GetStats();
	int GetDeviceType() const override { return MDEV_TIMIDITY; }
	bool Preprocess(MIDIStreamer *song, bool looping);
	void TimidityVolumeChanged();

protected:
	//TimidityPlus::Player *Renderer;

	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
};

// Config file to use
CVAR(String, timidity_config, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// added because Timidity's output is rather loud.
CUSTOM_CVAR (Float, timidity_mastervolume, 1.0f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 4.f)
		self = 4.f;
	if (currSong != NULL)
		currSong->TimidityVolumeChanged();
}


CUSTOM_CVAR (Int, timidity_frequency, 44100, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // Clamp frequency to Timidity's limits
	if (self < 4000)
		self = 4000;
	else if (self > 65000)
		self = 65000;
}

//==========================================================================
//
// TimidityPPMIDIDevice Constructor
//
//==========================================================================

TimidityPPMIDIDevice::TimidityPPMIDIDevice(const char *args) 
{
	if (args == NULL || *args == 0) args = timidity_config;

	if (CurrentConfig.CompareNoCase(args) != 0)
	{
		// reload instruments
		// if (!reload_inst()) CurrentConfig = "";
	}
	if (CurrentConfig.IsNotEmpty())
	{
		//Renderer = new TimidityPlus::Player(timidity_frequency, args);
	}
}

//==========================================================================
//
// TimidityPPMIDIDevice Destructor
//
//==========================================================================

TimidityPPMIDIDevice::~TimidityPPMIDIDevice ()
{
	Close();
	/*
	if (Renderer != nullptr)
	{
		delete Renderer;
	}
	*/

}

//==========================================================================
//
// TimidityPPMIDIDevice :: Preprocess
//
//==========================================================================

bool TimidityPPMIDIDevice::Preprocess(MIDIStreamer *song, bool looping)
{
	// Write MIDI song to temporary file
	song->CreateSMF(midi, looping ? 0 : 1);
	return midi.Size() > 0;
}

//==========================================================================
//
// TimidityPPMIDIDevice :: Open
//
//==========================================================================
namespace TimidityPlus
{
	int load_midi_file(FileReader *fr);
	void run_midi(int samples);
	void timidity_close();
}

int TimidityPPMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	int ret = OpenStream(2, 0, callback, userdata);
	if (ret == 0)
	{
		//Renderer->Reset();
		MemoryReader fr((char*)&midi[0], midi.Size());
		return !TimidityPlus::load_midi_file(&fr);
	}
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

void TimidityPPMIDIDevice::PrecacheInstruments(const uint16_t *instruments, int count)
{
	for (int i = 0; i < count; ++i)
	{
		//Renderer->MarkInstrument((instruments[i] >> 7) & 127, instruments[i] >> 14, instruments[i] & 127);
	}
	//Renderer->load_missing_instruments();
}

//==========================================================================
//
// TimidityPPMIDIDevice :: HandleEvent
//
//==========================================================================

void TimidityPPMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	//Renderer->HandleEvent(status, parm1, parm2);
}

//==========================================================================
//
// TimidityPPMIDIDevice :: HandleLongEvent
//
//==========================================================================

void TimidityPPMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
	//Renderer->HandleLongMessage(data, len);
}

//==========================================================================
//
// TimidityPPMIDIDevice :: ComputeOutput
//
//==========================================================================

void TimidityPPMIDIDevice::ComputeOutput(float *buffer, int len)
{
	TimidityPlus::run_midi(len / 8); // bytes to samples
	memset(buffer, len, 0);	// to do

	//Renderer->ComputeOutput(buffer, len);
}

//==========================================================================
//
// TimidityPPMIDIDevice :: TimidityVolumeChanged
//
//==========================================================================

void TimidityPPMIDIDevice::TimidityVolumeChanged()
{
	if (Stream != NULL)
	{
		Stream->SetVolume(timidity_mastervolume);
	}
}


MIDIDevice *CreateTimidityPPMIDIDevice(const char *args)
{
	return new TimidityPPMIDIDevice(args);
}
