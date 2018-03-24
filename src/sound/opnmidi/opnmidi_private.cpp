/*
 * libOPNMIDI is a free MIDI to WAV conversion library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * OPNMIDI Library and YM2612 support:   Copyright (c) 2017-2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "opnmidi_private.hpp"

std::string OPN2MIDI_ErrorString;

int opn2RefreshNumCards(OPN2_MIDIPlayer * /*device*/)
{
//    OPNMIDIplay *play = reinterpret_cast<OPNMIDIplay *>(device->opn2_midiPlayer);
    //OPN uses 4-op instruments only
//    unsigned n_fourop[2] = {0, 0}, n_total[2] = {0, 0};
//    for(unsigned a = 0; a < 256; ++a)
//    {
//        unsigned insno = banks[device->OpnBank][a];
//        if(insno == 198) continue;
//        ++n_total[a / 128];
//        if(adlins[insno].adlno1 != adlins[insno].adlno2)
//            ++n_fourop[a / 128];
//    }

//    device->NumFourOps =
//        (n_fourop[0] >= n_total[0] * 7 / 8) ? device->NumCards * 6
//        : (n_fourop[0] < n_total[0] * 1 / 8) ? 0
//        : (device->NumCards == 1 ? 1 : device->NumCards * 4);
//    reinterpret_cast<OPNMIDIplay *>(device->opn2_midiPlayer)->opn.NumFourOps = device->NumFourOps;
//    if(n_fourop[0] >= n_total[0] * 15 / 16 && device->NumFourOps == 0)
//    {
//        OPN2MIDI_ErrorString = "ERROR: You have selected a bank that consists almost exclusively of four-op patches.\n"
//                              "       The results (silence + much cpu load) would be probably\n"
//                              "       not what you want, therefore ignoring the request.\n";
//        return -1;
//    }
    return 0;
}
