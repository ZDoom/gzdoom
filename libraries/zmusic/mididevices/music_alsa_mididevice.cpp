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

#include <thread>
#include <mutex>
#include <condition_variable>

#include "mididevice.h"
#include "zmusic/m_swap.h"
#include "zmusic/mus2midi.h"

#include "music_alsa_state.h"
#include <alsa/asoundlib.h>

namespace {

enum class EventType {
	Null,
	Delay,
	Action
};

struct EventState {
	int ticks = 0;
	snd_seq_event_t data;
	int size_of = 0;

	void Clear() {
		ticks = 0;
		snd_seq_ev_clear(&data);
		size_of = 0;
	}
};

class AlsaMIDIDevice : public MIDIDevice
{
public:
	AlsaMIDIDevice(int dev_id, int (*printfunc_)(const char *, ...));
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

	void SendStopEvents();
	void SetExit(bool exit);
	bool WaitForExit(std::chrono::microseconds usec, snd_seq_queue_status_t * status);
	EventType PullEvent(EventState & state);
	void PumpEvents();


protected:
	AlsaSequencer &sequencer;
	int (*printfunc)(const char*, ...);

	MidiHeader *Events = nullptr;
	bool Started = false;
	uint32_t Position = 0;

	const static int IntendedPortId = 0;
	bool Connected = false;
	int PortId = -1;
	int QueueId = -1;

	int DestinationClientId;
	int DestinationPortId;
	int Technology;

	int Tempo = 480000;
	int TimeDiv = 480;

	std::thread PlayerThread;
	bool Exit = false;
	std::mutex ExitLock;
	std::condition_variable ExitCond;
};

}

AlsaMIDIDevice::AlsaMIDIDevice(int dev_id, int (*printfunc_)(const char*, ...) = nullptr) : sequencer(AlsaSequencer::Get()), printfunc(printfunc_)
{
	auto & internalDevices = sequencer.GetInternalDevices();
	auto & device = internalDevices.at(dev_id);
	DestinationClientId = device.ClientID;
	DestinationPortId = device.PortNumber;
	Technology = device.GetDeviceClass();
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
	return Technology;
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

EventType AlsaMIDIDevice::PullEvent(EventState & state) {
	state.Clear();

	if(!Events) {
		Callback(CallbackData);
		if(!Events) {
			return EventType::Null;
		}
	}
	if (Position >= Events->dwBytesRecorded)
	{
		Events = Events->lpNext;
		Position = 0;

		if (Callback != NULL)
		{
			Callback(CallbackData);
		}
		if(!Events) {
			return EventType::Null;
		}
	}

	uint32_t *event = (uint32_t *)(Events->lpData + Position);
	state.ticks = event[0];

	// Advance to next event.
	if (event[2] < 0x80000000)
	{ // Short message
		state.size_of = 12;
	}
	else
	{ // Long message
		state.size_of = 12 + ((MEVENT_EVENTPARM(event[2]) + 3) & ~3);
	}

	if (MEVENT_EVENTTYPE(event[2]) == MEVENT_TEMPO) {
		int tempo = MEVENT_EVENTPARM(event[2]);
		if(Tempo != tempo) {
			Tempo = tempo;
			snd_seq_change_queue_tempo(sequencer.handle, QueueId, Tempo, &state.data);
			return EventType::Action;
		}
	}
	else if (MEVENT_EVENTTYPE(event[2]) == MEVENT_LONGMSG) {
		// SysEx messages...
		uint8_t * data = (uint8_t *)&event[3];
		int len = MEVENT_EVENTPARM(event[2]);
		if (len > 1 && (data[0] == 0xF0 || data[0] == 0xF7))
		{
			snd_seq_ev_set_sysex(&state.data, len, (void *)data);
			return EventType::Action;
		}
	}
	else if (MEVENT_EVENTTYPE(event[2]) == 0) {
		// Short MIDI event
		int command = event[2] & 0xF0;
		int channel = event[2] & 0x0F;
		int parm1 = (event[2] >> 8) & 0x7f;
		int parm2 = (event[2] >> 16) & 0x7f;
		switch (command)
		{
		case MIDI_NOTEOFF:
			snd_seq_ev_set_noteoff(&state.data, channel, parm1, parm2);
			return EventType::Action;

		case MIDI_NOTEON:
			snd_seq_ev_set_noteon(&state.data, channel, parm1, parm2);
			return EventType::Action;

		case MIDI_POLYPRESS:
			// FIXME: Seems to be missing in the Alsa sequencer implementation
			break;

		case MIDI_CTRLCHANGE:
			snd_seq_ev_set_controller(&state.data, channel, parm1, parm2);
			return EventType::Action;

		case MIDI_PRGMCHANGE:
			snd_seq_ev_set_pgmchange(&state.data, channel, parm1);
			return EventType::Action;

		case MIDI_CHANPRESS:
			snd_seq_ev_set_chanpress(&state.data, channel, parm1);
			return EventType::Action;

		case MIDI_PITCHBEND: {
			long bend = ((long)parm1 + (long)(parm2 << 7)) - 0x2000;
			snd_seq_ev_set_pitchbend(&state.data, channel, bend);
			return EventType::Action;
		}

		default:
			break;
		}
	}
	// We didn't really recognize the event, treat it as a delay
	return EventType::Delay;
}

void AlsaMIDIDevice::SetExit(bool exit) {
	std::unique_lock<std::mutex> lock(ExitLock);
	if(exit != Exit) {
		Exit = exit;
		ExitCond.notify_all();
	}
}

bool AlsaMIDIDevice::WaitForExit(std::chrono::microseconds usec, snd_seq_queue_status_t * status) {
	std::unique_lock<std::mutex> lock(ExitLock);
	if(Exit) {
		return true;
	}
	ExitCond.wait_for(lock, usec);
	if(Exit) {
		return true;
	}
	snd_seq_get_queue_status(sequencer.handle, QueueId, status);
	return false;
}

/*
 * Pumps events from the input to the output in a worker thread.
 * It tries to keep the amount of events (time-wise) in the ALSA sequencer queue to be between 40 and 80ms by sleeping where necessary.
 * This means Alsa can play them safely without running out of things to do, and we have good control over the events themselves (volume, pause, etc.).
 */
void AlsaMIDIDevice::PumpEvents() {
	const std::chrono::microseconds pump_step(40000);

	// TODO: fill in error handling throughout this.
	snd_seq_queue_tempo_t *tempo;
	snd_seq_queue_tempo_alloca(&tempo);
	snd_seq_queue_tempo_set_tempo(tempo, Tempo);
	snd_seq_queue_tempo_set_ppq(tempo, TimeDiv);
	snd_seq_set_queue_tempo(sequencer.handle, QueueId, tempo);

	snd_seq_start_queue(sequencer.handle, QueueId, NULL);
	snd_seq_drain_output(sequencer.handle);

	int buffer_ticks = 0;
	EventState event;

	snd_seq_queue_status_t *status;
	snd_seq_queue_status_malloc(&status);

	while (true) {
		auto type = PullEvent(event);
		// if we reach the end of events, await our doom at a steady rate while looking for more events
		if(type == EventType::Null) {
			if(WaitForExit(pump_step, status)) {
				break;
			}
			continue;
		}

		// chomp delays as they come...
		if(type == EventType::Delay) {
			buffer_ticks += event.ticks;
			Position += event.size_of;
			continue;
		}

		// Figure out if we should sleep (the event is too far in the future for us to care), and for how long
		int next_event_tick = buffer_ticks + event.ticks;
		int queue_tick = snd_seq_queue_status_get_tick_time(status);
		int tick_delta = next_event_tick - queue_tick;
		auto usecs = std::chrono::microseconds(tick_delta * Tempo / TimeDiv);
		auto schedule_time = std::max(std::chrono::microseconds(0), usecs - pump_step);
		if(schedule_time >= pump_step) {
			if(WaitForExit(schedule_time, status)) {
				break;
			}
			continue;
		}
		if (tick_delta < 0) {
			if(printfunc) {
				printfunc("Alsa sequencer underrun: %d ticks!\n", tick_delta);
			}
		}

		// We found an event worthy of sending to the sequencer
		snd_seq_ev_set_source(&event.data, PortId);
		snd_seq_ev_set_subs(&event.data);
		snd_seq_ev_schedule_tick(&event.data, QueueId, false, buffer_ticks + event.ticks);
		int result = snd_seq_event_output(sequencer.handle, &event.data);
		if(result < 0) {
			if(printfunc) {
				printfunc("Alsa sequencer did not accept event: error %d!\n", result);
			}
			if(WaitForExit(pump_step, status)) {
				break;
			}
			continue;
		}
		buffer_ticks += event.ticks;
		Position += event.size_of;
		snd_seq_drain_output(sequencer.handle);
	}

	snd_seq_queue_status_free(status);
	snd_seq_drop_output(sequencer.handle);
	// FIXME: the event source should give use these, but it doesn't.
	{
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
	snd_seq_sync_output_queue(sequencer.handle);
	snd_seq_stop_queue(sequencer.handle, QueueId, NULL);
	snd_seq_drain_output(sequencer.handle);
}


int AlsaMIDIDevice::Resume()
{
	if(!Connected) {
		return 1;
	}
	SetExit(false);
	PlayerThread = std::thread(&AlsaMIDIDevice::PumpEvents, this);
	return 0;
}

void AlsaMIDIDevice::InitPlayback()
{
	SetExit(false);
}

void AlsaMIDIDevice::Stop()
{
	SetExit(true);
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
	return new AlsaMIDIDevice(mididevice, musicCallbacks.Alsa_MessageFunc);
}
#endif
