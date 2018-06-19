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

#include "adlmidi_private.hpp"
#include "wopl/wopl_file.h"

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
#   ifndef ADLMIDI_DISABLE_MUS_SUPPORT
#       include "adlmidi_mus2mid.h"
#   endif//MUS
#   ifndef ADLMIDI_DISABLE_XMI_SUPPORT
#       include "adlmidi_xmi2mid.h"
#   endif//XMI
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
uint64_t MIDIplay::ReadBEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(buffer);

    for(unsigned n = 0; n < nbytes; ++n)
        result = (result << 8) + data[n];

    return result;
}

uint64_t MIDIplay::ReadLEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(buffer);

    for(unsigned n = 0; n < nbytes; ++n)
        result = result + static_cast<uint64_t>(data[n] << (n * 8));

    return result;
}

#endif

bool MIDIplay::LoadBank(const std::string &filename)
{
    fileReader file;
    file.openFile(filename.c_str());
    return LoadBank(file);
}

bool MIDIplay::LoadBank(const void *data, size_t size)
{
    fileReader file;
    file.openData(data, size);
    return LoadBank(file);
}

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

    ins.tone = in.percussion_key_number;
    ins.flags = (in.inst_flags & WOPL_Ins_4op) && (in.inst_flags & WOPL_Ins_Pseudo4op) ? adlinsdata::Flag_Pseudo4op : 0;
    ins.flags|= (in.inst_flags & WOPL_Ins_4op) && ((in.inst_flags & WOPL_Ins_Pseudo4op) == 0) ? adlinsdata::Flag_Real4op : 0;
    ins.flags|= (in.inst_flags & WOPL_Ins_IsBlank) ? adlinsdata::Flag_NoSound : 0;

    bool fourOps = (in.inst_flags & WOPL_Ins_4op) || (in.inst_flags & WOPL_Ins_Pseudo4op);
    for(size_t op = 0, slt = 0; op < static_cast<size_t>(fourOps ? 4 : 2); op++, slt++)
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
    if(!fourOps)
        ins.adl[1] = ins.adl[0];
    else
    {
        ins.adl[1].finetune = static_cast<int8_t>(in.note_offset2);
        ins.adl[1].feedconn = in.fb_conn2_C0;
    }

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
            long value = lround(voice2_fine_tune * (1000.0 / 15.625));
            value = (value < -128) ? -128 : value;
            value = (value > +127) ? +127 : value;
            ins.second_voice_detune = static_cast<int8_t>(value);
        }
    }

    ins.percussion_key_number = in.tone;
    bool fourOps = (in.flags & adlinsdata::Flag_Pseudo4op) || in.adl[0] != in.adl[1];
    ins.inst_flags = fourOps ? WOPL_Ins_4op : 0;
    ins.inst_flags|= (in.flags & adlinsdata::Flag_Pseudo4op) ? WOPL_Ins_Pseudo4op : 0;
    ins.inst_flags|= (in.flags & adlinsdata::Flag_NoSound) ? WOPL_Ins_IsBlank : 0;

    for(size_t op = 0, slt = 0; op < static_cast<size_t>(fourOps ? 4 : 2); op++, slt++)
    {
        ins.operators[op].waveform_E0 = static_cast<uint8_t>(in.adl[slt].carrier_E862 >> 24);
        ins.operators[op].susrel_80 = static_cast<uint8_t>(in.adl[slt].carrier_E862 >> 16);
        ins.operators[op].atdec_60 = static_cast<uint8_t>(in.adl[slt].carrier_E862 >> 8);
        ins.operators[op].avekf_20 = static_cast<uint8_t>(in.adl[slt].carrier_E862 >> 0);
        ins.operators[op].ksl_l_40 = in.adl[slt].carrier_40;

        op++;
        ins.operators[op].waveform_E0 = static_cast<uint8_t>(in.adl[slt].carrier_E862 >> 24);
        ins.operators[op].susrel_80 = static_cast<uint8_t>(in.adl[slt].carrier_E862 >> 16);
        ins.operators[op].atdec_60 = static_cast<uint8_t>(in.adl[slt].carrier_E862 >> 8);
        ins.operators[op].avekf_20 = static_cast<uint8_t>(in.adl[slt].carrier_E862 >> 0);
        ins.operators[op].ksl_l_40 = in.adl[slt].carrier_40;
    }

    ins.note_offset1 = in.adl[0].finetune;
    ins.fb_conn1_C0 = in.adl[0].feedconn;
    if(!fourOps)
    {
        ins.operators[2] = ins.operators[0];
        ins.operators[3] = ins.operators[1];
    }
    else
    {
        ins.note_offset2 = in.adl[1].finetune;
        ins.fb_conn2_C0 = in.adl[1].feedconn;
    }

    ins.delay_on_ms = in.ms_sound_kon;
    ins.delay_off_ms = in.ms_sound_koff;
}

void cvt_ADLI_to_FMIns(adlinsdata2 &ins, const ADL_Instrument &in)
{
    return cvt_generic_to_FMIns(ins, in);
}

void cvt_FMIns_to_ADLI(ADL_Instrument &ins, const adlinsdata2 &in)
{
    cvt_FMIns_to_generic(ins, in);
}

bool MIDIplay::LoadBank(MIDIplay::fileReader &fr)
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
    fr.seek(0, SEEK_END);
    fsize = fr.tell();
    fr.seek(0, SEEK_SET);
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

    opl.dynamic_bank_setup.adLibPercussions = false;
    opl.dynamic_bank_setup.scaleModulators = false;
    opl.dynamic_bank_setup.deepTremolo = (wopl->opl_flags & WOPL_FLAG_DEEP_TREMOLO) != 0;
    opl.dynamic_bank_setup.deepVibrato = (wopl->opl_flags & WOPL_FLAG_DEEP_VIBRATO) != 0;
    opl.dynamic_bank_setup.volumeModel = wopl->volume_model;
    m_setup.HighTremoloMode = -1;
    m_setup.HighVibratoMode = -1;
    m_setup.VolumeModel = ADLMIDI_VolumeModel_AUTO;

    opl.setEmbeddedBank(m_setup.AdlBank);

    uint16_t slots_counts[2] = {wopl->banks_count_melodic, wopl->banks_count_percussion};
    WOPLBank *slots_src_ins[2] = { wopl->banks_melodic, wopl->banks_percussive };

    for(unsigned ss = 0; ss < 2; ss++)
    {
        for(unsigned i = 0; i < slots_counts[ss]; i++)
        {
            unsigned bankno =
                (slots_src_ins[ss][i].bank_midi_msb * 256) +
                slots_src_ins[ss][i].bank_midi_lsb +
                (ss ? OPL3::PercussionTag : 0);
            OPL3::Bank &bank = opl.dynamic_banks[bankno];
            for(int j = 0; j < 128; j++)
            {
                adlinsdata2 &ins = bank.ins[j];
                std::memset(&ins, 0, sizeof(adlinsdata2));
                WOPLInstrument &inIns = slots_src_ins[ss][i].ins[j];
                cvt_generic_to_FMIns(ins, inIns);
            }
        }
    }

    opl.AdlBank = ~0u; // Use dynamic banks!
    //Percussion offset is count of instruments multipled to count of melodic banks
    applySetup();

    WOPL_Free(wopl);

    return true;
}

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
bool MIDIplay::LoadMIDI(const std::string &filename)
{
    fileReader file;
    file.openFile(filename.c_str());
    if(!LoadMIDI(file))
        return false;
    return true;
}

bool MIDIplay::LoadMIDI(const void *data, size_t size)
{
    fileReader file;
    file.openData(data, size);
    return LoadMIDI(file);
}

bool MIDIplay::LoadMIDI(MIDIplay::fileReader &fr)
{
    size_t  fsize;
    ADL_UNUSED(fsize);
    //! Temp buffer for conversion
    AdlMIDI_CPtr<uint8_t> cvt_buf;
    errorString.clear();

    #ifdef DISABLE_EMBEDDED_BANKS
    if((opl.AdlBank != ~0u) || opl.dynamic_banks.empty())
    {
        errorStringOut = "Bank is not set! Please load any instruments bank by using of adl_openBankFile() or adl_openBankData() functions!";
        return false;
    }
    #endif

    if(!fr.isValid())
    {
        errorStringOut = "Invalid data stream!\n";
        #ifndef _WIN32
        errorStringOut += std::strerror(errno);
        #endif
        return false;
    }

    /**** Set all properties BEFORE starting of actial file reading! ****/
    applySetup();

    atEnd            = false;
    loopStart        = true;
    invalidLoop      = false;

    bool is_GMF = false; // GMD/MUS files (ScummVM)
    //bool is_MUS = false; // MUS/DMX files (Doom)
    bool is_IMF = false; // IMF
    bool is_CMF = false; // Creative Music format (CMF/CTMF)
    bool is_RSXX = false; // RSXX, such as Cartooners

    const size_t HeaderSize = 4 + 4 + 2 + 2 + 2; // 14
    char HeaderBuf[HeaderSize] = "";
    size_t DeltaTicks = 192, TrackCount = 1;

riffskip:
    fsize = fr.read(HeaderBuf, 1, HeaderSize);

    if(std::memcmp(HeaderBuf, "RIFF", 4) == 0)
    {
        fr.seek(6l, SEEK_CUR);
        goto riffskip;
    }

    if(std::memcmp(HeaderBuf, "GMF\x1", 4) == 0)
    {
        // GMD/MUS files (ScummVM)
        fr.seek(7 - static_cast<long>(HeaderSize), SEEK_CUR);
        is_GMF = true;
    }
    #ifndef ADLMIDI_DISABLE_MUS_SUPPORT
    else if(std::memcmp(HeaderBuf, "MUS\x1A", 4) == 0)
    {
        // MUS/DMX files (Doom)
        fr.seek(0, SEEK_END);
        size_t mus_len = fr.tell();
        fr.seek(0, SEEK_SET);
        uint8_t *mus = (uint8_t *)malloc(mus_len);
        if(!mus)
        {
            errorStringOut = "Out of memory!";
            return false;
        }
        fr.read(mus, 1, mus_len);
        //Close source stream
        fr.close();

        uint8_t *mid = NULL;
        uint32_t mid_len = 0;
        int m2mret = AdlMidi_mus2midi(mus, static_cast<uint32_t>(mus_len),
                                      &mid, &mid_len, 0);
        if(mus) free(mus);
        if(m2mret < 0)
        {
            errorStringOut = "Invalid MUS/DMX data format!";
            return false;
        }
        cvt_buf.reset(mid);
        //Open converted MIDI file
        fr.openData(mid, static_cast<size_t>(mid_len));
        //Re-Read header again!
        goto riffskip;
    }
    #endif //ADLMIDI_DISABLE_MUS_SUPPORT
    #ifndef ADLMIDI_DISABLE_XMI_SUPPORT
    else if(std::memcmp(HeaderBuf, "FORM", 4) == 0)
    {
        if(std::memcmp(HeaderBuf + 8, "XDIR", 4) != 0)
        {
            fr.close();
            errorStringOut = fr._fileName + ": Invalid format\n";
            return false;
        }

        fr.seek(0, SEEK_END);
        size_t mus_len = fr.tell();
        fr.seek(0, SEEK_SET);
        uint8_t *mus = (uint8_t*)malloc(mus_len);
        if(!mus)
        {
            errorStringOut = "Out of memory!";
            return false;
        }
        fr.read(mus, 1, mus_len);
        //Close source stream
        fr.close();

        uint8_t *mid = NULL;
        uint32_t mid_len = 0;
        int m2mret = AdlMidi_xmi2midi(mus, static_cast<uint32_t>(mus_len),
                                      &mid, &mid_len, XMIDI_CONVERT_NOCONVERSION);
        if(mus) free(mus);
        if(m2mret < 0)
        {
            errorStringOut = "Invalid XMI data format!";
            return false;
        }
        cvt_buf.reset(mid);
        //Open converted MIDI file
        fr.openData(mid, static_cast<size_t>(mid_len));
        //Re-Read header again!
        goto riffskip;
    }
    #endif //ADLMIDI_DISABLE_XMI_SUPPORT
    else if(std::memcmp(HeaderBuf, "CTMF", 4) == 0)
    {
        opl.dynamic_banks.clear();
        // Creative Music Format (CMF).
        // When playing CTMF files, use the following commandline:
        // adlmidi song8.ctmf -p -v 1 1 0
        // i.e. enable percussion mode, deeper vibrato, and use only 1 card.
        is_CMF = true;
        //unsigned version   = ReadLEint(HeaderBuf+4, 2);
        uint64_t ins_start = ReadLEint(HeaderBuf + 6, 2);
        uint64_t mus_start = ReadLEint(HeaderBuf + 8, 2);
        //unsigned deltas    = ReadLEint(HeaderBuf+10, 2);
        uint64_t ticks     = ReadLEint(HeaderBuf + 12, 2);
        // Read title, author, remarks start offsets in file
        fr.read(HeaderBuf, 1, 6);
        //unsigned long notes_starts[3] = {ReadLEint(HeaderBuf+0,2),ReadLEint(HeaderBuf+0,4),ReadLEint(HeaderBuf+0,6)};
        fr.seek(16, SEEK_CUR); // Skip the channels-in-use table
        fr.read(HeaderBuf, 1, 4);
        uint64_t ins_count =  ReadLEint(HeaderBuf + 0, 2); //, basictempo = ReadLEint(HeaderBuf+2, 2);
        fr.seek(static_cast<long>(ins_start), SEEK_SET);

        //std::printf("%u instruments\n", ins_count);
        for(unsigned i = 0; i < ins_count; ++i)
        {
            unsigned bank = i / 256;
            bank = (bank & 127) + ((bank >> 7) << 8);
            if(bank > 127 + (127 << 8))
                break;
            bank += (i % 256 < 128) ? 0 : OPL3::PercussionTag;

            unsigned char InsData[16];
            fr.read(InsData, 1, 16);
            /*std::printf("Ins %3u: %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X\n",
                        i, InsData[0],InsData[1],InsData[2],InsData[3], InsData[4],InsData[5],InsData[6],InsData[7],
                           InsData[8],InsData[9],InsData[10],InsData[11], InsData[12],InsData[13],InsData[14],InsData[15]);*/
            adlinsdata2 &adlins = opl.dynamic_banks[bank].ins[i % 128];
            adldata    adl;
            adl.modulator_E862 =
                ((static_cast<uint32_t>(InsData[8] & 0x07) << 24) & 0xFF000000) //WaveForm
                | ((static_cast<uint32_t>(InsData[6]) << 16) & 0x00FF0000) //Sustain/Release
                | ((static_cast<uint32_t>(InsData[4]) << 8) & 0x0000FF00) //Attack/Decay
                | ((static_cast<uint32_t>(InsData[0]) << 0) & 0x000000FF); //MultKEVA
            adl.carrier_E862 =
                ((static_cast<uint32_t>(InsData[9] & 0x07) << 24) & 0xFF000000) //WaveForm
                | ((static_cast<uint32_t>(InsData[7]) << 16) & 0x00FF0000) //Sustain/Release
                | ((static_cast<uint32_t>(InsData[5]) << 8) & 0x0000FF00) //Attack/Decay
                | ((static_cast<uint32_t>(InsData[1]) << 0) & 0x000000FF); //MultKEVA
            adl.modulator_40 = InsData[2];
            adl.carrier_40   = InsData[3];
            adl.feedconn     = InsData[10] & 0x0F;
            adl.finetune = 0;
            adlins.adl[0] = adl;
            adlins.adl[1] = adl;
            adlins.ms_sound_kon  = 1000;
            adlins.ms_sound_koff = 500;
            adlins.tone  = 0;
            adlins.flags = 0;
            adlins.voice2_fine_tune = 0.0;
        }

        fr.seeku(mus_start, SEEK_SET);
        TrackCount = 1;
        DeltaTicks = (size_t)ticks;
        opl.AdlBank    = ~0u; // Ignore AdlBank number, use dynamic banks instead
        //std::printf("CMF deltas %u ticks %u, basictempo = %u\n", deltas, ticks, basictempo);
        opl.AdlPercussionMode = true;
        opl.m_musicMode = OPL3::MODE_CMF;
        opl.m_volumeScale = OPL3::VOLUME_NATIVE;
    }
    else
    {
        // Try to identify RSXX format
        if(HeaderBuf[0] == 0x7D)
        {
            fr.seek(0x6D, SEEK_SET);
            fr.read(HeaderBuf, 6, 1);
            if(std::memcmp(HeaderBuf, "rsxx}u", 6) == 0)
            {
                is_RSXX = true;
                fr.seek(0x7D, SEEK_SET);
                TrackCount = 1;
                DeltaTicks = 60;
                //opl.CartoonersVolumes = true;
                opl.m_musicMode = OPL3::MODE_RSXX;
                opl.m_volumeScale = OPL3::VOLUME_NATIVE;
            }
        }

        // Try parsing as an IMF file
        if(!is_RSXX)
        {
            do
            {
                uint8_t raw[4];
                size_t end = static_cast<size_t>(HeaderBuf[0]) + 256 * static_cast<size_t>(HeaderBuf[1]);

                if(!end || (end & 3))
                    break;

                size_t backup_pos = fr.tell();
                int64_t sum1 = 0, sum2 = 0;
                fr.seek(2, SEEK_SET);

                for(unsigned n = 0; n < 42; ++n)
                {
                    if(fr.read(raw, 1, 4) != 4)
                        break;
                    int64_t value1 = raw[0];
                    value1 += raw[1] << 8;
                    sum1 += value1;
                    int64_t value2 = raw[2];
                    value2 += raw[3] << 8;
                    sum2 += value2;
                }

                fr.seek(static_cast<long>(backup_pos), SEEK_SET);

                if(sum1 > sum2)
                {
                    is_IMF = true;
                    DeltaTicks = 1;
                }
            } while(false);
        }

        if(!is_IMF && !is_RSXX)
        {
            if(std::memcmp(HeaderBuf, "MThd\0\0\0\6", 8) != 0)
            {
                fr.close();
                errorStringOut = fr._fileName + ": Invalid format, Header signature is unknown!\n";
                return false;
            }

            /*size_t  Fmt =      ReadBEint(HeaderBuf + 8,  2);*/
            TrackCount = (size_t)ReadBEint(HeaderBuf + 10, 2);
            DeltaTicks = (size_t)ReadBEint(HeaderBuf + 12, 2);
        }
    }

    TrackData.clear();
    TrackData.resize(TrackCount, std::vector<uint8_t>());
    InvDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(DeltaTicks));
    if(is_CMF || is_RSXX)
        Tempo         = fraction<uint64_t>(1,            static_cast<uint64_t>(DeltaTicks));
    else
        Tempo         = fraction<uint64_t>(1,            static_cast<uint64_t>(DeltaTicks) * 2);
    static const unsigned char EndTag[4] = {0xFF, 0x2F, 0x00, 0x00};
    size_t totalGotten = 0;

    for(size_t tk = 0; tk < TrackCount; ++tk)
    {
        // Read track header
        size_t TrackLength;

        if(is_IMF)
        {
            //std::fprintf(stderr, "Reading IMF file...\n");
            size_t end = static_cast<size_t>(HeaderBuf[0]) + 256 * static_cast<size_t>(HeaderBuf[1]);
            unsigned IMF_tempo = 1428;
            static const unsigned char imf_tempo[] = {0x0,//Zero delay!
                                                      MidiEvent::T_SPECIAL, MidiEvent::ST_TEMPOCHANGE, 0x4,
                                                      static_cast<uint8_t>(IMF_tempo >> 24),
                                                      static_cast<uint8_t>(IMF_tempo >> 16),
                                                      static_cast<uint8_t>(IMF_tempo >> 8),
                                                      static_cast<uint8_t>(IMF_tempo)
                                                     };
            TrackData[tk].insert(TrackData[tk].end(), imf_tempo, imf_tempo + sizeof(imf_tempo));
            TrackData[tk].push_back(0x00);
            fr.seek(2, SEEK_SET);

            while(fr.tell() < end && !fr.eof())
            {
                uint8_t special_event_buf[5];
                uint8_t raw[4];
                special_event_buf[0] = MidiEvent::T_SPECIAL;
                special_event_buf[1] = MidiEvent::ST_RAWOPL;
                special_event_buf[2] = 0x02;
                if(fr.read(raw, 1, 4) != 4)
                    break;
                special_event_buf[3] = raw[0]; // port index
                special_event_buf[4] = raw[1]; // port value
                uint32_t delay = static_cast<uint32_t>(raw[2]);
                delay += 256 * static_cast<uint32_t>(raw[3]);
                totalGotten += 4;
                //if(special_event_buf[3] <= 8) continue;
                //fprintf(stderr, "Put %02X <- %02X, plus %04X delay\n", special_event_buf[3],special_event_buf[4], delay);
                TrackData[tk].insert(TrackData[tk].end(), special_event_buf, special_event_buf + 5);
                //if(delay>>21) TrackData[tk].push_back( 0x80 | ((delay>>21) & 0x7F ) );
                if(delay >> 14)
                    TrackData[tk].push_back(0x80 | ((delay >> 14) & 0x7F));
                if(delay >> 7)
                    TrackData[tk].push_back(0x80 | ((delay >> 7) & 0x7F));
                TrackData[tk].push_back(((delay >> 0) & 0x7F));
            }

            TrackData[tk].insert(TrackData[tk].end(), EndTag + 0, EndTag + 4);
            //CurrentPosition.track[tk].delay = 0;
            //CurrentPosition.began = true;
            //std::fprintf(stderr, "Done reading IMF file\n");
            opl.NumFourOps = 0; //Don't use 4-operator channels for IMF playing!
            opl.m_musicMode = OPL3::MODE_IMF;
        }
        else
        {
            // Take the rest of the file
            if(is_GMF || is_CMF || is_RSXX)
            {
                size_t pos = fr.tell();
                fr.seek(0, SEEK_END);
                TrackLength = fr.tell() - pos;
                fr.seek(static_cast<long>(pos), SEEK_SET);
            }
            //else if(is_MUS) // Read TrackLength from file position 4
            //{
            //    size_t pos = fr.tell();
            //    fr.seek(4, SEEK_SET);
            //    TrackLength = static_cast<size_t>(fr.getc());
            //    TrackLength += static_cast<size_t>(fr.getc() << 8);
            //    fr.seek(static_cast<long>(pos), SEEK_SET);
            //}
            else
            {
                fsize = fr.read(HeaderBuf, 1, 8);
                if(std::memcmp(HeaderBuf, "MTrk", 4) != 0)
                {
                    fr.close();
                    errorStringOut = fr._fileName + ": Invalid format, MTrk signature is not found!\n";
                    return false;
                }
                TrackLength = (size_t)ReadBEint(HeaderBuf + 4, 4);
            }

            // Read track data
            TrackData[tk].resize(TrackLength);
            fsize = fr.read(&TrackData[tk][0], 1, TrackLength);
            totalGotten += fsize;

            if(is_GMF/*|| is_MUS*/) // Note: CMF does include the track end tag.
                TrackData[tk].insert(TrackData[tk].end(), EndTag + 0, EndTag + 4);
            if(is_RSXX)//Finalize raw track data with a zero
                TrackData[tk].push_back(0);

            //bool ok = false;
            //// Read next event time
            //uint64_t tkDelay = ReadVarLenEx(tk, ok);
            //if(ok)
            //    CurrentPosition.track[tk].delay = tkDelay;
            //else
            //{
            //    std::stringstream msg;
            //    msg << fr._fileName << ": invalid variable length in the track " << tk << "! (error code " << tkDelay << ")";
            //    ADLMIDI_ErrorString = msg.str();
            //    return false;
            //}
        }
    }

    for(size_t tk = 0; tk < TrackCount; ++tk)
        totalGotten += TrackData[tk].size();

    if(totalGotten == 0)
    {
        errorStringOut = fr._fileName + ": Empty track data";
        return false;
    }

    //Build new MIDI events table
    if(!buildTrackData())
    {
        errorStringOut = fr._fileName + ": MIDI data parsing error has occouped!\n" + errorString;
        return false;
    }

    opl.Reset(m_setup.emulator, m_setup.PCM_RATE, this); // Reset OPL3 chip
    //opl.Reset(); // ...twice (just in case someone misprogrammed OPL3 previously)
    ch.clear();
    ch.resize(opl.NumChannels);
    return true;
}
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER
