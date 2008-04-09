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
#include "doomdef.h"
#include "m_swap.h"

// MACROS ------------------------------------------------------------------

#define MAX_TIME	(1000000/10)	// Send out 1/10 of a sec of events at a time.

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR(Float, snd_musicvolume)

#ifdef _WIN32
extern UINT mididevice;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// MIDIStreamer Constructor
//
//==========================================================================

MIDIStreamer::MIDIStreamer(bool opl)
:
#ifdef _WIN32
  PlayerThread(0), ExitEvent(0), BufferDoneEvent(0),
#endif
  MIDI(0), Division(0), InitialTempo(500000), UseOPLDevice(opl)
{
#ifdef _WIN32
	BufferDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (BufferDoneEvent == NULL)
	{
		Printf(PRINT_BOLD, "Could not create buffer done event for MIDI playback\n");
	}
	ExitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (ExitEvent == NULL)
	{
		Printf(PRINT_BOLD, "Could not create exit event for MIDI playback\n");
		return;
	}
#endif
}

//==========================================================================
//
// MIDIStreamer OPL Dumping Constructor
//
//==========================================================================

MIDIStreamer::MIDIStreamer(const char *dumpname)
:
#ifdef _WIN32
  PlayerThread(0), ExitEvent(0), BufferDoneEvent(0),
#endif
  MIDI(0), Division(0), InitialTempo(500000), UseOPLDevice(true), DumpFilename(dumpname)
{
#ifdef _WIN32
	BufferDoneEvent = NULL;
	ExitEvent = NULL;
#endif
}

//==========================================================================
//
// MIDIStreamer Destructor
//
//==========================================================================

MIDIStreamer::~MIDIStreamer()
{
	Stop();
#ifdef _WIN32
	if (ExitEvent != NULL)
	{
		CloseHandle(ExitEvent);
	}
	if (BufferDoneEvent != NULL)
	{
		CloseHandle(BufferDoneEvent);
	}
#endif
	if (MIDI != NULL)
	{
		delete MIDI;
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
#ifdef _WIN32
	return ExitEvent != NULL && Division != 0;
#else
	return Division != 0;
#endif
}

//==========================================================================
//
// MIDIStreamer :: CheckCaps
//
// Called immediately after the device is opened in case a subclass should
// want to alter its behavior depending on which device it got.
//
//==========================================================================

void MIDIStreamer::CheckCaps()
{
}

//==========================================================================
//
// MIDIStreamer :: Play
//
//==========================================================================

void MIDIStreamer::Play(bool looping)
{
	DWORD tid;

	m_Status = STATE_Stopped;
	m_Looping = looping;
	EndQueued = 0;
	VolumeChanged = false;
	Restarting = true;
	InitialPlayback = true;

	assert(MIDI == NULL);
	if (DumpFilename.IsNotEmpty())
	{
		MIDI = new OPLDumperMIDIDevice(DumpFilename);
	}
	else
#ifdef _WIN32
	if (!UseOPLDevice)
	{
		MIDI = new WinMIDIDevice(mididevice);
	}
	else
#endif
	{
		MIDI = new OPLMIDIDevice;
	}
	
#ifndef _WIN32
	assert(MIDI->NeedThreadedCallback() == false);
#endif

	if (0 != MIDI->Open(Callback, this))
	{
		Printf(PRINT_BOLD, "Could not open MIDI out device\n");
		return;
	}

	CheckCaps();
	Precache();

	// Set time division and tempo.
	if (0 != MIDI->SetTimeDiv(Division) ||
		0 != MIDI->SetTempo(Tempo = InitialTempo))
	{
		Printf(PRINT_BOLD, "Setting MIDI stream speed failed\n");
		MIDI->Close();
		return;
	}

	MusicVolumeChanged();	// set volume to current music's properties

#ifdef _WIN32
	ResetEvent(ExitEvent);
	ResetEvent(BufferDoneEvent);
#endif

	// Fill the initial buffers for the song.
	BufferNum = 0;
	do
	{
		int res = FillBuffer(BufferNum, MAX_EVENTS, MAX_TIME);
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

	if (0 != MIDI->Resume())
	{
		Printf ("Starting MIDI playback failed\n");
		Stop();
	}
	else
	{
#ifdef _WIN32
		if (MIDI->NeedThreadedCallback())
		{
			PlayerThread = CreateThread(NULL, 0, PlayerProc, this, 0, &tid);
			if (PlayerThread == NULL)
			{
				Printf ("Creating MIDI thread failed\n");
				Stop();
			}
			else
			{
				m_Status = STATE_Playing;
			}
		}
		else
#endif
		{
			m_Status = STATE_Playing;
		}
	}
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
	EndQueued = 2;
#ifdef _WIN32
	if (PlayerThread != NULL)
	{
		SetEvent(ExitEvent);
		WaitForSingleObject(PlayerThread, INFINITE);
		CloseHandle(PlayerThread);
		PlayerThread = NULL;
	}
#endif
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
	if (MIDI->FakeVolume())
	{
		float realvolume = clamp<float>(snd_musicvolume * relative_volume, 0.f, 1.f);
		Volume = clamp<DWORD>((DWORD)(realvolume * 65535.f), 0, 65535);
	}
	else
	{
		Volume = 0xFFFF;
	}
	if (m_Status == STATE_Playing)
	{
		OutputVolume(Volume);
	}
}

//==========================================================================
//
// MIDIStreamer :: OutputVolume
//
// Signals the buffer filler to send volume change events on all channels.
//
//==========================================================================

void MIDIStreamer::OutputVolume (DWORD volume)
{
	if (MIDI->FakeVolume())
	{
		NewVolume = volume;
		VolumeChanged = true;
	}
}

//==========================================================================
//
// MIDIStreamer :: VolumeControllerChange
//
// Some devices don't support master volume
// (e.g. the Audigy's software MIDI synth--but not its two hardware ones),
// so assume none of them do and scale channel volumes manually.
//
//==========================================================================

int MIDIStreamer::VolumeControllerChange(int channel, int volume)
{
	ChannelVolumes[channel] = volume;
	return ((volume + 1) * Volume) >> 16;
}

//==========================================================================
//
// MIDIStreamer :: Callback											Static
//
// Signals the BufferDoneEvent to prepare the next buffer. The buffer is not
// prepared in the callback directly, because it's generally still in use by
// the MIDI streamer when this callback is executed.
//
//==========================================================================

void MIDIStreamer::Callback(unsigned int uMsg, void *userdata, DWORD dwParam1, DWORD dwParam2)
{
	MIDIStreamer *self = (MIDIStreamer *)userdata;

	if (self->EndQueued > 1)
	{
		return;
	}
	if (uMsg == MOM_DONE)
	{
#ifdef _WIN32
		if (self->PlayerThread != NULL)
		{
			SetEvent(self->BufferDoneEvent);
		}
		else
#endif
		{
			self->ServiceEvent();
		}
	}
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
#ifdef _WIN32
	// If the PlayerThread is signalled, then it's dead.
	if (PlayerThread != NULL &&
		WaitForSingleObject(PlayerThread, 0) == WAIT_OBJECT_0)
	{
		CloseHandle(PlayerThread);
		PlayerThread = NULL;
		Printf ("MIDI playback failure\n");
		Stop();
	}
#endif
}

//==========================================================================
//
// MIDIStreamer :: PlayerProc										Static
//
// Entry point for the player thread.
//
//==========================================================================

#ifdef _WIN32
DWORD WINAPI MIDIStreamer::PlayerProc (LPVOID lpParameter)
{
	return ((MIDIStreamer *)lpParameter)->PlayerLoop();
}
#endif

//==========================================================================
//
// MIDIStreamer :: PlayerLoop
//
// Services MIDI playback events.
//
//==========================================================================

#ifdef _WIN32
DWORD MIDIStreamer::PlayerLoop()
{
	HANDLE events[2] = { BufferDoneEvent, ExitEvent };

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	for (;;)
	{
		switch (WaitForMultipleObjects(2, events, FALSE, INFINITE))
		{
		case WAIT_OBJECT_0:
			if (ServiceEvent())
			{
				return 1;
			}
			break;

		case WAIT_OBJECT_0 + 1:
			return 0;

		default:
			// Should not happen.
			return 1;
		}
	}
}
#endif

//==========================================================================
//
// MIDIStreamer :: ServiceEvent
//
// Fills the buffer that just finished playing with new events and appends
// it to the MIDI stream queue. Stops the song if playback is over. Returns
// true if a problem occured and playback should stop.
//
//==========================================================================

bool MIDIStreamer::ServiceEvent()
{
	if (EndQueued == 1)
	{
		return false;
	}
	if (0 != MIDI->UnprepareHeader(&Buffer[BufferNum]))
	{
		return true;
	}
fill:
	switch (FillBuffer(BufferNum, MAX_EVENTS, MAX_TIME))
	{
	case SONG_MORE:
		if ((MIDI->NeedThreadedCallback() && 0 != MIDI->StreamOutSync(&Buffer[BufferNum])) ||
			(!MIDI->NeedThreadedCallback() && 0 != MIDI->StreamOut(&Buffer[BufferNum])))
		{
			return true;
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
		return true;
	}
	return false;
}

//==========================================================================
//
// MIDIStreamer :: FillBuffer
//
// Copies MIDI events from the SMF and puts them into a MIDI stream
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

int MIDIStreamer::FillBuffer(int buffer_num, int max_events, DWORD max_time)
{
	if (!Restarting && CheckDone())
	{
		return SONG_DONE;
	}

	int i;
	DWORD *events = Events[buffer_num], *max_event_p;
	DWORD tot_time = 0;
	DWORD time = 0;

	// The final event is for a NOP to hold the delay from the last event.
	max_event_p = events + (max_events - 1) * 3;

	if (InitialPlayback)
	{
		InitialPlayback = false;
		// Send the full master volume SysEx message.
		events[0] = 0;							// dwDeltaTime
		events[1] = 0;							// dwStreamID
		events[2] = (MEVT_LONGMSG << 24) | 8;	// dwEvent
		events[3] = 0x047f7ff0;					// dwParms[0]
		events[4] = 0xf77f7f01;					// dwParms[1]
		events += 5;
		DoInitialSetup();
	}

	// If the volume has changed, stick those events at the start of this buffer.
	if (VolumeChanged && (m_Status != STATE_Paused || NewVolume == 0))
	{
		VolumeChanged = false;
		for (i = 0; i < 16; ++i)
		{
			BYTE courseVol = (BYTE)(((ChannelVolumes[i]+1) * NewVolume) >> 16);
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
		events[0] = MAX<DWORD>(1, (max_time / 3) * Division / Tempo);
		events[1] = 0;
		events[2] = MEVT_NOP << 24;
		events += 3;
	}
	else
	{
		if (Restarting)
		{
			Restarting = false;
			// Stop all notes in case any were left hanging.
			for (i = 0; i < 16; ++i)
			{
				events[0] = 0;				// dwDeltaTime
				events[1] = 0;				// dwStreamID
				events[2] = MIDI_NOTEOFF | i | (60 << 8) | (64<<16);
				events += 3;
			}
			DoRestart();
		}
		events = MakeEvents(events, max_event_p, max_time);
	}
	memset(&Buffer[buffer_num], 0, sizeof(MIDIHDR));
	Buffer[buffer_num].lpData = (LPSTR)Events[buffer_num];
	Buffer[buffer_num].dwBufferLength = DWORD((LPSTR)events - Buffer[buffer_num].lpData);
	Buffer[buffer_num].dwBytesRecorded = Buffer[buffer_num].dwBufferLength;
	if (0 != MIDI->PrepareHeader(&Buffer[buffer_num]))
	{
		return SONG_ERROR;
	}
	return SONG_MORE;
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
// For each entry, bit 7 set indicates that the instrument is percussion and
// the lower 7 bits contain the note number to use on MIDI channel 10,
// otherwise it is melodic and the lower 7 bits are the program number.
//
//==========================================================================

void MIDIDevice::PrecacheInstruments(const BYTE *instruments, int count)
{
}
