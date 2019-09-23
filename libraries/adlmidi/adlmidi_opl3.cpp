/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "adlmidi_private.hpp"
#include <stdlib.h>
#include <cassert>

#ifdef ADLMIDI_HW_OPL
static const unsigned OPLBase = 0x388;
#else
#   if defined(ADLMIDI_DISABLE_NUKED_EMULATOR) && defined(ADLMIDI_DISABLE_DOSBOX_EMULATOR)
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
#endif

static const unsigned adl_emulatorSupport = 0
#ifndef ADLMIDI_HW_OPL
#   ifndef ADLMIDI_DISABLE_NUKED_EMULATOR
    | (1u << ADLMIDI_EMU_NUKED) | (1u << ADLMIDI_EMU_NUKED_174)
#   endif

#   ifndef ADLMIDI_DISABLE_DOSBOX_EMULATOR
    | (1u << ADLMIDI_EMU_DOSBOX)
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
static const uint16_t g_operatorsMap[23 * 2] =
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
    // Channel 18
    0x010, 0x013,  // operators 12,15
    // Channel 19
    0x014, 0xFFF,  // operator 16
    // Channel 19
    0x012, 0xFFF,  // operator 14
    // Channel 19
    0x015, 0xFFF,  // operator 17
    // Channel 19
    0x011, 0xFFF
}; // operator 13

//! Channel map to regoster offsets
static const uint16_t g_channelsMap[23] =
{
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, // 0..8
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, // 9..17 (secondary set)
    0x006, 0x007, 0x008, 0xFFF, 0xFFF
}; // <- hw percussions, 0xFFF = no support for pitch/pan

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

static adlinsdata2 makeEmptyInstrument()
{
    adlinsdata2 ins;
    memset(&ins, 0, sizeof(adlinsdata2));
    ins.flags = adlinsdata::Flag_NoSound;
    return ins;
}

const adlinsdata2 OPL3::m_emptyInstrument = makeEmptyInstrument();

OPL3::OPL3() :
    m_numChips(1),
    m_numFourOps(0),
    m_deepTremoloMode(false),
    m_deepVibratoMode(false),
    m_rhythmMode(false),
    m_softPanning(false),
    m_musicMode(MODE_MIDI),
    m_volumeScale(VOLUME_Generic)
{
    m_insBankSetup.volumeModel = OPL3::VOLUME_Generic;
    m_insBankSetup.deepTremolo = false;
    m_insBankSetup.deepVibrato = false;
    m_insBankSetup.adLibPercussions = false;
    m_insBankSetup.scaleModulators = false;

#ifdef DISABLE_EMBEDDED_BANKS
    m_embeddedBank = CustomBankTag;
#else
    setEmbeddedBank(0);
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

    if(bank >= static_cast<unsigned int>(maxAdlBanks()))
        return;

    Bank *bank_pair[2] =
    {
        &m_insBanks[0],
        &m_insBanks[PercussionTag]
    };

    for(unsigned i = 0; i < 256; ++i)
    {
        size_t meta = banks[bank][i];
        adlinsdata2 &ins = bank_pair[i / 128]->ins[i % 128];
        ins = adlinsdata2::from_adldata(::adlins[meta]);
    }
#else
    ADL_UNUSED(bank);
#endif
}

void OPL3::writeReg(size_t chip, uint16_t address, uint8_t value)
{
#ifdef ADLMIDI_HW_OPL
    ADL_UNUSED(chip);
    unsigned o = address >> 8;
    unsigned port = OPLBase + o * 2;

    #ifdef __DJGPP__
    outportb(port, address);
    for(unsigned c = 0; c < 6; ++c) inportb(port);
    outportb(port + 1, value);
    for(unsigned c = 0; c < 35; ++c) inportb(port);
    #endif

    #ifdef __WATCOMC__
    outp(port, address);
    for(uint16_t c = 0; c < 6; ++c)  inp(port);
    outp(port + 1, value);
    for(uint16_t c = 0; c < 35; ++c) inp(port);
    #endif//__WATCOMC__

#else//ADLMIDI_HW_OPL
    m_chips[chip]->writeReg(address, value);
#endif
}

void OPL3::writeRegI(size_t chip, uint32_t address, uint32_t value)
{
#ifdef ADLMIDI_HW_OPL
    writeReg(chip, static_cast<uint16_t>(address), static_cast<uint8_t>(value));
#else//ADLMIDI_HW_OPL
    m_chips[chip]->writeReg(static_cast<uint16_t>(address), static_cast<uint8_t>(value));
#endif
}

void OPL3::writePan(size_t chip, uint32_t address, uint32_t value)
{
#ifndef ADLMIDI_HW_OPL
    m_chips[chip]->writePan(static_cast<uint16_t>(address), static_cast<uint8_t>(value));
#else
    ADL_UNUSED(chip);
    ADL_UNUSED(address);
    ADL_UNUSED(value);
#endif
}


void OPL3::noteOff(size_t c)
{
    size_t chip = c / 23, cc = c % 23;

    if(cc >= 18)
    {
        m_regBD[chip] &= ~(0x10 >> (cc - 18));
        writeRegI(chip, 0xBD, m_regBD[chip]);
        return;
    }

    writeRegI(chip, 0xB0 + g_channelsMap[cc], m_keyBlockFNumCache[c] & 0xDF);
}

void OPL3::noteOn(size_t c1, size_t c2, double hertz) // Hertz range: 0..131071
{
    size_t chip = c1 / 23, cc1 = c1 % 23, cc2 = c2 % 23;
    uint32_t octave = 0, ftone = 0, mul_offset = 0;

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

    ftone = octave + static_cast<uint32_t>(hertz + 0.5);
    uint32_t chn = g_channelsMap[cc1];
    const adldata &patch1 = m_insCache[c1];
    const adldata &patch2 = m_insCache[c2 < m_insCache.size() ? c2 : 0];

    if(cc1 < 18)
    {
        ftone += 0x2000u; /* Key-ON [KON] */

        const bool natural_4op = (m_channelCategory[c1] == ChanCat_4op_Master);
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
            if((op > 0) && (op_addr[op] == 0xFFF))
                break;
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

    if(cc1 >= 18)
    {
        m_regBD[chip ] |= (0x10 >> (cc1 - 18));
        writeRegI(chip , 0x0BD, m_regBD[chip ]);
        //x |= 0x800; // for test
    }
}

void OPL3::touchNote(size_t c, uint8_t volume, uint8_t brightness)
{
    if(volume > 63)
        volume = 63;

    size_t chip = c / 23, cc = c % 23;
    const adldata &adli = m_insCache[c];
    uint16_t o1 = g_operatorsMap[cc * 2 + 0];
    uint16_t o2 = g_operatorsMap[cc * 2 + 1];
    uint8_t  x = adli.modulator_40, y = adli.carrier_40;
    uint32_t mode = 1; // 2-op AM

    if(m_channelCategory[c] == ChanCat_Regular ||
       m_channelCategory[c] == ChanCat_Rhythm_Bass)
    {
        mode = adli.feedconn & 1; // 2-op FM or 2-op AM
    }
    else if(m_channelCategory[c] == ChanCat_4op_Master ||
            m_channelCategory[c] == ChanCat_4op_Slave)
    {
        const adldata *i0, *i1;

        if(m_channelCategory[c] == ChanCat_4op_Master)
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

    if(m_musicMode == MODE_RSXX)
    {
        writeRegI(chip, 0x40 + o1, x);
        if(o2 != 0xFFF)
            writeRegI(chip, 0x40 + o2, y - volume / 2);
    }
    else
    {
        bool do_modulator = do_ops[ mode ][ 0 ] || m_scaleModulators;
        bool do_carrier   = do_ops[ mode ][ 1 ] || m_scaleModulators;

        uint32_t modulator = do_modulator ? (x | 63) - volume + volume * (x & 63) / 63 : x;
        uint32_t carrier   = do_carrier   ? (y | 63) - volume + volume * (y & 63) / 63 : y;

        if(brightness != 127)
        {
            brightness = static_cast<uint8_t>(::round(127.0 * ::sqrt((static_cast<double>(brightness)) * (1.0 / 127.0))) / 2.0);
            if(!do_modulator)
                modulator = (modulator | 63) - brightness + brightness * (modulator & 63) / 63;
            if(!do_carrier)
                carrier = (carrier | 63) - brightness + brightness * (carrier & 63) / 63;
        }

        writeRegI(chip, 0x40 + o1, modulator);
        if(o2 != 0xFFF)
            writeRegI(chip, 0x40 + o2, carrier);
    }

    // Correct formula (ST3, AdPlug):
    //   63-((63-(instrvol))/63)*chanvol
    // Reduces to (tested identical):
    //   63 - chanvol + chanvol*instrvol/63
    // Also (slower, floats):
    //   63 + chanvol * (instrvol / 63.0 - 1)
}

/*
void OPL3::Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
{
    if(LogarithmicVolumes)
        Touch_Real(c, volume * 127 / (127 * 127 * 127) / 2);
    else
    {
        // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
        Touch_Real(c, volume > 8725 ? static_cast<unsigned int>(std::log(volume) * 11.541561 + (0.5 - 104.22845)) : 0);
        // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
        //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
    }
}*/

void OPL3::setPatch(size_t c, const adldata &instrument)
{
    size_t chip = c / 23, cc = c % 23;
    static const uint8_t data[4] = {0x20, 0x60, 0x80, 0xE0};
    m_insCache[c] = instrument;
    uint16_t o1 = g_operatorsMap[cc * 2 + 0];
    uint16_t o2 = g_operatorsMap[cc * 2 + 1];
    unsigned x = instrument.modulator_E862, y = instrument.carrier_E862;

    for(size_t a = 0; a < 4; ++a, x >>= 8, y >>= 8)
    {
        writeRegI(chip, data[a] + o1, x & 0xFF);
        if(o2 != 0xFFF)
            writeRegI(chip, data[a] + o2, y & 0xFF);
    }
}

void OPL3::setPan(size_t c, uint8_t value)
{
    size_t chip = c / 23, cc = c % 23;
    if(g_channelsMap[cc] != 0xFFF)
    {
#ifndef ADLMIDI_HW_OPL
        if (m_softPanning)
        {
            writePan(chip, g_channelsMap[cc], value);
            writeRegI(chip, 0xC0 + g_channelsMap[cc], m_insCache[c].feedconn | OPL_PANNING_BOTH);
        }
        else
        {
#endif
            int panning = 0;
            if(value  < 64 + 32) panning |= OPL_PANNING_LEFT;
            if(value >= 64 - 32) panning |= OPL_PANNING_RIGHT;
            writePan(chip, g_channelsMap[cc], 64);
            writeRegI(chip, 0xC0 + g_channelsMap[cc], m_insCache[c].feedconn | panning);
#ifndef ADLMIDI_HW_OPL
        }
#endif
    }
}

void OPL3::silenceAll() // Silence all OPL channels.
{
    for(size_t c = 0; c < m_numChannels; ++c)
    {
        noteOff(c);
        touchNote(c, 0);
    }
}

void OPL3::updateChannelCategories()
{
    const uint32_t fours = m_numFourOps;

    for(uint32_t chip = 0, fours_left = fours; chip < m_numChips; ++chip)
    {
        m_regBD[chip] = (m_deepTremoloMode * 0x80 + m_deepVibratoMode * 0x40 + m_rhythmMode * 0x20);
        writeRegI(chip, 0x0BD, m_regBD[chip]);
        uint32_t fours_this_chip = std::min(fours_left, static_cast<uint32_t>(6u));
        writeRegI(chip, 0x104, (1 << fours_this_chip) - 1);
        fours_left -= fours_this_chip;
    }

    if(!m_rhythmMode)
    {
        for(size_t a = 0, n = m_numChips; a < n; ++a)
        {
            for(size_t b = 0; b < 23; ++b)
            {
                m_channelCategory[a * 23 + b] =
                    (b >= 18) ? ChanCat_Rhythm_Slave : ChanCat_Regular;
            }
        }
    }
    else
    {
        for(size_t a = 0, n = m_numChips; a < n; ++a)
        {
            for(size_t b = 0; b < 23; ++b)
            {
                m_channelCategory[a * 23 + b] =
                    (b >= 18) ? static_cast<ChanCat>(ChanCat_Rhythm_Bass + (b - 18)) :
                    (b >= 6 && b < 9) ? ChanCat_Rhythm_Slave : ChanCat_Regular;
            }
        }
    }

    uint32_t nextfour = 0;
    for(uint32_t a = 0; a < fours; ++a)
    {
        m_channelCategory[nextfour] = ChanCat_4op_Master;
        m_channelCategory[nextfour + 3] = ChanCat_4op_Slave;

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
            nextfour += 23 - 9 - 2;
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
      Channel 3:  CHANNEL 0 SLAVE
      Channel 4:  CHANNEL 1 SLAVE
      Channel 5:  CHANNEL 2 SLAVE
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
    }
}

#ifndef ADLMIDI_HW_OPL
void OPL3::clearChips()
{
    for(size_t i = 0; i < m_chips.size(); i++)
        m_chips[i].reset(NULL);
    m_chips.clear();
}
#endif

void OPL3::reset(int emulator, unsigned long PCM_RATE, void *audioTickHandler)
{
#ifndef ADLMIDI_HW_OPL
    clearChips();
#else
    (void)emulator;
    (void)PCM_RATE;
#endif
#if !defined(ADLMIDI_AUDIO_TICK_HANDLER)
    (void)audioTickHandler;
#endif
    m_insCache.clear();
    m_keyBlockFNumCache.clear();
    m_regBD.clear();

#ifndef ADLMIDI_HW_OPL
    m_chips.resize(m_numChips, AdlMIDI_SPtr<OPLChipBase>());
#endif

    const struct adldata defaultInsCache = { 0x1557403,0x005B381, 0x49,0x80, 0x4, +0 };
    m_numChannels = m_numChips * 23;
    m_insCache.resize(m_numChannels, defaultInsCache);
    m_keyBlockFNumCache.resize(m_numChannels,   0);
    m_regBD.resize(m_numChips,    0);
    m_channelCategory.resize(m_numChannels, 0);

    for(size_t p = 0, a = 0; a < m_numChips; ++a)
    {
        for(size_t b = 0; b < 18; ++b)
            m_channelCategory[p++] = 0;
        for(size_t b = 0; b < 5; ++b)
            m_channelCategory[p++] = ChanCat_Rhythm_Slave;
    }

    static const uint16_t data[] =
    {
        0x004, 96, 0x004, 128,        // Pulse timer
        0x105, 0, 0x105, 1, 0x105, 0, // Pulse OPL3 enable
        0x001, 32, 0x105, 1           // Enable wave, OPL3 extensions
    };
//    size_t fours = m_numFourOps;

    for(size_t i = 0; i < m_numChips; ++i)
    {
#ifndef ADLMIDI_HW_OPL
        OPLChipBase *chip;
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
        }
        m_chips[i].reset(chip);
        chip->setChipId((uint32_t)i);
        chip->setRate((uint32_t)PCM_RATE);
        if(m_runAtPcmRate)
            chip->setRunningAtPcmRate(true);
#   if defined(ADLMIDI_AUDIO_TICK_HANDLER)
        chip->setAudioTickHandlerInstance(audioTickHandler);
#   endif
#endif // ADLMIDI_HW_OPL

        /* Clean-up channels from any playing junk sounds */
        for(size_t a = 0; a < 18; ++a)
            writeRegI(i, 0xB0 + g_channelsMap[a], 0x00);
        for(size_t a = 0; a < sizeof(data) / sizeof(*data); a += 2)
            writeRegI(i, data[a], (data[a + 1]));
    }

    updateChannelCategories();
    silenceAll();
}
