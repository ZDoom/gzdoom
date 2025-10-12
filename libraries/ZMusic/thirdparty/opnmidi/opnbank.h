/*
 * libOPNMIDI is a free Software MIDI synthesizer library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2016-2025 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef OPNMIDI_OPNBANK_H
#define OPNMIDI_OPNBANK_H

#include <string.h>
#include <stdint.h>

#ifdef ADLMIDI_buildAsApp
#include <SDL2/SDL.h>
class MutexType
{
    SDL_mutex* mut;
public:
    MutexType() : mut(SDL_CreateMutex()) { }
    ~MutexType() { SDL_DestroyMutex(mut); }
    void Lock() { SDL_mutexP(mut); }
    void Unlock() { SDL_mutexV(mut); }
};
#endif

enum { opnNoteOnMaxTime = 40000 };

/* *********** FM Operator indexes *********** */
enum
{
    OPERATOR1 = 0,
    OPERATOR2 = 1,
    OPERATOR3 = 2,
    OPERATOR4 = 3
};
/* *********** FM Operator indexes *end******* */

#pragma pack(push, 1)
#define OPNDATA_BYTE_COMPARABLE(T)                      \
    inline bool operator==(const T &a, const T &b)      \
    { return !memcmp(&a, &b, sizeof(T)); }              \
    inline bool operator!=(const T &a, const T &b)      \
    { return !operator==(a, b); }

struct OPN_Operator
{
    //! Raw register data
    uint8_t     data[7];
    /*
    Bytes:
        0 - Deture/Multiple
        1 - Total Level
        2 - Rate Scale / Attack
        3 - Amplitude modulation / Decay-1
        4 - Decay-2
        5 - Systain / Release
        6 - SSG-EG byte
    */
};
OPNDATA_BYTE_COMPARABLE(struct OPN_Operator)

struct OpnTimbre
{
    //! Operators prepared for sending to OPL chip emulator
    OPN_Operator    OPS[4];
    //! Feedback / algorithm
    uint8_t         fbalg;
    //! LFO sensitivity
    uint8_t         lfosens;
    //! Semi-tone offset
    int16_t         noteOffset;
};
OPNDATA_BYTE_COMPARABLE(struct OpnTimbre)

/**
 * @brief Instrument data with operators included
 */
struct OpnInstMeta
{
    enum
    {
        Flag_Pseudo8op = 0x01,
        Flag_NoSound = 0x02
    };

    //! Operator data
    OpnTimbre op[2];
    //! Fixed note for drum instruments
    uint8_t  drumTone;
    //! Instrument flags
    uint8_t  flags;
    //! Number of milliseconds it produces sound while key on
    uint16_t soundKeyOnMs;
    //! Number of milliseconds it produces sound while releasing after key off
    uint16_t soundKeyOffMs;
    //! MIDI velocity offset
    int8_t   midiVelocityOffset;
    //! Second voice detune
    double   voice2_fine_tune;
};
OPNDATA_BYTE_COMPARABLE(struct OpnInstMeta)

#undef OPNDATA_BYTE_COMPARABLE
#pragma pack(pop)

/**
 * @brief Bank global setup
 */
struct OpnBankSetup
{
    int volumeModel;
    int lfoEnable;
    int lfoFrequency;
    int chipType;
    bool mt32defaults;
};

/**
 * @brief Convert external instrument to internal instrument
 */
void cvt_OPNI_to_FMIns(OpnInstMeta &dst, const struct OPN2_Instrument &src);

/**
 * @brief Convert internal instrument to external instrument
 */
void cvt_FMIns_to_OPNI(struct OPN2_Instrument &dst, const OpnInstMeta &src);

#endif  // OPNMIDI_OPNBANK_H
