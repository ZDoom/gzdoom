/*
 * libOPNMIDI is a free Software MIDI synthesizer library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
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

#include "opnbank.h"

template <class WOPNI>
static void cvt_generic_to_FMIns(OpnInstMeta &ins, const WOPNI &in)
{
    ins.drumTone = in.percussion_key_number;
    ins.flags = in.inst_flags;
    /* Junk, delete later */
    ins.voice2_fine_tune = 0.0;
    /* Junk, delete later */

    ins.op[0].fbalg = in.fbalg;
    ins.op[0].lfosens = in.lfosens;
    ins.op[0].noteOffset = in.note_offset;
    ins.midiVelocityOffset = in.midi_velocity_offset;

    for(size_t op = 0; op < 4; op++)
    {
        ins.op[0].OPS[op].data[0] = in.operators[op].dtfm_30;
        ins.op[0].OPS[op].data[1] = in.operators[op].level_40;
        ins.op[0].OPS[op].data[2] = in.operators[op].rsatk_50;
        ins.op[0].OPS[op].data[3] = in.operators[op].amdecay1_60;
        ins.op[0].OPS[op].data[4] = in.operators[op].decay2_70;
        ins.op[0].OPS[op].data[5] = in.operators[op].susrel_80;
        ins.op[0].OPS[op].data[6] = in.operators[op].ssgeg_90;
    }

    ins.op[1] = ins.op[0];

    ins.soundKeyOnMs  = in.delay_on_ms;
    ins.soundKeyOffMs = in.delay_off_ms;
}

template <class WOPNI>
static void cvt_FMIns_to_generic(WOPNI &ins, const OpnInstMeta &in)
{
    ins.percussion_key_number = in.drumTone;
    ins.inst_flags = in.flags;

    ins.fbalg = in.op[0].fbalg;
    ins.lfosens = in.op[0].lfosens;
    ins.note_offset = in.op[0].noteOffset;

    ins.midi_velocity_offset = in.midiVelocityOffset;

    for(size_t op = 0; op < 4; op++)
    {
        ins.operators[op].dtfm_30 = in.op[0].OPS[op].data[0];
        ins.operators[op].level_40 = in.op[0].OPS[op].data[1];
        ins.operators[op].rsatk_50 = in.op[0].OPS[op].data[2];
        ins.operators[op].amdecay1_60 = in.op[0].OPS[op].data[3];
        ins.operators[op].decay2_70 = in.op[0].OPS[op].data[4];
        ins.operators[op].susrel_80 = in.op[0].OPS[op].data[5];
        ins.operators[op].ssgeg_90 = in.op[0].OPS[op].data[6];
    }

    ins.delay_on_ms = in.soundKeyOnMs;
    ins.delay_off_ms = in.soundKeyOffMs;
}
