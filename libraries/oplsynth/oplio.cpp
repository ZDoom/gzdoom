//-----------------------------------------------------------------------------
//
// Copyright 2002-2016 Randy Heit
// Copyright 2005-2014 Simon Howard 
// Copyright 2017 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// OPL IO interface. Partly built from the non-MusLib code in the old version
// plus some additions from Chocolate Doom.
//

#include <math.h>
#include <assert.h>
#include <algorithm>
#include <string.h>

#include "genmidi.h"
#include "oplio.h"
#include "opl.h"

const double HALF_PI = (3.14159265358979323846 * 0.5);

OPLio::~OPLio()
{
}

void OPLio::SetClockRate(double samples_per_tick)
{
}

void OPLio::WriteDelay(int ticks)
{
}

//----------------------------------------------------------------------------
//
// Initialize OPL emulator
//
//----------------------------------------------------------------------------

int OPLio::Init(int core, uint32_t numchips, bool stereo, bool initopl3)
{
	assert(numchips >= 1 && numchips <= OPL_NUM_VOICES);
	uint32_t i;
	IsOPL3 = (core == 1 || core == 2 || core == 3);

	using CoreInit = OPLEmul* (*)(bool);
	static CoreInit inits[] =
	{
		YM3812Create,
		DBOPLCreate,
		JavaOPLCreate,
		NukedOPL3Create,
	};

	if (core < 0) core = 0;
	if (core > 3) core = 3;

	memset(chips, 0, sizeof(chips));
	if (IsOPL3)
	{
		numchips = (numchips + 1) >> 1;
	}
	for (i = 0; i < numchips; ++i)
	{
		OPLEmul* chip = inits[core](stereo);
		if (chip == NULL)
		{
			break;
		}
		chips[i] = chip;
	}
	NumChips = i;
	NumChannels = i * (IsOPL3 ? OPL3_NUM_VOICES : OPL_NUM_VOICES);
	WriteInitState(initopl3);
	return i;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void OPLio::WriteInitState(bool initopl3)
{
	for (uint32_t k = 0; k < NumChips; ++k)
	{
		int chip = k << (int)IsOPL3;
		if (IsOPL3 && initopl3)
		{
			WriteRegister(chip, OPL_REG_OPL3_ENABLE, 1);
			WriteRegister(chip, OPL_REG_4OPMODE_ENABLE, 0);
		}
		WriteRegister(chip, OPL_REG_WAVEFORM_ENABLE, WAVEFORM_ENABLED);
		WriteRegister(chip, OPL_REG_PERCUSSION_MODE, 0);	// should be the default, but cannot verify for some of the cores.
	}

	// Reset all channels.
	for (uint32_t k = 0; k < NumChannels; k++)
	{
		MuteChannel(k);
		WriteValue(OPL_REGS_FREQ_2, k, 0);
	}
}


//----------------------------------------------------------------------------
//
// Deinitialize emulator before shutdown
//
//----------------------------------------------------------------------------

void OPLio::Reset(void)
{
	for (auto &c : chips)
	{
		if (c != nullptr)
		{
			delete c;
			c = nullptr;
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void OPLio::WriteRegister(int chipnum, uint32_t reg, uint8_t data)
{
	if (IsOPL3)
	{
		reg |= (chipnum & 1) << 8;
		chipnum >>= 1;
	}
	if (chips[chipnum] != nullptr)
	{
		chips[chipnum]->WriteReg(reg, data);
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void OPLio::WriteValue(uint32_t regbase, uint32_t channel, uint8_t value)
{
	WriteRegister (channel / OPL_NUM_VOICES, regbase + (channel % OPL_NUM_VOICES), value);
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

static const int voice_operators[OPL_NUM_VOICES] = { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12 };

void OPLio::WriteOperator(uint32_t regbase, uint32_t channel, int index, uint8_t data2)
{
	WriteRegister(channel / OPL_NUM_VOICES, regbase + voice_operators[channel % OPL_NUM_VOICES] + 3*index, data2);
}

//----------------------------------------------------------------------------
//
// Write frequency/octave/keyon data to a channel
//
// [RH] This is totally different from the original MUS library code
// but matches exactly what DMX does. I haven't a clue why there are 284
// special bytes at the beginning of the table for the first few notes.
// That last byte in the table doesn't look right, either, but that's what
// it really is.
//
//----------------------------------------------------------------------------

static const uint16_t frequencies[] = {	// (this is the table from Chocolate Doom, which contains the same values as ZDoom's original one but is better formatted.

    0x133, 0x133, 0x134, 0x134, 0x135, 0x136, 0x136, 0x137,   // -1
    0x137, 0x138, 0x138, 0x139, 0x139, 0x13a, 0x13b, 0x13b,
    0x13c, 0x13c, 0x13d, 0x13d, 0x13e, 0x13f, 0x13f, 0x140,
    0x140, 0x141, 0x142, 0x142, 0x143, 0x143, 0x144, 0x144,

    0x145, 0x146, 0x146, 0x147, 0x147, 0x148, 0x149, 0x149,   // -2
    0x14a, 0x14a, 0x14b, 0x14c, 0x14c, 0x14d, 0x14d, 0x14e,
    0x14f, 0x14f, 0x150, 0x150, 0x151, 0x152, 0x152, 0x153,
    0x153, 0x154, 0x155, 0x155, 0x156, 0x157, 0x157, 0x158,

    // These are used for the first seven MIDI note values:

    0x158, 0x159, 0x15a, 0x15a, 0x15b, 0x15b, 0x15c, 0x15d,   // 0
    0x15d, 0x15e, 0x15f, 0x15f, 0x160, 0x161, 0x161, 0x162,
    0x162, 0x163, 0x164, 0x164, 0x165, 0x166, 0x166, 0x167,
    0x168, 0x168, 0x169, 0x16a, 0x16a, 0x16b, 0x16c, 0x16c,

    0x16d, 0x16e, 0x16e, 0x16f, 0x170, 0x170, 0x171, 0x172,   // 1
    0x172, 0x173, 0x174, 0x174, 0x175, 0x176, 0x176, 0x177,
    0x178, 0x178, 0x179, 0x17a, 0x17a, 0x17b, 0x17c, 0x17c,
    0x17d, 0x17e, 0x17e, 0x17f, 0x180, 0x181, 0x181, 0x182,

    0x183, 0x183, 0x184, 0x185, 0x185, 0x186, 0x187, 0x188,   // 2
    0x188, 0x189, 0x18a, 0x18a, 0x18b, 0x18c, 0x18d, 0x18d,
    0x18e, 0x18f, 0x18f, 0x190, 0x191, 0x192, 0x192, 0x193,
    0x194, 0x194, 0x195, 0x196, 0x197, 0x197, 0x198, 0x199,

    0x19a, 0x19a, 0x19b, 0x19c, 0x19d, 0x19d, 0x19e, 0x19f,   // 3
    0x1a0, 0x1a0, 0x1a1, 0x1a2, 0x1a3, 0x1a3, 0x1a4, 0x1a5,
    0x1a6, 0x1a6, 0x1a7, 0x1a8, 0x1a9, 0x1a9, 0x1aa, 0x1ab,
    0x1ac, 0x1ad, 0x1ad, 0x1ae, 0x1af, 0x1b0, 0x1b0, 0x1b1,

    0x1b2, 0x1b3, 0x1b4, 0x1b4, 0x1b5, 0x1b6, 0x1b7, 0x1b8,   // 4
    0x1b8, 0x1b9, 0x1ba, 0x1bb, 0x1bc, 0x1bc, 0x1bd, 0x1be,
    0x1bf, 0x1c0, 0x1c0, 0x1c1, 0x1c2, 0x1c3, 0x1c4, 0x1c4,
    0x1c5, 0x1c6, 0x1c7, 0x1c8, 0x1c9, 0x1c9, 0x1ca, 0x1cb,

    0x1cc, 0x1cd, 0x1ce, 0x1ce, 0x1cf, 0x1d0, 0x1d1, 0x1d2,   // 5
    0x1d3, 0x1d3, 0x1d4, 0x1d5, 0x1d6, 0x1d7, 0x1d8, 0x1d8,
    0x1d9, 0x1da, 0x1db, 0x1dc, 0x1dd, 0x1de, 0x1de, 0x1df,
    0x1e0, 0x1e1, 0x1e2, 0x1e3, 0x1e4, 0x1e5, 0x1e5, 0x1e6,

    0x1e7, 0x1e8, 0x1e9, 0x1ea, 0x1eb, 0x1ec, 0x1ed, 0x1ed,   // 6
    0x1ee, 0x1ef, 0x1f0, 0x1f1, 0x1f2, 0x1f3, 0x1f4, 0x1f5,
    0x1f6, 0x1f6, 0x1f7, 0x1f8, 0x1f9, 0x1fa, 0x1fb, 0x1fc,
    0x1fd, 0x1fe, 0x1ff, 0x200, 0x201, 0x201, 0x202, 0x203,

    // First note of looped range used for all octaves:

    0x204, 0x205, 0x206, 0x207, 0x208, 0x209, 0x20a, 0x20b,   // 7
    0x20c, 0x20d, 0x20e, 0x20f, 0x210, 0x210, 0x211, 0x212,
    0x213, 0x214, 0x215, 0x216, 0x217, 0x218, 0x219, 0x21a,
    0x21b, 0x21c, 0x21d, 0x21e, 0x21f, 0x220, 0x221, 0x222,

    0x223, 0x224, 0x225, 0x226, 0x227, 0x228, 0x229, 0x22a,   // 8
    0x22b, 0x22c, 0x22d, 0x22e, 0x22f, 0x230, 0x231, 0x232,
    0x233, 0x234, 0x235, 0x236, 0x237, 0x238, 0x239, 0x23a,
    0x23b, 0x23c, 0x23d, 0x23e, 0x23f, 0x240, 0x241, 0x242,

    0x244, 0x245, 0x246, 0x247, 0x248, 0x249, 0x24a, 0x24b,   // 9
    0x24c, 0x24d, 0x24e, 0x24f, 0x250, 0x251, 0x252, 0x253,
    0x254, 0x256, 0x257, 0x258, 0x259, 0x25a, 0x25b, 0x25c,
    0x25d, 0x25e, 0x25f, 0x260, 0x262, 0x263, 0x264, 0x265,

    0x266, 0x267, 0x268, 0x269, 0x26a, 0x26c, 0x26d, 0x26e,   // 10
    0x26f, 0x270, 0x271, 0x272, 0x273, 0x275, 0x276, 0x277,
    0x278, 0x279, 0x27a, 0x27b, 0x27d, 0x27e, 0x27f, 0x280,
    0x281, 0x282, 0x284, 0x285, 0x286, 0x287, 0x288, 0x289,

    0x28b, 0x28c, 0x28d, 0x28e, 0x28f, 0x290, 0x292, 0x293,   // 11
    0x294, 0x295, 0x296, 0x298, 0x299, 0x29a, 0x29b, 0x29c,
    0x29e, 0x29f, 0x2a0, 0x2a1, 0x2a2, 0x2a4, 0x2a5, 0x2a6,
    0x2a7, 0x2a9, 0x2aa, 0x2ab, 0x2ac, 0x2ae, 0x2af, 0x2b0,

    0x2b1, 0x2b2, 0x2b4, 0x2b5, 0x2b6, 0x2b7, 0x2b9, 0x2ba,   // 12
    0x2bb, 0x2bd, 0x2be, 0x2bf, 0x2c0, 0x2c2, 0x2c3, 0x2c4,
    0x2c5, 0x2c7, 0x2c8, 0x2c9, 0x2cb, 0x2cc, 0x2cd, 0x2ce,
    0x2d0, 0x2d1, 0x2d2, 0x2d4, 0x2d5, 0x2d6, 0x2d8, 0x2d9,

    0x2da, 0x2dc, 0x2dd, 0x2de, 0x2e0, 0x2e1, 0x2e2, 0x2e4,   // 13
    0x2e5, 0x2e6, 0x2e8, 0x2e9, 0x2ea, 0x2ec, 0x2ed, 0x2ee,
    0x2f0, 0x2f1, 0x2f2, 0x2f4, 0x2f5, 0x2f6, 0x2f8, 0x2f9,
    0x2fb, 0x2fc, 0x2fd, 0x2ff, 0x300, 0x302, 0x303, 0x304,

    0x306, 0x307, 0x309, 0x30a, 0x30b, 0x30d, 0x30e, 0x310,   // 14
    0x311, 0x312, 0x314, 0x315, 0x317, 0x318, 0x31a, 0x31b,
    0x31c, 0x31e, 0x31f, 0x321, 0x322, 0x324, 0x325, 0x327,
    0x328, 0x329, 0x32b, 0x32c, 0x32e, 0x32f, 0x331, 0x332,

    0x334, 0x335, 0x337, 0x338, 0x33a, 0x33b, 0x33d, 0x33e,   // 15
    0x340, 0x341, 0x343, 0x344, 0x346, 0x347, 0x349, 0x34a,
    0x34c, 0x34d, 0x34f, 0x350, 0x352, 0x353, 0x355, 0x357,
    0x358, 0x35a, 0x35b, 0x35d, 0x35e, 0x360, 0x361, 0x363,

    0x365, 0x366, 0x368, 0x369, 0x36b, 0x36c, 0x36e, 0x370,   // 16
    0x371, 0x373, 0x374, 0x376, 0x378, 0x379, 0x37b, 0x37c,
    0x37e, 0x380, 0x381, 0x383, 0x384, 0x386, 0x388, 0x389,
    0x38b, 0x38d, 0x38e, 0x390, 0x392, 0x393, 0x395, 0x397,

    0x398, 0x39a, 0x39c, 0x39d, 0x39f, 0x3a1, 0x3a2, 0x3a4,   // 17
    0x3a6, 0x3a7, 0x3a9, 0x3ab, 0x3ac, 0x3ae, 0x3b0, 0x3b1,
    0x3b3, 0x3b5, 0x3b7, 0x3b8, 0x3ba, 0x3bc, 0x3bd, 0x3bf,
    0x3c1, 0x3c3, 0x3c4, 0x3c6, 0x3c8, 0x3ca, 0x3cb, 0x3cd,

    // The last note has an incomplete range, and loops round back to
    // the start.  Note that the last value is actually a buffer overrun
    // and does not fit with the other values.

    0x3cf, 0x3d1, 0x3d2, 0x3d4, 0x3d6, 0x3d8, 0x3da, 0x3db,   // 18
    0x3dd, 0x3df, 0x3e1, 0x3e3, 0x3e4, 0x3e6, 0x3e8, 0x3ea,
    0x3ec, 0x3ed, 0x3ef, 0x3f1, 0x3f3, 0x3f5, 0x3f6, 0x3f8,
    0x3fa, 0x3fc, 0x3fe, 0x36c,
};

void OPLio::WriteFrequency(uint32_t channel, uint32_t note, uint32_t pitch, uint32_t keyon)
{
	int octave = 0;
	int j = (note << 5) + pitch;

	if (j < 0)
	{
		j = 0;
	}
	else if (j >= 284)
	{
		j -= 284;
		octave = j / (32*12);
		if (octave > 7)
		{
			octave = 7;
		}
		j = (j % (32*12)) + 284;
	}
	int i = frequencies[j] | (octave << 10);

	WriteValue (OPL_REGS_FREQ_1, channel, (uint8_t)i);
	WriteValue (OPL_REGS_FREQ_2, channel, (uint8_t)(i>>8)|(keyon<<5));
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

static uint8_t volumetable[128] = {
	  0,   1,   3,   5,   6,   8,  10,  11,
	 13,  14,  16,  17,  19,  20,  22,  23,
	 25,  26,  27,  29,  30,  32,  33,  34,
	 36,  37,  39,  41,  43,  45,  47,  49,
	 50,  52,  54,  55,  57,  59,  60,  61,
	 63,  64,  66,  67,  68,  69,  71,  72,
	 73,  74,  75,  76,  77,  79,  80,  81,
	 82,  83,  84,  84,  85,  86,  87,  88,
	 89,  90,  91,  92,  92,  93,  94,  95,
	 96,  96,  97,  98,  99,  99, 100, 101,
	101, 102, 103, 103, 104, 105, 105, 106,
	107, 107, 108, 109, 109, 110, 110, 111,
	112, 112, 113, 113, 114, 114, 115, 115,
	116, 117, 117, 118, 118, 119, 119, 120,
	120, 121, 121, 122, 122, 123, 123, 123,
	124, 124, 125, 125, 126, 126, 127, 127};

void OPLio::WriteVolume(uint32_t channel, struct GenMidiVoice *voice, uint32_t vol1, uint32_t vol2, uint32_t vol3)
{
	if (voice != nullptr)
	{
		uint32_t full_volume = volumetable[std::min<uint32_t>(127, (uint32_t)((uint64_t)vol1*vol2*vol3) / (127 * 127))];
		int reg_volume2 = ((0x3f - voice->carrier.level) * full_volume) / 128;
		reg_volume2 = (0x3f - reg_volume2) | voice->carrier.scale;
		WriteOperator(OPL_REGS_LEVEL, channel, 1, reg_volume2);

		int reg_volume1;
		if (voice->feedback & 0x01)
		{
			// Chocolate Doom says:
			// If we are using non-modulated feedback mode, we must set the
			// volume for both voices.
			// Note that the same register volume value is written for
			// both voices, always calculated from the carrier's level
			// value.

			// But Muslib does it differently than the comment above states. Which one is correct?
			
			reg_volume1 = ((0x3f - voice->modulator.level) * full_volume) / 128;
			reg_volume1 = (0x3f - reg_volume1) | voice->modulator.scale;
		}
		else
		{
			reg_volume1 = voice->modulator.level | voice->modulator.scale;
		}
		WriteOperator(OPL_REGS_LEVEL, channel, 0,reg_volume1);
	}
}


//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void OPLio::WritePan(uint32_t channel, struct GenMidiVoice *voice, int pan)
{
	if (voice != 0)
	{
		WriteValue(OPL_REGS_FEEDBACK, channel, voice->feedback | (pan >= 28 ? 0x20 : 0) | (pan <= 100 ? 0x10 : 0));

		// Set real panning if we're using emulated chips.
		int chanper = IsOPL3 ? OPL3_NUM_VOICES : OPL_NUM_VOICES;
		int which = channel / chanper;
		if (chips[which] != NULL)
		{
			// This is the MIDI-recommended pan formula. 0 and 1 are
			// both hard left so that 64 can be perfectly center.
			double level = (pan <= 1) ? 0 : (pan - 1) / 126.0;
			chips[which]->SetPanning(channel % chanper,
				(float)cos(HALF_PI * level), (float)sin(HALF_PI * level));
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void OPLio::WriteTremolo(uint32_t channel, struct GenMidiVoice *voice, bool vibrato)
{
	int val1 = voice->modulator.tremolo, val2 = voice->carrier.tremolo;
	if (vibrato)
	{
		if (voice->feedback & 1) val1 |= 0x40;
		val2 |= 0x40;
	}
	WriteOperator(OPL_REGS_TREMOLO, channel, 1, val2);
	WriteOperator(OPL_REGS_TREMOLO, channel, 0, val1);
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void OPLio::MuteChannel(uint32_t channel)
{
	WriteOperator(OPL_REGS_LEVEL, channel, 1, NO_VOLUME);
	WriteOperator(OPL_REGS_ATTACK, channel, 1, MAX_ATTACK_DECAY);
	WriteOperator(OPL_REGS_SUSTAIN, channel, 1, NO_SUSTAIN_MAX_RELEASE);

	WriteOperator(OPL_REGS_LEVEL, channel, 0, NO_VOLUME);
	WriteOperator(OPL_REGS_ATTACK, channel, 0, MAX_ATTACK_DECAY);
	WriteOperator(OPL_REGS_SUSTAIN, channel, 0, NO_SUSTAIN_MAX_RELEASE);
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void OPLio::LoadOperatorData(uint32_t channel, int op_index, genmidi_op_t *data, bool max_level, bool vibrato)
{
	// The scale and level fields must be combined for the level register.
	// For the carrier wave we always set the maximum level.

	int level = data->scale;
	if (max_level) level |= 0x3f;
	else level |= data->level;

	int tremolo = data->tremolo;
	if (vibrato) tremolo |= 0x40;

	WriteOperator(OPL_REGS_LEVEL, channel, op_index, level);
	WriteOperator(OPL_REGS_TREMOLO, channel, op_index, tremolo);
	WriteOperator(OPL_REGS_ATTACK, channel, op_index, data->attack);
	WriteOperator(OPL_REGS_SUSTAIN, channel, op_index, data->sustain);
	WriteOperator(OPL_REGS_WAVEFORM, channel, op_index, data->waveform);
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void OPLio::WriteInstrument(uint32_t channel, struct GenMidiVoice *voice, bool vibrato)
{
	bool modulating = (voice->feedback & 0x01) == 0;

	// Doom loads the second operator first, then the first.
	// The carrier is set to minimum volume until the voice volume
	// is set later.  If we are not using modulating mode, we must set both to minimum volume.

	LoadOperatorData(channel, 1, &voice->carrier, true, vibrato);
	LoadOperatorData(channel, 0, &voice->modulator, !modulating, vibrato && modulating);

	// The feedback register is written by the calling code.
}
