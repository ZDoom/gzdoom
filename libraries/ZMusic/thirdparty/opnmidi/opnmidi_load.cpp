/*
 * libOPNMIDI is a free Software MIDI synthesizer library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * OPNMIDI Library and YM2612 support:   Copyright (c) 2017-2025 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "opnmidi_midiplay.hpp"
#include "opnmidi_opn2.hpp"
#include "opnmidi_private.hpp"
#include "opnmidi_cvt.hpp"
#include "file_reader.hpp"
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
#include "midi_sequencer.hpp"
#endif
#include "wopn/wopn_file.h"

bool OPNMIDIplay::LoadBank(const std::string &filename)
{
    FileAndMemReader file;
    file.openFile(filename.c_str());
    return LoadBank(file);
}

bool OPNMIDIplay::LoadBank(const void *data, size_t size)
{
    FileAndMemReader file;
    file.openData(data, (size_t)size);
    return LoadBank(file);
}

void cvt_OPNI_to_FMIns(OpnInstMeta &ins, const OPN2_Instrument &in)
{
    return cvt_generic_to_FMIns(ins, in);
}

void cvt_FMIns_to_OPNI(OPN2_Instrument &ins, const OpnInstMeta &in)
{
    cvt_FMIns_to_generic(ins, in);
}

bool OPNMIDIplay::LoadBank(FileAndMemReader &fr)
{
    int err = 0;
    WOPNFile *wopn = NULL;
    char *raw_file_data = NULL;
    size_t  fsize;
    if(!fr.isValid())
    {
        errorStringOut = "Custom bank: Invalid data stream!";
        return false;
    }

    // Read complete bank file into the memory
    fsize = fr.fileSize();
    fr.seek(0, FileAndMemReader::SET);
    // Allocate necessary memory block
    raw_file_data = (char*)malloc(fsize);
    if(!raw_file_data)
    {
        errorStringOut = "Custom bank: Out of memory before of read!";
        return false;
    }
    fr.read(raw_file_data, 1, fsize);

    // Parse bank file from the memory
    wopn = WOPN_LoadBankFromMem((void*)raw_file_data, fsize, &err);
    //Free the buffer no more needed
    free(raw_file_data);

    // Check for any erros
    if(!wopn)
    {
        switch(err)
        {
        case WOPN_ERR_BAD_MAGIC:
            errorStringOut = "Custom bank: Invalid magic!";
            return false;
        case WOPN_ERR_UNEXPECTED_ENDING:
            errorStringOut = "Custom bank: Unexpected ending!";
            return false;
        case WOPN_ERR_INVALID_BANKS_COUNT:
            errorStringOut = "Custom bank: Invalid banks count!";
            return false;
        case WOPN_ERR_NEWER_VERSION:
            errorStringOut = "Custom bank: Version is newer than supported by this library!";
            return false;
        case WOPN_ERR_OUT_OF_MEMORY:
            errorStringOut = "Custom bank: Out of memory!";
            return false;
        default:
            errorStringOut = "Custom bank: Unknown error!";
            return false;
        }
    }

    Synth &synth = *m_synth;
    synth.m_insBankSetup.volumeModel = wopn->volume_model;
    synth.m_insBankSetup.lfoEnable = (wopn->lfo_freq & 8) != 0;
    synth.m_insBankSetup.lfoFrequency = wopn->lfo_freq & 7;
    synth.m_insBankSetup.chipType = wopn->chip_type;
    // FIXME: Implement the bank-side flag to enable this
    synth.m_insBankSetup.mt32defaults = false;
    m_setup.VolumeModel = OPNMIDI_VolumeModel_AUTO;
    m_setup.lfoEnable = -1;
    m_setup.lfoFrequency = -1;
    m_setup.chipType = -1;

    synth.m_insBanks.clear();

    uint16_t slots_counts[2] = {wopn->banks_count_melodic, wopn->banks_count_percussion};
    WOPNBank *slots_src_ins[2] = { wopn->banks_melodic, wopn->banks_percussive };

    for(size_t ss = 0; ss < 2; ss++)
    {
        for(size_t i = 0; i < slots_counts[ss]; i++)
        {
            size_t bankno = (slots_src_ins[ss][i].bank_midi_msb * 256) +
                            (slots_src_ins[ss][i].bank_midi_lsb) +
                            (ss ? size_t(Synth::PercussionTag) : 0);
            Synth::Bank &bank = synth.m_insBanks[bankno];
            for(int j = 0; j < 128; j++)
            {
                OpnInstMeta &ins = bank.ins[j];
                std::memset(&ins, 0, sizeof(OpnInstMeta));
                WOPNInstrument &inIns = slots_src_ins[ss][i].ins[j];
                cvt_generic_to_FMIns(ins, inIns);
            }
        }
    }

    applySetup();

    WOPN_Free(wopn);

    return true;
}

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER

bool OPNMIDIplay::LoadMIDI_pre()
{
    Synth &synth = *m_synth;
    if(synth.m_insBanks.empty())
    {
        errorStringOut = "Bank is not set! Please load any instruments bank by using of adl_openBankFile() or adl_openBankData() functions!";
        return false;
    }

    /**** Set all properties BEFORE starting of actial file reading! ****/
    resetMIDI();
    applySetup();

    return true;
}

bool OPNMIDIplay::LoadMIDI_post()
{
    Synth &synth = *m_synth;
    MidiSequencer &seq = *m_sequencer;
    MidiSequencer::FileFormat format = seq.getFormat();
    if(format == MidiSequencer::Format_CMF)
    {
        errorStringOut = "OPNMIDI doesn't supports CMF, use ADLMIDI to play this file!";
        /* As joke, why not to try implemented the converter of patches from OPL3 into OPN2? */
        return false;
    }
    else if(format == MidiSequencer::Format_RSXX)
    {
        synth.m_musicMode     = Synth::MODE_RSXX;
        synth.m_volumeScale   = Synth::VOLUME_Generic;
        synth.m_numChips = 2;
    }
    else if(format == MidiSequencer::Format_IMF)
    {
        errorStringOut = "OPNMIDI doesn't supports IMF, use ADLMIDI to play this file!";
        /* Same as for CMF */
        return false;
    }
    else if(format == MidiSequencer::Format_XMIDI)
        synth.m_musicMode = Synth::MODE_XMIDI;

    m_setup.tick_skip_samples_delay = 0;
    synth.reset(m_setup.emulator, m_setup.PCM_RATE, synth.chipFamily(), this); // Reset OPN2 chip
    m_chipChannels.clear();
    m_chipChannels.resize(synth.m_numChannels);
    resetMIDIDefaults();
#ifdef OPNMIDI_MIDI2VGM
    m_sequencerInterface->onloopStart = synth.m_loopStartHook;
    m_sequencerInterface->onloopStart_userData = synth.m_loopStartHookData;
    m_sequencerInterface->onloopEnd = synth.m_loopEndHook;
    m_sequencerInterface->onloopEnd_userData = synth.m_loopEndHookData;
    m_sequencer->setLoopHooksOnly(m_sequencerInterface->onloopStart != NULL);
#endif

    return true;
}


bool OPNMIDIplay::LoadMIDI(const std::string &filename)
{
    FileAndMemReader file;
    file.openFile(filename.c_str());
    if(!LoadMIDI_pre())
        return false;
    MidiSequencer &seq = *m_sequencer;
    if(!seq.loadMIDI(file))
    {
        errorStringOut = seq.getErrorString();
        return false;
    }
    if(!LoadMIDI_post())
        return false;
    return true;
}

bool OPNMIDIplay::LoadMIDI(const void *data, size_t size)
{
    FileAndMemReader file;
    file.openData(data, size);
    if(!LoadMIDI_pre())
        return false;
    MidiSequencer &seq = *m_sequencer;
    if(!seq.loadMIDI(file))
    {
        errorStringOut = seq.getErrorString();
        return false;
    }
    if(!LoadMIDI_post())
        return false;
    return true;
}

#endif //OPNMIDI_DISABLE_MIDI_SEQUENCER
