/*
** music_timidity_mididevice.cpp
** Provides access to TiMidity as a generic MIDI device.
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
#include "w_wad.h"
#include "v_text.h"
#include "timidity/timidity.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct FmtChunk
{
	DWORD ChunkID;
	DWORD ChunkLen;
	WORD  FormatTag;
	WORD  Channels;
	DWORD SamplesPerSec;
	DWORD AvgBytesPerSec;
	WORD  BlockAlign;
	WORD  BitsPerSample;
	WORD  ExtensionSize;
	WORD  ValidBitsPerSample;
	DWORD ChannelMask;
	DWORD SubFormatA;
	WORD  SubFormatB;
	WORD  SubFormatC;
	BYTE  SubFormatD[8];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR(Bool, timidity_watch, false, 0)

// CODE --------------------------------------------------------------------

//==========================================================================
//
// TimidityMIDIDevice Constructor
//
//==========================================================================

TimidityMIDIDevice::TimidityMIDIDevice()
{
	Stream = NULL;
	Tempo = 0;
	Division = 0;
	Events = NULL;
	Started = false;
	Renderer = NULL;
	Renderer = new Timidity::Renderer(GSnd->GetOutputRate());
}

//==========================================================================
//
// TimidityMIDIDevice Constructor with rate parameter
//
//==========================================================================

TimidityMIDIDevice::TimidityMIDIDevice(int rate)
{
	// Need to support multiple instances with different playback rates
	// before we can use this parameter.
	rate = (int)GSnd->GetOutputRate();
	Stream = NULL;
	Tempo = 0;
	Division = 0;
	Events = NULL;
	Started = false;
	Renderer = NULL;
	Renderer = new Timidity::Renderer((float)rate);
}

//==========================================================================
//
// TimidityMIDIDevice Destructor
//
//==========================================================================

TimidityMIDIDevice::~TimidityMIDIDevice()
{
	Close();
	if (Renderer != NULL)
	{
		delete Renderer;
	}
}

//==========================================================================
//
// TimidityMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int TimidityMIDIDevice::Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata)
{
	Stream = GSnd->CreateStream(FillStream, int(Renderer->rate / 2) * 4,
		SoundStream::Float, int(Renderer->rate), this);
	if (Stream == NULL)
	{
		return 2;
	}

	Callback = callback;
	CallbackData = userdata;
	Tempo = 500000;
	Division = 100;
	CalcTickRate();
	Renderer->Reset();
	return 0;
}

//==========================================================================
//
// TimidityMIDIDevice :: Close
//
//==========================================================================

void TimidityMIDIDevice::Close()
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
// TimidityMIDIDevice :: IsOpen
//
//==========================================================================

bool TimidityMIDIDevice::IsOpen() const
{
	return Stream != NULL;
}

//==========================================================================
//
// TimidityMIDIDevice :: GetTechnology
//
//==========================================================================

int TimidityMIDIDevice::GetTechnology() const
{
	return MOD_SWSYNTH;
}

//==========================================================================
//
// TimidityMIDIDevice :: SetTempo
//
//==========================================================================

int TimidityMIDIDevice::SetTempo(int tempo)
{
	Tempo = tempo;
	CalcTickRate();
	return 0;
}

//==========================================================================
//
// TimidityMIDIDevice :: SetTimeDiv
//
//==========================================================================

int TimidityMIDIDevice::SetTimeDiv(int timediv)
{
	Division = timediv;
	CalcTickRate();
	return 0;
}

//==========================================================================
//
// TimidityMIDIDevice :: CalcTickRate
//
// Tempo is the number of microseconds per quarter note.
// Division is the number of ticks per quarter note.
//
//==========================================================================

void TimidityMIDIDevice::CalcTickRate()
{
	SamplesPerTick = Renderer->rate / (1000000.0 / Tempo) / Division;
}

//==========================================================================
//
// TimidityMIDIDevice :: Resume
//
//==========================================================================

int TimidityMIDIDevice::Resume()
{
	if (!Started)
	{
		if (Stream->Play(true, 1/*timidity_mastervolume*/))
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
// TimidityMIDIDevice :: Stop
//
//==========================================================================

void TimidityMIDIDevice::Stop()
{
	if (Started)
	{
		Stream->Stop();
		Started = false;
	}
}

//==========================================================================
//
// TimidityMIDIDevice :: StreamOutSync
//
// This version is called from the main game thread and needs to
// synchronize with the player thread.
//
//==========================================================================

int TimidityMIDIDevice::StreamOutSync(MIDIHDR *header)
{
	CritSec.Enter();
	StreamOut(header);
	CritSec.Leave();
	return 0;
}

//==========================================================================
//
// TimidityMIDIDevice :: StreamOut
//
// This version is called from the player thread so does not need to
// arbitrate for access to the Events pointer.
//
//==========================================================================

int TimidityMIDIDevice::StreamOut(MIDIHDR *header)
{
	header->lpNext = NULL;
	if (Events == NULL)
	{
		Events = header;
		NextTickIn = SamplesPerTick * *(DWORD *)header->lpData;
		Position = 0;
	}
	else
	{
		MIDIHDR **p;

		for (p = &Events; *p != NULL; p = &(*p)->lpNext)
		{ }
		*p = header;
	}
	return 0;
}

//==========================================================================
//
// TimidityMIDIDevice :: PrepareHeader
//
//==========================================================================

int TimidityMIDIDevice::PrepareHeader(MIDIHDR *header)
{
	return 0;
}

//==========================================================================
//
// TimidityMIDIDevice :: UnprepareHeader
//
//==========================================================================

int TimidityMIDIDevice::UnprepareHeader(MIDIHDR *header)
{
	return 0;
}

//==========================================================================
//
// TimidityMIDIDevice :: FakeVolume
//
// Since the TiMidity output is rendered as a normal stream, its volume is
// controlled through the GSnd interface, not here.
//
//==========================================================================

bool TimidityMIDIDevice::FakeVolume()
{
	return false;
}

//==========================================================================
//
// TimidityMIDIDevice :: NeedThreadedCallabck
//
// OPL can service the callback directly rather than using a separate
// thread.
//
//==========================================================================

bool TimidityMIDIDevice::NeedThreadedCallback()
{
	return false;
}


//==========================================================================
//
// TimidityMIDIDevice :: TimidityVolumeChanged
//
//==========================================================================

void TimidityMIDIDevice::TimidityVolumeChanged()
{
	/*
	if (Stream != NULL)
	{
		Stream->SetVolume(timidity_mastervolume);
	}
	*/
}

//==========================================================================
//
// TimidityMIDIDevice :: Pause
//
//==========================================================================

bool TimidityMIDIDevice::Pause(bool paused)
{
	if (Stream != NULL)
	{
		return Stream->SetPaused(paused);
	}
	return true;
}

//==========================================================================
//
// TimidityMIDIDevice :: PrecacheInstruments
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
//==========================================================================

void TimidityMIDIDevice::PrecacheInstruments(const WORD *instruments, int count)
{
	for (int i = 0; i < count; ++i)
	{
		Renderer->MarkInstrument((instruments[i] >> 7) & 127, instruments[i] >> 14, instruments[i] & 127);
	}
	Renderer->load_missing_instruments();
}

//==========================================================================
//
// TimidityMIDIDevice :: PlayTick
//
// event[0] = delta time
// event[1] = unused
// event[2] = event
//
//==========================================================================

int TimidityMIDIDevice::PlayTick()
{
	DWORD delay = 0;

	while (delay == 0 && Events != NULL)
	{
		DWORD *event = (DWORD *)(Events->lpData + Position);
		if (MEVT_EVENTTYPE(event[2]) == MEVT_TEMPO)
		{
			SetTempo(MEVT_EVENTPARM(event[2]));
		}
		else if (MEVT_EVENTTYPE(event[2]) == MEVT_LONGMSG)
		{
			Renderer->HandleLongMessage((BYTE *)&event[3], MEVT_EVENTPARM(event[2]));
		}
		else if (MEVT_EVENTTYPE(event[2]) == 0)
		{ // Short MIDI event
			int status = event[2] & 0xff;
			int parm1 = (event[2] >> 8) & 0x7f;
			int parm2 = (event[2] >> 16) & 0x7f;
			Renderer->HandleEvent(status, parm1, parm2);

			if (timidity_watch)
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
#ifdef _WIN32
				char buffer[128];
				sprintf(buffer, "C%02d: %11s %3d %3d\n", (status & 15) + 1, commands[(status >> 4) & 7], parm1, parm2);
				OutputDebugString(buffer);
#else
				fprintf(stderr, "C%02d: %11s %3d %3d\n", (status & 15) + 1, commands[(status >> 4) & 7], parm1, parm2);
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
			Position += 12 + ((MEVT_EVENTPARM(event[2]) + 3) & ~3);
		}

		// Did we use up this buffer?
		if (Position >= Events->dwBytesRecorded)
		{
			Events = Events->lpNext;
			Position = 0;

			if (Callback != NULL)
			{
				Callback(MOM_DONE, CallbackData, 0, 0);
			}
		}

		if (Events == NULL)
		{ // No more events. Just return something to keep the song playing
		  // while we wait for more to be submitted.
			return int(Division);
		}

		delay = *(DWORD *)(Events->lpData + Position);
	}
	return delay;
}

//==========================================================================
//
// TimidityMIDIDevice :: ServiceStream
//
//==========================================================================

bool TimidityMIDIDevice::ServiceStream (void *buff, int numbytes)
{
	float *samples = (float *)buff;
	float *samples1;
	int numsamples = numbytes / sizeof(float) / 2;
	bool prev_ended = false;
	bool res = true;

	samples1 = samples;
	memset(buff, 0, numbytes);

	CritSec.Enter();
	while (Events != NULL && numsamples > 0)
	{
		double ticky = NextTickIn;
		int tick_in = int(NextTickIn);
		int samplesleft = MIN(numsamples, tick_in);

		if (samplesleft > 0)
		{
			Renderer->ComputeOutput(samples1, samplesleft);
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
					Renderer->ComputeOutput(samples1, numsamples);
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
	CritSec.Leave();
	return res;
}

//==========================================================================
//
// TimidityMIDIDevice :: FillStream									static
//
//==========================================================================

bool TimidityMIDIDevice::FillStream(SoundStream *stream, void *buff, int len, void *userdata)
{
	TimidityMIDIDevice *device = (TimidityMIDIDevice *)userdata;
	return device->ServiceStream(buff, len);
}

//==========================================================================
//
// TimidityMIDIDevice :: GetStats
//
//==========================================================================

FString TimidityMIDIDevice::GetStats()
{
	FString dots;
	FString out;
	int i, used;

	CritSec.Enter();
	for (i = used = 0; i < Renderer->voices; ++i)
	{
		switch (Renderer->voice[i].status)
		{
		case Timidity::VOICE_FREE:
			dots << TEXTCOLOR_PURPLE".";
			break;

		case Timidity::VOICE_ON:
			dots << TEXTCOLOR_GREEN;
			break;

		case Timidity::VOICE_SUSTAINED:
			dots << TEXTCOLOR_BLUE;
			break;

		case Timidity::VOICE_OFF:
			dots << TEXTCOLOR_ORANGE;
			break;

		case Timidity::VOICE_DIE:
			dots << TEXTCOLOR_RED;
			break;
		}
		if (Renderer->voice[i].status != Timidity::VOICE_FREE)
		{
			used++;
			if (Renderer->voice[i].envelope_increment == 0)
			{
				dots << TEXTCOLOR_BLUE"+";
			}
			else
			{
				dots << ('1' + Renderer->voice[i].envelope_stage);
			}
		}
	}
	CritSec.Leave();
	out.Format(TEXTCOLOR_YELLOW"%3d/%3d ", used, Renderer->voices);
	out += dots;
	if (Renderer->cut_notes | Renderer->lost_notes)
	{
		out.AppendFormat(TEXTCOLOR_RED" %d/%d", Renderer->cut_notes, Renderer->lost_notes);
	}
	return out;
}

//==========================================================================
//
// TimidityWaveWriterMIDIDevice Constructor
//
//==========================================================================

TimidityWaveWriterMIDIDevice::TimidityWaveWriterMIDIDevice(const char *filename, int rate)
{
	File = fopen(filename, "wb");
	if (File != NULL)
	{ // Write wave header
		DWORD work[3];
		FmtChunk fmt;

		work[0] = MAKE_ID('R','I','F','F');
		work[1] = 0;								// filled in later
		work[2] = MAKE_ID('W','A','V','E');
		if (3 != fwrite(work, 4, 3, File)) goto fail;

		fmt.ChunkID = MAKE_ID('f','m','t',' ');
		fmt.ChunkLen = LittleLong(sizeof(fmt) - 8);
		fmt.FormatTag = LittleShort(0xFFFE);		// WAVE_FORMAT_EXTENSIBLE
		fmt.Channels = LittleShort(2);
		fmt.SamplesPerSec = LittleLong((int)Renderer->rate);
		fmt.AvgBytesPerSec = LittleLong((int)Renderer->rate * 8);
		fmt.BlockAlign = LittleShort(8);
		fmt.BitsPerSample = LittleShort(32);
		fmt.ExtensionSize = LittleShort(2 + 4 + 16);
		fmt.ValidBitsPerSample = LittleShort(32);
		fmt.ChannelMask = LittleLong(3);
		fmt.SubFormatA = LittleLong(0x00000003);	// Set subformat to KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
		fmt.SubFormatB = LittleShort(0x0000);
		fmt.SubFormatC = LittleShort(0x0010);
		fmt.SubFormatD[0] = 0x80;
		fmt.SubFormatD[1] = 0x00;
		fmt.SubFormatD[2] = 0x00;
		fmt.SubFormatD[3] = 0xaa;
		fmt.SubFormatD[4] = 0x00;
		fmt.SubFormatD[5] = 0x38;
		fmt.SubFormatD[6] = 0x9b;
		fmt.SubFormatD[7] = 0x71;
		if (1 != fwrite(&fmt, sizeof(fmt), 1, File)) goto fail;

		work[0] = MAKE_ID('d','a','t','a');
		work[1] = 0;								// filled in later
		if (2 != fwrite(work, 4, 2, File)) goto fail;

		return;
fail:
		Printf("Failed to write %s: %s\n", filename, strerror(errno));
		fclose(File);
		File = NULL;
	}
}

//==========================================================================
//
// TimidityWaveWriterMIDIDevice Destructor
//
//==========================================================================

TimidityWaveWriterMIDIDevice::~TimidityWaveWriterMIDIDevice()
{
	if (File != NULL)
	{
		long pos = ftell(File);
		DWORD size;

		// data chunk size
		size = LittleLong(pos - 8);
		if (0 == fseek(File, 4, SEEK_SET))
		{
			if (1 == fwrite(&size, 4, 1, File))
			{
				size = LittleLong(pos - 12 - sizeof(FmtChunk) - 8);
				if (0 == fseek(File, 4 + sizeof(FmtChunk) + 4, SEEK_CUR))
				{
					if (1 == fwrite(&size, 4, 1, File))
					{
						fclose(File);
						return;
					}
				}
			}
		}
		Printf("Could not finish writing wave file: %s\n", strerror(errno));
		fclose(File);
	}
}

//==========================================================================
//
// TimidityWaveWriterMIDIDevice :: Resume
//
//==========================================================================

int TimidityWaveWriterMIDIDevice::Resume()
{
	float writebuffer[4096];

	while (ServiceStream(writebuffer, sizeof(writebuffer)))
	{
		if (fwrite(writebuffer, sizeof(writebuffer), 1, File) != 1)
		{
			Printf("Could not write entire wave file: %s\n", strerror(errno));
			return 1;
		}
	}
	return 0;
}

//==========================================================================
//
// TimidityWaveWriterMIDIDevice Stop
//
//==========================================================================

void TimidityWaveWriterMIDIDevice::Stop()
{
}
