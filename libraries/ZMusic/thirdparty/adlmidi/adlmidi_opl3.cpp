/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "adlmidi_opl3.hpp"
#include "adlmidi_private.hpp"
#include <stdlib.h>
#include <cassert>


#ifdef ENABLE_HW_OPL_DOS
#   include "chips/dos_hw_opl.h"
#else
#   if defined(ADLMIDI_DISABLE_NUKED_EMULATOR) && \
       defined(ADLMIDI_DISABLE_DOSBOX_EMULATOR) && \
       defined(ADLMIDI_DISABLE_OPAL_EMULATOR) && \
       defined(ADLMIDI_DISABLE_JAVA_EMULATOR) && \
       defined(ADLMIDI_DISABLE_ESFMU_EMULATOR) && \
       defined(ADLMIDI_DISABLE_MAME_OPL2_EMULATOR) && \
       defined(ADLMIDI_DISABLE_YMFM_EMULATOR) && \
       !defined(ADLMIDI_ENABLE_OPL2_LLE_EMULATOR) && \
       !defined(ADLMIDI_ENABLE_OPL3_LLE_EMULATOR) && \
       !defined(ADLMIDI_ENABLE_HW_SERIAL)
#       error "No emulators enabled. You must enable at least one emulator to use this library!"
#   endif

// Nuked OPL3 emulator, Most accurate, but requires the powerful CPU
#   ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
#       include "chips/nuked_opl3.h"
#       include "chips/nuked_opl3_v174.h"
#   endif

// DosBox 0.74 OPL3 emulator, Well-accurate and fast
#   ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
#       include "chips/dosbox_opl3.h"
#   endif

// Opal emulator
#   ifndef ADLMIDI_DISABLE_OPAL_EMULATOR
#       include "chips/opal_opl3.h"
#   endif

// Java emulator
#   ifndef ADLMIDI_DISABLE_JAVA_EMULATOR
#       include "chips/java_opl3.h"
#   endif

// ESFMu emulator
#   ifndef ADLMIDI_DISABLE_ESFMU_EMULATOR
#       include "chips/esfmu_opl3.h"
#   endif

// MAME OPL2 emulator
#   ifndef ADLMIDI_DISABLE_MAME_OPL2_EMULATOR
#       include "chips/mame_opl2.h"
#   endif

// YMFM emulators
#   ifndef ADLMIDI_DISABLE_YMFM_EMULATOR
#       include "chips/ymfm_opl2.h"
#       include "chips/ymfm_opl3.h"
#   endif

// Nuked OPL2 LLE emulator
#   ifdef ADLMIDI_ENABLE_OPL2_LLE_EMULATOR
#       include "chips/ym3812_lle.h"
#   endif

// Nuked OPL3 LLE emulator
#   ifdef ADLMIDI_ENABLE_OPL3_LLE_EMULATOR
#       include "chips/ymf262_lle.h"
#   endif

// HW OPL Serial
#   ifdef ADLMIDI_ENABLE_HW_SERIAL
#       include "chips/opl_serial_port.h"
#   endif
#endif

static const unsigned adl_emulatorSupport = 0
#ifndef ENABLE_HW_OPL_DOS
#   ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
    | (1u << ADLMIDI_EMU_NUKED) | (1u << ADLMIDI_EMU_NUKED_174)
#   endif

#   ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
    | (1u << ADLMIDI_EMU_DOSBOX)
#   endif

#   ifndef ADLMIDI_DISABLE_OPAL_EMULATOR
    | (1u << ADLMIDI_EMU_OPAL)
#   endif

#   ifndef ADLMIDI_DISABLE_ESFMU_EMULATOR
    | (1u << ADLMIDI_EMU_ESFMu)
#   endif

#   ifndef ADLMIDI_DISABLE_JAVA_EMULATOR
    | (1u << ADLMIDI_EMU_JAVA)
#   endif

#   ifndef ADLMIDI_DISABLE_MAME_OPL2_EMULATOR
    | (1u << ADLMIDI_EMU_MAME_OPL2)
#   endif

#   ifndef ADLMIDI_DISABLE_YMFM_EMULATOR
    | (1u << ADLMIDI_EMU_YMFM_OPL2)
    | (1u << ADLMIDI_EMU_YMFM_OPL3)
#   endif

#   ifdef ADLMIDI_ENABLE_OPL2_LLE_EMULATOR
    | (1u << ADLMIDI_EMU_NUKED_OPL2_LLE)
#   endif

#   ifdef ADLMIDI_ENABLE_OPL3_LLE_EMULATOR
    | (1u << ADLMIDI_EMU_NUKED_OPL3_LLE)
#   endif
#endif
;

//! Check emulator availability
bool adl_isEmulatorAvailable(int emulator)
{
    return (adl_emulatorSupport & (1u << (unsigned)emulator)) != 0;
}

//! Find highest emulator
int adl_getHighestEmulator()
{
    int emu = -1;
    for(unsigned m = adl_emulatorSupport; m > 0; m >>= 1)
        ++emu;
    return emu;
}

//! Find lowest emulator
int adl_getLowestEmulator()
{
    int emu = -1;
    unsigned m = adl_emulatorSupport;
    if(m > 0)
    {
        for(emu = 0; (m & 1) == 0; m >>= 1)
            ++emu;
    }
    return emu;
}

//! Per-channel and per-operator registers map
static const uint16_t g_operatorsMap[(NUM_OF_CHANNELS + NUM_OF_RM_CHANNELS) * 2] =
{
    // Channels 0-2
    0x000, 0x003, 0x001, 0x004, 0x002, 0x005, // operators  0, 3,  1, 4,  2, 5
    // Channels 3-5
    0x008, 0x00B, 0x009, 0x00C, 0x00A, 0x00D, // operators  6, 9,  7,10,  8,11
    // Channels 6-8
    0x010, 0x013, 0x011, 0x014, 0x012, 0x015, // operators 12,15, 13,16, 14,17
    // Same for second card
    0x100, 0x103, 0x101, 0x104, 0x102, 0x105, // operators 18,21, 19,22, 20,23
    0x108, 0x10B, 0x109, 0x10C, 0x10A, 0x10D, // operators 24,27, 25,28, 26,29
    0x110, 0x113, 0x111, 0x114, 0x112, 0x115, // operators 30,33, 31,34, 32,35

    //==For Rhythm-mode percussions
    // Channel 18
    0x010, 0x013,  // operators 12,15
    // Channel 19
    0xFFF, 0x014,  // operator 16
    // Channel 19
    0x012, 0xFFF,  // operator 14
    // Channel 19
    0xFFF, 0x015,  // operator 17
    // Channel 19
    0x011, 0xFFF,  // operator 13

    //==For Rhythm-mode percussions in CMF, snare and cymbal operators has inverted!
    0x010, 0x013,  // operators 12,15
    // Channel 19
    0x014, 0xFFF,  // operator 16
    // Channel 19
    0x012, 0xFFF,  // operator 14
    // Channel 19
    0x015, 0xFFF,  // operator 17
    // Channel 19
    0x011, 0xFFF   // operator 13
};

//! Channel map to register offsets
static const uint16_t g_channelsMap[NUM_OF_CHANNELS] =
{
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, // 0..8
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, // 9..17 (secondary set)
    0x006, 0x007, 0x008, 0x008, 0x008 // <- hw percussions, hihats and cymbals using tom-tom's channel as pitch source
};

//! Channel map to register offsets (separated copy for panning and for CMF)
static const uint16_t g_channelsMapPan[NUM_OF_CHANNELS] =
{
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, // 0..8
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, // 9..17 (secondary set)
    0x006, 0x007, 0x008, 0xFFF, 0xFFF // <- hw percussions, 0xFFF = no support for pitch/pan
};

//! Channel map to register offsets (separated copy for feedback+connection bits)
static const uint16_t g_channelsMapFBConn[NUM_OF_CHANNELS] =
{
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, // 0..8
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, // 9..17 (secondary set)
    0x006, 0xFFF, 0xFFF, 0xFFF, 0xFFF // <- hw percussions, 0xFFF = no support for pitch/pan
};

/*
    In OPL3 mode:
         0    1    2    6    7    8     9   10   11    16   17   18
       op0  op1  op2 op12 op13 op14  op18 op19 op20  op30 op31 op32
       op3  op4  op5 op15 op16 op17  op21 op22 op23  op33 op34 op35
         3    4    5                   13   14   15
       op6  op7  op8                 op24 op25 op26
       op9 op10 op11                 op27 op28 op29
    Ports:
        +0   +1   +2  +10  +11  +12  +100 +101 +102  +110 +111 +112
        +3   +4   +5  +13  +14  +15  +103 +104 +105  +113 +114 +115
        +8   +9   +A                 +108 +109 +10A
        +B   +C   +D                 +10B +10C +10D

    Percussion:
      bassdrum = op(0): 0xBD bit 0x10, operators 12 (0x10) and 15 (0x13) / channels 6, 6b
      snare    = op(3): 0xBD bit 0x08, operators 16 (0x14)               / channels 7b
      tomtom   = op(4): 0xBD bit 0x04, operators 14 (0x12)               / channels 8
      cym      = op(5): 0xBD bit 0x02, operators 17 (0x17)               / channels 8b
      hihat    = op(2): 0xBD bit 0x01, operators 13 (0x11)               / channels 7


    In OPTi mode ("extended FM" in 82C924, 82C925, 82C931 chips):
         0   1   2    3    4    5    6    7     8    9   10   11   12   13   14   15   16   17
       op0 op4 op6 op10 op12 op16 op18 op22  op24 op28 op30 op34 op36 op38 op40 op42 op44 op46
       op1 op5 op7 op11 op13 op17 op19 op23  op25 op29 op31 op35 op37 op39 op41 op43 op45 op47
       op2     op8      op14      op20       op26      op32
       op3     op9      op15      op21       op27      op33    for a total of 6 quad + 12 dual
    Ports: ???
*/





/***************************************************************
 *                    Volume model tables                      *
 ***************************************************************/

// Mapping from MIDI volume level to OPL level value.

static const uint_fast32_t s_dmx_volume_model[128] =
{
    0,  1,  3,  5,  6,  8,  10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127,
};

static const uint_fast32_t s_w9x_sb16_volume_model[32] =
{
    80, 63, 40, 36, 32, 28, 23, 21,
    19, 17, 15, 14, 13, 12, 11, 10,
    9,  8,  7,  6,  5,  5,  4,  4,
    3,  3,  2,  2,  1,  1,  0,  0
};

static const uint_fast32_t s_w9x_generic_fm_volume_model[32] =
{
    40, 36, 32, 28, 23, 21, 19, 17,
    15, 14, 13, 12, 11, 10, 9,  8,
    7,  6,  5,  5,  4,  4,  3,  3,
    2,  2,  1,  1,  1,  0,  0,  0
};

static const uint_fast32_t s_ail_vel_graph[16] =
{
    82,   85,  88,  91,  94,  97, 100, 103,
    106, 109, 112, 115, 118, 121, 124, 127
};

static const uint_fast32_t s_hmi_volume_table[64] =
{
    0x3F, 0x3A, 0x35, 0x30, 0x2C, 0x29, 0x25, 0x24,
    0x23, 0x22, 0x21, 0x20, 0x1F, 0x1E, 0x1D, 0x1C,
    0x1B, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14,
    0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0E, 0x0D,
    0x0D, 0x0C, 0x0C, 0x0B, 0x0B, 0x0A, 0x0A, 0x09,
    0x09, 0x08, 0x08, 0x07, 0x07, 0x06, 0x06, 0x06,
    0x05, 0x05, 0x05, 0x04, 0x04, 0x04, 0x04, 0x03,
    0x03, 0x03, 0x02, 0x02, 0x02, 0x01, 0x01, 0x00,
};





/***************************************************************
 *               Standard frequency formula                    *
 * *************************************************************/

static inline double s_commonFreq(double tone)
{
    return BEND_COEFFICIENT * std::exp(0.057762265 * tone);
}




/***************************************************************
 *                   DMX frequency model                       *
 * *************************************************************/

// DMX volumes table
static const int_fast32_t s_dmx_freq_table[] =
{
    0x0133, 0x0133, 0x0134, 0x0134, 0x0135, 0x0136, 0x0136, 0x0137,
    0x0137, 0x0138, 0x0138, 0x0139, 0x0139, 0x013A, 0x013B, 0x013B,
    0x013C, 0x013C, 0x013D, 0x013D, 0x013E, 0x013F, 0x013F, 0x0140,
    0x0140, 0x0141, 0x0142, 0x0142, 0x0143, 0x0143, 0x0144, 0x0144,

    0x0145, 0x0146, 0x0146, 0x0147, 0x0147, 0x0148, 0x0149, 0x0149,
    0x014A, 0x014A, 0x014B, 0x014C, 0x014C, 0x014D, 0x014D, 0x014E,
    0x014F, 0x014F, 0x0150, 0x0150, 0x0151, 0x0152, 0x0152, 0x0153,
    0x0153, 0x0154, 0x0155, 0x0155, 0x0156, 0x0157, 0x0157, 0x0158,

    0x0158, 0x0159, 0x015A, 0x015A, 0x015B, 0x015B, 0x015C, 0x015D,
    0x015D, 0x015E, 0x015F, 0x015F, 0x0160, 0x0161, 0x0161, 0x0162,
    0x0162, 0x0163, 0x0164, 0x0164, 0x0165, 0x0166, 0x0166, 0x0167,
    0x0168, 0x0168, 0x0169, 0x016A, 0x016A, 0x016B, 0x016C, 0x016C,

    0x016D, 0x016E, 0x016E, 0x016F, 0x0170, 0x0170, 0x0171, 0x0172,
    0x0172, 0x0173, 0x0174, 0x0174, 0x0175, 0x0176, 0x0176, 0x0177,
    0x0178, 0x0178, 0x0179, 0x017A, 0x017A, 0x017B, 0x017C, 0x017C,
    0x017D, 0x017E, 0x017E, 0x017F, 0x0180, 0x0181, 0x0181, 0x0182,

    0x0183, 0x0183, 0x0184, 0x0185, 0x0185, 0x0186, 0x0187, 0x0188,
    0x0188, 0x0189, 0x018A, 0x018A, 0x018B, 0x018C, 0x018D, 0x018D,
    0x018E, 0x018F, 0x018F, 0x0190, 0x0191, 0x0192, 0x0192, 0x0193,
    0x0194, 0x0194, 0x0195, 0x0196, 0x0197, 0x0197, 0x0198, 0x0199,

    0x019A, 0x019A, 0x019B, 0x019C, 0x019D, 0x019D, 0x019E, 0x019F,
    0x01A0, 0x01A0, 0x01A1, 0x01A2, 0x01A3, 0x01A3, 0x01A4, 0x01A5,
    0x01A6, 0x01A6, 0x01A7, 0x01A8, 0x01A9, 0x01A9, 0x01AA, 0x01AB,
    0x01AC, 0x01AD, 0x01AD, 0x01AE, 0x01AF, 0x01B0, 0x01B0, 0x01B1,

    0x01B2, 0x01B3, 0x01B4, 0x01B4, 0x01B5, 0x01B6, 0x01B7, 0x01B8,
    0x01B8, 0x01B9, 0x01BA, 0x01BB, 0x01BC, 0x01BC, 0x01BD, 0x01BE,
    0x01BF, 0x01C0, 0x01C0, 0x01C1, 0x01C2, 0x01C3, 0x01C4, 0x01C4,
    0x01C5, 0x01C6, 0x01C7, 0x01C8, 0x01C9, 0x01C9, 0x01CA, 0x01CB,

    0x01CC, 0x01CD, 0x01CE, 0x01CE, 0x01CF, 0x01D0, 0x01D1, 0x01D2,
    0x01D3, 0x01D3, 0x01D4, 0x01D5, 0x01D6, 0x01D7, 0x01D8, 0x01D8,
    0x01D9, 0x01DA, 0x01DB, 0x01DC, 0x01DD, 0x01DE, 0x01DE, 0x01DF,
    0x01E0, 0x01E1, 0x01E2, 0x01E3, 0x01E4, 0x01E5, 0x01E5, 0x01E6,

    0x01E7, 0x01E8, 0x01E9, 0x01EA, 0x01EB, 0x01EC, 0x01ED, 0x01ED,
    0x01EE, 0x01EF, 0x01F0, 0x01F1, 0x01F2, 0x01F3, 0x01F4, 0x01F5,
    0x01F6, 0x01F6, 0x01F7, 0x01F8, 0x01F9, 0x01FA, 0x01FB, 0x01FC,
    0x01FD, 0x01FE, 0x01FF,

    0x0200, 0x0201, 0x0201, 0x0202, 0x0203, 0x0204, 0x0205, 0x0206,
    0x0207, 0x0208, 0x0209, 0x020A, 0x020B, 0x020C, 0x020D, 0x020E,
    0x020F, 0x0210, 0x0210, 0x0211, 0x0212, 0x0213, 0x0214, 0x0215,
    0x0216, 0x0217, 0x0218, 0x0219, 0x021A, 0x021B, 0x021C, 0x021D,

    0x021E, 0x021F, 0x0220, 0x0221, 0x0222, 0x0223, 0x0224, 0x0225,
    0x0226, 0x0227, 0x0228, 0x0229, 0x022A, 0x022B, 0x022C, 0x022D,
    0x022E, 0x022F, 0x0230, 0x0231, 0x0232, 0x0233, 0x0234, 0x0235,
    0x0236, 0x0237, 0x0238, 0x0239, 0x023A, 0x023B, 0x023C, 0x023D,

    0x023E, 0x023F, 0x0240, 0x0241, 0x0242, 0x0244, 0x0245, 0x0246,
    0x0247, 0x0248, 0x0249, 0x024A, 0x024B, 0x024C, 0x024D, 0x024E,
    0x024F, 0x0250, 0x0251, 0x0252, 0x0253, 0x0254, 0x0256, 0x0257,
    0x0258, 0x0259, 0x025A, 0x025B, 0x025C, 0x025D, 0x025E, 0x025F,

    0x0260, 0x0262, 0x0263, 0x0264, 0x0265, 0x0266, 0x0267, 0x0268,
    0x0269, 0x026A, 0x026C, 0x026D, 0x026E, 0x026F, 0x0270, 0x0271,
    0x0272, 0x0273, 0x0275, 0x0276, 0x0277, 0x0278, 0x0279, 0x027A,
    0x027B, 0x027D, 0x027E, 0x027F, 0x0280, 0x0281, 0x0282, 0x0284,

    0x0285, 0x0286, 0x0287, 0x0288, 0x0289, 0x028B, 0x028C, 0x028D,
    0x028E, 0x028F, 0x0290, 0x0292, 0x0293, 0x0294, 0x0295, 0x0296,
    0x0298, 0x0299, 0x029A, 0x029B, 0x029C, 0x029E, 0x029F, 0x02A0,
    0x02A1, 0x02A2, 0x02A4, 0x02A5, 0x02A6, 0x02A7, 0x02A9, 0x02AA,

    0x02AB, 0x02AC, 0x02AE, 0x02AF, 0x02B0, 0x02B1, 0x02B2, 0x02B4,
    0x02B5, 0x02B6, 0x02B7, 0x02B9, 0x02BA, 0x02BB, 0x02BD, 0x02BE,
    0x02BF, 0x02C0, 0x02C2, 0x02C3, 0x02C4, 0x02C5, 0x02C7, 0x02C8,
    0x02C9, 0x02CB, 0x02CC, 0x02CD, 0x02CE, 0x02D0, 0x02D1, 0x02D2,

    0x02D4, 0x02D5, 0x02D6, 0x02D8, 0x02D9, 0x02DA, 0x02DC, 0x02DD,
    0x02DE, 0x02E0, 0x02E1, 0x02E2, 0x02E4, 0x02E5, 0x02E6, 0x02E8,
    0x02E9, 0x02EA, 0x02EC, 0x02ED, 0x02EE, 0x02F0, 0x02F1, 0x02F2,
    0x02F4, 0x02F5, 0x02F6, 0x02F8, 0x02F9, 0x02FB, 0x02FC, 0x02FD,

    0x02FF, 0x0300, 0x0302, 0x0303, 0x0304, 0x0306, 0x0307, 0x0309,
    0x030A, 0x030B, 0x030D, 0x030E, 0x0310, 0x0311, 0x0312, 0x0314,
    0x0315, 0x0317, 0x0318, 0x031A, 0x031B, 0x031C, 0x031E, 0x031F,
    0x0321, 0x0322, 0x0324, 0x0325, 0x0327, 0x0328, 0x0329, 0x032B,

    0x032C, 0x032E, 0x032F, 0x0331, 0x0332, 0x0334, 0x0335, 0x0337,
    0x0338, 0x033A, 0x033B, 0x033D, 0x033E, 0x0340, 0x0341, 0x0343,
    0x0344, 0x0346, 0x0347, 0x0349, 0x034A, 0x034C, 0x034D, 0x034F,
    0x0350, 0x0352, 0x0353, 0x0355, 0x0357, 0x0358, 0x035A, 0x035B,

    0x035D, 0x035E, 0x0360, 0x0361, 0x0363, 0x0365, 0x0366, 0x0368,
    0x0369, 0x036B, 0x036C, 0x036E, 0x0370, 0x0371, 0x0373, 0x0374,
    0x0376, 0x0378, 0x0379, 0x037B, 0x037C, 0x037E, 0x0380, 0x0381,
    0x0383, 0x0384, 0x0386, 0x0388, 0x0389, 0x038B, 0x038D, 0x038E,

    0x0390, 0x0392, 0x0393, 0x0395, 0x0397, 0x0398, 0x039A, 0x039C,
    0x039D, 0x039F, 0x03A1, 0x03A2, 0x03A4, 0x03A6, 0x03A7, 0x03A9,
    0x03AB, 0x03AC, 0x03AE, 0x03B0, 0x03B1, 0x03B3, 0x03B5, 0x03B7,
    0x03B8, 0x03BA, 0x03BC, 0x03BD, 0x03BF, 0x03C1, 0x03C3, 0x03C4,

    0x03C6, 0x03C8, 0x03CA, 0x03CB, 0x03CD, 0x03CF, 0x03D1, 0x03D2,
    0x03D4, 0x03D6, 0x03D8, 0x03DA, 0x03DB, 0x03DD, 0x03DF, 0x03E1,
    0x03E3, 0x03E4, 0x03E6, 0x03E8, 0x03EA, 0x03EC, 0x03ED, 0x03EF,
    0x03F1, 0x03F3, 0x03F5, 0x03F6, 0x03F8, 0x03FA, 0x03FC, 0x03FE,

    0x036C
};

static inline double s_dmxFreq(double tone)
{
    uint_fast32_t noteI = (uint_fast32_t)(tone);
    int_fast32_t bendI = 0;
    int_fast32_t outHz = 0;
    double bendDec = tone - (int)tone;

    bendI = (int_fast32_t)((bendDec * 128.0) / 2.0) + 128;
    bendI = bendI >> 1;

    int_fast32_t oct = 0;
    int_fast32_t freqIndex = (noteI << 5) + bendI;

#define MAX_FREQ_IDX 283 // 284 - with the DMX side bug
    if(freqIndex < 0)
        freqIndex = 0;
    else if(freqIndex >= MAX_FREQ_IDX)
    {
        freqIndex -= MAX_FREQ_IDX;
        oct = freqIndex / 384;
        freqIndex = (freqIndex % 384) + MAX_FREQ_IDX;
    }
#undef MAX_FREQ_IDX

    outHz = s_dmx_freq_table[freqIndex];

    while(oct > 1)
    {
        outHz *= 2;
        oct -= 1;
    }

    return (double)outHz;
}




/***************************************************************
 *             Apogee Sound System frequency model             *
 ***************************************************************/

static const int_fast32_t s_apogee_freq_table[31 + 1][12] =
{
    { 0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287 },
    { 0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca, 0x1e5, 0x202, 0x220, 0x242, 0x264, 0x288 },
    { 0x158, 0x16c, 0x182, 0x199, 0x1b1, 0x1cb, 0x1e6, 0x203, 0x221, 0x243, 0x265, 0x289 },
    { 0x158, 0x16c, 0x183, 0x19a, 0x1b2, 0x1cc, 0x1e7, 0x204, 0x222, 0x244, 0x266, 0x28a },
    { 0x159, 0x16d, 0x183, 0x19a, 0x1b3, 0x1cd, 0x1e8, 0x205, 0x223, 0x245, 0x267, 0x28b },
    { 0x15a, 0x16e, 0x184, 0x19b, 0x1b3, 0x1ce, 0x1e9, 0x206, 0x224, 0x246, 0x268, 0x28c },
    { 0x15a, 0x16e, 0x185, 0x19c, 0x1b4, 0x1ce, 0x1ea, 0x207, 0x225, 0x247, 0x269, 0x28e },
    { 0x15b, 0x16f, 0x185, 0x19d, 0x1b5, 0x1cf, 0x1eb, 0x208, 0x226, 0x248, 0x26a, 0x28f },
    { 0x15b, 0x170, 0x186, 0x19d, 0x1b6, 0x1d0, 0x1ec, 0x209, 0x227, 0x249, 0x26b, 0x290 },
    { 0x15c, 0x170, 0x187, 0x19e, 0x1b7, 0x1d1, 0x1ec, 0x20a, 0x228, 0x24a, 0x26d, 0x291 },
    { 0x15d, 0x171, 0x188, 0x19f, 0x1b7, 0x1d2, 0x1ed, 0x20b, 0x229, 0x24b, 0x26e, 0x292 },
    { 0x15d, 0x172, 0x188, 0x1a0, 0x1b8, 0x1d3, 0x1ee, 0x20c, 0x22a, 0x24c, 0x26f, 0x293 },
    { 0x15e, 0x172, 0x189, 0x1a0, 0x1b9, 0x1d4, 0x1ef, 0x20d, 0x22b, 0x24d, 0x270, 0x295 },
    { 0x15f, 0x173, 0x18a, 0x1a1, 0x1ba, 0x1d4, 0x1f0, 0x20e, 0x22c, 0x24e, 0x271, 0x296 },
    { 0x15f, 0x174, 0x18a, 0x1a2, 0x1bb, 0x1d5, 0x1f1, 0x20f, 0x22d, 0x24f, 0x272, 0x297 },
    { 0x160, 0x174, 0x18b, 0x1a3, 0x1bb, 0x1d6, 0x1f2, 0x210, 0x22e, 0x250, 0x273, 0x298 },
    { 0x161, 0x175, 0x18c, 0x1a3, 0x1bc, 0x1d7, 0x1f3, 0x211, 0x22f, 0x251, 0x274, 0x299 },
    { 0x161, 0x176, 0x18c, 0x1a4, 0x1bd, 0x1d8, 0x1f4, 0x212, 0x230, 0x252, 0x276, 0x29b },
    { 0x162, 0x176, 0x18d, 0x1a5, 0x1be, 0x1d9, 0x1f5, 0x212, 0x231, 0x254, 0x277, 0x29c },
    { 0x162, 0x177, 0x18e, 0x1a6, 0x1bf, 0x1d9, 0x1f5, 0x213, 0x232, 0x255, 0x278, 0x29d },
    { 0x163, 0x178, 0x18f, 0x1a6, 0x1bf, 0x1da, 0x1f6, 0x214, 0x233, 0x256, 0x279, 0x29e },
    { 0x164, 0x179, 0x18f, 0x1a7, 0x1c0, 0x1db, 0x1f7, 0x215, 0x235, 0x257, 0x27a, 0x29f },
    { 0x164, 0x179, 0x190, 0x1a8, 0x1c1, 0x1dc, 0x1f8, 0x216, 0x236, 0x258, 0x27b, 0x2a1 },
    { 0x165, 0x17a, 0x191, 0x1a9, 0x1c2, 0x1dd, 0x1f9, 0x217, 0x237, 0x259, 0x27c, 0x2a2 },
    { 0x166, 0x17b, 0x192, 0x1aa, 0x1c3, 0x1de, 0x1fa, 0x218, 0x238, 0x25a, 0x27e, 0x2a3 },
    { 0x166, 0x17b, 0x192, 0x1aa, 0x1c3, 0x1df, 0x1fb, 0x219, 0x239, 0x25b, 0x27f, 0x2a4 },
    { 0x167, 0x17c, 0x193, 0x1ab, 0x1c4, 0x1e0, 0x1fc, 0x21a, 0x23a, 0x25c, 0x280, 0x2a6 },
    { 0x168, 0x17d, 0x194, 0x1ac, 0x1c5, 0x1e0, 0x1fd, 0x21b, 0x23b, 0x25d, 0x281, 0x2a7 },
    { 0x168, 0x17d, 0x194, 0x1ad, 0x1c6, 0x1e1, 0x1fe, 0x21c, 0x23c, 0x25e, 0x282, 0x2a8 },
    { 0x169, 0x17e, 0x195, 0x1ad, 0x1c7, 0x1e2, 0x1ff, 0x21d, 0x23d, 0x260, 0x283, 0x2a9 },
    { 0x16a, 0x17f, 0x196, 0x1ae, 0x1c8, 0x1e3, 0x1ff, 0x21e, 0x23e, 0x261, 0x284, 0x2ab },
    { 0x16a, 0x17f, 0x197, 0x1af, 0x1c8, 0x1e4, 0x200, 0x21f, 0x23f, 0x262, 0x286, 0x2ac }
};

static inline double s_apogeeFreq(double tone)
{
    uint_fast32_t noteI = (uint_fast32_t)(tone);
    int_fast32_t bendI = 0;
    int_fast32_t outHz = 0;
    double bendDec = tone - (int)tone;
    int_fast32_t octave;
    int_fast32_t scaleNote;

    bendI = (int_fast32_t)(bendDec * 32) + 32;

    noteI += bendI / 32;
    noteI -= 1;

    scaleNote = noteI % 12;
    octave = noteI / 12;

    outHz = s_apogee_freq_table[bendI % 32][scaleNote];

    while(octave > 1)
    {
        outHz *= 2;
        octave -= 1;
    }

    return (double)outHz;
}




/***************************************************************
 *            Windows 9x FM drivers frequency model            *
 ***************************************************************/

//static const double s_9x_opl_samplerate = 50000.0;
//static const double s_9x_opl_tune = 440.0;
static const uint_fast8_t s_9x_opl_pitchfrac = 8;

static const uint_fast32_t s_9x_opl_freq[12] =
{
    0xAB7, 0xB5A, 0xC07, 0xCBE, 0xD80, 0xE4D, 0xF27, 0x100E, 0x1102, 0x1205, 0x1318, 0x143A
};

static const int32_t s_9x_opl_uppitch = 31;
static const int32_t s_9x_opl_downpitch = 27;

static uint_fast32_t s_9x_opl_applypitch(uint_fast32_t freq, int_fast32_t pitch)
{
    int32_t diff;

    if(pitch > 0)
    {
        diff = (pitch * s_9x_opl_uppitch) >> s_9x_opl_pitchfrac;
        freq += (diff * freq) >> 15;
    }
    else if (pitch < 0)
    {
        diff = (-pitch * s_9x_opl_downpitch) >> s_9x_opl_pitchfrac;
        freq -= (diff * freq) >> 15;
    }

    return freq;
}

static inline double s_9xFreq(double tone)
{
    uint_fast32_t note = (uint_fast32_t)(tone);
    int_fast32_t bend;
    double bendDec = tone - (int)tone; // 0.0 ± 1.0 - one halftone

    uint_fast32_t freq;
    uint_fast32_t freqpitched;
    uint_fast32_t octave;

    uint_fast32_t bendMsb;
    uint_fast32_t bendLsb;

    bend = (int_fast32_t)(bendDec * 4096) + 8192; // convert to MIDI standard value

    bendMsb = (bend >> 7) & 0x7F;
    bendLsb = (bend & 0x7F);

    bend = (bendMsb << 9) | (bendLsb << 2);
    bend = (int16_t)(uint16_t)(bend + 0x8000);

    octave = note / 12;
    freq = s_9x_opl_freq[note % 12];
    if(octave < 5)
        freq >>= (5 - octave);
    else if (octave > 5)
        freq <<= (octave - 5);

    freqpitched = s_9x_opl_applypitch(freq, bend);
    freqpitched *= 2;

    return (double)freqpitched;
}




/***************************************************************
 *         HMI Sound Operating System frequency model          *
 ***************************************************************/

const size_t s_hmi_freqtable_size = 103;
static uint_fast32_t s_hmi_freqtable[s_hmi_freqtable_size] =
{
    0x0157, 0x016B, 0x0181, 0x0198, 0x01B0, 0x01CA, 0x01E5, 0x0202, 0x0220, 0x0241, 0x0263, 0x0287,
    0x0557, 0x056B, 0x0581, 0x0598, 0x05B0, 0x05CA, 0x05E5, 0x0602, 0x0620, 0x0641, 0x0663, 0x0687,
    0x0957, 0x096B, 0x0981, 0x0998, 0x09B0, 0x09CA, 0x09E5, 0x0A02, 0x0A20, 0x0A41, 0x0A63, 0x0A87,
    0x0D57, 0x0D6B, 0x0D81, 0x0D98, 0x0DB0, 0x0DCA, 0x0DE5, 0x0E02, 0x0E20, 0x0E41, 0x0E63, 0x0E87,
    0x1157, 0x116B, 0x1181, 0x1198, 0x11B0, 0x11CA, 0x11E5, 0x1202, 0x1220, 0x1241, 0x1263, 0x1287,
    0x1557, 0x156B, 0x1581, 0x1598, 0x15B0, 0x15CA, 0x15E5, 0x1602, 0x1620, 0x1641, 0x1663, 0x1687,
    0x1957, 0x196B, 0x1981, 0x1998, 0x19B0, 0x19CA, 0x19E5, 0x1A02, 0x1A20, 0x1A41, 0x1A63, 0x1A87,
    0x1D57, 0x1D6B, 0x1D81, 0x1D98, 0x1DB0, 0x1DCA, 0x1DE5, 0x1E02, 0x1E20, 0x1E41, 0x1E63, 0x1E87,
    0x1EAE, 0x1EB7, 0x1F02, 0x1F30, 0x1F60, 0x1F94, 0x1FCA
};

const size_t s_hmi_bendtable_size = 12;
static uint_fast32_t s_hmi_bendtable[s_hmi_bendtable_size] =
{
    0x144, 0x132, 0x121, 0x110, 0x101, 0xf8, 0xe5, 0xd8, 0xcc, 0xc1, 0xb6, 0xac
};

#define hmi_range_fix(formula, maxVal) \
    ( \
        (formula) < 0 ? \
        0 : \
        ( \
            (formula) >= (int32_t)maxVal ? \
            (int32_t)maxVal : \
            (formula) \
        )\
    )

static uint_fast32_t s_hmi_bend_calc(uint_fast32_t bend, int_fast32_t note)
{
    const int_fast32_t midi_bend_range = 1;
    uint_fast32_t bendFactor, outFreq, fmOctave, fmFreq, newFreq, idx;
    int_fast32_t noteMod12;

    note -= 12;
//    while(doNote >= 12) // ugly way to MOD 12
//        doNote -= 12;
    noteMod12 = (note % 12);

    outFreq = s_hmi_freqtable[note];

    fmOctave = outFreq & 0x1c00;
    fmFreq = outFreq & 0x3ff;

    if(bend < 64)
    {
        bendFactor = ((63 - bend) * 1000) >> 6;

        idx = hmi_range_fix(note - midi_bend_range, s_hmi_freqtable_size);
        newFreq = outFreq - s_hmi_freqtable[idx];

        if(newFreq > 719)
        {
            newFreq = fmFreq - s_hmi_bendtable[midi_bend_range - 1];
            newFreq &= 0x3ff;
        }

        newFreq = (newFreq * bendFactor) / 1000;
        outFreq -= newFreq;
    }
    else
    {
        bendFactor = ((bend - 64) * 1000) >> 6;

        idx = hmi_range_fix(note + midi_bend_range, s_hmi_freqtable_size);
        newFreq = s_hmi_freqtable[idx] - outFreq;

        if(newFreq > 719)
        {
            idx = hmi_range_fix(11 - noteMod12, s_hmi_bendtable_size);
            fmFreq = s_hmi_bendtable[idx];
            outFreq = (fmOctave + 1024) | fmFreq;

            idx = hmi_range_fix(note + midi_bend_range, s_hmi_freqtable_size);
            newFreq = s_hmi_freqtable[idx] - outFreq;
        }

        newFreq = (newFreq * bendFactor) / 1000;
        outFreq += newFreq;
    }

    return outFreq;
}
#undef hmi_range_fix

static inline double s_hmiFreq(double tone)
{
    int_fast32_t note = (int_fast32_t)(tone);
    double bendDec = tone - (int)tone; // 0.0 ± 1.0 - one halftone
    int_fast32_t bend;
    uint_fast32_t inFreq;
    uint_fast32_t freq;
    int_fast32_t octave;
    int_fast32_t octaveOffset = 0;

    bend = (int_fast32_t)(bendDec * 64.0) + 64;

    while(note < 12)
    {
        octaveOffset--;
        note += 12;
    }
    while(note > 114)
    {
        octaveOffset++;
        note -= 12;
    }

    if(bend == 64)
        inFreq = s_hmi_freqtable[note - 12];
    else
        inFreq = s_hmi_bend_calc(bend, note);

    freq = inFreq & 0x3FF;
    octave = (inFreq >> 10) & 0x07;

    octave += octaveOffset;

    while(octave > 0)
    {
        freq *= 2;
        octave -= 1;
    }

    return freq;
}




/***************************************************************
 *          Audio Interface Library frequency model            *
 ***************************************************************/

static const uint_fast16_t s_ail_freqtable[] = {
    0x02b2, 0x02b4, 0x02b7, 0x02b9, 0x02bc, 0x02be, 0x02c1, 0x02c3,
    0x02c6, 0x02c9, 0x02cb, 0x02ce, 0x02d0, 0x02d3, 0x02d6, 0x02d8,
    0x02db, 0x02dd, 0x02e0, 0x02e3, 0x02e5, 0x02e8, 0x02eb, 0x02ed,
    0x02f0, 0x02f3, 0x02f6, 0x02f8, 0x02fb, 0x02fe, 0x0301, 0x0303,
    0x0306, 0x0309, 0x030c, 0x030f, 0x0311, 0x0314, 0x0317, 0x031a,
    0x031d, 0x0320, 0x0323, 0x0326, 0x0329, 0x032b, 0x032e, 0x0331,
    0x0334, 0x0337, 0x033a, 0x033d, 0x0340, 0x0343, 0x0346, 0x0349,
    0x034c, 0x034f, 0x0352, 0x0356, 0x0359, 0x035c, 0x035f, 0x0362,
    0x0365, 0x0368, 0x036b, 0x036f, 0x0372, 0x0375, 0x0378, 0x037b,
    0x037f, 0x0382, 0x0385, 0x0388, 0x038c, 0x038f, 0x0392, 0x0395,
    0x0399, 0x039c, 0x039f, 0x03a3, 0x03a6, 0x03a9, 0x03ad, 0x03b0,
    0x03b4, 0x03b7, 0x03bb, 0x03be, 0x03c1, 0x03c5, 0x03c8, 0x03cc,
    0x03cf, 0x03d3, 0x03d7, 0x03da, 0x03de, 0x03e1, 0x03e5, 0x03e8,
    0x03ec, 0x03f0, 0x03f3, 0x03f7, 0x03fb, 0x03fe, 0xfe01, 0xfe03,
    0xfe05, 0xfe07, 0xfe08, 0xfe0a, 0xfe0c, 0xfe0e, 0xfe10, 0xfe12,
    0xfe14, 0xfe16, 0xfe18, 0xfe1a, 0xfe1c, 0xfe1e, 0xfe20, 0xfe21,
    0xfe23, 0xfe25, 0xfe27, 0xfe29, 0xfe2b, 0xfe2d, 0xfe2f, 0xfe31,
    0xfe34, 0xfe36, 0xfe38, 0xfe3a, 0xfe3c, 0xfe3e, 0xfe40, 0xfe42,
    0xfe44, 0xfe46, 0xfe48, 0xfe4a, 0xfe4c, 0xfe4f, 0xfe51, 0xfe53,
    0xfe55, 0xfe57, 0xfe59, 0xfe5c, 0xfe5e, 0xfe60, 0xfe62, 0xfe64,
    0xfe67, 0xfe69, 0xfe6b, 0xfe6d, 0xfe6f, 0xfe72, 0xfe74, 0xfe76,
    0xfe79, 0xfe7b, 0xfe7d, 0xfe7f, 0xfe82, 0xfe84, 0xfe86, 0xfe89,
    0xfe8b, 0xfe8d, 0xfe90, 0xfe92, 0xfe95, 0xfe97, 0xfe99, 0xfe9c,
    0xfe9e, 0xfea1, 0xfea3, 0xfea5, 0xfea8, 0xfeaa, 0xfead, 0xfeaf
};

static const uint_fast8_t s_ail_note_octave[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07
};

static const uint_fast8_t s_ail_note_halftone[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b
};

static inline double s_ailFreq(double tone)
{
    int_fast32_t note = (int_fast32_t)(tone);
    double bendDec = tone - (int)tone; // 0.0 ± 1.0 - one halftone
    int_fast32_t pitch;
    uint_fast16_t freq;
    int_fast32_t octave;
    int_fast32_t octaveOffset = 0;
    uint_fast8_t halftones;

    pitch = (int_fast32_t)(bendDec * 4096) + 8192; // convert to MIDI standard value
    pitch = ((pitch - 0x2000) / 0x20) * 2;

    note -= 12;

    while(note < 0)
    {
        octaveOffset--;
        note += 12;
    }
    while(note > 95)
    {
        octaveOffset++;
        note -= 12;
    }

    pitch += (((uint_fast8_t)note) << 8) + 8;
    pitch /= 16;
    while (pitch < 12 * 16) {
        pitch += 12 * 16;
    }
    while (pitch > 96 * 16 - 1) {
        pitch -= 12 * 16;
    }

    halftones = (s_ail_note_halftone[pitch >> 4] << 4) + (pitch & 0x0f);
    freq = s_ail_freqtable[halftones];
    octave = s_ail_note_octave[pitch >> 4];

    if((freq & 0x8000) == 0)
    {
        if (octave > 0) {
            octave--;
        } else {
            freq /= 2;
        }
    }

    freq &= 0x3FF;

    octave += octaveOffset;

    while(octave > 0)
    {
        freq *= 2;
        octave -= 1;
    }

    return freq;
}







enum
{
    MasterVolumeDefault = 127
};

enum
{
    OPL_PANNING_LEFT  = 0x10,
    OPL_PANNING_RIGHT = 0x20,
    OPL_PANNING_BOTH  = 0x30
};



static OplInstMeta makeEmptyInstrument()
{
    OplInstMeta ins;
    memset(&ins, 0, sizeof(OplInstMeta));
    ins.flags = OplInstMeta::Flag_NoSound;
    return ins;
}

const OplInstMeta OPL3::m_emptyInstrument = makeEmptyInstrument();

OPL3::OPL3() :
#ifdef ADLMIDI_ENABLE_HW_SERIAL
    m_serial(false),
    m_serialBaud(0),
    m_serialProtocol(0),
#endif
    m_softPanningSup(false),
    m_currentChipType((int)OPLChipBase::CHIPTYPE_OPL3),
    m_perChipChannels(OPL3_CHANNELS_RHYTHM_BASE),
    m_numChips(1),
    m_numFourOps(0),
    m_deepTremoloMode(false),
    m_deepVibratoMode(false),
    m_rhythmMode(false),
    m_softPanning(false),
    m_masterVolume(MasterVolumeDefault),
    m_musicMode(MODE_MIDI),
    m_volumeScale(VOLUME_Generic),
    m_channelAlloc(ADLMIDI_ChanAlloc_AUTO)
{
    m_insBankSetup.volumeModel = OPL3::VOLUME_Generic;
    m_insBankSetup.deepTremolo = false;
    m_insBankSetup.deepVibrato = false;
    m_insBankSetup.scaleModulators = false;
    m_insBankSetup.mt32defaults = false;

#ifdef DISABLE_EMBEDDED_BANKS
    m_embeddedBank = CustomBankTag;
#else
    setEmbeddedBank(0);
#endif
}

OPL3::~OPL3()
{
    m_curState.clear();

#ifdef ENABLE_HW_OPL_DOS
    silenceAll();
    writeRegI(0, 0x0BD, 0);
    writeRegI(0, 0x104, 0);
    writeRegI(0, 0x105, 0);
    silenceAll();
#endif
}

bool OPL3::setupLocked()
{
    return (m_musicMode == MODE_CMF ||
            m_musicMode == MODE_IMF ||
            m_musicMode == MODE_RSXX);
}

void OPL3::setEmbeddedBank(uint32_t bank)
{
#ifndef DISABLE_EMBEDDED_BANKS
    m_embeddedBank = bank;
    //Embedded banks are supports 128:128 GM set only
    m_insBanks.clear();

    if(bank >= static_cast<uint32_t>(g_embeddedBanksCount))
        return;

    const BanksDump::BankEntry &bankEntry = g_embeddedBanks[m_embeddedBank];
    m_insBankSetup.deepTremolo = ((bankEntry.bankSetup >> 8) & 0x01) != 0;
    m_insBankSetup.deepVibrato = ((bankEntry.bankSetup >> 8) & 0x02) != 0;
    m_insBankSetup.mt32defaults = ((bankEntry.bankSetup >> 8) & 0x04) != 0;
    m_insBankSetup.volumeModel = (bankEntry.bankSetup & 0xFF);
    m_insBankSetup.scaleModulators = false;

    for(int ss = 0; ss < 2; ss++)
    {
        bank_count_t maxBanks = ss ? bankEntry.banksPercussionCount : bankEntry.banksMelodicCount;
        bank_count_t banksOffset = ss ? bankEntry.banksOffsetPercussive : bankEntry.banksOffsetMelodic;

        for(bank_count_t bankID = 0; bankID < maxBanks; bankID++)
        {
            size_t bankIndex = g_embeddedBanksMidiIndex[banksOffset + bankID];
            const BanksDump::MidiBank &bankData = g_embeddedBanksMidi[bankIndex];
            size_t bankMidiIndex = static_cast<size_t>((bankData.msb * 256) + bankData.lsb) + (ss ? static_cast<size_t>(PercussionTag) : 0);
            Bank &bankTarget = m_insBanks[bankMidiIndex];

            for(size_t instId = 0; instId < 128; instId++)
            {
                midi_bank_idx_t instIndex = bankData.insts[instId];
                if(instIndex < 0)
                {
                    bankTarget.ins[instId].flags = OplInstMeta::Flag_NoSound;
                    continue;
                }
                BanksDump::InstrumentEntry instIn = g_embeddedBanksInstruments[instIndex];
                OplInstMeta &instOut = bankTarget.ins[instId];

                adlFromInstrument(instIn, instOut);
            }
        }
    }

#else
    ADL_UNUSED(bank);
#endif
}

void OPL3::writeReg(size_t chip, uint16_t address, uint8_t value)
{
    m_chips[chip]->writeReg(address, value);
}

void OPL3::writeRegI(size_t chip, uint32_t address, uint32_t value)
{
    m_chips[chip]->writeReg(static_cast<uint16_t>(address), static_cast<uint8_t>(value));
}

void OPL3::writePan(size_t chip, uint32_t address, uint32_t value)
{
    m_chips[chip]->writePan(static_cast<uint16_t>(address), static_cast<uint8_t>(value));
}


void OPL3::noteOff(size_t c)
{
    size_t chip = c / NUM_OF_CHANNELS, cc = c % NUM_OF_CHANNELS;

    if(cc >= OPL3_CHANNELS_RHYTHM_BASE)
    {
        m_regBD[chip] &= ~(0x10 >> (cc - OPL3_CHANNELS_RHYTHM_BASE));
        writeRegI(chip, 0xBD, m_regBD[chip]);
        return;
    }
    else if(m_currentChipType == OPLChipBase::CHIPTYPE_OPL2 && cc >= NUM_OF_OPL2_CHANNELS)
        return;

    writeRegI(chip, 0xB0 + g_channelsMap[cc], m_keyBlockFNumCache[c] & 0xDF);
}

void OPL3::noteOn(size_t c1, size_t c2, double tone)
{
    size_t chip = c1 / NUM_OF_CHANNELS, cc1 = c1 % NUM_OF_CHANNELS, cc2 = c2 % NUM_OF_CHANNELS;
    uint32_t octave = 0, ftone = 0, mul_offset = 0;
    // Hertz range: 0..131071
    double hertz;

    if(tone < 0.0)
        tone = 0.0; // Lower than 0 is impossible!

    // Use different frequency formulas in depend on a volume model
    switch(m_volumeScale)
    {
    case VOLUME_DMX:
    case VOLUME_DMX_FIXED:
        hertz = s_dmxFreq(tone);
        break;

    case VOLUME_APOGEE:
    case VOLUME_APOGEE_FIXED:
        hertz = s_apogeeFreq(tone);
        break;

    case VOLUME_9X:
    case VOLUME_9X_GENERIC_FM:
        hertz = s_9xFreq(tone);
        break;

    case VOLUME_HMI:
    case VOLUME_HMI_OLD:
        hertz = s_hmiFreq(tone);
        break;

    case VOLUME_AIL:
        hertz = s_ailFreq(tone);
        break;

    default:
        hertz = s_commonFreq(tone);
    }

    if(hertz < 0)
        return;

    //Basic range until max of octaves reaching
    while((hertz >= 1023.5) && (octave < 0x1C00))
    {
        hertz /= 2.0;    // Calculate octave
        octave += 0x400;
    }
    //Extended range, rely on frequency multiplication increment
    while(hertz >= 1022.75)
    {
        hertz /= 2.0;    // Calculate octave
        mul_offset++;
    }

    ftone = octave + static_cast<uint32_t>(hertz /*+ 0.5*/);
    uint32_t chn = m_musicMode == MODE_CMF ? g_channelsMapPan[cc1] : g_channelsMap[cc1];
    const OplTimbre &patch1 = m_insCache[c1];
    const OplTimbre &patch2 = m_insCache[c2 < m_insCache.size() ? c2 : 0];

    if(cc1 < OPL3_CHANNELS_RHYTHM_BASE)
    {
        ftone += 0x2000u; /* Key-ON [KON] */

        const bool natural_4op = (m_channelCategory[c1] == ChanCat_4op_First);
        const size_t opsCount = natural_4op ? 4 : 2;
        const uint16_t op_addr[4] =
        {
            g_operatorsMap[cc1 * 2 + 0], g_operatorsMap[cc1 * 2 + 1],
            g_operatorsMap[cc2 * 2 + 0], g_operatorsMap[cc2 * 2 + 1]
        };
        const uint32_t ops[4] =
        {
            patch1.modulator_E862 & 0xFF,
            patch1.carrier_E862 & 0xFF,
            patch2.modulator_E862 & 0xFF,
            patch2.carrier_E862 & 0xFF
        };

        for(size_t op = 0; op < opsCount; op++)
        {
            if(op_addr[op] == 0xFFF)
                continue;
            if(mul_offset > 0)
            {
                uint32_t dt  = ops[op] & 0xF0;
                uint32_t mul = ops[op] & 0x0F;
                if((mul + mul_offset) > 0x0F)
                {
                    mul_offset = 0;
                    mul = 0x0F;
                }
                writeRegI(chip, 0x20 + op_addr[op],  (dt | (mul + mul_offset)) & 0xFF);
            }
            else
            {
                writeRegI(chip, 0x20 + op_addr[op],  ops[op] & 0xFF);
            }
        }
    }

    if(chn != 0xFFF)
    {
        writeRegI(chip , 0xA0 + chn, (ftone & 0xFF));
        writeRegI(chip , 0xB0 + chn, (ftone >> 8));
        m_keyBlockFNumCache[c1] = (ftone >> 8);
    }

    if(cc1 >= OPL3_CHANNELS_RHYTHM_BASE)
    {
        m_regBD[chip ] |= (0x10 >> (cc1 - OPL3_CHANNELS_RHYTHM_BASE));
        writeRegI(chip , 0x0BD, m_regBD[chip ]);
        //x |= 0x800; // for test
    }
}

static inline uint_fast32_t brightnessToOPL(uint_fast32_t brightness)
{
    double b = static_cast<double>(brightness);
    double ret = ::round(127.0 * ::sqrt(b * (1.0 / 127.0))) / 2.0;
    return static_cast<uint_fast32_t>(ret);
}

void OPL3::touchNote(size_t c,
                     uint_fast32_t velocity,
                     uint_fast32_t channelVolume,
                     uint_fast32_t channelExpression,
                     uint_fast32_t brightness,
                     bool isDrum)
{
    size_t chip = c / NUM_OF_CHANNELS, cc = c % NUM_OF_CHANNELS;
    const OplTimbre &adli = m_insCache[c];
    size_t cmf_offset = ((m_musicMode == MODE_CMF) && cc >= OPL3_CHANNELS_RHYTHM_BASE) ? 10 : 0;
    uint16_t o1 = g_operatorsMap[cc * 2 + 0 + cmf_offset];
    uint16_t o2 = g_operatorsMap[cc * 2 + 1 + cmf_offset];
    uint8_t  srcMod = adli.modulator_40,
             srcCar = adli.carrier_40;
    uint32_t mode = 1; // 2-op AM

    uint_fast32_t kslMod = srcMod & 0xC0;
    uint_fast32_t kslCar = srcCar & 0xC0;
    uint_fast32_t tlMod = srcMod & 0x3F;
    uint_fast32_t tlCar = srcCar & 0x3F;

    uint_fast32_t modulator;
    uint_fast32_t carrier;

    uint_fast32_t volume = 0;
    uint_fast32_t midiVolume = 0;

    bool do_modulator = false;
    bool do_carrier = true;

    static const bool do_ops[10][2] =
    {
        { false, true },  /* 2 op FM */
        { true,  true },  /* 2 op AM */
        { false, false }, /* 4 op FM-FM ops 1&2 */
        { true,  false }, /* 4 op AM-FM ops 1&2 */
        { false, true  }, /* 4 op FM-AM ops 1&2 */
        { true,  false }, /* 4 op AM-AM ops 1&2 */
        { false, true  }, /* 4 op FM-FM ops 3&4 */
        { false, true  }, /* 4 op AM-FM ops 3&4 */
        { false, true  }, /* 4 op FM-AM ops 3&4 */
        { true,  true  }  /* 4 op AM-AM ops 3&4 */
    };

    if(m_currentChipType == OPLChipBase::CHIPTYPE_OPL2 && m_channelCategory[c] == ChanCat_None)
        return; // Do nothing

    // ------ Mix volumes and compute average ------

    switch(m_volumeScale)
    {
    default:
    case VOLUME_Generic:
    {
        volume = velocity * m_masterVolume *
                 channelVolume * channelExpression;

        /* If the channel has arpeggio, the effective volume of
             * *this* instrument is actually lower due to timesharing.
             * To compensate, add extra volume that corresponds to the
             * time this note is *not* heard.
             * Empirical tests however show that a full equal-proportion
             * increment sounds wrong. Therefore, using the square root.
             */
        //volume = (int)(volume * std::sqrt( (double) ch[c].users.size() ));
        const double c1 = 11.541560327111707;
        const double c2 = 1.601379199767093e+02;
        const uint_fast32_t minVolume = 1108075; // 8725 * 127

        // The formula below: SOLVE(V=127^4 * 2^( (A-63.49999) / 8), A)
        if(volume > minVolume)
        {
            double lv = std::log(static_cast<double>(volume));
            volume = static_cast<uint_fast32_t>(lv * c1 - c2);
        }
        else
            volume = 0;
    }
    break;

    case VOLUME_NATIVE:
    {
        volume = velocity * channelVolume * channelExpression;
        // 4096766 = (127 * 127 * 127) / 2
        volume = (volume * m_masterVolume) / 4096766;
    }
    break;

    case VOLUME_DMX:
    case VOLUME_DMX_FIXED:
    {
        volume = (channelVolume * channelExpression * m_masterVolume) / 16129;
        volume = (s_dmx_volume_model[volume] + 1) << 1;
        volume = (s_dmx_volume_model[(velocity < 128) ? velocity : 127] * volume) >> 9;
    }
    break;

    case VOLUME_APOGEE:
    case VOLUME_APOGEE_FIXED:
    {
        midiVolume = (channelVolume * channelExpression * m_masterVolume / 16129);
    }
    break;

    case VOLUME_9X:
    {
        volume = (channelVolume * channelExpression * m_masterVolume) / 16129;
        volume = s_w9x_sb16_volume_model[volume >> 2];
    }
    break;

    case VOLUME_9X_GENERIC_FM:
    {
        volume = (channelVolume * channelExpression * m_masterVolume) / 16129;
        volume = s_w9x_generic_fm_volume_model[volume >> 2];
    }
    break;

    case VOLUME_AIL:
    {
        midiVolume = (channelVolume * channelExpression) * 2;
        midiVolume >>= 8;
        if(midiVolume != 0)
            midiVolume++;

        velocity = (velocity & 0x7F) >> 3;
        velocity = s_ail_vel_graph[velocity];

        midiVolume = (midiVolume * velocity) * 2;
        midiVolume >>= 8;
        if(midiVolume != 0)
            midiVolume++;

        if(m_masterVolume < 127)
            midiVolume = (midiVolume * m_masterVolume) / 127;
    }
    break;

    case VOLUME_HMI:
    case VOLUME_HMI_OLD:
    {
        volume = (channelVolume * channelExpression * m_masterVolume) / 16129;
        volume = (((volume * 128) / 127) * velocity) >> 7;
        volume = s_hmi_volume_table[volume >> 1];
    }
    break;
    }

    if(volume > 63)
        volume = 63;

    if(midiVolume > 127)
        midiVolume = 127;


    if(m_channelCategory[c] == ChanCat_Regular ||
       m_channelCategory[c] == ChanCat_Rhythm_Bass)
    {
        mode = adli.feedconn & 1; // 2-op FM or 2-op AM
    }
    else if(m_channelCategory[c] == ChanCat_4op_First ||
            m_channelCategory[c] == ChanCat_4op_Second)
    {
        const OplTimbre *i0, *i1;

        if(m_channelCategory[c] == ChanCat_4op_First)
        {
            i0 = &adli;
            i1 = &m_insCache[c + 3];
            mode = 2; // 4-op xx-xx ops 1&2
        }
        else
        {
            i0 = &m_insCache[c - 3];
            i1 = &adli;
            mode = 6; // 4-op xx-xx ops 3&4
        }

        mode += (i0->feedconn & 1) + (i1->feedconn & 1) * 2;
    }

    do_modulator = do_ops[mode][0] || m_scaleModulators;
    do_carrier   = do_ops[mode][1] || m_scaleModulators;

    // ------ Compute the total level register output data ------

    if(m_musicMode == MODE_RSXX)
    {
        tlCar -= volume / 2;
    }
    else if(m_volumeScale == Synth::VOLUME_APOGEE ||
            m_volumeScale == Synth::VOLUME_APOGEE_FIXED)
    {
        // volume = ((64 * (velocity + 0x80)) * volume) >> 15;

        if(do_carrier)
        {
            tlCar = 63 - tlCar;
            tlCar *= velocity + 0x80;
            tlCar = (midiVolume * tlCar) >> 15;
            tlCar = tlCar ^ 63;
        }

        if(do_modulator)
        {
            uint_fast32_t mod = tlCar;

            tlMod = 63 - tlMod;
            tlMod *= velocity + 0x80;

            if(m_volumeScale == Synth::VOLUME_APOGEE_FIXED || mode > 1)
                mod = tlMod; // Fix the AM voices bug

            // NOTE: Here is a bug of Apogee Sound System that makes modulator
            // to not work properly on AM instruments. The fix of this bug, you
            // need to replace the tlCar with tmMod in this formula.
            // Don't do the bug on 4-op voices.
            tlMod = (midiVolume * mod) >> 15;

            tlMod ^= 63;
        }
    }
    else if(m_volumeScale == Synth::VOLUME_DMX && mode <= 1)
    {
        tlCar = (63 - volume);

        if(do_modulator)
        {
            if(tlMod < tlCar)
                tlMod = tlCar;
        }
    }
    else if(m_volumeScale == Synth::VOLUME_9X)
    {
        if(do_carrier)
            tlCar += volume + s_w9x_sb16_volume_model[velocity >> 2];
        if(do_modulator)
            tlMod += volume + s_w9x_sb16_volume_model[velocity >> 2];

        if(tlCar > 0x3F)
            tlCar = 0x3F;
        if(tlMod > 0x3F)
            tlMod = 0x3F;
    }
    else if(m_volumeScale == Synth::VOLUME_9X_GENERIC_FM)
    {
        if(do_carrier)
            tlCar += volume + s_w9x_generic_fm_volume_model[velocity >> 2];
        if(do_modulator)
            tlMod += volume + s_w9x_generic_fm_volume_model[velocity >> 2];

        if(tlCar > 0x3F)
            tlCar = 0x3F;
        if(tlMod > 0x3F)
            tlMod = 0x3F;
    }
    else if(m_volumeScale == Synth::VOLUME_AIL)
    {
        uint_fast32_t v0_val = (~srcMod) & 0x3f;
        uint_fast32_t v1_val = (~srcCar) & 0x3f;

        if(do_modulator)
            v0_val = (v0_val * midiVolume) / 127;
        if(do_carrier)
            v1_val = (v1_val * midiVolume) / 127;

        tlMod = (~v0_val) & 0x3F;
        tlCar = (~v1_val) & 0x3F;
    }
    else if(m_volumeScale == Synth::VOLUME_HMI)
    {
        uint_fast32_t vol;

        if(do_modulator)
        {
            vol = (64 - volume) << 1;
            vol *= (64 - tlMod);
            tlMod = (8192 - vol) >> 7;
        }

        if(do_carrier)
        {
            vol = (64 - volume) << 1;
            vol *= (64 - tlCar);
            tlCar = (8192 - vol) >> 7;
        }
    }
    else if(m_volumeScale == Synth::VOLUME_HMI_OLD)
    {
        uint_fast32_t vol;

        if(adli.feedconn == 0 && !isDrum)
        {
            vol = (channelVolume * channelExpression * 64) / 16129;
            vol = (((vol * 128) / 127) * velocity) >> 7;
            vol = s_hmi_volume_table[vol >> 1];

            vol = (64 - vol) << 1;
            vol *= (64 - tlCar);
            tlMod = (8192 - vol) >> 7;
        }

        if(isDrum) // TODO: VERIFY A CORRECTNESS OF THIS!!!
            vol = (64 - s_hmi_volume_table[velocity >> 1]) << 1;
        else
            vol = (64 - volume) << 1;

        vol *= (64 - tlCar);
        tlCar = (8192 - vol) >> 7;
    }
    else
    {
        if(do_modulator)
            tlMod = 63 - volume + (volume * tlMod) / 63;
        if(do_carrier)
            tlCar = 63 - volume + (volume * tlCar) / 63;
    }

    if(brightness != 127 && !isDrum)
    {
        brightness = brightnessToOPL(brightness);
        if(!do_modulator)
            tlMod = 63 - brightness + (brightness * tlMod) / 63;
        if(!do_carrier)
            tlCar = 63 - brightness + (brightness * tlCar) / 63;
    }

    modulator = (kslMod & 0xC0) | (tlMod & 63);
    carrier = (kslCar & 0xC0) | (tlCar & 63);

    if(o1 != 0xFFF)
        writeRegI(chip, 0x40 + o1, modulator);
    if(o2 != 0xFFF)
        writeRegI(chip, 0x40 + o2, carrier);

    // Correct formula (ST3, AdPlug):
    //   63-((63-(instrvol))/63)*chanvol
    // Reduces to (tested identical):
    //   63 - chanvol + chanvol*instrvol/63
    // Also (slower, floats):
    //   63 + chanvol * (instrvol / 63.0 - 1)
}

void OPL3::setPatch(size_t c, const OplTimbre &instrument)
{
    size_t chip = c / NUM_OF_CHANNELS, cc = c % NUM_OF_CHANNELS;
    static const uint8_t data[4] = {0x20, 0x60, 0x80, 0xE0};
    m_insCache[c] = instrument;
    size_t cmf_offset = ((m_musicMode == MODE_CMF) && (cc >= OPL3_CHANNELS_RHYTHM_BASE)) ? 10 : 0;
    uint16_t o1 = g_operatorsMap[cc * 2 + 0 + cmf_offset];
    uint16_t o2 = g_operatorsMap[cc * 2 + 1 + cmf_offset];
    unsigned x = instrument.modulator_E862, y = instrument.carrier_E862;
    uint8_t fbconn = 0;
    uint16_t fbconn_reg = 0x00;

    for(size_t a = 0; a < 4; ++a, x >>= 8, y >>= 8)
    {
        if(o1 != 0xFFF)
            writeRegI(chip, data[a] + o1, x & 0xFF);
        if(o2 != 0xFFF)
            writeRegI(chip, data[a] + o2, y & 0xFF);
    }

    if(g_channelsMapFBConn[cc] != 0xFFF)
    {
        fbconn |= instrument.feedconn;
        fbconn_reg = 0xC0 + g_channelsMapFBConn[cc];
    }

    if(m_currentChipType != OPLChipBase::CHIPTYPE_OPL2 && g_channelsMapPan[cc] != 0xFFF)
    {
        fbconn |= (m_regC0[c] & OPL_PANNING_BOTH);
        if(!fbconn_reg)
            fbconn_reg = 0xC0 + g_channelsMapPan[cc];
    }

    if(fbconn_reg != 0x00)
        writeRegI(chip, fbconn_reg, fbconn);
}

void OPL3::setPan(size_t c, uint8_t value)
{
    size_t chip = c / NUM_OF_CHANNELS, cc = c % NUM_OF_CHANNELS;

    if(m_currentChipType == OPLChipBase::CHIPTYPE_OPL2)
    {
        m_regC0[c] = OPL_PANNING_BOTH;
        return; // OPL2 chip doesn't support panning at all
    }

    if(g_channelsMapPan[cc] != 0xFFF)
    {
#ifndef ENABLE_HW_OPL_DOS
        if (m_softPanningSup && m_softPanning)
        {
            writePan(chip, g_channelsMapPan[cc], value);
            m_regC0[c] = OPL_PANNING_BOTH;
            writeRegI(chip, 0xC0 + g_channelsMapPan[cc], m_insCache[c].feedconn | OPL_PANNING_BOTH);
        }
        else
        {
#endif
            uint8_t panning = 0;
            if(value  < 64 + 16) panning |= OPL_PANNING_LEFT;
            if(value >= 64 - 16) panning |= OPL_PANNING_RIGHT;
            m_regC0[c] = panning;
            writePan(chip, g_channelsMapPan[cc], 64);
            writeRegI(chip, 0xC0 + g_channelsMapPan[cc], m_insCache[c].feedconn | panning);
#ifndef ENABLE_HW_OPL_DOS
        }
#endif
    }
}

void OPL3::silenceAll() // Silence all OPL channels.
{
    for(size_t c = 0; c < m_numChannels; ++c)
    {
        noteOff(c);
        touchNote(c, 0, 0, 0);
    }
}

void OPL3::updateChannelCategories()
{
    const uint32_t fours = (m_currentChipType != OPLChipBase::CHIPTYPE_OPL2) ? m_numFourOps : 0;

    for(uint32_t chip = 0, fours_left = fours; chip < m_numChips; ++chip)
    {
        m_regBD[chip] = (m_deepTremoloMode * 0x80 + m_deepVibratoMode * 0x40 + m_rhythmMode * 0x20);
        writeRegI(chip, 0x0BD, m_regBD[chip]);
        uint32_t fours_this_chip = std::min(fours_left, static_cast<uint32_t>(6u));
        if(m_currentChipType != OPLChipBase::CHIPTYPE_OPL2)
            writeRegI(chip, 0x104, (1 << fours_this_chip) - 1);
        fours_left -= fours_this_chip;
    }

    for(size_t p = 0, a = 0, n = m_numChips; a < n; ++a)
    {
        for(size_t b = 0; b < OPL3_CHANNELS_RHYTHM_BASE; ++b, ++p)
        {
            if(m_currentChipType == OPLChipBase::CHIPTYPE_OPL2 && b >= NUM_OF_OPL2_CHANNELS)
                m_channelCategory[p] = ChanCat_None;
            else
                m_channelCategory[p] = ChanCat_Regular;

            if(m_rhythmMode && b >= 6 && b < 9)
                m_channelCategory[p] = ChanCat_Rhythm_Secondary;
        }

        if(!m_rhythmMode)
        {
            for(size_t b = 0; b < NUM_OF_RM_CHANNELS; ++b)
                m_channelCategory[p++] = ChanCat_Rhythm_Secondary;
        }
        else
        {
            for(size_t b = 0; b < NUM_OF_RM_CHANNELS; ++b)
                m_channelCategory[p++] = (ChanCat_Rhythm_Bass + b);
        }
    }

    uint32_t nextfour = 0;
    for(uint32_t a = 0; a < fours; ++a)
    {
        m_channelCategory[nextfour] = ChanCat_4op_First;
        m_channelCategory[nextfour + 3] = ChanCat_4op_Second;

        switch(a % 6)
        {
        case 0:
        case 1:
            nextfour += 1;
            break;
        case 2:
            nextfour += 9 - 2;
            break;
        case 3:
        case 4:
            nextfour += 1;
            break;
        case 5:
            nextfour += NUM_OF_CHANNELS - 9 - 2;
            break;
        }
    }

/**/
/*
    In two-op mode, channels 0..8 go as follows:
                  Op1[port]  Op2[port]
      Channel 0:  00  00     03  03
      Channel 1:  01  01     04  04
      Channel 2:  02  02     05  05
      Channel 3:  06  08     09  0B
      Channel 4:  07  09     10  0C
      Channel 5:  08  0A     11  0D
      Channel 6:  12  10     15  13
      Channel 7:  13  11     16  14
      Channel 8:  14  12     17  15
    In four-op mode, channels 0..8 go as follows:
                  Op1[port]  Op2[port]  Op3[port]  Op4[port]
      Channel 0:  00  00     03  03     06  08     09  0B
      Channel 1:  01  01     04  04     07  09     10  0C
      Channel 2:  02  02     05  05     08  0A     11  0D
      Channel 3:  CHANNEL 0 SECONDARY
      Channel 4:  CHANNEL 1 SECONDARY
      Channel 5:  CHANNEL 2 SECONDARY
      Channel 6:  12  10     15  13
      Channel 7:  13  11     16  14
      Channel 8:  14  12     17  15
     Same goes principally for channels 9-17 respectively.
    */
}

void OPL3::commitDeepFlags()
{
    for(size_t chip = 0; chip < m_numChips; ++chip)
    {
        m_regBD[chip] = (m_deepTremoloMode * 0x80 + m_deepVibratoMode * 0x40 + m_rhythmMode * 0x20);
        writeRegI(chip, 0x0BD, m_regBD[chip]);
    }
}

void OPL3::setVolumeScaleModel(ADLMIDI_VolumeModels volumeModel)
{
    switch(volumeModel)
    {
    default:
    case ADLMIDI_VolumeModel_AUTO://Do nothing until restart playing
        break;

    case ADLMIDI_VolumeModel_Generic:
        m_volumeScale = OPL3::VOLUME_Generic;
        break;

    case ADLMIDI_VolumeModel_NativeOPL3:
        m_volumeScale = OPL3::VOLUME_NATIVE;
        break;

    case ADLMIDI_VolumeModel_DMX:
        m_volumeScale = OPL3::VOLUME_DMX;
        break;

    case ADLMIDI_VolumeModel_APOGEE:
        m_volumeScale = OPL3::VOLUME_APOGEE;
        break;

    case ADLMIDI_VolumeModel_9X:
        m_volumeScale = OPL3::VOLUME_9X;
        break;

    case ADLMIDI_VolumeModel_DMX_Fixed:
        m_volumeScale = OPL3::VOLUME_DMX_FIXED;
        break;

    case ADLMIDI_VolumeModel_APOGEE_Fixed:
        m_volumeScale = OPL3::VOLUME_APOGEE_FIXED;
        break;

    case ADLMIDI_VolumeModel_AIL:
        m_volumeScale = OPL3::VOLUME_AIL;
        break;

    case ADLMIDI_VolumeModel_9X_GENERIC_FM:
        m_volumeScale = OPL3::VOLUME_9X_GENERIC_FM;
        break;

    case ADLMIDI_VolumeModel_HMI:
        m_volumeScale = OPL3::VOLUME_HMI;
        break;

    case ADLMIDI_VolumeModel_HMI_OLD:
        m_volumeScale = OPL3::VOLUME_HMI_OLD;
        break;
    }
}

ADLMIDI_VolumeModels OPL3::getVolumeScaleModel()
{
    switch(m_volumeScale)
    {
    default:
    case OPL3::VOLUME_Generic:
        return ADLMIDI_VolumeModel_Generic;
    case OPL3::VOLUME_NATIVE:
        return ADLMIDI_VolumeModel_NativeOPL3;
    case OPL3::VOLUME_DMX:
        return ADLMIDI_VolumeModel_DMX;
    case OPL3::VOLUME_APOGEE:
        return ADLMIDI_VolumeModel_APOGEE;
    case OPL3::VOLUME_9X:
        return ADLMIDI_VolumeModel_9X;
    case OPL3::VOLUME_DMX_FIXED:
        return ADLMIDI_VolumeModel_DMX_Fixed;
    case OPL3::VOLUME_APOGEE_FIXED:
        return ADLMIDI_VolumeModel_APOGEE_Fixed;
    case OPL3::VOLUME_AIL:
        return ADLMIDI_VolumeModel_AIL;
    case OPL3::VOLUME_9X_GENERIC_FM:
        return ADLMIDI_VolumeModel_9X_GENERIC_FM;
    case OPL3::VOLUME_HMI:
        return ADLMIDI_VolumeModel_HMI;
    case OPL3::VOLUME_HMI_OLD:
        return ADLMIDI_VolumeModel_HMI_OLD;
    }
}

void OPL3::clearChips()
{
    for(size_t i = 0; i < m_chips.size(); i++)
        m_chips[i].reset(NULL);
    m_chips.clear();
}

void OPL3::reset(int emulator, unsigned long PCM_RATE, void *audioTickHandler)
{
    bool rebuild_needed = m_curState.cmp(emulator, m_numChips);

    if(rebuild_needed)
        clearChips();

#if !defined(ADLMIDI_AUDIO_TICK_HANDLER)
    (void)audioTickHandler;
#endif

    const struct OplTimbre defaultInsCache = { 0x1557403,0x005B381, 0x49,0x80, 0x4, +0 };

    if(rebuild_needed)
    {
        m_insCache.clear();
        m_keyBlockFNumCache.clear();
        m_regBD.clear();
        m_regC0.clear();
        m_channelCategory.clear();
        m_chips.resize(m_numChips, AdlMIDI_SPtr<OPLChipBase>());
    }
    else
    {
        adl_fill_vector<OplTimbre>(m_insCache, defaultInsCache);
        adl_fill_vector<uint32_t>(m_channelCategory, 0);
        adl_fill_vector<uint32_t>(m_keyBlockFNumCache, 0);
        adl_fill_vector<uint32_t>(m_regBD, 0);
        adl_fill_vector<uint8_t>(m_regC0, OPL_PANNING_BOTH);
    }

#ifdef ADLMIDI_ENABLE_HW_SERIAL
    if(emulator >= 0) // If less than zero - it's hardware synth!
        m_serial = false;
#endif

    if(rebuild_needed)
    {
        m_numChannels = m_numChips * NUM_OF_CHANNELS;
        m_insCache.resize(m_numChannels, defaultInsCache);
        m_channelCategory.resize(m_numChannels, 0);
        m_keyBlockFNumCache.resize(m_numChannels,   0);
        m_regBD.resize(m_numChips,    0);
        m_regC0.resize(m_numChips * m_numChannels, OPL_PANNING_BOTH);
    }

    if(!rebuild_needed)
    {
        bool newRate = m_curState.cmp_rate(PCM_RATE);

        for(size_t i = 0; i < m_numChips; ++i)
        {
            if(newRate)
                m_chips[i]->setRate(PCM_RATE);

            initChip(i);
        }
    }
    else for(size_t i = 0; i < m_numChips; ++i)
    {
#ifdef ADLMIDI_ENABLE_HW_SERIAL
        if(emulator < 0)
        {
            OPL_SerialPort *serial = new OPL_SerialPort;
            serial->connectPort(m_serialName, m_serialBaud, m_serialProtocol);
            m_chips[i].reset(serial);
            initChip(i);
            break; // Only one REAL chip!
        }
#endif

        OPLChipBase *chip;
#ifdef ENABLE_HW_OPL_DOS
        chip = new DOS_HW_OPL();

#else // ENABLE_HW_OPL_DOS
        switch(emulator)
        {
        default:
            assert(false);
            abort();
#ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
        case ADLMIDI_EMU_NUKED: /* Latest Nuked OPL3 */
            chip = new NukedOPL3;
            break;
        case ADLMIDI_EMU_NUKED_174: /* Old Nuked OPL3 1.4.7 modified and optimized */
            chip = new NukedOPL3v174;
            break;
#endif
#ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
        case ADLMIDI_EMU_DOSBOX:
            chip = new DosBoxOPL3;
            break;
#endif
#ifndef ADLMIDI_DISABLE_OPAL_EMULATOR
        case ADLMIDI_EMU_OPAL:
            chip = new OpalOPL3;
            break;
#endif
#ifndef ADLMIDI_DISABLE_JAVA_EMULATOR
        case ADLMIDI_EMU_JAVA:
            chip = new JavaOPL3;
            break;
#endif
#ifndef ADLMIDI_DISABLE_ESFMU_EMULATOR
        case ADLMIDI_EMU_ESFMu:
            chip = new ESFMuOPL3;
            break;
#endif
#ifndef ADLMIDI_DISABLE_MAME_OPL2_EMULATOR
        case ADLMIDI_EMU_MAME_OPL2:
            chip = new MameOPL2;
            break;
#endif
#ifndef ADLMIDI_DISABLE_YMFM_EMULATOR
        case ADLMIDI_EMU_YMFM_OPL2:
            chip = new YmFmOPL2;
            break;
        case ADLMIDI_EMU_YMFM_OPL3:
            chip = new YmFmOPL3;
            break;
#endif
#ifdef ADLMIDI_ENABLE_OPL2_LLE_EMULATOR
        case ADLMIDI_EMU_NUKED_OPL2_LLE:
            chip = new Ym3812LLEOPL2;
            break;
#endif
#ifdef ADLMIDI_ENABLE_OPL3_LLE_EMULATOR
        case ADLMIDI_EMU_NUKED_OPL3_LLE:
            chip = new Ymf262LLEOPL3;
            break;
#endif
        }
#endif // ENABLE_HW_OPL_DOS

        m_chips[i].reset(chip);
        chip->setChipId((uint32_t)i);
        chip->setRate((uint32_t)PCM_RATE);

#ifndef ENABLE_HW_OPL_DOS
        if(m_runAtPcmRate)
            chip->setRunningAtPcmRate(true);
#endif

#   if defined(ADLMIDI_AUDIO_TICK_HANDLER) && !defined(ENABLE_HW_OPL_DOS)
        chip->setAudioTickHandlerInstance(audioTickHandler);
#   endif

        initChip(i);
    }

    updateChannelCategories();
    silenceAll();
}

void OPL3::initChip(size_t chip)
{
    static const uint16_t data_opl3[] =
    {
        0x004, 96, 0x004, 128,        // Pulse timer
        0x105, 0, 0x105, 1, 0x105, 0, // Pulse OPL3 enable
        0x001, 32, 0x105, 1,          // Enable wave, OPL3 extensions
        0x08, 0                       // CSW/Note Sel
    };
    static const size_t data_opl3_size = sizeof(data_opl3) / sizeof(uint16_t);

    static const uint16_t data_opl2[] =
    {
        0x004, 96, 0x004, 128,          // Pulse timer
        0x001, 32,                      // Enable wave
        0x08, 0                         // CSW/Note Sel
    };
    static const size_t data_opl2_size = sizeof(data_opl2) / sizeof(uint16_t);

    // Report does emulator/interface supports full-panning stereo or not
    if(chip == 0)
    {
        m_softPanningSup = m_chips[chip]->hasFullPanning();
        m_currentChipType = (int)m_chips[chip]->chipType();
        m_perChipChannels = OPL3_CHANNELS_RHYTHM_BASE;

        if(m_currentChipType == OPLChipBase::CHIPTYPE_OPL2)
        {
            m_perChipChannels = NUM_OF_OPL2_CHANNELS;
            m_numFourOps = 0; // Can't have 4ops on OPL2 chip
        }
    }

    /* Clean-up channels from any playing junk sounds */
    for(size_t a = 0; a < m_perChipChannels; ++a)
    {
        writeRegI(chip, 0x20 + g_operatorsMap[a * 2], 0x00);
        writeRegI(chip, 0x20 + g_operatorsMap[(a * 2) + 1], 0x00);
        writeRegI(chip, 0xA0 + g_channelsMap[a], 0x00);
        writeRegI(chip, 0xB0 + g_channelsMap[a], 0x00);
    }

    if(m_currentChipType == OPLChipBase::CHIPTYPE_OPL2)
    {
        for(size_t a = 0; a < data_opl2_size; a += 2)
            writeRegI(chip, data_opl2[a], (data_opl2[a + 1]));
    }
    else
    {
        for(size_t a = 0; a < data_opl3_size; a += 2)
            writeRegI(chip, data_opl3[a], (data_opl3[a + 1]));
    }
}

#ifdef ADLMIDI_ENABLE_HW_SERIAL
void OPL3::resetSerial(const std::string &serialName, unsigned int baud, unsigned int protocol)
{
    m_serial = true;
    m_serialName = serialName;
    m_serialBaud = baud;
    m_serialProtocol = protocol;
    m_numChips = 1; // Only one chip!
    m_softPanning = false; // Soft-panning doesn't work on hardware
    reset(-1, 0, NULL);
}
#endif
