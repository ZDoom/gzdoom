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

#include "adlmidi_midiplay.hpp"
#include "adlmidi_opl3.hpp"
#include "adlmidi_private.hpp"
#include "wopl/wopl_file.h"


std::string ADLMIDI_ErrorString;

// Generator callback on audio rate ticks

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
void adl_audioTickHandler(void *instance, uint32_t chipId, uint32_t rate)
{
    reinterpret_cast<MIDIplay *>(instance)->AudioTick(chipId, rate);
}
#endif

int adlCalculateFourOpChannels(MIDIplay *play, bool silent)
{
    Synth &synth = *play->m_synth;
    size_t n_fourop[2] = {0, 0}, n_total[2] = {0, 0};
    bool rhythmModeNeeded = false;
    size_t numFourOps = 0;

    //Automatically calculate how much 4-operator channels is necessary
    {
        //For custom bank
        Synth::BankMap::iterator it = synth.m_insBanks.begin();
        Synth::BankMap::iterator end = synth.m_insBanks.end();
        for(; it != end; ++it)
        {
            size_t bank = it->first;
            size_t div = (bank & Synth::PercussionTag) ? 1 : 0;
            for(size_t i = 0; i < 128; ++i)
            {
                OplInstMeta &ins = it->second.ins[i];
                if(ins.flags & OplInstMeta::Flag_NoSound)
                    continue;
                if((ins.flags & OplInstMeta::Flag_Real4op) != 0)
                    ++n_fourop[div];
                ++n_total[div];
                if(div && ((ins.flags & OplInstMeta::Mask_RhythmMode) != 0))
                    rhythmModeNeeded = true;
            }
        }
    }

    // All 2ops (no 4ops)
    if((n_fourop[0] == 0) && (n_fourop[1] == 0))
        numFourOps = 0;
    // All 2op melodics and Some (or All) 4op drums
    else if((n_fourop[0] == 0) && (n_fourop[1] > 0))
        numFourOps = 2;
    // Many 4op melodics
    else if((n_fourop[0] >= (n_total[0] * 7) / 8))
        numFourOps = 6;
    // Few 4op melodics
    else if(n_fourop[0] > 0)
        numFourOps = 4;

    synth.m_numFourOps = static_cast<unsigned>(numFourOps * synth.m_numChips);

    // Update channel categories and set up four-operator channels
    if(!silent)
        synth.updateChannelCategories();

    // Set rhythm mode when it needed
    synth.m_rhythmMode = rhythmModeNeeded;

    return 0;
}

#ifndef DISABLE_EMBEDDED_BANKS
void adlFromInstrument(const BanksDump::InstrumentEntry &instIn, OplInstMeta &instOut)
{
    instOut.voice2_fine_tune = 0.0;
    if(instIn.secondVoiceDetune != 0)
        instOut.voice2_fine_tune = (double)((((int)instIn.secondVoiceDetune + 128) >> 1) - 64) / 32.0;

    instOut.midiVelocityOffset = instIn.midiVelocityOffset;
    instOut.drumTone = instIn.percussionKeyNumber;
    instOut.flags = (instIn.instFlags & WOPL_Ins_4op) && (instIn.instFlags & WOPL_Ins_Pseudo4op) ? OplInstMeta::Flag_Pseudo4op : 0;
    instOut.flags|= (instIn.instFlags & WOPL_Ins_4op) && ((instIn.instFlags & WOPL_Ins_Pseudo4op) == 0) ? OplInstMeta::Flag_Real4op : 0;
    instOut.flags|= (instIn.instFlags & WOPL_Ins_IsBlank) ? OplInstMeta::Flag_NoSound : 0;
    instOut.flags|= instIn.instFlags & WOPL_RhythmModeMask;

    for(size_t op = 0; op < 2; op++)
    {
        if((instIn.ops[(op * 2) + 0] < 0) || (instIn.ops[(op * 2) + 1] < 0))
            break;
        const BanksDump::Operator &op1 = g_embeddedBanksOperators[instIn.ops[(op * 2) + 0]];
        const BanksDump::Operator &op2 = g_embeddedBanksOperators[instIn.ops[(op * 2) + 1]];
        instOut.op[op].modulator_E862 = op1.d_E862;
        instOut.op[op].modulator_40   = op1.d_40;
        instOut.op[op].carrier_E862 = op2.d_E862;
        instOut.op[op].carrier_40   = op2.d_40;
        instOut.op[op].feedconn = (instIn.fbConn >> (op * 8)) & 0xFF;
        instOut.op[op].noteOffset = static_cast<int8_t>(op == 0 ? instIn.noteOffset1 : instIn.noteOffset2);
    }

    instOut.soundKeyOnMs  = instIn.delay_on_ms;
    instOut.soundKeyOffMs = instIn.delay_off_ms;
}
#endif
