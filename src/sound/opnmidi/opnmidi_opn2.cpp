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

static const uint8_t NoteChannels[6] = { 0, 1, 2, 4, 5, 6 };

static inline void getOpnChannel(uint32_t   in_channel,
                                 unsigned   &out_card,
                                 uint8_t    &out_port,
                                 uint8_t    &out_ch)
{
    out_card    = in_channel / 6;
    uint8_t ch4 = in_channel % 6;
    out_port = ((ch4 < 3) ? 0 : 1);
    out_ch = ch4 % 3;
}

const opnInstMeta &OPN2::GetAdlMetaIns(size_t n)
{
    return dynamic_metainstruments[n];
}

size_t OPN2::GetAdlMetaNumber(size_t midiins)
{
    return midiins;
}

static const opnInstData opn2_emptyInstrument = {
    {
        {{0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0}},
        {{0, 0, 0, 0, 0, 0, 0}}
    },
    0, 0, 0
};

const opnInstData &OPN2::GetAdlIns(size_t insno)
{
    if(insno >= dynamic_instruments.size())
        return opn2_emptyInstrument;
    return dynamic_instruments[insno];
}

OPN2::OPN2() :
    regLFO(0),
    dynamic_percussion_offset(128),
    DynamicInstrumentTag(0x8000u),
    DynamicMetaInstrumentTag(0x4000000u),
    NumCards(1),
    LogarithmicVolumes(false),
    m_musicMode(MODE_MIDI),
    m_volumeScale(VOLUME_Generic)
{}

OPN2::~OPN2()
{
    ClearChips();
}

void OPN2::PokeO(size_t card, uint8_t port, uint8_t index, uint8_t value)
{
    #ifdef OPNMIDI_USE_LEGACY_EMULATOR
    if(port == 1)
        cardsOP2[card]->write1(index, value);
    else
        cardsOP2[card]->write0(index, value);
    #else
    OPN2_WriteBuffered(cardsOP2[card], 0 + (port) * 2, index);
    OPN2_WriteBuffered(cardsOP2[card], 1 + (port) * 2, value);
    #endif
}

void OPN2::NoteOff(size_t c)
{
    unsigned    card;
    uint8_t     port, cc;
    uint8_t     ch4 = c % 6;
    getOpnChannel(uint16_t(c), card, port, cc);
    PokeO(card, 0, 0x28, NoteChannels[ch4]);
}

void OPN2::NoteOn(unsigned c, double hertz) // Hertz range: 0..131071
{
    unsigned    card;
    uint8_t     port, cc;
    uint8_t     ch4 = c % 6;
    getOpnChannel(uint16_t(c), card, port, cc);

    uint16_t x2 = 0x0000;

    if(hertz < 0 || hertz > 262143) // Avoid infinite loop
        return;

    while(hertz >= 2047.5)
    {
        hertz /= 2.0;    // Calculate octave
        x2 += 0x800;
    }

    x2 += static_cast<uint32_t>(hertz + 0.5);
    PokeO(card, port, 0xA4 + cc, (x2>>8) & 0xFF);//Set frequency and octave
    PokeO(card, port, 0xA0 + cc,  x2 & 0xFF);
    PokeO(card, 0, 0x28, 0xF0 + NoteChannels[ch4]);
    pit[c] = static_cast<uint8_t>(x2 >> 8);
}

void OPN2::Touch_Real(unsigned c, unsigned volume, uint8_t brightness)
{
    if(volume > 127) volume = 127;

    unsigned    card;
    uint8_t     port, cc;
    getOpnChannel(c, card, port, cc);

    size_t i = ins[c];
    const opnInstData &adli = GetAdlIns(i);

    uint8_t op_vol[4] =
    {
        adli.OPS[OPERATOR1].data[1],
        adli.OPS[OPERATOR2].data[1],
        adli.OPS[OPERATOR3].data[1],
        adli.OPS[OPERATOR4].data[1],
    };

    bool alg_do[8][4] =
    {
        /*
         * Yeah, Operator 2 and 3 are seems swapped
         * which we can see in the algorithm 4
         */
        //OP1   OP3   OP2    OP4
        //30    34    38     3C
        {false,false,false,true},//Algorithm #0:  W = 1 * 2 * 3 * 4
        {false,false,false,true},//Algorithm #1:  W = (1 + 2) * 3 * 4
        {false,false,false,true},//Algorithm #2:  W = (1 + (2 * 3)) * 4
        {false,false,false,true},//Algorithm #3:  W = ((1 * 2) + 3) * 4
        {false,false,true, true},//Algorithm #4:  W = (1 * 2) + (3 * 4)
        {false,true ,true ,true},//Algorithm #5:  W = (1 * (2 + 3 + 4)
        {false,true ,true ,true},//Algorithm #6:  W = (1 * 2) + 3 + 4
        {true ,true ,true ,true},//Algorithm #7:  W = 1 + 2 + 3 + 4
    };

    uint8_t alg = adli.fbalg & 0x07;
    for(uint8_t op = 0; op < 4; op++)
    {
        bool do_op = alg_do[alg][op] || ScaleModulators;
        uint8_t x = op_vol[op];
        uint8_t vol_res = do_op ? uint8_t(127 - (volume * (127 - (x&127)))/127) : x;
        if(brightness != 127)
        {
            brightness = static_cast<uint8_t>(::round(127.0 * ::sqrt((static_cast<double>(brightness)) * (1.0 / 127.0))));
            if(!do_op)
                vol_res = uint8_t(127 - (brightness * (127 - (uint32_t(vol_res) & 127))) / 127);
        }
        PokeO(card, port, 0x40 + cc + (4 * op), vol_res);
    }
    // Correct formula (ST3, AdPlug):
    //   63-((63-(instrvol))/63)*chanvol
    // Reduces to (tested identical):
    //   63 - chanvol + chanvol*instrvol/63
    // Also (slower, floats):
    //   63 + chanvol * (instrvol / 63.0 - 1)
}

void OPN2::Patch(uint16_t c, size_t i)
{
    unsigned    card;
    uint8_t     port, cc;
    getOpnChannel(uint16_t(c), card, port, cc);
    ins[c] = i;
    const opnInstData &adli = GetAdlIns(i);
    #if 1 //Reg1-Op1, Reg1-Op2, Reg1-Op3, Reg1-Op4,....
    for(uint8_t d = 0; d < 7; d++)
    {
        for(uint8_t op = 0; op < 4; op++)
            PokeO(card, port, 0x30 + (0x10 * d) + (op * 4) + cc, adli.OPS[op].data[d]);
    }
    #else //Reg1-Op1, Reg2-Op1, Reg3-Op1, Reg4-Op1,....
    for(uint8_t op = 0; op < 4; op++)
    {
        PokeO(card, port, 0x30 + (op * 4) + cc, adli.OPS[op].data[0]);
        PokeO(card, port, 0x40 + (op * 4) + cc, adli.OPS[op].data[1]);
        PokeO(card, port, 0x50 + (op * 4) + cc, adli.OPS[op].data[2]);
        PokeO(card, port, 0x60 + (op * 4) + cc, adli.OPS[op].data[3]);
        PokeO(card, port, 0x70 + (op * 4) + cc, adli.OPS[op].data[4]);
        PokeO(card, port, 0x80 + (op * 4) + cc, adli.OPS[op].data[5]);
        PokeO(card, port, 0x90 + (op * 4) + cc, adli.OPS[op].data[6]);
    }
    #endif

    PokeO(card, port, 0xB0 + cc, adli.fbalg);//Feedback/Algorithm
    regBD[c] = (regBD[c] & 0xC0) | (adli.lfosens & 0x3F);
    PokeO(card, port, 0xB4 + cc, regBD[c]);//Panorame and LFO bits
}

void OPN2::Pan(unsigned c, unsigned value)
{
    unsigned    card;
    uint8_t     port, cc;
    getOpnChannel(uint16_t(c), card, port, cc);
    const opnInstData &adli = GetAdlIns(ins[c]);
    uint8_t val = (value & 0xC0) | (adli.lfosens & 0x3F);
    regBD[c] = val;
    PokeO(card, port, 0xB4 + cc, val);
}

void OPN2::Silence() // Silence all OPL channels.
{
    for(unsigned c = 0; c < NumChannels; ++c)
    {
        NoteOff(c);
        Touch_Real(c, 0);
    }
}

void OPN2::ChangeVolumeRangesModel(OPNMIDI_VolumeModels volumeModel)
{
    switch(volumeModel)
    {
    case OPNMIDI_VolumeModel_AUTO://Do nothing until restart playing
        break;

    case OPNMIDI_VolumeModel_Generic:
        m_volumeScale = OPN2::VOLUME_Generic;
        break;

    case OPNMIDI_VolumeModel_CMF:
        LogarithmicVolumes = true;
        m_volumeScale = OPN2::VOLUME_CMF;
        break;

    case OPNMIDI_VolumeModel_DMX:
        m_volumeScale = OPN2::VOLUME_DMX;
        break;

    case OPNMIDI_VolumeModel_APOGEE:
        m_volumeScale = OPN2::VOLUME_APOGEE;
        break;

    case OPNMIDI_VolumeModel_9X:
        m_volumeScale = OPN2::VOLUME_9X;
        break;
    }
}

void OPN2::ClearChips()
{
    for(size_t i = 0; i < cardsOP2.size(); i++)
        delete cardsOP2[i];
    cardsOP2.clear();
}

void OPN2::Reset(unsigned long PCM_RATE)
{
    ClearChips();
    ins.clear();
    pit.clear();
    regBD.clear();
    cardsOP2.resize(NumCards, NULL);

    #ifndef OPNMIDI_USE_LEGACY_EMULATOR
    OPN2_SetChipType(ym3438_type_asic);
    #endif
    for(size_t i = 0; i < cardsOP2.size(); i++)
    {
    #ifdef OPNMIDI_USE_LEGACY_EMULATOR
        cardsOP2[i] = new OPNMIDI_Ym2612_Emu();
        cardsOP2[i]->set_rate(PCM_RATE, 7670454.0);
    #else
        cardsOP2[i] = new ym3438_t;
        std::memset(cardsOP2[i], 0, sizeof(ym3438_t));
        OPN2_Reset(cardsOP2[i], (Bit32u)PCM_RATE, 7670454);
    #endif
    }
    NumChannels = NumCards * 6;
    ins.resize(NumChannels,   189);
    pit.resize(NumChannels,   0);
    regBD.resize(NumChannels,    0);

    for(unsigned card = 0; card < NumCards; ++card)
    {
        PokeO(card, 0, 0x22, regLFO);//push current LFO state
        PokeO(card, 0, 0x27, 0x00);  //set Channel 3 normal mode
        PokeO(card, 0, 0x2B, 0x00);  //Disable DAC
        //Shut up all channels
        PokeO(card, 0, 0x28, 0x00 ); //Note Off 0 channel
        PokeO(card, 0, 0x28, 0x01 ); //Note Off 1 channel
        PokeO(card, 0, 0x28, 0x02 ); //Note Off 2 channel
        PokeO(card, 0, 0x28, 0x04 ); //Note Off 3 channel
        PokeO(card, 0, 0x28, 0x05 ); //Note Off 4 channel
        PokeO(card, 0, 0x28, 0x06 ); //Note Off 5 channel
    }

    Silence();
}
