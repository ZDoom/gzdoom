/*
 * libOPNMIDI is a free Software MIDI synthesizer library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * OPNMIDI Library and YM2612 support:   Copyright (c) 2017-2025 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "opnmidi_opn2.hpp"
#include "opnmidi_private.hpp"

#if defined(OPNMIDI_DISABLE_NUKED_EMULATOR) && defined(OPNMIDI_DISABLE_MAME_EMULATOR) && \
    defined(OPNMIDI_DISABLE_GENS_EMULATOR) && defined(OPNMIDI_DISABLE_GX_EMULATOR) && \
    defined(OPNMIDI_DISABLE_NP2_EMULATOR) && defined(OPNMIDI_DISABLE_MAME_2608_EMULATOR) && \
    defined(OPNMIDI_DISABLE_PMDWIN_EMULATOR)
#error "No emulators enabled. You must enable at least one emulator to use this library!"
#endif

// Nuked OPN2 emulator, Most accurate, but requires the powerful CPU
#ifndef OPNMIDI_DISABLE_NUKED_EMULATOR
#include "chips/nuked_opn2.h"
#endif

// MAME YM2612 emulator, Well-accurate and fast
#ifndef OPNMIDI_DISABLE_MAME_EMULATOR
#include "chips/mame_opn2.h"
#endif

// GENS 2.10 emulator, very outdated and inaccurate, but gives the best performance
#ifndef OPNMIDI_DISABLE_GENS_EMULATOR
#include "chips/gens_opn2.h"
#endif

//// Genesis Plus GX emulator, Variant of MAME with enhancements
//#ifndef OPNMIDI_DISABLE_GX_EMULATOR
//#include "chips/gx_opn2.h"
//#endif

// Neko Project II OPNA emulator
#ifndef OPNMIDI_DISABLE_NP2_EMULATOR
#include "chips/np2_opna.h"
#endif

// MAME YM2608 emulator
#ifndef OPNMIDI_DISABLE_MAME_2608_EMULATOR
#include "chips/mame_opna.h"
#endif

//// PMDWin OPNA emulator
//#ifndef OPNMIDI_DISABLE_PMDWIN_EMULATOR
//#include "chips/pmdwin_opna.h"
//#endif

// YMFM emulators
#ifndef OPNMIDI_DISABLE_YMFM_EMULATOR
#include "chips/ymfm_opn2.h"
#include "chips/ymfm_opna.h"
#endif


// VGM File dumper
#ifdef OPNMIDI_MIDI2VGM
#include "chips/vgm_file_dumper.h"
#endif

static const unsigned opn2_emulatorSupport = 0
#ifndef OPNMIDI_DISABLE_NUKED_EMULATOR
    | (1u << OPNMIDI_EMU_NUKED_YM2612)
    | (1u << OPNMIDI_EMU_NUKED_YM3438)
#endif
#ifndef OPNMIDI_DISABLE_MAME_EMULATOR
    | (1u << OPNMIDI_EMU_MAME)
#endif
#ifndef OPNMIDI_DISABLE_GENS_EMULATOR
    | (1u << OPNMIDI_EMU_GENS)
#endif
#ifndef OPNMIDI_DISABLE_YMFM_EMULATOR
    | (1u << OPNMIDI_EMU_YMFM_OPN2)
    | (1u << OPNMIDI_EMU_YMFM_OPNA)
#endif
//#ifndef OPNMIDI_DISABLE_GX_EMULATOR
//    | (1u << OPNMIDI_EMU_GX)
//#endif
#ifndef OPNMIDI_DISABLE_NP2_EMULATOR
    | (1u << OPNMIDI_EMU_NP2)
#endif
#ifndef OPNMIDI_DISABLE_MAME_2608_EMULATOR
    | (1u << OPNMIDI_EMU_MAME_2608)
#endif
//#ifndef OPNMIDI_DISABLE_PMDWIN_EMULATOR
//    | (1u << OPNMIDI_EMU_PMDWIN)
//#endif
#ifdef OPNMIDI_MIDI2VGM
    | (1u << OPNMIDI_VGM_DUMPER)
#endif
;

//! Check emulator availability
bool opn2_isEmulatorAvailable(int emulator)
{
    return (opn2_emulatorSupport & (1u << (unsigned)emulator)) != 0;
}

//! Find highest emulator
int opn2_getHighestEmulator()
{
    int emu = -1;
    for(unsigned m = opn2_emulatorSupport; m > 0; m >>= 1)
        ++emu;
    return emu;
}

//! Find lowest emulator
int opn2_getLowestEmulator()
{
    int emu = -1;
    unsigned m = opn2_emulatorSupport;
    if(m > 0)
    {
        for(emu = 0; (m & 1) == 0; m >>= 1)
            ++emu;
    }
    return emu;
}



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

static const uint_fast32_t W9X_volume_mapping_table[32] =
{
    63, 63, 40, 36, 32, 28, 23, 21,
    19, 17, 15, 14, 13, 12, 11, 10,
    9,  8,  7,  6,  5,  5,  4,  4,
    3,  3,  2,  2,  1,  1,  0,  0
};

static const uint32_t g_noteChannelsMap[6] = { 0, 1, 2, 4, 5, 6 };

static inline void getOpnChannel(size_t     in_channel,
                                 size_t     &out_chip,
                                 uint8_t    &out_port,
                                 uint32_t   &out_ch)
{
    out_chip    = in_channel / 6;
    size_t ch4 = in_channel % 6;
    out_port = ((ch4 < 3) ? 0 : 1);
    out_ch = static_cast<uint32_t>(ch4 % 3);
}


/***************************************************************
 *               Standard frequency formula                    *
 * *************************************************************/

static inline double s_commonFreq(double tone)
{
    return std::exp(0.057762265 * tone);
}



enum
{
    MasterVolumeDefault = 127
};

enum
{
    OPN_PANNING_LEFT  = 0x80,
    OPN_PANNING_RIGHT = 0x40,
    OPN_PANNING_BOTH  = 0xC0
};

static OpnInstMeta makeEmptyInstrument()
{
    OpnInstMeta ins;
    memset(&ins, 0, sizeof(OpnInstMeta));
    ins.flags = OpnInstMeta::Flag_NoSound;
    return ins;
}

const OpnInstMeta OPN2::m_emptyInstrument = makeEmptyInstrument();

OPN2::OPN2() :
    m_regLFOSetup(0),
    m_numChips(1),
    m_scaleModulators(false),
    m_runAtPcmRate(false),
    m_softPanning(false),
    m_masterVolume(MasterVolumeDefault),
    m_musicMode(MODE_MIDI),
    m_volumeScale(VOLUME_Generic),
    m_channelAlloc(OPNMIDI_ChanAlloc_AUTO),
    m_lfoEnable(false),
    m_lfoFrequency(0),
    m_chipFamily(OPNChip_OPN2)
{
    m_insBankSetup.volumeModel = OPN2::VOLUME_Generic;
    m_insBankSetup.lfoEnable = false;
    m_insBankSetup.lfoFrequency = 0;
    m_insBankSetup.chipType = OPNChip_OPN2;
    m_insBankSetup.mt32defaults = false;

    // Initialize blank instruments banks
    m_insBanks.clear();
}

OPN2::~OPN2()
{
    clearChips();
}

bool OPN2::setupLocked()
{
    return (m_musicMode == MODE_CMF ||
            m_musicMode == MODE_IMF ||
            m_musicMode == MODE_RSXX);
}

void OPN2::writeReg(size_t chip, uint8_t port, uint8_t index, uint8_t value)
{
    m_chips[chip]->writeReg(port, index, value);
}

void OPN2::writeRegI(size_t chip, uint8_t port, uint32_t index, uint32_t value)
{
    m_chips[chip]->writeReg(port, static_cast<uint8_t>(index), static_cast<uint8_t>(value));
}

void OPN2::writePan(size_t chip, uint32_t index, uint32_t value)
{
    m_chips[chip]->writePan(static_cast<uint16_t>(index), static_cast<uint8_t>(value));
}

void OPN2::noteOff(size_t c)
{
    size_t      chip;
    uint8_t     port;
    uint32_t    cc;
    size_t      ch4 = c % 6;
    getOpnChannel(c, chip, port, cc);
    writeRegI(chip, 0, 0x28, g_noteChannelsMap[ch4]);
}

void OPN2::noteOn(size_t c, double tone)
{
    // Hertz range: 0..131071
    double hertz = s_commonFreq(tone);

    if(hertz < 0) // Avoid infinite loop
        return;

    double coef;
    switch(m_chipFamily)
    {
    case OPNChip_OPN2: default:
        coef = 321.88557; break;
    case OPNChip_OPNA:
        coef = 309.12412; break;
    }
    hertz *= coef;

    size_t      chip;
    uint8_t     port;
    uint32_t    cc;
    size_t      ch4 = c % 6;
    getOpnChannel(c, chip, port, cc);

    uint32_t octave = 0, ftone = 0, mul_offset = 0;
    const OpnTimbre &adli = m_insCache[c];

    //Basic range until max of octaves reaching
    while((hertz >= 1023.75) && (octave < 0x3800))
    {
        hertz /= 2.0;    // Calculate octave
        octave += 0x800;
    }
    //Extended range, rely on frequency multiplication increment
    while(hertz >= 2036.75)
    {
        hertz /= 2.0;    // Calculate octave
        mul_offset++;
    }
    ftone = octave + static_cast<uint32_t>(hertz + 0.5);

    for(size_t op = 0; op < 4; op++)
    {
        uint32_t reg = adli.OPS[op].data[0];
        uint16_t address = static_cast<uint16_t>(0x30 + (op * 4) + cc);
        if(mul_offset > 0) // Increase frequency multiplication value
        {
            uint32_t dt  = reg & 0xF0;
            uint32_t mul = reg & 0x0F;
            if((mul + mul_offset) > 0x0F)
            {
                mul_offset = 0;
                mul = 0x0F;
            }
            writeRegI(chip, port, address, uint8_t(dt | (mul + mul_offset)));
        }
        else
        {
            writeRegI(chip, port, address, uint8_t(reg));
        }
    }

    writeRegI(chip, port, 0xA4 + cc, (ftone>>8) & 0xFF);//Set frequency and octave
    writeRegI(chip, port, 0xA0 + cc, ftone & 0xFF);
    writeRegI(chip, 0, 0x28, 0xF0 + g_noteChannelsMap[ch4]);
}

void OPN2::touchNote(size_t c,
                     uint_fast32_t velocity,
                     uint_fast32_t channelVolume,
                     uint_fast32_t channelExpression,
                     uint8_t brightness)
{
    size_t      chip;
    uint8_t     port;
    uint32_t    cc;
    getOpnChannel(c, chip, port, cc);

    const OpnTimbre &adli = m_insCache[c];

    uint_fast32_t volume = 0;

    uint8_t op_vol[4] =
    {
        adli.OPS[OPERATOR1].data[1],
        adli.OPS[OPERATOR2].data[1],
        adli.OPS[OPERATOR3].data[1],
        adli.OPS[OPERATOR4].data[1],
    };

    bool alg_do[8][4] =
    {
        /*
         * Yeah, Operator 2 and 3 are seems swapped
         * which we can see in the algorithm 4
         */
        //OP1   OP3   OP2    OP4
        //30    34    38     3C
        {false,false,false,true},//Algorithm #0:  W = 1 * 2 * 3 * 4
        {false,false,false,true},//Algorithm #1:  W = (1 + 2) * 3 * 4
        {false,false,false,true},//Algorithm #2:  W = (1 + (2 * 3)) * 4
        {false,false,false,true},//Algorithm #3:  W = ((1 * 2) + 3) * 4
        {false,false,true, true},//Algorithm #4:  W = (1 * 2) + (3 * 4)
        {false,true ,true ,true},//Algorithm #5:  W = (1 * (2 + 3 + 4)
        {false,true ,true ,true},//Algorithm #6:  W = (1 * 2) + 3 + 4
        {true ,true ,true ,true},//Algorithm #7:  W = 1 + 2 + 3 + 4
    };

    switch(m_volumeScale)
    {
    default:
    case Synth::VOLUME_Generic:
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
            volume = static_cast<uint_fast32_t>(lv * c1 - c2) * 2;
        }
        else
            volume = 0;
    }
    break;

    case Synth::VOLUME_NATIVE:
    {
        volume = velocity * channelVolume * channelExpression;
        //volume = volume * m_masterVolume / (127 * 127 * 127) / 2;
        volume = (volume * m_masterVolume) / 4096766;

        if(volume > 0)
            volume += 64;//OPN has 0~127 range. As 0...63 is almost full silence, but at 64 to 127 is very closed to OPL3, just add 64.
    }
    break;

    case Synth::VOLUME_DMX:
    {
        volume = (channelVolume * channelExpression * m_masterVolume) / 16129;
        volume = (s_dmx_volume_model[volume] + 1) << 1;
        volume = (s_dmx_volume_model[(velocity < 128) ? velocity : 127] * volume) >> 9;

        if(volume > 0)
            volume += 64;//OPN has 0~127 range. As 0...63 is almost full silence, but at 64 to 127 is very closed to OPL3, just add 64.
    }
    break;

    case Synth::VOLUME_APOGEE:
    {
        volume = (channelVolume * channelExpression * m_masterVolume / 16129);
        volume = ((64 * (velocity + 0x80)) * volume) >> 15;
        //volume = ((63 * (vol + 0x80)) * Ch[MidCh].volume) >> 15;
        if(volume > 0)
            volume += 64;//OPN has 0~127 range. As 0...63 is almost full silence, but at 64 to 127 is very closed to OPL3, just add 64.
    }
    break;

    case Synth::VOLUME_9X:
    {
        //volume = 63 - W9X_volume_mapping_table[(((vol * Ch[MidCh].volume /** Ch[MidCh].expression*/) * 127 / 16129 /*2048383*/) >> 2)];
        volume = 63 - W9X_volume_mapping_table[((velocity * channelVolume * channelExpression * m_masterVolume / 2048383) >> 2)];
        //volume = W9X_volume_mapping_table[vol >> 2] + volume;
        if(volume > 0)
            volume += 64;//OPN has 0~127 range. As 0...63 is almost full silence, but at 64 to 127 is very closed to OPL3, just add 64.
    }
    break;
    }


    if(volume > 127)
        volume = 127;

    uint8_t alg = adli.fbalg & 0x07;
    for(uint8_t op = 0; op < 4; op++)
    {
        bool do_op = alg_do[alg][op] || m_scaleModulators;
        uint32_t x = op_vol[op];
        uint32_t vol_res = do_op ? (127 - (static_cast<uint32_t>(volume) * (127 - (x & 127))) / 127) : x;
        if(brightness != 127)
        {
            brightness = static_cast<uint32_t>(::round(127.0 * ::sqrt((static_cast<double>(brightness)) * (1.0 / 127.0))));
            if(!do_op)
                vol_res = (127 - (brightness * (127 - (static_cast<uint32_t>(vol_res) & 127))) / 127);
        }
        writeRegI(chip, port, 0x40 + cc + (4 * op), vol_res);
    }
    // Correct formula (ST3, AdPlug):
    //   63-((63-(instrvol))/63)*chanvol
    // Reduces to (tested identical):
    //   63 - chanvol + chanvol*instrvol/63
    // Also (slower, floats):
    //   63 + chanvol * (instrvol / 63.0 - 1)
}

void OPN2::setPatch(size_t c, const OpnTimbre &instrument)
{
    size_t      chip;
    uint8_t     port;
    uint32_t    cc;
    getOpnChannel(c, chip, port, cc);
    m_insCache[c] = instrument;
    for(uint8_t d = 0; d < 7; d++)
    {
        for(uint8_t op = 0; op < 4; op++)
            writeRegI(chip, port, 0x30 + (0x10 * d) + (op * 4) + cc, instrument.OPS[op].data[d]);
    }

    writeRegI(chip, port, 0xB0 + cc, instrument.fbalg);//Feedback/Algorithm
    m_regLFOSens[c] = (m_regLFOSens[c] & 0xC0) | (instrument.lfosens & 0x3F);
    writeRegI(chip, port, 0xB4 + cc, m_regLFOSens[c]);//Panorame and LFO bits
}

void OPN2::setPan(size_t c, uint8_t value)
{
    size_t      chip;
    uint8_t     port;
    uint32_t    cc;
    getOpnChannel(c, chip, port, cc);
    const OpnTimbre &adli = m_insCache[c];
    uint8_t val = 0;
    if(m_softPanning)
    {
        val = (OPN_PANNING_BOTH & 0xC0) | (adli.lfosens & 0x3F);
        writePan(chip, c % 6, value);
        writeRegI(chip, port, 0xB4 + cc, val);
    }
    else
    {
        int panning = 0;
        if(value  < 64 + 16) panning |= OPN_PANNING_LEFT;
        if(value >= 64 - 16) panning |= OPN_PANNING_RIGHT;
        val = (panning & 0xC0) | (adli.lfosens & 0x3F);
        writePan(chip, c % 6, 64);
        writeRegI(chip, port, 0xB4 + cc, val);
    }
    m_regLFOSens[c] = val;
}

void OPN2::silenceAll() // Silence all OPL channels.
{
    for(size_t c = 0; c < m_numChannels; ++c)
    {
        noteOff(c);
        touchNote(c, 0);
    }
}

void OPN2::commitLFOSetup()
{
    uint8_t regLFOSetup = (m_lfoEnable ? 8 : 0) | (m_lfoFrequency & 7);
    m_regLFOSetup = regLFOSetup;
    for(size_t chip = 0; chip < m_numChips; ++chip)
        writeReg(chip, 0, 0x22, regLFOSetup);
}

void OPN2::setVolumeScaleModel(OPNMIDI_VolumeModels volumeModel)
{
    switch(volumeModel)
    {
    default:
    case OPNMIDI_VolumeModel_AUTO://Do nothing until restart playing
        break;

    case OPNMIDI_VolumeModel_Generic:
        m_volumeScale = OPN2::VOLUME_Generic;
        break;

    case OPNMIDI_VolumeModel_NativeOPN2:
        m_volumeScale = OPN2::VOLUME_NATIVE;
        break;

    case OPNMIDI_VolumeModel_DMX:
        m_volumeScale = OPN2::VOLUME_DMX;
        break;

    case OPNMIDI_VolumeModel_APOGEE:
        m_volumeScale = OPN2::VOLUME_APOGEE;
        break;

    case OPNMIDI_VolumeModel_9X:
        m_volumeScale = OPN2::VOLUME_9X;
        break;
    }
}

OPNMIDI_VolumeModels OPN2::getVolumeScaleModel()
{
    switch(m_volumeScale)
    {
    default:
    case OPN2::VOLUME_Generic:
        return OPNMIDI_VolumeModel_Generic;
    case OPN2::VOLUME_NATIVE:
        return OPNMIDI_VolumeModel_NativeOPN2;
    case OPN2::VOLUME_DMX:
        return OPNMIDI_VolumeModel_DMX;
    case OPN2::VOLUME_APOGEE:
        return OPNMIDI_VolumeModel_APOGEE;
    case OPN2::VOLUME_9X:
        return OPNMIDI_VolumeModel_9X;
    }
}

void OPN2::clearChips()
{
    for(size_t i = 0; i < m_chips.size(); i++)
        m_chips[i].reset(NULL);
    m_chips.clear();
}

void OPN2::reset(int emulator, unsigned long PCM_RATE, OPNFamily family, void *audioTickHandler)
{
#if !defined(ADLMIDI_AUDIO_TICK_HANDLER)
    ADL_UNUSED(audioTickHandler);
#endif
    clearChips();
    m_insCache.clear();
    m_regLFOSens.clear();
#ifdef OPNMIDI_MIDI2VGM
    if(emulator == OPNMIDI_VGM_DUMPER && (m_numChips > 2))
        m_numChips = 2;// VGM Dumper can't work in multichip mode
#endif
    m_chips.clear();
    m_chips.resize(m_numChips, AdlMIDI_SPtr<OPNChipBase>());

#ifdef OPNMIDI_MIDI2VGM
    m_loopStartHook = NULL;
    m_loopStartHookData = NULL;
    m_loopEndHook = NULL;
    m_loopEndHookData = NULL;
#endif

    for(size_t i = 0; i < m_chips.size(); i++)
    {
        OPNChipBase *chip = NULL;

        switch(emulator)
        {
        default:
            assert(false);
            abort();
#ifndef OPNMIDI_DISABLE_MAME_EMULATOR
        case OPNMIDI_EMU_MAME:
            chip = new MameOPN2(family);
            break;
#endif
#ifndef OPNMIDI_DISABLE_NUKED_EMULATOR
        case OPNMIDI_EMU_NUKED_YM3438:
            chip = new NukedOPN2(family, true);
            break;
        case OPNMIDI_EMU_NUKED_YM2612:
            chip = new NukedOPN2(family, false);
            break;
#endif
#ifndef OPNMIDI_DISABLE_GENS_EMULATOR
        case OPNMIDI_EMU_GENS:
            chip = new GensOPN2(family);
            break;
#endif
#ifndef OPNMIDI_DISABLE_YMFM_EMULATOR
        case OPNMIDI_EMU_YMFM_OPN2:
            chip = new YmFmOPN2(family);
            break;
#endif
//#ifndef OPNMIDI_DISABLE_GX_EMULATOR
//        case OPNMIDI_EMU_GX:
//            chip = new GXOPN2(family);
//            break;
//#endif
#ifndef OPNMIDI_DISABLE_NP2_EMULATOR
        case OPNMIDI_EMU_NP2:
            chip = new NP2OPNA<>(family);
            break;
#endif
#ifndef OPNMIDI_DISABLE_MAME_2608_EMULATOR
        case OPNMIDI_EMU_MAME_2608:
            chip = new MameOPNA(family);
            break;
#endif
#ifndef OPNMIDI_DISABLE_YMFM_EMULATOR
        case OPNMIDI_EMU_YMFM_OPNA:
            chip = new YmFmOPNA(family);
            break;
#endif
//#ifndef OPNMIDI_DISABLE_PMDWIN_EMULATOR
//        case OPNMIDI_EMU_PMDWIN:
//            chip = new PMDWinOPNA(family);
//            break;
//#endif
#ifdef OPNMIDI_MIDI2VGM
        case OPNMIDI_VGM_DUMPER:
            chip = new VGMFileDumper(family, i, (i == 0 ? NULL : m_chips[0].get()));
            if(i == 0)//Set hooks for first chip only
            {
                m_loopStartHook = &VGMFileDumper::loopStartHook;
                m_loopStartHookData = chip;
                m_loopEndHook  = &VGMFileDumper::loopEndHook;
                m_loopEndHookData = chip;
            }
            break;
#endif
        }

        m_chips[i].reset(chip);
        chip->setChipId(static_cast<uint32_t>(i));
        chip->setRate(static_cast<uint32_t>(PCM_RATE), chip->nativeClockRate());

        if(m_runAtPcmRate)
            chip->setRunningAtPcmRate(true);

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
        chip->setAudioTickHandlerInstance(audioTickHandler);
#endif
        family = chip->family();
    }

    m_chipFamily = family;
    m_numChannels = m_numChips * 6;
    m_insCache.resize(m_numChannels,   m_emptyInstrument.op[0]);
    m_regLFOSens.resize(m_numChannels,    0);

    uint8_t regLFOSetup = (m_lfoEnable ? 8 : 0) | (m_lfoFrequency & 7);
    m_regLFOSetup = regLFOSetup;

    for(size_t chip = 0; chip < m_numChips; ++chip)
    {
        writeReg(chip, 0, 0x22, regLFOSetup);//push current LFO state
        writeReg(chip, 0, 0x27, 0x00);  //set Channel 3 normal mode
        writeReg(chip, 0, 0x2B, 0x00);  //Disable DAC
        //Shut up all channels
        writeReg(chip, 0, 0x28, 0x00); //Note Off 0 channel
        writeReg(chip, 0, 0x28, 0x01); //Note Off 1 channel
        writeReg(chip, 0, 0x28, 0x02); //Note Off 2 channel
        writeReg(chip, 0, 0x28, 0x04); //Note Off 3 channel
        writeReg(chip, 0, 0x28, 0x05); //Note Off 4 channel
        writeReg(chip, 0, 0x28, 0x06); //Note Off 5 channel
    }

    silenceAll();
#ifdef OPNMIDI_MIDI2VGM
    if(m_loopStartHook) // Post-initialization Loop Start hook (fix for loop edge passing clicks)
        m_loopStartHook(m_loopStartHookData);
#endif
}

OPNFamily OPN2::chipFamily() const
{
    return m_chipFamily;
}
