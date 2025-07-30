/* Nuked OPL3
 * Copyright (C) 2013-2020 Nuke.YKT
 *
 * This file is part of Nuked OPL3.
 *
 * Nuked OPL3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1
 * of the License, or (at your option) any later version.
 *
 * Nuked OPL3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Nuked OPL3. If not, see <https://www.gnu.org/licenses/>.

 *  Nuked OPL3 emulator.
 *  Thanks:
 *      MAME Development Team(Jarek Burczynski, Tatsuyuki Satoh):
 *          Feedback and Rhythm part calculation information.
 *      forums.submarine.org.uk(carbon14, opl3):
 *          Tremolo and phase generator calculation information.
 *      OPLx decapsulated(Matthew Gambrell, Olli Niemitalo):
 *          OPL2 ROMs.
 *      siliconpr0n.org(John McMaster, digshadow):
 *          YMF262 and VRC VII decaps and die shots.
 *
 * version: 1.8
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nukedopl3.h"

#if OPL_ENABLE_STEREOEXT && !defined OPL_SIN
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES 1
#endif
#include <math.h>
/* input: [0, 256), output: [0, 65536] */
#define OPL_SIN(x) ((int32_t)(sin((x) * M_PI / 512.0) * 65536.0))
#endif

/* Quirk: Some FM channels are output one sample later on the left side than the right. */
#ifndef OPL_QUIRK_CHANNELSAMPLEDELAY
#define OPL_QUIRK_CHANNELSAMPLEDELAY (!OPL_ENABLE_STEREOEXT)
#endif

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


#if OPL_FAST_WAVEGEN
/*
    logsin table
*/

static const uint16_t logsinrom[512] = {
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
    exp table
*/

static const uint16_t exprom[256] = {
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
#else
/*
    logsin table
*/

static const uint16_t logsinrom[256] = {
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
    0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000
};

/*
    exp table
*/

static const uint16_t exprom[256] = {
    0x7fa, 0x7f5, 0x7ef, 0x7ea, 0x7e4, 0x7df, 0x7da, 0x7d4,
    0x7cf, 0x7c9, 0x7c4, 0x7bf, 0x7b9, 0x7b4, 0x7ae, 0x7a9,
    0x7a4, 0x79f, 0x799, 0x794, 0x78f, 0x78a, 0x784, 0x77f,
    0x77a, 0x775, 0x770, 0x76a, 0x765, 0x760, 0x75b, 0x756,
    0x751, 0x74c, 0x747, 0x742, 0x73d, 0x738, 0x733, 0x72e,
    0x729, 0x724, 0x71f, 0x71a, 0x715, 0x710, 0x70b, 0x706,
    0x702, 0x6fd, 0x6f8, 0x6f3, 0x6ee, 0x6e9, 0x6e5, 0x6e0,
    0x6db, 0x6d6, 0x6d2, 0x6cd, 0x6c8, 0x6c4, 0x6bf, 0x6ba,
    0x6b5, 0x6b1, 0x6ac, 0x6a8, 0x6a3, 0x69e, 0x69a, 0x695,
    0x691, 0x68c, 0x688, 0x683, 0x67f, 0x67a, 0x676, 0x671,
    0x66d, 0x668, 0x664, 0x65f, 0x65b, 0x657, 0x652, 0x64e,
    0x649, 0x645, 0x641, 0x63c, 0x638, 0x634, 0x630, 0x62b,
    0x627, 0x623, 0x61e, 0x61a, 0x616, 0x612, 0x60e, 0x609,
    0x605, 0x601, 0x5fd, 0x5f9, 0x5f5, 0x5f0, 0x5ec, 0x5e8,
    0x5e4, 0x5e0, 0x5dc, 0x5d8, 0x5d4, 0x5d0, 0x5cc, 0x5c8,
    0x5c4, 0x5c0, 0x5bc, 0x5b8, 0x5b4, 0x5b0, 0x5ac, 0x5a8,
    0x5a4, 0x5a0, 0x59c, 0x599, 0x595, 0x591, 0x58d, 0x589,
    0x585, 0x581, 0x57e, 0x57a, 0x576, 0x572, 0x56f, 0x56b,
    0x567, 0x563, 0x560, 0x55c, 0x558, 0x554, 0x551, 0x54d,
    0x549, 0x546, 0x542, 0x53e, 0x53b, 0x537, 0x534, 0x530,
    0x52c, 0x529, 0x525, 0x522, 0x51e, 0x51b, 0x517, 0x514,
    0x510, 0x50c, 0x509, 0x506, 0x502, 0x4ff, 0x4fb, 0x4f8,
    0x4f4, 0x4f1, 0x4ed, 0x4ea, 0x4e7, 0x4e3, 0x4e0, 0x4dc,
    0x4d9, 0x4d6, 0x4d2, 0x4cf, 0x4cc, 0x4c8, 0x4c5, 0x4c2,
    0x4be, 0x4bb, 0x4b8, 0x4b5, 0x4b1, 0x4ae, 0x4ab, 0x4a8,
    0x4a4, 0x4a1, 0x49e, 0x49b, 0x498, 0x494, 0x491, 0x48e,
    0x48b, 0x488, 0x485, 0x482, 0x47e, 0x47b, 0x478, 0x475,
    0x472, 0x46f, 0x46c, 0x469, 0x466, 0x463, 0x460, 0x45d,
    0x45a, 0x457, 0x454, 0x451, 0x44e, 0x44b, 0x448, 0x445,
    0x442, 0x43f, 0x43c, 0x439, 0x436, 0x433, 0x430, 0x42d,
    0x42a, 0x428, 0x425, 0x422, 0x41f, 0x41c, 0x419, 0x416,
    0x414, 0x411, 0x40e, 0x40b, 0x408, 0x406, 0x403, 0x400
};
#endif

/*
    freq mult table multiplied by 2

    1/2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15
*/

static const uint8_t mt[16] = {
    1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30
};

/*
    ksl table
*/

static const uint8_t kslrom[16] = {
    0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64
};

static const uint8_t kslshift[4] = {
    8, 1, 2, 0
};

/*
    envelope generator constants
*/

static const uint8_t eg_incstep[4][4] = {
    { 0, 0, 0, 0 },
    { 1, 0, 0, 0 },
    { 1, 0, 1, 0 },
    { 1, 1, 1, 0 }
};

/*
    address decoding
*/

static const int8_t ad_slot[0x20] = {
    0, 1, 2, 3, 4, 5, -1, -1, 6, 7, 8, 9, 10, 11, -1, -1,
    12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static const uint8_t ch_slot[18] = {
    0, 1, 2, 6, 7, 8, 12, 13, 14, 18, 19, 20, 24, 25, 26, 30, 31, 32
};

#if OPL_ENABLE_STEREOEXT
/*
    stereo extension panning table
*/

static int32_t panpot_lut[256];
static uint8_t panpot_lut_build = 0;
#endif

/*
    Pan law table
*/

static const uint16_t panlawtable[] =
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
    Envelope generator
*/

#if !OPL_FAST_WAVEGEN
typedef int16_t(*envelope_sinfunc)(uint16_t phase, uint16_t envelope);
typedef void(*envelope_genfunc)(opl3_slot *slott);

static int16_t OPL3_EnvelopeCalcExp(uint32_t level)
{
    if (level > 0x1fff)
    {
        level = 0x1fff;
    }
    return (exprom[level & 0xffu] << 1) >> (level >> 8);
}

static int16_t OPL3_EnvelopeCalcSin0(uint16_t phase, uint16_t envelope)
{
    uint16_t out = 0;
    uint16_t neg = 0;
    phase &= 0x3ff;
    if (phase & 0x200)
    {
        neg = 0xffff;
    }
    if (phase & 0x100)
    {
        out = logsinrom[(phase & 0xffu) ^ 0xffu];
    }
    else
    {
        out = logsinrom[phase & 0xffu];
    }
    return OPL3_EnvelopeCalcExp(out + (envelope << 3)) ^ neg;
}

static int16_t OPL3_EnvelopeCalcSin1(uint16_t phase, uint16_t envelope)
{
    uint16_t out = 0;
    phase &= 0x3ff;
    if (phase & 0x200)
    {
        out = 0x1000;
    }
    else if (phase & 0x100)
    {
        out = logsinrom[(phase & 0xffu) ^ 0xffu];
    }
    else
    {
        out = logsinrom[phase & 0xffu];
    }
    return OPL3_EnvelopeCalcExp(out + (envelope << 3));
}

static int16_t OPL3_EnvelopeCalcSin2(uint16_t phase, uint16_t envelope)
{
    uint16_t out = 0;
    phase &= 0x3ff;
    if (phase & 0x100)
    {
        out = logsinrom[(phase & 0xffu) ^ 0xffu];
    }
    else
    {
        out = logsinrom[phase & 0xffu];
    }
    return OPL3_EnvelopeCalcExp(out + (envelope << 3));
}

static int16_t OPL3_EnvelopeCalcSin3(uint16_t phase, uint16_t envelope)
{
    uint16_t out = 0;
    phase &= 0x3ff;
    if (phase & 0x100)
    {
        out = 0x1000;
    }
    else
    {
        out = logsinrom[phase & 0xffu];
    }
    return OPL3_EnvelopeCalcExp(out + (envelope << 3));
}

static int16_t OPL3_EnvelopeCalcSin4(uint16_t phase, uint16_t envelope)
{
    uint16_t out = 0;
    uint16_t neg = 0;
    phase &= 0x3ff;
    if ((phase & 0x300) == 0x100)
    {
        neg = 0xffff;
    }
    if (phase & 0x200)
    {
        out = 0x1000;
    }
    else if (phase & 0x80)
    {
        out = logsinrom[((phase ^ 0xffu) << 1u) & 0xffu];
    }
    else
    {
        out = logsinrom[(phase << 1u) & 0xffu];
    }
    return OPL3_EnvelopeCalcExp(out + (envelope << 3)) ^ neg;
}

static int16_t OPL3_EnvelopeCalcSin5(uint16_t phase, uint16_t envelope)
{
    uint16_t out = 0;
    phase &= 0x3ff;
    if (phase & 0x200)
    {
        out = 0x1000;
    }
    else if (phase & 0x80)
    {
        out = logsinrom[((phase ^ 0xffu) << 1u) & 0xffu];
    }
    else
    {
        out = logsinrom[(phase << 1u) & 0xffu];
    }
    return OPL3_EnvelopeCalcExp(out + (envelope << 3));
}

static int16_t OPL3_EnvelopeCalcSin6(uint16_t phase, uint16_t envelope)
{
    uint16_t neg = 0;
    phase &= 0x3ff;
    if (phase & 0x200)
    {
        neg = 0xffff;
    }
    return OPL3_EnvelopeCalcExp(envelope << 3) ^ neg;
}

static int16_t OPL3_EnvelopeCalcSin7(uint16_t phase, uint16_t envelope)
{
    uint16_t out = 0;
    uint16_t neg = 0;
    phase &= 0x3ff;
    if (phase & 0x200)
    {
        neg = 0xffff;
        phase = (phase & 0x1ff) ^ 0x1ff;
    }
    out = phase << 3;
    return OPL3_EnvelopeCalcExp(out + (envelope << 3)) ^ neg;
}

static const envelope_sinfunc envelope_sin[8] = {
    OPL3_EnvelopeCalcSin0,
    OPL3_EnvelopeCalcSin1,
    OPL3_EnvelopeCalcSin2,
    OPL3_EnvelopeCalcSin3,
    OPL3_EnvelopeCalcSin4,
    OPL3_EnvelopeCalcSin5,
    OPL3_EnvelopeCalcSin6,
    OPL3_EnvelopeCalcSin7
};
#endif

enum envelope_gen_num
{
    envelope_gen_num_attack = 0,
    envelope_gen_num_decay = 1,
    envelope_gen_num_sustain = 2,
    envelope_gen_num_release = 3
};

static void OPL3_EnvelopeUpdateKSL(opl3_slot *slot)
{
    int16_t ksl = (kslrom[slot->channel->f_num >> 6u] << 2)
               - ((0x08 - slot->channel->block) << 5);
    if (ksl < 0)
    {
        ksl = 0;
    }
    slot->eg_ksl = (uint8_t)ksl;
}

static void OPL3_EnvelopeCalc(opl3_slot *slot)
{
    uint8_t nonzero;
    uint8_t rate;
    uint8_t rate_hi;
    uint8_t rate_lo;
    uint8_t reg_rate = 0;
    uint8_t ks;
    uint8_t eg_shift, shift;
    uint16_t eg_rout;
    int16_t eg_inc;
    uint8_t eg_off;
    uint8_t reset = 0;
    slot->eg_out = slot->eg_rout + (slot->reg_tl << 2)
                 + (slot->eg_ksl >> kslshift[slot->reg_ksl]) + *slot->trem;

#if OPL_FAST_WAVEGEN
    if (slot->eg_out > 0x1ff)
    {
        slot->eg_out = 0x1ff;
    }
    slot->eg_out <<= 3;
#endif

    if (slot->key && slot->eg_gen == envelope_gen_num_release)
    {
        reset = 1;
        reg_rate = slot->reg_ar;
    }
    else
    {
        switch (slot->eg_gen)
        {
        case envelope_gen_num_attack:
            reg_rate = slot->reg_ar;
            break;
        case envelope_gen_num_decay:
            reg_rate = slot->reg_dr;
            break;
        case envelope_gen_num_sustain:
            if (!slot->reg_type)
            {
                reg_rate = slot->reg_rr;
            }
            break;
        case envelope_gen_num_release:
            reg_rate = slot->reg_rr;
            break;
        }
    }
    slot->pg_reset = reset;
    ks = slot->channel->ksv >> ((slot->reg_ksr ^ 1) << 1);
    nonzero = (reg_rate != 0);
    rate = ks + (reg_rate << 2);
    rate_hi = rate >> 2;
    rate_lo = rate & 0x03;
    if (rate_hi & 0x10)
    {
        rate_hi = 0x0f;
    }
    eg_shift = rate_hi + slot->chip->eg_add;
    shift = 0;
    if (nonzero)
    {
        if (rate_hi < 12)
        {
            if (slot->chip->eg_state)
            {
                switch (eg_shift)
                {
                case 12:
                    shift = 1;
                    break;
                case 13:
                    shift = (rate_lo >> 1) & 0x01;
                    break;
                case 14:
                    shift = rate_lo & 0x01;
                    break;
                default:
                    break;
                }
            }
        }
        else
        {
            shift = (rate_hi & 0x03) + eg_incstep[rate_lo][slot->chip->timer & 0x03u];
            if (shift & 0x04)
            {
                shift = 0x03;
            }
            if (!shift)
            {
                shift = slot->chip->eg_state;
            }
        }
    }
    eg_rout = slot->eg_rout;
    eg_inc = 0;
    eg_off = 0;
    /* Instant attack */
    if (reset && rate_hi == 0x0f)
    {
        eg_rout = 0x00;
    }
    /* Envelope off */
    if ((slot->eg_rout & 0x1f8) == 0x1f8)
    {
        eg_off = 1;
    }
    if (slot->eg_gen != envelope_gen_num_attack && !reset && eg_off)
    {
        eg_rout = 0x1ff;
    }
    switch (slot->eg_gen)
    {
    case envelope_gen_num_attack:
        if (!slot->eg_rout)
        {
            slot->eg_gen = envelope_gen_num_decay;
        }
        else if (slot->key && shift > 0 && rate_hi != 0x0f)
        {
            eg_inc = ~slot->eg_rout >> (4 - shift);
        }
        break;
    case envelope_gen_num_decay:
        if ((slot->eg_rout >> 4) == slot->reg_sl)
        {
            slot->eg_gen = envelope_gen_num_sustain;
        }
        else if (!eg_off && !reset && shift > 0)
        {
            eg_inc = 1 << (shift - 1);
        }
        break;
    case envelope_gen_num_sustain:
    case envelope_gen_num_release:
        if (!eg_off && !reset && shift > 0)
        {
            eg_inc = 1 << (shift - 1);
        }
        break;
    }
    slot->eg_rout = (eg_rout + eg_inc) & 0x1ff;
    /* Key off */
    if (reset)
    {
        slot->eg_gen = envelope_gen_num_attack;
    }
    if (!slot->key)
    {
        slot->eg_gen = envelope_gen_num_release;
    }
}

static void OPL3_EnvelopeKeyOn(opl3_slot *slot, uint8_t type)
{
    slot->key |= type;
}

static void OPL3_EnvelopeKeyOff(opl3_slot *slot, uint8_t type)
{
    slot->key &= ~type;
}

/*
    Phase Generator
*/

static void OPL3_PhaseGenerate(opl3_slot *slot)
{
    opl3_chip *chip;
    uint16_t f_num;
    uint32_t basefreq;
    uint8_t rm_xor, n_bit;
    uint32_t noise;
    uint16_t phase;

    chip = slot->chip;
    f_num = slot->channel->f_num;
    if (slot->reg_vib)
    {
        int8_t range;
        uint8_t vibpos;

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
    phase = (uint16_t)(slot->pg_phase >> 9);
    if (slot->pg_reset)
    {
        slot->pg_phase = 0;
    }
    slot->pg_phase += (basefreq * mt[slot->reg_mult]) >> 1;
    /* Rhythm mode */
    noise = chip->noise;
    slot->pg_phase_out = phase;
    if (slot->slot_num == 13) /* hh */
    {
        chip->rm_hh_bit2 = (phase >> 2) & 1;
        chip->rm_hh_bit3 = (phase >> 3) & 1;
        chip->rm_hh_bit7 = (phase >> 7) & 1;
        chip->rm_hh_bit8 = (phase >> 8) & 1;
    }
    if (slot->slot_num == 17 && (chip->rhy & 0x20)) /* tc */
    {
        chip->rm_tc_bit3 = (phase >> 3) & 1;
        chip->rm_tc_bit5 = (phase >> 5) & 1;
    }
    if (chip->rhy & 0x20)
    {
        rm_xor = (chip->rm_hh_bit2 ^ chip->rm_hh_bit7)
               | (chip->rm_hh_bit3 ^ chip->rm_tc_bit5)
               | (chip->rm_tc_bit3 ^ chip->rm_tc_bit5);
        switch (slot->slot_num)
        {
        case 13: /* hh */
            slot->pg_phase_out = rm_xor << 9;
            if (rm_xor ^ (noise & 1))
            {
                slot->pg_phase_out |= 0xd0;
            }
            else
            {
                slot->pg_phase_out |= 0x34;
            }
            break;
        case 16: /* sd */
            slot->pg_phase_out = (chip->rm_hh_bit8 << 9)
                               | ((chip->rm_hh_bit8 ^ (noise & 1)) << 8);
            break;
        case 17: /* tc */
            slot->pg_phase_out = (rm_xor << 9) | 0x80;
            break;
        default:
            break;
        }
    }
    n_bit = ((noise >> 14) ^ noise) & 0x01;
    chip->noise = (noise >> 1) | (n_bit << 22);
}

/*
    Slot
*/

static void OPL3_SlotWrite20(opl3_slot *slot, uint8_t data)
{
    if ((data >> 7) & 0x01)
    {
        slot->trem = &slot->chip->tremolo;
    }
    else
    {
        slot->trem = (uint8_t*)&slot->chip->zeromod;
    }
    slot->reg_vib = (data >> 6) & 0x01;
    slot->reg_type = (data >> 5) & 0x01;
    slot->reg_ksr = (data >> 4) & 0x01;
    slot->reg_mult = data & 0x0f;
}

static void OPL3_SlotWrite40(opl3_slot *slot, uint8_t data)
{
    slot->reg_ksl = (data >> 6) & 0x03;
    slot->reg_tl = data & 0x3f;
    OPL3_EnvelopeUpdateKSL(slot);
}

static void OPL3_SlotWrite60(opl3_slot *slot, uint8_t data)
{
    slot->reg_ar = (data >> 4) & 0x0f;
    slot->reg_dr = data & 0x0f;
}

static void OPL3_SlotWrite80(opl3_slot *slot, uint8_t data)
{
    slot->reg_sl = (data >> 4) & 0x0f;
    if (slot->reg_sl == 0x0f)
    {
        slot->reg_sl = 0x1f;
    }
    slot->reg_rr = data & 0x0f;
}

static void OPL3_SlotWriteE0(opl3_slot *slot, uint8_t data)
{
    slot->reg_wf = data & 0x07;
    if (slot->chip->newm == 0x00)
    {
        slot->reg_wf &= 0x03;
    }

#if OPL_FAST_WAVEGEN
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
#endif
}

#if OPL_FAST_WAVEGEN
static void OPL3_SlotGenerate(opl3_slot *slot)
{
    uint16_t phase = slot->pg_phase_out + *slot->mod;
    uint32_t neg, level;
    uint8_t  phaseshift;

    /* Fast paths for mute segments */
    if (phase & slot->maskzero)
    {
        slot->out = 0;
        return;
    }

    neg = (int32_t)((uint32_t)phase << slot->signpos) >> 31;
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
#else
static void OPL3_SlotGenerate(opl3_slot *slot)
{
    slot->out = envelope_sin[slot->reg_wf](slot->pg_phase_out + *slot->mod, slot->eg_out);
}
#endif

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
    Channel
*/

static void OPL3_ChannelSetupAlg(opl3_channel *channel);

static void OPL3_ChannelUpdateRhythm(opl3_chip *chip, uint8_t data)
{
    opl3_channel *channel6;
    opl3_channel *channel7;
    opl3_channel *channel8;
    uint8_t chnum;

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
        OPL3_ChannelSetupAlg(channel7);
        OPL3_ChannelSetupAlg(channel8);
        /* hh */
        if (chip->rhy & 0x01)
        {
            OPL3_EnvelopeKeyOn(channel7->slotz[0], egk_drum);
        }
        else
        {
            OPL3_EnvelopeKeyOff(channel7->slotz[0], egk_drum);
        }
        /* tc */
        if (chip->rhy & 0x02)
        {
            OPL3_EnvelopeKeyOn(channel8->slotz[1], egk_drum);
        }
        else
        {
            OPL3_EnvelopeKeyOff(channel8->slotz[1], egk_drum);
        }
        /* tom */
        if (chip->rhy & 0x04)
        {
            OPL3_EnvelopeKeyOn(channel8->slotz[0], egk_drum);
        }
        else
        {
            OPL3_EnvelopeKeyOff(channel8->slotz[0], egk_drum);
        }
        /* sd */
        if (chip->rhy & 0x08)
        {
            OPL3_EnvelopeKeyOn(channel7->slotz[1], egk_drum);
        }
        else
        {
            OPL3_EnvelopeKeyOff(channel7->slotz[1], egk_drum);
        }
        /* bd */
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

static void OPL3_ChannelWriteA0(opl3_channel *channel, uint8_t data)
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
    if (channel->chip->newm && channel->chtype == ch_4op)
    {
        channel->pair->f_num = channel->f_num;
        channel->pair->ksv = channel->ksv;
        OPL3_EnvelopeUpdateKSL(channel->pair->slotz[0]);
        OPL3_EnvelopeUpdateKSL(channel->pair->slotz[1]);
    }
}

static void OPL3_ChannelWriteB0(opl3_channel *channel, uint8_t data)
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
    if (channel->chip->newm && channel->chtype == ch_4op)
    {
        channel->pair->f_num = channel->f_num;
        channel->pair->block = channel->block;
        channel->pair->ksv = channel->ksv;
        OPL3_EnvelopeUpdateKSL(channel->pair->slotz[0]);
        OPL3_EnvelopeUpdateKSL(channel->pair->slotz[1]);
    }
}

static void OPL3_ChannelSetupAlg(opl3_channel *channel)
{
    if (channel->chtype == ch_drum)
    {
        if (channel->ch_num == 7 || channel->ch_num == 8)
        {
            channel->slotz[0]->mod = &channel->chip->zeromod;
            channel->slotz[1]->mod = &channel->chip->zeromod;
            return;
        }
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

static void OPL3_ChannelUpdateAlg(opl3_channel *channel)
{
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
}

static void OPL3_ChannelWriteC0(opl3_channel *channel, uint8_t data)
{
    channel->fb = (data & 0x0e) >> 1;
    channel->con = data & 0x01;
    OPL3_ChannelUpdateAlg(channel);
    if (channel->chip->newm)
    {
        channel->cha = ((data >> 4) & 0x01) ? ~0 : 0;
        channel->chb = ((data >> 5) & 0x01) ? ~0 : 0;
        channel->chc = ((data >> 6) & 0x01) ? ~0 : 0;
        channel->chd = ((data >> 7) & 0x01) ? ~0 : 0;
    }
    else
    {
        channel->cha = channel->chb = (uint16_t)~0;
        /* TODO: Verify on real chip if DAC2 output is disabled in compat mode */
        channel->chc = channel->chd = 0;
    }
#if OPL_ENABLE_STEREOEXT
    if (!channel->chip->stereoext)
    {
        channel->leftpan = channel->cha << 16;
        channel->rightpan = channel->chb << 16;
    }
#endif
}

#if OPL_ENABLE_STEREOEXT
static void OPL3_ChannelWriteD0(opl3_channel* channel, uint8_t data)
{
    if (channel->chip->stereoext)
    {
        channel->leftpan = panpot_lut[data ^ 0xffu];
        channel->rightpan = panpot_lut[data];
    }
}
#endif

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

static void OPL3_ChannelSet4Op(opl3_chip *chip, uint8_t data)
{
    uint8_t bit;
    uint8_t chnum;
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
            chip->channel[chnum + 3u].chtype = ch_4op2;
            OPL3_ChannelUpdateAlg(&chip->channel[chnum]);
        }
        else
        {
            chip->channel[chnum].chtype = ch_2op;
            chip->channel[chnum + 3u].chtype = ch_2op;
            OPL3_ChannelUpdateAlg(&chip->channel[chnum]);
            OPL3_ChannelUpdateAlg(&chip->channel[chnum + 3u]);
        }
    }
}

static int16_t OPL3_ClipSample(int32_t sample)
{
    if (sample > 32767)
    {
        sample = 32767;
    }
    else if (sample < -32768)
    {
        sample = -32768;
    }
    return (int16_t)sample;
}

static void OPL3_ProcessSlot(opl3_slot *slot)
{
    OPL3_SlotCalcFB(slot);
    OPL3_EnvelopeCalc(slot);
    OPL3_PhaseGenerate(slot);
    OPL3_SlotGenerate(slot);
}

inline void OPL3_Generate4Ch(opl3_chip *chip, int16_t *buf4)
{
    opl3_channel *channel;
    opl3_writebuf *writebuf;
    int16_t **out;
    int32_t mix[2];
    uint8_t ii;
    int16_t accm;
    uint8_t shift = 0;

    buf4[1] = OPL3_ClipSample(chip->mixbuff[1]);
    buf4[3] = OPL3_ClipSample(chip->mixbuff[3]);

#if OPL_QUIRK_CHANNELSAMPLEDELAY
    for (ii = 0; ii < 15; ii++)
#else
    for (ii = 0; ii < 36; ii++)
#endif
    {
        OPL3_ProcessSlot(&chip->slot[ii]);
    }

    mix[0] = mix[1] = 0;
    for (ii = 0; ii < 18; ii++)
    {
        channel = &chip->channel[ii];
        out = channel->out;
        accm = *out[0] + *out[1] + *out[2] + *out[3];
#if OPL_ENABLE_STEREOEXT
        mix[0] += (int16_t)((accm * channel->leftpan) >> 16);
#else
        mix[0] += (int16_t)((accm * chip->channel[ii].chl / 65535) & channel->cha);
#endif
        mix[1] += (int16_t)(accm & channel->chc);
    }
    chip->mixbuff[0] = mix[0];
    chip->mixbuff[2] = mix[1];

#if OPL_QUIRK_CHANNELSAMPLEDELAY
    for (ii = 15; ii < 18; ii++)
    {
        OPL3_ProcessSlot(&chip->slot[ii]);
    }
#endif

    buf4[0] = OPL3_ClipSample(chip->mixbuff[0]);
    buf4[2] = OPL3_ClipSample(chip->mixbuff[2]);

#if OPL_QUIRK_CHANNELSAMPLEDELAY
    for (ii = 18; ii < 33; ii++)
    {
        OPL3_ProcessSlot(&chip->slot[ii]);
    }
#endif

    mix[0] = mix[1] = 0;
    for (ii = 0; ii < 18; ii++)
    {
        channel = &chip->channel[ii];
        out = channel->out;
        accm = *out[0] + *out[1] + *out[2] + *out[3];
#if OPL_ENABLE_STEREOEXT
        mix[0] += (int16_t)((accm * channel->rightpan) >> 16);
#else
        mix[0] += (int16_t)((accm * chip->channel[ii].chr / 65535) & channel->chb);
#endif
        mix[1] += (int16_t)(accm & channel->chd);
    }
    chip->mixbuff[1] = mix[0];
    chip->mixbuff[3] = mix[1];

#if OPL_QUIRK_CHANNELSAMPLEDELAY
    for (ii = 33; ii < 36; ii++)
    {
        OPL3_ProcessSlot(&chip->slot[ii]);
    }
#endif

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

    chip->eg_add = 0;
    if (chip->eg_timer)
    {
        while (shift < 36 && ((chip->eg_timer >> shift) & 1) == 0)
        {
            shift++;
        }
        if (shift > 12)
        {
            chip->eg_add = 0;
        }
        else
        {
            chip->eg_add = shift + 1;
        }
    }

    if (chip->eg_timerrem || chip->eg_state)
    {
        if (chip->eg_timer == UINT64_C(0xfffffffff))
        {
            chip->eg_timer = 0;
            chip->eg_timerrem = 1;
        }
        else
        {
            chip->eg_timer++;
            chip->eg_timerrem = 0;
        }
    }

    chip->eg_state ^= 1;

    while ((writebuf = &chip->writebuf[chip->writebuf_cur]), writebuf->time <= chip->writebuf_samplecnt)
    {
        if (!(writebuf->reg & 0x200))
        {
            break;
        }
        writebuf->reg &= 0x1ff;
        OPL3_WriteReg(chip, writebuf->reg, writebuf->data);
        chip->writebuf_cur = (chip->writebuf_cur + 1) % OPL_WRITEBUF_SIZE;
    }
    chip->writebuf_samplecnt++;
}

void OPL3_Generate(opl3_chip *chip, int16_t *buf)
{
    int16_t samples[4];
    OPL3_Generate4Ch(chip, samples);
    buf[0] = samples[0];
    buf[1] = samples[1];
}

void OPL3_Generate4ChResampled(opl3_chip *chip, int16_t *buf4)
{
    while (chip->samplecnt >= chip->rateratio)
    {
        chip->oldsamples[0] = chip->samples[0];
        chip->oldsamples[1] = chip->samples[1];
        chip->oldsamples[2] = chip->samples[2];
        chip->oldsamples[3] = chip->samples[3];
        OPL3_Generate4Ch(chip, chip->samples);
        chip->samplecnt -= chip->rateratio;
    }
    buf4[0] = (int16_t)((chip->oldsamples[0] * (chip->rateratio - chip->samplecnt)
                        + chip->samples[0] * chip->samplecnt) / chip->rateratio);
    buf4[1] = (int16_t)((chip->oldsamples[1] * (chip->rateratio - chip->samplecnt)
                        + chip->samples[1] * chip->samplecnt) / chip->rateratio);
    buf4[2] = (int16_t)((chip->oldsamples[2] * (chip->rateratio - chip->samplecnt)
                        + chip->samples[2] * chip->samplecnt) / chip->rateratio);
    buf4[3] = (int16_t)((chip->oldsamples[3] * (chip->rateratio - chip->samplecnt)
                        + chip->samples[3] * chip->samplecnt) / chip->rateratio);
    chip->samplecnt += 1 << RSM_FRAC;
}

void OPL3_GenerateResampled(opl3_chip *chip, int16_t *buf)
{
    int16_t samples[4];
    OPL3_Generate4ChResampled(chip, samples);
    buf[0] = samples[0];
    buf[1] = samples[1];
}

void OPL3_Reset(opl3_chip *chip, uint32_t samplerate)
{
    opl3_slot *slot;
    opl3_channel *channel;
    uint8_t slotnum;
    uint8_t channum;
    uint8_t local_ch_slot;

    memset(chip, 0, sizeof(opl3_chip));
    for (slotnum = 0; slotnum < 36; slotnum++)
    {
        slot = &chip->slot[slotnum];
        slot->chip = chip;
        slot->mod = &chip->zeromod;
        slot->eg_rout = 0x1ff;
#if OPL_FAST_WAVEGEN
        slot->eg_out = 0x1ff << 3;
#else
        slot->eg_out = 0x1ff;
#endif
        slot->eg_gen = envelope_gen_num_release;
        slot->trem = (uint8_t*)&chip->zeromod;
        slot->slot_num = slotnum;
#if OPL_FAST_WAVEGEN
        slot->signpos = (31-9);  /* for wf=0 need use sigext of (phase & 0x200) */
#endif
    }
    for (channum = 0; channum < 18; channum++)
    {
        channel = &chip->channel[channum];
        local_ch_slot = ch_slot[channum];
        channel->slotz[0] = &chip->slot[local_ch_slot];
        channel->slotz[1] = &chip->slot[local_ch_slot + 3u];
        chip->slot[local_ch_slot].channel = channel;
        chip->slot[local_ch_slot + 3u].channel = channel;
        if ((channum % 9) < 3)
        {
            channel->pair = &chip->channel[channum + 3u];
        }
        else if ((channum % 9) < 6)
        {
            channel->pair = &chip->channel[channum - 3u];
        }
        channel->chip = chip;
        channel->out[0] = &chip->zeromod;
        channel->out[1] = &chip->zeromod;
        channel->out[2] = &chip->zeromod;
        channel->out[3] = &chip->zeromod;
        channel->chtype = ch_2op;
        channel->cha = 0xffff;
        channel->chb = 0xffff;
        channel->chl = 46340;
        channel->chr = 46340;
#if OPL_ENABLE_STEREOEXT
        channel->leftpan = 0x10000;
        channel->rightpan = 0x10000;
#endif
        channel->ch_num = channum;
        OPL3_ChannelSetupAlg(channel);
    }
    chip->noise = 1;
    chip->rateratio = (samplerate << RSM_FRAC) / 49716;
    chip->tremoloshift = 4;
    chip->vibshift = 1;

#if OPL_ENABLE_STEREOEXT
    if (!panpot_lut_build)
    {
        int32_t i;
        for (i = 0; i < 256; i++)
        {
            panpot_lut[i] = OPL_SIN(i);
        }
        panpot_lut_build = 1;
    }
#endif
}

static void OPL3_ChannelWritePan(opl3_channel *channel, uint8_t data)
{
    channel->chl = panlawtable[data & 0x7F];
    channel->chr = panlawtable[0x7F - (data & 0x7F)];
}

void OPL3_WritePan(opl3_chip *chip, uint16_t reg, uint8_t v)
{
    uint8_t high = (reg >> 8) & 0x01;
    uint8_t regm = reg & 0xff;
    OPL3_ChannelWritePan(&chip->channel[9 * high + (regm & 0x0f)], v);
}

void OPL3_WriteReg(opl3_chip *chip, uint16_t reg, uint8_t v)
{
    uint8_t high = (reg >> 8) & 0x01;
    uint8_t regm = reg & 0xff;
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
#if OPL_ENABLE_STEREOEXT
                chip->stereoext = (v >> 1) & 0x01;
#endif
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
        if (ad_slot[regm & 0x1fu] >= 0)
        {
            OPL3_SlotWrite20(&chip->slot[18u * high + ad_slot[regm & 0x1fu]], v);
        }
        break;
    case 0x40:
    case 0x50:
        if (ad_slot[regm & 0x1fu] >= 0)
        {
            OPL3_SlotWrite40(&chip->slot[18u * high + ad_slot[regm & 0x1fu]], v);
        }
        break;
    case 0x60:
    case 0x70:
        if (ad_slot[regm & 0x1fu] >= 0)
        {
            OPL3_SlotWrite60(&chip->slot[18u * high + ad_slot[regm & 0x1fu]], v);
        }
        break;
    case 0x80:
    case 0x90:
        if (ad_slot[regm & 0x1fu] >= 0)
        {
            OPL3_SlotWrite80(&chip->slot[18u * high + ad_slot[regm & 0x1fu]], v);
        }
        break;
    case 0xe0:
    case 0xf0:
        if (ad_slot[regm & 0x1fu] >= 0)
        {
            OPL3_SlotWriteE0(&chip->slot[18u * high + ad_slot[regm & 0x1fu]], v);
        }
        break;
    case 0xa0:
        if ((regm & 0x0f) < 9)
        {
            OPL3_ChannelWriteA0(&chip->channel[9u * high + (regm & 0x0fu)], v);
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
            OPL3_ChannelWriteB0(&chip->channel[9u * high + (regm & 0x0fu)], v);
            if (v & 0x20)
            {
                OPL3_ChannelKeyOn(&chip->channel[9u * high + (regm & 0x0fu)]);
            }
            else
            {
                OPL3_ChannelKeyOff(&chip->channel[9u * high + (regm & 0x0fu)]);
            }
        }
        break;
    case 0xc0:
        if ((regm & 0x0f) < 9)
        {
            OPL3_ChannelWriteC0(&chip->channel[9u * high + (regm & 0x0fu)], v);
        }
        break;
#if OPL_ENABLE_STEREOEXT
    case 0xd0:
        if ((regm & 0x0f) < 9)
        {
            OPL3_ChannelWriteD0(&chip->channel[9u * high + (regm & 0x0fu)], v);
        }
        break;
#endif
    }
}

void OPL3_WriteRegBuffered(opl3_chip *chip, uint16_t reg, uint8_t v)
{
    uint64_t time1, time2;
    opl3_writebuf *writebuf;
    uint32_t writebuf_last;

    writebuf_last = chip->writebuf_last;
    writebuf = &chip->writebuf[writebuf_last];

    if (writebuf->reg & 0x200)
    {
        OPL3_WriteReg(chip, writebuf->reg & 0x1ff, writebuf->data);

        chip->writebuf_cur = (writebuf_last + 1) % OPL_WRITEBUF_SIZE;
        chip->writebuf_samplecnt = writebuf->time;
    }

    writebuf->reg = reg | 0x200;
    writebuf->data = v;
    time1 = chip->writebuf_lasttime + OPL_WRITEBUF_DELAY;
    time2 = chip->writebuf_samplecnt;

    if (time1 < time2)
    {
        time1 = time2;
    }

    writebuf->time = time1;
    chip->writebuf_lasttime = time1;
    chip->writebuf_last = (writebuf_last + 1) % OPL_WRITEBUF_SIZE;
}

void OPL3_Generate4ChStream(opl3_chip *chip, int16_t *sndptr1, int16_t *sndptr2, uint32_t numsamples)
{
    uint_fast32_t i;
    int16_t samples[4];

    for(i = 0; i < numsamples; i++)
    {
        OPL3_Generate4ChResampled(chip, samples);
        sndptr1[0] = samples[0];
        sndptr1[1] = samples[1];
        sndptr2[0] = samples[2];
        sndptr2[1] = samples[3];
        sndptr1 += 2;
        sndptr2 += 2;
    }
}

void OPL3_GenerateStream(opl3_chip *chip, int16_t *sndptr, uint32_t numsamples)
{
    uint_fast32_t i;

    for(i = 0; i < numsamples; i++)
    {
        OPL3_GenerateResampled(chip, sndptr);
        sndptr += 2;
    }
}
