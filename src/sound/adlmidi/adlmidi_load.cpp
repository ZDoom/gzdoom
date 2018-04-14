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

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
#   ifndef ADLMIDI_DISABLE_MUS_SUPPORT
#       include "adlmidi_mus2mid.h"
#   endif//MUS
#   ifndef ADLMIDI_DISABLE_XMI_SUPPORT
#       include "adlmidi_xmi2mid.h"
#   endif//XMI
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

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



/* WOPL-needed misc functions */
static uint16_t toUint16LE(const uint8_t *arr)
{
    uint16_t num = arr[0];
    num |= ((arr[1] << 8) & 0xFF00);
    return num;
}

static uint16_t toUint16BE(const uint8_t *arr)
{
    uint16_t num = arr[1];
    num |= ((arr[0] << 8) & 0xFF00);
    return num;
}

static int16_t toSint16BE(const uint8_t *arr)
{
    int16_t num = *reinterpret_cast<const int8_t *>(&arr[0]);
    num *= 1 << 8;
    num |= arr[1];
    return num;
}

static const char       *wopl3_magic = "WOPL3-BANK\0";
static const uint16_t   wopl_latest_version = 3;

#define WOPL_INST_SIZE_V2 62
#define WOPL_INST_SIZE_V3 66

enum WOPL_InstrumentFlags
{
    WOPL_Flags_NONE      = 0,
    WOPL_Flag_Enable4OP  = 0x01,
    WOPL_Flag_Pseudo4OP  = 0x02,
    WOPL_Flag_NoSound    = 0x04,
};

struct WOPL_Inst
{
    bool fourOps;
    char padding[7];
    struct adlinsdata adlins;
    struct adldata    op[2];
    uint16_t ms_sound_kon;
    uint16_t ms_sound_koff;
};

static bool readInstrument(MIDIplay::fileReader &file, WOPL_Inst &ins, uint16_t &version, bool isPercussion = false)
{
    uint8_t idata[WOPL_INST_SIZE_V3];
    if(version >= 3)
    {
        if(file.read(idata, 1, WOPL_INST_SIZE_V3) != WOPL_INST_SIZE_V3)
            return false;
    }
    else
    {
        if(file.read(idata, 1, WOPL_INST_SIZE_V2) != WOPL_INST_SIZE_V2)
            return false;
    }

    //strncpy(ins.name, char_p(idata), 32);
    ins.op[0].finetune = (int8_t)toSint16BE(idata + 32);
    ins.op[1].finetune = (int8_t)toSint16BE(idata + 34);
    //ins.velocity_offset = int8_t(idata[36]);

    ins.adlins.voice2_fine_tune = 0.0;
    int8_t voice2_fine_tune = int8_t(idata[37]);
    if(voice2_fine_tune != 0)
    {
        if(voice2_fine_tune == 1)
            ins.adlins.voice2_fine_tune = 0.000025;
        else if(voice2_fine_tune == -1)
            ins.adlins.voice2_fine_tune = -0.000025;
        else
            ins.adlins.voice2_fine_tune = ((voice2_fine_tune * 15.625) / 1000.0);
    }

    ins.adlins.tone = isPercussion ? idata[38] : 0;

    uint8_t flags       = idata[39];
    ins.adlins.flags = (flags & WOPL_Flag_Enable4OP) && (flags & WOPL_Flag_Pseudo4OP) ? adlinsdata::Flag_Pseudo4op : 0;
    ins.adlins.flags|= (flags & WOPL_Flag_NoSound) ? adlinsdata::Flag_NoSound : 0;
    ins.fourOps      = (flags & WOPL_Flag_Enable4OP) || (flags & WOPL_Flag_Pseudo4OP);

    ins.op[0].feedconn = (idata[40]);
    ins.op[1].feedconn = (idata[41]);

    for(size_t op = 0, slt = 0; op < 4; op++, slt++)
    {
        size_t off = 42 + size_t(op) * 5;
        //        ins.setAVEKM(op,    idata[off + 0]);//AVEKM
        //        ins.setAtDec(op,    idata[off + 2]);//AtDec
        //        ins.setSusRel(op,   idata[off + 3]);//SusRel
        //        ins.setWaveForm(op, idata[off + 4]);//WaveForm
        //        ins.setKSLL(op,     idata[off + 1]);//KSLL
        ins.op[slt].carrier_E862 =
            ((static_cast<uint32_t>(idata[off + 4]) << 24) & 0xFF000000)    //WaveForm
            | ((static_cast<uint32_t>(idata[off + 3]) << 16) & 0x00FF0000)  //SusRel
            | ((static_cast<uint32_t>(idata[off + 2]) << 8) & 0x0000FF00)   //AtDec
            | ((static_cast<uint32_t>(idata[off + 0]) << 0) & 0x000000FF);  //AVEKM
        ins.op[slt].carrier_40 = idata[off + 1];//KSLL

        op++;
        off = 42 + size_t(op) * 5;
        ins.op[slt].modulator_E862 =
            ((static_cast<uint32_t>(idata[off + 4]) << 24) & 0xFF000000)    //WaveForm
            | ((static_cast<uint32_t>(idata[off + 3]) << 16) & 0x00FF0000)  //SusRel
            | ((static_cast<uint32_t>(idata[off + 2]) << 8) & 0x0000FF00)   //AtDec
            | ((static_cast<uint32_t>(idata[off + 0]) << 0) & 0x000000FF);  //AVEKM
        ins.op[slt].modulator_40 = idata[off + 1];//KSLL
    }

    if(version >= 3)
    {
        ins.ms_sound_kon  = toUint16BE(idata + 62);
        ins.ms_sound_koff = toUint16BE(idata + 64);
    }
    else
    {
        ins.ms_sound_kon = 1000;
        ins.ms_sound_koff = 500;
    }

    return true;
}

bool MIDIplay::LoadBank(MIDIplay::fileReader &fr)
{
    size_t  fsize;
    ADL_UNUSED(fsize);
    if(!fr.isValid())
    {
        errorStringOut = "Custom bank: Invalid data stream!";
        return false;
    }

    char magic[32];
    std::memset(magic, 0, 32);

    uint16_t version = 0;

    uint16_t count_melodic_banks     = 1;
    uint16_t count_percusive_banks   = 1;

    if(fr.read(magic, 1, 11) != 11)
    {
        errorStringOut = "Custom bank: Can't read magic number!";
        return false;
    }

    if(std::strncmp(magic, wopl3_magic, 11) != 0)
    {
        errorStringOut = "Custom bank: Invalid magic number!";
        return false;
    }

    uint8_t version_raw[2];
    if(fr.read(version_raw, 1, 2) != 2)
    {
        errorStringOut = "Custom bank: Can't read version!";
        return false;
    }

    version = toUint16LE(version_raw);
    if(version > wopl_latest_version)
    {
        errorStringOut = "Custom bank: Unsupported WOPL version!";
        return false;
    }

    uint8_t head[6];
    std::memset(head, 0, 6);
    if(fr.read(head, 1, 6) != 6)
    {
        errorStringOut = "Custom bank: Can't read header!";
        return false;
    }

    count_melodic_banks     = toUint16BE(head);
    count_percusive_banks   = toUint16BE(head + 2);

    if((count_melodic_banks < 1) || (count_percusive_banks < 1))
    {
        errorStringOut = "Custom bank: Too few banks in this file!";
        return false;
    }

    /*UNUSED YET*/
    bool default_deep_vibrato   = ((head[4]>>0) & 0x01);
    bool default_deep_tremolo   = ((head[4]>>1) & 0x01);

    //5'th byte reserved for Deep-Tremolo and Deep-Vibrato flags
    m_setup.HighTremoloMode = default_deep_tremolo;
    m_setup.HighVibratoMode = default_deep_vibrato;
    //6'th byte reserved for ADLMIDI's default volume model
    m_setup.VolumeModel = (int)head[5];

    opl.dynamic_melodic_banks.clear();
    opl.dynamic_percussion_banks.clear();

    opl.setEmbeddedBank(m_setup.AdlBank);

    if(version >= 2)//Read bank meta-entries
    {
        for(uint16_t i = 0; i < count_melodic_banks; i++)
        {
            uint8_t bank_meta[34];
            if(fr.read(bank_meta, 1, 34) != 34)
            {
                errorStringOut = "Custom bank: Fail to read melodic bank meta-data!";
                return false;
            }
            uint16_t bank = uint16_t(bank_meta[33]) * 256 + uint16_t(bank_meta[32]);
            size_t offset = opl.dynamic_melodic_banks.size();
            opl.dynamic_melodic_banks[bank] = offset;
            //strncpy(bankMeta.name, char_p(bank_meta), 32);
        }

        for(uint16_t i = 0; i < count_percusive_banks; i++)
        {
            uint8_t bank_meta[34];
            if(fr.read(bank_meta, 1, 34) != 34)
            {
                errorStringOut = "Custom bank: Fail to read percussion bank meta-data!";
                return false;
            }
            uint16_t bank = uint16_t(bank_meta[33]) * 256 + uint16_t(bank_meta[32]);
            size_t offset = opl.dynamic_percussion_banks.size();
            opl.dynamic_percussion_banks[bank] = offset;
            //strncpy(bankMeta.name, char_p(bank_meta), 32);
        }
    }

    uint16_t total = 128 * count_melodic_banks;
    bool readPercussion = false;

tryAgain:
    for(uint16_t i = 0; i < total; i++)
    {
        WOPL_Inst ins;
        std::memset(&ins, 0, sizeof(WOPL_Inst));
        if(!readInstrument(fr, ins, version, readPercussion))
        {
            opl.setEmbeddedBank(m_setup.AdlBank);
            errorStringOut = "Custom bank: Fail to read instrument!";
            return false;
        }
        ins.adlins.ms_sound_kon  = ins.ms_sound_kon;
        ins.adlins.ms_sound_koff = ins.ms_sound_koff;
        ins.adlins.adlno1 = static_cast<uint16_t>(opl.dynamic_instruments.size() | opl.DynamicInstrumentTag);
        opl.dynamic_instruments.push_back(ins.op[0]);
        ins.adlins.adlno2 = ins.adlins.adlno1;
        if(ins.fourOps)
        {
            ins.adlins.adlno2 = static_cast<uint16_t>(opl.dynamic_instruments.size() | opl.DynamicInstrumentTag);
            opl.dynamic_instruments.push_back(ins.op[1]);
        }
        opl.dynamic_metainstruments.push_back(ins.adlins);
    }

    if(!readPercussion)
    {
        total = 128 * count_percusive_banks;
        readPercussion = true;
        goto tryAgain;
    }

    opl.AdlBank = ~0u; // Use dynamic banks!
    //Percussion offset is count of instruments multipled to count of melodic banks
    opl.dynamic_percussion_offset = 128 * count_melodic_banks;

    applySetup();

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
    if((opl.AdlBank != ~0u) || (opl.dynamic_metainstruments.size() < 256))
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
        opl.dynamic_instruments.clear();
        opl.dynamic_metainstruments.clear();
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
            unsigned char InsData[16];
            fr.read(InsData, 1, 16);
            /*std::printf("Ins %3u: %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X\n",
                        i, InsData[0],InsData[1],InsData[2],InsData[3], InsData[4],InsData[5],InsData[6],InsData[7],
                           InsData[8],InsData[9],InsData[10],InsData[11], InsData[12],InsData[13],InsData[14],InsData[15]);*/
            struct adldata    adl;
            struct adlinsdata adlins;
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
            adlins.adlno1 = static_cast<uint16_t>(opl.dynamic_instruments.size() | opl.DynamicInstrumentTag);
            adlins.adlno2 = adlins.adlno1;
            adlins.ms_sound_kon  = 1000;
            adlins.ms_sound_koff = 500;
            adlins.tone  = 0;
            adlins.flags = 0;
            adlins.voice2_fine_tune = 0.0;
            opl.dynamic_metainstruments.push_back(adlins);
            opl.dynamic_instruments.push_back(adl);
        }

        fr.seeku(mus_start, SEEK_SET);
        TrackCount = 1;
        DeltaTicks = (size_t)ticks;
        opl.AdlBank    = ~0u; // Ignore AdlBank number, use dynamic banks instead
        //std::printf("CMF deltas %u ticks %u, basictempo = %u\n", deltas, ticks, basictempo);
        opl.LogarithmicVolumes = true;
        opl.AdlPercussionMode = true;
        opl.m_musicMode = OPL3::MODE_CMF;
        opl.m_volumeScale = OPL3::VOLUME_CMF;
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
                opl.LogarithmicVolumes = true;
                //opl.CartoonersVolumes = true;
                opl.m_musicMode = OPL3::MODE_RSXX;
                opl.m_volumeScale = OPL3::VOLUME_CMF;
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
    //CurrentPosition.track.clear();
    //CurrentPosition.track.resize(TrackCount);
    InvDeltaTicks = fraction<uint64_t>(1, 1000000l * static_cast<uint64_t>(DeltaTicks));
    //Tempo       = 1000000l * InvDeltaTicks;
    Tempo         = fraction<uint64_t>(1,            static_cast<uint64_t>(DeltaTicks));
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

    //Build new MIDI events table (ALPHA!!!)
    if(!buildTrackData())
    {
        errorStringOut = fr._fileName + ": MIDI data parsing error has occouped!\n" + errorString;
        return false;
    }

    opl.Reset(m_setup.PCM_RATE); // Reset AdLib
    //opl.Reset(); // ...twice (just in case someone misprogrammed OPL3 previously)
    ch.clear();
    ch.resize(opl.NumChannels);
    return true;
}
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER
