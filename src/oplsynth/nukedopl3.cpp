/*
*  Copyright (C) 2013-2014 Nuke.YKT
*
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Lesser General Public
*  License as published by the Free Software Foundation; either
*  version 2.1 of the License, or (at your option) any later version.
* 
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Lesser General Public License for more details.
* 
*  You should have received a copy of the GNU Lesser General Public
*  License along with this library; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
	Nuked Yamaha YMF262(aka OPL3) emulator.
	Thanks:
		MAME Development Team:
			Feedback and Rhythm part calculation information.
		forums.submarine.org.uk(carbon14, opl3):
			Tremolo and phase generator calculation information.
*/

//version 1.4.2

/* Changelog:
	v1.1:
		Vibrato's sign fix
	v1.2:
		Operator key fix
		Corrected 4-operator mode
		Corrected rhythm mode
		Some small fixes
	v1.2.1:
		Small envelope generator fix
		Removed EX_Get function(not used)
	v1.3:
		Complete rewrite
		(Not released)
	v1.4:
		New envelope and waveform generator
		Some small fixes.
		(Not released)
	v1.4.1:
		Envelope generator rate calculation fix
		(Not released)
	v1.4.2:
		Version for ZDoom.
*/


/* Verified:
	Noise generator.
	Waveform generator.
	Envelope generator increase table.
	Tremolo.
*/

/* TODO:
	Verify:
		kslrom[15] value(is it 128?).
		Sustain level = 15.
		Vibrato, Phase generator.
		Rhythm part.
		Envelope generator state switching(decay->sustain when egt = 1 and decay->release).
		Feedback.
		Register write.
		4-operator.
*/

#include <stdlib.h>
#include <string.h>
#include "nukedopl3.h"

// Channel types

enum {
	ch_4op2,
	ch_2op,
	ch_4op,
	ch_drum
};

// Envelope generator states

enum {
	eg_off,
	eg_attack,
	eg_decay,
	eg_sustain,
	eg_release
};

// Envelope key types

enum {
	egk_norm = 1,
	egk_drum = 2
};

//
// logsin table
//

static const Bit16u logsinrom[256] = {
	0x859, 0x6c3, 0x607, 0x58b, 0x52e, 0x4e4, 0x4a6, 0x471, 0x443, 0x41a, 0x3f5, 0x3d3, 0x3b5, 0x398, 0x37e, 0x365,
	0x34e, 0x339, 0x324, 0x311, 0x2ff, 0x2ed, 0x2dc, 0x2cd, 0x2bd, 0x2af, 0x2a0, 0x293, 0x286, 0x279, 0x26d, 0x261,
	0x256, 0x24b, 0x240, 0x236, 0x22c, 0x222, 0x218, 0x20f, 0x206, 0x1fd, 0x1f5, 0x1ec, 0x1e4, 0x1dc, 0x1d4, 0x1cd,
	0x1c5, 0x1be, 0x1b7, 0x1b0, 0x1a9, 0x1a2, 0x19b, 0x195, 0x18f, 0x188, 0x182, 0x17c, 0x177, 0x171, 0x16b, 0x166,
	0x160, 0x15b, 0x155, 0x150, 0x14b, 0x146, 0x141, 0x13c, 0x137, 0x133, 0x12e, 0x129, 0x125, 0x121, 0x11c, 0x118,
	0x114, 0x10f, 0x10b, 0x107, 0x103, 0x0ff, 0x0fb, 0x0f8, 0x0f4, 0x0f0, 0x0ec, 0x0e9, 0x0e5, 0x0e2, 0x0de, 0x0db,
	0x0d7, 0x0d4, 0x0d1, 0x0cd, 0x0ca, 0x0c7, 0x0c4, 0x0c1, 0x0be, 0x0bb, 0x0b8, 0x0b5, 0x0b2, 0x0af, 0x0ac, 0x0a9,
	0x0a7, 0x0a4, 0x0a1, 0x09f, 0x09c, 0x099, 0x097, 0x094, 0x092, 0x08f, 0x08d, 0x08a, 0x088, 0x086, 0x083, 0x081,
	0x07f, 0x07d, 0x07a, 0x078, 0x076, 0x074, 0x072, 0x070, 0x06e, 0x06c, 0x06a, 0x068, 0x066, 0x064, 0x062, 0x060,
	0x05e, 0x05c, 0x05b, 0x059, 0x057, 0x055, 0x053, 0x052, 0x050, 0x04e, 0x04d, 0x04b, 0x04a, 0x048, 0x046, 0x045,
	0x043, 0x042, 0x040, 0x03f, 0x03e, 0x03c, 0x03b, 0x039, 0x038, 0x037, 0x035, 0x034, 0x033, 0x031, 0x030, 0x02f,
	0x02e, 0x02d, 0x02b, 0x02a, 0x029, 0x028, 0x027, 0x026, 0x025, 0x024, 0x023, 0x022, 0x021, 0x020, 0x01f, 0x01e,
	0x01d, 0x01c, 0x01b, 0x01a, 0x019, 0x018, 0x017, 0x017, 0x016, 0x015, 0x014, 0x014, 0x013, 0x012, 0x011, 0x011,
	0x010, 0x00f, 0x00f, 0x00e, 0x00d, 0x00d, 0x00c, 0x00c, 0x00b, 0x00a, 0x00a, 0x009, 0x009, 0x008, 0x008, 0x007,
	0x007, 0x007, 0x006, 0x006, 0x005, 0x005, 0x005, 0x004, 0x004, 0x004, 0x003, 0x003, 0x003, 0x002, 0x002, 0x002,
	0x002, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000
};

//
// exp table
//

static const Bit16u exprom[256] = {
	0x000, 0x003, 0x006, 0x008, 0x00b, 0x00e, 0x011, 0x014, 0x016, 0x019, 0x01c, 0x01f, 0x022, 0x025, 0x028, 0x02a,
	0x02d, 0x030, 0x033, 0x036, 0x039, 0x03c, 0x03f, 0x042, 0x045, 0x048, 0x04b, 0x04e, 0x051, 0x054, 0x057, 0x05a,
	0x05d, 0x060, 0x063, 0x066, 0x069, 0x06c, 0x06f, 0x072, 0x075, 0x078, 0x07b, 0x07e, 0x082, 0x085, 0x088, 0x08b,
	0x08e, 0x091, 0x094, 0x098, 0x09b, 0x09e, 0x0a1, 0x0a4, 0x0a8, 0x0ab, 0x0ae, 0x0b1, 0x0b5, 0x0b8, 0x0bb, 0x0be,
	0x0c2, 0x0c5, 0x0c8, 0x0cc, 0x0cf, 0x0d2, 0x0d6, 0x0d9, 0x0dc, 0x0e0, 0x0e3, 0x0e7, 0x0ea, 0x0ed, 0x0f1, 0x0f4,
	0x0f8, 0x0fb, 0x0ff, 0x102, 0x106, 0x109, 0x10c, 0x110, 0x114, 0x117, 0x11b, 0x11e, 0x122, 0x125, 0x129, 0x12c,
	0x130, 0x134, 0x137, 0x13b, 0x13e, 0x142, 0x146, 0x149, 0x14d, 0x151, 0x154, 0x158, 0x15c, 0x160, 0x163, 0x167,
	0x16b, 0x16f, 0x172, 0x176, 0x17a, 0x17e, 0x181, 0x185, 0x189, 0x18d, 0x191, 0x195, 0x199, 0x19c, 0x1a0, 0x1a4,
	0x1a8, 0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c0, 0x1c4, 0x1c8, 0x1cc, 0x1d0, 0x1d4, 0x1d8, 0x1dc, 0x1e0, 0x1e4,
	0x1e8, 0x1ec, 0x1f0, 0x1f5, 0x1f9, 0x1fd, 0x201, 0x205, 0x209, 0x20e, 0x212, 0x216, 0x21a, 0x21e, 0x223, 0x227,
	0x22b, 0x230, 0x234, 0x238, 0x23c, 0x241, 0x245, 0x249, 0x24e, 0x252, 0x257, 0x25b, 0x25f, 0x264, 0x268, 0x26d,
	0x271, 0x276, 0x27a, 0x27f, 0x283, 0x288, 0x28c, 0x291, 0x295, 0x29a, 0x29e, 0x2a3, 0x2a8, 0x2ac, 0x2b1, 0x2b5,
	0x2ba, 0x2bf, 0x2c4, 0x2c8, 0x2cd, 0x2d2, 0x2d6, 0x2db, 0x2e0, 0x2e5, 0x2e9, 0x2ee, 0x2f3, 0x2f8, 0x2fd, 0x302,
	0x306, 0x30b, 0x310, 0x315, 0x31a, 0x31f, 0x324, 0x329, 0x32e, 0x333, 0x338, 0x33d, 0x342, 0x347, 0x34c, 0x351,
	0x356, 0x35b, 0x360, 0x365, 0x36a, 0x370, 0x375, 0x37a, 0x37f, 0x384, 0x38a, 0x38f, 0x394, 0x399, 0x39f, 0x3a4,
	0x3a9, 0x3ae, 0x3b4, 0x3b9, 0x3bf, 0x3c4, 0x3c9, 0x3cf, 0x3d4, 0x3da, 0x3df, 0x3e4, 0x3ea, 0x3ef, 0x3f5, 0x3fa
};

// 
// freq mult table multiplied by 2
//
// 1/2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15
//

static const Bit8u mt[16] = { 1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30 };

//
// ksl table
//

static const Bit8u kslrom[16] = { 0, 64, 80, 90, 96, 102, 106, 110, 112, 116, 118, 120, 122, 124, 126, 127 };

static const Bit8u kslshift[4] = { 8, 1, 2, 0 };

//
// LFO vibrato
//

static const Bit8u vib_table[8] = { 3, 1, 0, 1, 3, 1, 0, 1 };
static const Bit8s vibsgn_table[8] = { 1, 1, 1, 1, -1, -1, -1, -1 };

//
// envelope generator constants
//

static const Bit8u eg_incstep[3][4][8] = {
		{ { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 } },
		{ { 0, 1, 0, 1, 0, 1, 0, 1 }, { 1, 1, 0, 1, 0, 1, 0, 1 }, { 1, 1, 0, 1, 1, 1, 0, 1 }, { 1, 1, 1, 1, 1, 1, 0, 1 } },
		{ { 1, 1, 1, 1, 1, 1, 1, 1 }, { 2, 2, 1, 1, 1, 1, 1, 1 }, { 2, 2, 1, 1, 2, 2, 1, 1 }, { 2, 2, 2, 2, 2, 2, 1, 1 } }
};

//
// address decoding
//

static const Bit8s ad_slot[0x20] = { 0, 2, 4, 1, 3, 5, -1, -1, 6, 8, 10, 7, 9, 11, -1, -1, 12, 14, 16, 13, 15, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

static const Bit8u op_offset[18] = { 0x00, 0x03, 0x01, 0x04, 0x02, 0x05, 0x08, 0x0b, 0x09, 0x0c, 0x0a, 0x0d, 0x10, 0x13, 0x11, 0x14, 0x12, 0x15 };



static const Bit8u eg_incdesc[16] = {
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2
};

static const Bit8s eg_incsh[16] = {
	0, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, -1, -2
};

typedef Bit16s(*envelope_sinfunc)(Bit16u phase, Bit16u envelope);
typedef void(*envelope_genfunc)(slot *slott);

//
// Phase generator 
//

void PG_Generate(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	channel *chan = &opl->Channels[op / 2];
	Bit16u fnum = chan->f_number;
	if (slt->vibrato) {
		Bit8u fnum_high = chan->f_number >> (7 + vib_table[opl->vib_pos] + (!opl->dvb));
		fnum += fnum_high * vibsgn_table[opl->vib_pos];
	}
	slt->PG_pos += (((fnum << chan->block) >> 1) * mt[slt->mult]) >> 1;
}

//
// Envelope generator
//

Bit16s envelope_calcexp(Bit32u level) {
	return ((exprom[(level & 0xff) ^ 0xff] | 0x400) << 1) >> (level >> 8);
}

Bit16s envelope_calcsin0(Bit16u phase, Bit16u envelope) {
	phase &= 0x3ff;
	Bit16u out = 0;
	Bit16u neg = 0;
	if (phase & 0x200 && (phase & 0x1ff)) {
		phase--;
		neg = ~0;
	}
	if (phase & 0x100) {
		out = logsinrom[(phase & 0xff) ^ 0xff];
	}
	else {
		out = logsinrom[phase & 0xff];
	}
	return envelope_calcexp(out + (envelope << 3)) ^ neg;
}

Bit16s envelope_calcsin1(Bit16u phase, Bit16u envelope) {
	phase &= 0x3ff;
	Bit16u out = 0;
	if (phase & 0x200) {
		out = 0x1000;
	}
	else if (phase & 0x100) {
		out = logsinrom[(phase & 0xff) ^ 0xff];
	}
	else {
		out = logsinrom[phase & 0xff];
	}
	return envelope_calcexp(out + (envelope << 3));
}

Bit16s envelope_calcsin2(Bit16u phase, Bit16u envelope) {
	phase &= 0x3ff;
	Bit16u out = 0;
	if (phase & 0x100) {
		out = logsinrom[(phase & 0xff) ^ 0xff];
	}
	else {
		out = logsinrom[phase & 0xff];
	}
	return envelope_calcexp(out + (envelope << 3));
}

Bit16s envelope_calcsin3(Bit16u phase, Bit16u envelope) {
	phase &= 0x3ff;
	Bit16u out = 0;
	if (phase & 0x100) {
		out = 0x1000;
	}
	else {
		out = logsinrom[phase & 0xff];
	}
	return envelope_calcexp(out + (envelope << 3));
}

Bit16s envelope_calcsin4(Bit16u phase, Bit16u envelope) {
	phase &= 0x3ff;
	Bit16u out = 0;
	Bit16u neg = 0;
	if ((phase & 0x300) == 0x100 && (phase & 0xff)) {
		phase--;
		neg = ~0;
	}
	if (phase & 0x200) {
		out = 0x1000;
	}
	else if (phase & 0x80) {
		out = logsinrom[((phase ^ 0xff) << 1) & 0xff];
	}
	else {
		out = logsinrom[(phase << 1) & 0xff];
	}
	return envelope_calcexp(out + (envelope << 3)) ^ neg;
}

Bit16s envelope_calcsin5(Bit16u phase, Bit16u envelope) {
	phase &= 0x3ff;
	Bit16u out = 0;
	if (phase & 0x200) {
		out = 0x1000;
	}
	else if (phase & 0x80) {
		out = logsinrom[((phase ^ 0xff) << 1) & 0xff];
	}
	else {
		out = logsinrom[(phase << 1) & 0xff];
	}
	return envelope_calcexp(out + (envelope << 3));
}

Bit16s envelope_calcsin6(Bit16u phase, Bit16u envelope) {
	phase &= 0x3ff;
	Bit16u neg = 0;
	if (phase & 0x200 && (phase & 0x1ff)) {
		phase--;
		neg = ~0;
	}
	return envelope_calcexp(envelope << 3) ^ neg;
}

Bit16s envelope_calcsin7(Bit16u phase, Bit16u envelope) {
	phase &= 0x3ff;
	Bit16u out = 0;
	Bit16u neg = 0;
	if (phase & 0x200 && (phase & 0x1ff)) {
		phase--;
		neg = ~0;
		phase = (phase & 0x1ff) ^ 0x1ff;
	}
	out = phase << 3;
	return envelope_calcexp(out + (envelope << 3)) ^ neg;
}

envelope_sinfunc envelope_sin[8] = {
	envelope_calcsin0,
	envelope_calcsin1,
	envelope_calcsin2,
	envelope_calcsin3,
	envelope_calcsin4,
	envelope_calcsin5,
	envelope_calcsin6,
	envelope_calcsin7
};

void envelope_gen_off(slot *slott);
void envelope_gen_change(slot *slott);
void envelope_gen_attack(slot *slott);
void envelope_gen_decay(slot *slott);
void envelope_gen_sustain(slot *slott);
void envelope_gen_release(slot *slott);

envelope_genfunc envelope_gen[6] = {
	envelope_gen_off,
	envelope_gen_attack,
	envelope_gen_decay,
	envelope_gen_sustain,
	envelope_gen_release,
	envelope_gen_change
};

enum envelope_gen_num {
	envelope_gen_num_off = 0,
	envelope_gen_num_attack = 1,
	envelope_gen_num_decay = 2,
	envelope_gen_num_sustain = 3,
	envelope_gen_num_release = 4,
	envelope_gen_num_change = 5
};

void envelope_gen_off(slot *slott) {
	slott->EG_out = 0x1ff;
}

void envelope_gen_change(slot *slott) {
	slott->eg_gen = slott->eg_gennext;
}

void envelope_gen_attack(slot *slott) {
	slott->EG_out += ((~slott->EG_out) *slott->eg_inc) >> 3;
	if (slott->EG_out < 0x00) {
		slott->EG_out = 0x00;
	}
	if (slott->EG_out == 0x00) {
		slott->eg_gen = envelope_gen_num_change;
		slott->eg_gennext = envelope_gen_num_decay;
	}
}

void envelope_gen_decay(slot *slott) {
	slott->EG_out += slott->eg_inc;
	if (slott->EG_out >= slott->EG_sl << 4) {
		slott->eg_gen = envelope_gen_num_change;
		slott->eg_gennext = envelope_gen_num_sustain;
	}
}

void envelope_gen_sustain(slot *slott) {
	if (!slott->EG_type) {
		envelope_gen_release(slott);
	}
}

void envelope_gen_release(slot *slott) {
	slott->EG_out += slott->eg_inc;
	if (slott->EG_out >= 0x1ff) {
		slott->eg_gen = envelope_gen_num_change;
		slott->eg_gennext = envelope_gen_num_off;
	}
}

Bit8u EG_CalcRate(chip *opl, Bit8u op, Bit8u rate) {
	slot *slt = &opl->OPs[op];
	channel *chan = &opl->Channels[op / 2];
	if (rate == 0x00) {
		return 0x00;
	}
	Bit8u rof = slt->ksr ? chan->ksv : (chan->ksv >> 2);
	Bit8u rat = (rate << 2) + rof;
	if (rat > 0x3c) {
		rat = 0x3c;
	}
	return rat;
}

void envelope_calc(chip *opl, Bit8u op) {
	slot *slott = &opl->OPs[op];
	Bit16u timer = opl->timer;
	Bit8u rate_h, rate_l;
	Bit8u rate;
	Bit8u reg_rate = 0;;
	switch (slott->eg_gen) {
	case envelope_gen_num_attack:
		reg_rate = slott->EG_ar;
		break;
	case envelope_gen_num_decay:
		reg_rate = slott->EG_dr;
		break;
	case envelope_gen_num_sustain:
	case envelope_gen_num_release:
		reg_rate = slott->EG_rr;
		break;
	}
	rate = EG_CalcRate(opl, op, reg_rate);
	rate_h = rate >> 2;
	rate_l = rate & 3;
	Bit8u inc = 0;
	if (slott->eg_gen == envelope_gen_num_attack && rate_h == 0x0f) {
		inc = 8;
	}
	else if (eg_incsh[rate_h] > 0) {
		if ((timer & ((1 << eg_incsh[rate_h]) - 1)) == 0) {
			inc = eg_incstep[eg_incdesc[rate_h]][rate_l][((timer) >> eg_incsh[rate_h]) & 0x07];
		}
	}
	else {
		inc = eg_incstep[eg_incdesc[rate_h]][rate_l][timer & 0x07] << (-eg_incsh[rate_h]);
	}
	slott->eg_inc = inc;
	envelope_gen[slott->eg_gen](slott);
}

void EG_UpdateKSL(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	channel *chan = &opl->Channels[op / 2];
	Bit8u fnum_high = (chan->f_number >> 6) & 0x0f;
	Bit16s ksl = (kslrom[fnum_high] << 1) - ((chan->block ^ 0x07) << 5) - 0x20;
	if (ksl < 0x00) {
		ksl = 0x00;
	}
	slt->EG_ksl = ksl >> kslshift[slt->ksl];
}

void EG_Generate(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	envelope_calc(opl, op);
	slt->EG_mout = slt->EG_out + slt->EG_ksl + (slt->EG_tl << 2);
	if (slt->tremolo) {
		slt->EG_mout += opl->trem_val;
	}
	if (slt->EG_mout > 0x1ff) {
		slt->EG_mout = 0x1ff;
	}
}

void EG_KeyOn(chip *opl, Bit8u op, Bit8u type) {
	slot *slt = &opl->OPs[op];
	if (!slt->key) {
		slt->EG_state = eg_attack;
		slt->eg_gen = envelope_gen_num_change;
		slt->eg_gennext = envelope_gen_num_attack;
		slt->PG_pos = 0;
	}
	slt->key |= type;
}

void EG_KeyOff(chip *opl, Bit8u op, Bit8u type) {
	slot *slt = &opl->OPs[op];
	if (slt->key) {
		slt->key &= (~type);
		if (slt->key == 0x00) {
			slt->EG_state = eg_release;
			slt->eg_gen = envelope_gen_num_change;
			slt->eg_gennext = envelope_gen_num_release;
		}
	}
}

//
// Noise Generator
//

void N_Generate(chip *opl) {
	if (opl->noise & 1) {
		opl->noise ^= 0x800302;
	}
	opl->noise >>= 1;
}

//
// Operator(Slot)
//

void OP_Update20(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	slt->tremolo = (opl->opl_memory[0x20 + slt->offset] >> 7);
	slt->vibrato = (opl->opl_memory[0x20 + slt->offset] >> 6) & 0x01;
	slt->EG_type = (opl->opl_memory[0x20 + slt->offset] >> 5) & 0x01;
	slt->ksr = (opl->opl_memory[0x20 + slt->offset] >> 4) & 0x01;
	slt->mult = (opl->opl_memory[0x20 + slt->offset]) & 0x0f;
}

void OP_Update40(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	slt->EG_tl = (opl->opl_memory[0x40 + slt->offset]) & 0x3f;
	slt->ksl = (opl->opl_memory[0x40 + slt->offset] >> 6) & 0x03;
	EG_UpdateKSL(opl, op);
}

void OP_Update60(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	slt->EG_dr = (opl->opl_memory[0x60 + slt->offset]) & 0x0f;
	slt->EG_ar = (opl->opl_memory[0x60 + slt->offset] >> 4) & 0x0f;
}

void OP_Update80(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	slt->EG_rr = (opl->opl_memory[0x80 + slt->offset]) & 0x0f;
	slt->EG_sl = (opl->opl_memory[0x80 + slt->offset] >> 4) & 0x0f;
	if (slt->EG_sl == 0x0f) {
		slt->EG_sl = 0x1f;
	}
}

void OP_UpdateE0(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	slt->waveform = opl->opl_memory[0xe0 + slt->offset] & 0x07;
	if (!opl->newm) {
		slt->waveform &= 0x03;
	}
}

void OP_GeneratePhase(chip *opl, Bit8u op, Bit16u phase) {
	slot *slt = &opl->OPs[op];
	slt->out = envelope_sin[slt->waveform](phase, slt->EG_out);
}

void OP_Generate(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	slt->out = envelope_sin[slt->waveform]((Bit16u)((slt->PG_pos >> 9) + (*slt->mod)), slt->EG_mout);
}

void OP_GenerateZM(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	slt->out = envelope_sin[slt->waveform]((Bit16u)(slt->PG_pos >> 9), slt->EG_mout);
}

void OP_CalcFB(chip *opl, Bit8u op) {
	slot *slt = &opl->OPs[op];
	channel *chan = &opl->Channels[op / 2];
	slt->prevout[1] = slt->prevout[0];
	slt->prevout[0] = slt->out;
	if (chan->feedback) {
		slt->fbmod = (slt->prevout[0] + slt->prevout[1]) >> chan->feedback;
	} else {
		slt->fbmod = 0;
	}
}

//
// Channel
//

void CH_UpdateRhythm(chip *opl) {
	opl->rhythm = (opl->opl_memory[0xbd] & 0x3f);
	if (opl->rhythm & 0x20) {
		for (Bit8u i = 6; i < 9; i++) {
			opl->Channels[i].chtype = ch_drum;
		}
		//HH
		if (opl->rhythm & 0x01) {
			EG_KeyOn(opl, 14, egk_drum);
		} else {
			EG_KeyOff(opl, 14, egk_drum);
		}
		//TC
		if (opl->rhythm & 0x02) {
			EG_KeyOn(opl, 17, egk_drum);
		} else {
			EG_KeyOff(opl, 17, egk_drum);
		}
		//TOM
		if (opl->rhythm & 0x04) {
			EG_KeyOn(opl, 16, egk_drum);
		} else {
			EG_KeyOff(opl, 16, egk_drum);
		}
		//SD
		if (opl->rhythm & 0x08) {
			EG_KeyOn(opl, 15, egk_drum);
		} else {
			EG_KeyOff(opl, 15, egk_drum);
		}
		//BD
		if (opl->rhythm & 0x10) {
			EG_KeyOn(opl, 12, egk_drum);
			EG_KeyOn(opl, 13, egk_drum);
		} else {
			EG_KeyOff(opl, 12, egk_drum);
			EG_KeyOff(opl, 13, egk_drum);
		}
	} else {
		for (Bit8u i = 6; i < 9; i++) {
			opl->Channels[i].chtype = ch_2op;
		}
	}
}

void CH_UpdateAB0(chip *opl, Bit8u ch) {
	channel *chan = &opl->Channels[ch];
	if (opl->newm && chan->chtype == ch_4op2) {
		return;
	}
	Bit16u f_number = (opl->opl_memory[0xa0 + chan->offset]) | (((opl->opl_memory[0xb0 + chan->offset]) & 0x03) << 8);
	Bit8u block = ((opl->opl_memory[0xb0 + chan->offset]) >> 2) & 0x07;
	Bit8u ksv = block * 2 | ((f_number >> (9 - opl->nts)) & 0x01);
	chan->f_number = f_number;
	chan->block = block;
	chan->ksv = ksv;
	EG_UpdateKSL(opl, ch * 2);
	EG_UpdateKSL(opl, ch * 2 + 1);
	OP_Update60(opl, ch * 2);
	OP_Update60(opl, ch * 2 + 1);
	OP_Update80(opl, ch * 2);
	OP_Update80(opl, ch * 2 + 1);
	if (opl->newm && chan->chtype == ch_4op) {
		chan = &opl->Channels[ch + 3];
		chan->f_number = f_number;
		chan->block = block;
		chan->ksv = ksv;
		EG_UpdateKSL(opl, (ch + 3) * 2);
		EG_UpdateKSL(opl, (ch + 3) * 2 + 1);
		OP_Update60(opl, (ch + 3) * 2);
		OP_Update60(opl, (ch + 3) * 2 + 1);
		OP_Update80(opl, (ch + 3) * 2);
		OP_Update80(opl, (ch + 3) * 2 + 1);
	}
}

void CH_SetupAlg(chip *opl, Bit8u ch) {
	channel *chan = &opl->Channels[ch];
	if (chan->alg & 0x08) {
		return;
	}
	if (chan->alg & 0x04) {
		switch (chan->alg & 0x03) {
		case 0:
			opl->OPs[(ch - 3) * 2].mod = &opl->OPs[(ch - 3) * 2].fbmod;
			opl->OPs[(ch - 3) * 2 + 1].mod = &opl->OPs[(ch - 3) * 2].out;
			opl->OPs[ch * 2].mod = &opl->OPs[(ch - 3) * 2 + 1].out;
			opl->OPs[ch * 2 + 1].mod = &opl->OPs[ch * 2].out;
			break;
		case 1:
			opl->OPs[(ch - 3) * 2].mod = &opl->OPs[(ch - 3) * 2].fbmod;
			opl->OPs[(ch - 3) * 2 + 1].mod = &opl->OPs[(ch - 3) * 2].out;
			opl->OPs[ch * 2].mod = &opl->zm;
			opl->OPs[ch * 2 + 1].mod = &opl->OPs[ch * 2].out;
			break;
		case 2:
			opl->OPs[(ch - 3) * 2].mod = &opl->OPs[(ch - 3) * 2].fbmod;
			opl->OPs[(ch - 3) * 2 + 1].mod = &opl->zm;
			opl->OPs[ch * 2].mod = &opl->OPs[(ch - 3) * 2 + 1].out;
			opl->OPs[ch * 2 + 1].mod = &opl->OPs[ch * 2].out;
			break;
		case 3:
			opl->OPs[(ch - 3) * 2].mod = &opl->OPs[(ch - 3) * 2].fbmod;
			opl->OPs[(ch - 3) * 2 + 1].mod = &opl->zm;
			opl->OPs[ch * 2].mod = &opl->OPs[(ch - 3) * 2 + 1].out;
			opl->OPs[ch * 2 + 1].mod = &opl->zm;
			break;
		}
	} else {
		switch (chan->alg & 0x01) {
		case 0:
			opl->OPs[ch * 2].mod = &opl->OPs[ch * 2].fbmod;
			opl->OPs[ch * 2 + 1].mod = &opl->OPs[ch * 2].out;
			break;
		case 1:
			opl->OPs[ch * 2].mod = &opl->OPs[ch * 2].fbmod;
			opl->OPs[ch * 2 + 1].mod = &opl->zm;
			break;
		}
	}
}

void CH_UpdateC0(chip *opl, Bit8u ch) {
	channel *chan = &opl->Channels[ch];
	Bit8u fb = (opl->opl_memory[0xc0 + chan->offset] & 0x0e) >> 1;
	chan->feedback = fb ? (9 - fb) : 0;
	chan->con = opl->opl_memory[0xc0 + chan->offset] & 0x01;
	chan->alg = chan->con;
	if (opl->newm) {
		if (chan->chtype == ch_4op) {
			channel *chan1 = &opl->Channels[ch + 3];
			chan1->alg = 0x04 | (chan->con << 1) | (chan1->con);
			chan->alg = 0x08;
			CH_SetupAlg(opl, ch + 3);
		} else if (chan->chtype == ch_4op2) {
			channel *chan1 = &opl->Channels[ch - 3];
			chan->alg = 0x04 | (chan1->con << 1) | (chan->con);
			chan1->alg = 0x08;
			CH_SetupAlg(opl, ch);
		} else {
			CH_SetupAlg(opl, ch);
		}
	} else {
		CH_SetupAlg(opl, ch);
	}
	if (opl->newm) {
		chan->cha = ((opl->opl_memory[0xc0 + chan->offset] >> 4) & 0x01) ? ~0 : 0;
		chan->chb = ((opl->opl_memory[0xc0 + chan->offset] >> 5) & 0x01) ? ~0 : 0;
		chan->chc = ((opl->opl_memory[0xc0 + chan->offset] >> 6) & 0x01) ? ~0 : 0;
		chan->chd = ((opl->opl_memory[0xc0 + chan->offset] >> 7) & 0x01) ? ~0 : 0;
	} else {
		opl->Channels[ch].cha = opl->Channels[ch].chb = ~0;
		opl->Channels[ch].chc = opl->Channels[ch].chd = 0;
	}
}

void CH_Set2OP(chip *opl) {
	for (Bit8u i = 0; i < 18; i++) {
		opl->Channels[i].chtype = ch_2op;
		CH_UpdateC0(opl, i);
	}
}

void CH_Set4OP(chip *opl) {
	for (Bit8u i = 0; i < 3; i++) {
		if ((opl->opl_memory[0x104] >> i) & 0x01) {
			opl->Channels[i].chtype = ch_4op;
			opl->Channels[i + 3].chtype = ch_4op2;
			CH_UpdateC0(opl, i);
			CH_UpdateC0(opl, i + 3);
		}
		if ((opl->opl_memory[0x104] >> (i + 3)) & 0x01) {
			opl->Channels[i + 9].chtype = ch_4op;
			opl->Channels[i + 3 + 9].chtype = ch_4op2;
			CH_UpdateC0(opl, i + 9);
			CH_UpdateC0(opl, i + 3 + 9);
		}
	}
}

void CH_GenerateRhythm(chip *opl) {
	if (opl->rhythm & 0x20) {
		channel *chan6 = &opl->Channels[6];
		channel *chan7 = &opl->Channels[7];
		channel *chan8 = &opl->Channels[8];
		slot *slt12 = &opl->OPs[12];
		slot *slt13 = &opl->OPs[13];
		slot *slt14 = &opl->OPs[14];
		slot *slt15 = &opl->OPs[15];
		slot *slt16 = &opl->OPs[16];
		slot *slt17 = &opl->OPs[17];
		//BD
		OP_Generate(opl, 12);
		OP_Generate(opl, 13);
		chan6->out = slt13->out * 2;
		Bit16u P14 = (slt14->PG_pos >> 9) & 0x3ff;
		Bit16u P17 = (slt17->PG_pos >> 9) & 0x3ff;
		Bit16u phase = 0;
		// HH TC Phase bit
		Bit16u PB = ((P14 & 0x08) | (((P14 >> 5) ^ P14) & 0x04) | (((P17 >> 2) ^ P17) & 0x08)) ? 0x01 : 0x00;
		//HH
		phase = (PB << 9) | (0x34 << ((PB ^ (opl->noise & 0x01) << 1)));
		OP_GeneratePhase(opl, 14, phase);
		//SD
		phase = (0x100 << ((P14 >> 8) & 0x01)) ^ ((opl->noise & 0x01) << 8);
		OP_GeneratePhase(opl, 15, phase);
		//TT
		OP_GenerateZM(opl, 16);
		//TC
		phase = 0x100 | (PB << 9);
		OP_GeneratePhase(opl, 17, phase);
		chan7->out = (slt14->out + slt15->out) * 2;
		chan8->out = (slt16->out + slt17->out) * 2;
	}
}

void CH_Generate(chip *opl, Bit8u ch) {
	channel *chan = &opl->Channels[ch];
	if (chan->chtype == ch_drum) {
		return;
	}
	if (chan->alg & 0x08) {
		chan->out = 0;
		return;
	} else if (chan->alg & 0x04) {
		OP_Generate(opl, (ch - 3) * 2);
		OP_Generate(opl, (ch - 3) * 2 + 1);
		OP_Generate(opl, ch * 2);
		OP_Generate(opl, ch * 2 + 1);
		switch (chan->alg & 0x03) {
		case 0:
			chan->out = opl->OPs[ch * 2 + 1].out;
			break;
		case 1:
			chan->out = opl->OPs[(ch - 3) * 2 + 1].out + opl->OPs[ch * 2 + 1].out;
			break;
		case 2:
			chan->out = opl->OPs[(ch - 3) * 2].out + opl->OPs[ch * 2 + 1].out;
			break;
		case 3:
			chan->out = opl->OPs[(ch - 3) * 2].out + opl->OPs[ch * 2].out + opl->OPs[ch * 2 + 1].out;
			break;
		}
	}
	else {
		OP_Generate(opl, ch * 2);
		OP_Generate(opl, ch * 2 + 1);
		switch (chan->alg & 0x01) {
		case 0:
			chan->out = opl->OPs[ch * 2 + 1].out;
			break;
		case 1:
			chan->out = opl->OPs[ch * 2].out + opl->OPs[ch * 2 + 1].out;
			break;
		}
	}
}

void CH_Enable(chip *opl, Bit8u ch) {
	channel *chan = &opl->Channels[ch];
	if (opl->newm) {
		if (chan->chtype == ch_4op) {
			EG_KeyOn(opl, ch * 2, egk_norm);
			EG_KeyOn(opl, ch * 2 + 1, egk_norm);
			EG_KeyOn(opl, (ch + 3) * 2, egk_norm);
			EG_KeyOn(opl, (ch + 3) * 2 + 1, egk_norm);
		}
		else if (chan->chtype == ch_2op || chan->chtype == ch_drum) {
			EG_KeyOn(opl, ch * 2, egk_norm);
			EG_KeyOn(opl, ch * 2 + 1, egk_norm);
		}
	}
	else {
		EG_KeyOn(opl, ch * 2, egk_norm);
		EG_KeyOn(opl, ch * 2 + 1, egk_norm);
	}
}

void CH_Disable(chip *opl, Bit8u ch) {
	channel *chan = &opl->Channels[ch];
	if (opl->newm) {
		if (chan->chtype == ch_4op) {
			EG_KeyOff(opl, ch * 2, egk_norm);
			EG_KeyOff(opl, ch * 2 + 1, egk_norm);
			EG_KeyOff(opl, (ch + 3) * 2, egk_norm);
			EG_KeyOff(opl, (ch + 3) * 2 + 1, egk_norm);
		}
		else if (chan->chtype == ch_2op || chan->chtype == ch_drum) {
			EG_KeyOff(opl, ch * 2, egk_norm);
			EG_KeyOff(opl, ch * 2 + 1, egk_norm);
		}
	}
	else {
		EG_KeyOff(opl, ch * 2, egk_norm);
		EG_KeyOff(opl, ch * 2 + 1, egk_norm);
	}
}

Bit16s limshort(Bit32s a) {
	if (a > 32767) {
		a = 32767;
	}
	else if (a < -32768) {
		a = -32768;
	}
	return (Bit16s)a;
}

void NukedOPL3::Reset() {
	for (Bit8u i = 0; i < 36; i++) {
		opl3.OPs[i].PG_pos = 0;
		opl3.OPs[i].PG_inc = 0;
		opl3.OPs[i].EG_out = 0x1ff;
		opl3.OPs[i].EG_mout = 0x1ff;
		opl3.OPs[i].eg_inc = 0;
		opl3.OPs[i].eg_gen = 0;
		opl3.OPs[i].eg_gennext = 0;
		opl3.OPs[i].EG_ksl = 0;
		opl3.OPs[i].EG_ar = 0;
		opl3.OPs[i].EG_dr = 0;
		opl3.OPs[i].EG_sl = 0;
		opl3.OPs[i].EG_rr = 0;
		opl3.OPs[i].EG_state = eg_off;
		opl3.OPs[i].EG_type = 0;
		opl3.OPs[i].out = 0;
		opl3.OPs[i].prevout[0] = 0;
		opl3.OPs[i].prevout[1] = 0;
		opl3.OPs[i].fbmod = 0;
		opl3.OPs[i].offset = op_offset[i % 18] + ((i > 17) << 8);
		opl3.OPs[i].mult = 0;
		opl3.OPs[i].vibrato = 0;
		opl3.OPs[i].tremolo = 0;
		opl3.OPs[i].ksr = 0;
		opl3.OPs[i].EG_tl = 0;
		opl3.OPs[i].ksl = 0;
		opl3.OPs[i].key = 0;
		opl3.OPs[i].waveform = 0;
	}
	for (Bit8u i = 0; i < 9; i++) {
		opl3.Channels[i].con = 0;
		opl3.Channels[i + 9].con = 0;
		opl3.Channels[i].chtype = ch_2op;
		opl3.Channels[i + 9].chtype = ch_2op;
		opl3.Channels[i].alg = 0;
		opl3.Channels[i + 9].alg = 0;
		opl3.Channels[i].offset = i;
		opl3.Channels[i + 9].offset = 0x100 + i;
		opl3.Channels[i].feedback = 0;
		opl3.Channels[i + 9].feedback = 0;
		opl3.Channels[i].out = 0;
		opl3.Channels[i + 9].out = 0;
		opl3.Channels[i].cha = ~0;
		opl3.Channels[i + 9].cha = ~0;
		opl3.Channels[i].chb = ~0;
		opl3.Channels[i + 9].chb = ~0;
		opl3.Channels[i].chc = 0;
		opl3.Channels[i + 9].chc = 0;
		opl3.Channels[i].chd = 0;
		opl3.Channels[i + 9].chd = 0;
		opl3.Channels[i].out = 0;
		opl3.Channels[i + 9].out = 0;
		opl3.Channels[i].f_number = 0;
		opl3.Channels[i + 9].f_number = 0;
		opl3.Channels[i].block = 0;
		opl3.Channels[i + 9].block = 0;
		opl3.Channels[i].ksv = 0;
		opl3.Channels[i + 9].ksv = 0;
		opl3.Channels[i].panl = (float)CENTER_PANNING_POWER;
		opl3.Channels[i + 9].panl = (float)CENTER_PANNING_POWER;
		opl3.Channels[i].panr = (float)CENTER_PANNING_POWER;
		opl3.Channels[i + 9].panr = (float)CENTER_PANNING_POWER;
	}
	memset(opl3.opl_memory, 0, 0x200);
	opl3.newm = 0;
	opl3.nts = 0;
	opl3.rhythm = 0;
	opl3.dvb = 0;
	opl3.dam = 0;
	opl3.noise = 0x306600;
	opl3.vib_pos = 0;
	opl3.timer = 0;
	opl3.trem_inc = 0;
	opl3.trem_tval = 0;
	opl3.trem_dir = 0;
	opl3.trem_val = 0;
	opl3.zm = 0;
	CH_Set2OP(&opl3);
}

void NukedOPL3::WriteReg(int reg, int v) {
	v &= 0xff;
	reg &= 0x1ff;
	Bit8u highbank = (reg >> 8) & 0x01;
	Bit8u regm = reg & 0xff;
	opl3.opl_memory[reg & 0x1ff] = v;
	switch (regm & 0xf0) {
	case 0x00:
		if (highbank) {
			switch (regm & 0x0f) {
			case 0x04:
				CH_Set2OP(&opl3);
				CH_Set4OP(&opl3);
				break;
			case 0x05:
				opl3.newm = v & 0x01;
				break;
			}
		}
		else {
			switch (regm & 0x0f) {
			case 0x08:
				opl3.nts = (v >> 6) & 0x01;
				break;
			}
		}
		break;
	case 0x20:
	case 0x30:
		if (ad_slot[regm & 0x1f] >= 0) {
			OP_Update20(&opl3, 18 * highbank + ad_slot[regm & 0x1f]);
		}
		break;
	case 0x40:
	case 0x50:
		if (ad_slot[regm & 0x1f] >= 0) {
			OP_Update40(&opl3, 18 * highbank + ad_slot[regm & 0x1f]);
		}
		break;
	case 0x60:
	case 0x70:
		if (ad_slot[regm & 0x1f] >= 0) {
			OP_Update60(&opl3, 18 * highbank + ad_slot[regm & 0x1f]);
		}
		break;
	case 0x80:
	case 0x90:
		if (ad_slot[regm & 0x1f] >= 0) {
			OP_Update80(&opl3, 18 * highbank + ad_slot[regm & 0x1f]);
		}
		break;
	case 0xe0:
	case 0xf0:
		if (ad_slot[regm & 0x1f] >= 0) {
			OP_UpdateE0(&opl3, 18 * highbank + ad_slot[regm & 0x1f]);
		}
		break;
	case 0xa0:
		if ((regm & 0x0f) < 9) {
			CH_UpdateAB0(&opl3, 9 * highbank + (regm & 0x0f));
		}
		break;
	case 0xb0:
		if (regm == 0xbd && !highbank) {
			opl3.dam = v >> 7;
			opl3.dvb = (v >> 6) & 0x01;
			CH_UpdateRhythm(&opl3);
		}
		else if ((regm & 0x0f) < 9) {
			CH_UpdateAB0(&opl3, 9 * highbank + (regm & 0x0f));
			if (v & 0x20) {
				CH_Enable(&opl3, 9 * highbank + (regm & 0x0f));
			}
			else {
				CH_Disable(&opl3, 9 * highbank + (regm & 0x0f));
			}
		}
		break;
	case 0xc0:
		if ((regm & 0x0f) < 9) {
			CH_UpdateC0(&opl3, 9 * highbank + (regm & 0x0f));
		}
		break;
	}
}

void NukedOPL3::Update(float* sndptr, int numsamples) {
	Bit32s outa, outb;
	Bit8u ii = 0;
	for (Bit32u i = 0; i < (Bit32u)numsamples; i++) {
		outa = 0;
		outb = 0;
		for (ii = 0; ii < 36; ii++) {
			OP_CalcFB(&opl3, ii);
		}
		CH_GenerateRhythm(&opl3);
		for (ii = 0; ii < 18; ii++) {
			CH_Generate(&opl3, ii);
			if (FullPan) {
				outa += (Bit16s)(opl3.Channels[ii].out * opl3.Channels[ii].panl);
				outb += (Bit16s)(opl3.Channels[ii].out * opl3.Channels[ii].panr);
			}
			else {
				outa += (Bit16s)(opl3.Channels[ii].out & opl3.Channels[ii].cha);
				outb += (Bit16s)(opl3.Channels[ii].out & opl3.Channels[ii].chb);
			}
		}
		for (ii = 0; ii < 36; ii++) {
			EG_Generate(&opl3, ii);
			PG_Generate(&opl3, ii);
		}
		N_Generate(&opl3);
		opl3.trem_inc++;
		if (!(opl3.trem_inc & 0x3f)) {
			if (!opl3.trem_dir) {
				if (opl3.trem_tval == 105) {
					opl3.trem_tval--;
					opl3.trem_dir = 1;
				}
				else {
					opl3.trem_tval++;
				}
			}
			else {
				if (opl3.trem_tval == 0) {
					opl3.trem_tval++;
					opl3.trem_dir = 0;
				}
				else {
					opl3.trem_tval--;
				}
			}
			opl3.trem_val = (opl3.trem_tval >> 2) >> ((!opl3.dam) << 1);
		}
		opl3.timer++;
		opl3.vib_pos = (opl3.timer >> 10) & 0x07;
		*sndptr++ += (float)(outa / 10240.0);
		*sndptr++ += (float)(outb / 10240.0);
	}
}

void NukedOPL3::SetPanning(int c, float left, float right) {
	if (FullPan) {
		opl3.Channels[c].panl = left;
		opl3.Channels[c].panr = right;
	}
}

NukedOPL3::NukedOPL3(bool stereo) {
	FullPan = stereo;
	Reset();
}

OPLEmul *NukedOPL3Create(bool stereo) {
	return new NukedOPL3(stereo);
}
