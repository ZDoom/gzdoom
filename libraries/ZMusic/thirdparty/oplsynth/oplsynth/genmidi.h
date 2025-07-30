#pragma once
#include <stdint.h>

#if defined(__GNUC__)
// With versions of GCC newer than 4.2, it appears it was determined that the
// cost of an unaligned pointer on PPC was high enough to add padding to the
// end of packed structs.  For whatever reason __packed__ and pragma pack are
// handled differently in this regard. Note that this only needs to be applied
// to types which are used in arrays or sizeof is needed. This also prevents
// code from taking references to the struct members.
#define FORCE_PACKED __attribute__((__packed__))
#else
#define FORCE_PACKED
#endif

#pragma pack(push, 1)
struct genmidi_op_t
{
	uint8_t tremolo;
	uint8_t attack;
	uint8_t sustain;
	uint8_t waveform;
	uint8_t scale;
	uint8_t level;
} FORCE_PACKED;

struct GenMidiVoice
{
	genmidi_op_t modulator;
	uint8_t feedback;
	genmidi_op_t carrier;
	uint8_t unused;
	int16_t base_note_offset;
} FORCE_PACKED;


struct GenMidiInstrument
{
	uint16_t flags;
	uint8_t fine_tuning;
	uint8_t fixed_note;
	GenMidiVoice voices[2];
} FORCE_PACKED;

#pragma pack(pop)

enum
{
	GENMIDI_FLAG_FIXED = 0x0001,         /* fixed pitch */
	GENMIDI_FLAG_2VOICE = 0x0004         /* double voice (OPL3) */
};

enum
{
	GENMIDI_NUM_INSTRS = 128,
	GENMIDI_FIST_PERCUSSION = 35,
	GENMIDI_NUM_PERCUSSION = 47,
	GENMIDI_NUM_TOTAL = GENMIDI_NUM_INSTRS + GENMIDI_NUM_PERCUSSION
};

