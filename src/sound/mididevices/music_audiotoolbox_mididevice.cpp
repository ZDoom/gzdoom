//
//---------------------------------------------------------------------------
//
// MIDI device for Apple's macOS using AudioToolbox framework
// Copyright(C) 2017 Alexey Lysiuk
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

// Implementation is loosely based on macOS native MIDI support from SDL_mixer

#ifdef __APPLE__

#include <AudioToolbox/AudioToolbox.h>
#include "i_musicinterns.h"
#include "templates.h"

// AudioToolbox implementation of a MIDI output device ----------------------

class AudioToolboxMIDIDevice : public MIDIDevice
{
public:
	virtual int Open(MidiCallback, void *userData) override;
	virtual void Close() override;
	virtual bool IsOpen() const override;
	virtual int GetTechnology() const override;
	virtual int SetTempo(int tempo) override;
	virtual int SetTimeDiv(int timediv) override;
	virtual int StreamOut(MidiHeader *data) override;
	virtual int StreamOutSync(MidiHeader *data) override;
	virtual int Resume() override;
	virtual void Stop() override;
	virtual int PrepareHeader(MidiHeader* data) override;
	virtual bool FakeVolume() override { return true; }
	virtual bool Pause(bool paused) override;
	virtual bool Preprocess(MIDIStreamer *song, bool looping) override;

private:
	MusicPlayer m_player = nullptr;
	MusicSequence m_sequence = nullptr;
	AudioUnit m_audioUnit = nullptr;
	CFRunLoopTimerRef m_timer = nullptr;
	MusicTimeStamp m_length = 0;

	MidiCallback Callback;
	Callback m_callback = nullptr;
	void* m_userData = nullptr;

	static void TimerCallback(CFRunLoopTimerRef timer, void* info);
};



#define AT_MIDI_CHECK_ERROR(CALL,...)                              \
{                                                                  \
	const OSStatus result = CALL;                                  \
	if (noErr != result)                                           \
	{                                                              \
		DPrintf(DMSG_ERROR,                                        \
			"Failed with error 0x%08X at " __FILE__ ":%d:\n> %s",  \
			result, __LINE__, #CALL);                              \
		return __VA_ARGS__;                                        \
	}                                                              \
}

int AudioToolboxMIDIDevice::Open(MidiCallback callback, void *userData)
{
	AT_MIDI_CHECK_ERROR(NewMusicPlayer(&m_player), false);
	AT_MIDI_CHECK_ERROR(NewMusicSequence(&m_sequence), false);
	AT_MIDI_CHECK_ERROR(MusicPlayerSetSequence(m_player, m_sequence), false);

	CFRunLoopTimerContext context = { 0, this, nullptr, nullptr, nullptr };
	m_timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(), 0.1, 0, 0, TimerCallback, &context);

	if (nullptr == m_timer)
	{
		DPrintf(DMSG_ERROR, "Failed with create timer for MIDI playback");
		return 1;
	}

	CFRunLoopAddTimer(CFRunLoopGetMain(), m_timer, kCFRunLoopDefaultMode);

	m_callback = callback;
	m_userData = userData;

	return 0;
}

void AudioToolboxMIDIDevice::Close()
{
	m_length = 0;
	m_audioUnit = nullptr;

	m_callback = nullptr;
	m_userData = nullptr;

	if (nullptr != m_timer)
	{
		CFRunLoopRemoveTimer(CFRunLoopGetMain(), m_timer, kCFRunLoopDefaultMode);

		CFRelease(m_timer);
		m_timer = nullptr;
	}

	if (nullptr != m_sequence)
	{
		DisposeMusicSequence(m_sequence);
		m_sequence = nullptr;
	}

	if (nullptr != m_player)
	{
		DisposeMusicPlayer(m_player);
		m_player = nullptr;
	}
}

bool AudioToolboxMIDIDevice::IsOpen() const
{
	return nullptr != m_player
		&& nullptr != m_sequence
		&& nullptr != m_timer;
}

int AudioToolboxMIDIDevice::GetTechnology() const
{
	return MIDIDEV_SWSYNTH;
}

int AudioToolboxMIDIDevice::SetTempo(int tempo)
{
	return 0;
}

int AudioToolboxMIDIDevice::SetTimeDiv(int timediv)
{
	return 0;
}

int AudioToolboxMIDIDevice::StreamOut(MidiHeader* data)
{
	return 0;
}

int AudioToolboxMIDIDevice::StreamOutSync(MidiHeader* data)
{
	return 0;
}

int AudioToolboxMIDIDevice::Resume()
{
	AT_MIDI_CHECK_ERROR(MusicPlayerSetTime(m_player, 0), false);
	AT_MIDI_CHECK_ERROR(MusicPlayerPreroll(m_player), false);

	if (nullptr == m_audioUnit)
	{
		AUGraph graph;
		AT_MIDI_CHECK_ERROR(MusicSequenceGetAUGraph(m_sequence, &graph), false);

		UInt32 nodecount;
		AT_MIDI_CHECK_ERROR(AUGraphGetNodeCount(graph, &nodecount), false);

		for (UInt32 i = 0; i < nodecount; ++i)
		{
			AUNode node;
			AT_MIDI_CHECK_ERROR(AUGraphGetIndNode(graph, i, &node), false);

			AudioComponentDescription desc = {};
			AudioUnit audioUnit = nullptr;
			AT_MIDI_CHECK_ERROR(AUGraphNodeInfo(graph, node, &desc, &audioUnit), false);

			if (   kAudioUnitType_Output           != desc.componentType
				|| kAudioUnitSubType_DefaultOutput != desc.componentSubType)
			{
				continue;
			}

			const float volume = clamp<float>(snd_musicvolume * relative_volume, 0.f, 1.f);
			AT_MIDI_CHECK_ERROR(AudioUnitSetParameter(audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, volume, 0), false);

			m_audioUnit = audioUnit;
			break;
		}
	}

	AT_MIDI_CHECK_ERROR(MusicPlayerStart(m_player), false);

	return 0;
}

void AudioToolboxMIDIDevice::Stop()
{
	AT_MIDI_CHECK_ERROR(MusicPlayerStop(m_player));
}

int AudioToolboxMIDIDevice::PrepareHeader(MidiHeader* data)
{
	MidiHeader* events = data;
	uint32_t position = 0;

	while (nullptr != events)
	{
		uint32_t* const event = reinterpret_cast<uint32_t*>(events->lpData + position);
		const uint32_t message = event[2];

		if (0 == MEVENT_EVENTTYPE(message))
		{
			static const uint32_t VOLUME_CHANGE_EVENT = 7;

			const uint32_t status =  message        & 0xFF;
			const uint32_t param1 = (message >>  8) & 0x7F;
			const uint32_t param2 = (message >> 16) & 0x7F;

			if (nullptr != m_audioUnit && MIDI_CTRLCHANGE == status && VOLUME_CHANGE_EVENT == param1)
			{
				AT_MIDI_CHECK_ERROR(AudioUnitSetParameter(m_audioUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, param2 / 100.f, 0), false);
			}
		}

		// Advance to next event
		position += 12 + ( (message < 0x80000000)
			? 0
			: ((MEVENT_EVENTPARM(message) + 3) & ~3) );

		// Did we use up this buffer?
		if (position >= events->dwBytesRecorded)
		{
			events = events->lpNext;
			position = 0;
		}

		if (nullptr == events)
		{
			break;
		}
	}

	return 0;
}

bool AudioToolboxMIDIDevice::Pause(bool paused)
{
	return false;
}

static MusicTimeStamp GetSequenceLength(MusicSequence sequence)
{
	UInt32 trackCount;
	AT_MIDI_CHECK_ERROR(MusicSequenceGetTrackCount(sequence, &trackCount), 0);

	MusicTimeStamp result = 0;

	for (UInt32 i = 0; i < trackCount; ++i)
	{
		MusicTrack track;
		AT_MIDI_CHECK_ERROR(MusicSequenceGetIndTrack(sequence, i, &track), 0);

		MusicTimeStamp trackLength = 0;
		UInt32 trackLengthSize = sizeof trackLength;

		AT_MIDI_CHECK_ERROR(MusicTrackGetProperty(track, kSequenceTrackProperty_TrackLength, &trackLength, &trackLengthSize), 0);

		if (result < trackLength)
		{
			result = trackLength;
		}
	}

	return result;
}

bool AudioToolboxMIDIDevice::Preprocess(MIDIStreamer* song, bool looping)
{
	assert(nullptr != song);

	TArray<uint8_t> midi;
	song->CreateSMF(midi, looping ? 0 : 1);

	CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, &midi[0], midi.Size(), kCFAllocatorNull);
	if (nullptr == data)
	{
		DPrintf(DMSG_ERROR, "Failed with create CFDataRef for MIDI song");
		return false;
	}

	AT_MIDI_CHECK_ERROR(MusicSequenceFileLoadData(m_sequence, data, kMusicSequenceFile_MIDIType, 0), CFRelease(data), false);

	CFRelease(data);

	m_length = GetSequenceLength(m_sequence);

	return true;
}

void AudioToolboxMIDIDevice::TimerCallback(CFRunLoopTimerRef timer, void* info)
{
	AudioToolboxMIDIDevice* const self = static_cast<AudioToolboxMIDIDevice*>(info);

	if (nullptr != self->m_callback)
	{
		self->m_callback(MIDI_DONE, self->m_userData);
	}

	MusicTimeStamp currentTime = 0;
	AT_MIDI_CHECK_ERROR(MusicPlayerGetTime(self->m_player, &currentTime));

	if (currentTime > self->m_length)
	{
		MusicPlayerSetTime(self->m_player, 0);
	}
}

#undef AT_MIDI_CHECK_ERROR

MIDIDevice *CreateAudioToolboxMIDTDevice(int mididevice)
{
	return new AudioToolboxMIDIDevice(mididevice);
}

#endif // __APPLE__
