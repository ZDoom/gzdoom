// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

// Based on Nuked OPN2 ym3438.c and ym3438.h

#include "Ym2612_Nuked.h"

/*
 * Copyright (C) 2017 Alexey Khokholov (Nuke.YKT)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *
 *  Nuked OPN2(Yamaha YM3438) emulator.
 *  Thanks:
 *      Silicon Pr0n:
 *          Yamaha YM3438 decap and die shot(digshadow).
 *      OPLx decapsulated(Matthew Gambrell, Olli Niemitalo):
 *          OPL2 ROMs.
 *
 * version: 1.0.7
 */


#include <stdint.h>
#include <string.h>

typedef uintptr_t       Bitu;
typedef intptr_t        Bits;
typedef uint64_t        Bit64u;
typedef int64_t         Bit64s;
typedef uint32_t        Bit32u;
typedef int32_t         Bit32s;
typedef uint16_t        Bit16u;
typedef int16_t         Bit16s;
typedef uint8_t         Bit8u;
typedef int8_t Bit8s;

namespace Ym2612_NukedImpl
{

/*EXTRA*/
#define RSM_FRAC 10
#define OPN_WRITEBUF_SIZE 2048
#define OPN_WRITEBUF_DELAY 15

enum {
    ym3438_type_discrete = 0,   /* Discrete YM3438 (Teradrive)          */
    ym3438_type_asic = 1,       /* ASIC YM3438 (MD1 VA7, MD2, MD3, etc) */
    ym3438_type_ym2612 = 2      /* YM2612 (MD1, MD2 VA2)                */
};

/*EXTRA*/
typedef struct _opn2_writebuf {
    Bit64u time;
    Bit8u port;
    Bit8u data;
    Bit8u reserved[6];
} opn2_writebuf;

typedef struct
{
    Bit32u cycles;
    Bit32u channel;
    Bit16s mol, mor;
    /* IO */
    Bit16u write_data;
    Bit8u write_a;
    Bit8u write_d;
    Bit8u write_a_en;
    Bit8u write_d_en;
    Bit8u write_busy;
    Bit8u write_busy_cnt;
    Bit8u write_fm_address;
    Bit8u write_fm_data;
    Bit8u write_fm_mode_a;
    Bit16u address;
    Bit8u data;
    Bit8u pin_test_in;
    Bit8u pin_irq;
    Bit8u busy;
    /* LFO */
    Bit8u lfo_en;
    Bit8u lfo_freq;
    Bit8u lfo_pm;
    Bit8u lfo_am;
    Bit8u lfo_cnt;
    Bit8u lfo_inc;
    Bit8u lfo_quotient;
    /* Phase generator */
    Bit16u pg_fnum;
    Bit8u pg_block;
    Bit8u pg_kcode;
    Bit32u pg_inc[24];
    Bit32u pg_phase[24];
    Bit8u pg_reset[24];
    Bit32u pg_read;
    /* Envelope generator */
    Bit8u eg_cycle;
    Bit8u eg_cycle_stop;
    Bit8u eg_shift;
    Bit8u eg_shift_lock;
    Bit8u eg_timer_low_lock;
    Bit16u eg_timer;
    Bit8u eg_timer_inc;
    Bit16u eg_quotient;
    Bit8u eg_custom_timer;
    Bit8u eg_rate;
    Bit8u eg_ksv;
    Bit8u eg_inc;
    Bit8u eg_ratemax;
    Bit8u eg_sl[2];
    Bit8u eg_lfo_am;
    Bit8u eg_tl[2];
    Bit8u eg_state[24];
    Bit16u eg_level[24];
    Bit16u eg_out[24];
    Bit8u eg_kon[24];
    Bit8u eg_kon_csm[24];
    Bit8u eg_kon_latch[24];
    Bit8u eg_csm_mode[24];
    Bit8u eg_ssg_enable[24];
    Bit8u eg_ssg_pgrst_latch[24];
    Bit8u eg_ssg_repeat_latch[24];
    Bit8u eg_ssg_hold_up_latch[24];
    Bit8u eg_ssg_dir[24];
    Bit8u eg_ssg_inv[24];
    Bit32u eg_read[2];
    Bit8u eg_read_inc;
    /* FM */
    Bit16s fm_op1[6][2];
    Bit16s fm_op2[6];
    Bit16s fm_out[24];
    Bit16u fm_mod[24];
    /* Channel */
    Bit16s ch_acc[6];
    Bit16s ch_out[6];
    Bit16s ch_lock;
    Bit8u ch_lock_l;
    Bit8u ch_lock_r;
    Bit16s ch_read;
    /* Timer */
    Bit16u timer_a_cnt;
    Bit16u timer_a_reg;
    Bit8u timer_a_load_lock;
    Bit8u timer_a_load;
    Bit8u timer_a_enable;
    Bit8u timer_a_reset;
    Bit8u timer_a_load_latch;
    Bit8u timer_a_overflow_flag;
    Bit8u timer_a_overflow;

    Bit16u timer_b_cnt;
    Bit8u timer_b_subcnt;
    Bit16u timer_b_reg;
    Bit8u timer_b_load_lock;
    Bit8u timer_b_load;
    Bit8u timer_b_enable;
    Bit8u timer_b_reset;
    Bit8u timer_b_load_latch;
    Bit8u timer_b_overflow_flag;
    Bit8u timer_b_overflow;

    /* Register set */
    Bit8u mode_test_21[8];
    Bit8u mode_test_2c[8];
    Bit8u mode_ch3;
    Bit8u mode_kon_channel;
    Bit8u mode_kon_operator[4];
    Bit8u mode_kon[24];
    Bit8u mode_csm;
    Bit8u mode_kon_csm;
    Bit8u dacen;
    Bit16s dacdata;

    Bit8u ks[24];
    Bit8u ar[24];
    Bit8u sr[24];
    Bit8u dt[24];
    Bit8u multi[24];
    Bit8u sl[24];
    Bit8u rr[24];
    Bit8u dr[24];
    Bit8u am[24];
    Bit8u tl[24];
    Bit8u ssg_eg[24];

    Bit16u fnum[6];
    Bit8u block[6];
    Bit8u kcode[6];
    Bit16u fnum_3ch[6];
    Bit8u block_3ch[6];
    Bit8u kcode_3ch[6];
    Bit8u reg_a4;
    Bit8u reg_ac;
    Bit8u connect[6];
    Bit8u fb[6];
    Bit8u pan_l[6], pan_r[6];
    Bit8u ams[6];
    Bit8u pms[6];

    /*EXTRA*/
    Bit32u mute[7];
    Bit32s rateratio;
    Bit32s samplecnt;
    Bit32s oldsamples[2];
    Bit32s samples[2];

    Bit64u writebuf_samplecnt;
    Bit32u writebuf_cur;
    Bit32u writebuf_last;
    Bit64u writebuf_lasttime;
    opn2_writebuf writebuf[OPN_WRITEBUF_SIZE];
} ym3438_t;

/* EXTRA, original was "void OPN2_Reset(ym3438_t *chip)" */
void OPN2_Reset(ym3438_t *chip, Bit32u rate, Bit32u clock);
void OPN2_SetChipType(Bit32u type);
void OPN2_Clock(ym3438_t *chip, Bit16s *buffer);
void OPN2_Write(ym3438_t *chip, Bit32u port, Bit8u data);
void OPN2_SetTestPin(ym3438_t *chip, Bit32u value);
Bit32u OPN2_ReadTestPin(ym3438_t *chip);
Bit32u OPN2_ReadIRQPin(ym3438_t *chip);
Bit8u OPN2_Read(ym3438_t *chip, Bit32u port);

/*EXTRA*/
void OPN2_WriteBuffered(ym3438_t *chip, Bit32u port, Bit8u data);
void OPN2_Generate(ym3438_t *chip, Bit16s *buf);
void OPN2_GenerateResampled(ym3438_t *chip, Bit16s *buf);
void OPN2_GenerateStream(ym3438_t *chip, Bit16s *output, Bit32u numsamples);
void OPN2_GenerateStreamMix(ym3438_t *chip, Bit16s *output, Bit32u numsamples);
void OPN2_SetOptions(Bit8u flags);
void OPN2_SetMute(ym3438_t *chip, Bit32u mute);





enum {
    eg_num_attack = 0,
    eg_num_decay = 1,
    eg_num_sustain = 2,
    eg_num_release = 3
};

/* logsin table */
static const Bit16u logsinrom[256] = {
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

/* exp table */
static const Bit16u exprom[256] = {
    0x000, 0x003, 0x006, 0x008, 0x00b, 0x00e, 0x011, 0x014,
    0x016, 0x019, 0x01c, 0x01f, 0x022, 0x025, 0x028, 0x02a,
    0x02d, 0x030, 0x033, 0x036, 0x039, 0x03c, 0x03f, 0x042,
    0x045, 0x048, 0x04b, 0x04e, 0x051, 0x054, 0x057, 0x05a,
    0x05d, 0x060, 0x063, 0x066, 0x069, 0x06c, 0x06f, 0x072,
    0x075, 0x078, 0x07b, 0x07e, 0x082, 0x085, 0x088, 0x08b,
    0x08e, 0x091, 0x094, 0x098, 0x09b, 0x09e, 0x0a1, 0x0a4,
    0x0a8, 0x0ab, 0x0ae, 0x0b1, 0x0b5, 0x0b8, 0x0bb, 0x0be,
    0x0c2, 0x0c5, 0x0c8, 0x0cc, 0x0cf, 0x0d2, 0x0d6, 0x0d9,
    0x0dc, 0x0e0, 0x0e3, 0x0e7, 0x0ea, 0x0ed, 0x0f1, 0x0f4,
    0x0f8, 0x0fb, 0x0ff, 0x102, 0x106, 0x109, 0x10c, 0x110,
    0x114, 0x117, 0x11b, 0x11e, 0x122, 0x125, 0x129, 0x12c,
    0x130, 0x134, 0x137, 0x13b, 0x13e, 0x142, 0x146, 0x149,
    0x14d, 0x151, 0x154, 0x158, 0x15c, 0x160, 0x163, 0x167,
    0x16b, 0x16f, 0x172, 0x176, 0x17a, 0x17e, 0x181, 0x185,
    0x189, 0x18d, 0x191, 0x195, 0x199, 0x19c, 0x1a0, 0x1a4,
    0x1a8, 0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c0, 0x1c4,
    0x1c8, 0x1cc, 0x1d0, 0x1d4, 0x1d8, 0x1dc, 0x1e0, 0x1e4,
    0x1e8, 0x1ec, 0x1f0, 0x1f5, 0x1f9, 0x1fd, 0x201, 0x205,
    0x209, 0x20e, 0x212, 0x216, 0x21a, 0x21e, 0x223, 0x227,
    0x22b, 0x230, 0x234, 0x238, 0x23c, 0x241, 0x245, 0x249,
    0x24e, 0x252, 0x257, 0x25b, 0x25f, 0x264, 0x268, 0x26d,
    0x271, 0x276, 0x27a, 0x27f, 0x283, 0x288, 0x28c, 0x291,
    0x295, 0x29a, 0x29e, 0x2a3, 0x2a8, 0x2ac, 0x2b1, 0x2b5,
    0x2ba, 0x2bf, 0x2c4, 0x2c8, 0x2cd, 0x2d2, 0x2d6, 0x2db,
    0x2e0, 0x2e5, 0x2e9, 0x2ee, 0x2f3, 0x2f8, 0x2fd, 0x302,
    0x306, 0x30b, 0x310, 0x315, 0x31a, 0x31f, 0x324, 0x329,
    0x32e, 0x333, 0x338, 0x33d, 0x342, 0x347, 0x34c, 0x351,
    0x356, 0x35b, 0x360, 0x365, 0x36a, 0x370, 0x375, 0x37a,
    0x37f, 0x384, 0x38a, 0x38f, 0x394, 0x399, 0x39f, 0x3a4,
    0x3a9, 0x3ae, 0x3b4, 0x3b9, 0x3bf, 0x3c4, 0x3c9, 0x3cf,
    0x3d4, 0x3da, 0x3df, 0x3e4, 0x3ea, 0x3ef, 0x3f5, 0x3fa
};

/* Note table */
static const Bit32u fn_note[16] = {
    0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3
};

/* Envelope generator */
static const Bit32u eg_stephi[4][4] = {
    { 0, 0, 0, 0 },
    { 1, 0, 0, 0 },
    { 1, 0, 1, 0 },
    { 1, 1, 1, 0 }
};

static const Bit8u eg_am_shift[4] = {
    7, 3, 1, 0
};

/* Phase generator */
static const Bit32u pg_detune[8] = { 16, 17, 19, 20, 22, 24, 27, 29 };

static const Bit32u pg_lfo_sh1[8][8] = {
    { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 7, 7, 1, 1 },
    { 7, 7, 7, 7, 1, 1, 1, 1 },
    { 7, 7, 7, 1, 1, 1, 1, 0 },
    { 7, 7, 1, 1, 0, 0, 0, 0 },
    { 7, 7, 1, 1, 0, 0, 0, 0 },
    { 7, 7, 1, 1, 0, 0, 0, 0 }
};

static const Bit32u pg_lfo_sh2[8][8] = {
    { 7, 7, 7, 7, 7, 7, 7, 7 },
    { 7, 7, 7, 7, 2, 2, 2, 2 },
    { 7, 7, 7, 2, 2, 2, 7, 7 },
    { 7, 7, 2, 2, 7, 7, 2, 2 },
    { 7, 7, 2, 7, 7, 7, 2, 7 },
    { 7, 7, 7, 2, 7, 7, 2, 1 },
    { 7, 7, 7, 2, 7, 7, 2, 1 },
    { 7, 7, 7, 2, 7, 7, 2, 1 }
};

/* Address decoder */
static const Bit32u op_offset[12] = {
    0x000, /* Ch1 OP1/OP2 */
    0x001, /* Ch2 OP1/OP2 */
    0x002, /* Ch3 OP1/OP2 */
    0x100, /* Ch4 OP1/OP2 */
    0x101, /* Ch5 OP1/OP2 */
    0x102, /* Ch6 OP1/OP2 */
    0x004, /* Ch1 OP3/OP4 */
    0x005, /* Ch2 OP3/OP4 */
    0x006, /* Ch3 OP3/OP4 */
    0x104, /* Ch4 OP3/OP4 */
    0x105, /* Ch5 OP3/OP4 */
    0x106  /* Ch6 OP3/OP4 */
};

static const Bit32u ch_offset[6] = {
    0x000, /* Ch1 */
    0x001, /* Ch2 */
    0x002, /* Ch3 */
    0x100, /* Ch4 */
    0x101, /* Ch5 */
    0x102  /* Ch6 */
};

/* LFO */
static const Bit32u lfo_cycles[8] = {
    108, 77, 71, 67, 62, 44, 8, 5
};

/* FM algorithm */
static const Bit32u fm_algorithm[4][6][8] = {
    {
        { 1, 1, 1, 1, 1, 1, 1, 1 }, /* OP1_0         */
        { 1, 1, 1, 1, 1, 1, 1, 1 }, /* OP1_1         */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* OP2           */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* Last operator */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* Last operator */
        { 0, 0, 0, 0, 0, 0, 0, 1 }  /* Out           */
    },
    {
        { 0, 1, 0, 0, 0, 1, 0, 0 }, /* OP1_0         */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* OP1_1         */
        { 1, 1, 1, 0, 0, 0, 0, 0 }, /* OP2           */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* Last operator */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* Last operator */
        { 0, 0, 0, 0, 0, 1, 1, 1 }  /* Out           */
    },
    {
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* OP1_0         */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* OP1_1         */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* OP2           */
        { 1, 0, 0, 1, 1, 1, 1, 0 }, /* Last operator */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* Last operator */
        { 0, 0, 0, 0, 1, 1, 1, 1 }  /* Out           */
    },
    {
        { 0, 0, 1, 0, 0, 1, 0, 0 }, /* OP1_0         */
        { 0, 0, 0, 0, 0, 0, 0, 0 }, /* OP1_1         */
        { 0, 0, 0, 1, 0, 0, 0, 0 }, /* OP2           */
        { 1, 1, 0, 1, 1, 0, 0, 0 }, /* Last operator */
        { 0, 0, 1, 0, 0, 0, 0, 0 }, /* Last operator */
        { 1, 1, 1, 1, 1, 1, 1, 1 }  /* Out           */
    }
};

static Bit32u chip_type = ym3438_type_discrete;

void OPN2_DoIO(ym3438_t *chip)
{
    /* Write signal check */
    chip->write_a_en = (chip->write_a & 0x03) == 0x01;
    chip->write_d_en = (chip->write_d & 0x03) == 0x01;
    chip->write_a <<= 1;
    chip->write_d <<= 1;
    /* Busy counter */
    chip->busy = chip->write_busy;
    chip->write_busy_cnt += chip->write_busy;
    chip->write_busy = (chip->write_busy && !(chip->write_busy_cnt >> 5)) || chip->write_d_en;
    chip->write_busy_cnt &= 0x1f;
}

void OPN2_DoRegWrite(ym3438_t *chip)
{
    Bit32u i;
    Bit32u slot = chip->cycles % 12;
    Bit32u address;
    Bit32u channel = chip->channel;
    /* Update registers */
    if (chip->write_fm_data)
    {
        /* Slot */
        if (op_offset[slot] == (chip->address & 0x107))
        {
            if (chip->address & 0x08)
            {
                /* OP2, OP4 */
                slot += 12;
            }
            address = chip->address & 0xf0;
            switch (address)
            {
            case 0x30: /* DT, MULTI */
                chip->multi[slot] = chip->data & 0x0f;
                if (!chip->multi[slot])
                {
                    chip->multi[slot] = 1;
                }
                else
                {
                    chip->multi[slot] <<= 1;
                }
                chip->dt[slot] = (chip->data >> 4) & 0x07;
                break;
            case 0x40: /* TL */
                chip->tl[slot] = chip->data & 0x7f;
                break;
            case 0x50: /* KS, AR */
                chip->ar[slot] = chip->data & 0x1f;
                chip->ks[slot] = (chip->data >> 6) & 0x03;
                break;
            case 0x60: /* AM, DR */
                chip->dr[slot] = chip->data & 0x1f;
                chip->am[slot] = (chip->data >> 7) & 0x01;
                break;
            case 0x70: /* SR */
                chip->sr[slot] = chip->data & 0x1f;
                break;
            case 0x80: /* SL, RR */
                chip->rr[slot] = chip->data & 0x0f;
                chip->sl[slot] = (chip->data >> 4) & 0x0f;
                chip->sl[slot] |= (chip->sl[slot] + 1) & 0x10;
                break;
            case 0x90: /* SSG-EG */
                chip->ssg_eg[slot] = chip->data & 0x0f;
                break;
            default:
                break;
            }
        }

        /* Channel */
        if (ch_offset[channel] == (chip->address & 0x103))
        {
            address = chip->address & 0xfc;
            switch (address)
            {
            case 0xa0:
                chip->fnum[channel] = (chip->data & 0xff) | ((chip->reg_a4 & 0x07) << 8);
                chip->block[channel] = (chip->reg_a4 >> 3) & 0x07;
                chip->kcode[channel] = (chip->block[channel] << 2) | fn_note[chip->fnum[channel] >> 7];
                break;
            case 0xa4:
                chip->reg_a4 = chip->data & 0xff;
                break;
            case 0xa8:
                chip->fnum_3ch[channel] = (chip->data & 0xff) | ((chip->reg_ac & 0x07) << 8);
                chip->block_3ch[channel] = (chip->reg_ac >> 3) & 0x07;
                chip->kcode_3ch[channel] = (chip->block_3ch[channel] << 2) | fn_note[chip->fnum_3ch[channel] >> 7];
                break;
            case 0xac:
                chip->reg_ac = chip->data & 0xff;
                break;
            case 0xb0:
                chip->connect[channel] = chip->data & 0x07;
                chip->fb[channel] = (chip->data >> 3) & 0x07;
                break;
            case 0xb4:
                chip->pms[channel] = chip->data & 0x07;
                chip->ams[channel] = (chip->data >> 4) & 0x03;
                chip->pan_l[channel] = (chip->data >> 7) & 0x01;
                chip->pan_r[channel] = (chip->data >> 6) & 0x01;
                break;
            default:
                break;
            }
        }
    }

    if (chip->write_a_en || chip->write_d_en)
    {
        /* Data */
        if (chip->write_a_en)
        {
            chip->write_fm_data = 0;
        }

        if (chip->write_fm_address && chip->write_d_en)
        {
            chip->write_fm_data = 1;
        }

        /* Address */
        if (chip->write_a_en)
        {
            if ((chip->write_data & 0xf0) != 0x00)
            {
                /* FM Write */
                chip->address = chip->write_data;
                chip->write_fm_address = 1;
            }
            else
            {
                /* SSG write */
                chip->write_fm_address = 0;
            }
        }

        /* FM Mode */
        /* Data */
        if (chip->write_d_en && (chip->write_data & 0x100) == 0)
        {
            switch (chip->address)
            {
            case 0x21: /* LSI test 1 */
                for (i = 0; i < 8; i++)
                {
                    chip->mode_test_21[i] = (chip->write_data >> i) & 0x01;
                }
                break;
            case 0x22: /* LFO control */
                if ((chip->write_data >> 3) & 0x01)
                {
                    chip->lfo_en = 0x7f;
                }
                else
                {
                    chip->lfo_en = 0;
                }
                chip->lfo_freq = chip->write_data & 0x07;
                break;
            case 0x24: /* Timer A */
                chip->timer_a_reg &= 0x03;
                chip->timer_a_reg |= (chip->write_data & 0xff) << 2;
                break;
            case 0x25:
                chip->timer_a_reg &= 0x3fc;
                chip->timer_a_reg |= chip->write_data & 0x03;
                break;
            case 0x26: /* Timer B */
                chip->timer_b_reg = chip->write_data & 0xff;
                break;
            case 0x27: /* CSM, Timer control */
                chip->mode_ch3 = (chip->write_data & 0xc0) >> 6;
                chip->mode_csm = chip->mode_ch3 == 2;
                chip->timer_a_load = chip->write_data & 0x01;
                chip->timer_a_enable = (chip->write_data >> 2) & 0x01;
                chip->timer_a_reset = (chip->write_data >> 4) & 0x01;
                chip->timer_b_load = (chip->write_data >> 1) & 0x01;
                chip->timer_b_enable = (chip->write_data >> 3) & 0x01;
                chip->timer_b_reset = (chip->write_data >> 5) & 0x01;
                break;
            case 0x28: /* Key on/off */
                for (i = 0; i < 4; i++)
                {
                    chip->mode_kon_operator[i] = (chip->write_data >> (4 + i)) & 0x01;
                }
                if ((chip->write_data & 0x03) == 0x03)
                {
                    /* Invalid address */
                    chip->mode_kon_channel = 0xff;
                }
                else
                {
                    chip->mode_kon_channel = (chip->write_data & 0x03) + ((chip->write_data >> 2) & 1) * 3;
                }
                break;
            case 0x2a: /* DAC data */
                chip->dacdata &= 0x01;
                chip->dacdata |= (chip->write_data ^ 0x80) << 1;
                break;
            case 0x2b: /* DAC enable */
                chip->dacen = chip->write_data >> 7;
                break;
            case 0x2c: /* LSI test 2 */
                for (i = 0; i < 8; i++)
                {
                    chip->mode_test_2c[i] = (chip->write_data >> i) & 0x01;
                }
                chip->dacdata &= 0x1fe;
                chip->dacdata |= chip->mode_test_2c[3];
                chip->eg_custom_timer = !chip->mode_test_2c[7] && chip->mode_test_2c[6];
                break;
            default:
                break;
            }
        }

        /* Address */
        if (chip->write_a_en)
        {
            chip->write_fm_mode_a = chip->write_data & 0xff;
        }
    }

    if (chip->write_fm_data)
    {
        chip->data = chip->write_data & 0xff;
    }
}

void OPN2_PhaseCalcIncrement(ym3438_t *chip)
{
    Bit32u chan = chip->channel;
    Bit32u slot = chip->cycles;
    Bit32u fnum = chip->pg_fnum;
    Bit32u fnum_h = fnum >> 4;
    Bit32u fm;
    Bit32u basefreq;
    Bit8u lfo = chip->lfo_pm;
    Bit8u lfo_l = lfo & 0x0f;
    Bit8u pms = chip->pms[chan];
    Bit8u dt = chip->dt[slot];
    Bit8u dt_l = dt & 0x03;
    Bit8u detune = 0;
    Bit8u block, note;
    Bit8u sum, sum_h, sum_l;
    Bit8u kcode = chip->pg_kcode;

    fnum <<= 1;
    /* Apply LFO */
    if (lfo_l & 0x08)
    {
        lfo_l ^= 0x0f;
    }
    fm = (fnum_h >> pg_lfo_sh1[pms][lfo_l]) + (fnum_h >> pg_lfo_sh2[pms][lfo_l]);
    if (pms > 5)
    {
        fm <<= pms - 5;
    }
    fm >>= 2;
    if (lfo & 0x10)
    {
        fnum -= fm;
    }
    else
    {
        fnum += fm;
    }
    fnum &= 0xfff;

    basefreq = (fnum << chip->pg_block) >> 2;

    /* Apply detune */
    if (dt_l)
    {
        if (kcode > 0x1c)
        {
            kcode = 0x1c;
        }
        block = kcode >> 2;
        note = kcode & 0x03;
        sum = block + 9 + ((dt_l == 3) | (dt_l & 0x02));
        sum_h = sum >> 1;
        sum_l = sum & 0x01;
        detune = pg_detune[(sum_l << 2) | note] >> (9 - sum_h);
    }
    if (dt & 0x04)
    {
        basefreq -= detune;
    }
    else
    {
        basefreq += detune;
    }
    basefreq &= 0x1ffff;
    chip->pg_inc[slot] = (basefreq * chip->multi[slot]) >> 1;
    chip->pg_inc[slot] &= 0xfffff;
}

void OPN2_PhaseGenerate(ym3438_t *chip)
{
    Bit32u slot;
    /* Mask increment */
    slot = (chip->cycles + 20) % 24;
    if (chip->pg_reset[slot])
    {
        chip->pg_inc[slot] = 0;
    }
    /* Phase step */
    slot = (chip->cycles + 19) % 24;
    chip->pg_phase[slot] += chip->pg_inc[slot];
    chip->pg_phase[slot] &= 0xfffff;
    if (chip->pg_reset[slot] || chip->mode_test_21[3])
    {
        chip->pg_phase[slot] = 0;
    }
}

void OPN2_EnvelopeSSGEG(ym3438_t *chip)
{
    Bit32u slot = chip->cycles;
    Bit8u direction = 0;
    chip->eg_ssg_pgrst_latch[slot] = 0;
    chip->eg_ssg_repeat_latch[slot] = 0;
    chip->eg_ssg_hold_up_latch[slot] = 0;
    chip->eg_ssg_inv[slot] = 0;
    if (chip->ssg_eg[slot] & 0x08)
    {
        direction = chip->eg_ssg_dir[slot];
        if (chip->eg_level[slot] & 0x200)
        {
            /* Reset */
            if ((chip->ssg_eg[slot] & 0x03) == 0x00)
            {
                chip->eg_ssg_pgrst_latch[slot] = 1;
            }
            /* Repeat */
            if ((chip->ssg_eg[slot] & 0x01) == 0x00)
            {
                chip->eg_ssg_repeat_latch[slot] = 1;
            }
            /* Inverse */
            if ((chip->ssg_eg[slot] & 0x03) == 0x02)
            {
                direction ^= 1;
            }
            if ((chip->ssg_eg[slot] & 0x03) == 0x03)
            {
                direction = 1;
            }
        }
        /* Hold up */
        if (chip->eg_kon_latch[slot]
         && ((chip->ssg_eg[slot] & 0x07) == 0x05 || (chip->ssg_eg[slot] & 0x07) == 0x03))
        {
            chip->eg_ssg_hold_up_latch[slot] = 1;
        }
        direction &= chip->eg_kon[slot];
        chip->eg_ssg_inv[slot] = (chip->eg_ssg_dir[slot] ^ ((chip->ssg_eg[slot] >> 2) & 0x01))
                               & chip->eg_kon[slot];
    }
    chip->eg_ssg_dir[slot] = direction;
    chip->eg_ssg_enable[slot] = (chip->ssg_eg[slot] >> 3) & 0x01;
}

void OPN2_EnvelopeADSR(ym3438_t *chip)
{
    Bit32u slot = (chip->cycles + 22) % 24;

    Bit8u nkon = chip->eg_kon_latch[slot];
    Bit8u okon = chip->eg_kon[slot];
    Bit8u kon_event;
    Bit8u koff_event;
    Bit8u eg_off;
    Bit16s level;
    Bit16s nextlevel = 0;
    Bit16s ssg_level;
    Bit8u nextstate = chip->eg_state[slot];
    Bit16s inc = 0;
    chip->eg_read[0] = chip->eg_read_inc;
    chip->eg_read_inc = chip->eg_inc > 0;

    /* Reset phase generator */
    chip->pg_reset[slot] = (nkon && !okon) || chip->eg_ssg_pgrst_latch[slot];

    /* KeyOn/Off */
    kon_event = (nkon && !okon) || (okon && chip->eg_ssg_repeat_latch[slot]);
    koff_event = okon && !nkon;

    ssg_level = level = (Bit16s)chip->eg_level[slot];

    if (chip->eg_ssg_inv[slot])
    {
        /* Inverse */
        ssg_level = 512 - level;
        ssg_level &= 0x3ff;
    }
    if (koff_event)
    {
        level = ssg_level;
    }
    if (chip->eg_ssg_enable[slot])
    {
        eg_off = level >> 9;
    }
    else
    {
        eg_off = (level & 0x3f0) == 0x3f0;
    }
    nextlevel = level;
    if (kon_event)
    {
        nextstate = eg_num_attack;
        /* Instant attack */
        if (chip->eg_ratemax)
        {
            nextlevel = 0;
        }
        else if (chip->eg_state[slot] == eg_num_attack && level != 0 && chip->eg_inc && nkon)
        {
            inc = (~level << chip->eg_inc) >> 5;
        }
    }
    else
    {
        switch (chip->eg_state[slot])
        {
        case eg_num_attack:
            if (level == 0)
            {
                nextstate = eg_num_decay;
            }
            else if(chip->eg_inc && !chip->eg_ratemax && nkon)
            {
                inc = (~level << chip->eg_inc) >> 5;
            }
            break;
        case eg_num_decay:
            if ((level >> 5) == chip->eg_sl[1])
            {
                nextstate = eg_num_sustain;
            }
            else if (!eg_off && chip->eg_inc)
            {
                inc = 1 << (chip->eg_inc - 1);
                if (chip->eg_ssg_enable[slot])
                {
                    inc <<= 2;
                }
            }
            break;
        case eg_num_sustain:
        case eg_num_release:
            if (!eg_off && chip->eg_inc)
            {
                inc = 1 << (chip->eg_inc - 1);
                if (chip->eg_ssg_enable[slot])
                {
                    inc <<= 2;
                }
            }
            break;
        default:
            break;
        }
        if (!nkon)
        {
            nextstate = eg_num_release;
        }
    }
    if (chip->eg_kon_csm[slot])
    {
        nextlevel |= chip->eg_tl[1] << 3;
    }

    /* Envelope off */
    if (!kon_event && !chip->eg_ssg_hold_up_latch[slot] && chip->eg_state[slot] != eg_num_attack && eg_off)
    {
        nextstate = eg_num_release;
        nextlevel = 0x3ff;
    }

    nextlevel += inc;

    chip->eg_kon[slot] = chip->eg_kon_latch[slot];
    chip->eg_level[slot] = (Bit16u)nextlevel & 0x3ff;
    chip->eg_state[slot] = nextstate;
}

void OPN2_EnvelopePrepare(ym3438_t *chip)
{
    Bit8u rate;
    Bit8u sum;
    Bit8u inc = 0;
    Bit32u slot = chip->cycles;
    Bit8u rate_sel;

    /* Prepare increment */
    rate = (chip->eg_rate << 1) + chip->eg_ksv;

    if (rate > 0x3f)
    {
        rate = 0x3f;
    }

    sum = ((rate >> 2) + chip->eg_shift_lock) & 0x0f;
    if (chip->eg_rate != 0 && chip->eg_quotient == 2)
    {
        if (rate < 48)
        {
            switch (sum)
            {
            case 12:
                inc = 1;
                break;
            case 13:
                inc = (rate >> 1) & 0x01;
                break;
            case 14:
                inc = rate & 0x01;
                break;
            default:
                break;
            }
        }
        else
        {
            inc = eg_stephi[rate & 0x03][chip->eg_timer_low_lock] + (rate >> 2) - 11;
            if (inc > 4)
            {
                inc = 4;
            }
        }
    }
    chip->eg_inc = inc;
    chip->eg_ratemax = (rate >> 1) == 0x1f;

    /* Prepare rate & ksv */
    rate_sel = chip->eg_state[slot];
    if ((chip->eg_kon[slot] && chip->eg_ssg_repeat_latch[slot])
     || (!chip->eg_kon[slot] && chip->eg_kon_latch[slot]))
    {
        rate_sel = eg_num_attack;
    }
    switch (rate_sel)
    {
    case eg_num_attack:
        chip->eg_rate = chip->ar[slot];
        break;
    case eg_num_decay:
        chip->eg_rate = chip->dr[slot];
        break;
    case eg_num_sustain:
        chip->eg_rate = chip->sr[slot];
        break;
    case eg_num_release:
        chip->eg_rate = (chip->rr[slot] << 1) | 0x01;
        break;
    default:
        break;
    }
    chip->eg_ksv = chip->pg_kcode >> (chip->ks[slot] ^ 0x03);
    if (chip->am[slot])
    {
        chip->eg_lfo_am = chip->lfo_am >> eg_am_shift[chip->ams[chip->channel]];
    }
    else
    {
        chip->eg_lfo_am = 0;
    }
    /* Delay TL & SL value */
    chip->eg_tl[1] = chip->eg_tl[0];
    chip->eg_tl[0] = chip->tl[slot];
    chip->eg_sl[1] = chip->eg_sl[0];
    chip->eg_sl[0] = chip->sl[slot];
}

void OPN2_EnvelopeGenerate(ym3438_t *chip)
{
    Bit32u slot = (chip->cycles + 23) % 24;
    Bit16u level;

    level = chip->eg_level[slot];

    if (chip->eg_ssg_inv[slot])
    {
        /* Inverse */
        level = 512 - level;
    }
    if (chip->mode_test_21[5])
    {
        level = 0;
    }
    level &= 0x3ff;

    /* Apply AM LFO */
    level += chip->eg_lfo_am;

    /* Apply TL */
    if (!(chip->mode_csm && chip->channel == 2 + 1))
    {
        level += chip->eg_tl[0] << 3;
    }
    if (level > 0x3ff)
    {
        level = 0x3ff;
    }
    chip->eg_out[slot] = level;
}

void OPN2_UpdateLFO(ym3438_t *chip)
{
    if ((chip->lfo_quotient & lfo_cycles[chip->lfo_freq]) == lfo_cycles[chip->lfo_freq])
    {
        chip->lfo_quotient = 0;
        chip->lfo_cnt++;
    }
    else
    {
        chip->lfo_quotient += chip->lfo_inc;
    }
    chip->lfo_cnt &= chip->lfo_en;
}

void OPN2_FMPrepare(ym3438_t *chip)
{
    Bit32u slot = (chip->cycles + 6) % 24;
    Bit32u channel = chip->channel;
    Bit16s mod, mod1, mod2;
    Bit32u op = slot / 6;
    Bit8u connect = chip->connect[channel];
    Bit32u prevslot = (chip->cycles + 18) % 24;

    /* Calculate modulation */
    mod1 = mod2 = 0;

    if (fm_algorithm[op][0][connect])
    {
        mod2 |= chip->fm_op1[channel][0];
    }
    if (fm_algorithm[op][1][connect])
    {
        mod1 |= chip->fm_op1[channel][1];
    }
    if (fm_algorithm[op][2][connect])
    {
        mod1 |= chip->fm_op2[channel];
    }
    if (fm_algorithm[op][3][connect])
    {
        mod2 |= chip->fm_out[prevslot];
    }
    if (fm_algorithm[op][4][connect])
    {
        mod1 |= chip->fm_out[prevslot];
    }
    mod = mod1 + mod2;
    if (op == 0)
    {
        /* Feedback */
        mod = mod >> (10 - chip->fb[channel]);
        if (!chip->fb[channel])
        {
            mod = 0;
        }
    }
    else
    {
        mod >>= 1;
    }
    chip->fm_mod[slot] = mod;

    slot = (chip->cycles + 18) % 24;
    /* OP1 */
    if (slot / 6 == 0)
    {
        chip->fm_op1[channel][1] = chip->fm_op1[channel][0];
        chip->fm_op1[channel][0] = chip->fm_out[slot];
    }
    /* OP2 */
    if (slot / 6 == 2)
    {
        chip->fm_op2[channel] = chip->fm_out[slot];
    }
}

void OPN2_ChGenerate(ym3438_t *chip)
{
    Bit32u slot = (chip->cycles + 18) % 24;
    Bit32u channel = chip->channel;
    Bit32u op = slot / 6;
    Bit32u test_dac = chip->mode_test_2c[5];
    Bit16s acc = chip->ch_acc[channel];
    Bit16s add = test_dac;
    Bit16s sum = 0;
    if (op == 0 && !test_dac)
    {
        acc = 0;
    }
    if (fm_algorithm[op][5][chip->connect[channel]] && !test_dac)
    {
        add += chip->fm_out[slot] >> 5;
    }
    sum = acc + add;
    /* Clamp */
    if (sum > 255)
    {
        sum = 255;
    }
    else if(sum < -256)
    {
        sum = -256;
    }

    if (op == 0 || test_dac)
    {
        chip->ch_out[channel] = chip->ch_acc[channel];
    }
    chip->ch_acc[channel] = sum;
}

void OPN2_ChOutput(ym3438_t *chip)
{
    Bit32u cycles = chip->cycles;
    Bit32u slot = chip->cycles;
    Bit32u channel = chip->channel;
    Bit32u test_dac = chip->mode_test_2c[5];
    Bit16s out;
    Bit16s sign;
    Bit32u out_en;
    chip->ch_read = chip->ch_lock;
    if (slot < 12)
    {
        /* Ch 4,5,6 */
        channel++;
    }
    if ((cycles & 3) == 0)
    {
        if (!test_dac)
        {
            /* Lock value */
            chip->ch_lock = chip->ch_out[channel];
        }
        chip->ch_lock_l = chip->pan_l[channel];
        chip->ch_lock_r = chip->pan_r[channel];
    }
    /* Ch 6 */
    if (((cycles >> 2) == 1 && chip->dacen) || test_dac)
    {
        out = (Bit16s)chip->dacdata;
        out <<= 7;
        out >>= 7;
    }
    else
    {
        out = chip->ch_lock;
    }
    chip->mol = 0;
    chip->mor = 0;

    if (chip_type == ym3438_type_ym2612)
    {
        out_en = ((cycles & 3) == 3) || test_dac;
        /* YM2612 DAC emulation(not verified) */
        sign = out >> 8;
        if (out >= 0)
        {
            out++;
            sign++;
        }
        if (chip->ch_lock_l && out_en)
        {
            chip->mol = out;
        }
        else
        {
            chip->mol = sign;
        }
        if (chip->ch_lock_r && out_en)
        {
            chip->mor = out;
        }
        else
        {
            chip->mor = sign;
        }
        /* Amplify signal */
        chip->mol *= 3;
        chip->mor *= 3;
    }
    else
    {
        out_en = ((cycles & 3) != 0) || test_dac;
        /* Discrete YM3438 seems has the ladder effect too */
        if (out >= 0 && chip_type == ym3438_type_discrete)
        {
            out++;
        }
        if (chip->ch_lock_l && out_en)
        {
            chip->mol = out;
        }
        if (chip->ch_lock_r && out_en)
        {
            chip->mor = out;
        }
    }
}

void OPN2_FMGenerate(ym3438_t *chip)
{
    Bit32u slot = (chip->cycles + 19) % 24;
    /* Calculate phase */
    Bit16u phase = (chip->fm_mod[slot] + (chip->pg_phase[slot] >> 10)) & 0x3ff;
    Bit16u quarter;
    Bit16u level;
    Bit16s output;
    if (phase & 0x100)
    {
        quarter = (phase ^ 0xff) & 0xff;
    }
    else
    {
        quarter = phase & 0xff;
    }
    level = logsinrom[quarter];
    /* Apply envelope */
    level += chip->eg_out[slot] << 2;
    /* Transform */
    if (level > 0x1fff)
    {
        level = 0x1fff;
    }
    output = ((exprom[(level & 0xff) ^ 0xff] | 0x400) << 2) >> (level >> 8);
    if (phase & 0x200)
    {
        output = ((~output) ^ (chip->mode_test_21[4] << 13)) + 1;
    }
    else
    {
        output = output ^ (chip->mode_test_21[4] << 13);
    }
    output <<= 2;
    output >>= 2;
    chip->fm_out[slot] = output;
}

void OPN2_DoTimerA(ym3438_t *chip)
{
    Bit16u time;
    Bit8u load;
    load = chip->timer_a_overflow;
    if (chip->cycles == 2)
    {
        /* Lock load value */
        load |= (!chip->timer_a_load_lock && chip->timer_a_load);
        chip->timer_a_load_lock = chip->timer_a_load;
        if (chip->mode_csm)
        {
            /* CSM KeyOn */
            chip->mode_kon_csm = load;
        }
        else
        {
            chip->mode_kon_csm = 0;
        }
    }
    /* Load counter */
    if (chip->timer_a_load_latch)
    {
        time = chip->timer_a_reg;
    }
    else
    {
        time = chip->timer_a_cnt;
    }
    chip->timer_a_load_latch = load;
    /* Increase counter */
    if ((chip->cycles == 1 && chip->timer_a_load_lock) || chip->mode_test_21[2])
    {
        time++;
    }
    /* Set overflow flag */
    if (chip->timer_a_reset)
    {
        chip->timer_a_reset = 0;
        chip->timer_a_overflow_flag = 0;
    }
    else
    {
        chip->timer_a_overflow_flag |= chip->timer_a_overflow & chip->timer_a_enable;
    }
    chip->timer_a_overflow = (time >> 10);
    chip->timer_a_cnt = time & 0x3ff;
}

void OPN2_DoTimerB(ym3438_t *chip)
{
    Bit16u time;
    Bit8u load;
    load = chip->timer_b_overflow;
    if (chip->cycles == 2)
    {
        /* Lock load value */
        load |= (!chip->timer_b_load_lock && chip->timer_b_load);
        chip->timer_b_load_lock = chip->timer_b_load;
    }
    /* Load counter */
    if (chip->timer_b_load_latch)
    {
        time = chip->timer_b_reg;
    }
    else
    {
        time = chip->timer_b_cnt;
    }
    chip->timer_b_load_latch = load;
    /* Increase counter */
    if (chip->cycles == 1)
    {
        chip->timer_b_subcnt++;
    }
    if ((chip->timer_b_subcnt == 0x10 && chip->timer_b_load_lock) || chip->mode_test_21[2])
    {
        time++;
    }
    chip->timer_b_subcnt &= 0x0f;
    /* Set overflow flag */
    if (chip->timer_b_reset)
    {
        chip->timer_b_reset = 0;
        chip->timer_b_overflow_flag = 0;
    }
    else
    {
        chip->timer_b_overflow_flag |= chip->timer_b_overflow & chip->timer_b_enable;
    }
    chip->timer_b_overflow = (time >> 8);
    chip->timer_b_cnt = time & 0xff;
}

void OPN2_KeyOn(ym3438_t*chip)
{
    Bit32u slot = chip->cycles;
    Bit32u chan = chip->channel;
    /* Key On */
    chip->eg_kon_latch[slot] = chip->mode_kon[slot];
    chip->eg_kon_csm[slot] = 0;
    if (chip->channel == 2 && chip->mode_kon_csm)
    {
        /* CSM Key On */
        chip->eg_kon_latch[slot] = 1;
        chip->eg_kon_csm[slot] = 1;
    }
    if (chip->cycles == chip->mode_kon_channel)
    {
        /* OP1 */
        chip->mode_kon[chan] = chip->mode_kon_operator[0];
        /* OP2 */
        chip->mode_kon[chan + 12] = chip->mode_kon_operator[1];
        /* OP3 */
        chip->mode_kon[chan + 6] = chip->mode_kon_operator[2];
        /* OP4 */
        chip->mode_kon[chan + 18] = chip->mode_kon_operator[3];
    }
}

void OPN2_Reset(ym3438_t *chip, Bit32u rate, Bit32u clock)
{
    Bit32u i, rateratio;
    rateratio = (Bit32u)chip->rateratio;
    memset(chip, 0, sizeof(ym3438_t));
    for (i = 0; i < 24; i++)
    {
        chip->eg_out[i] = 0x3ff;
        chip->eg_level[i] = 0x3ff;
        chip->eg_state[i] = eg_num_release;
        chip->multi[i] = 1;
    }
    for (i = 0; i < 6; i++)
    {
        chip->pan_l[i] = 1;
        chip->pan_r[i] = 1;
    }

    if (rate != 0)
    {
        chip->rateratio = (Bit32s)(Bit32u)((((Bit64u)144 * rate) << RSM_FRAC) / clock);
    }
    else
    {
        chip->rateratio = (Bit32s)rateratio;
    }
}

void OPN2_SetChipType(Bit32u type)
{
    chip_type = type;
}

void OPN2_Clock(ym3438_t *chip, Bit16s *buffer)
{
    Bit32u slot = chip->cycles;
    chip->lfo_inc = chip->mode_test_21[1];
    chip->pg_read >>= 1;
    chip->eg_read[1] >>= 1;
    chip->eg_cycle++;
    /* Lock envelope generator timer value */
    if (chip->cycles == 1 && chip->eg_quotient == 2)
    {
        if (chip->eg_cycle_stop)
        {
            chip->eg_shift_lock = 0;
        }
        else
        {
            chip->eg_shift_lock = chip->eg_shift + 1;
        }
        chip->eg_timer_low_lock = chip->eg_timer & 0x03;
    }
    /* Cycle specific functions */
    switch (chip->cycles)
    {
    case 0:
        chip->lfo_pm = chip->lfo_cnt >> 2;
        if (chip->lfo_cnt & 0x40)
        {
            chip->lfo_am = chip->lfo_cnt & 0x3f;
        }
        else
        {
            chip->lfo_am = chip->lfo_cnt ^ 0x3f;
        }
        chip->lfo_am <<= 1;
        break;
    case 1:
        chip->eg_quotient++;
        chip->eg_quotient %= 3;
        chip->eg_cycle = 0;
        chip->eg_cycle_stop = 1;
        chip->eg_shift = 0;
        chip->eg_timer_inc |= chip->eg_quotient >> 1;
        chip->eg_timer = chip->eg_timer + chip->eg_timer_inc;
        chip->eg_timer_inc = chip->eg_timer >> 12;
        chip->eg_timer &= 0xfff;
        break;
    case 2:
        chip->pg_read = chip->pg_phase[21] & 0x3ff;
        chip->eg_read[1] = chip->eg_out[0];
        break;
    case 13:
        chip->eg_cycle = 0;
        chip->eg_cycle_stop = 1;
        chip->eg_shift = 0;
        chip->eg_timer = chip->eg_timer + chip->eg_timer_inc;
        chip->eg_timer_inc = chip->eg_timer >> 12;
        chip->eg_timer &= 0xfff;
        break;
    case 23:
        chip->lfo_inc |= 1;
        break;
    }
    chip->eg_timer &= ~(chip->mode_test_21[5] << chip->eg_cycle);
    if (((chip->eg_timer >> chip->eg_cycle) | (chip->pin_test_in & chip->eg_custom_timer)) & chip->eg_cycle_stop)
    {
        chip->eg_shift = chip->eg_cycle;
        chip->eg_cycle_stop = 0;
    }

    OPN2_DoIO(chip);

    OPN2_DoTimerA(chip);
    OPN2_DoTimerB(chip);
    OPN2_KeyOn(chip);

    OPN2_ChOutput(chip);
    OPN2_ChGenerate(chip);

    OPN2_FMPrepare(chip);
    OPN2_FMGenerate(chip);

    OPN2_PhaseGenerate(chip);
    OPN2_PhaseCalcIncrement(chip);

    OPN2_EnvelopeADSR(chip);
    OPN2_EnvelopeGenerate(chip);
    OPN2_EnvelopeSSGEG(chip);
    OPN2_EnvelopePrepare(chip);

    /* Prepare fnum & block */
    if (chip->mode_ch3)
    {
        /* Channel 3 special mode */
        switch (slot)
        {
        case 1: /* OP1 */
            chip->pg_fnum = chip->fnum_3ch[1];
            chip->pg_block = chip->block_3ch[1];
            chip->pg_kcode = chip->kcode_3ch[1];
            break;
        case 7: /* OP3 */
            chip->pg_fnum = chip->fnum_3ch[0];
            chip->pg_block = chip->block_3ch[0];
            chip->pg_kcode = chip->kcode_3ch[0];
            break;
        case 13: /* OP2 */
            chip->pg_fnum = chip->fnum_3ch[2];
            chip->pg_block = chip->block_3ch[2];
            chip->pg_kcode = chip->kcode_3ch[2];
            break;
        case 19: /* OP4 */
        default:
            chip->pg_fnum = chip->fnum[(chip->channel + 1) % 6];
            chip->pg_block = chip->block[(chip->channel + 1) % 6];
            chip->pg_kcode = chip->kcode[(chip->channel + 1) % 6];
            break;
        }
    }
    else
    {
        chip->pg_fnum = chip->fnum[(chip->channel + 1) % 6];
        chip->pg_block = chip->block[(chip->channel + 1) % 6];
        chip->pg_kcode = chip->kcode[(chip->channel + 1) % 6];
    }

    OPN2_UpdateLFO(chip);
    OPN2_DoRegWrite(chip);
    chip->cycles = (chip->cycles + 1) % 24;
    chip->channel = chip->cycles % 6;

    buffer[0] = chip->mol;
    buffer[1] = chip->mor;
}

void OPN2_Write(ym3438_t *chip, Bit32u port, Bit8u data)
{
    port &= 3;
    chip->write_data = ((port << 7) & 0x100) | data;
    if (port & 1)
    {
        /* Data */
        chip->write_d |= 1;
    }
    else
    {
        /* Address */
        chip->write_a |= 1;
    }
}

void OPN2_SetTestPin(ym3438_t *chip, Bit32u value)
{
    chip->pin_test_in = value & 1;
}

Bit32u OPN2_ReadTestPin(ym3438_t *chip)
{
    if (!chip->mode_test_2c[7])
    {
        return 0;
    }
    return chip->cycles == 23;
}

Bit32u OPN2_ReadIRQPin(ym3438_t *chip)
{
    return chip->timer_a_overflow_flag | chip->timer_b_overflow_flag;
}

Bit8u OPN2_Read(ym3438_t *chip, Bit32u port)
{
    if ((port & 3) == 0 || chip_type == ym3438_type_asic)
    {
        if (chip->mode_test_21[6])
        {
            /* Read test data */
            Bit32u slot = (chip->cycles + 18) % 24;
            Bit16u testdata = ((chip->pg_read & 0x01) << 15)
                            | ((chip->eg_read[chip->mode_test_21[0]] & 0x01) << 14);
            if (chip->mode_test_2c[4])
            {
                testdata |= chip->ch_read & 0x1ff;
            }
            else
            {
                testdata |= chip->fm_out[slot] & 0x3fff;
            }
            if (chip->mode_test_21[7])
            {
                return testdata & 0xff;
            }
            else
            {
                return testdata >> 8;
            }
        }
        else
        {
            return (Bit8u)(chip->busy << 7) | (Bit8u)(chip->timer_b_overflow_flag << 1)
                    | (Bit8u)chip->timer_a_overflow_flag;
        }
    }
    return 0;
}

void OPN2_WriteBuffered(ym3438_t *chip, Bit32u port, Bit8u data)
{
    Bit64u time1, time2;
    Bit16s buffer[2];
    Bit64u skip;

    if (chip->writebuf[chip->writebuf_last].port & 0x04)
    {
        OPN2_Write(chip, chip->writebuf[chip->writebuf_last].port & 0X03,
                   chip->writebuf[chip->writebuf_last].data);

        chip->writebuf_cur = (chip->writebuf_last + 1) % OPN_WRITEBUF_SIZE;
        skip = chip->writebuf[chip->writebuf_last].time - chip->writebuf_samplecnt;
        chip->writebuf_samplecnt = chip->writebuf[chip->writebuf_last].time;
        while (skip--)
        {
            OPN2_Clock(chip, buffer);
        }
    }

    chip->writebuf[chip->writebuf_last].port = (port & 0x03) | 0x04;
    chip->writebuf[chip->writebuf_last].data = data;
    time1 = chip->writebuf_lasttime + OPN_WRITEBUF_DELAY;
    time2 = chip->writebuf_samplecnt;

    if (time1 < time2)
    {
        time1 = time2;
    }

    chip->writebuf[chip->writebuf_last].time = time1;
    chip->writebuf_lasttime = time1;
    chip->writebuf_last = (chip->writebuf_last + 1) % OPN_WRITEBUF_SIZE;
}

void OPN2_Generate(ym3438_t *chip, Bit16s *buf)
{
    Bit32u i;
    Bit16s buffer[2];
    Bit32u mute;

    buf[0] = 0;
    buf[1] = 0;

    for (i = 0; i < 24; i++)
    {
        switch (chip->cycles >> 2)
        {
        case 0: /* Ch 2 */
            mute = chip->mute[1];
            break;
        case 1: /* Ch 6, DAC */
            mute = chip->mute[5 + chip->dacen];
            break;
        case 2: /* Ch 4 */
            mute = chip->mute[3];
            break;
        case 3: /* Ch 1 */
            mute = chip->mute[0];
            break;
        case 4: /* Ch 5 */
            mute = chip->mute[4];
            break;
        case 5: /* Ch 3 */
            mute = chip->mute[2];
            break;
        default:
            mute = 0;
            break;
        }
        OPN2_Clock(chip, buffer);
        if (!mute)
        {
            buf[0] += buffer[0];
            buf[1] += buffer[1];
        }

        while (chip->writebuf[chip->writebuf_cur].time <= chip->writebuf_samplecnt)
        {
            if (!(chip->writebuf[chip->writebuf_cur].port & 0x04))
            {
                break;
            }
            chip->writebuf[chip->writebuf_cur].port &= 0x03;
            OPN2_Write(chip, chip->writebuf[chip->writebuf_cur].port,
                       chip->writebuf[chip->writebuf_cur].data);
            chip->writebuf_cur = (chip->writebuf_cur + 1) % OPN_WRITEBUF_SIZE;
        }
        chip->writebuf_samplecnt++;
    }
}

void OPN2_GenerateResampled(ym3438_t *chip, Bit16s *buf)
{
    Bit16s buffer[2];

    while (chip->samplecnt >= chip->rateratio)
    {
        chip->oldsamples[0] = chip->samples[0];
        chip->oldsamples[1] = chip->samples[1];
        OPN2_Generate(chip, buffer);
        chip->samples[0] = buffer[0] * 11;
        chip->samples[1] = buffer[1] * 11;
        chip->samplecnt -= chip->rateratio;
    }
    buf[0] = (Bit16s)(((chip->oldsamples[0] * (chip->rateratio - chip->samplecnt)
                     + chip->samples[0] * chip->samplecnt) / chip->rateratio)>>1);
    buf[1] = (Bit16s)(((chip->oldsamples[1] * (chip->rateratio - chip->samplecnt)
                     + chip->samples[1] * chip->samplecnt) / chip->rateratio)>>1);
    chip->samplecnt += 1 << RSM_FRAC;
}

void OPN2_GenerateStream(ym3438_t *chip, Bit16s *output, Bit32u numsamples)
{
    Bit32u i;
    Bit16s buffer[2];

    for (i = 0; i < numsamples; i++)
    {
        OPN2_GenerateResampled(chip, buffer);
        *output++ = buffer[0];
        *output++ = buffer[1];
    }
}

void OPN2_GenerateStreamMix(ym3438_t *chip, Bit16s *output, Bit32u numsamples)
{
    Bit32u i;
    Bit16s buffer[2];

    for (i = 0; i < numsamples; i++)
    {
        OPN2_GenerateResampled(chip, buffer);
        *output++ += buffer[0];
        *output++ += buffer[1];
    }
}


void OPN2_SetOptions(Bit8u flags)
{
    switch ((flags >> 3) & 0x03)
    {
    case 0x00: /* YM2612 */
    default:
        OPN2_SetChipType(ym3438_type_ym2612);
        break;
    case 0x01: /* ASIC YM3438 */
        OPN2_SetChipType(ym3438_type_asic);
        break;
    case 0x02: /* Discrete YM3438 */
        OPN2_SetChipType(ym3438_type_discrete);
        break;
    }
}

void OPN2_SetMute(ym3438_t *chip, Bit32u mute)
{
    Bit32u i;
    for (i = 0; i < 7; i++)
    {
        chip->mute[i] = (mute >> i) & 0x01;
    }
}


} // Ym2612_NukedImpl


Ym2612_Nuked_Emu::Ym2612_Nuked_Emu()
{
	Ym2612_NukedImpl::OPN2_SetChipType( Ym2612_NukedImpl::ym3438_type_asic );
	impl = new Ym2612_NukedImpl::ym3438_t;
}

Ym2612_Nuked_Emu::~Ym2612_Nuked_Emu()
{
	Ym2612_NukedImpl::ym3438_t *chip_r = reinterpret_cast<Ym2612_NukedImpl::ym3438_t*>(impl);
	if ( chip_r ) delete chip_r;
}

const char *Ym2612_Nuked_Emu::set_rate(double sample_rate, double clock_rate)
{
	Ym2612_NukedImpl::ym3438_t *chip_r = reinterpret_cast<Ym2612_NukedImpl::ym3438_t*>(impl);
	if ( !chip_r )
		return "Out of memory";
	prev_sample_rate = sample_rate;
	prev_clock_rate = clock_rate;
	Ym2612_NukedImpl::OPN2_Reset( chip_r, static_cast<Bit32u>(sample_rate), static_cast<Bit32u>(clock_rate) );
	return 0;
}

void Ym2612_Nuked_Emu::reset()
{
	Ym2612_NukedImpl::ym3438_t *chip_r = reinterpret_cast<Ym2612_NukedImpl::ym3438_t*>(impl);
	if ( !chip_r ) Ym2612_NukedImpl::OPN2_Reset( chip_r, static_cast<Bit32u>(prev_sample_rate), static_cast<Bit32u>(prev_clock_rate) );
}

void Ym2612_Nuked_Emu::mute_voices(int mask)
{
	Ym2612_NukedImpl::ym3438_t *chip_r = reinterpret_cast<Ym2612_NukedImpl::ym3438_t*>(impl);
	if ( chip_r ) Ym2612_NukedImpl::OPN2_SetMute( chip_r, mask );
}

void Ym2612_Nuked_Emu::write0(int addr, int data)
{
	Ym2612_NukedImpl::ym3438_t *chip_r = reinterpret_cast<Ym2612_NukedImpl::ym3438_t*>(impl);
	if ( !chip_r ) return;
	Ym2612_NukedImpl::OPN2_WriteBuffered( chip_r, 0, static_cast<Bit8u>(addr) );
	Ym2612_NukedImpl::OPN2_WriteBuffered( chip_r, 1, static_cast<Bit8u>(data) );
}

void Ym2612_Nuked_Emu::write1(int addr, int data)
{
	Ym2612_NukedImpl::ym3438_t *chip_r = reinterpret_cast<Ym2612_NukedImpl::ym3438_t*>(impl);
	if ( !chip_r ) return;
	Ym2612_NukedImpl::OPN2_WriteBuffered( chip_r, 0 + 2, static_cast<Bit8u>(addr) );
	Ym2612_NukedImpl::OPN2_WriteBuffered( chip_r, 1 + 2, static_cast<Bit8u>(data) );
}

void Ym2612_Nuked_Emu::run(int pair_count, Ym2612_Nuked_Emu::sample_t *out)
{
	Ym2612_NukedImpl::ym3438_t *chip_r = reinterpret_cast<Ym2612_NukedImpl::ym3438_t*>(impl);
	if ( !chip_r ) return;
	Ym2612_NukedImpl::OPN2_GenerateStream(chip_r, out, pair_count);
}
