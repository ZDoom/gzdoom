/*
** music_win_mididevice.cpp
** Provides a WinMM implementation of a MIDI output device.
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

#ifdef _WIN32

#include "i_midi_win32.h"

// HEADER FILES ------------------------------------------------------------

#include "i_musicinterns.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "doomerrors.h"

#ifndef __GNUC__
#include <mmdeviceapi.h>
#endif

// MACROS ------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static bool IgnoreMIDIVolume(UINT id);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------
// WinMM implementation of a MIDI output device -----------------------------

class WinMIDIDevice : public MIDIDevice
{
public:
	WinMIDIDevice(int dev_id);
	~WinMIDIDevice();
	int Open(MidiCallback, void *userdata);
	void Close();
	bool IsOpen() const;
	int GetTechnology() const;
	int SetTempo(int tempo);
	int SetTimeDiv(int timediv);
	int StreamOut(MidiHeader *data);
	int StreamOutSync(MidiHeader *data);
	int Resume();
	void Stop();
	int PrepareHeader(MidiHeader *data);
	int UnprepareHeader(MidiHeader *data);
	bool FakeVolume();
	bool Pause(bool paused);
	void InitPlayback() override;
	bool Update() override;
	void PrecacheInstruments(const uint16_t *instruments, int count);
	DWORD PlayerLoop();
	bool CanHandleSysex() const override
	{
		// No Sysex for GS synth.
		return VolumeWorks;
	}


//protected:
	static void CALLBACK CallbackFunc(HMIDIOUT, UINT, DWORD_PTR, DWORD, DWORD);

	MIDIStreamer *Streamer;
	HMIDISTRM MidiOut;
	UINT DeviceID;
	DWORD SavedVolume;
	MIDIHDR WinMidiHeaders[2];
	int HeaderIndex;
	bool VolumeWorks;

	MidiCallback Callback;
	void *CallbackData;

	HANDLE BufferDoneEvent;
	HANDLE ExitEvent;
	HANDLE PlayerThread;


};

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool, snd_midiprecache, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);

// CODE --------------------------------------------------------------------

//==========================================================================
//
// WinMIDIDevice Contructor
//
//==========================================================================

WinMIDIDevice::WinMIDIDevice(int dev_id)
{
	DeviceID = MAX<DWORD>(dev_id, 0);
	MidiOut = 0;
	HeaderIndex = 0;
	memset(WinMidiHeaders, 0, sizeof(WinMidiHeaders));

	BufferDoneEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (BufferDoneEvent == nullptr)
	{
		Printf(PRINT_BOLD, "Could not create buffer done event for MIDI playback\n");
	}
	ExitEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (ExitEvent == nullptr)
	{
		Printf(PRINT_BOLD, "Could not create exit event for MIDI playback\n");
	}
	PlayerThread = nullptr;
}

//==========================================================================
//
// WinMIDIDevice Destructor
//
//==========================================================================

WinMIDIDevice::~WinMIDIDevice()
{
	Close();

	if (ExitEvent != nullptr)
	{
		CloseHandle(ExitEvent);
	}
	if (BufferDoneEvent != nullptr)
	{
		CloseHandle(BufferDoneEvent);
	}
}

//==========================================================================
//
// WinMIDIDevice :: Open
//
//==========================================================================

int WinMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	MMRESULT err;

	Callback = callback;
	CallbackData = userdata;
	if (MidiOut == nullptr)
	{
		err = midiStreamOpen(&MidiOut, &DeviceID, 1, (DWORD_PTR)CallbackFunc, (DWORD_PTR)this, CALLBACK_FUNCTION);

		if (err == MMSYSERR_NOERROR)
		{
			if (IgnoreMIDIVolume(DeviceID))
			{
				VolumeWorks = false;
			}
			else
			{
				// Set master volume to full, if the device allows it on this interface.
				VolumeWorks = (MMSYSERR_NOERROR == midiOutGetVolume((HMIDIOUT)MidiOut, &SavedVolume));
				if (VolumeWorks)
				{
					VolumeWorks &= (MMSYSERR_NOERROR == midiOutSetVolume((HMIDIOUT)MidiOut, 0xffffffff));
				}
			}
		}
		else
		{
			return 1;
		}
	}
	return 0;
}

//==========================================================================
//
// WinMIDIDevice :: Close
//
//==========================================================================

void WinMIDIDevice::Close()
{
	if (MidiOut != nullptr)
	{
		midiStreamClose(MidiOut);
		MidiOut = nullptr;
	}
}

//==========================================================================
//
// WinMIDIDevice :: IsOpen
//
//==========================================================================

bool WinMIDIDevice::IsOpen() const
{
	return MidiOut != nullptr;
}

//==========================================================================
//
// WinMIDIDevice :: GetTechnology
//
//==========================================================================

int WinMIDIDevice::GetTechnology() const
{
	MIDIOUTCAPS caps;

	if (MMSYSERR_NOERROR == midiOutGetDevCaps(DeviceID, &caps, sizeof(caps)))
	{
		return caps.wTechnology;
	}
	return -1;
}

//==========================================================================
//
// WinMIDIDevice :: SetTempo
//
//==========================================================================

int WinMIDIDevice::SetTempo(int tempo)
{
	MIDIPROPTEMPO data = { sizeof(MIDIPROPTEMPO), (DWORD)tempo };
	return midiStreamProperty(MidiOut, (LPBYTE)&data, MIDIPROP_SET | MIDIPROP_TEMPO);
}

//==========================================================================
//
// WinMIDIDevice :: SetTimeDiv
//
//==========================================================================

int WinMIDIDevice::SetTimeDiv(int timediv)
{
	MIDIPROPTIMEDIV data = { sizeof(MIDIPROPTIMEDIV), (DWORD)timediv };
	return midiStreamProperty(MidiOut, (LPBYTE)&data, MIDIPROP_SET | MIDIPROP_TIMEDIV);
}

//==========================================================================
//
// MIDIStreamer :: PlayerProc										Static
//
// Entry point for the player thread.
//
//==========================================================================

DWORD WINAPI PlayerProc(LPVOID lpParameter)
{
	return ((WinMIDIDevice *)lpParameter)->PlayerLoop();
}

//==========================================================================
//
// WinMIDIDevice :: Resume
//
//==========================================================================

int WinMIDIDevice::Resume()
{
	DWORD tid;
	int ret =  midiStreamRestart(MidiOut);
	if (ret == 0)
	{
		PlayerThread = CreateThread(nullptr, 0, PlayerProc, this, 0, &tid);
		if (PlayerThread == nullptr)
		{
			Printf("Creating MIDI thread failed\n");
			Stop();
			return MMSYSERR_NOTSUPPORTED;
		}
	}
	return ret;
}

//==========================================================================
//
// WinMIDIDevice :: InitPlayback
//
//==========================================================================

void WinMIDIDevice::InitPlayback()
{
	ResetEvent(ExitEvent);
	ResetEvent(BufferDoneEvent);
}

//==========================================================================
//
// WinMIDIDevice :: Stop
//
//==========================================================================

void WinMIDIDevice::Stop()
{
	if (PlayerThread != nullptr)
	{
		SetEvent(ExitEvent);
		WaitForSingleObject(PlayerThread, INFINITE);
		CloseHandle(PlayerThread);
		PlayerThread = nullptr;
	}

	midiStreamStop(MidiOut);
	midiOutReset((HMIDIOUT)MidiOut);
	if (VolumeWorks)
	{
		midiOutSetVolume((HMIDIOUT)MidiOut, SavedVolume);
	}
}

//==========================================================================
//
// MIDIStreamer :: PlayerLoop
//
// Services MIDI playback events.
//
//==========================================================================

DWORD WinMIDIDevice::PlayerLoop()
{
	HANDLE events[2] = { BufferDoneEvent, ExitEvent };

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	for (;;)
	{
		switch (WaitForMultipleObjects(2, events, FALSE, INFINITE))
		{
		case WAIT_OBJECT_0:
			if (Callback != nullptr) Callback(CallbackData);
			break;

		case WAIT_OBJECT_0 + 1:
			return 0;

		default:
			// Should not happen.
			return MMSYSERR_ERROR;
		}
	}
	return 0;
}


//==========================================================================
//
// WinMIDIDevice :: PrecacheInstruments
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
// My old GUS PnP needed the instruments to be preloaded, or it would miss
// some notes the first time through a song. I doubt any modern
// hardware has this problem, but since I'd already written the code for
// ZDoom 1.22 and below, I'm resurrecting it now for completeness, since I'm
// using preloading for the internal Timidity.
//
// NOTETOSELF: Why did I never notice the midiOutCache(Drum)Patches calls
// before now? Should I switch to them? This code worked on my GUS, but
// using the APIs intended for caching might be better.
//
//==========================================================================

void WinMIDIDevice::PrecacheInstruments(const uint16_t *instruments, int count)
{
	// Setting snd_midiprecache to false disables this precaching, since it
	// does involve sleeping for more than a miniscule amount of time.
	if (!snd_midiprecache)
	{
		return;
	}
	uint8_t bank[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int i, chan;

	for (i = 0, chan = 0; i < count; ++i)
	{
		int instr = instruments[i] & 127;
		int banknum = (instruments[i] >> 7) & 127;
		int percussion = instruments[i] >> 14;

		if (percussion)
		{
			if (bank[9] != banknum)
			{
				midiOutShortMsg((HMIDIOUT)MidiOut, MIDI_CTRLCHANGE | 9 | (0 << 8) | (banknum << 16));
				bank[9] = banknum;
			}
			midiOutShortMsg((HMIDIOUT)MidiOut, MIDI_NOTEON | 9 | ((instruments[i] & 0x7f) << 8) | (1 << 16));
		}
		else
		{ // Melodic
			if (bank[chan] != banknum)
			{
				midiOutShortMsg((HMIDIOUT)MidiOut, MIDI_CTRLCHANGE | 9 | (0 << 8) | (banknum << 16));
				bank[chan] = banknum;
			}
			midiOutShortMsg((HMIDIOUT)MidiOut, MIDI_PRGMCHANGE | chan | (instruments[i] << 8));
			midiOutShortMsg((HMIDIOUT)MidiOut, MIDI_NOTEON | chan | (60 << 8) | (1 << 16));
			if (++chan == 9)
			{ // Skip the percussion channel
				chan = 10;
			}
		}
		// Once we've got an instrument playing on each melodic channel, sleep to give
		// the driver time to load the instruments. Also do this for the final batch
		// of instruments.
		if (chan == 16 || i == count - 1)
		{
			Sleep(250);
			for (chan = 15; chan-- != 0; )
			{
				// Turn all notes off
				midiOutShortMsg((HMIDIOUT)MidiOut, MIDI_CTRLCHANGE | chan | (123 << 8));
			}
			// And now chan is back at 0, ready to start the cycle over.
		}
	}
	// Make sure all channels are set back to bank 0.
	for (i = 0; i < 16; ++i)
	{
		if (bank[i] != 0)
		{
			midiOutShortMsg((HMIDIOUT)MidiOut, MIDI_CTRLCHANGE | 9 | (0 << 8) | (0 << 16));
		}
	}
}

//==========================================================================
//
// WinMIDIDevice :: Pause
//
// Some docs claim pause is unreliable and can cause the stream to stop
// functioning entirely. Truth or fiction?
//
//==========================================================================

bool WinMIDIDevice::Pause(bool paused)
{
	return false;
}

//==========================================================================
//
// WinMIDIDevice :: StreamOut
//
//==========================================================================

int WinMIDIDevice::StreamOut(MidiHeader *header)
{
	auto syshdr = (MIDIHDR*)header->lpNext;
	assert(syshdr == &WinMidiHeaders[0] || syshdr == &WinMidiHeaders[1]);
	return midiStreamOut(MidiOut, syshdr, sizeof(MIDIHDR));
}

//==========================================================================
//
// WinMIDIDevice :: StreamOutSync
//
//==========================================================================

int WinMIDIDevice::StreamOutSync(MidiHeader *header)
{
	return StreamOut(header);
}

//==========================================================================
//
// WinMIDIDevice :: PrepareHeader
//
//==========================================================================

int WinMIDIDevice::PrepareHeader(MidiHeader *header)
{
	// This code depends on the driving implementation only having two buffers that get passed alternatingly.
	// If there were more buffers this would require more intelligent handling.
	assert(header->lpNext == nullptr);
	MIDIHDR *syshdr = &WinMidiHeaders[HeaderIndex ^= 1];
	memset(syshdr, 0, sizeof(MIDIHDR));
	syshdr->lpData = (LPSTR)header->lpData;
	syshdr->dwBufferLength = header->dwBufferLength;
	syshdr->dwBytesRecorded = header->dwBytesRecorded;
	// this device does not use the lpNext pointer to link MIDI events so use it to point to the system data structure.
	header->lpNext = (MidiHeader*)syshdr;	
	return midiOutPrepareHeader((HMIDIOUT)MidiOut, syshdr, sizeof(MIDIHDR));
}

//==========================================================================
//
// WinMIDIDevice :: UnprepareHeader
//
//==========================================================================

int WinMIDIDevice::UnprepareHeader(MidiHeader *header)
{
	auto syshdr = (MIDIHDR*)header->lpNext;
	if (syshdr != nullptr)
	{
		assert(syshdr == &WinMidiHeaders[0] || syshdr == &WinMidiHeaders[1]);
		header->lpNext = nullptr;
		return midiOutUnprepareHeader((HMIDIOUT)MidiOut, syshdr, sizeof(MIDIHDR));
	}
	else
	{
		return MMSYSERR_NOERROR;
	}
}

//==========================================================================
//
// WinMIDIDevice :: FakeVolume
//
// Because there are too many MIDI devices out there that don't support
// global volume changes, fake the volume for all of them.
//
//==========================================================================

bool WinMIDIDevice::FakeVolume()
{
	return true;
}

//==========================================================================
//
// WinMIDIDevice :: Update
//
//==========================================================================

bool WinMIDIDevice::Update()
{
	// If the PlayerThread is signalled, then it's dead.
	if (PlayerThread != nullptr &&
		WaitForSingleObject(PlayerThread, 0) == WAIT_OBJECT_0)
	{
		static const char *const MMErrorCodes[] =
		{
			"No error",
			"Unspecified error",
			"Device ID out of range",
			"Driver failed enable",
			"Device already allocated",
			"Device handle is invalid",
			"No device driver present",
			"Memory allocation error",
			"Function isn't supported",
			"Error value out of range",
			"Invalid flag passed",
			"Invalid parameter passed",
			"Handle being used simultaneously on another thread",
			"Specified alias not found",
			"Bad registry database",
			"Registry key not found",
			"Registry read error",
			"Registry write error",
			"Registry delete error",
			"Registry value not found",
			"Driver does not call DriverCallback",
			"More data to be returned",
		};
		static const char *const MidiErrorCodes[] =
		{
			"MIDI header not prepared",
			"MIDI still playing something",
			"MIDI no configured instruments",
			"MIDI hardware is still busy",
			"MIDI port no longer connected",
			"MIDI invalid MIF",
			"MIDI operation unsupported with open mode",
			"MIDI through device 'eating' a message",
		};
		DWORD code = 0xABADCAFE;
		GetExitCodeThread(PlayerThread, &code);
		CloseHandle(PlayerThread);
		PlayerThread = nullptr;
		Printf("MIDI playback failure: ");
		if (code < countof(MMErrorCodes))
		{
			Printf("%s\n", MMErrorCodes[code]);
		}
		else if (code >= MIDIERR_BASE && code < MIDIERR_BASE + countof(MidiErrorCodes))
		{
			Printf("%s\n", MidiErrorCodes[code - MIDIERR_BASE]);
		}
		else
		{
			Printf("%08x\n", code);
		}
		return false;
	}
	return true;
}

//==========================================================================
//
// WinMIDIDevice :: CallbackFunc									static
//
//==========================================================================

void CALLBACK WinMIDIDevice::CallbackFunc(HMIDIOUT hOut, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	WinMIDIDevice *self = (WinMIDIDevice *)dwInstance;
	if (uMsg == MOM_DONE)
	{
		SetEvent(self->BufferDoneEvent);
	}
}

//==========================================================================
//
// IgnoreMIDIVolume
//
// Should we ignore this MIDI device's volume control even if it works?
//
// Under Windows Vista and up, when using the standard "Microsoft GS
// Wavetable Synth", midiOutSetVolume() will affect the application's audio
// session volume rather than the volume for just the MIDI stream. At first,
// I thought I could get around this by enumerating the streams in the
// audio session to find the MIDI device's stream to set its volume
// manually, but there doesn't appear to be any way to enumerate the
// individual streams in a session. Consequently, we'll just assume the MIDI
// device gets created at full volume like we want. (Actual volume changes
// are done by sending MIDI channel volume messages to the stream, not
// through midiOutSetVolume().)
//
//==========================================================================

static bool IgnoreMIDIVolume(UINT id)
{
	MIDIOUTCAPSA caps;

	if (MMSYSERR_NOERROR == midiOutGetDevCapsA(id, &caps, sizeof(caps)))
	{
		if (caps.wTechnology == MIDIDEV_MAPPER)
		{
			// We cannot determine what this is so we have to assume the worst, as the default
			// devive's volume control is irreparably broken.
			return true;
		}
		// The Microsoft GS Wavetable Synth advertises itself as MIDIDEV_SWSYNTH with a VOLUME control.
		// If the one we're using doesn't match that, we don't need to bother checking the name.
		if (caps.wTechnology == MIDIDEV_SWSYNTH && (caps.dwSupport & MIDICAPS_VOLUME))
		{
			if (strncmp(caps.szPname, "Microsoft GS", 12) == 0)
			{
				return true;
			}
		}
	}
	return false;
}

MIDIDevice *CreateWinMIDIDevice(int mididevice)
{
	auto d = new WinMIDIDevice(mididevice);
	if (d->BufferDoneEvent == nullptr || d->ExitEvent == nullptr)
	{
		delete d;
		I_Error("failed to create MIDI events");
	}
	return d;
}
#endif


