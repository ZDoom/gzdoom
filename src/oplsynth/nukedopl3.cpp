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
		MAME Development Team(Jarek Burczynski, Tatsuyuki Satoh):
			Feedback and Rhythm part calculation information.
		forums.submarine.org.uk(carbon14, opl3):
			Tremolo and phase generator calculation information.
		OPLx decapsulated(Matthew Gambrell, Olli Niemitalo):
			OPL2 ROMs.
*/

//version 1.5

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
	v1.5:
		Optimizations
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

//
// Envelope generator
//

typedef Bit16s(*envelope_sinfunc)(Bit16u phase, Bit16u envelope);
typedef void(*envelope_genfunc)(opl_slot *slott);

Bit16s envelope_calcexp(Bit32u level) {
	if (level > 0x1fff) {
		level = 0x1fff;
	}
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

void envelope_gen_off(opl_slot *slott);
void envelope_gen_change(opl_slot *slott);
void envelope_gen_attack(opl_slot *slott);
void envelope_gen_decay(opl_slot *slott);
void envelope_gen_sustain(opl_slot *slott);
void envelope_gen_release(opl_slot *slott);

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

Bit8u envelope_calc_rate(opl_slot *slot, Bit8u reg_rate) {
	if (reg_rate == 0x00) {
		return 0x00;
	}
	Bit8u rate = (reg_rate << 2) + (slot->reg_ksr ? slot->channel->ksv : (slot->channel->ksv >> 2));
	if (rate > 0x3c) {
		rate = 0x3c;
	}
	return rate;
}

void envelope_update_ksl(opl_slot *slot) {
	Bit16s ksl = (kslrom[slot->channel->f_num >> 6] << 1) - ((slot->channel->block ^ 0x07) << 5) - 0x20;
	if (ksl < 0) {
		ksl = 0;
	}
	slot->eg_ksl = (Bit8u)ksl;
}

void envelope_update_rate(opl_slot *slot) {
	switch (slot->eg_gen) {
	case envelope_gen_num_off:
		slot->eg_rate = 0;
		break;
	case envelope_gen_num_attack:
		slot->eg_rate = envelope_calc_rate(slot, slot->reg_ar);
		break;
	case envelope_gen_num_decay:
		slot->eg_rate = envelope_calc_rate(slot, slot->reg_dr);
		break;
	case envelope_gen_num_sustain:
	case envelope_gen_num_release:
		slot->eg_rate = envelope_calc_rate(slot, slot->reg_rr);
		break;
	}
}

void envelope_gen_off(opl_slot *slot) {
	slot->eg_rout = 0x1ff;
}

void envelope_gen_change(opl_slot *slot) {
	slot->eg_gen = slot->eg_gennext;
	envelope_update_rate(slot);
}

void envelope_gen_attack(opl_slot *slot) {
	slot->eg_rout += ((~slot->eg_rout) *slot->eg_inc) >> 3;
	if (slot->eg_rout < 0x00) {
		slot->eg_rout = 0x00;
	}
	if (!slot->eg_rout) {
		slot->eg_gen = envelope_gen_num_change;
		slot->eg_gennext = envelope_gen_num_decay;
	}
}

void envelope_gen_decay(opl_slot *slot) {
	slot->eg_rout += slot->eg_inc;
	if (slot->eg_rout >= slot->reg_sl << 4) {
		slot->eg_gen = envelope_gen_num_change;
		slot->eg_gennext = envelope_gen_num_sustain;
	}
}

void envelope_gen_sustain(opl_slot *slot) {
	if (!slot->reg_type) {
		envelope_gen_release(slot);
	}
}

void envelope_gen_release(opl_slot *slot) {
	slot->eg_rout += slot->eg_inc;
	if (slot->eg_rout >= 0x1ff) {
		slot->eg_gen = envelope_gen_num_change;
		slot->eg_gennext = envelope_gen_num_off;
	}
}

void envelope_calc(opl_slot *slot) {
	Bit8u rate_h, rate_l;
	rate_h = slot->eg_rate >> 2;
	rate_l = slot->eg_rate & 3;
	Bit8u inc = 0;
	if (slot->eg_gen == envelope_gen_num_attack && rate_h == 0x0f) {
		inc = 8;
	}
	else if (eg_incsh[rate_h] > 0) {
		if ((slot->chip->timer & ((1 << eg_incsh[rate_h]) - 1)) == 0) {
			inc = eg_incstep[eg_incdesc[rate_h]][rate_l][((slot->chip->timer) >> eg_incsh[rate_h]) & 0x07];
		}
	}
	else {
		inc = eg_incstep[eg_incdesc[rate_h]][rate_l][slot->chip->timer & 0x07] << (-eg_incsh[rate_h]);
	}
	slot->eg_inc = inc;
	envelope_gen[slot->eg_gen](slot);
	slot->eg_out = slot->eg_rout + (slot->reg_tl << 2) + (slot->eg_ksl >> kslshift[slot->reg_ksl]) + *slot->trem;
}

void eg_keyon(opl_slot *slot, Bit8u type) {
	if (!slot->key) {
		slot->eg_gen = envelope_gen_num_change;
		slot->eg_gennext = envelope_gen_num_attack;
		slot->pg_phase = 0x00;
	}
	slot->key |= type;
}

void eg_keyoff(opl_slot *slot, Bit8u type) {
	if (slot->key) {
		slot->key &= (~type);
		if (!slot->key) {
			slot->eg_gen = envelope_gen_num_change;
			slot->eg_gennext = envelope_gen_num_release;
		}
	}
}

//
// Phase Generator
//

void pg_generate(opl_slot *slot) {
	Bit16u f_num = slot->channel->f_num;
	if (slot->reg_vib) {
		Bit8u f_num_high = f_num >> (7 + vib_table[(slot->chip->timer >> 10)&0x07] + (0x01 - slot->chip->dvb));
		f_num += f_num_high * vibsgn_table[(slot->chip->timer >> 10) & 0x07];
	}
	slot->pg_phase += (((f_num << slot->channel->block) >> 1) * mt[slot->reg_mult]) >> 1;
}

//
// Noise Generator
//

void n_generate(opl_chip *chip) {
	if (chip->noise & 0x01) {
		chip->noise ^= 0x800302;
	}
	chip->noise >>= 1;
}

//
// Slot
//

void slot_write20(opl_slot *slot,Bit8u data) {
	if ((data >> 7) & 0x01) {
		slot->trem = &slot->chip->tremval;
	}
	else {
		slot->trem = (Bit8u*)&slot->chip->zeromod;
	}
	slot->reg_vib = (data >> 6) & 0x01;
	slot->reg_type = (data >> 5) & 0x01;
	slot->reg_ksr = (data >> 4) & 0x01;
	slot->reg_mult = data & 0x0f;
	envelope_update_rate(slot);
}

void slot_write40(opl_slot *slot, Bit8u data) {
	slot->reg_ksl = (data >> 6) & 0x03;
	slot->reg_tl = data & 0x3f;
	envelope_update_ksl(slot);
}

void slot_write60(opl_slot *slot, Bit8u data) {
	slot->reg_ar = (data >> 4) & 0x0f;
	slot->reg_dr = data & 0x0f;
	envelope_update_rate(slot);
}

void slot_write80(opl_slot *slot, Bit8u data) {
	slot->reg_sl = (data >> 4) & 0x0f;
	if (slot->reg_sl == 0x0f) {
		slot->reg_sl = 0x1f;
	}
	slot->reg_rr = data & 0x0f;
	envelope_update_rate(slot);
}

void slot_writee0(opl_slot *slot, Bit8u data) {
	slot->reg_wf = data & 0x07;
	if (slot->chip->newm == 0x00) {
		slot->reg_wf &= 0x03;
	}
}

void slot_generatephase(opl_slot *slot, Bit16u phase) {
	slot->out = envelope_sin[slot->reg_wf](phase, slot->eg_out);
}

void slot_generate(opl_slot *slot) {
	slot->out = envelope_sin[slot->reg_wf]((slot->pg_phase >> 9) + (*slot->mod), slot->eg_out);
}

void slot_generatezm(opl_slot *slot) {
	slot->out = envelope_sin[slot->reg_wf]((slot->pg_phase >> 9), slot->eg_out);
}

void slot_calgfb(opl_slot *slot) {
	slot->prout[1] = slot->prout[0];
	slot->prout[0] = slot->out;
	if (slot->channel->fb != 0x00) {
		slot->fbmod = (slot->prout[0] + slot->prout[1]) >> (0x09 - slot->channel->fb);
	}
	else {
		slot->fbmod = 0;
	}
}

//
// Channel
//

void chan_setupalg(opl_channel *channel);

void chan_updaterhythm(opl_chip *chip, Bit8u data) {
	chip->rhy = data & 0x3f;
	if (chip->rhy & 0x20) {
		chip->channel[6].out[0] = &chip->slot[13].out;
		chip->channel[6].out[1] = &chip->slot[13].out;
		chip->channel[6].out[2] = &chip->zeromod;
		chip->channel[6].out[3] = &chip->zeromod;
		chip->channel[7].out[0] = &chip->slot[14].out;
		chip->channel[7].out[1] = &chip->slot[14].out;
		chip->channel[7].out[2] = &chip->slot[15].out;
		chip->channel[7].out[3] = &chip->slot[15].out;
		chip->channel[8].out[0] = &chip->slot[16].out;
		chip->channel[8].out[1] = &chip->slot[16].out;
		chip->channel[8].out[2] = &chip->slot[17].out;
		chip->channel[8].out[3] = &chip->slot[17].out;
		for (Bit8u chnum = 6; chnum < 9; chnum++) {
			chip->channel[chnum].chtype = ch_drum;
		}
		//hh
		if (chip->rhy & 0x01) {
			eg_keyon(&chip->slot[14], egk_drum);
		}
		else {
			eg_keyoff(&chip->slot[14], egk_drum);
		}
		//tc
		if (chip->rhy & 0x02) {
			eg_keyon(&chip->slot[17], egk_drum);
		}
		else {
			eg_keyoff(&chip->slot[17], egk_drum);
		}
		//tom
		if (chip->rhy & 0x04) {
			eg_keyon(&chip->slot[16], egk_drum);
		}
		else {
			eg_keyoff(&chip->slot[16], egk_drum);
		}
		//sd
		if (chip->rhy & 0x08) {
			eg_keyon(&chip->slot[15], egk_drum);
		}
		else {
			eg_keyoff(&chip->slot[15], egk_drum);
		}
		//bd
		if (chip->rhy & 0x10) {
			eg_keyon(&chip->slot[12], egk_drum);
			eg_keyon(&chip->slot[13], egk_drum);
		}
		else {
			eg_keyoff(&chip->slot[12], egk_drum);
			eg_keyoff(&chip->slot[13], egk_drum);
		}
	}
	else {
		for (Bit8u chnum = 6; chnum < 9; chnum++) {
			chip->channel[chnum].chtype = ch_2op;
			chan_setupalg(&chip->channel[chnum]);
		}
	}
}

void chan_writea0(opl_channel *channel, Bit8u data) {
	if (channel->chip->newm && channel->chtype == ch_4op2) {
		return;
	}
	channel->f_num = (channel->f_num & 0x300) | data;
	channel->ksv = (channel->block << 1) | ((channel->f_num >> (0x09 - channel->chip->nts)) & 0x01);
	envelope_update_ksl(channel->slots[0]);
	envelope_update_ksl(channel->slots[1]);
	envelope_update_rate(channel->slots[0]);
	envelope_update_rate(channel->slots[1]);
	if (channel->chip->newm && channel->chtype == ch_4op) {
		channel->pair->f_num = channel->f_num;
		channel->pair->ksv = channel->ksv;
		envelope_update_ksl(channel->pair->slots[0]);
		envelope_update_ksl(channel->pair->slots[1]);
		envelope_update_rate(channel->pair->slots[0]);
		envelope_update_rate(channel->pair->slots[1]);
	}
}

void chan_writeb0(opl_channel *channel, Bit8u data) {
	if (channel->chip->newm && channel->chtype == ch_4op2) {
		return;
	}
	channel->f_num = (channel->f_num & 0xff) | ((data & 0x03) << 8);
	channel->block = (data >> 2) & 0x07;
	channel->ksv = (channel->block << 1) | ((channel->f_num >> (0x09 - channel->chip->nts)) & 0x01);
	envelope_update_ksl(channel->slots[0]);
	envelope_update_ksl(channel->slots[1]);
	envelope_update_rate(channel->slots[0]);
	envelope_update_rate(channel->slots[1]);
	if (channel->chip->newm && channel->chtype == ch_4op) {
		channel->pair->f_num = channel->f_num;
		channel->pair->block = channel->block;
		channel->pair->ksv = channel->ksv;
		envelope_update_ksl(channel->pair->slots[0]);
		envelope_update_ksl(channel->pair->slots[1]);
		envelope_update_rate(channel->pair->slots[0]);
		envelope_update_rate(channel->pair->slots[1]);
	}
}

void chan_setupalg(opl_channel *channel) {
	if (channel->chtype == ch_drum) {
		return;
	}
	if (channel->alg & 0x08) {
		return;
	}
	if (channel->alg & 0x04) {
		channel->pair->out[0] = &channel->chip->zeromod;
		channel->pair->out[1] = &channel->chip->zeromod;
		channel->pair->out[2] = &channel->chip->zeromod;
		channel->pair->out[3] = &channel->chip->zeromod;
		switch (channel->alg & 0x03) {
		case 0x00:
			channel->pair->slots[0]->mod = &channel->pair->slots[0]->fbmod;
			channel->pair->slots[1]->mod = &channel->pair->slots[0]->out;
			channel->slots[0]->mod = &channel->pair->slots[1]->out;
			channel->slots[1]->mod = &channel->slots[0]->out;
			channel->out[0] = &channel->slots[1]->out;
			channel->out[1] = &channel->chip->zeromod;
			channel->out[2] = &channel->chip->zeromod;
			channel->out[3] = &channel->chip->zeromod;
			break;
		case 0x01:
			channel->pair->slots[0]->mod = &channel->pair->slots[0]->fbmod;
			channel->pair->slots[1]->mod = &channel->pair->slots[0]->out;
			channel->slots[0]->mod = &channel->chip->zeromod;
			channel->slots[1]->mod = &channel->slots[0]->out;
			channel->out[0] = &channel->pair->slots[1]->out;
			channel->out[1] = &channel->slots[1]->out;
			channel->out[2] = &channel->chip->zeromod;
			channel->out[3] = &channel->chip->zeromod;
			break;
		case 0x02:
			channel->pair->slots[0]->mod = &channel->pair->slots[0]->fbmod;
			channel->pair->slots[1]->mod = &channel->chip->zeromod;
			channel->slots[0]->mod = &channel->pair->slots[1]->out;
			channel->slots[1]->mod = &channel->slots[0]->out;
			channel->out[0] = &channel->pair->slots[0]->out;
			channel->out[1] = &channel->slots[1]->out;
			channel->out[2] = &channel->chip->zeromod;
			channel->out[3] = &channel->chip->zeromod;
			break;
		case 0x03:
			channel->pair->slots[0]->mod = &channel->pair->slots[0]->fbmod;
			channel->pair->slots[1]->mod = &channel->chip->zeromod;
			channel->slots[0]->mod = &channel->pair->slots[1]->out;
			channel->slots[1]->mod = &channel->chip->zeromod;
			channel->out[0] = &channel->pair->slots[0]->out;
			channel->out[1] = &channel->slots[0]->out;
			channel->out[2] = &channel->slots[1]->out;
			channel->out[3] = &channel->chip->zeromod;
			break;
		}
	}
	else {
		switch (channel->alg & 0x01) {
		case 0x00:
			channel->slots[0]->mod = &channel->slots[0]->fbmod;
			channel->slots[1]->mod = &channel->slots[0]->out;
			channel->out[0] = &channel->slots[1]->out;
			channel->out[1] = &channel->chip->zeromod;
			channel->out[2] = &channel->chip->zeromod;
			channel->out[3] = &channel->chip->zeromod;
			break;
		case 0x01:
			channel->slots[0]->mod = &channel->slots[0]->fbmod;
			channel->slots[1]->mod = &channel->chip->zeromod;
			channel->out[0] = &channel->slots[0]->out;
			channel->out[1] = &channel->slots[1]->out;
			channel->out[2] = &channel->chip->zeromod;
			channel->out[3] = &channel->chip->zeromod;
			break;
		}
	}
}

void chan_writec0(opl_channel *channel, Bit8u data) {
	channel->fb = (data & 0x0e) >> 1;
	channel->con = data & 0x01;
	channel->alg = channel->con;
	if (channel->chip->newm) {
		if (channel->chtype == ch_4op) {
			channel->pair->alg = 0x04 | (channel->con << 1) | (channel->pair->con);
			channel->alg = 0x08;
			chan_setupalg(channel->pair);
		}
		else if (channel->chtype == ch_4op2) {
			channel->alg = 0x04 | (channel->pair->con << 1) | (channel->con);
			channel->pair->alg = 0x08;
			chan_setupalg(channel);
		}
		else {
			chan_setupalg(channel);
		}
	}
	else {
		chan_setupalg(channel);
	}
	if (channel->chip->newm) {
		channel->cha = ((data >> 4) & 0x01) ? ~0 : 0;
		channel->chb = ((data >> 5) & 0x01) ? ~0 : 0;
	}
	else {
		channel->cha = channel->chb = ~0;
	}
}

void chan_generaterhythm(opl_chip *chip) {
	if (chip->rhy & 0x20) {
		opl_channel *channel6 = &chip->channel[6];
		opl_channel *channel7 = &chip->channel[7];
		opl_channel *channel8 = &chip->channel[8];
		slot_generate(channel6->slots[0]);
		slot_generate(channel6->slots[1]);
		Bit16u phase14 = channel7->slots[0]->pg_phase & 0x3ff;
		Bit16u phase17 = channel8->slots[1]->pg_phase & 0x3ff;
		Bit16u phase = 0x00;
		//hh tc phase bit
		Bit16u phasebit = ((phase14 & 0x08) | (((phase14 >> 5) ^ phase14) & 0x04) | (((phase17 >> 2) ^ phase17) & 0x08)) ? 0x01 : 0x00;
		//hh
		phase = (phasebit << 9) | (0x34 << ((phasebit ^ (chip->noise & 0x01) << 1)));
		slot_generatephase(channel7->slots[0], phase);
		//sd
		phase = (0x100 << ((phase14 >> 8) & 0x01)) ^ ((chip->noise & 0x01) << 8);
		slot_generatephase(channel7->slots[1], phase);
		//tt
		slot_generatezm(channel8->slots[0]);
		//tc
		phase = 0x100 | (phasebit << 9);
		slot_generatephase(channel8->slots[1], phase);
	}
}

void chan_generate(opl_channel *channel) {
	if (channel->chtype == ch_drum) {
		return;
	}
	if (channel->alg & 0x08) {
		return;
	}
	if (channel->alg & 0x04) {
		slot_generate(channel->pair->slots[0]);
		slot_generate(channel->pair->slots[1]);
		slot_generate(channel->slots[0]);
		slot_generate(channel->slots[1]);
	}
	else {
		slot_generate(channel->slots[0]);
		slot_generate(channel->slots[1]);
	}
}

void chan_enable(opl_channel *channel) {
	if (channel->chip->newm) {
		if (channel->chtype == ch_4op) {
			eg_keyon(channel->slots[0], egk_norm);
			eg_keyon(channel->slots[1], egk_norm);
			eg_keyon(channel->pair->slots[0], egk_norm);
			eg_keyon(channel->pair->slots[1], egk_norm);
		}
		else if (channel->chtype == ch_2op || channel->chtype == ch_drum) {
			eg_keyon(channel->slots[0], egk_norm);
			eg_keyon(channel->slots[1], egk_norm);
		}
	}
	else {
		eg_keyon(channel->slots[0], egk_norm);
		eg_keyon(channel->slots[1], egk_norm);
	}
}

void chan_disable(opl_channel *channel) {
	if (channel->chip->newm) {
		if (channel->chtype == ch_4op) {
			eg_keyoff(channel->slots[0], egk_norm);
			eg_keyoff(channel->slots[1], egk_norm);
			eg_keyoff(channel->pair->slots[0], egk_norm);
			eg_keyoff(channel->pair->slots[1], egk_norm);
		}
		else if (channel->chtype == ch_2op || channel->chtype == ch_drum) {
			eg_keyoff(channel->slots[0], egk_norm);
			eg_keyoff(channel->slots[1], egk_norm);
		}
	}
	else {
		eg_keyoff(channel->slots[0], egk_norm);
		eg_keyoff(channel->slots[1], egk_norm);
	}
}

void chan_set4op(opl_chip *chip, Bit8u data) {
	for (Bit8u bit = 0; bit < 6; bit++) {
		Bit8u chnum = bit;
		if (bit >= 3) {
			chnum += 9 - 3;
		}
		if ((data >> bit) & 0x01) {
			chip->channel[chnum].chtype = ch_4op;
			chip->channel[chnum + 3].chtype = ch_4op2;
		}
		else {
			chip->channel[chnum].chtype = ch_2op;
			chip->channel[chnum + 3].chtype = ch_2op;
		}
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
	memset(&opl3, 0, sizeof(opl_chip));
	for (Bit8u slotnum = 0; slotnum < 36; slotnum++) {
		opl3.slot[slotnum].channel = &opl3.channel[slotnum / 2];
		opl3.slot[slotnum].chip = &opl3;
		opl3.slot[slotnum].mod = &opl3.zeromod;
		opl3.slot[slotnum].eg_rout = 0x1ff;
		opl3.slot[slotnum].eg_out = 0x1ff;
		opl3.slot[slotnum].eg_gen = envelope_gen_num_off;
		opl3.slot[slotnum].eg_gennext = envelope_gen_num_off;
		opl3.slot[slotnum].trem = (Bit8u*)&opl3.zeromod;
	}
	for (Bit8u channum = 0; channum < 18; channum++) {
		opl3.channel[channum].slots[0] = &opl3.slot[2 * channum];
		opl3.channel[channum].slots[1] = &opl3.slot[2 * channum + 1];
		if ((channum % 9) < 3) {
			opl3.channel[channum].pair = &opl3.channel[channum + 3];
		}
		else if ((channum % 9) < 6) {
			opl3.channel[channum].pair = &opl3.channel[channum - 3];
		}
		opl3.channel[channum].chip = &opl3;
		opl3.channel[channum].out[0] = &opl3.zeromod;
		opl3.channel[channum].out[1] = &opl3.zeromod;
		opl3.channel[channum].out[2] = &opl3.zeromod;
		opl3.channel[channum].out[3] = &opl3.zeromod;
		opl3.channel[channum].chtype = ch_2op;
		opl3.channel[channum].cha = ~0;
		opl3.channel[channum].chb = ~0;
		opl3.channel[channum].fcha = 1.0;
		opl3.channel[channum].fchb = 1.0;
		chan_setupalg(&opl3.channel[channum]);
	}
	opl3.noise = 0x306600;
}

void NukedOPL3::WriteReg(int reg, int v) {
	v &= 0xff;
	reg &= 0x1ff;
	Bit8u high = (reg >> 8) & 0x01;
	Bit8u regm = reg & 0xff;
	switch (regm & 0xf0) {
	case 0x00:
		if (high) {
			switch (regm & 0x0f) {
			case 0x04:
				chan_set4op(&opl3, v);
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
			slot_write20(&opl3.slot[18 * high + ad_slot[regm & 0x1f]], v);
		}
		break;
	case 0x40:
	case 0x50:
		if (ad_slot[regm & 0x1f] >= 0) {
			slot_write40(&opl3.slot[18 * high + ad_slot[regm & 0x1f]], v);
		}
		break;
	case 0x60:
	case 0x70:
		if (ad_slot[regm & 0x1f] >= 0) {
			slot_write60(&opl3.slot[18 * high + ad_slot[regm & 0x1f]], v);
		}
		break;
	case 0x80:
	case 0x90:
		if (ad_slot[regm & 0x1f] >= 0) {
			slot_write80(&opl3.slot[18 * high + ad_slot[regm & 0x1f]], v);;
		}
		break;
	case 0xe0:
	case 0xf0:
		if (ad_slot[regm & 0x1f] >= 0) {
			slot_writee0(&opl3.slot[18 * high + ad_slot[regm & 0x1f]], v);
		}
		break;
	case 0xa0:
		if ((regm & 0x0f) < 9) {
			chan_writea0(&opl3.channel[9 * high + (regm & 0x0f)], v);
		}
		break;
	case 0xb0:
		if (regm == 0xbd && !high) {
			opl3.dam = v >> 7;
			opl3.dvb = (v >> 6) & 0x01;
			chan_updaterhythm(&opl3, v);
		}
		else if ((regm & 0x0f) < 9) {
			chan_writeb0(&opl3.channel[9 * high + (regm & 0x0f)], v);
			if (v & 0x20) {
				chan_enable(&opl3.channel[9 * high + (regm & 0x0f)]);
			}
			else {
				chan_disable(&opl3.channel[9 * high + (regm & 0x0f)]);
			}
		}
		break;
	case 0xc0:
		if ((regm & 0x0f) < 9) {
			chan_writec0(&opl3.channel[9 * high + (regm & 0x0f)], v);
		}
		break;
	}
}

void NukedOPL3::Update(float* sndptr, int numsamples) {
	Bit32s outa, outb;
	for (Bit32u i = 0; i < (Bit32u)numsamples; i++) {
		outa = 0;
		outb = 0;
		for (Bit8u ii = 0; ii < 36; ii++) {
			slot_calgfb(&opl3.slot[ii]);
		}
		chan_generaterhythm(&opl3);
		for (Bit8u ii = 0; ii < 18; ii++) {
			chan_generate(&opl3.channel[ii]);
			Bit16s accm = 0;
			for (Bit8u jj = 0; jj < 4; jj++) {
				accm += *opl3.channel[ii].out[jj];
			}
			if (FullPan) {
				outa += (Bit16s)(accm * opl3.channel[ii].fcha);
				outb += (Bit16s)(accm * opl3.channel[ii].fchb);
			}
			else {
				outa += (Bit16s)(accm & opl3.channel[ii].cha);
				outb += (Bit16s)(accm & opl3.channel[ii].chb);
			}
		}
		for (Bit8u ii = 0; ii < 36; ii++) {
			envelope_calc(&opl3.slot[ii]);
			pg_generate(&opl3.slot[ii]);
		}
		n_generate(&opl3);
		opl3.timer++;
		if (!(opl3.timer & 0x3f)) {
			if (!opl3.tremdir) {
				if (opl3.tremtval == 105) {
					opl3.tremtval--;
					opl3.tremdir = 1;
				}
				else {
					opl3.tremtval++;
				}
			}
			else {
				if (opl3.tremtval == 0) {
					opl3.tremtval++;
					opl3.tremdir = 0;
				}
				else {
					opl3.tremtval--;
				}
			}
			opl3.tremval = (opl3.tremtval >> 2) >> ((1 - opl3.dam) << 1);
		}
		*sndptr++ += (float)(outa / 10240.0);
		*sndptr++ += (float)(outb / 10240.0);
	}
}

void NukedOPL3::SetPanning(int c, float left, float right) {
	if (FullPan) {
		opl3.channel[c].fcha = left;
		opl3.channel[c].fchb = right;
	}
}

NukedOPL3::NukedOPL3(bool stereo) {
	FullPan = stereo;
	Reset();
}

OPLEmul *NukedOPL3Create(bool stereo) {
	return new NukedOPL3(stereo);
}