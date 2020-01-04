/*
** music_midistream.cpp
** Implements base class for MIDI and MUS streaming.
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

#include <string>
#include <algorithm>
#include <assert.h>
#include "zmusic/zmusic_internal.h"
#include "zmusic/musinfo.h"
#include "mididevices/mididevice.h"
#include "midisources/midisource.h"

#ifdef HAVE_SYSTEM_MIDI
#ifdef __linux__
#include "mididevices/music_alsa_state.h"
#endif
#endif

// MACROS ------------------------------------------------------------------

enum
{
	MAX_TIME	= (1000000/10)	// Send out 1/10 of a sec of events at a time.
};

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Base class for streaming MUS and MIDI files ------------------------------

class MIDIStreamer : public MusInfo
{
public:
	MIDIStreamer(EMidiDevice type, const char* args);
	~MIDIStreamer();

	void MusicVolumeChanged() override;
	void Play(bool looping, int subsong) override;
	void Pause() override;
	void Resume() override;
	void Stop() override;
	bool IsPlaying() override;
	bool IsMIDI() const override;
	bool IsValid() const override;
	bool SetSubsong(int subsong) override;
	void Update() override;
	std::string GetStats() override;
	void ChangeSettingInt(const char* setting, int value) override;
	void ChangeSettingNum(const char* setting, double value) override;
	void ChangeSettingString(const char* setting, const char* value) override;
	int ServiceEvent();
	void SetMIDISource(MIDISource* _source);
	bool ServiceStream(void* buff, int len) override;
	SoundStreamInfo GetStreamInfo() const override;

	int GetDeviceType() const override;

	bool DumpWave(const char* filename, int subsong, int samplerate);


protected:
	MIDIStreamer(const char* dumpname, EMidiDevice type);

	void OutputVolume(uint32_t volume);
	int FillBuffer(int buffer_num, int max_events, uint32_t max_time);
	int FillStopBuffer(int buffer_num);
	uint32_t* WriteStopNotes(uint32_t* events);
	int VolumeControllerChange(int channel, int volume);
	void SetTempo(int new_tempo);
	void Precache();
	void StartPlayback();
	bool InitPlayback();

	//void SetMidiSynth(MIDIDevice *synth);


	static EMidiDevice SelectMIDIDevice(EMidiDevice devtype);
	MIDIDevice* CreateMIDIDevice(EMidiDevice devtype, int samplerate);

	static void Callback(void* userdata);

	enum
	{
		SONG_MORE,
		SONG_DONE,
		SONG_ERROR
	};

	std::unique_ptr<MIDIDevice> MIDI;
	uint32_t Events[2][MAX_MIDI_EVENTS * 3];
	MidiHeader Buffer[2];
	int BufferNum;
	int EndQueued;
	bool VolumeChanged;
	bool Restarting;
	bool InitialPlayback;
	uint32_t NewVolume;
	uint32_t Volume;
	EMidiDevice DeviceType;
	bool CallbackIsThreaded;
	int LoopLimit;
	std::string Args;
	std::unique_ptr<MIDISource> source;
};


// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// MIDIStreamer Constructor
//
//==========================================================================

MIDIStreamer::MIDIStreamer(EMidiDevice type, const char *args)
:
  DeviceType(type), Args(args)
{
	memset(Buffer, 0, sizeof(Buffer));
}

//==========================================================================
//
// MIDIStreamer Destructor
//
//==========================================================================

MIDIStreamer::~MIDIStreamer()
{
	Stop();
}

//==========================================================================
//
// MIDIStreamer :: IsMIDI
//
// You bet it is!
//
//==========================================================================

bool MIDIStreamer::IsMIDI() const
{
	return true;
}

//==========================================================================
//
// MIDIStreamer :: IsValid
//
//==========================================================================

bool MIDIStreamer::IsValid() const
{
	return source != nullptr && source->isValid();
}


//==========================================================================
//
// MIDIStreamer :: SelectMIDIDevice									static
//
// Select the MIDI device to play on
//
//==========================================================================

EMidiDevice MIDIStreamer::SelectMIDIDevice(EMidiDevice device)
{
	/* MIDI are played as:
		- OPL: 
			- if explicitly selected by $mididevice 
			- when snd_mididevice  is -3 and no midi device is set for the song

		- Timidity: 
			- if explicitly selected by $mididevice 
			- when snd_mididevice  is -2 and no midi device is set for the song

		- MMAPI (Win32 only):
			- if explicitly selected by $mididevice (non-Win32 redirects this to Sound System)
			- when snd_mididevice  is >= 0 and no midi device is set for the song
			- as fallback when both OPL and Timidity failed and snd_mididevice is >= 0
	*/

	// Choose the type of MIDI device we want.
	if (device != MDEV_DEFAULT)
	{
		return device;
	}
	switch (miscConfig.snd_mididevice)
	{
	case -1:		return MDEV_SNDSYS;
	case -2:		return MDEV_TIMIDITY;
	case -3:		return MDEV_OPL;
	case -4:		return MDEV_GUS;
	case -5:		return MDEV_FLUIDSYNTH;
	case -6:		return MDEV_WILDMIDI;
	case -7:		return MDEV_ADL;
	case -8:		return MDEV_OPN;
	default:
		#ifdef HAVE_SYSTEM_MIDI
					return MDEV_STANDARD;
		#else
					return MDEV_SNDSYS;
		#endif
	}
}

//==========================================================================
//
// MIDIStreamer :: CreateMIDIDevice
//
//==========================================================================

static EMidiDevice lastRequestedDevice, lastSelectedDevice;

MIDIDevice *MIDIStreamer::CreateMIDIDevice(EMidiDevice devtype, int samplerate)
{
	bool checked[MDEV_COUNT] = { false };

	MIDIDevice *dev = nullptr;
	if (devtype == MDEV_SNDSYS) devtype = MDEV_FLUIDSYNTH;
	EMidiDevice requestedDevice = devtype, selectedDevice;
	while (dev == nullptr)
	{
		selectedDevice = devtype;
		try
		{
			switch (devtype)
			{
			case MDEV_GUS:
				dev = CreateTimidityMIDIDevice(Args.c_str(), samplerate);
				break;

			case MDEV_ADL:
				dev = CreateADLMIDIDevice(Args.c_str());
				break;

			case MDEV_OPN:
				dev = CreateOPNMIDIDevice(Args.c_str());
				break;

			case MDEV_STANDARD:

#ifdef HAVE_SYSTEM_MIDI
#ifdef _WIN32
				dev = CreateWinMIDIDevice(std::max(0, miscConfig.snd_mididevice));
#elif __linux__
                dev = CreateAlsaMIDIDevice(std::max(0, miscConfig.snd_mididevice));
#endif
				break;
#endif
				// Intentional fall-through for systems without standard midi support

			case MDEV_FLUIDSYNTH:
				dev = CreateFluidSynthMIDIDevice(samplerate, Args.c_str());
				break;

			case MDEV_OPL:
				dev = CreateOplMIDIDevice(Args.c_str());
				break;

			case MDEV_TIMIDITY:
				dev = CreateTimidityPPMIDIDevice(Args.c_str(), samplerate);
				break;

			case MDEV_WILDMIDI:
				dev = CreateWildMIDIDevice(Args.c_str(), samplerate);
				break;

			default:
				break;
			}
		}
		catch (std::runtime_error &err)
		{
			//DPrintf(DMSG_WARNING, "%s\n", err.what());
			checked[devtype] = true;
			devtype = MDEV_DEFAULT;
			// Opening the requested device did not work out so choose another one.
			if (!checked[MDEV_FLUIDSYNTH]) devtype = MDEV_FLUIDSYNTH;
			else if (!checked[MDEV_TIMIDITY]) devtype = MDEV_TIMIDITY;
			else if (!checked[MDEV_WILDMIDI]) devtype = MDEV_WILDMIDI;
			else if (!checked[MDEV_GUS]) devtype = MDEV_GUS;
#ifdef HAVE_SYSTEM_MIDI
			else if (!checked[MDEV_STANDARD]) devtype = MDEV_STANDARD;
#endif
			else if (!checked[MDEV_ADL]) devtype = MDEV_ADL;
			else if (!checked[MDEV_OPN]) devtype = MDEV_OPN;
			else if (!checked[MDEV_OPL]) devtype = MDEV_OPL;

			if (devtype == MDEV_DEFAULT)
			{
				std::string message = std::string(err.what()) + "\n\nFailed to play music: Unable to open any MIDI Device.";
				throw std::runtime_error(message);
			}
		}
	}
	if (selectedDevice != requestedDevice && (selectedDevice != lastSelectedDevice || requestedDevice != lastRequestedDevice))
	{
		static const char *devnames[] = {
			"System Default",
			"OPL",
			"",
			"Timidity++",
			"FluidSynth",
			"GUS",
			"WildMidi",
			"ADL",
			"OPN",
		};

		lastRequestedDevice = requestedDevice;
		lastSelectedDevice = selectedDevice;
		if (musicCallbacks.Fluid_MessageFunc)
			musicCallbacks.Fluid_MessageFunc("Unable to create %s MIDI device. Falling back to %s\n", devnames[requestedDevice], devnames[selectedDevice]);
	}
	return dev;
}

//==========================================================================
//
// MIDIStreamer :: Play
//
//==========================================================================

void MIDIStreamer::Play(bool looping, int subsong)
{
	EMidiDevice devtype;

	if (source == nullptr) return;	// We have nothing to play so abort.

	assert(MIDI == NULL);
	m_Looping = looping;
	source->SetMIDISubsong(subsong);
	devtype = SelectMIDIDevice(DeviceType);
	MIDI.reset(CreateMIDIDevice(devtype, miscConfig.snd_outputrate));
	InitPlayback();
}

//==========================================================================
//
// MIDIStreamer :: DumpWave
//
//==========================================================================

bool MIDIStreamer::DumpWave(const char *filename, int subsong, int samplerate)
{
	m_Looping = false;
	if (source == nullptr) return false;	// We have nothing to play so abort.
	source->SetMIDISubsong(subsong);

	assert(MIDI == NULL);
	auto devtype = SelectMIDIDevice(DeviceType);
	if (devtype == MDEV_STANDARD)
	{
		throw std::runtime_error("System MIDI device is not supported");
	}
	auto iMIDI = CreateMIDIDevice(devtype, samplerate);
	auto writer = new MIDIWaveWriter(filename, static_cast<SoftSynthMIDIDevice*>(iMIDI));
	MIDI.reset(writer);
	bool res = InitPlayback();
	if (!writer->CloseFile())
	{
		char buffer[80];
		snprintf(buffer, 80, "Could not finish writing wave file: %s\n", strerror(errno));
		throw std::runtime_error(buffer);
	}
	return res;
}

//==========================================================================
//
// MIDIStreamer :: InitPlayback
//
//==========================================================================

bool MIDIStreamer::InitPlayback()
{
	m_Status = STATE_Stopped;
	EndQueued = 0;
	VolumeChanged = false;
	Restarting = true;
	InitialPlayback = true;
	if (MIDI) MIDI->SetCallback(Callback, this);

	if (MIDI == NULL || 0 != MIDI->Open())
	{
		throw std::runtime_error("Could not open MIDI out device");
	}

	source->CheckCaps(MIDI->GetTechnology());
	if (!MIDI->CanHandleSysex()) source->SkipSysex();

	StartPlayback();
	if (MIDI == nullptr)
	{ // The MIDI file had no content and has been automatically closed.
		return false;
	}

	int res = MIDI->Resume();

	if (res)
	{
		throw std::runtime_error("Starting MIDI playback failed");
	}
	else
	{
		m_Status = STATE_Playing;
		return true;
	}
}

SoundStreamInfo MIDIStreamer::GetStreamInfo() const
{
	if (MIDI) return MIDI->GetStreamInfo();
	else return { 0, 0, 0 };
}

//==========================================================================
//
// MIDIStreamer :: StartPlayback
//
//==========================================================================

void MIDIStreamer::StartPlayback()
{
	auto data = source->PrecacheData();
	MIDI->PrecacheInstruments(data.data(), (int)data.size());
	source->StartPlayback(m_Looping);
	
	// Set time division and tempo.
	if (0 != MIDI->SetTimeDiv(source->getDivision()) ||
		0 != MIDI->SetTempo(source->getInitialTempo()))
	{
		throw std::runtime_error("Setting MIDI stream speed failed");
	}

	MusicVolumeChanged();	// set volume to current music's properties
	OutputVolume(Volume);

	MIDI->InitPlayback();

	// Fill the initial buffers for the song.
	BufferNum = 0;
	do
	{
		int res = FillBuffer(BufferNum, MAX_MIDI_EVENTS, MAX_TIME);
		if (res == SONG_MORE)
		{
			if (0 != MIDI->StreamOutSync(&Buffer[BufferNum]))
			{
				throw std::runtime_error("Initial midiStreamOut failed");
			}
			BufferNum ^= 1;
		}
		else if (res == SONG_DONE)
		{
			// Do not play super short songs that can't fill the initial two buffers.
			Stop();
			return;
		}
		else
		{
			Stop();
			return;
		}
	}
	while (BufferNum != 0);
}

//==========================================================================
//
// MIDIStreamer :: Pause
//
// "Pauses" the song by setting it to zero volume and filling subsequent
// buffers with NOPs until the song is unpaused. A MIDI device that
// supports real pauses will return true from its Pause() method.
//
//==========================================================================

void MIDIStreamer::Pause()
{
	if (m_Status == STATE_Playing)
	{
		m_Status = STATE_Paused;
		if (!MIDI->Pause(true))
		{
			OutputVolume(0);
		}
	}
}

//==========================================================================
//
// MIDIStreamer :: Resume
//
// "Unpauses" a song by restoring the volume and letting subsequent
// buffers store real MIDI events again.
//
//==========================================================================

void MIDIStreamer::Resume()
{
	if (m_Status == STATE_Paused)
	{
		if (!MIDI->Pause(false))
		{
			OutputVolume(Volume);
		}
		m_Status = STATE_Playing;
	}
}

//==========================================================================
//
// MIDIStreamer :: Stop
//
// Stops playback and closes the player thread and MIDI device.
//
//==========================================================================

void MIDIStreamer::Stop()
{
	EndQueued = 4;

	if (MIDI != NULL && MIDI->IsOpen())
	{
		MIDI->Stop();
		MIDI->UnprepareHeader(&Buffer[0]);
		MIDI->UnprepareHeader(&Buffer[1]);
		MIDI->Close();
	}
	if (MIDI != nullptr)
	{
		MIDI.reset();
	}
	m_Status = STATE_Stopped;
}

//==========================================================================
//
// MIDIStreamer :: IsPlaying
//
//==========================================================================

bool MIDIStreamer::IsPlaying()
{
	if (m_Status != STATE_Stopped && (MIDI == NULL || (EndQueued != 0 && EndQueued < 4)))
	{
		std::lock_guard<std::mutex> lock(CritSec);
		Stop();
	}
	if (m_Status != STATE_Stopped && !MIDI->IsOpen())
	{
		std::lock_guard<std::mutex> lock(CritSec);
		Stop();
	}
	return m_Status != STATE_Stopped;
}

//==========================================================================
//
// MIDIStreamer :: MusicVolumeChanged
//
// WinMM MIDI doesn't go through the sound system, so the normal volume
// changing procedure doesn't work for it.
//
//==========================================================================

void MIDIStreamer::MusicVolumeChanged()
{
	if (MIDI != NULL && MIDI->FakeVolume())
	{
		float realvolume = miscConfig.snd_musicvolume * miscConfig.relative_volume * miscConfig.snd_mastervolume;
		if (realvolume < 0 || realvolume > 1) realvolume = 1;
		Volume = (uint32_t)(realvolume * 65535.f);
	}
	else
	{
		Volume = 0xFFFF;
	}
	source->setVolume(Volume);
	if (m_Status == STATE_Playing)
	{
		OutputVolume(Volume);
	}
}

//==========================================================================
//
// MIDIStreamer :: ChangeSettingInt
//
//==========================================================================

void MIDIStreamer::ChangeSettingInt(const char *setting, int value)
{
	if (MIDI != NULL)
	{
		MIDI->ChangeSettingInt(setting, value);
	}
}

//==========================================================================
//
// MIDIStreamer :: ChangeSettingNum
//
//==========================================================================

void MIDIStreamer::ChangeSettingNum(const char *setting, double value)
{
	if (MIDI != NULL)
	{
		MIDI->ChangeSettingNum(setting, value);
	}
}

//==========================================================================
//
// MIDIDeviceStreamer :: ChangeSettingString
//
//==========================================================================

void MIDIStreamer::ChangeSettingString(const char *setting, const char *value)
{
	if (MIDI != NULL)
	{
		MIDI->ChangeSettingString(setting, value);
	}
}


//==========================================================================
//
// MIDIStreamer :: OutputVolume
//
// Signals the buffer filler to send volume change events on all channels.
//
//==========================================================================

void MIDIStreamer::OutputVolume (uint32_t volume)
{
	if (MIDI != NULL && MIDI->FakeVolume())
	{
		NewVolume = volume;
		VolumeChanged = true;
	}
}

//==========================================================================
//
// MIDIStreamer :: Callback											Static
//
//==========================================================================

void MIDIStreamer::Callback(void *userdata)
{
	MIDIStreamer *self = (MIDIStreamer *)userdata;

	if (self->EndQueued >= 4)
	{
		return;
	}
	self->ServiceEvent();
}

//==========================================================================
//
// MIDIStreamer :: Update
//
// Called periodically to see if the player thread is still alive. If it
// isn't, stop playback now.
//
//==========================================================================

void MIDIStreamer::Update()
{
	if (MIDI != nullptr && !MIDI->Update())
	{
		std::lock_guard<std::mutex> lock(CritSec);
		Stop();
	}
}

//==========================================================================
//
// MIDIStreamer :: ServiceEvent
//
// Fills the buffer that just finished playing with new events and appends
// it to the MIDI stream queue. Stops the song if playback is over. Returns
// non-zero if a problem occured and playback should stop.
//
//==========================================================================

int MIDIStreamer::ServiceEvent()
{
	int res;

	if (EndQueued == 2)
	{
		return 0;
	}
	if (0 != (res = MIDI->UnprepareHeader(&Buffer[BufferNum])))
	{
		return res;
	}
fill:
	if (EndQueued == 1)
	{
		res = FillStopBuffer(BufferNum);
		if ((res & 3) != SONG_ERROR)
		{
			EndQueued = 2;
		}
	}
	else
	{
		res = FillBuffer(BufferNum, MAX_MIDI_EVENTS, MAX_TIME);
	}
	switch (res & 3)
	{
	case SONG_MORE:
		res = MIDI->StreamOut(&Buffer[BufferNum]);
		if (res != 0)
		{
			return res;
		}
		else
		{
			BufferNum ^= 1;
		}
		break;

	case SONG_DONE:
		if (m_Looping)
		{
			Restarting = true;
			goto fill;
		}
		EndQueued = 1;
		break;

	default:
		return res >> 2;
	}
	return 0;
}

//==========================================================================
//
// MIDIStreamer :: FillBuffer
//
// Copies MIDI events from the MIDI file and puts them into a MIDI stream
// buffer. Filling the buffer stops when the song end is encountered, the
// buffer space is used up, or the maximum time for a buffer is hit.
//
// Can return:
// - SONG_MORE if the buffer was prepared with data.
// - SONG_DONE if the song's end was reached.
//             The buffer will never have data in this case.
// - SONG_ERROR if there was a problem preparing the buffer.
//
//==========================================================================

int MIDIStreamer::FillBuffer(int buffer_num, int max_events, uint32_t max_time)
{
	if (!Restarting && source->CheckDone())
	{
		return SONG_DONE;
	}

	int i;
	uint32_t *events = Events[buffer_num], *max_event_p;


	// The final event is for a NOP to hold the delay from the last event.
	max_event_p = events + (max_events - 1) * 3;

	if (InitialPlayback)
	{
		InitialPlayback = false;
		// Send the GS System Reset SysEx message.
		events[0] = 0;								// dwDeltaTime
		events[1] = 0;								// dwStreamID
		events[2] = (MEVENT_LONGMSG << 24) | 6;		// dwEvent
		events[3] = MAKE_ID(0xf0, 0x7e, 0x7f, 0x09);	// dwParms[0]
		events[4] = MAKE_ID(0x01, 0xf7, 0x00, 0x00);	// dwParms[1]
		events += 5;

		// Send the full master volume SysEx message.
		events[0] = 0;								// dwDeltaTime
		events[1] = 0;								// dwStreamID
		events[2] = (MEVENT_LONGMSG << 24) | 8;		// dwEvent
		events[3] = MAKE_ID(0xf0,0x7f,0x7f,0x04);	// dwParms[0]
		events[4] = MAKE_ID(0x01,0x7f,0x7f,0xf7);	// dwParms[1]
		events += 5;
		source->DoInitialSetup();
	}

	// If the volume has changed, stick those events at the start of this buffer.
	if (VolumeChanged && (m_Status != STATE_Paused || NewVolume == 0))
	{
		VolumeChanged = false;
		for (i = 0; i < 16; ++i)
		{
			uint8_t courseVol = (uint8_t)(((source->getChannelVolume(i)+1) * NewVolume) >> 16);
			events[0] = 0;				// dwDeltaTime
			events[1] = 0;				// dwStreamID
			events[2] = MIDI_CTRLCHANGE | i | (7<<8) | (courseVol<<16);
			events += 3;
		}
	}

	// Play nothing while paused.
	if (m_Status == STATE_Paused)
	{
		// Be more responsive when unpausing by only playing each buffer
		// for a third of the maximum time.
		events[0] = std::max<uint32_t>(1, (max_time / 3) * source->getDivision() / source->getTempo());
		events[1] = 0;
		events[2] = MEVENT_NOP << 24;
		events += 3;
	}
	else
	{
		if (Restarting)
		{
			Restarting = false;
			// Reset the tempo to the inital value.
			events[0] = 0;									// dwDeltaTime
			events[1] = 0;									// dwStreamID
			events[2] = (MEVENT_TEMPO << 24) | source->getInitialTempo();	// dwEvent
			events += 3;
			// Stop all notes in case any were left hanging.
			events = WriteStopNotes(events);
			source->DoRestart();
		}
		events = source->MakeEvents(events, max_event_p, max_time);
	}
	memset(&Buffer[buffer_num], 0, sizeof(MidiHeader));
	Buffer[buffer_num].lpData = (uint8_t *)Events[buffer_num];
	Buffer[buffer_num].dwBufferLength = uint32_t((uint8_t *)events - Buffer[buffer_num].lpData);
	Buffer[buffer_num].dwBytesRecorded = Buffer[buffer_num].dwBufferLength;
	if (0 != (i = MIDI->PrepareHeader(&Buffer[buffer_num])))
	{
		return SONG_ERROR | (i << 2);
	}
	return SONG_MORE;
}

//==========================================================================
//
// MIDIStreamer :: FillStopBuffer
//
// Fills a MIDI buffer with events to stop all channels.
//
//==========================================================================

int MIDIStreamer::FillStopBuffer(int buffer_num)
{
	uint32_t *events = Events[buffer_num];
	int i;

	events = WriteStopNotes(events);

	// wait some tics, just so that this buffer takes some time
	events[0] = 500;
	events[1] = 0;
	events[2] = MEVENT_NOP << 24;
	events += 3;

	memset(&Buffer[buffer_num], 0, sizeof(MidiHeader));
	Buffer[buffer_num].lpData = (uint8_t*)Events[buffer_num];
	Buffer[buffer_num].dwBufferLength = uint32_t((uint8_t*)events - Buffer[buffer_num].lpData);
	Buffer[buffer_num].dwBytesRecorded = Buffer[buffer_num].dwBufferLength;
	if (0 != (i = MIDI->PrepareHeader(&Buffer[buffer_num])))
	{
		return SONG_ERROR | (i << 2);
	}
	return SONG_MORE;
}

//==========================================================================
//
// MIDIStreamer :: WriteStopNotes
//
// Generates MIDI events to stop all notes and reset controllers on
// every channel.
//
//==========================================================================

uint32_t *MIDIStreamer::WriteStopNotes(uint32_t *events)
{
	for (int i = 0; i < 16; ++i)
	{
		events[0] = 0;				// dwDeltaTime
		events[1] = 0;				// dwStreamID
		events[2] = MIDI_CTRLCHANGE | i | (123 << 8);	// All notes off
		events[3] = 0;
		events[4] = 0;
		events[5] = MIDI_CTRLCHANGE | i | (121 << 8);	// Reset controllers
		events += 6;
	}
	return events;
}

//==========================================================================
//
//
//
//==========================================================================

void MIDIStreamer::SetMIDISource(MIDISource *_source)
{
	source.reset(_source);
	source->setTempoCallback([=](int tempo) { return !!MIDI->SetTempo(tempo); } );
}

int MIDIStreamer::GetDeviceType() const 
{
	return nullptr == MIDI
		? MusInfo::GetDeviceType()
		: MIDI->GetDeviceType();
}

//==========================================================================
//
// MIDIStreamer :: GetStats
//
//==========================================================================

std::string MIDIStreamer::GetStats()
{
	if (MIDI == NULL)
	{
		return "No MIDI device in use.";
	}
	return MIDI->GetStats();
}

//==========================================================================
//
// MIDIStreamer :: SetSubsong
//
// Selects which subsong to play in an already-playing file. This is public.
//
//==========================================================================

bool MIDIStreamer::SetSubsong(int subsong)
{
	if (source->SetMIDISubsong(subsong))
	{
		Stop();
		Play(m_Looping, subsong);
		return true;
	}
	return false;
}

//==========================================================================
//
// MIDIStreamer :: FillStream
//
//==========================================================================

bool MIDIStreamer::ServiceStream(void* buff, int len)
{
	if (!MIDI) return false;
	return static_cast<SoftSynthMIDIDevice*>(MIDI.get())->ServiceStream(buff, len);
}

//==========================================================================
//
// create a streamer
//
//==========================================================================

MusInfo* CreateMIDIStreamer(MIDISource *source, EMidiDevice devtype, const char* args)
{
	auto me = new MIDIStreamer(devtype, args);
	me->SetMIDISource(source);
	return me;
}

DLL_EXPORT bool ZMusic_MIDIDumpWave(ZMusic_MidiSource source, EMidiDevice devtype, const char *devarg, const char *outname, int subsong, int samplerate)
{
	try
	{
		MIDIStreamer me(devtype, devarg);
		me.SetMIDISource(source);
		me.DumpWave(outname, subsong, samplerate);
		return true;
	}
	catch (const std::exception & ex)
	{
		SetError(ex.what());
		return false;
	}
}
