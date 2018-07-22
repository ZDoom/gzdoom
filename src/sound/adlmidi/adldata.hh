/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
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

#ifndef ADLDATA_H
#define ADLDATA_H

#include <string.h>
#include <stdint.h>

#pragma pack(push, 1)
#define ADLDATA_BYTE_COMPARABLE(T)                      \
    inline bool operator==(const T &a, const T &b)      \
    { return !memcmp(&a, &b, sizeof(T)); }              \
    inline bool operator!=(const T &a, const T &b)      \
    { return !operator==(a, b); }

extern const struct adldata
{
    uint32_t    modulator_E862, carrier_E862;  // See below
    uint8_t     modulator_40, carrier_40; // KSL/attenuation settings
    uint8_t     feedconn; // Feedback/connection bits for the channel

    int8_t      finetune;
} adl[];
ADLDATA_BYTE_COMPARABLE(struct adldata)
enum { adlDefaultNumber = 189 };

extern const struct adlinsdata
{
    enum { Flag_Pseudo4op = 0x01, Flag_NoSound = 0x02, Flag_Real4op = 0x04 };

    uint16_t    adlno1, adlno2;
    uint8_t     tone;
    uint8_t     flags;
    uint16_t    ms_sound_kon;  // Number of milliseconds it produces sound;
    uint16_t    ms_sound_koff;
    double      voice2_fine_tune;
} adlins[];
ADLDATA_BYTE_COMPARABLE(struct adlinsdata)
int maxAdlBanks();
extern const unsigned short banks[][256];
extern const char* const banknames[];

enum { adlNoteOnMaxTime = 40000 };

/**
 * @brief Instrument data with operators included
 */
struct adlinsdata2
{
    adldata     adl[2];
    uint8_t     tone;
    uint8_t     flags;
    uint16_t    ms_sound_kon;  // Number of milliseconds it produces sound;
    uint16_t    ms_sound_koff;
    double      voice2_fine_tune;
    adlinsdata2() {}
    explicit adlinsdata2(const adlinsdata &d);
};
ADLDATA_BYTE_COMPARABLE(struct adlinsdata2)

#undef ADLDATA_BYTE_COMPARABLE
#pragma pack(pop)

/**
 * @brief Bank global setup
 */
extern const struct AdlBankSetup
{
    int     volumeModel;
    bool    deepTremolo;
    bool    deepVibrato;
    bool    adLibPercussions;
    bool    scaleModulators;
} adlbanksetup[];

/**
 * @brief Conversion of storage formats
 */
inline adlinsdata2::adlinsdata2(const adlinsdata &d)
    : tone(d.tone), flags(d.flags),
      ms_sound_kon(d.ms_sound_kon), ms_sound_koff(d.ms_sound_koff),
      voice2_fine_tune(d.voice2_fine_tune)
{
    adl[0] = ::adl[d.adlno1];
    adl[1] = ::adl[d.adlno2];
}

/**
 * @brief Convert external instrument to internal instrument
 */
void cvt_ADLI_to_FMIns(adlinsdata2 &dst, const struct ADL_Instrument &src);

/**
 * @brief Convert internal instrument to external instrument
 */
void cvt_FMIns_to_ADLI(struct ADL_Instrument &dst, const adlinsdata2 &src);

#endif //ADLDATA_H
