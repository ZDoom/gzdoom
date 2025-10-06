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
#include "adlmidi_cvt.hpp"
#include "file_reader.hpp"
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
#   define BWMIDI_ENABLE_OPL_MUSIC_SUPPORT
#   include "midi_sequencer.hpp"
#endif
#include "wopl/wopl_file.h"

bool MIDIplay::LoadBank(const std::string &filename)
{
    FileAndMemReader file;
    file.openFile(filename.c_str());
    return LoadBank(file);
}

bool MIDIplay::LoadBank(const void *data, size_t size)
{
    FileAndMemReader file;
    file.openData(data, size);
    return LoadBank(file);
}

void cvt_ADLI_to_FMIns(OplInstMeta &ins, const ADL_Instrument &in)
{
    return cvt_generic_to_FMIns(ins, in);
}

void cvt_FMIns_to_ADLI(ADL_Instrument &ins, const OplInstMeta &in)
{
    cvt_FMIns_to_generic(ins, in);
}

bool MIDIplay::LoadBank(FileAndMemReader &fr)
{
    int err = 0;
    WOPLFile *wopl = NULL;
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
    wopl = WOPL_LoadBankFromMem((void*)raw_file_data, fsize, &err);
    //Free the buffer no more needed
    free(raw_file_data);

    // Check for any erros
    if(!wopl)
    {
        switch(err)
        {
        case WOPL_ERR_BAD_MAGIC:
            errorStringOut = "Custom bank: Invalid magic!";
            return false;
        case WOPL_ERR_UNEXPECTED_ENDING:
            errorStringOut = "Custom bank: Unexpected ending!";
            return false;
        case WOPL_ERR_INVALID_BANKS_COUNT:
            errorStringOut = "Custom bank: Invalid banks count!";
            return false;
        case WOPL_ERR_NEWER_VERSION:
            errorStringOut = "Custom bank: Version is newer than supported by this library!";
            return false;
        case WOPL_ERR_OUT_OF_MEMORY:
            errorStringOut = "Custom bank: Out of memory!";
            return false;
        default:
            errorStringOut = "Custom bank: Unknown error!";
            return false;
        }
    }

    Synth &synth = *m_synth;

    synth.setEmbeddedBank(m_setup.bankId);

    synth.m_insBankSetup.scaleModulators = false;
    synth.m_insBankSetup.deepTremolo = (wopl->opl_flags & WOPL_FLAG_DEEP_TREMOLO) != 0;
    synth.m_insBankSetup.deepVibrato = (wopl->opl_flags & WOPL_FLAG_DEEP_VIBRATO) != 0;
    synth.m_insBankSetup.mt32defaults = (wopl->opl_flags & WOPL_FLAG_MT32) != 0;
    synth.m_insBankSetup.volumeModel = wopl->volume_model;
    m_setup.deepTremoloMode = -1;
    m_setup.deepVibratoMode = -1;
    m_setup.volumeScaleModel = ADLMIDI_VolumeModel_AUTO;

    uint16_t slots_counts[2] = {wopl->banks_count_melodic, wopl->banks_count_percussion};
    WOPLBank *slots_src_ins[2] = { wopl->banks_melodic, wopl->banks_percussive };

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
                OplInstMeta &ins = bank.ins[j];
                std::memset(&ins, 0, sizeof(OplInstMeta));
                WOPLInstrument &inIns = slots_src_ins[ss][i].ins[j];
                cvt_generic_to_FMIns(ins, inIns);
            }
        }
    }

    synth.m_embeddedBank = Synth::CustomBankTag; // Use dynamic banks!
    //Percussion offset is count of instruments multipled to count of melodic banks
    applySetup();

    WOPL_Free(wopl);

    return true;
}

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER

bool MIDIplay::LoadMIDI_pre()
{
#ifdef DISABLE_EMBEDDED_BANKS
    Synth &synth = *m_synth;
    if((synth.m_embeddedBank != Synth::CustomBankTag) || synth.m_insBanks.empty())
    {
        errorStringOut = "Bank is not set! Please load any instruments bank by using of adl_openBankFile() or adl_openBankData() functions!";
        return false;
    }
#endif
    /**** Set all properties BEFORE starting of actial file reading! ****/
    resetMIDI();
    applySetup();

    return true;
}

bool MIDIplay::LoadMIDI_post()
{
    Synth &synth = *m_synth;
    MidiSequencer &seq = *m_sequencer;
    MidiSequencer::FileFormat format = seq.getFormat();
    bool setToOPL2 = false;

    if(format == MidiSequencer::Format_CMF)
    {
        const std::vector<MidiSequencer::CmfInstrument> &instruments = seq.getRawCmfInstruments();
        synth.m_insBanks.clear();//Clean up old banks

        uint16_t ins_count = static_cast<uint16_t>(instruments.size());
        for(uint16_t i = 0; i < ins_count; ++i)
        {
            const uint8_t *insData = instruments[i].data;
            size_t bank = i / 256;
            bank = ((bank & 127) + ((bank >> 7) << 8));
            if(bank > 127 + (127 << 8))
                break;
            bank += (i % 256 < 128) ? 0 : size_t(Synth::PercussionTag);

            /*std::printf("Ins %3u: %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X\n",
                        i, InsData[0],InsData[1],InsData[2],InsData[3], InsData[4],InsData[5],InsData[6],InsData[7],
                           InsData[8],InsData[9],InsData[10],InsData[11], InsData[12],InsData[13],InsData[14],InsData[15]);*/
            OplInstMeta &adlins = synth.m_insBanks[bank].ins[i % 128];
            OplTimbre    adl;
            adl.modulator_E862 =
                ((static_cast<uint32_t>(insData[8] & 0x07) << 24) & 0xFF000000) //WaveForm
                | ((static_cast<uint32_t>(insData[6]) << 16) & 0x00FF0000) //Sustain/Release
                | ((static_cast<uint32_t>(insData[4]) << 8) & 0x0000FF00) //Attack/Decay
                | ((static_cast<uint32_t>(insData[0]) << 0) & 0x000000FF); //MultKEVA
            adl.carrier_E862 =
                ((static_cast<uint32_t>(insData[9] & 0x07) << 24) & 0xFF000000) //WaveForm
                | ((static_cast<uint32_t>(insData[7]) << 16) & 0x00FF0000) //Sustain/Release
                | ((static_cast<uint32_t>(insData[5]) << 8) & 0x0000FF00) //Attack/Decay
                | ((static_cast<uint32_t>(insData[1]) << 0) & 0x000000FF); //MultKEVA
            adl.modulator_40 = insData[2];
            adl.carrier_40   = insData[3];
            adl.feedconn     = insData[10] & 0x0F;
            adl.noteOffset = 0;
            adlins.op[0] = adl;
            adlins.op[1] = adl;
            adlins.soundKeyOnMs  = 1000;
            adlins.soundKeyOffMs = 500;
            adlins.drumTone  = 0;
            adlins.flags = 0;
            adlins.voice2_fine_tune = 0.0;
        }

        synth.m_embeddedBank = Synth::CustomBankTag; // Ignore AdlBank number, use dynamic banks instead
        //std::printf("CMF deltas %u ticks %u, basictempo = %u\n", deltas, ticks, basictempo);
        synth.m_rhythmMode = true;
        synth.m_musicMode = Synth::MODE_CMF;
        synth.m_volumeScale = Synth::VOLUME_NATIVE;
        setToOPL2 = true;

        synth.m_numChips = 1;
        synth.m_numFourOps = 0;
    }
    else if(format == MidiSequencer::Format_RSXX)
    {
        //opl.CartoonersVolumes = true;
        synth.m_musicMode     = Synth::MODE_RSXX;
        synth.m_volumeScale   = Synth::VOLUME_NATIVE;

        synth.m_numChips = 1;
        synth.m_numFourOps = 0;
    }
    else if(format == MidiSequencer::Format_IMF || format == MidiSequencer::Format_KLM)
    {
        //std::fprintf(stderr, "Done reading IMF file\n");
        synth.m_numFourOps  = 0; //Don't use 4-operator channels for IMF playing!
        synth.m_rhythmMode = false;//Don't enforce rhythm-mode when it's unneeded
        synth.m_musicMode = Synth::MODE_IMF;
        setToOPL2 = true;

        synth.m_numChips = 1;
        synth.m_numFourOps = 0;
    }
    else
    {
        if(format == MidiSequencer::Format_XMIDI)
            synth.m_musicMode = Synth::MODE_XMIDI;

        synth.m_numChips = m_setup.numChips;
        if(m_setup.numFourOps < 0)
            adlCalculateFourOpChannels(this, true);
    }

    resetMIDIDefaults();

    m_setup.tick_skip_samples_delay = 0;
    chipReset(); // Reset OPL3 chip
    //opl.Reset(); // ...twice (just in case someone misprogrammed OPL3 previously)
    m_chipChannels.clear();
    m_chipChannels.resize(synth.m_numChannels);

    if(setToOPL2)
        synth.toggleOPL3(false);

    return true;
}

bool MIDIplay::LoadMIDI(const std::string &filename)
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

bool MIDIplay::LoadMIDI(const void *data, size_t size)
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

#endif /* ADLMIDI_DISABLE_MIDI_SEQUENCER */
