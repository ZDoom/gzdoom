#pragma once

#include <stdint.h>

enum
{
	MAX_MIDI_EVENTS = 128
};

inline constexpr uint8_t MEVENT_EVENTTYPE(uint32_t x) { return ((uint8_t)((x) >> 24)); }
inline constexpr uint32_t MEVENT_EVENTPARM(uint32_t x) { return ((x) & 0xffffff); }

// These constants must match the corresponding values of the Windows headers
// to avoid readjustment in the native Windows device's playback functions 
// and should not be changed.
enum EMidiDeviceClass
{
	MIDIDEV_MIDIPORT = 1,
	MIDIDEV_SYNTH,
	MIDIDEV_SQSYNTH,
	MIDIDEV_FMSYNTH,
	MIDIDEV_MAPPER,
	MIDIDEV_WAVETABLE,
	MIDIDEV_SWSYNTH
};

enum EMIDIType
{
	MIDI_NOTMIDI,
	MIDI_MIDI,
	MIDI_HMI,
	MIDI_XMI,
	MIDI_MUS
};

enum EMidiEvent : uint8_t
{
	MEVENT_TEMPO = 1,
	MEVENT_NOP = 2,
	MEVENT_LONGMSG = 128,
};

enum EMidiDevice
{
	MDEV_DEFAULT = -1,
	MDEV_MMAPI = 0,
	MDEV_OPL = 1,
	MDEV_SNDSYS = 2,
	MDEV_TIMIDITY = 3,
	MDEV_FLUIDSYNTH = 4,
	MDEV_GUS = 5,
	MDEV_WILDMIDI = 6,
	MDEV_ADL = 7,
	MDEV_OPN = 8,

	MDEV_COUNT
};

enum ESoundFontTypes
{
    SF_SF2 = 1,
    SF_GUS = 2,
    SF_WOPL = 4,
    SF_WOPN = 8
};


struct SoundStreamInfo
{
	int mBufferSize;	// If mBufferSize is 0, the song doesn't use streaming but plays through a different interface. 
	int mSampleRate;
	int mNumChannels;	// If mNumChannels is negative, 16 bit integer format is used instead of floating point.
};

#ifndef MAKE_ID
#ifndef __BIG_ENDIAN__
#define MAKE_ID(a,b,c,d)	((uint32_t)((a)|((b)<<8)|((c)<<16)|((d)<<24)))
#else
#define MAKE_ID(a,b,c,d)	((uint32_t)((d)|((c)<<8)|((b)<<16)|((a)<<24)))
#endif
#endif

