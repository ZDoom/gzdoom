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


#ifndef ADLDATA_DB_H
#define ADLDATA_DB_H

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vector>

#if !defined(_MSC_VER) && !defined(__APPLE__) && !defined(__aarch64__) && !defined(__3DS__)
#define ATTRIB_PACKED __attribute__((__packed__))
#else
#define ATTRIB_PACKED
#endif

typedef uint16_t bank_count_t;
typedef int16_t midi_bank_idx_t;

#ifndef DISABLE_EMBEDDED_BANKS
extern const size_t g_embeddedBanksCount;
#endif

namespace BanksDump
{

struct BankEntry
{
    uint16_t bankSetup;
    bank_count_t banksMelodicCount;
    bank_count_t banksPercussionCount;
    const char *title;
    bank_count_t banksOffsetMelodic;
    bank_count_t banksOffsetPercussive;
} ATTRIB_PACKED;

struct MidiBank
{
    uint8_t msb;
    uint8_t lsb;
    midi_bank_idx_t insts[128];
} ATTRIB_PACKED;

struct InstrumentEntry
{
    int16_t noteOffset1;
    int16_t noteOffset2;
    int8_t  midiVelocityOffset;
    uint8_t percussionKeyNumber;
    uint8_t instFlags;
    int8_t  secondVoiceDetune;
    uint16_t fbConn;
    uint16_t delay_on_ms;
    uint16_t delay_off_ms;
    int16_t ops[4];
} ATTRIB_PACKED;

struct Operator
{
    uint32_t d_E862;
    uint8_t  d_40;
} ATTRIB_PACKED;

} /* namespace BanksDump */

#ifndef DISABLE_EMBEDDED_BANKS
extern const char* const g_embeddedBankNames[];
extern const BanksDump::BankEntry g_embeddedBanks[];
extern const size_t g_embeddedBanksMidiIndex[];
extern const BanksDump::MidiBank g_embeddedBanksMidi[];
extern const BanksDump::InstrumentEntry g_embeddedBanksInstruments[];
extern const BanksDump::Operator g_embeddedBanksOperators[];
#endif

#endif // ADLDATA_DB_H
