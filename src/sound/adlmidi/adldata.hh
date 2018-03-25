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

#include <stdint.h>

extern const struct adldata
{
    uint32_t    modulator_E862, carrier_E862;  // See below
    uint8_t     modulator_40, carrier_40; // KSL/attenuation settings
    uint8_t     feedconn; // Feedback/connection bits for the channel

    int8_t      finetune;
} adl[];

extern const struct adlinsdata
{
    enum { Flag_Pseudo4op = 0x01, Flag_NoSound = 0x02 };

    uint16_t    adlno1, adlno2;
    uint8_t     tone;
    uint8_t     flags;
    uint16_t    ms_sound_kon;  // Number of milliseconds it produces sound;
    uint16_t    ms_sound_koff;
    double voice2_fine_tune;
} adlins[];
int maxAdlBanks();
extern const unsigned short banks[][256];
extern const char* const banknames[];

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

#endif //ADLDATA_H
