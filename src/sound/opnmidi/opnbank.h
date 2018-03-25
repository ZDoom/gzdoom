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

/* *********** FM Operator indexes *********** */
#define OPERATOR1    0
#define OPERATOR2    1
#define OPERATOR3    2
#define OPERATOR4    3
/* *********** FM Operator indexes *end******* */

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

struct opnInstData
{
    //! Operators prepared for sending to OPL chip emulator
    OPN_Operator    OPS[4];
    uint8_t         fbalg;
    uint8_t         lfosens;
    //! Note offset
    int16_t         finetune;
};

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
