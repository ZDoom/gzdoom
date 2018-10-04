/*
 * Copyright (C) 2013-2016 Alexey Khokholov (Nuke.YKT)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *  Nuked OPL3 emulator.
 *  Thanks:
 *      MAME Development Team(Jarek Burczynski, Tatsuyuki Satoh):
 *          Feedback and Rhythm part calculation information.
 *      forums.submarine.org.uk(carbon14, opl3):
 *          Tremolo and phase generator calculation information.
 *      OPLx decapsulated(Matthew Gambrell, Olli Niemitalo):
 *          OPL2 ROMs.
 *
 * version: 1.7.4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nukedopl3_174.h"

#define RSM_FRAC    10

/* Channel types */

enum {
    ch_2op = 0,
    ch_4op = 1,
    ch_4op2 = 2,
    ch_drum = 3
};

/* Envelope key types */

enum {
    egk_norm = 0x01,
    egk_drum = 0x02
};


/*
 * logsin table
 */

static const Bit16u logsinrom[512] = {
    0x859, 0x6c3, 0x607, 0x58b, 0x52e, 0x4e4, 0x4a6, 0x471,
    0x443, 0x41a, 0x3f5, 0x3d3, 0x3b5, 0x398, 0x37e, 0x365,
    0x34e, 0x339, 0x324, 0x311, 0x2ff, 0x2ed, 0x2dc, 0x2cd,
    0x2bd, 0x2af, 0x2a0, 0x293, 0x286, 0x279, 0x26d, 0x261,
    0x256, 0x24b, 0x240, 0x236, 0x22c, 0x222, 0x218, 0x20f,
    0x206, 0x1fd, 0x1f5, 0x1ec, 0x1e4, 0x1dc, 0x1d4, 0x1cd,
    0x1c5, 0x1be, 0x1b7, 0x1b0, 0x1a9, 0x1a2, 0x19b, 0x195,
    0x18f, 0x188, 0x182, 0x17c, 0x177, 0x171, 0x16b, 0x166,
    0x160, 0x15b, 0x155, 0x150, 0x14b, 0x146, 0x141, 0x13c,
    0x137, 0x133, 0x12e, 0x129, 0x125, 0x121, 0x11c, 0x118,
    0x114, 0x10f, 0x10b, 0x107, 0x103, 0x0ff, 0x0fb, 0x0f8,
    0x0f4, 0x0f0, 0x0ec, 0x0e9, 0x0e5, 0x0e2, 0x0de, 0x0db,
    0x0d7, 0x0d4, 0x0d1, 0x0cd, 0x0ca, 0x0c7, 0x0c4, 0x0c1,
    0x0be, 0x0bb, 0x0b8, 0x0b5, 0x0b2, 0x0af, 0x0ac, 0x0a9,
    0x0a7, 0x0a4, 0x0a1, 0x09f, 0x09c, 0x099, 0x097, 0x094,
    0x092, 0x08f, 0x08d, 0x08a, 0x088, 0x086, 0x083, 0x081,
    0x07f, 0x07d, 0x07a, 0x078, 0x076, 0x074, 0x072, 0x070,
    0x06e, 0x06c, 0x06a, 0x068, 0x066, 0x064, 0x062, 0x060,
    0x05e, 0x05c, 0x05b, 0x059, 0x057, 0x055, 0x053, 0x052,
    0x050, 0x04e, 0x04d, 0x04b, 0x04a, 0x048, 0x046, 0x045,
    0x043, 0x042, 0x040, 0x03f, 0x03e, 0x03c, 0x03b, 0x039,
    0x038, 0x037, 0x035, 0x034, 0x033, 0x031, 0x030, 0x02f,
    0x02e, 0x02d, 0x02b, 0x02a, 0x029, 0x028, 0x027, 0x026,
    0x025, 0x024, 0x023, 0x022, 0x021, 0x020, 0x01f, 0x01e,
    0x01d, 0x01c, 0x01b, 0x01a, 0x019, 0x018, 0x017, 0x017,
    0x016, 0x015, 0x014, 0x014, 0x013, 0x012, 0x011, 0x011,
    0x010, 0x00f, 0x00f, 0x00e, 0x00d, 0x00d, 0x00c, 0x00c,
    0x00b, 0x00a, 0x00a, 0x009, 0x009, 0x008, 0x008, 0x007,
    0x007, 0x007, 0x006, 0x006, 0x005, 0x005, 0x005, 0x004,
    0x004, 0x004, 0x003, 0x003, 0x003, 0x002, 0x002, 0x002,
    0x002, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001,
    0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
    0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
    0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x002,
    0x002, 0x002, 0x002, 0x003, 0x003, 0x003, 0x004, 0x004,
    0x004, 0x005, 0x005, 0x005, 0x006, 0x006, 0x007, 0x007,
    0x007, 0x008, 0x008, 0x009, 0x009, 0x00a, 0x00a, 0x00b,
    0x00c, 0x00c, 0x00d, 0x00d, 0x00e, 0x00f, 0x00f, 0x010,
    0x011, 0x011, 0x012, 0x013, 0x014, 0x014, 0x015, 0x016,
    0x017, 0x017, 0x018, 0x019, 0x01a, 0x01b, 0x01c, 0x01d,
    0x01e, 0x01f, 0x020, 0x021, 0x022, 0x023, 0x024, 0x025,
    0x026, 0x027, 0x028, 0x029, 0x02a, 0x02b, 0x02d, 0x02e,
    0x02f, 0x030, 0x031, 0x033, 0x034, 0x035, 0x037, 0x038,
    0x039, 0x03b, 0x03c, 0x03e, 0x03f, 0x040, 0x042, 0x043,
    0x045, 0x046, 0x048, 0x04a, 0x04b, 0x04d, 0x04e, 0x050,
    0x052, 0x053, 0x055, 0x057, 0x059, 0x05b, 0x05c, 0x05e,
    0x060, 0x062, 0x064, 0x066, 0x068, 0x06a, 0x06c, 0x06e,
    0x070, 0x072, 0x074, 0x076, 0x078, 0x07a, 0x07d, 0x07f,
    0x081, 0x083, 0x086, 0x088, 0x08a, 0x08d, 0x08f, 0x092,
    0x094, 0x097, 0x099, 0x09c, 0x09f, 0x0a1, 0x0a4, 0x0a7,
    0x0a9, 0x0ac, 0x0af, 0x0b2, 0x0b5, 0x0b8, 0x0bb, 0x0be,
    0x0c1, 0x0c4, 0x0c7, 0x0ca, 0x0cd, 0x0d1, 0x0d4, 0x0d7,
    0x0db, 0x0de, 0x0e2, 0x0e5, 0x0e9, 0x0ec, 0x0f0, 0x0f4,
    0x0f8, 0x0fb, 0x0ff, 0x103, 0x107, 0x10b, 0x10f, 0x114,
    0x118, 0x11c, 0x121, 0x125, 0x129, 0x12e, 0x133, 0x137,
    0x13c, 0x141, 0x146, 0x14b, 0x150, 0x155, 0x15b, 0x160,
    0x166, 0x16b, 0x171, 0x177, 0x17c, 0x182, 0x188, 0x18f,
    0x195, 0x19b, 0x1a2, 0x1a9, 0x1b0, 0x1b7, 0x1be, 0x1c5,
    0x1cd, 0x1d4, 0x1dc, 0x1e4, 0x1ec, 0x1f5, 0x1fd, 0x206,
    0x20f, 0x218, 0x222, 0x22c, 0x236, 0x240, 0x24b, 0x256,
    0x261, 0x26d, 0x279, 0x286, 0x293, 0x2a0, 0x2af, 0x2bd,
    0x2cd, 0x2dc, 0x2ed, 0x2ff, 0x311, 0x324, 0x339, 0x34e,
    0x365, 0x37e, 0x398, 0x3b5, 0x3d3, 0x3f5, 0x41a, 0x443,
    0x471, 0x4a6, 0x4e4, 0x52e, 0x58b, 0x607, 0x6c3, 0x859
};

/*
 * exp table
 */

static const Bit16u exprom[256] = {
    0xff4, 0xfea, 0xfde, 0xfd4, 0xfc8, 0xfbe, 0xfb4, 0xfa8,
    0xf9e, 0xf92, 0xf88, 0xf7e, 0xf72, 0xf68, 0xf5c, 0xf52,
    0xf48, 0xf3e, 0xf32, 0xf28, 0xf1e, 0xf14, 0xf08, 0xefe,
    0xef4, 0xeea, 0xee0, 0xed4, 0xeca, 0xec0, 0xeb6, 0xeac,
    0xea2, 0xe98, 0xe8e, 0xe84, 0xe7a, 0xe70, 0xe66, 0xe5c,
    0xe52, 0xe48, 0xe3e, 0xe34, 0xe2a, 0xe20, 0xe16, 0xe0c,
    0xe04, 0xdfa, 0xdf0, 0xde6, 0xddc, 0xdd2, 0xdca, 0xdc0,
    0xdb6, 0xdac, 0xda4, 0xd9a, 0xd90, 0xd88, 0xd7e, 0xd74,
    0xd6a, 0xd62, 0xd58, 0xd50, 0xd46, 0xd3c, 0xd34, 0xd2a,
    0xd22, 0xd18, 0xd10, 0xd06, 0xcfe, 0xcf4, 0xcec, 0xce2,
    0xcda, 0xcd0, 0xcc8, 0xcbe, 0xcb6, 0xcae, 0xca4, 0xc9c,
    0xc92, 0xc8a, 0xc82, 0xc78, 0xc70, 0xc68, 0xc60, 0xc56,
    0xc4e, 0xc46, 0xc3c, 0xc34, 0xc2c, 0xc24, 0xc1c, 0xc12,
    0xc0a, 0xc02, 0xbfa, 0xbf2, 0xbea, 0xbe0, 0xbd8, 0xbd0,
    0xbc8, 0xbc0, 0xbb8, 0xbb0, 0xba8, 0xba0, 0xb98, 0xb90,
    0xb88, 0xb80, 0xb78, 0xb70, 0xb68, 0xb60, 0xb58, 0xb50,
    0xb48, 0xb40, 0xb38, 0xb32, 0xb2a, 0xb22, 0xb1a, 0xb12,
    0xb0a, 0xb02, 0xafc, 0xaf4, 0xaec, 0xae4, 0xade, 0xad6,
    0xace, 0xac6, 0xac0, 0xab8, 0xab0, 0xaa8, 0xaa2, 0xa9a,
    0xa92, 0xa8c, 0xa84, 0xa7c, 0xa76, 0xa6e, 0xa68, 0xa60,
    0xa58, 0xa52, 0xa4a, 0xa44, 0xa3c, 0xa36, 0xa2e, 0xa28,
    0xa20, 0xa18, 0xa12, 0xa0c, 0xa04, 0x9fe, 0x9f6, 0x9f0,
    0x9e8, 0x9e2, 0x9da, 0x9d4, 0x9ce, 0x9c6, 0x9c0, 0x9b8,
    0x9b2, 0x9ac, 0x9a4, 0x99e, 0x998, 0x990, 0x98a, 0x984,
    0x97c, 0x976, 0x970, 0x96a, 0x962, 0x95c, 0x956, 0x950,
    0x948, 0x942, 0x93c, 0x936, 0x930, 0x928, 0x922, 0x91c,
    0x916, 0x910, 0x90a, 0x904, 0x8fc, 0x8f6, 0x8f0, 0x8ea,
    0x8e4, 0x8de, 0x8d8, 0x8d2, 0x8cc, 0x8c6, 0x8c0, 0x8ba,
    0x8b4, 0x8ae, 0x8a8, 0x8a2, 0x89c, 0x896, 0x890, 0x88a,
    0x884, 0x87e, 0x878, 0x872, 0x86c, 0x866, 0x860, 0x85a,
    0x854, 0x850, 0x84a, 0x844, 0x83e, 0x838, 0x832, 0x82c,
    0x828, 0x822, 0x81c, 0x816, 0x810, 0x80c, 0x806, 0x800
};

/*
 * freq mult table multiplied by 2
 *
 * 1/2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15
 */

static const Bit8u mt[16] = {
    1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30
};

/*
 * ksl table
 */

static const Bit8u kslrom[16] = {
    0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64
};

static const Bit8u kslshift[4] = {
    8, 1, 2, 0
};

/*
 * envelope generator constants
 */

static const Bit8u eg_incstep[3][4][8] = {
    {
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 }
    },
    {
        { 0, 1, 0, 1, 0, 1, 0, 1 },
        { 0, 1, 0, 1, 1, 1, 0, 1 },
        { 0, 1, 1, 1, 0, 1, 1, 1 },
        { 0, 1, 1, 1, 1, 1, 1, 1 }
    },
    {
        { 1, 1, 1, 1, 1, 1, 1, 1 },
        { 2, 2, 1, 1, 1, 1, 1, 1 },
        { 2, 2, 1, 1, 2, 2, 1, 1 },
        { 2, 2, 2, 2, 2, 2, 1, 1 }
    }
};

static const Bit8u eg_incdesc[16] = {
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2
};

static const Bit8s eg_incsh[16] = {
    0, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, -1, -2
};

/*
 * address decoding
 */

static const Bit8s ad_slot[0x20] = {
    0, 1, 2, 3, 4, 5, -1, -1, 6, 7, 8, 9, 10, 11, -1, -1,
    12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const Bit8u ch_slot[18] = {
    0, 1, 2, 6, 7, 8, 12, 13, 14, 18, 19, 20, 24, 25, 26, 30, 31, 32
};

/*
 * Pan law table
 */

static const Bit16u panlawtable[] =
{
    65535, 65529, 65514, 65489, 65454, 65409, 65354, 65289,
    65214, 65129, 65034, 64929, 64814, 64689, 64554, 64410,
    64255, 64091, 63917, 63733, 63540, 63336, 63123, 62901,
    62668, 62426, 62175, 61914, 61644, 61364, 61075, 60776,
    60468, 60151, 59825, 59489, 59145, 58791, 58428, 58057,
    57676, 57287, 56889, 56482, 56067, 55643, 55211, 54770,
    54320, 53863, 53397, 52923, 52441, 51951, 51453, 50947,
    50433, 49912, 49383, 48846, 48302, 47750, 47191,
    46340, /* Center left */
    46340, /* Center right */
    45472, 44885, 44291, 43690, 43083, 42469, 41848, 41221,
    40588, 39948, 39303, 38651, 37994, 37330, 36661, 35986,
    35306, 34621, 33930, 33234, 32533, 31827, 31116, 30400,
    29680, 28955, 28225, 27492, 26754, 26012, 25266, 24516,
    23762, 23005, 22244, 21480, 20713, 19942, 19169, 18392,
    17613, 16831, 16046, 15259, 14469, 13678, 12884, 12088,
    11291, 10492, 9691, 8888, 8085, 7280, 6473, 5666,
    4858, 4050, 3240, 2431, 1620, 810, 0
};

/*
 * Envelope generator
 */

static void OPL3_EnvelopeGenOff(opl3_slot *slot);
static void OPL3_EnvelopeGenAttack(opl3_slot *slot);
static void OPL3_EnvelopeGenDecay(opl3_slot *slot);
static void OPL3_EnvelopeGenSustain(opl3_slot *slot);
static void OPL3_EnvelopeGenRelease(opl3_slot *slot);

typedef void(*envelope_genfunc)(opl3_slot *slot);

envelope_genfunc envelope_gen[5] = {
    OPL3_EnvelopeGenOff,
    OPL3_EnvelopeGenAttack,
    OPL3_EnvelopeGenDecay,
    OPL3_EnvelopeGenSustain,
    OPL3_EnvelopeGenRelease
};

enum envelope_gen_num
{
    envelope_gen_num_off = 0,
    envelope_gen_num_attack = 1,
    envelope_gen_num_decay = 2,
    envelope_gen_num_sustain = 3,
    envelope_gen_num_release = 4
};

static Bit8u OPL3_EnvelopeCalcRate(opl3_slot *slot, Bit8u reg_rate)
{
    Bit8u rate;
    if (reg_rate == 0x00)
    {
        return 0x00;
    }
    rate = (reg_rate << 2)
         + (slot->reg_ksr ? slot->channel->ksv : (slot->channel->ksv >> 2));
    if (rate > 0x3c)
    {
        rate = 0x3c;
    }
    return rate;
}

static void OPL3_EnvelopeUpdateKSL(opl3_slot *slot)
{
    Bit16s ksl = (kslrom[slot->channel->f_num >> 6] << 2)
               - ((0x08 - slot->channel->block) << 5);
    if (ksl < 0)
    {
        ksl = 0;
    }
    slot->eg_ksl = (Bit8u)ksl;
}

static void OPL3_EnvelopeUpdateRate(opl3_slot *slot)
{
    switch (slot->eg_gen)
    {
    case envelope_gen_num_off:
    case envelope_gen_num_attack:
        slot->eg_rate = OPL3_EnvelopeCalcRate(slot, slot->reg_ar);
        break;
    case envelope_gen_num_decay:
        slot->eg_rate = OPL3_EnvelopeCalcRate(slot, slot->reg_dr);
        break;
    case envelope_gen_num_sustain:
    case envelope_gen_num_release:
        slot->eg_rate = OPL3_EnvelopeCalcRate(slot, slot->reg_rr);
        break;
    }
}

static void OPL3_EnvelopeGenOff(opl3_slot *slot)
{
    slot->eg_rout = 0x1ff;
}

static void OPL3_EnvelopeGenAttack(opl3_slot *slot)
{
    if (slot->eg_rout == 0x00)
    {
        slot->eg_gen = envelope_gen_num_decay;
        OPL3_EnvelopeUpdateRate(slot);
        return;
    }
    slot->eg_rout += ((~slot->eg_rout) * slot->eg_inc) >> 3;
    if (slot->eg_rout < 0x00)
    {
        slot->eg_rout = 0x00;
    }
}

static void OPL3_EnvelopeGenDecay(opl3_slot *slot)
{
    if (slot->eg_rout >= slot->reg_sl << 4)
    {
        slot->eg_gen = envelope_gen_num_sustain;
        OPL3_EnvelopeUpdateRate(slot);
        return;
    }
    slot->eg_rout += slot->eg_inc;
}

static void OPL3_EnvelopeGenSustain(opl3_slot *slot)
{
    if (!slot->reg_type)
    {
        OPL3_EnvelopeGenRelease(slot);
    }
}

static void OPL3_EnvelopeGenRelease(opl3_slot *slot)
{
    if (slot->eg_rout >= 0x1ff)
    {
        slot->eg_gen = envelope_gen_num_off;
        slot->eg_rout = 0x1ff;
        OPL3_EnvelopeUpdateRate(slot);
        return;
    }
    slot->eg_rout += slot->eg_inc;
}

static void OPL3_EnvelopeCalc(opl3_slot *slot)
{
    Bit8u rate_h, rate_l;
    Bit8u inc = 0;
    rate_h = slot->eg_rate >> 2;
    rate_l = slot->eg_rate & 3;
    if (eg_incsh[rate_h] > 0)
    {
        if ((slot->chip->timer & ((1 << eg_incsh[rate_h]) - 1)) == 0)
        {
            inc = eg_incstep[eg_incdesc[rate_h]][rate_l]
                            [((slot->chip->timer)>> eg_incsh[rate_h]) & 0x07];
        }
    }
    else
    {
        inc = eg_incstep[eg_incdesc[rate_h]][rate_l]
                        [slot->chip->timer & 0x07] << (-eg_incsh[rate_h]);
    }
    slot->eg_inc = inc;
    slot->eg_out = slot->eg_rout + (slot->reg_tl << 2)
                 + (slot->eg_ksl >> kslshift[slot->reg_ksl]) + *slot->trem;
    if (slot->eg_out > 0x1ff)  /* TODO: Remove this if possible */
    {
        slot->eg_out = 0x1ff;
    }
    slot->eg_out <<= 3;

    envelope_gen[slot->eg_gen](slot);
}

static void OPL3_EnvelopeKeyOn(opl3_slot *slot, Bit8u type)
{
    if (!slot->key)
    {
        slot->eg_gen = envelope_gen_num_attack;
        OPL3_EnvelopeUpdateRate(slot);
        if ((slot->eg_rate >> 2) == 0x0f)
        {
            slot->eg_gen = envelope_gen_num_decay;
            OPL3_EnvelopeUpdateRate(slot);
            slot->eg_rout = 0x00;
        }
        slot->pg_phase = 0x00;
    }
    slot->key |= type;
}

static void OPL3_EnvelopeKeyOff(opl3_slot *slot, Bit8u type)
{
    if (slot->key)
    {
        slot->key &= (~type);
        if (!slot->key)
        {
            slot->eg_gen = envelope_gen_num_release;
            OPL3_EnvelopeUpdateRate(slot);
        }
    }
}

/*
 * Phase Generator
 */

static void OPL3_PhaseGenerate(opl3_slot *slot)
{
    Bit16u f_num;
    Bit32u basefreq;

    f_num = slot->channel->f_num;
    if (slot->reg_vib)
    {
        Bit8s range;
        Bit8u vibpos;

        range = (f_num >> 7) & 7;
        vibpos = slot->chip->vibpos;

        if (!(vibpos & 3))
        {
            range = 0;
        }
        else if (vibpos & 1)
        {
            range >>= 1;
        }
        range >>= slot->chip->vibshift;

        if (vibpos & 4)
        {
            range = -range;
        }
        f_num += range;
    }
    basefreq = (f_num << slot->channel->block) >> 1;
    slot->pg_phase += (basefreq * mt[slot->reg_mult]) >> 1;
}

/*
 * Noise Generator
 */

static void OPL3_NoiseGenerate(opl3_chip *chip)
{
    if (chip->noise & 0x01)
    {
        chip->noise ^= 0x800302;
    }
    chip->noise >>= 1;
}

/*
 * Slot
 */

static void OPL3_SlotWrite20(opl3_slot *slot, Bit8u data)
{
    if ((data >> 7) & 0x01)
    {
        slot->trem = &slot->chip->tremolo;
    }
    else
    {
        slot->trem = (Bit8u*)&slot->chip->zeromod;
    }
    slot->reg_vib = (data >> 6) & 0x01;
    slot->reg_type = (data >> 5) & 0x01;
    slot->reg_ksr = (data >> 4) & 0x01;
    slot->reg_mult = data & 0x0f;
    OPL3_EnvelopeUpdateRate(slot);
}

static void OPL3_SlotWrite40(opl3_slot *slot, Bit8u data)
{
    slot->reg_ksl = (data >> 6) & 0x03;
    slot->reg_tl = data & 0x3f;
    OPL3_EnvelopeUpdateKSL(slot);
}

static void OPL3_SlotWrite60(opl3_slot *slot, Bit8u data)
{
    slot->reg_ar = (data >> 4) & 0x0f;
    slot->reg_dr = data & 0x0f;
    OPL3_EnvelopeUpdateRate(slot);
}

static void OPL3_SlotWrite80(opl3_slot *slot, Bit8u data)
{
    slot->reg_sl = (data >> 4) & 0x0f;
    if (slot->reg_sl == 0x0f)
    {
        slot->reg_sl = 0x1f;
    }
    slot->reg_rr = data & 0x0f;
    OPL3_EnvelopeUpdateRate(slot);
}

static void OPL3_SlotWriteE0(opl3_slot *slot, Bit8u data)
{
    slot->reg_wf = data & 0x07;
    if (slot->chip->newm == 0x00)
    {
        slot->reg_wf &= 0x03;
    }

    switch (slot->reg_wf)
    {
    case 1:
    case 4:
    case 5:
        slot->maskzero = 0x200;
        break;
    case 3:
        slot->maskzero = 0x100;
        break;
    default:
        slot->maskzero = 0;
        break;
    }

    switch (slot->reg_wf)
    {
    case 4:
        slot->signpos = (31-8);  /* sigext of (phase & 0x100) */
        break;
    case 0:
    case 6:
    case 7:
        slot->signpos = (31-9);  /* sigext of (phase & 0x200) */
        break;
    default:
        slot->signpos = (31-16);  /* set "neg" to zero */
        break;
    }

    switch (slot->reg_wf)
    {
    case 4:
    case 5:
        slot->phaseshift = 1;
        break;
    case 6:
        slot->phaseshift = 16; /* set phase to zero and flag for non-sin wave */
        break;
    case 7:
        slot->phaseshift = 32; /* no shift (work by mod 32), but flag for non-sin wave */
        break;
    default:
        slot->phaseshift = 0;
        break;
    }
}

static void OPL3_SlotGeneratePhase(opl3_slot *slot, Bit16u phase)
{
    Bit32u neg, level;
    Bit8u  phaseshift;

    /* Fast paths for mute segments */
    if (phase & slot->maskzero)
    {
        slot->out = 0;
        return;
    }

    neg = (Bit32s)((Bit32u)phase << slot->signpos) >> 31;
    phaseshift = slot->phaseshift;
    level = slot->eg_out;

    phase <<= phaseshift;
    if (phaseshift <= 1)
    {
        level += logsinrom[phase & 0x1ff];
    }
    else
    {
        level += ((phase ^ neg) & 0x3ff) << 3;
    }
    slot->out = exprom[level & 0xff] >> (level >> 8) ^ neg;
}

static void OPL3_SlotGenerate(opl3_slot *slot)
{
    OPL3_SlotGeneratePhase(slot, (Bit16u)(slot->pg_phase >> 9) + *slot->mod);
}

static void OPL3_SlotGenerateZM(opl3_slot *slot)
{
    OPL3_SlotGeneratePhase(slot, (Bit16u)(slot->pg_phase >> 9));
}

static void OPL3_SlotCalcFB(opl3_slot *slot)
{
    if (slot->channel->fb != 0x00)
    {
        slot->fbmod = (slot->prout + slot->out) >> (0x09 - slot->channel->fb);
    }
    else
    {
        slot->fbmod = 0;
    }
    slot->prout = slot->out;
}

/*
 * Channel
 */

static void OPL3_ChannelSetupAlg(opl3_channel *channel);

static void OPL3_ChannelUpdateRhythm(opl3_chip *chip, Bit8u data)
{
    opl3_channel *channel6;
    opl3_channel *channel7;
    opl3_channel *channel8;
    Bit8u chnum;

    chip->rhy = data & 0x3f;
    if (chip->rhy & 0x20)
    {
        channel6 = &chip->channel[6];
        channel7 = &chip->channel[7];
        channel8 = &chip->channel[8];
        channel6->out[0] = &channel6->slotz[1]->out;
        channel6->out[1] = &channel6->slotz[1]->out;
        channel6->out[2] = &chip->zeromod;
        channel6->out[3] = &chip->zeromod;
        channel7->out[0] = &channel7->slotz[0]->out;
        channel7->out[1] = &channel7->slotz[0]->out;
        channel7->out[2] = &channel7->slotz[1]->out;
        channel7->out[3] = &channel7->slotz[1]->out;
        channel8->out[0] = &channel8->slotz[0]->out;
        channel8->out[1] = &channel8->slotz[0]->out;
        channel8->out[2] = &channel8->slotz[1]->out;
        channel8->out[3] = &channel8->slotz[1]->out;
        for (chnum = 6; chnum < 9; chnum++)
        {
            chip->channel[chnum].chtype = ch_drum;
        }
        OPL3_ChannelSetupAlg(channel6);
        /*hh*/
        if (chip->rhy & 0x01)
        {
            OPL3_EnvelopeKeyOn(channel7->slotz[0], egk_drum);
        }
        else
        {
            OPL3_EnvelopeKeyOff(channel7->slotz[0], egk_drum);
        }
        /*tc*/
        if (chip->rhy & 0x02)
        {
            OPL3_EnvelopeKeyOn(channel8->slotz[1], egk_drum);
        }
        else
        {
            OPL3_EnvelopeKeyOff(channel8->slotz[1], egk_drum);
        }
        /*tom*/
        if (chip->rhy & 0x04)
        {
            OPL3_EnvelopeKeyOn(channel8->slotz[0], egk_drum);
        }
        else
        {
            OPL3_EnvelopeKeyOff(channel8->slotz[0], egk_drum);
        }
        /*sd*/
        if (chip->rhy & 0x08)
        {
            OPL3_EnvelopeKeyOn(channel7->slotz[1], egk_drum);
        }
        else
        {
            OPL3_EnvelopeKeyOff(channel7->slotz[1], egk_drum);
        }
        /*bd*/
        if (chip->rhy & 0x10)
        {
            OPL3_EnvelopeKeyOn(channel6->slotz[0], egk_drum);
            OPL3_EnvelopeKeyOn(channel6->slotz[1], egk_drum);
        }
        else
        {
            OPL3_EnvelopeKeyOff(channel6->slotz[0], egk_drum);
            OPL3_EnvelopeKeyOff(channel6->slotz[1], egk_drum);
        }
    }
    else
    {
        for (chnum = 6; chnum < 9; chnum++)
        {
            chip->channel[chnum].chtype = ch_2op;
            OPL3_ChannelSetupAlg(&chip->channel[chnum]);
            OPL3_EnvelopeKeyOff(chip->channel[chnum].slotz[0], egk_drum);
            OPL3_EnvelopeKeyOff(chip->channel[chnum].slotz[1], egk_drum);
        }
    }
}

static void OPL3_ChannelWriteA0(opl3_channel *channel, Bit8u data)
{
    if (channel->chip->newm && channel->chtype == ch_4op2)
    {
        return;
    }
    channel->f_num = (channel->f_num & 0x300) | data;
    channel->ksv = (channel->block << 1)
                 | ((channel->f_num >> (0x09 - channel->chip->nts)) & 0x01);
    OPL3_EnvelopeUpdateKSL(channel->slotz[0]);
    OPL3_EnvelopeUpdateKSL(channel->slotz[1]);
    OPL3_EnvelopeUpdateRate(channel->slotz[0]);
    OPL3_EnvelopeUpdateRate(channel->slotz[1]);
    if (channel->chip->newm && channel->chtype == ch_4op)
    {
        channel->pair->f_num = channel->f_num;
        channel->pair->ksv = channel->ksv;
        OPL3_EnvelopeUpdateKSL(channel->pair->slotz[0]);
        OPL3_EnvelopeUpdateKSL(channel->pair->slotz[1]);
        OPL3_EnvelopeUpdateRate(channel->pair->slotz[0]);
        OPL3_EnvelopeUpdateRate(channel->pair->slotz[1]);
    }
}

static void OPL3_ChannelWriteB0(opl3_channel *channel, Bit8u data)
{
    if (channel->chip->newm && channel->chtype == ch_4op2)
    {
        return;
    }
    channel->f_num = (channel->f_num & 0xff) | ((data & 0x03) << 8);
    channel->block = (data >> 2) & 0x07;
    channel->ksv = (channel->block << 1)
                 | ((channel->f_num >> (0x09 - channel->chip->nts)) & 0x01);
    OPL3_EnvelopeUpdateKSL(channel->slotz[0]);
    OPL3_EnvelopeUpdateKSL(channel->slotz[1]);
    OPL3_EnvelopeUpdateRate(channel->slotz[0]);
    OPL3_EnvelopeUpdateRate(channel->slotz[1]);
    if (channel->chip->newm && channel->chtype == ch_4op)
    {
        channel->pair->f_num = channel->f_num;
        channel->pair->block = channel->block;
        channel->pair->ksv = channel->ksv;
        OPL3_EnvelopeUpdateKSL(channel->pair->slotz[0]);
        OPL3_EnvelopeUpdateKSL(channel->pair->slotz[1]);
        OPL3_EnvelopeUpdateRate(channel->pair->slotz[0]);
        OPL3_EnvelopeUpdateRate(channel->pair->slotz[1]);
    }
}

static void OPL3_ChannelSetupAlg(opl3_channel *channel)
{
    if (channel->chtype == ch_drum)
    {
        switch (channel->alg & 0x01)
        {
        case 0x00:
            channel->slotz[0]->mod = &channel->slotz[0]->fbmod;
            channel->slotz[1]->mod = &channel->slotz[0]->out;
            break;
        case 0x01:
            channel->slotz[0]->mod = &channel->slotz[0]->fbmod;
            channel->slotz[1]->mod = &channel->chip->zeromod;
            break;
        }
        return;
    }
    if (channel->alg & 0x08)
    {
        return;
    }
    if (channel->alg & 0x04)
    {
        channel->pair->out[0] = &channel->chip->zeromod;
        channel->pair->out[1] = &channel->chip->zeromod;
        channel->pair->out[2] = &channel->chip->zeromod;
        channel->pair->out[3] = &channel->chip->zeromod;
        switch (channel->alg & 0x03)
        {
        case 0x00:
            channel->pair->slotz[0]->mod = &channel->pair->slotz[0]->fbmod;
            channel->pair->slotz[1]->mod = &channel->pair->slotz[0]->out;
            channel->slotz[0]->mod = &channel->pair->slotz[1]->out;
            channel->slotz[1]->mod = &channel->slotz[0]->out;
            channel->out[0] = &channel->slotz[1]->out;
            channel->out[1] = &channel->chip->zeromod;
            channel->out[2] = &channel->chip->zeromod;
            channel->out[3] = &channel->chip->zeromod;
            break;
        case 0x01:
            channel->pair->slotz[0]->mod = &channel->pair->slotz[0]->fbmod;
            channel->pair->slotz[1]->mod = &channel->pair->slotz[0]->out;
            channel->slotz[0]->mod = &channel->chip->zeromod;
            channel->slotz[1]->mod = &channel->slotz[0]->out;
            channel->out[0] = &channel->pair->slotz[1]->out;
            channel->out[1] = &channel->slotz[1]->out;
            channel->out[2] = &channel->chip->zeromod;
            channel->out[3] = &channel->chip->zeromod;
            break;
        case 0x02:
            channel->pair->slotz[0]->mod = &channel->pair->slotz[0]->fbmod;
            channel->pair->slotz[1]->mod = &channel->chip->zeromod;
            channel->slotz[0]->mod = &channel->pair->slotz[1]->out;
            channel->slotz[1]->mod = &channel->slotz[0]->out;
            channel->out[0] = &channel->pair->slotz[0]->out;
            channel->out[1] = &channel->slotz[1]->out;
            channel->out[2] = &channel->chip->zeromod;
            channel->out[3] = &channel->chip->zeromod;
            break;
        case 0x03:
            channel->pair->slotz[0]->mod = &channel->pair->slotz[0]->fbmod;
            channel->pair->slotz[1]->mod = &channel->chip->zeromod;
            channel->slotz[0]->mod = &channel->pair->slotz[1]->out;
            channel->slotz[1]->mod = &channel->chip->zeromod;
            channel->out[0] = &channel->pair->slotz[0]->out;
            channel->out[1] = &channel->slotz[0]->out;
            channel->out[2] = &channel->slotz[1]->out;
            channel->out[3] = &channel->chip->zeromod;
            break;
        }
    }
    else
    {
        switch (channel->alg & 0x01)
        {
        case 0x00:
            channel->slotz[0]->mod = &channel->slotz[0]->fbmod;
            channel->slotz[1]->mod = &channel->slotz[0]->out;
            channel->out[0] = &channel->slotz[1]->out;
            channel->out[1] = &channel->chip->zeromod;
            channel->out[2] = &channel->chip->zeromod;
            channel->out[3] = &channel->chip->zeromod;
            break;
        case 0x01:
            channel->slotz[0]->mod = &channel->slotz[0]->fbmod;
            channel->slotz[1]->mod = &channel->chip->zeromod;
            channel->out[0] = &channel->slotz[0]->out;
            channel->out[1] = &channel->slotz[1]->out;
            channel->out[2] = &channel->chip->zeromod;
            channel->out[3] = &channel->chip->zeromod;
            break;
        }
    }
}

static void OPL3_ChannelWriteC0(opl3_channel *channel, Bit8u data)
{
    channel->fb = (data & 0x0e) >> 1;
    channel->con = data & 0x01;
    channel->alg = channel->con;
    if (channel->chip->newm)
    {
        if (channel->chtype == ch_4op)
        {
            channel->pair->alg = 0x04 | (channel->con << 1) | (channel->pair->con);
            channel->alg = 0x08;
            OPL3_ChannelSetupAlg(channel->pair);
        }
        else if (channel->chtype == ch_4op2)
        {
            channel->alg = 0x04 | (channel->pair->con << 1) | (channel->con);
            channel->pair->alg = 0x08;
            OPL3_ChannelSetupAlg(channel);
        }
        else
        {
            OPL3_ChannelSetupAlg(channel);
        }
    }
    else
    {
        OPL3_ChannelSetupAlg(channel);
    }
    if (channel->chip->newm)
    {
        channel->cha = ((data >> 4) & 0x01) ? ~0 : 0;
        channel->chb = ((data >> 5) & 0x01) ? ~0 : 0;
    }
    else
    {
        channel->cha = channel->chb = ~0;
    }
}

static void OPL3_ChannelKeyOn(opl3_channel *channel)
{
    if (channel->chip->newm)
    {
        if (channel->chtype == ch_4op)
        {
            OPL3_EnvelopeKeyOn(channel->slotz[0], egk_norm);
            OPL3_EnvelopeKeyOn(channel->slotz[1], egk_norm);
            OPL3_EnvelopeKeyOn(channel->pair->slotz[0], egk_norm);
            OPL3_EnvelopeKeyOn(channel->pair->slotz[1], egk_norm);
        }
        else if (channel->chtype == ch_2op || channel->chtype == ch_drum)
        {
            OPL3_EnvelopeKeyOn(channel->slotz[0], egk_norm);
            OPL3_EnvelopeKeyOn(channel->slotz[1], egk_norm);
        }
    }
    else
    {
        OPL3_EnvelopeKeyOn(channel->slotz[0], egk_norm);
        OPL3_EnvelopeKeyOn(channel->slotz[1], egk_norm);
    }
}

static void OPL3_ChannelKeyOff(opl3_channel *channel)
{
    if (channel->chip->newm)
    {
        if (channel->chtype == ch_4op)
        {
            OPL3_EnvelopeKeyOff(channel->slotz[0], egk_norm);
            OPL3_EnvelopeKeyOff(channel->slotz[1], egk_norm);
            OPL3_EnvelopeKeyOff(channel->pair->slotz[0], egk_norm);
            OPL3_EnvelopeKeyOff(channel->pair->slotz[1], egk_norm);
        }
        else if (channel->chtype == ch_2op || channel->chtype == ch_drum)
        {
            OPL3_EnvelopeKeyOff(channel->slotz[0], egk_norm);
            OPL3_EnvelopeKeyOff(channel->slotz[1], egk_norm);
        }
    }
    else
    {
        OPL3_EnvelopeKeyOff(channel->slotz[0], egk_norm);
        OPL3_EnvelopeKeyOff(channel->slotz[1], egk_norm);
    }
}

static void OPL3_ChannelSet4Op(opl3_chip *chip, Bit8u data)
{
    Bit8u bit;
    Bit8u chnum;
    for (bit = 0; bit < 6; bit++)
    {
        chnum = bit;
        if (bit >= 3)
        {
            chnum += 9 - 3;
        }
        if ((data >> bit) & 0x01)
        {
            chip->channel[chnum].chtype = ch_4op;
            chip->channel[chnum + 3].chtype = ch_4op2;
        }
        else
        {
            chip->channel[chnum].chtype = ch_2op;
            chip->channel[chnum + 3].chtype = ch_2op;
        }
    }
}

static Bit16s OPL3_ClipSample(Bit32s sample)
{
    if (sample > 32767)
    {
        sample = 32767;
    }
    else if (sample < -32768)
    {
        sample = -32768;
    }
    return (Bit16s)sample;
}

static void OPL3_GenerateRhythm1(opl3_chip *chip)
{
    opl3_channel *channel6;
    opl3_channel *channel7;
    opl3_channel *channel8;
    Bit16u phase14;
    Bit16u phase17;
    Bit16u phase;
    Bit16u phasebit;

    channel6 = &chip->channel[6];
    channel7 = &chip->channel[7];
    channel8 = &chip->channel[8];
    OPL3_SlotGenerate(channel6->slotz[0]);
    phase14 = (channel7->slotz[0]->pg_phase >> 9) & 0x3ff;
    phase17 = (channel8->slotz[1]->pg_phase >> 9) & 0x3ff;
    phase = 0x00;
    /*hh tc phase bit*/
    phasebit = ((phase14 & 0x08) | (((phase14 >> 5) ^ phase14) & 0x04)
             | (((phase17 >> 2) ^ phase17) & 0x08)) ? 0x01 : 0x00;
    /*hh*/
    phase = (phasebit << 9)
          | (0x34 << ((phasebit ^ (chip->noise & 0x01)) << 1));
    OPL3_SlotGeneratePhase(channel7->slotz[0], phase);
    /*tt*/
    OPL3_SlotGenerateZM(channel8->slotz[0]);
}

static void OPL3_GenerateRhythm2(opl3_chip *chip)
{
    opl3_channel *channel6;
    opl3_channel *channel7;
    opl3_channel *channel8;
    Bit16u phase14;
    Bit16u phase17;
    Bit16u phase;
    Bit16u phasebit;

    channel6 = &chip->channel[6];
    channel7 = &chip->channel[7];
    channel8 = &chip->channel[8];
    OPL3_SlotGenerate(channel6->slotz[1]);
    phase14 = (channel7->slotz[0]->pg_phase >> 9) & 0x3ff;
    phase17 = (channel8->slotz[1]->pg_phase >> 9) & 0x3ff;
    phase = 0x00;
    /*hh tc phase bit*/
    phasebit = ((phase14 & 0x08) | (((phase14 >> 5) ^ phase14) & 0x04)
             | (((phase17 >> 2) ^ phase17) & 0x08)) ? 0x01 : 0x00;
    /*sd*/
    phase = (0x100 << ((phase14 >> 8) & 0x01)) ^ ((chip->noise & 0x01) << 8);
    OPL3_SlotGeneratePhase(channel7->slotz[1], phase);
    /*tc*/
    phase = 0x100 | (phasebit << 9);
    OPL3_SlotGeneratePhase(channel8->slotz[1], phase);
}

void OPL3v17_Generate(opl3_chip *chip, Bit16s *buf)
{
    Bit8u ii;
    Bit8u jj;
    Bit16s accm;

    buf[1] = OPL3_ClipSample(chip->mixbuff[1]);

    for (ii = 0; ii < 12; ii++)
    {
        OPL3_SlotCalcFB(&chip->chipslot[ii]);
        OPL3_PhaseGenerate(&chip->chipslot[ii]);
        OPL3_EnvelopeCalc(&chip->chipslot[ii]);
        OPL3_SlotGenerate(&chip->chipslot[ii]);
    }

    for (ii = 12; ii < 15; ii++)
    {
        OPL3_SlotCalcFB(&chip->chipslot[ii]);
        OPL3_PhaseGenerate(&chip->chipslot[ii]);
        OPL3_EnvelopeCalc(&chip->chipslot[ii]);
    }

    if (chip->rhy & 0x20)
    {
        OPL3_GenerateRhythm1(chip);
    }
    else
    {
        OPL3_SlotGenerate(&chip->chipslot[12]);
        OPL3_SlotGenerate(&chip->chipslot[13]);
        OPL3_SlotGenerate(&chip->chipslot[14]);
    }

    chip->mixbuff[0] = 0;
    for (ii = 0; ii < 18; ii++)
    {
        accm = 0;
        for (jj = 0; jj < 4; jj++)
        {
            accm += *chip->channel[ii].out[jj];
        }
        chip->mixbuff[0] += (Bit16s)((accm * chip->channel[ii].chl / 65535) & chip->channel[ii].cha);
    }

    for (ii = 15; ii < 18; ii++)
    {
        OPL3_SlotCalcFB(&chip->chipslot[ii]);
        OPL3_PhaseGenerate(&chip->chipslot[ii]);
        OPL3_EnvelopeCalc(&chip->chipslot[ii]);
    }

    if (chip->rhy & 0x20)
    {
        OPL3_GenerateRhythm2(chip);
    }
    else
    {
        OPL3_SlotGenerate(&chip->chipslot[15]);
        OPL3_SlotGenerate(&chip->chipslot[16]);
        OPL3_SlotGenerate(&chip->chipslot[17]);
    }

    buf[0] = OPL3_ClipSample(chip->mixbuff[0]);

    for (ii = 18; ii < 33; ii++)
    {
        OPL3_SlotCalcFB(&chip->chipslot[ii]);
        OPL3_PhaseGenerate(&chip->chipslot[ii]);
        OPL3_EnvelopeCalc(&chip->chipslot[ii]);
        OPL3_SlotGenerate(&chip->chipslot[ii]);
    }

    chip->mixbuff[1] = 0;
    for (ii = 0; ii < 18; ii++)
    {
        accm = 0;
        for (jj = 0; jj < 4; jj++)
        {
            accm += *chip->channel[ii].out[jj];
        }
        chip->mixbuff[1] += (Bit16s)((accm * chip->channel[ii].chr / 65535) & chip->channel[ii].chb);
    }

    for (ii = 33; ii < 36; ii++)
    {
        OPL3_SlotCalcFB(&chip->chipslot[ii]);
        OPL3_PhaseGenerate(&chip->chipslot[ii]);
        OPL3_EnvelopeCalc(&chip->chipslot[ii]);
        OPL3_SlotGenerate(&chip->chipslot[ii]);
    }

    OPL3_NoiseGenerate(chip);

    if ((chip->timer & 0x3f) == 0x3f)
    {
        chip->tremolopos = (chip->tremolopos + 1) % 210;
    }
    if (chip->tremolopos < 105)
    {
        chip->tremolo = chip->tremolopos >> chip->tremoloshift;
    }
    else
    {
        chip->tremolo = (210 - chip->tremolopos) >> chip->tremoloshift;
    }

    if ((chip->timer & 0x3ff) == 0x3ff)
    {
        chip->vibpos = (chip->vibpos + 1) & 7;
    }

    chip->timer++;

    while (chip->writebuf[chip->writebuf_cur].time <= chip->writebuf_samplecnt)
    {
        if (!(chip->writebuf[chip->writebuf_cur].reg & 0x200))
        {
            break;
        }
        chip->writebuf[chip->writebuf_cur].reg &= 0x1ff;
        OPL3v17_WriteReg(chip, chip->writebuf[chip->writebuf_cur].reg,
                      chip->writebuf[chip->writebuf_cur].data);
        chip->writebuf_cur = (chip->writebuf_cur + 1) % OPL_WRITEBUF_SIZE;
    }
    chip->writebuf_samplecnt++;
}

void OPL3v17_GenerateResampled(opl3_chip *chip, Bit16s *buf)
{
    while (chip->samplecnt >= chip->rateratio)
    {
        chip->oldsamples[0] = chip->samples[0];
        chip->oldsamples[1] = chip->samples[1];
        OPL3v17_Generate(chip, chip->samples);
        chip->samplecnt -= chip->rateratio;
    }
    buf[0] = (Bit16s)((chip->oldsamples[0] * (chip->rateratio - chip->samplecnt)
                     + chip->samples[0] * chip->samplecnt) / chip->rateratio);
    buf[1] = (Bit16s)((chip->oldsamples[1] * (chip->rateratio - chip->samplecnt)
                     + chip->samples[1] * chip->samplecnt) / chip->rateratio);
    chip->samplecnt += 1 << RSM_FRAC;
}

void OPL3v17_Reset(opl3_chip *chip, Bit32u samplerate)
{
    Bit8u slotnum;
    Bit8u channum;

    memset(chip, 0, sizeof(opl3_chip));
    for (slotnum = 0; slotnum < 36; slotnum++)
    {
        chip->chipslot[slotnum].chip = chip;
        chip->chipslot[slotnum].mod = &chip->zeromod;
        chip->chipslot[slotnum].eg_rout = 0x1ff;
        chip->chipslot[slotnum].eg_out = 0x1ff << 3;
        chip->chipslot[slotnum].eg_gen = envelope_gen_num_off;
        chip->chipslot[slotnum].trem = (Bit8u*)&chip->zeromod;
        chip->chipslot[slotnum].signpos = (31-9);  /* for wf=0 need use sigext of (phase & 0x200) */
    }
    for (channum = 0; channum < 18; channum++)
    {
        chip->channel[channum].slotz[0] = &chip->chipslot[ch_slot[channum]];
        chip->channel[channum].slotz[1] = &chip->chipslot[ch_slot[channum] + 3];
        chip->chipslot[ch_slot[channum]].channel = &chip->channel[channum];
        chip->chipslot[ch_slot[channum] + 3].channel = &chip->channel[channum];
        if ((channum % 9) < 3)
        {
            chip->channel[channum].pair = &chip->channel[channum + 3];
        }
        else if ((channum % 9) < 6)
        {
            chip->channel[channum].pair = &chip->channel[channum - 3];
        }
        chip->channel[channum].chip = chip;
        chip->channel[channum].out[0] = &chip->zeromod;
        chip->channel[channum].out[1] = &chip->zeromod;
        chip->channel[channum].out[2] = &chip->zeromod;
        chip->channel[channum].out[3] = &chip->zeromod;
        chip->channel[channum].chtype = ch_2op;
        chip->channel[channum].cha = 0xffff;
        chip->channel[channum].chb = 0xffff;
        chip->channel[channum].chl = 46340;
        chip->channel[channum].chr = 46340;
        OPL3_ChannelSetupAlg(&chip->channel[channum]);
    }
    chip->noise = 0x306600;
    chip->rateratio = (samplerate << RSM_FRAC) / 49716;
    chip->tremoloshift = 4;
    chip->vibshift = 1;
}

static void OPL3v17_ChannelWritePan(opl3_channel *channel, Bit8u data)
{
    channel->chl = panlawtable[data & 0x7F];
    channel->chr = panlawtable[0x7F - (data & 0x7F)];
}

void OPL3v17_WritePan(opl3_chip *chip, Bit16u reg, Bit8u v)
{
    Bit8u high = (reg >> 8) & 0x01;
    Bit8u regm = reg & 0xff;
    OPL3v17_ChannelWritePan(&chip->channel[9 * high + (regm & 0x0f)], v);
}

void OPL3v17_WriteReg(opl3_chip *chip, Bit16u reg, Bit8u v)
{
    Bit8u high = (reg >> 8) & 0x01;
    Bit8u regm = reg & 0xff;
    switch (regm & 0xf0)
    {
    case 0x00:
        if (high)
        {
            switch (regm & 0x0f)
            {
            case 0x04:
                OPL3_ChannelSet4Op(chip, v);
                break;
            case 0x05:
                chip->newm = v & 0x01;
                break;
            }
        }
        else
        {
            switch (regm & 0x0f)
            {
            case 0x08:
                chip->nts = (v >> 6) & 0x01;
                break;
            }
        }
        break;
    case 0x20:
    case 0x30:
        if (ad_slot[regm & 0x1f] >= 0)
        {
            OPL3_SlotWrite20(&chip->chipslot[18 * high + ad_slot[regm & 0x1f]], v);
        }
        break;
    case 0x40:
    case 0x50:
        if (ad_slot[regm & 0x1f] >= 0)
        {
            OPL3_SlotWrite40(&chip->chipslot[18 * high + ad_slot[regm & 0x1f]], v);
        }
        break;
    case 0x60:
    case 0x70:
        if (ad_slot[regm & 0x1f] >= 0)
        {
            OPL3_SlotWrite60(&chip->chipslot[18 * high + ad_slot[regm & 0x1f]], v);
        }
        break;
    case 0x80:
    case 0x90:
        if (ad_slot[regm & 0x1f] >= 0)
        {
            OPL3_SlotWrite80(&chip->chipslot[18 * high + ad_slot[regm & 0x1f]], v);
        }
        break;
    case 0xe0:
    case 0xf0:
        if (ad_slot[regm & 0x1f] >= 0)
        {
            OPL3_SlotWriteE0(&chip->chipslot[18 * high + ad_slot[regm & 0x1f]], v);
        }
        break;
    case 0xa0:
        if ((regm & 0x0f) < 9)
        {
            OPL3_ChannelWriteA0(&chip->channel[9 * high + (regm & 0x0f)], v);
        }
        break;
    case 0xb0:
        if (regm == 0xbd && !high)
        {
            chip->tremoloshift = (((v >> 7) ^ 1) << 1) + 2;
            chip->vibshift = ((v >> 6) & 0x01) ^ 1;
            OPL3_ChannelUpdateRhythm(chip, v);
        }
        else if ((regm & 0x0f) < 9)
        {
            OPL3_ChannelWriteB0(&chip->channel[9 * high + (regm & 0x0f)], v);
            if (v & 0x20)
            {
                OPL3_ChannelKeyOn(&chip->channel[9 * high + (regm & 0x0f)]);
            }
            else
            {
                OPL3_ChannelKeyOff(&chip->channel[9 * high + (regm & 0x0f)]);
            }
        }
        break;
    case 0xc0:
        if ((regm & 0x0f) < 9)
        {
            OPL3_ChannelWriteC0(&chip->channel[9 * high + (regm & 0x0f)], v);
        }
        break;
    }
}

void OPL3v17_WriteRegBuffered(opl3_chip *chip, Bit16u reg, Bit8u v)
{
    Bit64u time1, time2;

    if (chip->writebuf[chip->writebuf_last].reg & 0x200)
    {
        OPL3v17_WriteReg(chip, chip->writebuf[chip->writebuf_last].reg & 0x1ff,
                      chip->writebuf[chip->writebuf_last].data);

        chip->writebuf_cur = (chip->writebuf_last + 1) % OPL_WRITEBUF_SIZE;
        chip->writebuf_samplecnt = chip->writebuf[chip->writebuf_last].time;
    }

    chip->writebuf[chip->writebuf_last].reg = reg | 0x200;
    chip->writebuf[chip->writebuf_last].data = v;
    time1 = chip->writebuf_lasttime + OPL_WRITEBUF_DELAY;
    time2 = chip->writebuf_samplecnt;

    if (time1 < time2)
    {
        time1 = time2;
    }

    chip->writebuf[chip->writebuf_last].time = time1;
    chip->writebuf_lasttime = time1;
    chip->writebuf_last = (chip->writebuf_last + 1) % OPL_WRITEBUF_SIZE;
}

void OPL3v17_GenerateStream(opl3_chip *chip, Bit16s *sndptr, Bit32u numsamples)
{
    Bit32u i;

    for(i = 0; i < numsamples; i++)
    {
        OPL3v17_GenerateResampled(chip, sndptr);
        sndptr += 2;
    }
}

#define OPL3_MIN(A, B)          (((A) > (B)) ? (B) : (A))
#define OPL3_MAX(A, B)          (((A) < (B)) ? (B) : (A))
#define OPL3_CLAMP(V, MIN, MAX) OPL3_MAX(OPL3_MIN(V, MAX), MIN)

void OPL3v17_GenerateStreamMix(opl3_chip *chip, Bit16s *sndptr, Bit32u numsamples)
{
    Bit32u i;
    Bit16s sample[2];
    Bit32s mix[2];

    for(i = 0; i < numsamples; i++)
    {
        OPL3v17_GenerateResampled(chip, sample);
        mix[0] = sndptr[0] + sample[0];
        mix[1] = sndptr[1] + sample[1];
        sndptr[0] = OPL3_CLAMP(mix[0], INT16_MIN, INT16_MAX);
        sndptr[1] = OPL3_CLAMP(mix[1], INT16_MIN, INT16_MAX);
        sndptr += 2;
    }
}

