/*
** Provides an ALSA implementation of a MIDI output device.
**
**---------------------------------------------------------------------------
** Copyright 2008-2010 Randy Heit
** Copyright 2020 Petr Mrazek
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

#if defined __linux__ && defined HAVE_SYSTEM_MIDI

#include <algorithm>
#include <memory>
#include <assert.h>
#include <thread>
#include <pthread.h>
#include <atomic>
#include <cstring>

#include "mididevice.h"
#include "zmusic/m_swap.h"
#include "zmusic/mus2midi.h"

#include "music_alsa_state.h"
#include <alsa/asoundlib.h>

namespace {

class AlsaMIDIDevice : public MIDIDevice
{
public:
	AlsaMIDIDevice(int dev_id);
	~AlsaMIDIDevice();
	int Open() override;
	void Close() override;
	bool IsOpen() const override;
	int GetTechnology() const override;
	int SetTempo(int tempo) override;
	int SetTimeDiv(int timediv) override;
	int StreamOut(MidiHeader *data) override;
	int StreamOutSync(MidiHeader *data) override;
	int Resume() override;
	void Stop() override;

	bool FakeVolume() override {
		// Not sure if we even can control the volume this way with Alsa, so make it fake.
		return true;
	};

	bool Pause(bool paused) override;
	void InitPlayback() override;
	bool Update() override;
	void PrecacheInstruments(const uint16_t *instruments, int count) override {}

	bool CanHandleSysex() const override
	{
		// Assume we can, let Alsa sort it out. We do not truly have full control.
		return true;
	}

	void HandleTempoChange(int tick, int tempo);
	void HandleEvent(int tick, int status, int parm1, int parm2);
	void HandleLongEvent(int tick, const uint8_t *data, int len);
	void SendStopEvents();
	void PumpEvents();


protected:
	AlsaSequencer &sequencer;

	MidiHeader *Events = nullptr;
	bool Started = false;
	uint32_t Position = 0;

	const static int IntendedPortId = 0;
	bool Connected = false;
	int PortId = -1;
	int QueueId = -1;

	int DestinationClientId;
	int DestinationPortId;

	int Tempo = 480000;
	int TimeDiv = 480;

	std::thread PlayerThread;
	std::atomic<bool> Exit;
};

}

AlsaMIDIDevice::AlsaMIDIDevice(int dev_id) : sequencer(AlsaSequencer::Get())
{
	auto & internalDevices = sequencer.GetInternalDevices();
	auto & device = internalDevices.at(dev_id);
	DestinationClientId = device.ClientID;
	DestinationPortId = device.PortNumber;
}

AlsaMIDIDevice::~AlsaMIDIDevice()
{
	Close();
}

int AlsaMIDIDevice::Open()
{
	if (!sequencer.IsOpen()) {
		return 1;
	}

	if(PortId < 0)
	{
		snd_seq_port_info_t *pinfo;
		snd_seq_port_info_alloca(&pinfo);

		snd_seq_port_info_set_port(pinfo, IntendedPortId);
		snd_seq_port_info_set_port_specified(pinfo, 1);

		snd_seq_port_info_set_name(pinfo, "GZDoom Music");

		snd_seq_port_info_set_capability(pinfo, 0);
		snd_seq_port_info_set_type(pinfo, SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

		int err = 0;
		err = snd_seq_create_port(sequencer.handle, pinfo);
		PortId = IntendedPortId;
	}

	if (QueueId < 0)
	{
		QueueId = snd_seq_alloc_named_queue(sequencer.handle, "GZDoom Queue");
	}

	if (!Connected) {
		Connected = (snd_seq_connect_to(sequencer.handle, PortId, DestinationClientId, DestinationPortId) == 0);
	}
	return 0;
}

void AlsaMIDIDevice::Close()
{
	if(Connected) {
		snd_seq_disconnect_to(sequencer.handle, PortId, DestinationClientId, DestinationPortId);
		Connected = false;
	}
	if(QueueId >= 0) {
		snd_seq_free_queue(sequencer.handle, QueueId);
		QueueId = -1;
	}
	if(PortId >= 0) {
		snd_seq_delete_port(sequencer.handle, PortId);
		PortId = -1;
	}
}

bool AlsaMIDIDevice::IsOpen() const
{
	return Connected;
}

int AlsaMIDIDevice::GetTechnology() const
{
	// TODO: implement properly, for now assume everything is an external MIDI device
	return MIDIDEV_MIDIPORT;
}

int AlsaMIDIDevice::SetTempo(int tempo)
{
	Tempo = tempo;
	return 0;
}

int AlsaMIDIDevice::SetTimeDiv(int timediv)
{
	TimeDiv = timediv;
	return 0;
}

void AlsaMIDIDevice::HandleEvent(int tick, int status, int parm1, int parm2)
{
	int command = status & 0xF0;
	int channel = status & 0x0F;

	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_source(&ev, PortId);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_schedule_tick(&ev, QueueId, false, tick);

	switch (command)
	{
	case MIDI_NOTEOFF:
		snd_seq_ev_set_noteoff(&ev, channel, parm1, parm2);
		break;

	case MIDI_NOTEON:
		snd_seq_ev_set_noteon(&ev, channel, parm1, parm2);
		break;

	case MIDI_POLYPRESS:
		// FIXME: Seems to be missing in the Alsa sequencer implementation
		return;

	case MIDI_CTRLCHANGE:
		snd_seq_ev_set_controller(&ev, channel, parm1, parm2);
		break;

	case MIDI_PRGMCHANGE:
		snd_seq_ev_set_pgmchange(&ev, channel, parm1);
		break;

	case MIDI_CHANPRESS:
		snd_seq_ev_set_chanpress(&ev, channel, parm1);
		break;

	case MIDI_PITCHBEND: {
		long bend = ((long)parm1 + (long)(parm2 << 7)) - 0x2000;
		snd_seq_ev_set_pitchbend(&ev, channel, bend);
		break;
	}

	default:
		return;
	}
	snd_seq_event_output(sequencer.handle, &ev);
}

void AlsaMIDIDevice::HandleLongEvent(int tick, const uint8_t *data, int len)
{
	// SysEx messages...
	if (len > 1 && (data[0] == 0xF0 || data[0] == 0xF7))
	{
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		snd_seq_ev_set_source(&ev, PortId);
		snd_seq_ev_set_subs(&ev);
		snd_seq_ev_schedule_tick(&ev, QueueId, false, tick);
		snd_seq_ev_set_sysex(&ev, len, (void *)data);
		snd_seq_event_output(sequencer.handle, &ev);
	}
}

void AlsaMIDIDevice::HandleTempoChange(int tick, int tempo) {
	if(Tempo != tempo) {
		Tempo = tempo;
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		snd_seq_ev_set_source(&ev, PortId);
		snd_seq_ev_set_subs(&ev);
		snd_seq_ev_schedule_tick(&ev, QueueId, false, tick);
		snd_seq_change_queue_tempo(sequencer.handle, QueueId, Tempo, &ev);
		snd_seq_event_output(sequencer.handle, &ev);
	}
}

void AlsaMIDIDevice::PumpEvents() {
	int error = 0;
	snd_seq_queue_tempo_t *tempo;
	snd_seq_queue_tempo_alloca(&tempo);
	snd_seq_queue_tempo_set_tempo(tempo, Tempo);
	snd_seq_queue_tempo_set_ppq(tempo, TimeDiv);
	error = snd_seq_set_queue_tempo(sequencer.handle, QueueId, tempo);

	snd_seq_start_queue(sequencer.handle, QueueId, NULL);
	error = snd_seq_drain_output(sequencer.handle);

	int running_time = 0;
	while (!Exit) {
		if(!Events) {
			// NOTE: in practice, this is never reached. however, if it were, it would prevent crashes below.
			continue;
		}

		uint32_t *event = (uint32_t *)(Events->lpData + Position);
		int ticks = event[0];
		running_time += ticks;
		if (MEVENT_EVENTTYPE(event[2]) == MEVENT_TEMPO) {
			HandleTempoChange(running_time, MEVENT_EVENTPARM(event[2]));
		}
		else if (MEVENT_EVENTTYPE(event[2]) == MEVENT_LONGMSG) {
			HandleLongEvent(running_time, (uint8_t *)&event[3], MEVENT_EVENTPARM(event[2]));
		}
		else if (MEVENT_EVENTTYPE(event[2]) == 0) {
			// Short MIDI event
			int status = event[2] & 0xff;
			int parm1 = (event[2] >> 8) & 0x7f;
			int parm2 = (event[2] >> 16) & 0x7f;
			HandleEvent(running_time, status, parm1, parm2);
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
			snd_seq_drain_output(sequencer.handle);
			snd_seq_sync_output_queue(sequencer.handle);
		}
	}
	// Send stop events, just to be sure we don't end up with stuck notes
	{
		snd_seq_drop_output(sequencer.handle);
		SendStopEvents();
		// FIXME: attach to a timestamped event and make it go through the queue?
		snd_seq_stop_queue(sequencer.handle, QueueId, NULL);
		snd_seq_drain_output(sequencer.handle);
		snd_seq_sync_output_queue(sequencer.handle);
	}
}

void AlsaMIDIDevice::SendStopEvents() {
	// NOTE: for some reason, the midi streamer doesn't send us these.
	for (int channel = 0; channel < 16; ++channel)
	{
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		snd_seq_ev_set_source(&ev, PortId);
		snd_seq_ev_set_subs(&ev);
		snd_seq_ev_schedule_tick(&ev, QueueId, true, 0);
		snd_seq_ev_set_controller(&ev, channel, MIDI_CTL_ALL_NOTES_OFF, 0);
		snd_seq_event_output(sequencer.handle, &ev);
		snd_seq_ev_set_controller(&ev, channel, MIDI_CTL_RESET_CONTROLLERS, 0);
		snd_seq_event_output(sequencer.handle, &ev);
	}
	snd_seq_drain_output(sequencer.handle);
	snd_seq_sync_output_queue(sequencer.handle);
}

int AlsaMIDIDevice::Resume()
{
	if(!Connected) {
		return 1;
	}
	Exit = false;
	PlayerThread = std::thread(&AlsaMIDIDevice::PumpEvents, this);
	return 0;
}

void AlsaMIDIDevice::InitPlayback()
{
	Exit = false;
}

void AlsaMIDIDevice::Stop()
{
	/*
	 * NOTE: this is slow. Maybe we should just leave the thread be and let it asynchronously drain in the background.
	 */
	Exit = true;
	PlayerThread.join();
}

bool AlsaMIDIDevice::Pause(bool paused)
{
	// TODO: implement
	return false;
}


int AlsaMIDIDevice::StreamOut(MidiHeader *header)
{
	header->lpNext = NULL;
	if (Events == NULL)
	{
		Events = header;
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


int AlsaMIDIDevice::StreamOutSync(MidiHeader *header)
{
	return StreamOut(header);
}

bool AlsaMIDIDevice::Update()
{
	return true;
}

MIDIDevice *CreateAlsaMIDIDevice(int mididevice)
{
	return new AlsaMIDIDevice(mididevice);
}
#endif
