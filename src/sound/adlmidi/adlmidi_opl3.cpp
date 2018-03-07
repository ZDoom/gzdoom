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

#ifdef ADLMIDI_HW_OPL
static const unsigned OPLBase = 0x388;
#endif

#ifdef DISABLE_EMBEDDED_BANKS
/*
    Dummy data which replaces adldata.cpp banks database
*/

const struct adldata adl[] =
{
    {0, 0, (unsigned char)'\0', (unsigned char)'\0', (unsigned char)'\0', 0}
};

const struct adlinsdata adlins[] =
{
    {0, 0, 0, 0, 0, 0, 0.0}
};

int maxAdlBanks()
{
    return 0;
}

const unsigned short banks[][256] = {{0}};
const char *const banknames[] = {"<Embedded banks are disabled>"};
const AdlBankSetup adlbanksetup[] = {{0, 1, 1, 0, 0}};
#endif

static const unsigned short Operators[23 * 2] =
{
    // Channels 0-2
    0x000, 0x003, 0x001, 0x004, 0x002, 0x005, // operators  0, 3,  1, 4,  2, 5
    // Channels 3-5
    0x008, 0x00B, 0x009, 0x00C, 0x00A, 0x00D, // operators  6, 9,  7,10,  8,11
    // Channels 6-8
    0x010, 0x013, 0x011, 0x014, 0x012, 0x015, // operators 12,15, 13,16, 14,17
    // Same for second card
    0x100, 0x103, 0x101, 0x104, 0x102, 0x105, // operators 18,21, 19,22, 20,23
    0x108, 0x10B, 0x109, 0x10C, 0x10A, 0x10D, // operators 24,27, 25,28, 26,29
    0x110, 0x113, 0x111, 0x114, 0x112, 0x115, // operators 30,33, 31,34, 32,35
    // Channel 18
    0x010, 0x013,  // operators 12,15
    // Channel 19
    0x014, 0xFFF,  // operator 16
    // Channel 19
    0x012, 0xFFF,  // operator 14
    // Channel 19
    0x015, 0xFFF,  // operator 17
    // Channel 19
    0x011, 0xFFF
}; // operator 13

static const unsigned short Channels[23] =
{
    0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, // 0..8
    0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, // 9..17 (secondary set)
    0x006, 0x007, 0x008, 0xFFF, 0xFFF
}; // <- hw percussions, 0xFFF = no support for pitch/pan

/*
    In OPL3 mode:
         0    1    2    6    7    8     9   10   11    16   17   18
       op0  op1  op2 op12 op13 op14  op18 op19 op20  op30 op31 op32
       op3  op4  op5 op15 op16 op17  op21 op22 op23  op33 op34 op35
         3    4    5                   13   14   15
       op6  op7  op8                 op24 op25 op26
       op9 op10 op11                 op27 op28 op29
    Ports:
        +0   +1   +2  +10  +11  +12  +100 +101 +102  +110 +111 +112
        +3   +4   +5  +13  +14  +15  +103 +104 +105  +113 +114 +115
        +8   +9   +A                 +108 +109 +10A
        +B   +C   +D                 +10B +10C +10D

    Percussion:
      bassdrum = op(0): 0xBD bit 0x10, operators 12 (0x10) and 15 (0x13) / channels 6, 6b
      snare    = op(3): 0xBD bit 0x08, operators 16 (0x14)               / channels 7b
      tomtom   = op(4): 0xBD bit 0x04, operators 14 (0x12)               / channels 8
      cym      = op(5): 0xBD bit 0x02, operators 17 (0x17)               / channels 8b
      hihat    = op(2): 0xBD bit 0x01, operators 13 (0x11)               / channels 7


    In OPTi mode ("extended FM" in 82C924, 82C925, 82C931 chips):
         0   1   2    3    4    5    6    7     8    9   10   11   12   13   14   15   16   17
       op0 op4 op6 op10 op12 op16 op18 op22  op24 op28 op30 op34 op36 op38 op40 op42 op44 op46
       op1 op5 op7 op11 op13 op17 op19 op23  op25 op29 op31 op35 op37 op39 op41 op43 op45 op47
       op2     op8      op14      op20       op26      op32
       op3     op9      op15      op21       op27      op33    for a total of 6 quad + 12 dual
    Ports: ???
*/


const adlinsdata &OPL3::GetAdlMetaIns(size_t n)
{
    return (n & DynamicMetaInstrumentTag) ?
           dynamic_metainstruments[n & ~DynamicMetaInstrumentTag]
           : adlins[n];
}

size_t OPL3::GetAdlMetaNumber(size_t midiins)
{
    return (AdlBank == ~0u) ?
           (midiins | DynamicMetaInstrumentTag)
           : banks[AdlBank][midiins];
}

const adldata &OPL3::GetAdlIns(size_t insno)
{
    return (insno & DynamicInstrumentTag)
           ? dynamic_instruments[insno & ~DynamicInstrumentTag]
            : adl[insno];
}

void OPL3::setEmbeddedBank(unsigned int bank)
{
    AdlBank = bank;
    //Embedded banks are supports 128:128 GM set only
    dynamic_percussion_offset = 128;
    dynamic_melodic_banks.clear();
    dynamic_percussion_banks.clear();
}


OPL3::OPL3() :
    dynamic_percussion_offset(128),
    DynamicInstrumentTag(0x8000u),
    DynamicMetaInstrumentTag(0x4000000u),
    NumCards(1),
    AdlBank(0),
    NumFourOps(0),
    HighTremoloMode(false),
    HighVibratoMode(false),
    AdlPercussionMode(false),
    LogarithmicVolumes(false),
    //CartoonersVolumes(false),
    m_musicMode(MODE_MIDI),
    m_volumeScale(VOLUME_Generic)
{}

void OPL3::Poke(size_t card, uint32_t index, uint32_t value)
{
    #ifdef ADLMIDI_HW_OPL
    (void)card;
    unsigned o = index >> 8;
    unsigned port = OPLBase + o * 2;

    #ifdef __DJGPP__
    outportb(port, index);
    for(unsigned c = 0; c < 6; ++c) inportb(port);
    outportb(port + 1, value);
    for(unsigned c = 0; c < 35; ++c) inportb(port);
    #endif//__DJGPP__

    #ifdef __WATCOMC__
    outp(port, index);
    for(uint16_t c = 0; c < 6; ++c)  inp(port);
    outp(port + 1, value);
    for(uint16_t c = 0; c < 35; ++c) inp(port);
    #endif//__WATCOMC__

    #else//ADLMIDI_HW_OPL

    #ifdef ADLMIDI_USE_DOSBOX_OPL
    cards[card].WriteReg(index, static_cast<uint8_t>(value));
    #else
    OPL3_WriteReg(&cards[card], static_cast<Bit16u>(index), static_cast<Bit8u>(value));
    #endif

    #endif//ADLMIDI_HW_OPL
}

void OPL3::PokeN(size_t card, uint16_t index, uint8_t value)
{
    #ifdef ADLMIDI_HW_OPL
    (void)card;
    unsigned o = index >> 8;
    unsigned port = OPLBase + o * 2;

    #ifdef __DJGPP__
    outportb(port, index);
    for(unsigned c = 0; c < 6; ++c) inportb(port);
    outportb(port + 1, value);
    for(unsigned c = 0; c < 35; ++c) inportb(port);
    #endif

    #ifdef __WATCOMC__
    outp(port, index);
    for(uint16_t c = 0; c < 6; ++c)  inp(port);
    outp(port + 1, value);
    for(uint16_t c = 0; c < 35; ++c) inp(port);
    #endif//__WATCOMC__

    #else
    #ifdef ADLMIDI_USE_DOSBOX_OPL
    cards[card].WriteReg(static_cast<Bit32u>(index), value);
    #else
    OPL3_WriteReg(&cards[card], index, value);
    #endif
    #endif
}


void OPL3::NoteOff(size_t c)
{
    size_t card = c / 23, cc = c % 23;

    if(cc >= 18)
    {
        regBD[card] &= ~(0x10 >> (cc - 18));
        Poke(card, 0xBD, regBD[card]);
        return;
    }

    Poke(card, 0xB0 + Channels[cc], pit[c] & 0xDF);
}

void OPL3::NoteOn(unsigned c, double hertz) // Hertz range: 0..131071
{
    unsigned card = c / 23, cc = c % 23;
    unsigned x = 0x2000;

    if(hertz < 0 || hertz > 131071) // Avoid infinite loop
        return;

    while(hertz >= 1023.5)
    {
        hertz /= 2.0;    // Calculate octave
        x += 0x400;
    }

    x += static_cast<unsigned int>(hertz + 0.5);
    unsigned chn = Channels[cc];

    if(cc >= 18)
    {
        regBD[card] |= (0x10 >> (cc - 18));
        Poke(card, 0x0BD, regBD[card]);
        x &= ~0x2000u;
        //x |= 0x800; // for test
    }

    if(chn != 0xFFF)
    {
        Poke(card, 0xA0 + chn, x & 0xFF);
        Poke(card, 0xB0 + chn, pit[c] = static_cast<uint8_t>(x >> 8));
    }
}

void OPL3::Touch_Real(unsigned c, unsigned volume, uint8_t brightness)
{
    if(volume > 63)
        volume = 63;

    size_t card = c / 23, cc = c % 23;
    size_t i = ins[c];
    uint16_t o1 = Operators[cc * 2 + 0];
    uint16_t o2 = Operators[cc * 2 + 1];
    const adldata &adli = GetAdlIns(i);
    uint8_t  x = adli.modulator_40, y = adli.carrier_40;
    uint16_t mode = 1; // 2-op AM

    if(four_op_category[c] == 0 || four_op_category[c] == 3)
    {
        mode = adli.feedconn & 1; // 2-op FM or 2-op AM
    }
    else if(four_op_category[c] == 1 || four_op_category[c] == 2)
    {
        size_t i0, i1;

        if(four_op_category[c] == 1)
        {
            i0 = i;
            i1 = ins[c + 3];
            mode = 2; // 4-op xx-xx ops 1&2
        }
        else
        {
            i0 = ins[c - 3];
            i1 = i;
            mode = 6; // 4-op xx-xx ops 3&4
        }

        mode += (GetAdlIns(i0).feedconn & 1) + (GetAdlIns(i1).feedconn & 1) * 2;
    }

    static const bool do_ops[10][2] =
    {
        { false, true },  /* 2 op FM */
        { true,  true },  /* 2 op AM */
        { false, false }, /* 4 op FM-FM ops 1&2 */
        { true,  false }, /* 4 op AM-FM ops 1&2 */
        { false, true  }, /* 4 op FM-AM ops 1&2 */
        { true,  false }, /* 4 op AM-AM ops 1&2 */
        { false, true  }, /* 4 op FM-FM ops 3&4 */
        { false, true  }, /* 4 op AM-FM ops 3&4 */
        { false, true  }, /* 4 op FM-AM ops 3&4 */
        { true,  true  }  /* 4 op AM-AM ops 3&4 */
    };

    if(m_musicMode == MODE_RSXX)
    {
        Poke(card, 0x40 + o1, x);
        if(o2 != 0xFFF)
            Poke(card, 0x40 + o2, y - volume / 2);
    }
    else
    {
        bool do_modulator = do_ops[ mode ][ 0 ] || ScaleModulators;
        bool do_carrier   = do_ops[ mode ][ 1 ] || ScaleModulators;

        uint32_t modulator = do_modulator ? (x | 63) - volume + volume * (x & 63) / 63 : x;
        uint32_t carrier   = do_carrier   ? (y | 63) - volume + volume * (y & 63) / 63 : y;

        if(brightness != 127)
        {
            brightness = static_cast<uint8_t>(::round(127.0 * ::sqrt((static_cast<double>(brightness)) * (1.0 / 127.0))) / 2.0);
            if(!do_modulator)
                modulator = (modulator | 63) - brightness + brightness * (modulator & 63) / 63;
            if(!do_carrier)
                carrier = (carrier | 63) - brightness + brightness * (carrier & 63) / 63;
        }

        Poke(card, 0x40 + o1, modulator);
        if(o2 != 0xFFF)
            Poke(card, 0x40 + o2, carrier);
    }

    // Correct formula (ST3, AdPlug):
    //   63-((63-(instrvol))/63)*chanvol
    // Reduces to (tested identical):
    //   63 - chanvol + chanvol*instrvol/63
    // Also (slower, floats):
    //   63 + chanvol * (instrvol / 63.0 - 1)
}

/*
void OPL3::Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
{
    if(LogarithmicVolumes)
        Touch_Real(c, volume * 127 / (127 * 127 * 127) / 2);
    else
    {
        // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
        Touch_Real(c, volume > 8725 ? static_cast<unsigned int>(std::log(volume) * 11.541561 + (0.5 - 104.22845)) : 0);
        // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
        //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
    }
}*/

void OPL3::Patch(uint16_t c, size_t i)
{
    uint16_t card = c / 23, cc = c % 23;
    static const uint8_t data[4] = {0x20, 0x60, 0x80, 0xE0};
    ins[c] = i;
    uint16_t o1 = Operators[cc * 2 + 0];
    uint16_t o2 = Operators[cc * 2 + 1];
    const adldata &adli = GetAdlIns(i);
    unsigned x = adli.modulator_E862, y = adli.carrier_E862;

    for(unsigned a = 0; a < 4; ++a, x >>= 8, y >>= 8)
    {
        Poke(card, data[a] + o1, x & 0xFF);
        if(o2 != 0xFFF)
            Poke(card, data[a] + o2, y & 0xFF);
    }
}

void OPL3::Pan(unsigned c, unsigned value)
{
    unsigned card = c / 23, cc = c % 23;

    if(Channels[cc] != 0xFFF)
        Poke(card, 0xC0 + Channels[cc], GetAdlIns(ins[c]).feedconn | value);
}

void OPL3::Silence() // Silence all OPL channels.
{
    for(unsigned c = 0; c < NumChannels; ++c)
    {
        NoteOff(c);
        Touch_Real(c, 0);
    }
}

void OPL3::updateFlags()
{
    unsigned fours = NumFourOps;

    for(unsigned card = 0; card < NumCards; ++card)
    {
        Poke(card, 0x0BD, regBD[card] = (HighTremoloMode * 0x80
                                         + HighVibratoMode * 0x40
                                         + AdlPercussionMode * 0x20));
        unsigned fours_this_card = std::min(fours, 6u);
        Poke(card, 0x104, (1 << fours_this_card) - 1);
        fours -= fours_this_card;
    }

    // Mark all channels that are reserved for four-operator function
    if(AdlPercussionMode == 1)
        for(unsigned a = 0; a < NumCards; ++a)
        {
            for(unsigned b = 0; b < 5; ++b)
                four_op_category[a * 23 + 18 + b] = static_cast<char>(b + 3);
            for(unsigned b = 0; b < 3; ++b)
                four_op_category[a * 23 + 6  + b] = 8;
        }

    unsigned nextfour = 0;

    for(unsigned a = 0; a < NumFourOps; ++a)
    {
        four_op_category[nextfour  ] = 1;
        four_op_category[nextfour + 3] = 2;

        switch(a % 6)
        {
        case 0:
        case 1:
            nextfour += 1;
            break;
        case 2:
            nextfour += 9 - 2;
            break;
        case 3:
        case 4:
            nextfour += 1;
            break;
        case 5:
            nextfour += 23 - 9 - 2;
            break;
        }
    }
}

void OPL3::updateDeepFlags()
{
    for(unsigned card = 0; card < NumCards; ++card)
    {
        Poke(card, 0x0BD, regBD[card] = (HighTremoloMode * 0x80
                                         + HighVibratoMode * 0x40
                                         + AdlPercussionMode * 0x20));
    }
}

void OPL3::ChangeVolumeRangesModel(ADLMIDI_VolumeModels volumeModel)
{
    switch(volumeModel)
    {
    case ADLMIDI_VolumeModel_AUTO://Do nothing until restart playing
        break;

    case ADLMIDI_VolumeModel_Generic:
        m_volumeScale = OPL3::VOLUME_Generic;
        break;

    case ADLMIDI_VolumeModel_CMF:
        LogarithmicVolumes = true;
        m_volumeScale = OPL3::VOLUME_CMF;
        break;

    case ADLMIDI_VolumeModel_DMX:
        m_volumeScale = OPL3::VOLUME_DMX;
        break;

    case ADLMIDI_VolumeModel_APOGEE:
        m_volumeScale = OPL3::VOLUME_APOGEE;
        break;

    case ADLMIDI_VolumeModel_9X:
        m_volumeScale = OPL3::VOLUME_9X;
        break;
    }
}

void OPL3::Reset(unsigned long PCM_RATE)
{
    #ifndef ADLMIDI_HW_OPL
    #ifdef ADLMIDI_USE_DOSBOX_OPL
    DBOPL::Handler emptyChip; //Constructors inside are will initialize necessary fields
    #else
    _opl3_chip emptyChip;
    std::memset(&emptyChip, 0, sizeof(_opl3_chip));
    #endif
    cards.clear();
    #endif
    (void)PCM_RATE;
    ins.clear();
    pit.clear();
    regBD.clear();
    #ifndef ADLMIDI_HW_OPL
    cards.resize(NumCards, emptyChip);
    #endif
    NumChannels = NumCards * 23;
    ins.resize(NumChannels, 189);
    pit.resize(NumChannels,   0);
    regBD.resize(NumCards,    0);
    four_op_category.resize(NumChannels, 0);

    for(unsigned p = 0, a = 0; a < NumCards; ++a)
    {
        for(unsigned b = 0; b < 18; ++b) four_op_category[p++] = 0;
        for(unsigned b = 0; b < 5; ++b)  four_op_category[p++] = 8;
    }

    static const uint16_t data[] =
    {
        0x004, 96, 0x004, 128,        // Pulse timer
        0x105, 0, 0x105, 1, 0x105, 0, // Pulse OPL3 enable
        0x001, 32, 0x105, 1           // Enable wave, OPL3 extensions
    };
    unsigned fours = NumFourOps;

    for(unsigned card = 0; card < NumCards; ++card)
    {
        #ifndef ADLMIDI_HW_OPL
        #   ifdef ADLMIDI_USE_DOSBOX_OPL
        cards[card].Init(PCM_RATE);
        #   else
        OPL3_Reset(&cards[card], static_cast<Bit32u>(PCM_RATE));
        #   endif
        #endif

        for(unsigned a = 0; a < 18; ++a) Poke(card, 0xB0 + Channels[a], 0x00);
        for(unsigned a = 0; a < sizeof(data) / sizeof(*data); a += 2)
            PokeN(card, data[a], static_cast<uint8_t>(data[a + 1]));
        Poke(card, 0x0BD, regBD[card] = (HighTremoloMode * 0x80
                                         + HighVibratoMode * 0x40
                                         + AdlPercussionMode * 0x20));
        unsigned fours_this_card = std::min(fours, 6u);
        Poke(card, 0x104, (1 << fours_this_card) - 1);
        //fprintf(stderr, "Card %u: %u four-ops.\n", card, fours_this_card);
        fours -= fours_this_card;
    }

    // Mark all channels that are reserved for four-operator function
    if(AdlPercussionMode == 1)
    {
        for(unsigned a = 0; a < NumCards; ++a)
        {
            for(unsigned b = 0; b < 5; ++b) four_op_category[a * 23 + 18 + b] = static_cast<char>(b + 3);
            for(unsigned b = 0; b < 3; ++b) four_op_category[a * 23 + 6  + b] = 8;
        }
    }
    unsigned nextfour = 0;

    for(unsigned a = 0; a < NumFourOps; ++a)
    {
        four_op_category[nextfour  ] = 1;
        four_op_category[nextfour + 3] = 2;

        switch(a % 6)
        {
        case 0:
        case 1:
            nextfour += 1;
            break;

        case 2:
            nextfour += 9 - 2;
            break;

        case 3:
        case 4:
            nextfour += 1;
            break;

        case 5:
            nextfour += 23 - 9 - 2;
            break;
        }
    }

    /**/
    /*
        In two-op mode, channels 0..8 go as follows:
                      Op1[port]  Op2[port]
          Channel 0:  00  00     03  03
          Channel 1:  01  01     04  04
          Channel 2:  02  02     05  05
          Channel 3:  06  08     09  0B
          Channel 4:  07  09     10  0C
          Channel 5:  08  0A     11  0D
          Channel 6:  12  10     15  13
          Channel 7:  13  11     16  14
          Channel 8:  14  12     17  15
        In four-op mode, channels 0..8 go as follows:
                      Op1[port]  Op2[port]  Op3[port]  Op4[port]
          Channel 0:  00  00     03  03     06  08     09  0B
          Channel 1:  01  01     04  04     07  09     10  0C
          Channel 2:  02  02     05  05     08  0A     11  0D
          Channel 3:  CHANNEL 0 SLAVE
          Channel 4:  CHANNEL 1 SLAVE
          Channel 5:  CHANNEL 2 SLAVE
          Channel 6:  12  10     15  13
          Channel 7:  13  11     16  14
          Channel 8:  14  12     17  15
         Same goes principally for channels 9-17 respectively.
        */
    Silence();
}
