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


#include "i_musicinterns.h"
#include "templates.h"
#include "doomerrors.h"
#include "v_text.h"

// MACROS ------------------------------------------------------------------

#define MAX_TIME	(1000000/10)	// Send out 1/10 of a sec of events at a time.


// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void WriteVarLen (TArray<uint8_t> &file, uint32_t value);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR(Float, snd_musicvolume)
EXTERN_CVAR(Int, snd_mididevice)

#ifdef _WIN32
extern unsigned mididevice;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// MIDIStreamer Constructor
//
//==========================================================================

MIDIStreamer::MIDIStreamer(EMidiDevice type, const char *args)
:
  MIDI(0), DeviceType(type), Args(args)
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
	if (MIDI != NULL)
	{
		delete MIDI;
	}
	if (source != nullptr)
	{
		delete source;
	}
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
	switch (snd_mididevice)
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
		#ifdef _WIN32
					return MDEV_MMAPI;
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
				dev = new TimidityMIDIDevice(Args, samplerate);
				break;

			case MDEV_ADL:
				dev = new ADLMIDIDevice(Args);
				break;

			case MDEV_OPN:
				dev = new OPNMIDIDevice(Args);
				break;

			case MDEV_MMAPI:

#ifdef _WIN32
				dev = CreateWinMIDIDevice(mididevice);
				break;
#endif
				// Intentional fall-through for non-Windows systems.

			case MDEV_FLUIDSYNTH:
				dev = new FluidSynthMIDIDevice(Args, samplerate);
				break;

			case MDEV_OPL:
				dev = new OPLMIDIDevice(Args);
				break;

			case MDEV_TIMIDITY:
				dev = CreateTimidityPPMIDIDevice(Args, samplerate);
				break;

			case MDEV_WILDMIDI:
				dev = new WildMIDIDevice(Args, samplerate);
				break;

			default:
				break;
			}
		}
		catch (CRecoverableError &err)
		{
			DPrintf(DMSG_WARNING, "%s\n", err.GetMessage());
			checked[devtype] = true;
			devtype = MDEV_DEFAULT;
			// Opening the requested device did not work out so choose another one.
			if (!checked[MDEV_FLUIDSYNTH]) devtype = MDEV_FLUIDSYNTH;
			else if (!checked[MDEV_TIMIDITY]) devtype = MDEV_TIMIDITY;
			else if (!checked[MDEV_WILDMIDI]) devtype = MDEV_WILDMIDI;
			else if (!checked[MDEV_GUS]) devtype = MDEV_GUS;
#ifdef _WIN32
			else if (!checked[MDEV_MMAPI]) devtype = MDEV_MMAPI;
#endif
			else if (!checked[MDEV_OPL]) devtype = MDEV_OPL;

			if (devtype == MDEV_DEFAULT)
			{
				Printf("Failed to play music: Unable to open any MIDI Device.\n");
				return nullptr;
			}
		}
	}
	if (selectedDevice != requestedDevice && (selectedDevice != lastSelectedDevice || requestedDevice != lastRequestedDevice))
	{
		static const char *devnames[] = {
			"Windows Default",
			"OPL",
			"",
			"Timidity++",
			"FluidSynth",
			"GUS",
			"WildMidi"
		};

		lastRequestedDevice = requestedDevice;
		lastSelectedDevice = selectedDevice;
		Printf(TEXTCOLOR_RED "Unable to create " TEXTCOLOR_ORANGE "%s" TEXTCOLOR_RED " MIDI device. Falling back to " TEXTCOLOR_ORANGE "%s\n", devnames[requestedDevice], devnames[selectedDevice]);
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
	MIDI = CreateMIDIDevice(devtype, 0);
	InitPlayback();
}

//==========================================================================
//
// MIDIStreamer :: DumpWave
//
//==========================================================================

bool MIDIStreamer::DumpOPL(const char *filename, int subsong)
{
	m_Looping = false;
	if (source == nullptr) return false;	// We have nothing to play so abort.
	source->SetMIDISubsong(subsong);

	assert(MIDI == NULL);
	MIDI = new OPLDumperMIDIDevice(filename);
	return InitPlayback();
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
	if (devtype == MDEV_MMAPI)
	{
		Printf("MMAPI device is not supported\n");
		return false;
	}
	MIDI = CreateMIDIDevice(devtype, samplerate);
	MIDI = new MIDIWaveWriter(filename, reinterpret_cast<SoftSynthMIDIDevice *>(MIDI));
	return InitPlayback();
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

	if (MIDI == NULL || 0 != MIDI->Open(Callback, this))
	{
		Printf(PRINT_BOLD, "Could not open MIDI out device\n");
		if (MIDI != NULL)
		{
			delete MIDI;
			MIDI = NULL;
		}
		return false;
	}

	source->CheckCaps(MIDI->GetTechnology());
	if (!MIDI->CanHandleSysex()) source->SkipSysex();

	if (MIDI->Preprocess(this, m_Looping))
	{
		StartPlayback();
		if (MIDI == NULL)
		{ // The MIDI file had no content and has been automatically closed.
			return false;
		}
	}

	if (0 != MIDI->Resume())
	{
		Printf ("Starting MIDI playback failed\n");
		Stop();
		return false;
	}
	else
	{
		m_Status = STATE_Playing;
		return true;
	}
}

//==========================================================================
//
// MIDIStreamer :: StartPlayback
//
//==========================================================================

void MIDIStreamer::StartPlayback()
{
	auto data = source->PrecacheData();
	MIDI->PrecacheInstruments(&data[0], data.Size());
	source->StartPlayback(m_Looping);
	
	// Set time division and tempo.
	if (0 != MIDI->SetTimeDiv(source->getDivision()) ||
		0 != MIDI->SetTempo(source->getInitialTempo()))
	{
		Printf(PRINT_BOLD, "Setting MIDI stream speed failed\n");
		MIDI->Close();
		return;
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
				Printf ("Initial midiStreamOut failed\n");
				Stop();
				return;
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
	if (MIDI != NULL)
	{
		delete MIDI;
		MIDI = NULL;
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
		Stop();
	}
	if (m_Status != STATE_Stopped && !MIDI->IsOpen())
	{
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
		float realvolume = clamp<float>(snd_musicvolume * relative_volume * snd_mastervolume, 0.f, 1.f);
		Volume = clamp<uint32_t>((uint32_t)(realvolume * 65535.f), 0, 65535);
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
// MIDIStreamer :: FluidSettingInt
//
//==========================================================================

void MIDIStreamer::FluidSettingInt(const char *setting, int value)
{
	if (MIDI != NULL)
	{
		MIDI->FluidSettingInt(setting, value);
	}
}

//==========================================================================
//
// MIDIStreamer :: FluidSettingNum
//
//==========================================================================

void MIDIStreamer::FluidSettingNum(const char *setting, double value)
{
	if (MIDI != NULL)
	{
		MIDI->FluidSettingNum(setting, value);
	}
}

//==========================================================================
//
// MIDIDeviceStreamer :: FluidSettingStr
//
//==========================================================================

void MIDIStreamer::FluidSettingStr(const char *setting, const char *value)
{
	if (MIDI != NULL)
	{
		MIDI->FluidSettingStr(setting, value);
	}
}


//==========================================================================
//
// MIDIDeviceStreamer :: WildMidiSetOption
//
//==========================================================================

void MIDIStreamer::WildMidiSetOption(int opt, int set)
{
	if (MIDI != NULL)
	{
		MIDI->WildMidiSetOption(opt, set);
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
	if (MIDI != nullptr && !MIDI->Update()) Stop();
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
	uint32_t tot_time = 0;
	uint32_t time = 0;

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
		events[0] = MAX<uint32_t>(1, (max_time / 3) * source->getDivision() / source->getTempo());
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
	source = _source;
	source->setTempoCallback([=](int tempo) { return !!MIDI->SetTempo(tempo); } );
}


//==========================================================================
//
// MIDIStreamer :: GetStats
//
//==========================================================================

FString MIDIStreamer::GetStats()
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
// MIDIDevice stubs.
//
//==========================================================================

MIDIDevice::MIDIDevice()
{
}

MIDIDevice::~MIDIDevice()
{
}

//==========================================================================
//
// MIDIDevice :: PrecacheInstruments
//
// The MIDIStreamer calls this method between device open and the first
// buffered stream with a list of instruments known to be used by the song.
// If the device can benefit from preloading the instruments, it can do so
// now.
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
//==========================================================================

void MIDIDevice::PrecacheInstruments(const uint16_t *instruments, int count)
{
}

//==========================================================================
//
// MIDIDevice :: Preprocess
//
// Gives the MIDI device a chance to do some processing with the song before
// it starts playing it. Returns true if MIDIStreamer should perform its
// standard playback startup sequence.
//
//==========================================================================

bool MIDIDevice::Preprocess(MIDIStreamer *song, bool looping)
{
	return true;
}

//==========================================================================
//
// MIDIDevice :: PrepareHeader
//
// Wrapper for MCI's midiOutPrepareHeader.
//
//==========================================================================

int MIDIDevice::PrepareHeader(MidiHeader *header)
{
	return 0;
}

//==========================================================================
//
// MIDIDevice :: UnprepareHeader
//
// Wrapper for MCI's midiOutUnprepareHeader.
//
//==========================================================================

int MIDIDevice::UnprepareHeader(MidiHeader *header)
{
	return 0;
}

//==========================================================================
//
// MIDIDevice :: FakeVolume
//
// Since most implementations render as a normal stream, their volume is
// controlled through the GSnd interface, not here.
//
//==========================================================================

bool MIDIDevice::FakeVolume()
{
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

void MIDIDevice::InitPlayback()
{
}

//==========================================================================
//
//
//
//==========================================================================

bool MIDIDevice::Update()
{
	return true;
}

//==========================================================================
//
// MIDIDevice :: FluidSettingInt
//
//==========================================================================

void MIDIDevice::FluidSettingInt(const char *setting, int value)
{
}

//==========================================================================
//
// MIDIDevice :: FluidSettingNum
//
//==========================================================================

void MIDIDevice::FluidSettingNum(const char *setting, double value)
{
}

//==========================================================================
//
// MIDIDevice :: FluidSettingStr
//
//==========================================================================

void MIDIDevice::FluidSettingStr(const char *setting, const char *value)
{
}

//==========================================================================
//
// MIDIDevice :: WildMidiSetOption
//
//==========================================================================

void MIDIDevice::WildMidiSetOption(int opt, int set)
{
}

//==========================================================================
//
// MIDIDevice :: GetStats
//
//==========================================================================

FString MIDIDevice::GetStats()
{
	return "This MIDI device does not have any stats.";
}
