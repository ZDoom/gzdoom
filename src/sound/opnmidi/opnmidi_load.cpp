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

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
#   ifndef OPNMIDI_DISABLE_MUS_SUPPORT
#       include "opnmidi_mus2mid.h"
#   endif
#   ifndef OPNMIDI_DISABLE_XMI_SUPPORT
#       include "opnmidi_xmi2mid.h"
#   endif
#endif //OPNMIDI_DISABLE_MIDI_SEQUENCER

uint64_t OPNMIDIplay::ReadBEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(buffer);

    for(unsigned n = 0; n < nbytes; ++n)
        result = (result << 8) + data[n];

    return result;
}

uint64_t OPNMIDIplay::ReadLEint(const void *buffer, size_t nbytes)
{
    uint64_t result = 0;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(buffer);

    for(unsigned n = 0; n < nbytes; ++n)
        result = result + static_cast<uint64_t>(data[n] << (n * 8));

    return result;
}

//uint64_t OPNMIDIplay::ReadVarLenEx(size_t tk, bool &ok)
//{
//    uint64_t result = 0;
//    ok = false;

//    for(;;)
//    {
//        if(tk >= TrackData.size())
//            return 1;

//        if(tk >= CurrentPosition.track.size())
//            return 2;

//        size_t ptr = CurrentPosition.track[tk].ptr;

//        if(ptr >= TrackData[tk].size())
//            return 3;

//        unsigned char byte = TrackData[tk][CurrentPosition.track[tk].ptr++];
//        result = (result << 7) + (byte & 0x7F);

//        if(!(byte & 0x80)) break;
//    }

//    ok = true;
//    return result;
//}

bool OPNMIDIplay::LoadBank(const std::string &filename)
{
    fileReader file;
    file.openFile(filename.c_str());
    return LoadBank(file);
}

bool OPNMIDIplay::LoadBank(const void *data, size_t size)
{
    fileReader file;
    file.openData(data, (size_t)size);
    return LoadBank(file);
}

size_t readU16BE(OPNMIDIplay::fileReader &fr, uint16_t &out)
{
    uint8_t arr[2];
    size_t ret = fr.read(arr, 1, 2);
    out = arr[1];
    out |= ((arr[0] << 8) & 0xFF00);
    return ret;
}

size_t readS16BE(OPNMIDIplay::fileReader &fr, int16_t &out)
{
    uint8_t arr[2];
    size_t ret = fr.read(arr, 1, 2);
    out = *reinterpret_cast<signed char *>(&arr[0]);
    out *= 1 << 8;
    out |= arr[1];
    return ret;
}

int16_t toSint16BE(uint8_t *arr)
{
    int16_t num = *reinterpret_cast<const int8_t *>(&arr[0]);
    num *= 1 << 8;
    num |= arr[1];
    return num;
}

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


static const char *wopn2_magic1 = "WOPN2-BANK\0";
static const char *wopn2_magic2 = "WOPN2-B2NK\0";

#define WOPL_INST_SIZE_V1 65
#define WOPL_INST_SIZE_V2 69

static const uint16_t latest_version = 2;

bool OPNMIDIplay::LoadBank(OPNMIDIplay::fileReader &fr)
{
    size_t  fsize;
    ADL_UNUSED(fsize);
    if(!fr.isValid())
    {
        errorStringOut = "Can't load bank file: Invalid data stream!";
        return false;
    }

    char magic[32];
    std::memset(magic, 0, 32);
    uint16_t version = 1;

    uint16_t count_melodic_banks     = 1;
    uint16_t count_percusive_banks   = 1;

    if(fr.read(magic, 1, 11) != 11)
    {
        errorStringOut = "Can't load bank file: Can't read magic number!";
        return false;
    }

    bool is1 = std::strncmp(magic, wopn2_magic1, 11) == 0;
    bool is2 = std::strncmp(magic, wopn2_magic2, 11) == 0;

    if(!is1 && !is2)
    {
        errorStringOut = "Can't load bank file: Invalid magic number!";
        return false;
    }

    if(is2)
    {
        uint8_t ver[2];
        if(fr.read(ver, 1, 2) != 2)
        {
            errorStringOut = "Can't load bank file: Can't read version number!";
            return false;
        }
        version = toUint16LE(ver);
        if(version < 2 || version > latest_version)
        {
            errorStringOut = "Can't load bank file: unsupported WOPN version!";
            return false;
        }
    }

    opn.dynamic_instruments.clear();
    opn.dynamic_metainstruments.clear();
    if((readU16BE(fr, count_melodic_banks) != 2) || (readU16BE(fr, count_percusive_banks) != 2))
    {
        errorStringOut = "Can't load bank file: Can't read count of banks!";
        return false;
    }

    if((count_melodic_banks < 1) || (count_percusive_banks < 1))
    {
        errorStringOut = "Custom bank: Too few banks in this file!";
        return false;
    }

    if(fr.read(&opn.regLFO, 1, 1) != 1)
    {
        errorStringOut = "Can't load bank file: Can't read LFO registry state!";
        return false;
    }

    opn.dynamic_melodic_banks.clear();
    opn.dynamic_percussion_banks.clear();

    if(version >= 2)//Read bank meta-entries
    {
        for(uint16_t i = 0; i < count_melodic_banks; i++)
        {
            uint8_t bank_meta[34];
            if(fr.read(bank_meta, 1, 34) != 34)
            {
                opn.dynamic_melodic_banks.clear();
                errorStringOut = "Custom bank: Fail to read melodic bank meta-data!";
                return false;
            }
            uint16_t bank = uint16_t(bank_meta[33]) * 256 + uint16_t(bank_meta[32]);
            size_t offset = opn.dynamic_melodic_banks.size();
            opn.dynamic_melodic_banks[bank] = offset;
            //strncpy(bankMeta.name, char_p(bank_meta), 32);
        }

        for(uint16_t i = 0; i < count_percusive_banks; i++)
        {
            uint8_t bank_meta[34];
            if(fr.read(bank_meta, 1, 34) != 34)
            {
                opn.dynamic_melodic_banks.clear();
                opn.dynamic_percussion_banks.clear();
                errorStringOut = "Custom bank: Fail to read percussion bank meta-data!";
                return false;
            }
            uint16_t bank = uint16_t(bank_meta[33]) * 256 + uint16_t(bank_meta[32]);
            size_t offset = opn.dynamic_percussion_banks.size();
            opn.dynamic_percussion_banks[bank] = offset;
            //strncpy(bankMeta.name, char_p(bank_meta), 32);
        }
    }

    opn.dynamic_percussion_offset = count_melodic_banks * 128;
    uint16_t total = 128 * count_melodic_banks + 128 * count_percusive_banks;

    for(uint16_t i = 0; i < total; i++)
    {
        opnInstData data;
        opnInstMeta meta;
        uint8_t idata[WOPL_INST_SIZE_V2];

        size_t readSize = version >= 2 ? WOPL_INST_SIZE_V2 : WOPL_INST_SIZE_V1;
        if(fr.read(idata, 1, readSize) != readSize)
        {
            opn.dynamic_instruments.clear();
            opn.dynamic_metainstruments.clear();
            opn.dynamic_melodic_banks.clear();
            opn.dynamic_percussion_banks.clear();
            errorStringOut = "Can't load bank file: Failed to read instrument data";
            return false;
        }
        data.finetune = toSint16BE(idata + 32);
        //Percussion instrument note number or a "fixed note sound"
        meta.tone  = idata[34];
        data.fbalg = idata[35];
        data.lfosens = idata[36];
        for(size_t op = 0; op < 4; op++)
        {
            size_t off = 37 + op * 7;
            std::memcpy(data.OPS[op].data, idata + off, 7);
        }

        meta.flags = 0;
        if(version >= 2)
        {
            meta.ms_sound_kon   = toUint16BE(idata + 65);
            meta.ms_sound_koff  = toUint16BE(idata + 67);
            if((meta.ms_sound_kon == 0) && (meta.ms_sound_koff == 0))
                meta.flags |= opnInstMeta::Flag_NoSound;
        }
        else
        {
            meta.ms_sound_kon   = 1000;
            meta.ms_sound_koff  = 500;
        }

        meta.opnno1 = uint16_t(opn.dynamic_instruments.size());
        meta.opnno2 = uint16_t(opn.dynamic_instruments.size());

        /* Junk, delete later */
        meta.fine_tune      = 0.0;
        /* Junk, delete later */

        opn.dynamic_instruments.push_back(data);
        opn.dynamic_metainstruments.push_back(meta);
    }

    applySetup();

    return true;
}

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
bool OPNMIDIplay::LoadMIDI(const std::string &filename)
{
    fileReader file;
    file.openFile(filename.c_str());
    if(!LoadMIDI(file))
        return false;
    return true;
}

bool OPNMIDIplay::LoadMIDI(const void *data, size_t size)
{
    fileReader file;
    file.openData(data, size);
    return LoadMIDI(file);
}

bool OPNMIDIplay::LoadMIDI(OPNMIDIplay::fileReader &fr)
{
    size_t  fsize;
    ADL_UNUSED(fsize);
    //! Temp buffer for conversion
    AdlMIDI_CPtr<uint8_t> cvt_buf;
    errorString.clear();

    if(opn.dynamic_instruments.empty())
    {
        errorStringOut = "Bank is not set! Please load any instruments bank by using of adl_openBankFile() or adl_openBankData() functions!";
        return false;
    }

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

    bool is_GMF     = false; // GMD/MUS files (ScummVM)
    bool is_RSXX    = false; // RSXX, such as Cartooners

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

    #ifndef OPNMIDI_DISABLE_MUS_SUPPORT
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
        int m2mret = OpnMidi_mus2midi(mus, static_cast<uint32_t>(mus_len),
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
    #endif //OPNMIDI_DISABLE_MUS_SUPPORT

    #ifndef OPNMIDI_DISABLE_XMI_SUPPORT
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
        int m2mret = OpnMidi_xmi2midi(mus, static_cast<uint32_t>(mus_len),
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
    #endif //OPNMIDI_DISABLE_XMI_SUPPORT

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
                opn.LogarithmicVolumes = true;
                //opl.CartoonersVolumes = true;
                opn.m_musicMode = OPN2::MODE_RSXX;
                opn.m_volumeScale = OPN2::VOLUME_CMF;
            }
        }

        if(!is_RSXX)
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
        {
            if(is_GMF || is_RSXX) // Take the rest of the file
            {
                size_t pos = fr.tell();
                fr.seek(0, SEEK_END);
                TrackLength = fr.tell() - pos;
                fr.seek(static_cast<long>(pos), SEEK_SET);
            }
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
            //    OPN2MIDI_ErrorString = msg.str();
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

    opn.Reset(m_setup.PCM_RATE); // Reset AdLib
    ch.clear();
    ch.resize(opn.NumChannels);
    return true;
}
#endif //OPNMIDI_DISABLE_MIDI_SEQUENCER
