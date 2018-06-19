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

#if defined(OPNMIDI_DISABLE_NUKED_EMULATOR) && defined(OPNMIDI_DISABLE_MAME_EMULATOR) && \
    defined(OPNMIDI_DISABLE_GENS_EMULATOR) && defined(OPNMIDI_DISABLE_GX_EMULATOR)
#error "No emulators enabled. You must enable at least one emulator to use this library!"
#endif

// Nuked OPN2 emulator, Most accurate, but requires the powerful CPU
#ifndef OPNMIDI_DISABLE_NUKED_EMULATOR
#include "chips/nuked_opn2.h"
#endif

// MAME YM2612 emulator, Well-accurate and fast
#ifndef OPNMIDI_DISABLE_MAME_EMULATOR
#include "chips/mame_opn2.h"
#endif

// GENS 2.10 emulator, very outdated and inaccurate, but gives the best performance
#ifndef OPNMIDI_DISABLE_GENS_EMULATOR
#include "chips/gens_opn2.h"
#endif

// Genesis Plus GX emulator, Variant of MAME with enhancements
#ifndef OPNMIDI_DISABLE_GX_EMULATOR
#include "chips/gx_opn2.h"
#endif


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

void OPN2::cleanInstrumentBanks()
{
    dynamic_banks.clear();
}

static opnInstMeta2 makeEmptyInstrument()
{
    opnInstMeta2 ins;
    memset(&ins, 0, sizeof(opnInstMeta2));
    ins.flags = opnInstMeta::Flag_NoSound;
    return ins;
}

const opnInstMeta2 OPN2::emptyInstrument = makeEmptyInstrument();

OPN2::OPN2() :
    regLFO(0),
    NumCards(1),
    m_musicMode(MODE_MIDI),
    m_volumeScale(VOLUME_Generic)
{
    // Initialize blank instruments banks
    cleanInstrumentBanks();
}

OPN2::~OPN2()
{
    ClearChips();
}

void OPN2::PokeO(size_t card, uint8_t port, uint8_t index, uint8_t value)
{
    cardsOP2[card]->writeReg(port, index, value);
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

    while((hertz >= 1023.75) && (x2 < 0x3800))
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

    const opnInstData &adli = ins[c];

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

void OPN2::Patch(uint16_t c, const opnInstData &adli)
{
    unsigned    card;
    uint8_t     port, cc;
    getOpnChannel(uint16_t(c), card, port, cc);
    ins[c] = adli;
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
    const opnInstData &adli = ins[c];
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
        cardsOP2[i].reset(NULL);
    cardsOP2.clear();
}

void OPN2::Reset(int emulator, unsigned long PCM_RATE)
{
    ClearChips();
    ins.clear();
    pit.clear();
    regBD.clear();
    cardsOP2.resize(NumCards, AdlMIDI_SPtr<OPNChipBase>());

    for(size_t i = 0; i < cardsOP2.size(); i++)
    {
        switch(emulator)
        {
        default:
#ifndef OPNMIDI_DISABLE_MAME_EMULATOR
        case OPNMIDI_EMU_MAME:
            cardsOP2[i].reset(new MameOPN2());
            break;
#endif
#ifndef OPNMIDI_DISABLE_NUKED_EMULATOR
        case OPNMIDI_EMU_NUKED:
            cardsOP2[i].reset(new NukedOPN2());
            break;
#endif
#ifndef OPNMIDI_DISABLE_GENS_EMULATOR
        case OPNMIDI_EMU_GENS:
            cardsOP2[i].reset(new GensOPN2());
            break;
#endif
#ifndef OPNMIDI_DISABLE_GX_EMULATOR
        case OPNMIDI_EMU_GX:
            cardsOP2[i].reset(new GXOPN2());
            break;
#endif
        }
        cardsOP2[i]->setRate((uint32_t)PCM_RATE, 7670454);
        if(runAtPcmRate)
            cardsOP2[i]->setRunningAtPcmRate(true);
    }

    NumChannels = NumCards * 6;
    ins.resize(NumChannels,   emptyInstrument.opn[0]);
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
