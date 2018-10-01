/*
 * libOPNMIDI is a free MIDI to WAV conversion library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
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
static void cvt_generic_to_FMIns(opnInstMeta2 &ins, const WOPNI &in)
{
    ins.tone = in.percussion_key_number;
    ins.flags = in.inst_flags;
    /* Junk, delete later */
    ins.fine_tune = 0.0;
    /* Junk, delete later */

    ins.opn[0].fbalg = in.fbalg;
    ins.opn[0].lfosens = in.lfosens;
    ins.opn[0].finetune = in.note_offset;
    ins.midi_velocity_offset = in.midi_velocity_offset;

    for(size_t op = 0; op < 4; op++)
    {
        ins.opn[0].OPS[op].data[0] = in.operators[op].dtfm_30;
        ins.opn[0].OPS[op].data[1] = in.operators[op].level_40;
        ins.opn[0].OPS[op].data[2] = in.operators[op].rsatk_50;
        ins.opn[0].OPS[op].data[3] = in.operators[op].amdecay1_60;
        ins.opn[0].OPS[op].data[4] = in.operators[op].decay2_70;
        ins.opn[0].OPS[op].data[5] = in.operators[op].susrel_80;
        ins.opn[0].OPS[op].data[6] = in.operators[op].ssgeg_90;
    }

    ins.opn[1] = ins.opn[0];

    ins.ms_sound_kon  = in.delay_on_ms;
    ins.ms_sound_koff = in.delay_off_ms;
}

template <class WOPNI>
static void cvt_FMIns_to_generic(WOPNI &ins, const opnInstMeta2 &in)
{
    ins.percussion_key_number = in.tone;
    ins.inst_flags = in.flags;

    ins.fbalg = in.opn[0].fbalg;
    ins.lfosens = in.opn[0].lfosens;
    ins.note_offset = in.opn[0].finetune;

    ins.midi_velocity_offset = in.midi_velocity_offset;

    for(size_t op = 0; op < 4; op++)
    {
        ins.operators[op].dtfm_30 = in.opn[0].OPS[op].data[0];
        ins.operators[op].level_40 = in.opn[0].OPS[op].data[1];
        ins.operators[op].rsatk_50 = in.opn[0].OPS[op].data[2];
        ins.operators[op].amdecay1_60 = in.opn[0].OPS[op].data[3];
        ins.operators[op].decay2_70 = in.opn[0].OPS[op].data[4];
        ins.operators[op].susrel_80 = in.opn[0].OPS[op].data[5];
        ins.operators[op].ssgeg_90 = in.opn[0].OPS[op].data[6];
    }

    ins.delay_on_ms = in.ms_sound_kon;
    ins.delay_off_ms = in.ms_sound_koff;
}
