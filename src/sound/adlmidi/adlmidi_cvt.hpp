/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
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

#include "adldata.hh"
#include "wopl/wopl_file.h"
#include <cmath>

template <class WOPLI>
static void cvt_generic_to_FMIns(adlinsdata2 &ins, const WOPLI &in)
{
    ins.voice2_fine_tune = 0.0;
    int8_t voice2_fine_tune = in.second_voice_detune;
    if(voice2_fine_tune != 0)
    {
        if(voice2_fine_tune == 1)
            ins.voice2_fine_tune = 0.000025;
        else if(voice2_fine_tune == -1)
            ins.voice2_fine_tune = -0.000025;
        else
            ins.voice2_fine_tune = voice2_fine_tune * (15.625 / 1000.0);
    }

    ins.midi_velocity_offset = in.midi_velocity_offset;
    ins.tone = in.percussion_key_number;
    ins.flags = (in.inst_flags & WOPL_Ins_4op) && (in.inst_flags & WOPL_Ins_Pseudo4op) ? adlinsdata::Flag_Pseudo4op : 0;
    ins.flags|= (in.inst_flags & WOPL_Ins_4op) && ((in.inst_flags & WOPL_Ins_Pseudo4op) == 0) ? adlinsdata::Flag_Real4op : 0;
    ins.flags|= (in.inst_flags & WOPL_Ins_IsBlank) ? adlinsdata::Flag_NoSound : 0;
    ins.flags|= in.inst_flags & WOPL_RhythmModeMask;

    for(size_t op = 0, slt = 0; op < 4; op++, slt++)
    {
        ins.adl[slt].carrier_E862 =
            ((static_cast<uint32_t>(in.operators[op].waveform_E0) << 24) & 0xFF000000) //WaveForm
            | ((static_cast<uint32_t>(in.operators[op].susrel_80) << 16) & 0x00FF0000) //SusRel
            | ((static_cast<uint32_t>(in.operators[op].atdec_60) << 8) & 0x0000FF00)   //AtDec
            | ((static_cast<uint32_t>(in.operators[op].avekf_20) << 0) & 0x000000FF);  //AVEKM
        ins.adl[slt].carrier_40 = in.operators[op].ksl_l_40;//KSLL

        op++;
        ins.adl[slt].modulator_E862 =
            ((static_cast<uint32_t>(in.operators[op].waveform_E0) << 24) & 0xFF000000) //WaveForm
            | ((static_cast<uint32_t>(in.operators[op].susrel_80) << 16) & 0x00FF0000) //SusRel
            | ((static_cast<uint32_t>(in.operators[op].atdec_60) << 8) & 0x0000FF00)   //AtDec
            | ((static_cast<uint32_t>(in.operators[op].avekf_20) << 0) & 0x000000FF);  //AVEKM
        ins.adl[slt].modulator_40 = in.operators[op].ksl_l_40;//KSLL
    }

    ins.adl[0].finetune = static_cast<int8_t>(in.note_offset1);
    ins.adl[0].feedconn = in.fb_conn1_C0;
    ins.adl[1].finetune = static_cast<int8_t>(in.note_offset2);
    ins.adl[1].feedconn = in.fb_conn2_C0;

    ins.ms_sound_kon  = in.delay_on_ms;
    ins.ms_sound_koff = in.delay_off_ms;
}

template <class WOPLI>
static void cvt_FMIns_to_generic(WOPLI &ins, const adlinsdata2 &in)
{
    ins.second_voice_detune = 0;
    double voice2_fine_tune = in.voice2_fine_tune;
    if(voice2_fine_tune != 0)
    {
        if(voice2_fine_tune > 0 && voice2_fine_tune <= 0.000025)
            ins.second_voice_detune = 1;
        else if(voice2_fine_tune < 0 && voice2_fine_tune >= -0.000025)
            ins.second_voice_detune = -1;
        else
        {
            long value = static_cast<long>(round(voice2_fine_tune * (1000.0 / 15.625)));
            value = (value < -128) ? -128 : value;
            value = (value > +127) ? +127 : value;
            ins.second_voice_detune = static_cast<int8_t>(value);
        }
    }

    ins.midi_velocity_offset = in.midi_velocity_offset;
    ins.percussion_key_number = in.tone;
    ins.inst_flags = (in.flags & (adlinsdata::Flag_Pseudo4op|adlinsdata::Flag_Real4op)) ? WOPL_Ins_4op : 0;
    ins.inst_flags|= (in.flags & adlinsdata::Flag_Pseudo4op) ? WOPL_Ins_Pseudo4op : 0;
    ins.inst_flags|= (in.flags & adlinsdata::Flag_NoSound) ? WOPL_Ins_IsBlank : 0;
    ins.inst_flags |= in.flags & adlinsdata::Mask_RhythmMode;

    for(size_t op = 0; op < 4; op++)
    {
        const adldata &in2op = in.adl[(op < 2) ? 0 : 1];
        uint32_t regE862 = ((op & 1) == 0) ? in2op.carrier_E862 : in2op.modulator_E862;
        uint8_t reg40 = ((op & 1) == 0) ? in2op.carrier_40 : in2op.modulator_40;

        ins.operators[op].waveform_E0 = static_cast<uint8_t>(regE862 >> 24);
        ins.operators[op].susrel_80 = static_cast<uint8_t>(regE862 >> 16);
        ins.operators[op].atdec_60 = static_cast<uint8_t>(regE862 >> 8);
        ins.operators[op].avekf_20 = static_cast<uint8_t>(regE862 >> 0);
        ins.operators[op].ksl_l_40 = reg40;
    }

    ins.note_offset1 = in.adl[0].finetune;
    ins.fb_conn1_C0 = in.adl[0].feedconn;
    ins.note_offset2 = in.adl[1].finetune;
    ins.fb_conn2_C0 = in.adl[1].feedconn;

    ins.delay_on_ms = in.ms_sound_kon;
    ins.delay_off_ms = in.ms_sound_koff;
}
