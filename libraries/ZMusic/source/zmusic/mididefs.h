#pragma once

#include <stdint.h>

enum
{
	MAX_MIDI_EVENTS = 128
};

inline constexpr uint8_t MEVENT_EVENTTYPE(uint32_t x) { return ((uint8_t)((x) >> 24)); }
inline constexpr uint32_t MEVENT_EVENTPARM(uint32_t x) { return ((x) & 0xffffff); }

enum EMidiEvent : uint8_t
{
	MEVENT_TEMPO = 1,
	MEVENT_NOP = 2,
	MEVENT_LONGMSG = 128,
};

#ifndef MAKE_ID
#ifndef __BIG_ENDIAN__
#define MAKE_ID(a,b,c,d)	((uint32_t)((a)|((b)<<8)|((c)<<16)|((d)<<24)))
#else
#define MAKE_ID(a,b,c,d)	((uint32_t)((d)|((c)<<8)|((b)<<16)|((a)<<24)))
#endif
#endif

