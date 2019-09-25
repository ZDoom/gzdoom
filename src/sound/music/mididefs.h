#pragma once

enum
{
	MAX_MIDI_EVENTS = 128
};

inline constexpr uint8_t MEVENT_EVENTTYPE(uint32_t x) { return ((uint8_t)((x) >> 24)); }
inline constexpr uint32_t MEVENT_EVENTPARM(uint32_t x) { return ((x) & 0xffffff); }

// These constants must match the corresponding values of the Windows headers
// to avoid readjustment in the native Windows device's playback functions 
// and should not be changed.
enum
{
	MIDIDEV_MIDIPORT = 1,
	MIDIDEV_SYNTH,
	MIDIDEV_SQSYNTH,
	MIDIDEV_FMSYNTH,
	MIDIDEV_MAPPER,
	MIDIDEV_WAVETABLE,
	MIDIDEV_SWSYNTH
};

enum : uint8_t
{
	MEVENT_TEMPO = 1,
	MEVENT_NOP = 2,
	MEVENT_LONGMSG = 128,
};

