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

#ifndef OPLINST_H
#define OPLINST_H

#include <string.h>
#include <stdint.h>
#include <cstring>

#pragma pack(push, 1)
#define OPLINST_BYTE_COMPARABLE(T)                      \
    inline bool operator==(const T &a, const T &b)      \
    { return !memcmp(&a, &b, sizeof(T)); }              \
    inline bool operator!=(const T &a, const T &b)      \
    { return !operator==(a, b); }

/**
 * @brief OPL3 Operator data for single tumbre
 */
struct OplTimbre
{
    //! WaveForm, Sustain/Release, AttackDecay, and AM/VIB/EG/KSR/F-Mult settings
    uint32_t    modulator_E862, carrier_E862;
    //! KSL/attenuation settings
    uint8_t     modulator_40, carrier_40;
    //! Feedback/connection bits for the channel
    uint8_t     feedconn;
    //! Semi-tone offset
    int8_t      noteOffset;
};
OPLINST_BYTE_COMPARABLE(struct OplTimbre)


enum { OPLNoteOnMaxTime = 40000 };


/**
 * @brief Instrument data with operators included
 */
struct OplInstMeta
{
    enum
    {
        Flag_Pseudo4op = 0x01,
        Flag_NoSound = 0x02,
        Flag_Real4op = 0x04
    };

    enum
    {
        Flag_RM_BassDrum  = 0x08,
        Flag_RM_Snare = 0x10,
        Flag_RM_TomTom = 0x18,
        Flag_RM_Cymbal = 0x20,
        Flag_RM_HiHat = 0x28,
        Mask_RhythmMode = 0x38
    };

    //! Operator data
    OplTimbre    op[2];
    //! Fixed note for drum instruments
    uint8_t         drumTone;
    //! Instrument flags
    uint8_t         flags;
    //! Number of milliseconds it produces sound while key on
    uint16_t        soundKeyOnMs;
    //! Number of milliseconds it produces sound while releasing after key off
    uint16_t        soundKeyOffMs;
    //! MIDI velocity offset
    int8_t          midiVelocityOffset;
    //! Second voice detune
    double          voice2_fine_tune;
};
OPLINST_BYTE_COMPARABLE(struct OplInstMeta)

#undef OPLINST_BYTE_COMPARABLE
#pragma pack(pop)

/**
 * @brief Bank global setup
 */
struct OplBankSetup
{
    int     volumeModel;
    bool    deepTremolo;
    bool    deepVibrato;
    bool    scaleModulators;
    bool    mt32defaults;
};

/**
 * @brief Convert external instrument to internal instrument
 */
void cvt_ADLI_to_FMIns(OplInstMeta &dst, const struct ADL_Instrument &src);

/**
 * @brief Convert internal instrument to external instrument
 */
void cvt_FMIns_to_ADLI(struct ADL_Instrument &dst, const OplInstMeta &src);

#endif // OPLINST_H
