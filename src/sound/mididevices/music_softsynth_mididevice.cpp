/*
** music_softsynth_mididevice.cpp
** Common base class for software synthesis MIDI devices.
**
**---------------------------------------------------------------------------
** Copyright 2008-2010 Randy Heit
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
#include "i_system.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR(Bool, synth_watch, false, 0)

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SoftSynthMIDIDevice Constructor
//
//==========================================================================

SoftSynthMIDIDevice::SoftSynthMIDIDevice(int samplerate, int minrate, int maxrate)
{
	Stream = NULL;
	Tempo = 0;
	Division = 0;
	Events = NULL;
	Started = false;
	SampleRate = samplerate;
	if (SampleRate < minrate || SampleRate > maxrate) SampleRate = GSnd != NULL ? clamp((int)GSnd->GetOutputRate(), minrate, maxrate) : 44100;
}

//==========================================================================
//
// SoftSynthMIDIDevice Destructor
//
//==========================================================================

SoftSynthMIDIDevice::~SoftSynthMIDIDevice()
{
	Close();
}

//==========================================================================
//
// SoftSynthMIDIDevice :: OpenStream
//
//==========================================================================

int SoftSynthMIDIDevice::OpenStream(int chunks, int flags, MidiCallback callback, void *userdata)
{
	int chunksize = (SampleRate / chunks) * 4;
	if (!(flags & SoundStream::Mono))
	{
		chunksize *= 2;
	}
	Stream = GSnd->CreateStream(FillStream, chunksize, SoundStream::Float | flags, SampleRate, this);
	if (Stream == NULL)
	{
		return 2;
	}

	Callback = callback;
	CallbackData = userdata;
	Tempo = 500000;
	Division = 100;
	CalcTickRate();
	return 0;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: Close
//
//==========================================================================

void SoftSynthMIDIDevice::Close()
{
	if (Stream != NULL)
	{
		delete Stream;
		Stream = NULL;
	}
	Started = false;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: IsOpen
//
//==========================================================================

bool SoftSynthMIDIDevice::IsOpen() const
{
	return Stream != NULL;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: GetTechnology
//
//==========================================================================

int SoftSynthMIDIDevice::GetTechnology() const
{
	return MIDIDEV_SWSYNTH;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: SetTempo
//
//==========================================================================

int SoftSynthMIDIDevice::SetTempo(int tempo)
{
	Tempo = tempo;
	CalcTickRate();
	return 0;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: SetTimeDiv
//
//==========================================================================

int SoftSynthMIDIDevice::SetTimeDiv(int timediv)
{
	Division = timediv;
	CalcTickRate();
	return 0;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: CalcTickRate
//
// Tempo is the number of microseconds per quarter note.
// Division is the number of ticks per quarter note.
//
//==========================================================================

void SoftSynthMIDIDevice::CalcTickRate()
{
	SamplesPerTick = SampleRate / (1000000.0 / Tempo) / Division;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: Resume
//
//==========================================================================

int SoftSynthMIDIDevice::Resume()
{
	if (!Started)
	{
		if (Stream->Play(true, 1))
		{
			Started = true;
			return 0;
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: Stop
//
//==========================================================================

void SoftSynthMIDIDevice::Stop()
{
	if (Started)
	{
		Stream->Stop();
		Started = false;
	}
}

//==========================================================================
//
// SoftSynthMIDIDevice :: StreamOutSync
//
// This version is called from the main game thread and needs to
// synchronize with the player thread.
//
//==========================================================================

int SoftSynthMIDIDevice::StreamOutSync(MidiHeader *header)
{
	std::lock_guard<std::mutex> lock(CritSec);
	StreamOut(header);
	return 0;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: StreamOut
//
// This version is called from the player thread so does not need to
// arbitrate for access to the Events pointer.
//
//==========================================================================

int SoftSynthMIDIDevice::StreamOut(MidiHeader *header)
{
	header->lpNext = NULL;
	if (Events == NULL)
	{
		Events = header;
		NextTickIn = SamplesPerTick * *(uint32_t *)header->lpData;
		Position = 0;
	}
	else
	{
		MidiHeader **p;

		for (p = &Events; *p != NULL; p = &(*p)->lpNext)
		{ }
		*p = header;
	}
	return 0;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: Pause
//
//==========================================================================

bool SoftSynthMIDIDevice::Pause(bool paused)
{
	if (Stream != NULL)
	{
		return Stream->SetPaused(paused);
	}
	return true;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: PlayTick
//
// event[0] = delta time
// event[1] = unused
// event[2] = event
//
//==========================================================================

int SoftSynthMIDIDevice::PlayTick()
{
	uint32_t delay = 0;

	while (delay == 0 && Events != NULL)
	{
		uint32_t *event = (uint32_t *)(Events->lpData + Position);
		if (MEVENT_EVENTTYPE(event[2]) == MEVENT_TEMPO)
		{
			SetTempo(MEVENT_EVENTPARM(event[2]));
		}
		else if (MEVENT_EVENTTYPE(event[2]) == MEVENT_LONGMSG)
		{
			HandleLongEvent((uint8_t *)&event[3], MEVENT_EVENTPARM(event[2]));
		}
		else if (MEVENT_EVENTTYPE(event[2]) == 0)
		{ // Short MIDI event
			int status = event[2] & 0xff;
			int parm1 = (event[2] >> 8) & 0x7f;
			int parm2 = (event[2] >> 16) & 0x7f;
			HandleEvent(status, parm1, parm2);

			if (synth_watch)
			{
				static const char *const commands[8] =
				{
					"Note off",
					"Note on",
					"Poly press",
					"Ctrl change",
					"Prgm change",
					"Chan press",
					"Pitch bend",
					"SysEx"
				};
				char buffer[128];
				mysnprintf(buffer, countof(buffer), "C%02d: %11s %3d %3d\n", (status & 15) + 1, commands[(status >> 4) & 7], parm1, parm2);
#ifdef _WIN32
				I_DebugPrint(buffer);
#else
				fputs(buffer, stderr);
#endif
			}
		}

		// Advance to next event.
		if (event[2] < 0x80000000)
		{ // Short message
			Position += 12;
		}
		else
		{ // Long message
			Position += 12 + ((MEVENT_EVENTPARM(event[2]) + 3) & ~3);
		}

		// Did we use up this buffer?
		if (Position >= Events->dwBytesRecorded)
		{
			Events = Events->lpNext;
			Position = 0;

			if (Callback != NULL)
			{
				Callback(CallbackData);
			}
		}

		if (Events == NULL)
		{ // No more events. Just return something to keep the song playing
		  // while we wait for more to be submitted.
			return int(Division);
		}

		delay = *(uint32_t *)(Events->lpData + Position);
	}
	return delay;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: ServiceStream
//
//==========================================================================

bool SoftSynthMIDIDevice::ServiceStream (void *buff, int numbytes)
{
	float *samples = (float *)buff;
	float *samples1;
	int numsamples = numbytes / sizeof(float) / 2;
	bool prev_ended = false;
	bool res = true;

	samples1 = samples;
	memset(buff, 0, numbytes);

	std::lock_guard<std::mutex> lock(CritSec);
	while (Events != NULL && numsamples > 0)
	{
		double ticky = NextTickIn;
		int tick_in = int(NextTickIn);
		int samplesleft = MIN(numsamples, tick_in);

		if (samplesleft > 0)
		{
			ComputeOutput(samples1, samplesleft);
			assert(NextTickIn == ticky);
			NextTickIn -= samplesleft;
			assert(NextTickIn >= 0);
			numsamples -= samplesleft;
			samples1 += samplesleft * 2;
		}
		
		if (NextTickIn < 1)
		{
			int next = PlayTick();
			assert(next >= 0);
			if (next == 0)
			{ // end of song
				if (numsamples > 0)
				{
					ComputeOutput(samples1, numsamples);
				}
				res = false;
				break;
			}
			else
			{
				NextTickIn += SamplesPerTick * next;
				assert(NextTickIn >= 0);
			}
		}
	}

	if (Events == NULL)
	{
		res = false;
	}
	return res;
}

//==========================================================================
//
// SoftSynthMIDIDevice :: FillStream								static
//
//==========================================================================

bool SoftSynthMIDIDevice::FillStream(SoundStream *stream, void *buff, int len, void *userdata)
{
	SoftSynthMIDIDevice *device = (SoftSynthMIDIDevice *)userdata;
	return device->ServiceStream(buff, len);
}
