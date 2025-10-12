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

#include "oplinst.h"
#include "wopl/wopl_file.h"
#include <cmath>

template <class WOPLI>
static void cvt_generic_to_FMIns(OplInstMeta &ins, const WOPLI &in)
{
    ins.voice2_fine_tune = 0.0;
    int voice2_fine_tune = in.second_voice_detune;

    if(voice2_fine_tune != 0)
    {
        // Simulate behavior of DMX second voice detune
        ins.voice2_fine_tune = (double)(((voice2_fine_tune + 128) >> 1) - 64) / 32.0;
    }

    ins.midiVelocityOffset = in.midi_velocity_offset;
    ins.drumTone = in.percussion_key_number;
    ins.flags = (in.inst_flags & WOPL_Ins_4op) && (in.inst_flags & WOPL_Ins_Pseudo4op) ? OplInstMeta::Flag_Pseudo4op : 0;
    ins.flags|= (in.inst_flags & WOPL_Ins_4op) && ((in.inst_flags & WOPL_Ins_Pseudo4op) == 0) ? OplInstMeta::Flag_Real4op : 0;
    ins.flags|= (in.inst_flags & WOPL_Ins_IsBlank) ? OplInstMeta::Flag_NoSound : 0;
    ins.flags|= in.inst_flags & WOPL_RhythmModeMask;

    for(size_t op = 0, slt = 0; op < 4; op++, slt++)
    {
        ins.op[slt].carrier_E862 =
            ((static_cast<uint32_t>(in.operators[op].waveform_E0) << 24) & 0xFF000000) //WaveForm
            | ((static_cast<uint32_t>(in.operators[op].susrel_80) << 16) & 0x00FF0000) //SusRel
            | ((static_cast<uint32_t>(in.operators[op].atdec_60) << 8) & 0x0000FF00)   //AtDec
            | ((static_cast<uint32_t>(in.operators[op].avekf_20) << 0) & 0x000000FF);  //AVEKM
        ins.op[slt].carrier_40 = in.operators[op].ksl_l_40;//KSLL

        op++;
        ins.op[slt].modulator_E862 =
            ((static_cast<uint32_t>(in.operators[op].waveform_E0) << 24) & 0xFF000000) //WaveForm
            | ((static_cast<uint32_t>(in.operators[op].susrel_80) << 16) & 0x00FF0000) //SusRel
            | ((static_cast<uint32_t>(in.operators[op].atdec_60) << 8) & 0x0000FF00)   //AtDec
            | ((static_cast<uint32_t>(in.operators[op].avekf_20) << 0) & 0x000000FF);  //AVEKM
        ins.op[slt].modulator_40 = in.operators[op].ksl_l_40;//KSLL
    }

    ins.op[0].noteOffset = static_cast<int8_t>(in.note_offset1);
    ins.op[0].feedconn = in.fb_conn1_C0;
    ins.op[1].noteOffset = static_cast<int8_t>(in.note_offset2);
    ins.op[1].feedconn = in.fb_conn2_C0;

    ins.soundKeyOnMs  = in.delay_on_ms;
    ins.soundKeyOffMs = in.delay_off_ms;
}

template <class WOPLI>
static void cvt_FMIns_to_generic(WOPLI &ins, const OplInstMeta &in)
{
    ins.second_voice_detune = 0;
    double voice2_fine_tune = in.voice2_fine_tune;
    if(voice2_fine_tune != 0)
    {
        int m = (int)(voice2_fine_tune * 32.0);
        m += 64;
        m <<= 1;
        m -= 128;
        ins.second_voice_detune = (uint8_t)m;
    }

    ins.midi_velocity_offset = in.midiVelocityOffset;
    ins.percussion_key_number = in.drumTone;
    ins.inst_flags = (in.flags & (OplInstMeta::Flag_Pseudo4op|OplInstMeta::Flag_Real4op)) ? WOPL_Ins_4op : 0;
    ins.inst_flags|= (in.flags & OplInstMeta::Flag_Pseudo4op) ? WOPL_Ins_Pseudo4op : 0;
    ins.inst_flags|= (in.flags & OplInstMeta::Flag_NoSound) ? WOPL_Ins_IsBlank : 0;
    ins.inst_flags |= in.flags & OplInstMeta::Mask_RhythmMode;

    for(size_t op = 0; op < 4; op++)
    {
        const OplTimbre &in2op = in.op[(op < 2) ? 0 : 1];
        uint32_t regE862 = ((op & 1) == 0) ? in2op.carrier_E862 : in2op.modulator_E862;
        uint8_t reg40 = ((op & 1) == 0) ? in2op.carrier_40 : in2op.modulator_40;

        ins.operators[op].waveform_E0 = static_cast<uint8_t>(regE862 >> 24);
        ins.operators[op].susrel_80 = static_cast<uint8_t>(regE862 >> 16);
        ins.operators[op].atdec_60 = static_cast<uint8_t>(regE862 >> 8);
        ins.operators[op].avekf_20 = static_cast<uint8_t>(regE862 >> 0);
        ins.operators[op].ksl_l_40 = reg40;
    }

    ins.note_offset1 = in.op[0].noteOffset;
    ins.fb_conn1_C0 = in.op[0].feedconn;
    ins.note_offset2 = in.op[1].noteOffset;
    ins.fb_conn2_C0 = in.op[1].feedconn;

    ins.delay_on_ms = in.soundKeyOnMs;
    ins.delay_off_ms = in.soundKeyOffMs;
}
