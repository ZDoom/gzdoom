/***************************************************************************
 * libgens: Gens Emulation Library.                                        *
 * Ym2612.hpp: Yamaha YM2612 FM synthesis chip emulator.                   *
 *                                                                         *
 * Copyright (c) 1999-2002 by Stéphane Dallongeville                       *
 * Copyright (c) 2003-2004 by Stéphane Akhoun                              *
 * Copyright (c) 2008-2010 by David Korth                                  *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

/***********************************************************
 *                                                         *
 * YM2612.C : YM2612 emulator                              *
 *                                                         *
 * Almost constantes are taken from the MAME core          *
 *                                                         *
 * This source is a part of Gens project                   *
 * Written by Stéphane Dallongeville (gens@consolemul.com) *
 * Copyright (c) 2002 by Stéphane Dallongeville            *
 *                                                         *
 ***********************************************************/

#include "Ym2612.hpp"
#include "Ym2612_p.hpp"

// C includes.
#include <stdint.h>

// C includes. (C++ namespace)
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cassert>

/* Message logging. */
//#define LOG_MSG(...)
//#include "macros/log_msg.h"

#if 0
// Sound Manager.
#include "SoundMgr.hpp"
#include "Vdp/Vdp.hpp"

// TODO: Get rid of EmuContext.
#include "EmuContext/EmuContext.hpp"
#endif

namespace LibGens {

/** Ym2612Private **/

// Static variables.
bool Ym2612Private::isInit = false;
int *Ym2612Private::SIN_TAB[SIN_LENGTH];			// SINUS TABLE (pointer on TL TABLE)
int Ym2612Private::TL_TAB[TL_LENGTH * 2];			// TOTAL LEVEL TABLE (plus and minus)
unsigned int Ym2612Private::ENV_TAB[2 * ENV_LENGTH * 8];	// ENV CURVE TABLE (attack & decay)
//unsigned int Ym2612Private::ATTACK_TO_DECAY[ENV_LENGTH];	// Conversion from attack to decay phase
unsigned int Ym2612Private::DECAY_TO_ATTACK[ENV_LENGTH];	// Conversion from decay to attack phase

// libOPNMIDI: Fixed the compatibility with the error: "floating-point literal cannot appear in a constant-expression"
int Ym2612Private::LIMIT_CH_OUT = ((int) (((1 << OUT_BITS) * 1.5) - 1));
int Ym2612Private::PG_CUT_OFF = ((int) (78.0 / ENV_STEP));
int Ym2612Private::ENV_CUT_OFF = ((int) (68.0 / ENV_STEP));
int Ym2612Private::LFO_FMS_BASE = ((int) (0.05946309436 * 0.0338 * (double) (1 << LFO_FMS_LBITS)));

// Next Enveloppe phase functions pointer table
const Ym2612Private::Env_Event Ym2612Private::ENV_NEXT_EVENT[8] = {
	Env_Attack_Next,
	Env_Decay_Next,
	Env_Substain_Next,
	Env_Release_Next,
	Env_NULL_Next,
	Env_NULL_Next,
	Env_NULL_Next,
	Env_NULL_Next
};

// Default detune table.
// FD == F number
const uint8_t Ym2612Private::DT_DEF_TAB[4][32] = {
	// FD = 0
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

	// FD = 1
	{0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
	 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8},

	// FD = 2
	{1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
	 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 16, 16, 16},

	// FD = 3
	{2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
	 8, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 20, 22, 22, 22, 22}
};

const uint8_t Ym2612Private::FKEY_TAB[16] = {
	0, 0, 0, 0,
	0, 0, 0, 1,
	2, 3, 3, 3,
	3, 3, 3, 3
};

const uint8_t Ym2612Private::LFO_AMS_TAB[4] = {
	31, 4, 1, 0
};

const uint8_t Ym2612Private::LFO_FMS_TAB[8] = {
	static_cast<uint8_t>(LFO_FMS_BASE * 0), static_cast<uint8_t>(LFO_FMS_BASE * 1),
	static_cast<uint8_t>(LFO_FMS_BASE * 2), static_cast<uint8_t>(LFO_FMS_BASE * 3),
	static_cast<uint8_t>(LFO_FMS_BASE * 4), static_cast<uint8_t>(LFO_FMS_BASE * 6),
	static_cast<uint8_t>(LFO_FMS_BASE * 12), static_cast<uint8_t>(LFO_FMS_BASE * 24)
};

/*
 * Pan law table
 */
static const unsigned short panlawtable[] =
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

unsigned int Ym2612Private::SL_TAB[16];		// Sustain level table. (STATIC)
unsigned int Ym2612Private::NULL_RATE[32];	// Table for NULL rate. (STATIC)

// LFO tables.
int Ym2612Private::LFO_ENV_TAB[LFO_LENGTH];	// LFO AMS TABLE (adjusted for 11.8 dB)
int Ym2612Private::LFO_FREQ_TAB[LFO_LENGTH];	// LFO FMS TABLE

// NOTE: INTER_TAB isn't used...
//int Ym2612::INTER_TAB[MAX_UPDATE_LENGTH];	// Interpolation table

/** Gens-specific **/

// TODO for LibGens
#if 0
#include "util/sound/gym.hpp"
#endif
/** end **/

Ym2612Private::Ym2612Private(Ym2612 *q)
	: q(q)
{
	if (!isInit) {
		// Initialize the static tables.
		isInit = true;
		doStaticInit();
	}
}

void Ym2612Private::doStaticInit(void)
{
	// Sine table:
	// SIN_TAB[x][y] = sin(x) * y;
	// x = phase and y = volume
	SIN_TAB[0] = SIN_TAB[SIN_LENGTH / 2] = &TL_TAB[(int)PG_CUT_OFF];

	for (int i = 1; i <= SIN_LENGTH / 4; i++) {
		double x = sin(2.0 * PI * (double) (i) / (double) (SIN_LENGTH));	// Sinus
		x = 20 * log10(1 / x);	// convert to dB

		int j = (int)(x / ENV_STEP);	// Get TL range
		if (j > PG_CUT_OFF) {
			j = (int) PG_CUT_OFF;
		}

		SIN_TAB[i] = SIN_TAB[(SIN_LENGTH / 2) - i] = &TL_TAB[j];
		SIN_TAB[(SIN_LENGTH / 2) + i] = SIN_TAB[SIN_LENGTH - i] = &TL_TAB[TL_LENGTH + j];

//		LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG3,
//			"SIN[%d][0] = %.8X    SIN[%d][0] = %.8X    SIN[%d][0] = %.8X    SIN[%d][0] = %.8X",
//			i, SIN_TAB[i][0], (SIN_LENGTH / 2) - i,
//			SIN_TAB[(SIN_LENGTH / 2) - i][0], (SIN_LENGTH / 2) + i,
//			SIN_TAB[(SIN_LENGTH / 2) + i][0], SIN_LENGTH - i,
//			SIN_TAB[SIN_LENGTH - i][0]);
	}

	// LFO table:
	for (int i = 0; i < LFO_LENGTH; i++) {
		double x = sin (2.0 * PI * (double) (i) / (double) (LFO_LENGTH));	// Sinus
		x += 1.0;
		x /= 2.0;		// positive only
		x *= 11.8 / ENV_STEP;	// adjusted to MAX envelope modulation

		LFO_ENV_TAB[i] = (int) x;

		x = sin(2.0 * PI * (double) (i) / (double) (LFO_LENGTH));	// Sinus
		x *= (double) ((1 << (LFO_HBITS - 1)) - 1);

		LFO_FREQ_TAB[i] = (int) x;

//		LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG3,
//			"LFO[%d] = %.8X", i, LFO_ENV_TAB[i]);
	}

	// Envelope table:
	// ENV_TAB[0] -> ENV_TAB[ENV_LENGTH - 1]              = attack curve
	// ENV_TAB[ENV_LENGTH] -> ENV_TAB[2 * ENV_LENGTH - 1] = decay curve
	for (int i = 0; i < ENV_LENGTH; i++) {
		// Attack curve (x^8 - music level 2 Vectorman 2)
		double x = pow(((double)((ENV_LENGTH - 1) - i) / (double)(ENV_LENGTH)), 8);
		x *= ENV_LENGTH;

		ENV_TAB[i] = (int)x;

		// Decay curve (just linear)
		x = pow(((double) (i) / (double) (ENV_LENGTH)), 1);
		x *= ENV_LENGTH;

		ENV_TAB[ENV_LENGTH + i] = (int)x;

//		LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG3,
//			"ATTACK[%d] = %d   DECAY[%d] = %d",
//			i, ENV_TAB[i], i, ENV_TAB[ENV_LENGTH + i]);
	}

	ENV_TAB[ENV_END >> ENV_LBITS] = ENV_LENGTH - 1;	// for the stopped state

	// Decay -> Attack table.
	// NOTE: There used to be an Attack -> Decay table,
	// but it wasn't implemented as of the original
	// port of Gens for Linux...
	for (int i = 0, j = ENV_LENGTH - 1; i < ENV_LENGTH; i++) {
		while (j && (ENV_TAB[j] < (unsigned) i))
			j--;

		DECAY_TO_ATTACK[i] = j << ENV_LBITS;
	}

	// Sustain Level table:
	for (int i = 0; i < 15; i++) {
		double x = i * 3;		// 3 and not 6 (Mickey Mania first music for test)
		x /= ENV_STEP;

		int j = (int)x;
		j <<= ENV_LBITS;

		SL_TAB[i] = j + ENV_DECAY;
	}

	int j = ENV_LENGTH - 1;		// special case : volume off
	j <<= ENV_LBITS;
	SL_TAB[15] = j + ENV_DECAY;

	// TL table:
	// [0     -  4095] = +output  [4095  - ...] = +output overflow (fill with 0)
	// [12288 - 16383] = -output  [16384 - ...] = -output overflow (fill with 0)
	for (int i = 0; i < TL_LENGTH; i++) {
		if (i >= PG_CUT_OFF) {
			// YM2612 cut off sound after 78 dB (14 bits output ?)
			TL_TAB[TL_LENGTH + i] = TL_TAB[i] = 0;
		} else {
			double x = MAX_OUT;			// Max output
			x /= pow(10, (ENV_STEP * i) / 20);	// Decibel -> Voltage

			TL_TAB[i] = (int) x;
			TL_TAB[TL_LENGTH + i] = -TL_TAB[i];
		}

//		LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG3,
//			"TL_TAB[%d] = %.8X    TL_TAB[%d] = %.8X",
//			i, TL_TAB[i], TL_LENGTH + i, TL_TAB[TL_LENGTH + i]);
	}

	// NULL frequency rate table.
	// It's always 0.
	memset(NULL_RATE, 0, sizeof(NULL_RATE));
}

/*****************************************
 * Functions for calculating parameters. *
 *****************************************/

inline void Ym2612Private::CALC_FINC_SL(slot_t *SL, int finc, int kc)
{
	int ksr;

	SL->Finc = (finc + SL->DT[kc]) * SL->MUL;
	ksr = kc >> SL->KSR_S;	// keycode atténuation

//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//		"FINC = %d  SL->Finc = %d", finc, SL->Finc);

	if (SL->KSR != ksr) {
		// si le KSR a changé alors
		// les différents taux pour l'enveloppe sont mis à jour
		SL->KSR = ksr;

		SL->EincA = SL->AR[ksr];
		SL->EincD = SL->DR[ksr];
		SL->EincS = SL->SR[ksr];
		SL->EincR = SL->RR[ksr];

		if (SL->Ecurp == ATTACK) {
			SL->Einc = SL->EincA;
		} else if (SL->Ecurp == DECAY) {
			SL->Einc = SL->EincD;
		} else if (SL->Ecnt < ENV_END) {
			if (SL->Ecurp == SUSTAIN)
				SL->Einc = SL->EincS;
			else if (SL->Ecurp == RELEASE)
			SL->Einc = SL->EincR;
		}

//		LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//			"KSR = %.4X  EincA = %.8X EincD = %.8X EincS = %.8X EincR = %.8X",
//			ksr, SL->EincA, SL->EincD, SL->EincS, SL->EincR);
	}
}

inline void Ym2612Private::CALC_FINC_CH(channel_t *CH)
{
	int finc, kc;

	finc = FINC_TAB[CH->FNUM[0]] >> (7 - CH->FOCT[0]);
	kc = CH->KC[0];

	CALC_FINC_SL(&CH->_SLOT[0], finc, kc);
	CALC_FINC_SL(&CH->_SLOT[1], finc, kc);
	CALC_FINC_SL(&CH->_SLOT[2], finc, kc);
	CALC_FINC_SL(&CH->_SLOT[3], finc, kc);
}

/*********************************
 * Functions for setting values. *
 *********************************/

/**
 * KEY_ON event.
 * @param ch Channel.
 * @param nsl Slot number.
 */
void Ym2612Private::KEY_ON(channel_t *CH, int nsl)
{
	slot_t *SL = &(CH->_SLOT[nsl]);	// on recupère le bon pointeur de slot

	// la touche est-elle relâchée ?
	if (SL->Ecurp == RELEASE) {
		SL->Fcnt = 0;

		// Fix Ecco 2 splash sound
		SL->Ecnt = (DECAY_TO_ATTACK[ENV_TAB[SL->Ecnt >> ENV_LBITS]] + ENV_ATTACK) & SL->ChgEnM;
		SL->ChgEnM = ~0;

		/*
		SL->Ecnt = DECAY_TO_ATTACK[ENV_TAB[SL->Ecnt >> ENV_LBITS]] + ENV_ATTACK;
		SL->Ecnt = 0;
		*/

		SL->Einc = SL->EincA;
		SL->Ecmp = ENV_DECAY;
		SL->Ecurp = ATTACK;
	}
}

/**
 * KEY_OFF event.
 * @param ch Channel.
 * @param nsl Slot number.
 */
void Ym2612Private::KEY_OFF(channel_t *CH, int nsl)
{
	slot_t *SL = &(CH->_SLOT[nsl]);	// on recupère le bon pointeur de slot

	// la touche est-elle appuyée ?
	if (SL->Ecurp != RELEASE) {
		// attack phase ?
		if (SL->Ecnt < ENV_DECAY) {
			SL->Ecnt = (ENV_TAB[SL->Ecnt >> ENV_LBITS] << ENV_LBITS) + ENV_DECAY;
		}

		SL->Einc = SL->EincR;
		SL->Ecmp = ENV_END;
		SL->Ecurp = RELEASE;
	}
}

inline void Ym2612Private::CSM_Key_Control(void)
{
	KEY_ON(&state.CHANNEL[2], 0);
	KEY_ON(&state.CHANNEL[2], 1);
	KEY_ON(&state.CHANNEL[2], 2);
	KEY_ON(&state.CHANNEL[2], 3);
}

/**
 * Set a value for a slot.
 * @param address Register address.
 * @param data Data.
 */
int Ym2612Private::SLOT_SET(int address, uint8_t data)
{
	int nch = address & 3;
	if (nch == 3)
		return 1;
	if (address & 0x100)
		nch += 3;
	channel_t *CH = &state.CHANNEL[nch];

	const int nsl = (address >> 2) & 3;
	slot_t *SL = &CH->_SLOT[nsl];

	switch (address & 0xF0) {
		case 0x30:
			if ((SL->MUL = (data & 0x0F)) != 0) {
				SL->MUL <<= 1;
			} else {
				SL->MUL = 1;
			}

			SL->DT = DT_TAB[(data >> 4) & 7];
			CH->_SLOT[0].Finc = -1;
//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d], SLOT[%d] DTMUL = %.2X", nch, nsl, data & 0x7F);
			break;

		case 0x40:
			SL->TL = data & 0x7F;

			// SOR2 do a lot of TL adjustement and this fix R.Shinobi jump sound...
			q->specialUpdate();

		#if ((ENV_HBITS - 7) < 0)
			SL->TLL = SL->TL >> (7 - ENV_HBITS);
		#else
			SL->TLL = SL->TL << (ENV_HBITS - 7);
		#endif

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d], SLOT[%d] TL = %.2X", nch, nsl, SL->TL);
			break;

		case 0x50:
			SL->KSR_S = 3 - (data >> 6);
			CH->_SLOT[0].Finc = -1;

			if (data &= 0x1F) {
				SL->AR = &AR_TAB[data << 1];
			} else {
				SL->AR = &NULL_RATE[0];
			}

			SL->EincA = SL->AR[SL->KSR];
			if (SL->Ecurp == ATTACK)
				SL->Einc = SL->EincA;

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d], SLOT[%d] AR = %.2X  EincA = %.6X", nch, nsl, data, SL->EincA);
			break;

		case 0x60:
			if ((SL->AMSon = (data & 0x80))) {
				SL->AMS = CH->AMS;
			} else {
				SL->AMS = 31;
			}

			if (data &= 0x1F) {
				SL->DR = &DR_TAB[data << 1];
			} else {
				SL->DR = &NULL_RATE[0];
			}

			SL->EincD = SL->DR[SL->KSR];
			if (SL->Ecurp == DECAY)
				SL->Einc = SL->EincD;

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d], SLOT[%d] AMS = %d  DR = %.2X  EincD = %.6X",
//				nch, nsl, SL->AMSon, data, SL->EincD);
			break;

		case 0x70:
			if (data &= 0x1F) {
				SL->SR = &DR_TAB[data << 1];
			} else {
				SL->SR = &NULL_RATE[0];
			}

			SL->EincS = SL->SR[SL->KSR];
			if ((SL->Ecurp == SUSTAIN) && (SL->Ecnt < ENV_END))
				SL->Einc = SL->EincS;

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d], SLOT[%d] SR = %.2X  EincS = %.6X",
//				nch, nsl, data, SL->EincS);
			break;

		case 0x80:
			SL->SLL = SL_TAB[data >> 4];
			SL->RR = &DR_TAB[((data & 0xF) << 2) + 2];
			SL->EincR = SL->RR[SL->KSR];
			if ((SL->Ecurp == RELEASE) && (SL->Ecnt < ENV_END))
				SL->Einc = SL->EincR;

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d], SLOT[%d] SL = %.8X", nch, nsl, SL->SLL);
//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d], SLOT[%d] RR = %.2X  EincR = %.2X",
//				nch, nsl, ((data & 0xF) << 1) | 2, SL->EincR);
			break;

		case 0x90:
			// SSG-EG envelope shapes:
			/*
			  E  At Al H
			  1  0  0  0  \\\\
			  1  0  0  1  \___
			  1  0  1  0  \/\/
			  1  0  1  1  \
			  1  1  0  0  ////
			  1  1  0  1  /
			  1  1  1  0  /\/\
			  1  1  1  1  /___

			  E  = SSG-EG enable
			  At = Start negate
			  Al = Altern
			  H  = Hold
			*/

			if (data & 0x08) {
				SL->SEG = data & 0x0F;
			} else {
				SL->SEG = 0;
			}

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d], SLOT[%d] SSG-EG = %.2X", nch, nsl, data);
			break;
	}

	return 0;
}

/**
 * Set a value for a channel.
 * @param address Register address.
 * @param data Data.
 */
int Ym2612Private::CHANNEL_SET(int address, uint8_t data)
{
	int nch = address & 3;
	if (nch == 3)
		return 1;
	if (address & 0x100)
		nch += 3;

	channel_t *CH = &state.CHANNEL[nch];

	switch (address & 0xFC) {
		case 0xA0:
			q->specialUpdate();

			CH->FNUM[0] = (CH->FNUM[0] & 0x700) + data;
			CH->KC[0] = (CH->FOCT[0] << 2) | FKEY_TAB[CH->FNUM[0] >> 7];

			CH->_SLOT[0].Finc = -1;

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d] part1 FNUM = %d  KC = %d",
//				nch, CH->FNUM[0], CH->KC[0]);
			break;

		case 0xA4:
			q->specialUpdate();

			CH->FNUM[0] = (CH->FNUM[0] & 0x0FF) + ((int) (data & 0x07) << 8);
			CH->FOCT[0] = (data & 0x38) >> 3;
			CH->KC[0] = (CH->FOCT[0] << 2) | FKEY_TAB[CH->FNUM[0] >> 7];

			CH->_SLOT[0].Finc = -1;

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d] part2 FNUM = %d  FOCT = %d  KC = %d",
//				nch, CH->FNUM[0], CH->FOCT[0], CH->KC[0]);
			break;

		case 0xA8:
			if (address < 0x100) {
				// Channel 3: Specific slot operator.
				const int nsl = nch + 1;

				q->specialUpdate();

				state.CHANNEL[2].FNUM[nsl] = (state.CHANNEL[2].FNUM[nsl] & 0x700) + data;
				state.CHANNEL[2].KC[nsl] = (state.CHANNEL[2].FOCT[nsl] << 2) |
								FKEY_TAB[state.CHANNEL[2].FNUM[nsl] >> 7];

				state.CHANNEL[2]._SLOT[0].Finc = -1;

//				LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//					"CHANNEL[2] part1 FNUM[%d] = %d  KC[%d] = %d",
//					nsl, state.CHANNEL[2].FNUM[nsl],
//					nsl, state.CHANNEL[2].KC[nsl]);
			}
			break;

		case 0xAC:
			if (address < 0x100) {
				// Channel 3: Specific slot operator.
				const int nsl = nch + 1;

				q->specialUpdate();

				state.CHANNEL[2].FNUM[nsl] = (state.CHANNEL[2].FNUM[nsl] & 0x0FF) +
								((int) (data & 0x07) << 8);
				state.CHANNEL[2].FOCT[nsl] = (data & 0x38) >> 3;
				state.CHANNEL[2].KC[nsl] = (state.CHANNEL[2].FOCT[nsl] << 2) |
								FKEY_TAB[state.CHANNEL[2].FNUM[nsl] >> 7];

				state.CHANNEL[2]._SLOT[0].Finc = -1;

//				LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//					"CHANNEL[2] part2 FNUM[%d] = %d  FOCT[%d] = %d  KC[%d] = %d",
//					nsl, state.CHANNEL[2].FNUM[nsl],
//					nsl, state.CHANNEL[2].FOCT[nsl],
//					nsl, state.CHANNEL[2].KC[nsl]);
			}
			break;

		case 0xB0:
			if (CH->ALGO != (data & 7)) {
				// Fix VectorMan 2 heli sound (level 1)
				q->specialUpdate();

				CH->ALGO = data & 7;

				CH->_SLOT[0].ChgEnM = 0;
				CH->_SLOT[1].ChgEnM = 0;
				CH->_SLOT[2].ChgEnM = 0;
				CH->_SLOT[3].ChgEnM = 0;
			}

			CH->FB = 9 - ((data >> 3) & 7);	// Real thing ?

			/*if (CH->FB = ((data >> 3) & 7)) {
				// Thunder force 4 (music stage 8), Gynoug, Aladdin bug sound...
				CH->FB = 9 - CH->FB;
			} else {
				CH->FB = 31;
			}*/

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//				"CHANNEL[%d] ALGO = %d  FB = %d", nch, CH->ALGO, CH->FB);
			break;

		case 0xB4:
			q->specialUpdate();

			// -1 = 0xFFFFFFFF. Useful trick from game-music-emu.
			CH->LEFT = 0 - ((data >> 7) & 1);
			CH->RIGHT = 0 - ((data >> 6) & 1);

			CH->AMS = LFO_AMS_TAB[(data >> 4) & 3];
			CH->FMS = LFO_FMS_TAB[data & 7];

			for (int i = 0; i < 3; i++) {
				slot_t *SL = &CH->_SLOT[i];
				SL->AMS = (SL->AMSon ? CH->AMS : 31);
			}

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG1,
//				"CHANNEL[%d] AMS = %d  FMS = %d", nch, CH->AMS, CH->FMS);
			break;
	}

	return 0;
}

/**
 * Set a value for the entire device.
 * @param address Register address.
 * @param data Data.
 */
int Ym2612Private::YM_SET(int address, uint8_t data)
{
	switch (address) {
		case 0x22:
			// LFO enable
			if (data & 8) {
				// Cool Spot music 1, LFO modified severals time which
				// distord the sound, have to check that on a real genesis...
				state.LFOinc = LFO_INC_TAB[data & 7];
//				LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG1,
//					"LFO Enable, LFOinc = %.8X   %d", state.LFOinc, data & 7);
			} else {
				state.LFOinc = state.LFOcnt = 0;
//				LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG1,
//					"LFO Disable");
			}
			break;

		case 0x24:
			state.TimerA = (state.TimerA & 0x003) | (((int)data) << 2);
			if (state.TimerAL != (1024 - state.TimerA) << 12) {
				state.TimerAcnt = state.TimerAL = (1024 - state.TimerA) << 12;
//				LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//					"Timer A Set = %.8X", state.TimerAcnt);
			}
			break;

		case 0x25:
			state.TimerA = (state.TimerA & 0x3FC) | (data & 3);
			if (state.TimerAL != (1024 - state.TimerA) << 12) {
				state.TimerAcnt = state.TimerAL = (1024 - state.TimerA) << 12;
//				LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//					"Timer A Set = %.8X", state.TimerAcnt);
			}
			break;

		case 0x26:
			state.TimerB = data;
			if (state.TimerBL != (256 - state.TimerB) << (4 + 12)) {
				state.TimerBcnt = state.TimerBL = (256 - state.TimerB) << (4 + 12);
//				LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//					"Timer B Set = %.8X", state.TimerBcnt);
			}
			break;

		case 0x27:
			// Paramètre divers
			// b7 = CSM MODE
			// b6 = 3 slot mode
			// b5 = reset b
			// b4 = reset a
			// b3 = timer enable b
			// b2 = timer enable a
			// b1 = load b
			// b0 = load a

			if ((data ^ state.Mode) & 0x40) {
				// We changed the channel 2 mode, so recalculate phase step
				// This fix the punch sound in Street of Rage 2
				q->specialUpdate();
				state.CHANNEL[2]._SLOT[0].Finc = -1;	// recalculate phase step
			}

			/*
			if ((data & 2) && (YM2612.Status & 2))
				YM2612.TimerBcnt = YM2612.TimerBL;
			if ((data & 1) && (YM2612.Status & 1))
				YM2612.TimerAcnt = YM2612.TimerAL;
			*/

			//YM2612.Status &= (~data >> 4);	// Reset du Status au cas ou c'est demandé
			state.status &= (~data >> 4) & (data >> 2);	// Reset Status

			state.Mode = data;

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG1,
//				"Mode reg = %.2X", data);
			break;

		case 0x28: {
			int nch = data & 3;
			if (nch == 3)
				return 1;

			if (data & 4)
				nch += 3;
			channel_t *CH = &state.CHANNEL[nch];

			q->specialUpdate();

			// KEY_ON or KEY_OFF the selected operator(s).
			if (data & 0x10)
				KEY_ON(CH, S0);		// On appuie sur la touche pour le slot 1
			else
				KEY_OFF(CH, S0);	// On relâche la touche pour le slot 1
			if (data & 0x20)
				KEY_ON(CH, S1);		// On appuie sur la touche pour le slot 3
			else
				KEY_OFF(CH, S1);	// On relâche la touche pour le slot 3
			if (data & 0x40)
				KEY_ON(CH, S2);		// On appuie sur la touche pour le slot 2
			else
				KEY_OFF(CH, S2);	// On relâche la touche pour le slot 2
			if (data & 0x80)
				KEY_ON(CH, S3);		// On appuie sur la touche pour le slot 4
			else
				KEY_OFF(CH, S3);	// On relâche la touche pour le slot 4

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG1,
//				"CHANNEL[%d]  KEY %.1X", nch, ((data & 0xF0) >> 4));
			break;
		}

		case 0x2A:
			// Set the DAC value.
			state.DACdata = ((int)data - 0x80) << 7;	// donnée du DAC
			break;

		case 0x2B:
			if (state.DAC ^ (data & 0x80))
				q->specialUpdate();

			// Enable/disable the DAC.
			state.DAC = data & 0x80;	// Activate / Deactivate the DAC.
			break;
	}

	return 0;
}

/***********************************************
 *          fonctions de génération            *
 ***********************************************/

void Ym2612Private::Env_NULL_Next(slot_t *SL)
{
	// Mark SL as unused.
	// TODO: Write a macro to do this!
	((void)SL);
}

void Ym2612Private::Env_Attack_Next(slot_t *SL)
{
	// Verified with Gynoug even in HQ (explode SFX)
	SL->Ecnt = ENV_DECAY;

	SL->Einc = SL->EincD;
	SL->Ecmp = SL->SLL;
	SL->Ecurp = DECAY;
}

void Ym2612Private::Env_Decay_Next(slot_t *SL)
{
	// Verified with Gynoug even in HQ (explode SFX)
	SL->Ecnt = SL->SLL;

	SL->Einc = SL->EincS;
	SL->Ecmp = ENV_END;
	SL->Ecurp = SUSTAIN;
}

void Ym2612Private::Env_Substain_Next(slot_t *SL)
{
	if (SL->SEG & 8) {
		// SSG-EG is enabled.
		if (SL->SEG & 1) {
			SL->Ecnt = ENV_END;
			SL->Einc = 0;
			SL->Ecmp = ENV_END + 1;
		} else {
			// re KEY ON

			// SL->Fcnt = 0;
			// SL->ChgEnM = 0xFFFFFFFF;

			SL->Ecnt = 0;
			SL->Einc = SL->EincA;
			SL->Ecmp = ENV_DECAY;
			SL->Ecurp = ATTACK;
		}

		SL->SEG ^= (SL->SEG & 2) << 1;
	} else {
		// SSG-EG is disabled.
		SL->Ecnt = ENV_END;
		SL->Einc = 0;
		SL->Ecmp = ENV_END + 1;
	}
}

void Ym2612Private::Env_Release_Next(slot_t *SL)
{
	SL->Ecnt = ENV_END;
	SL->Einc = 0;
	SL->Ecmp = ENV_END + 1;
}

#define GET_CURRENT_PHASE() do {	\
	in0 = CH->_SLOT[S0].Fcnt;	\
	in1 = CH->_SLOT[S1].Fcnt;	\
	in2 = CH->_SLOT[S2].Fcnt;	\
	in3 = CH->_SLOT[S3].Fcnt;	\
} while (0)

#define UPDATE_PHASE() do {				\
	CH->_SLOT[S0].Fcnt += CH->_SLOT[S0].Finc;	\
	CH->_SLOT[S1].Fcnt += CH->_SLOT[S1].Finc;	\
	CH->_SLOT[S2].Fcnt += CH->_SLOT[S2].Finc;	\
	CH->_SLOT[S3].Fcnt += CH->_SLOT[S3].Finc;	\
} while (0)

#define UPDATE_PHASE_LFO() do {										\
if ((freq_LFO = (CH->FMS * LFO_FREQ_UP[i]) >> (LFO_HBITS - 1)))						\
{													\
	CH->_SLOT[S0].Fcnt += CH->_SLOT[S0].Finc + ((CH->_SLOT[S0].Finc * freq_LFO) >> LFO_FMS_LBITS);	\
	CH->_SLOT[S1].Fcnt += CH->_SLOT[S1].Finc + ((CH->_SLOT[S1].Finc * freq_LFO) >> LFO_FMS_LBITS);	\
	CH->_SLOT[S2].Fcnt += CH->_SLOT[S2].Finc + ((CH->_SLOT[S2].Finc * freq_LFO) >> LFO_FMS_LBITS);	\
	CH->_SLOT[S3].Fcnt += CH->_SLOT[S3].Finc + ((CH->_SLOT[S3].Finc * freq_LFO) >> LFO_FMS_LBITS);	\
}													\
else													\
{													\
	CH->_SLOT[S0].Fcnt += CH->_SLOT[S0].Finc;							\
	CH->_SLOT[S1].Fcnt += CH->_SLOT[S1].Finc;							\
	CH->_SLOT[S2].Fcnt += CH->_SLOT[S2].Finc;							\
	CH->_SLOT[S3].Fcnt += CH->_SLOT[S3].Finc;							\
}													\
} while (0)

// Commented out from Gens Rerecording
/*
#define GET_CURRENT_ENV											\
if (CH->_SLOT[S0].SEG & 4)										\
{													\
	if ((en0 = ENV_TAB[(CH->_SLOT[S0].Ecnt >> ENV_LBITS)] + CH->_SLOT[S0].TLL) > ENV_MASK) en0 = 0;	\
	else en0 ^= ENV_MASK;										\
}													\
else en0 = ENV_TAB[(CH->_SLOT[S0].Ecnt >> ENV_LBITS)] + CH->_SLOT[S0].TLL;				\
if (CH->_SLOT[S1].SEG & 4)										\
{													\
	if ((en1 = ENV_TAB[(CH->_SLOT[S1].Ecnt >> ENV_LBITS)] + CH->_SLOT[S1].TLL) > ENV_MASK) en1 = 0;	\
	else en1 ^= ENV_MASK;										\
}													\
else en1 = ENV_TAB[(CH->_SLOT[S1].Ecnt >> ENV_LBITS)] + CH->_SLOT[S1].TLL;				\
if (CH->_SLOT[S2].SEG & 4)										\
{													\
	if ((en2 = ENV_TAB[(CH->_SLOT[S2].Ecnt >> ENV_LBITS)] + CH->_SLOT[S2].TLL) > ENV_MASK) en2 = 0;	\
	else en2 ^= ENV_MASK;										\
}													\
else en2 = ENV_TAB[(CH->_SLOT[S2].Ecnt >> ENV_LBITS)] + CH->_SLOT[S2].TLL;				\
if (CH->_SLOT[S3].SEG & 4)										\
{													\
	if ((en3 = ENV_TAB[(CH->_SLOT[S3].Ecnt >> ENV_LBITS)] + CH->_SLOT[S3].TLL) > ENV_MASK) en3 = 0;	\
	else en3 ^= ENV_MASK;										\
}													\
else en3 = ENV_TAB[(CH->_SLOT[S3].Ecnt >> ENV_LBITS)] + CH->_SLOT[S3].TLL;
*/

// New version from Gens Rerecording
#define GET_CURRENT_ENV() do {						\
	en0 = ENV_TAB[(CH->_SLOT[S0].Ecnt >> ENV_LBITS)] + CH->_SLOT[S0].TLL;	\
	en1 = ENV_TAB[(CH->_SLOT[S1].Ecnt >> ENV_LBITS)] + CH->_SLOT[S1].TLL;	\
	en2 = ENV_TAB[(CH->_SLOT[S2].Ecnt >> ENV_LBITS)] + CH->_SLOT[S2].TLL;	\
	en3 = ENV_TAB[(CH->_SLOT[S3].Ecnt >> ENV_LBITS)] + CH->_SLOT[S3].TLL;	\
} while (0)

// Commented out from Gens Rerecording
/*
#define GET_CURRENT_ENV_LFO										\
env_LFO = LFO_ENV_UP[i];										\
													\
if (CH->_SLOT[S0].SEG & 4)										\
{													\
	if ((en0 = ENV_TAB[(CH->_SLOT[S0].Ecnt >> ENV_LBITS)] + CH->_SLOT[S0].TLL) > ENV_MASK) en0 = 0;	\
	else en0 = (en0 ^ ENV_MASK) + (env_LFO >> CH->_SLOT[S0].AMS);					\
}													\
else en0 = ENV_TAB[(CH->_SLOT[S0].Ecnt >> ENV_LBITS)] + CH->_SLOT[S0].TLL + (env_LFO >> CH->_SLOT[S0].AMS); \
if (CH->_SLOT[S1].SEG & 4)										\
{													\
	if ((en1 = ENV_TAB[(CH->_SLOT[S1].Ecnt >> ENV_LBITS)] + CH->_SLOT[S1].TLL) > ENV_MASK) en1 = 0;	\
	else en1 = (en1 ^ ENV_MASK) + (env_LFO >> CH->_SLOT[S1].AMS);					\
}													\
else en1 = ENV_TAB[(CH->_SLOT[S1].Ecnt >> ENV_LBITS)] + CH->_SLOT[S1].TLL + (env_LFO >> CH->_SLOT[S1].AMS); \
if (CH->_SLOT[S2].SEG & 4)										\
{													\
	if ((en2 = ENV_TAB[(CH->_SLOT[S2].Ecnt >> ENV_LBITS)] + CH->_SLOT[S2].TLL) > ENV_MASK) en2 = 0;	\
	else en2 = (en2 ^ ENV_MASK) + (env_LFO >> CH->_SLOT[S2].AMS);					\
}													\
else en2 = ENV_TAB[(CH->_SLOT[S2].Ecnt >> ENV_LBITS)] + CH->_SLOT[S2].TLL + (env_LFO >> CH->_SLOT[S2].AMS); \
if (CH->_SLOT[S3].SEG & 4)										\
{													\
	if ((en3 = ENV_TAB[(CH->_SLOT[S3].Ecnt >> ENV_LBITS)] + CH->_SLOT[S3].TLL) > ENV_MASK) en3 = 0;	\
	else en3 = (en3 ^ ENV_MASK) + (env_LFO >> CH->_SLOT[S3].AMS);					\
}													\
else en3 = ENV_TAB[(CH->_SLOT[S3].Ecnt >> ENV_LBITS)] + CH->_SLOT[S3].TLL + (env_LFO >> CH->_SLOT[S3].AMS);
*/

// New version from Gens Rerecording
#define GET_CURRENT_ENV_LFO() do {									\
env_LFO = LFO_ENV_UP[i];										\
en0 = ENV_TAB[(CH->_SLOT[S0].Ecnt >> ENV_LBITS)] + CH->_SLOT[S0].TLL + (env_LFO >> CH->_SLOT[S0].AMS);	\
en1 = ENV_TAB[(CH->_SLOT[S1].Ecnt >> ENV_LBITS)] + CH->_SLOT[S1].TLL + (env_LFO >> CH->_SLOT[S1].AMS);	\
en2 = ENV_TAB[(CH->_SLOT[S2].Ecnt >> ENV_LBITS)] + CH->_SLOT[S2].TLL + (env_LFO >> CH->_SLOT[S2].AMS);	\
en3 = ENV_TAB[(CH->_SLOT[S3].Ecnt >> ENV_LBITS)] + CH->_SLOT[S3].TLL + (env_LFO >> CH->_SLOT[S3].AMS);	\
} while (0)

#define UPDATE_ENV() do {							\
	if ((CH->_SLOT[S0].Ecnt += CH->_SLOT[S0].Einc) >= CH->_SLOT[S0].Ecmp)	\
		ENV_NEXT_EVENT[CH->_SLOT[S0].Ecurp](&(CH->_SLOT[S0]));		\
	if ((CH->_SLOT[S1].Ecnt += CH->_SLOT[S1].Einc) >= CH->_SLOT[S1].Ecmp)	\
		ENV_NEXT_EVENT[CH->_SLOT[S1].Ecurp](&(CH->_SLOT[S1]));		\
	if ((CH->_SLOT[S2].Ecnt += CH->_SLOT[S2].Einc) >= CH->_SLOT[S2].Ecmp)	\
		ENV_NEXT_EVENT[CH->_SLOT[S2].Ecurp](&(CH->_SLOT[S2]));		\
	if ((CH->_SLOT[S3].Ecnt += CH->_SLOT[S3].Einc) >= CH->_SLOT[S3].Ecmp)	\
		ENV_NEXT_EVENT[CH->_SLOT[S3].Ecurp](&(CH->_SLOT[S3]));		\
} while (0)

#define DO_LIMIT() do {				\
	if (CH->OUTd > LIMIT_CH_OUT)		\
		CH->OUTd = LIMIT_CH_OUT;	\
	else if (CH->OUTd < -LIMIT_CH_OUT)	\
		CH->OUTd = -LIMIT_CH_OUT;	\
} while (0)

#define DO_FEEDBACK0() do {						\
	in0 += CH->S0_OUT[0] >> CH->FB;					\
	CH->S0_OUT[0] = SIN_TAB[(in0 >> SIN_LBITS) & SIN_MASK][en0];	\
} while (0)

#define DO_FEEDBACK() do {						\
	in0 += (CH->S0_OUT[0] + CH->S0_OUT[1]) >> CH->FB;		\
	CH->S0_OUT[1] = CH->S0_OUT[0];					\
	CH->S0_OUT[0] = SIN_TAB[(in0 >> SIN_LBITS) & SIN_MASK][en0];	\
} while (0)

#define DO_FEEDBACK2() do {								\
	in0 += (CH->S0_OUT[0] + (CH->S0_OUT[0] >> 2) + CH->S0_OUT[1]) >> CH->FB;	\
	CH->S0_OUT[1] = CH->S0_OUT[0] >> 2;						\
	CH->S0_OUT[0] = SIN_TAB[(in0 >> SIN_LBITS) & SIN_MASK][en0];			\
} while (0)

#define DO_FEEDBACK3() do {									\
	in0 += (CH->S0_OUT[0] + CH->S0_OUT[1] + CH->S0_OUT[2] + CH->S0_OUT[3]) >> CH->FB;	\
	CH->S0_OUT[3] = CH->S0_OUT[2] >> 1;							\
	CH->S0_OUT[2] = CH->S0_OUT[1] >> 1;							\
	CH->S0_OUT[1] = CH->S0_OUT[0] >> 1;							\
	CH->S0_OUT[0] = SIN_TAB[(in0 >> SIN_LBITS) & SIN_MASK][en0];				\
} while (0)

#define DO_ALGO_0() do {							\
	DO_FEEDBACK();								\
	in1 += CH->S0_OUT[0];							\
	in2 += SIN_TAB[(in1 >> SIN_LBITS) & SIN_MASK][en1];			\
	in3 += SIN_TAB[(in2 >> SIN_LBITS) & SIN_MASK][en2];			\
	CH->OUTd = (SIN_TAB[(in3 >> SIN_LBITS) & SIN_MASK][en3]) >> OUT_SHIFT;	\
} while (0)

#define DO_ALGO_1() do {							\
	DO_FEEDBACK();								\
	in2 += CH->S0_OUT[0] + SIN_TAB[(in1 >> SIN_LBITS) & SIN_MASK][en1];	\
	in3 += SIN_TAB[(in2 >> SIN_LBITS) & SIN_MASK][en2];			\
	CH->OUTd = (SIN_TAB[(in3 >> SIN_LBITS) & SIN_MASK][en3]) >> OUT_SHIFT;	\
} while (0)

#define DO_ALGO_2() do {							\
	DO_FEEDBACK();								\
	in2 += SIN_TAB[(in1 >> SIN_LBITS) & SIN_MASK][en1];			\
	in3 += CH->S0_OUT[0] + SIN_TAB[(in2 >> SIN_LBITS) & SIN_MASK][en2];	\
	CH->OUTd = (SIN_TAB[(in3 >> SIN_LBITS) & SIN_MASK][en3]) >> OUT_SHIFT;	\
} while (0)

#define DO_ALGO_3() do {							\
	DO_FEEDBACK();								\
	in1 += CH->S0_OUT[0];							\
	in3 += SIN_TAB[(in1 >> SIN_LBITS) & SIN_MASK][en1] +			\
	       SIN_TAB[(in2 >> SIN_LBITS) & SIN_MASK][en2];			\
	CH->OUTd = (SIN_TAB[(in3 >> SIN_LBITS) & SIN_MASK][en3]) >> OUT_SHIFT;	\
} while (0)

#define DO_ALGO_4() do {								\
	DO_FEEDBACK();									\
	in1 += CH->S0_OUT[0];								\
	in3 += SIN_TAB[(in2 >> SIN_LBITS) & SIN_MASK][en2];				\
	CH->OUTd = ((int)SIN_TAB[(in3 >> SIN_LBITS) & SIN_MASK][en3] +			\
		    (int)SIN_TAB[(in1 >> SIN_LBITS) & SIN_MASK][en1]) >> OUT_SHIFT;	\
	DO_LIMIT();									\
} while (0)

#define DO_ALGO_5() do {								\
	DO_FEEDBACK();									\
	in1 += CH->S0_OUT[0];								\
	in2 += CH->S0_OUT[0];								\
	in3 += CH->S0_OUT[0];								\
	CH->OUTd = ((int)SIN_TAB[(in3 >> SIN_LBITS) & SIN_MASK][en3] +			\
		    (int)SIN_TAB[(in1 >> SIN_LBITS) & SIN_MASK][en1] +			\
		    (int)SIN_TAB[(in2 >> SIN_LBITS) & SIN_MASK][en2]) >> OUT_SHIFT;	\
	DO_LIMIT();									\
} while (0)

#define DO_ALGO_6() do {								\
	DO_FEEDBACK();									\
	in1 += CH->S0_OUT[0];								\
	CH->OUTd = ((int)SIN_TAB[(in3 >> SIN_LBITS) & SIN_MASK][en3] +			\
		    (int)SIN_TAB[(in1 >> SIN_LBITS) & SIN_MASK][en1] +			\
		    (int)SIN_TAB[(in2 >> SIN_LBITS) & SIN_MASK][en2]) >> OUT_SHIFT;	\
	DO_LIMIT();									\
} while (0)

#define DO_ALGO_7() do {						\
	DO_FEEDBACK();							\
	CH->OUTd = ((int)SIN_TAB[(in3 >> SIN_LBITS) & SIN_MASK][en3] +	\
		    (int)SIN_TAB[(in1 >> SIN_LBITS) & SIN_MASK][en1] +	\
		    (int)SIN_TAB[(in2 >> SIN_LBITS) & SIN_MASK][en2] +	\
		    CH->S0_OUT[0]) >> OUT_SHIFT;			\
	DO_LIMIT();							\
} while (0)

#define DO_OUTPUT() do {			\
	bufL[i] += (int)((CH->OUTd * CH->PANVolumeL / 65535) & CH->LEFT);	\
	bufR[i] += (int)((CH->OUTd * CH->PANVolumeR / 65535) & CH->RIGHT);	\
} while (0)

#define DO_OUTPUT_INT0() do {					\
	if ((int_cnt += state.Inter_Step) & 0x04000)	{	\
		int_cnt &= 0x3FFF;				\
		bufL[i] += (int)((CH->OUTd * CH->PANVolumeL / 65535) & CH->LEFT);		\
		bufR[i] += (int)((CH->OUTd * CH->PANVolumeR / 65535) & CH->RIGHT);		\
	} else {						\
		i--;						\
	}							\
} while (0)

#define DO_OUTPUT_INT1() do {					\
	CH->Old_OUTd = (CH->OUTd + CH->Old_OUTd) >> 1;		\
	if ((int_cnt += state.Inter_Step) & 0x04000) {		\
		int_cnt &= 0x3FFF;				\
		bufL[i] += (int)((CH->Old_OUTd * CH->PANVolumeL / 65535) & CH->LEFT);	\
		bufR[i] += (int)((CH->Old_OUTd * CH->PANVolumeR / 65535) & CH->RIGHT);	\
	} else {						\
		i--;						\
	}							\
} while (0)

#define DO_OUTPUT_INT2() do {					\
	if ((int_cnt += state.Inter_Step) & 0x04000) {		\
		int_cnt &= 0x3FFF;				\
		CH->Old_OUTd = (CH->OUTd + CH->Old_OUTd) >> 1;	\
		bufL[i] += (int)((CH->Old_OUTd * CH->PANVolumeL / 65535) & CH->LEFT);	\
		bufR[i] += (int)((CH->Old_OUTd * CH->PANVolumeR / 65535) & CH->RIGHT);	\
	} else {						\
		i--;						\
	} \							\
	CH->Old_OUTd = CH->OUTd;				\
} while (0)

#define DO_OUTPUT_INT() do {						\
	if ((int_cnt += state.Inter_Step) & 0x04000)	{		\
		int_cnt &= 0x3FFF;					\
		CH->Old_OUTd = (((int_cnt ^ 0x3FFF) * CH->OUTd) +	\
				(int_cnt * CH->Old_OUTd)) >> 14;	\
		bufL[i] += (int)((CH->Old_OUTd * CH->PANVolumeL / 65535) & CH->LEFT);		\
		bufR[i] += (int)((CH->Old_OUTd * CH->PANVolumeR / 65535) & CH->RIGHT);		\
	} else {							\
		i--;							\
	}								\
	CH->Old_OUTd = CH->OUTd;					\
} while (0)

template<int algo>
inline void Ym2612Private::T_Update_Chan(channel_t *CH, int32_t *bufL, int32_t *bufR, int length)
{
	// Check if the channel has reached the end of the update.
	{
		int not_end = (CH->_SLOT[S3].Ecnt - ENV_END);

		// Special cases.
		// Copied from Game_Music_Emu v0.5.2.
		if (algo == 7)
			not_end |= (CH->_SLOT[S0].Ecnt - ENV_END);
		if (algo >= 5)
			not_end |= (CH->_SLOT[S2].Ecnt - ENV_END);
		if (algo >= 4)
			not_end |= (CH->_SLOT[S1].Ecnt - ENV_END);

		if (not_end == 0)
			return;
	}

//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//		"Algo %d len = %d", algo, length);

	for (int i = 0; i < length; i++) {
		int in0, in1, in2, in3;		// current phase calculation
		int en0, en1, en2, en3;		// current envelope calculation

		GET_CURRENT_PHASE();
		UPDATE_PHASE();
		GET_CURRENT_ENV();
		UPDATE_ENV();

		assert(algo >= 0 && algo <= 7);
		switch (algo) {
			case 0:
				DO_ALGO_0();
				break;
			case 1:
				DO_ALGO_1();
				break;
			case 2:
				DO_ALGO_2();
				break;
			case 3:
				DO_ALGO_3();
				break;
			case 4:
				DO_ALGO_4();
				break;
			case 5:
				DO_ALGO_5();
				break;
			case 6:
				DO_ALGO_6();
				break;
			case 7:
				DO_ALGO_7();
				break;
			default:
				break;
		}

		DO_OUTPUT();
	}
}

template<int algo>
inline void Ym2612Private::T_Update_Chan_LFO(channel_t *CH, int32_t *bufL, int32_t *bufR, int length)
{
	// Check if the channel has reached the end of the update.
	{
		int not_end = (CH->_SLOT[S3].Ecnt - ENV_END);

		// Special cases.
		// Copied from Game_Music_Emu v0.5.2.
		if (algo == 7)
			not_end |= (CH->_SLOT[S0].Ecnt - ENV_END);
		if (algo >= 5)
			not_end |= (CH->_SLOT[S2].Ecnt - ENV_END);
		if (algo >= 4)
			not_end |= (CH->_SLOT[S1].Ecnt - ENV_END);

		if (not_end == 0)
			return;
	}

	int env_LFO, freq_LFO;

//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//		"Algo %d LFO len = %d", algo, length);

	for (int i = 0; i < length; i++) {
		int in0, in1, in2, in3;		// current phase calculation
		int en0, en1, en2, en3;		// current envelope calculation

		GET_CURRENT_PHASE();
		UPDATE_PHASE_LFO();
		GET_CURRENT_ENV_LFO();
		UPDATE_ENV();

		assert(algo >= 0 && algo <= 7);
		switch (algo) {
			case 0:
				DO_ALGO_0();
				break;
			case 1:
				DO_ALGO_1();
				break;
			case 2:
				DO_ALGO_2();
				break;
			case 3:
				DO_ALGO_3();
				break;
			case 4:
				DO_ALGO_4();
				break;
			case 5:
				DO_ALGO_5();
				break;
			case 6:
				DO_ALGO_6();
				break;
			case 7:
				DO_ALGO_7();
				break;
			default:
				break;
		}

		DO_OUTPUT();
	}
}

/******************************************************
 *          Interpolated output                       *
 *****************************************************/

template<int algo>
inline void Ym2612Private::T_Update_Chan_Int(channel_t *CH, int32_t *bufL, int32_t *bufR, int length)
{
	// Check if the channel has reached the end of the update.
	{
		int not_end = (CH->_SLOT[S3].Ecnt - ENV_END);

		// Special cases.
		// Copied from Game_Music_Emu v0.5.2.
		if (algo == 7)
			not_end |= (CH->_SLOT[S0].Ecnt - ENV_END);
		if (algo >= 5)
			not_end |= (CH->_SLOT[S2].Ecnt - ENV_END);
		if (algo >= 4)
			not_end |= (CH->_SLOT[S1].Ecnt - ENV_END);

		if (not_end == 0)
			return;
	}

//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//		"Algo %d Int len = %d", algo, length);

	int_cnt = state.Inter_Cnt;

	for (int i = 0; i < length; i++) {
		int in0, in1, in2, in3;		// current phase calculation
		int en0, en1, en2, en3;		// current envelope calculation

		GET_CURRENT_PHASE();
		UPDATE_PHASE();
		GET_CURRENT_ENV();
		UPDATE_ENV();

		assert(algo >= 0 && algo <= 7);
		switch (algo) {
			case 0:
				DO_ALGO_0();
				break;
			case 1:
				DO_ALGO_1();
				break;
			case 2:
				DO_ALGO_2();
				break;
			case 3:
				DO_ALGO_3();
				break;
			case 4:
				DO_ALGO_4();
				break;
			case 5:
				DO_ALGO_5();
				break;
			case 6:
				DO_ALGO_6();
				break;
			case 7:
				DO_ALGO_7();
				break;
			default:
				break;
		}

		DO_OUTPUT_INT();
	}
}

template<int algo>
inline void Ym2612Private::T_Update_Chan_LFO_Int(channel_t *CH, int32_t *bufL, int32_t *bufR, int length)
{
	// Check if the channel has reached the end of the update.
	{
		int not_end = (CH->_SLOT[S3].Ecnt - ENV_END);

		// Special cases.
		// Copied from Game_Music_Emu v0.5.2.
		if (algo == 7)
			not_end |= (CH->_SLOT[S0].Ecnt - ENV_END);
		if (algo >= 5)
			not_end |= (CH->_SLOT[S2].Ecnt - ENV_END);
		if (algo >= 4)
			not_end |= (CH->_SLOT[S1].Ecnt - ENV_END);

		if (not_end == 0)
			return;
	}

	int_cnt = state.Inter_Cnt;
	int env_LFO, freq_LFO;

//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//		"Algo %d LFO Int len = %d", algo, length);

	for (int i = 0; i < length; i++) {
		int in0, in1, in2, in3;		// current phase calculation
		int en0, en1, en2, en3;		// current envelope calculation

		GET_CURRENT_PHASE();
		UPDATE_PHASE_LFO();
		GET_CURRENT_ENV_LFO();
		UPDATE_ENV();

		assert(algo >= 0 && algo <= 7);
		switch (algo) {
			case 0:
				DO_ALGO_0();
				break;
			case 1:
				DO_ALGO_1();
				break;
			case 2:
				DO_ALGO_2();
				break;
			case 3:
				DO_ALGO_3();
				break;
			case 4:
				DO_ALGO_4();
				break;
			case 5:
				DO_ALGO_5();
				break;
			case 6:
				DO_ALGO_6();
				break;
			case 7:
				DO_ALGO_7();
				break;
			default:
				break;
		}

		DO_OUTPUT_INT();
	}
}

/**
 * Update Channel function.
 * Replaces the UPDATE_CHAN function pointer table.
 * NOTE: This will probably be slower than the function pointer table.
 * TODO: Figure out how to optimize it!
 */
void Ym2612Private::Update_Chan(int algo_type, channel_t *CH, int32_t *bufL, int32_t *bufR, int length)
{
	switch (algo_type & 0x1F) {
		case 0x00:	T_Update_Chan<0>(CH, bufL, bufR, length);		break;
		case 0x01:	T_Update_Chan<1>(CH, bufL, bufR, length);		break;
		case 0x02:	T_Update_Chan<2>(CH, bufL, bufR, length);		break;
		case 0x03:	T_Update_Chan<3>(CH, bufL, bufR, length);		break;
		case 0x04:	T_Update_Chan<4>(CH, bufL, bufR, length);		break;
		case 0x05:	T_Update_Chan<5>(CH, bufL, bufR, length);		break;
		case 0x06:	T_Update_Chan<6>(CH, bufL, bufR, length);		break;
		case 0x07:	T_Update_Chan<7>(CH, bufL, bufR, length);		break;

		case 0x08:	T_Update_Chan_LFO<0>(CH, bufL, bufR, length);		break;
		case 0x09:	T_Update_Chan_LFO<1>(CH, bufL, bufR, length);		break;
		case 0x0A:	T_Update_Chan_LFO<2>(CH, bufL, bufR, length);		break;
		case 0x0B:	T_Update_Chan_LFO<3>(CH, bufL, bufR, length);		break;
		case 0x0C:	T_Update_Chan_LFO<4>(CH, bufL, bufR, length);		break;
		case 0x0D:	T_Update_Chan_LFO<5>(CH, bufL, bufR, length);		break;
		case 0x0E:	T_Update_Chan_LFO<6>(CH, bufL, bufR, length);		break;
		case 0x0F:	T_Update_Chan_LFO<7>(CH, bufL, bufR, length);		break;

		case 0x10:	T_Update_Chan_Int<0>(CH, bufL, bufR, length);		break;
		case 0x11:	T_Update_Chan_Int<1>(CH, bufL, bufR, length);		break;
		case 0x12:	T_Update_Chan_Int<2>(CH, bufL, bufR, length);		break;
		case 0x13:	T_Update_Chan_Int<3>(CH, bufL, bufR, length);		break;
		case 0x14:	T_Update_Chan_Int<4>(CH, bufL, bufR, length);		break;
		case 0x15:	T_Update_Chan_Int<5>(CH, bufL, bufR, length);		break;
		case 0x16:	T_Update_Chan_Int<6>(CH, bufL, bufR, length);		break;
		case 0x17:	T_Update_Chan_Int<7>(CH, bufL, bufR, length);		break;

		case 0x18:	T_Update_Chan_LFO_Int<0>(CH, bufL, bufR, length);	break;
		case 0x19:	T_Update_Chan_LFO_Int<1>(CH, bufL, bufR, length);	break;
		case 0x1A:	T_Update_Chan_LFO_Int<2>(CH, bufL, bufR, length);	break;
		case 0x1B:	T_Update_Chan_LFO_Int<3>(CH, bufL, bufR, length);	break;
		case 0x1C:	T_Update_Chan_LFO_Int<4>(CH, bufL, bufR, length);	break;
		case 0x1D:	T_Update_Chan_LFO_Int<5>(CH, bufL, bufR, length);	break;
		case 0x1E:	T_Update_Chan_LFO_Int<6>(CH, bufL, bufR, length);	break;
		case 0x1F:	T_Update_Chan_LFO_Int<7>(CH, bufL, bufR, length);	break;

		default:
			break;
	}
}

/***********************************************
 *              Public functions.              *
 ***********************************************/

/** Ym2612 **/

Ym2612::Ym2612()
	: d(new Ym2612Private(this))
{
	// TODO: Some initialization should go here!
	m_writeLen = 0;
	m_enabled = true;	// TODO: Make this customizable.
	m_dacEnabled = true;	// TODO: Make this customizable.
	m_improved = true;	// TODO: Make this customizable.
}

Ym2612::Ym2612(int clock, int rate)
	: d(new Ym2612Private(this))
{
	// TODO: Some initialization should go here!
	m_writeLen = 0;
	m_enabled = true;	// TODO: Make this customizable.
	m_dacEnabled = true;	// TODO: Make this customizable.
	m_improved = true;	// TODO: Make this customizable.

	reInit(clock, rate);
}

Ym2612::~Ym2612()
{
	delete d;
}

/**
 * (Re-)Initialize the YM2612.
 * @param clock YM2612 clock frequency.
 * @param rate Sound rate.
 * @return 0 on success; non-zero on error.
 */
// Initialisation de l'émulateur YM2612
int Ym2612::reInit(int clock, int rate)
{
	assert(rate > 0);
	assert(clock > rate);
	if (rate <= 0 || clock <= rate)
		return -1;

	// Clear the state struct.
	memset(&d->state, 0, sizeof(d->state));
	d->state.Clock = clock;
	d->state.Rate = rate;

	// 144 = 12 * (prescale * 2) = 12 * 6 * 2
	// prescale set to 6 by default

	d->state.Frequence = ((double)(d->state.Clock) / (double)(d->state.Rate)) / 144.0;
	d->state.TimerBase = (int)(d->state.Frequence * 4096.0);

	if (m_improved && (d->state.Frequence > 1.0)) {
		d->state.Inter_Step = (unsigned int)((1.0 / d->state.Frequence) * (double)(0x4000));
		d->state.Inter_Cnt = 0;

		// We recalculate rate and frequence after interpolation
		d->state.Rate = (int)(d->state.Clock / 144.0);
		d->state.Frequence = 1.0;
	} else {
		// No interpolation.
		d->state.Inter_Step = 0x4000;
		d->state.Inter_Cnt = 0;
	}

//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG2,
//		"YM2612 frequency = %g, rate = %d, interp step = %.8X",
//		d->state.Frequence, d->state.Rate, d->state.Inter_Step);

	// Frequency Step table.
	for (int i = 0; i < 2048; i++) {
		double x = (double)(i) * d->state.Frequence;
		#if ((SIN_LBITS + SIN_HBITS - (21 - 7)) < 0)
			x /= (double) (1 << ((21 - 7) - SIN_LBITS - SIN_HBITS));
		#else
			x *= (double) (1 << (SIN_LBITS + SIN_HBITS - (21 - 7)));
		#endif
		x /= 2.0;			// because MUL = value * 2
		d->FINC_TAB[i] = (unsigned int)x;
	}

	// Attack and Decay Rate tables.
	// Entries 0-3 are always 0.
	for (int i = 0; i < 4; i++) {
		d->AR_TAB[i] = 0;
		d->DR_TAB[i] = 0;
	}

	// Attack and Decay Rate tables.
	// Calculate entries 32-63.
	for (int i = 0; i < 60; i++) {
		double x = d->state.Frequence;

		x *= 1.0 + ((i & 3) * 0.25);		 // bits 0-1 : x1.00, x1.25, x1.50, x1.75
		x *= (double) (1 << ((i >> 2)));	 // bits 2-5 : shift bits (x2^0 - x2^15)
		x *= (double) (d->ENV_LENGTH << ENV_LBITS); // on ajuste pour le tableau ENV_TAB

		d->AR_TAB[i + 4] = (unsigned int) (x / d->AR_RATE);
		d->DR_TAB[i + 4] = (unsigned int) (x / d->DR_RATE);
	}

	// Attack and Decay Rate tables.
	// Entries 64-95 are copies of entry 63.
	for (int i = 64; i < 96; i++) {
		d->AR_TAB[i] = d->AR_TAB[63];
		d->DR_TAB[i] = d->DR_TAB[63];
	}

	// Detune table.
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 32; j++) {
			double x;
			#if ((SIN_LBITS + SIN_HBITS - 21) < 0)
				x = (double)d->DT_DEF_TAB[i][j] * d->state.Frequence /
				    (double)(1 << (21 - SIN_LBITS - SIN_HBITS));
			#else
				x = (double)d->DT_DEF_TAB[i][j] * d->state.Frequence *
				    (double)(1 << (SIN_LBITS + SIN_HBITS - 21));
			#endif

			d->DT_TAB[i + 0][j] = (int) x;
			d->DT_TAB[i + 4][j] = (int) -x;
		}
	}

	// LFO step table.
	// TODO: Move to a static class variable?
	static const double LFO_BITS[8] = {
		3.98, 5.56, 6.02, 6.37, 6.88, 9.63, 48.1, 72.2
	};

	double j = (d->state.Rate * d->state.Inter_Step) / 0x4000;
	for (int i = 0; i < 8; i++) {
		d->LFO_INC_TAB[i] = (unsigned int) (LFO_BITS[i] * (double) (1 << (LFO_HBITS + LFO_LBITS)) / j);
	}


	// Reset the YM2612.
	reset();
	return 0;
}

void Ym2612::write_pan(int channel, int data)
{
	d->state.CHANNEL[channel].PANVolumeL = panlawtable[data & 0x7F];
	d->state.CHANNEL[channel].PANVolumeR = panlawtable[0x7F - (data & 0x7F)];
}

/**
 * Reset the YM2612.
 */
void Ym2612::reset(void)
{
//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG1,
//		"Starting reseting YM2612 ...");

	d->state.LFOcnt = 0;
	d->state.TimerA = 0;
	d->state.TimerAL = 0;
	d->state.TimerAcnt = 0;
	d->state.TimerB = 0;
	d->state.TimerBL = 0;
	d->state.TimerBcnt = 0;
	d->state.DAC = 0;
	d->state.DACdata = 0;

	d->state.status = 0;

	d->state.OPNAadr = 0;
	d->state.OPNBadr = 0;
	d->state.Inter_Cnt = 0;

	for (int i = 0; i < 6; i++) {
		d->state.CHANNEL[i].Old_OUTd = 0;
		d->state.CHANNEL[i].OUTd = 0;
		d->state.CHANNEL[i].LEFT = 0xFFFFFFFF;
		d->state.CHANNEL[i].RIGHT = 0xFFFFFFFF;
		d->state.CHANNEL[i].ALGO = 0;;
		d->state.CHANNEL[i].FB = 31;
		d->state.CHANNEL[i].FMS = 0;
		d->state.CHANNEL[i].AMS = 0;


		d->state.CHANNEL[i].PANVolumeL = 46340;
		d->state.CHANNEL[i].PANVolumeR = 46340;

		for (int j = 0; j < 4; j++) {
			d->state.CHANNEL[i].S0_OUT[j] = 0;
			d->state.CHANNEL[i].FNUM[j] = 0;
			d->state.CHANNEL[i].FOCT[j] = 0;
			d->state.CHANNEL[i].KC[j] = 0;

			d->state.CHANNEL[i]._SLOT[j].Fcnt = 0;
			d->state.CHANNEL[i]._SLOT[j].Finc = 0;
			d->state.CHANNEL[i]._SLOT[j].Ecnt = d->ENV_END;	// Put it at the end of Decay phase...
			d->state.CHANNEL[i]._SLOT[j].Einc = 0;
			d->state.CHANNEL[i]._SLOT[j].Ecmp = 0;
			d->state.CHANNEL[i]._SLOT[j].Ecurp = d->RELEASE;

			d->state.CHANNEL[i]._SLOT[j].ChgEnM = 0;
		}
	}

	// Initialize registers to 0xFF.
	memset(d->state.REG, 0xFF, sizeof(d->state.REG));

	// Initialize other registers to 0xC0.
	for (int i = 0xB6; i >= 0xB4; i--) {
		this->write(0, (uint8_t)i);
		this->write(2, (uint8_t)i);
		this->write(1, 0xC0);
		this->write(3, 0xC0);
	}

	// Initialize more registers to 0x00.
	for (int i = 0xB2; i >= 0x22; i--) {
		this->write(0, (uint8_t)i);
		this->write(2, (uint8_t)i);
		this->write(1, 0x00);
		this->write(3, 0x00);
	}

	// Initialize DAC to 0x80. (silence)
	this->write(0, 0x2A);
	this->write(1, 0x80);

//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG1,
//		"Finishing reseting YM2612 ...");
}

/**
 * Read the YM2612 status register.
 * @return YM2612 status register.
 */
uint8_t Ym2612::read(void) const
{
#if 0
	static int cnt = 0;

	if (cnt++ == 50)
	{
		cnt = 0;
		return YM2612.Status;
	}
	else return YM2612.Status | 0x80;
#endif

	/**
	 * READ DATA is the same for all four addresses.
	 * Format: [BUSY X X X X X OVRA OVRB]
	 * BUSY: If 1, YM2612 is busy and cannot accept new data.
	 * OVRA: If 1, timer A has overflowed.
	 * OVRB: If 1, timer B has overflowed.
	 */
	return (uint8_t)d->state.status;
}

/**
 * Write to a YM2612 register.
 * @param address Address.
 * @param data Data.
 * @return 0 on success; non-zero on error. (TODO: This isn't used by anything!)
 */
int Ym2612::write(unsigned int address, uint8_t data)
{
	/**
	 * Possible addresses:
	 * - 0: Bank 0 register number.
	 * - 1: Bank 0 data.
	 * - 2: Bank 1 register number.
	 * - 3: Bank 1 data.
	 */

	int reg_num;
	switch (address & 0x03) {
		case 0:
			d->state.OPNAadr = data;
			break;

		case 1:
			// Trivial optimization for DAC.
			if (d->state.OPNAadr == 0x2A) {
				d->state.DACdata = ((int)data - 0x80) << 7;
				return 0;
			}

			reg_num = d->state.OPNAadr & 0xF0;
			if (reg_num >= 0x30) {
				if (d->state.REG[0][d->state.OPNAadr] == data) {
					// Don't bother doing anything if the
					// register has the same value.
					// TODO: Is this correct?
					return 2;
				}
				d->state.REG[0][d->state.OPNAadr] = data;

				if (reg_num < 0xA0) {
					d->SLOT_SET(d->state.OPNAadr, data);
				} else {
					d->CHANNEL_SET(d->state.OPNAadr, data);
				}
			} else {
				// YM2612 control registers.
				d->state.REG[0][d->state.OPNAadr] = data;

				/* TODO: Reimplement GYM dumping support in LibGens.
				if ((GYM_Dumping) &&
				    ((d->state.OPNAadr == 0x22) ||
				     (d->state.OPNAadr == 0x27) ||
				     (d->state.OPNAadr == 0x28)))
				{
					gym_dump_update(1, (uint8_t)d->state.OPNAadr, data);
				}
				*/

				d->YM_SET(d->state.OPNAadr, data);
			}
			break;

		case 2:
			d->state.OPNBadr = data;
			break;

		case 3:
			reg_num = d->state.OPNBadr & 0xF0;

			if (reg_num >= 0x30) {
				if (d->state.REG[1][d->state.OPNBadr] == data) {
					// Don't bother doing anything if the
					// register has the same value.
					// TODO: Is this correct?
					return 2;
				}
				d->state.REG[1][d->state.OPNBadr] = data;

				/* TODO: Reimplement GYM dumping support in LibGens.
				if (GYM_Dumping)
					gym_dump_update(2, (uint8_t)d->state.OPNBadr, data);
				*/

				if (reg_num < 0xA0) {
					d->SLOT_SET(d->state.OPNBadr + 0x100, data);
				} else {
					d->CHANNEL_SET(d->state.OPNBadr + 0x100, data);
				}
			} else {
				// Invalid register.
				// TODO: Save the value anyway?
				return 1;
			}
			break;
	}

	return 0;
}

/**
 * Update the YM2612 audio output.
 * @param bufL Left audio buffer. (16-bit; int32_t is used for saturation.)
 * @param bufR Right audio buffer. (16-bit; int32_t is used for saturation.)
 * @param length Length to write.
 */
void Ym2612::update(int32_t *bufL, int32_t *bufR, int length)
{
//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG4,
//		"Starting generating sound...");

	// Mise à jour des pas des compteurs-fréquences s'ils ont été modifiés
 	if (d->state.CHANNEL[0]._SLOT[0].Finc == -1) {
		d->CALC_FINC_CH(&d->state.CHANNEL[0]);
	}
	if (d->state.CHANNEL[1]._SLOT[0].Finc == -1) {
		d->CALC_FINC_CH(&d->state.CHANNEL[1]);
	}
	if (d->state.CHANNEL[2]._SLOT[0].Finc == -1) {
		if (d->state.Mode & 0x40) {
			d->CALC_FINC_SL(&(d->state.CHANNEL[2]._SLOT[d->S0]),
				d->FINC_TAB[d->state.CHANNEL[2].FNUM[2]] >> (7 - d->state.CHANNEL[2].FOCT[2]),
				d->state.CHANNEL[2].KC[2]);
			d->CALC_FINC_SL(&(d->state.CHANNEL[2]._SLOT[d->S1]),
				d->FINC_TAB[d->state.CHANNEL[2].FNUM[3]] >> (7 - d->state.CHANNEL[2].FOCT[3]),
				d->state.CHANNEL[2].KC[3]);
			d->CALC_FINC_SL(&(d->state.CHANNEL[2]._SLOT[d->S2]),
				d->FINC_TAB[d->state.CHANNEL[2].FNUM[1]] >> (7 - d->state.CHANNEL[2].FOCT[1]),
				d->state.CHANNEL[2].KC[1]);
			d->CALC_FINC_SL(&(d->state.CHANNEL[2]._SLOT[d->S3]),
				d->FINC_TAB[d->state.CHANNEL[2].FNUM[0]] >> (7 - d->state.CHANNEL[2].FOCT[0]),
				d->state.CHANNEL[2].KC[0]);
		} else {
			d->CALC_FINC_CH(&d->state.CHANNEL[2]);
		}
	}
	if (d->state.CHANNEL[3]._SLOT[0].Finc == -1) {
		d->CALC_FINC_CH(&d->state.CHANNEL[3]);
	}
	if (d->state.CHANNEL[4]._SLOT[0].Finc == -1) {
		d->CALC_FINC_CH(&d->state.CHANNEL[4]);
	}
	if (d->state.CHANNEL[5]._SLOT[0].Finc == -1) {
		d->CALC_FINC_CH(&d->state.CHANNEL[5]);
	}

	/*
	d->CALC_FINC_CH(&d->state.CHANNEL[0]);
	d->CALC_FINC_CH(&d->state.CHANNEL[1]);
	if (d->state.Mode & 0x40) {
		d->CALC_FINC_SL(&(d->state.CHANNEL[2].SLOT[0]), d->FINC_TAB[d->state.CHANNEL[2].FNUM[2]] >> (7 - d->state.CHANNEL[2].FOCT[2]), YM2612.CHANNEL[2].KC[2]);
		d->CALC_FINC_SL(&(d->state.CHANNEL[2].SLOT[1]), d->FINC_TAB[d->state.CHANNEL[2].FNUM[3]] >> (7 - d->state.CHANNEL[2].FOCT[3]), YM2612.CHANNEL[2].KC[3]);
		d->CALC_FINC_SL(&(d->state.CHANNEL[2].SLOT[2]), d->FINC_TAB[d->state.CHANNEL[2].FNUM[1]] >> (7 - d->state.CHANNEL[2].FOCT[1]), YM2612.CHANNEL[2].KC[1]);
		d->CALC_FINC_SL(&(d->state.CHANNEL[2].SLOT[3]), d->FINC_TAB[d->state.CHANNEL[2].FNUM[0]] >> (7 - d->state.CHANNEL[2].FOCT[0]), d->state.CHANNEL[2].KC[0]);
	} else {
		d->CALC_FINC_CH(&d->state.CHANNEL[2]);
	}
	d->CALC_FINC_CH(&d->state.CHANNEL[3]);
	d->CALC_FINC_CH(&d->state.CHANNEL[4]);
	d->CALC_FINC_CH(&d->state.CHANNEL[5]);
	*/

	// Determine the algorithm type.
	int algo_type;
	if (d->state.Inter_Step & 0x04000) {
		algo_type = 0;
	} else {
		algo_type = 16;
	}

	if (d->state.LFOinc) {
		// Precalculate LFO wav

		for (int i = 0; i < length; i++) {
			int j = ((d->state.LFOcnt += d->state.LFOinc) >> LFO_LBITS) & d->LFO_MASK;

			d->LFO_ENV_UP[i] = d->LFO_ENV_TAB[j];
			d->LFO_FREQ_UP[i] = d->LFO_FREQ_TAB[j];

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG4,
//				"LFO_ENV_UP[%d] = %d   LFO_FREQ_UP[%d] = %d",
//				i, d->LFO_ENV_UP[i], i, d->LFO_FREQ_UP[i]);
		}

		algo_type |= 8;
	}

	d->Update_Chan((d->state.CHANNEL[0].ALGO + algo_type), &(d->state.CHANNEL[0]), bufL, bufR, length);
	d->Update_Chan((d->state.CHANNEL[1].ALGO + algo_type), &(d->state.CHANNEL[1]), bufL, bufR, length);
	d->Update_Chan((d->state.CHANNEL[2].ALGO + algo_type), &(d->state.CHANNEL[2]), bufL, bufR, length);
	d->Update_Chan((d->state.CHANNEL[3].ALGO + algo_type), &(d->state.CHANNEL[3]), bufL, bufR, length);
	d->Update_Chan((d->state.CHANNEL[4].ALGO + algo_type), &(d->state.CHANNEL[4]), bufL, bufR, length);
	if (!(d->state.DAC)) {
		// Update channel 6 only if DAC is disabled.
		d->Update_Chan((d->state.CHANNEL[5].ALGO + algo_type), &(d->state.CHANNEL[5]), bufL, bufR, length);
	}

	d->state.Inter_Cnt = d->int_cnt;

//	LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG4,
//		"Finishing generating sound...");
}

/**
 * Update the YM2612 DAC output and timers.
 * @param bufL Left audio buffer. (16-bit; int32_t is used for saturation.)
 * @param bufR Right audio buffer. (16-bit; int32_t is used for saturation.)
 * @param length Length of the output buffer.
 */
void Ym2612::updateDacAndTimers(int32_t *bufL, int32_t *bufR, int length)
{
	// Update DAC.
	if (d->state.DAC && d->state.DACdata && m_dacEnabled) {
		for (int i = 0; i < length; i++) {
			bufL[i] += (d->state.DACdata & d->state.CHANNEL[5].LEFT);
			bufR[i] += (d->state.DACdata & d->state.CHANNEL[5].RIGHT);
		}
	}

	// Update timers.
	int i = d->state.TimerBase * length;

	if (d->state.Mode & 1) {
		// Timer A is ON.
		//if ((YM2612.TimerAcnt -= 14073) <= 0)           // 13879=NTSC (old: 14475=NTSC  14586=PAL)
		if ((d->state.TimerAcnt -= i) <= 0) {
			d->state.status |= (d->state.Mode & 0x04) >> 2;
			d->state.TimerAcnt += d->state.TimerAL;

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG1,
//				"Counter A overflow");

			if (d->state.Mode & 0x80) {
				d->CSM_Key_Control();
			}
		}
	}

	if (d->state.Mode & 2) {
		// Timer B is ON.
		//if ((d->state.TimerBcnt -= 14073) <= 0)           // 13879=NTSC (old: 14475=NTSC  14586=PAL)
		if ((d->state.TimerBcnt -= i) <= 0) {
			d->state.status |= (d->state.Mode & 0x08) >> 2;
			d->state.TimerBcnt += d->state.TimerBL;

//			LOG_MSG(ym2612, LOG_MSG_LEVEL_DEBUG1,
//				"Counter B overflow");
		}
	}
}

/**
 * Update the YM2612 buffer.
 */
void Ym2612::specialUpdate(void)
{
	if (!(m_writeLen > 0 && m_enabled))
		return;

	// Update the sound buffer.
	update(m_bufPtrL, m_bufPtrR, m_writeLen);
	m_writeLen = 0;

#if 0
	// TODO: Don't use EmuContext here...
	int line_num = 1;
	EmuContext *context = EmuContext::Instance();
	if (context != nullptr)
		line_num = (context->m_vdp->VDP_Lines.currentLine + 1);

	// Determine the new starting position.
	int writePos = SoundMgr::GetWritePos(line_num);

	// Update the PSG buffer pointers.
	m_bufPtrL = &SoundMgr::ms_SegBufL[writePos];
	m_bufPtrR = &SoundMgr::ms_SegBufR[writePos];
#endif
}

/**
 * Get the value of a register.
 * @param regID Register ID.
 * @return Value of the register.
 */
int Ym2612::getReg(int regID) const
{
	if (regID < 0 || regID >= 0x200)
		return -1;

	return d->state.REG[(regID >> 8) & 1][regID & 0xFF];
}

/**
 * Reset the YM2612 buffer pointers.
 */
void Ym2612::resetBufferPtrs(int32_t *bufPtrL, int32_t *bufPtrR)
{
	m_bufPtrL = bufPtrL;
	m_bufPtrR = bufPtrR;
}

/* end */

}

