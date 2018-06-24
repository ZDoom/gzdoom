/*
 * libOPNMIDI is a free MIDI to WAV conversion library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2016 Vitaly Novichkov <admin@wohlnet.ru>
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
    OPERATOR4 = 3,
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

struct opnInstData
{
    //! Operators prepared for sending to OPL chip emulator
    OPN_Operator    OPS[4];
    uint8_t         fbalg;
    uint8_t         lfosens;
    //! Note offset
    int16_t         finetune;
};
OPNDATA_BYTE_COMPARABLE(struct opnInstData)

struct opnInstMeta
{
    enum { Flag_Pseudo8op = 0x01, Flag_NoSound = 0x02 };
    uint16_t opnno1, opnno2;
    uint8_t  tone;
    uint8_t  flags;
    uint16_t ms_sound_kon;  // Number of milliseconds it produces sound;
    uint16_t ms_sound_koff;
    double   fine_tune;
};
OPNDATA_BYTE_COMPARABLE(struct opnInstMeta)

/**
 * @brief Instrument data with operators included
 */
struct opnInstMeta2
{
    opnInstData opn[2];
    uint8_t  tone;
    uint8_t  flags;
    uint16_t ms_sound_kon;  // Number of milliseconds it produces sound;
    uint16_t ms_sound_koff;
    double   fine_tune;
#if 0
    opnInstMeta2() {}
    explicit opnInstMeta2(const opnInstMeta &d);
#endif
};
OPNDATA_BYTE_COMPARABLE(struct opnInstMeta2)

#undef OPNDATA_BYTE_COMPARABLE
#pragma pack(pop)

#if 0
/**
 * @brief Conversion of storage formats
 */
inline opnInstMeta2::opnInstMeta2(const opnInstMeta &d)
    : tone(d.tone), flags(d.flags),
      ms_sound_kon(d.ms_sound_kon), ms_sound_koff(d.ms_sound_koff),
      fine_tune(d.fine_tune)
{
    opn[0] = ::opn[d.opnno1];
    opn[1] = ::opn[d.opnno2];
}
#endif

#endif  // OPNMIDI_OPNBANK_H
