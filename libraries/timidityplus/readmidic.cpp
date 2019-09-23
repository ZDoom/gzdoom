/*
TiMidity++ -- MIDI to WAVE converter and player
Copyright (C) 1999-2009 Masanao Izumo <iz@onicos.co.jp>
Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <stdlib.h>

#include "timidity.h"
#include "instrum.h"
#include "playmidi.h"

namespace TimidityPlus
{


inline void SETMIDIEVENT(MidiEvent &e, int32_t /*time, not needed anymore*/, uint32_t t, uint32_t ch, uint32_t pa, uint32_t pb)
{
	(e).type = (t);
	(e).channel = (uint8_t)(ch);
	(e).a = (uint8_t)(pa);
	(e).b = (uint8_t)(pb);
}

#define MERGE_CHANNEL_PORT(ch) ((int)(ch) | (midi_port_number << 4))
#define MERGE_CHANNEL_PORT2(ch, port) ((int)(ch) | ((int)port << 4))

/* Map XG types onto GS types.  XG should eventually have its own tables */
static int set_xg_reverb_type(int msb, int lsb)
{
	int type = 4;

	if ((msb == 0x00) ||
		(msb >= 0x05 && msb <= 0x0F) ||
		(msb >= 0x14))			/* NO EFFECT */
	{
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"XG Set Reverb Type (NO EFFECT %d %d)", msb, lsb);
		return -1;
	}

	switch (msb)
	{
	case 0x01:
		type = 3;			/* Hall 1 */
		break;
	case 0x02:
		type = 0;			/* Room 1 */
		break;
	case 0x03:
		type = 3;			/* Stage 1 -> Hall 1 */
		break;
	case 0x04:
		type = 5;			/* Plate */
		break;
	default:
		type = 4;			/* unsupported -> Hall 2 */
		break;
	}
	if (lsb == 0x01)
	{
		switch (msb)
		{
		case 0x01:
			type = 4;			/* Hall 2 */
			break;
		case 0x02:
			type = 1;			/* Room 2 */
			break;
		case 0x03:
			type = 4;			/* Stage 2 -> Hall 2 */
			break;
		default:
			break;
		}
	}
	if (lsb == 0x02 && msb == 0x02)
		type = 2;				/* Room 3 */

	//ctl_cmsg(CMSG_INFO,VERB_NOISY,"XG Set Reverb Type (%d)", type);
	return type;
}


/* Map XG types onto GS types.  XG should eventually have its own tables */
static int set_xg_chorus_type(int msb, int lsb)
{
	int type = 2;

	if ((msb >= 0x00 && msb <= 0x40) ||
		(msb >= 0x45 && msb <= 0x47) ||
		(msb >= 0x49))			/* NO EFFECT */
	{
		//ctl_cmsg(CMSG_INFO,VERB_NOISY,"XG Set Chorus Type (NO EFFECT %d %d)", msb, lsb);
		return -1;
	}

	switch (msb)
	{
	case 0x41:
		type = 0;			/* Chorus 1 */
		break;
	case 0x42:
		type = 0;			/* Celeste 1 -> Chorus 1 */
		break;
	case 0x43:
		type = 5;
		break;
	default:
		type = 2;			/* unsupported -> Chorus 3 */
		break;
	}
	if (lsb == 0x01)
	{
		switch (msb)
		{
		case 0x41:
			type = 1;			/* Chorus 2 */
			break;
		case 0x42:
			type = 1;			/* Celeste 2 -> Chorus 2 */
			break;
		default:
			break;
		}
	}
	else if (lsb == 0x02)
	{
		switch (msb)
		{
		case 0x41:
			type = 2;			/* Chorus 3 */
			break;
		case 0x42:
			type = 2;			/* Celeste 3 -> Chorus 3 */
			break;
		default:
			break;
		}
	}
	else if (lsb == 0x08)
	{
		switch (msb)
		{
		case 0x41:
			type = 3;			/* Chorus 4 */
			break;
		case 0x42:
			type = 3;			/* Celeste 4 -> Chorus 4 */
			break;
		default:
			break;
		}
	}

	//ctl_cmsg(CMSG_INFO,VERB_NOISY,"XG Set Chorus Type (%d)", type);
	return type;
}

static int block_to_part(int block, int port)
{
	int p;
	p = block & 0x0F;
	if (p == 0) { p = 9; }
	else if (p <= 9) { p--; }
	return MERGE_CHANNEL_PORT2(p, port);
}

static uint16_t gs_convert_master_vol(int vol)
{
	double v;

	if (vol >= 0x7f)
		return 0xffff;
	v = (double)vol * (0xffff / 127.0);
	if (v >= 0xffff)
		return 0xffff;
	return (uint16_t)v;
}

static uint16_t gm_convert_master_vol(uint16_t v1, uint16_t v2)
{
	return (((v1 & 0x7f) | ((v2 & 0x7f) << 7)) << 2) | 3;
}


/* XG SysEx parsing function by Eric A. Welsh
* Also handles GS patch+bank changes
*
* This function provides basic support for XG Bulk Dump and Parameter
* Change SysEx events
*/
int SysexConvert::parse_sysex_event_multi(const uint8_t *val, int32_t len, MidiEvent *evm, Instruments *instruments)
{
	int num_events = 0;				/* Number of events added */

	uint32_t channel_tt;
	int i, j;
	static uint8_t xg_reverb_type_msb = 0x01, xg_reverb_type_lsb = 0x00;
	static uint8_t xg_chorus_type_msb = 0x41, xg_chorus_type_lsb = 0x00;

	/* Effect 1 or Multi EQ */
	if (len >= 8 &&
		val[0] == 0x43 && /* Yamaha ID */
		val[2] == 0x4C && /* XG Model ID */
		((val[1] <  0x10 && val[5] == 0x02) ||	/* Bulk Dump*/
		(val[1] >= 0x10 && val[3] == 0x02)))	/* Parameter Change */
	{
		uint8_t addhigh, addmid, addlow;		/* Addresses */
		const uint8_t *body;				/* SysEx body */
		int ent, v;				/* Entry # of sub-event */
		const uint8_t *body_end;			/* End of SysEx body */

		if (val[1] < 0x10)	/* Bulk Dump */
		{
			addhigh = val[5];
			addmid = val[6];
			addlow = val[7];
			body = val + 8;
			body_end = val + len - 3;
		}
		else			/* Parameter Change */
		{
			addhigh = val[3];
			addmid = val[4];
			addlow = val[5];
			body = val + 6;
			body_end = val + len - 2;
		}

		/* set the SYSEX_XG_MSB info */
		SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_MSB, 0, addhigh, addmid);
		num_events++;

		for (ent = addlow; body <= body_end; body++, ent++) {
			if (addmid == 0x01) {	/* Effect 1 */
				switch (ent) {
				case 0x00:	/* Reverb Type MSB */
					xg_reverb_type_msb = *body;
#if 0	/* XG specific reverb is not supported yet, use GS instead */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
#endif
					break;

				case 0x01:	/* Reverb Type LSB */
					xg_reverb_type_lsb = *body;
#if 0	/* XG specific reverb is not supported yet, use GS instead */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
#else
					v = set_xg_reverb_type(xg_reverb_type_msb, xg_reverb_type_lsb);
					if (v >= 0) {
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, v, 0x05);
						num_events++;
					}
#endif
					break;

				case 0x0C:	/* Reverb Return */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x20:	/* Chorus Type MSB */
					xg_chorus_type_msb = *body;
#if 0	/* XG specific chorus is not supported yet, use GS instead */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
#endif
					break;

				case 0x21:	/* Chorus Type LSB */
					xg_chorus_type_lsb = *body;
#if 0	/* XG specific chorus is not supported yet, use GS instead */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
#else
					v = set_xg_chorus_type(xg_chorus_type_msb, xg_chorus_type_lsb);
					if (v >= 0) {
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, v, 0x0D);
						num_events++;
					}
#endif
					break;

				case 0x2C:	/* Chorus Return */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				default:
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;
				}
			}
			else if (addmid == 0x40) {	/* Multi EQ */
				switch (ent) {
				case 0x00:	/* EQ type */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x01:	/* EQ gain1 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x02:	/* EQ frequency1 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x03:	/* EQ Q1 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x04:	/* EQ shape1 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x05:	/* EQ gain2 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x06:	/* EQ frequency2 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x07:	/* EQ Q2 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x09:	/* EQ gain3 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x0A:	/* EQ frequency3 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x0B:	/* EQ Q3 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x0D:	/* EQ gain4 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x0E:	/* EQ frequency4 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x0F:	/* EQ Q4 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x11:	/* EQ gain5 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x12:	/* EQ frequency5 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x13:	/* EQ Q5 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				case 0x14:	/* EQ shape5 */
					SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
					num_events++;
					break;

				default:
					break;
				}
			}
		}
	}

	/* Effect 2 (Insertion Effects) */
	else if (len >= 8 &&
		val[0] == 0x43 && /* Yamaha ID */
		val[2] == 0x4C && /* XG Model ID */
		((val[1] <  0x10 && val[5] == 0x03) ||	/* Bulk Dump*/
		(val[1] >= 0x10 && val[3] == 0x03)))	/* Parameter Change */
	{
		uint8_t addhigh, addmid, addlow;		/* Addresses */
		const uint8_t *body;				/* SysEx body */
		int ent;				/* Entry # of sub-event */
		const uint8_t *body_end;			/* End of SysEx body */

		if (val[1] < 0x10)	/* Bulk Dump */
		{
			addhigh = val[5];
			addmid = val[6];
			addlow = val[7];
			body = val + 8;
			body_end = val + len - 3;
		}
		else			/* Parameter Change */
		{
			addhigh = val[3];
			addmid = val[4];
			addlow = val[5];
			body = val + 6;
			body_end = val + len - 2;
		}

		/* set the SYSEX_XG_MSB info */
		SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_MSB, 0, addhigh, addmid);
		num_events++;

		for (ent = addlow; body <= body_end; body++, ent++) {
			SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, 0, *body, ent);
			num_events++;
		}
	}

	/* XG Multi Part Data parameter change */
	else if (len >= 10 &&
		val[0] == 0x43 && /* Yamaha ID */
		val[2] == 0x4C && /* XG Model ID */
		((val[1] <  0x10 && val[5] == 0x08 &&	/* Bulk Dump */
		(val[4] == 0x29 || val[4] == 0x3F)) ||	/* Blocks 1 or 2 */
			(val[1] >= 0x10 && val[3] == 0x08)))	/* Parameter Change */
	{
		uint8_t addhigh, addmid, addlow;		/* Addresses */
		const uint8_t *body;				/* SysEx body */
		uint8_t p;				/* Channel part number [0..15] */
		int ent;				/* Entry # of sub-event */
		const uint8_t *body_end;			/* End of SysEx body */

		if (val[1] < 0x10)	/* Bulk Dump */
		{
			addhigh = val[5];
			addmid = val[6];
			addlow = val[7];
			body = val + 8;
			p = addmid;
			body_end = val + len - 3;
		}
		else			/* Parameter Change */
		{
			addhigh = val[3];
			addmid = val[4];
			addlow = val[5];
			body = val + 6;
			p = addmid;
			body_end = val + len - 2;
		}

		/* set the SYSEX_XG_MSB info */
		SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_MSB, p, addhigh, addmid);
		num_events++;

		for (ent = addlow; body <= body_end; body++, ent++) {
			switch (ent) {
			case 0x00:	/* Element Reserve */
						/*			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Element Reserve is not supported. (CH:%d VAL:%d)", p, *body); */
				break;

			case 0x01:	/* bank select MSB */
				SETMIDIEVENT(evm[num_events], 0, ME_TONE_BANK_MSB, p, *body, SYSEX_TAG);
				num_events++;
				break;

			case 0x02:	/* bank select LSB */
				SETMIDIEVENT(evm[num_events], 0, ME_TONE_BANK_LSB, p, *body, SYSEX_TAG);
				num_events++;
				break;

			case 0x03:	/* program number */
				SETMIDIEVENT(evm[num_events], 0, ME_PROGRAM, p, *body, SYSEX_TAG);
				num_events++;
				break;

			case 0x04:	/* Rcv CHANNEL */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, p, *body, 0x99);
				num_events++;
				break;

			case 0x05:	/* mono/poly mode */
				if (*body == 0) { SETMIDIEVENT(evm[num_events], 0, ME_MONO, p, 0, SYSEX_TAG); }
				else { SETMIDIEVENT(evm[num_events], 0, ME_POLY, p, 0, SYSEX_TAG); }
				num_events++;
				break;

			case 0x06:	/* Same Note Number Key On Assign */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, p, *body, ent);
				num_events++;
				break;

			case 0x07:	/* Part Mode */
				drum_setup_xg[*body] = p;
				SETMIDIEVENT(evm[num_events], 0, ME_DRUMPART, p, *body, SYSEX_TAG);
				num_events++;
				break;

			case 0x08:	/* note shift */
				SETMIDIEVENT(evm[num_events], 0, ME_KEYSHIFT, p, *body, SYSEX_TAG);
				num_events++;
				break;

			case 0x09:	/* Detune 1st bit */
				break;

			case 0x0A:	/* Detune 2nd bit */
				break;

			case 0x0B:	/* volume */
				SETMIDIEVENT(evm[num_events], 0, ME_MAINVOLUME, p, *body, SYSEX_TAG);
				num_events++;
				break;

			case 0x0C:	/* Velocity Sense Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, p, *body, 0x21);
				num_events++;
				break;

			case 0x0D:	/* Velocity Sense Offset */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, p, *body, 0x22);
				num_events++;
				break;

			case 0x0E:	/* pan */
				if (*body == 0) {
					SETMIDIEVENT(evm[num_events], 0, ME_RANDOM_PAN, p, 0, SYSEX_TAG);
				}
				else {
					SETMIDIEVENT(evm[num_events], 0, ME_PAN, p, *body, SYSEX_TAG);
				}
				num_events++;
				break;

			case 0x0F:	/* Note Limit Low */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x42);
				num_events++;
				break;

			case 0x10:	/* Note Limit High */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x43);
				num_events++;
				break;

			case 0x11:	/* Dry Level */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, p, *body, ent);
				num_events++;
				break;

			case 0x12:	/* chorus send */
				SETMIDIEVENT(evm[num_events], 0, ME_CHORUS_EFFECT, p, *body, SYSEX_TAG);
				num_events++;
				break;

			case 0x13:	/* reverb send */
				SETMIDIEVENT(evm[num_events], 0, ME_REVERB_EFFECT, p, *body, SYSEX_TAG);
				num_events++;
				break;

			case 0x14:	/* Variation Send */
				SETMIDIEVENT(evm[num_events], 0, ME_CELESTE_EFFECT, p, *body, SYSEX_TAG);
				num_events++;
				break;

			case 0x15:	/* Vibrato Rate */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x08, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x16:	/* Vibrato Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x09, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x17:	/* Vibrato Delay */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x0A, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x18:	/* Filter Cutoff Frequency */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x20, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x19:	/* Filter Resonance */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x21, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x1A:	/* EG Attack Time */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x63, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x1B:	/* EG Decay Time */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x64, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x1C:	/* EG Release Time */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x66, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x1D:	/* MW Pitch Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x16);
				num_events++;
				break;

			case 0x1E:	/* MW Filter Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x17);
				num_events++;
				break;

			case 0x1F:	/* MW Amplitude Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x18);
				num_events++;
				break;

			case 0x20:	/* MW LFO PMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x1A);
				num_events++;
				break;

			case 0x21:	/* MW LFO FMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x1B);
				num_events++;
				break;

			case 0x22:	/* MW LFO AMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x1C);
				num_events++;
				break;

			case 0x23:	/* bend pitch control */
				SETMIDIEVENT(evm[num_events], 0, ME_RPN_MSB, p, 0, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_RPN_LSB, p, 0, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, (*body - 0x40) & 0x7F, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x24:	/* Bend Filter Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x22);
				num_events++;
				break;

			case 0x25:	/* Bend Amplitude Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x23);
				num_events++;
				break;

			case 0x26:	/* Bend LFO PMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x25);
				num_events++;
				break;

			case 0x27:	/* Bend LFO FMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x26);
				num_events++;
				break;

			case 0x28:	/* Bend LFO AMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x27);
				num_events++;
				break;

			case 0x30:	/* Rcv Pitch Bend */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x48);
				num_events++;
				break;

			case 0x31:	/* Rcv Channel Pressure */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x49);
				num_events++;
				break;

			case 0x32:	/* Rcv Program Change */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4A);
				num_events++;
				break;

			case 0x33:	/* Rcv Control Change */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4B);
				num_events++;
				break;

			case 0x34:	/* Rcv Poly Pressure */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4C);
				num_events++;
				break;

			case 0x35:	/* Rcv Note Message */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4D);
				num_events++;
				break;

			case 0x36:	/* Rcv RPN */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4E);
				num_events++;
				break;

			case 0x37:	/* Rcv NRPN */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x4F);
				num_events++;
				break;

			case 0x38:	/* Rcv Modulation */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x50);
				num_events++;
				break;

			case 0x39:	/* Rcv Volume */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x51);
				num_events++;
				break;

			case 0x3A:	/* Rcv Pan */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x52);
				num_events++;
				break;

			case 0x3B:	/* Rcv Expression */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x53);
				num_events++;
				break;

			case 0x3C:	/* Rcv Hold1 */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x54);
				num_events++;
				break;

			case 0x3D:	/* Rcv Portamento */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x55);
				num_events++;
				break;

			case 0x3E:	/* Rcv Sostenuto */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x56);
				num_events++;
				break;

			case 0x3F:	/* Rcv Soft */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x57);
				num_events++;
				break;

			case 0x40:	/* Rcv Bank Select */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x58);
				num_events++;
				break;

			case 0x41:	/* scale tuning */
			case 0x42:
			case 0x43:
			case 0x44:
			case 0x45:
			case 0x46:
			case 0x47:
			case 0x48:
			case 0x49:
			case 0x4a:
			case 0x4b:
			case 0x4c:
				SETMIDIEVENT(evm[num_events], 0, ME_SCALE_TUNING, p, ent - 0x41, *body - 64);
				num_events++;
				break;

			case 0x4D:	/* CAT Pitch Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x00);
				num_events++;
				break;

			case 0x4E:	/* CAT Filter Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x01);
				num_events++;
				break;

			case 0x4F:	/* CAT Amplitude Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x02);
				num_events++;
				break;

			case 0x50:	/* CAT LFO PMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x04);
				num_events++;
				break;

			case 0x51:	/* CAT LFO FMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x05);
				num_events++;
				break;

			case 0x52:	/* CAT LFO AMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x06);
				num_events++;
				break;

			case 0x53:	/* PAT Pitch Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x0B);
				num_events++;
				break;

			case 0x54:	/* PAT Filter Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x0C);
				num_events++;
				break;

			case 0x55:	/* PAT Amplitude Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x0D);
				num_events++;
				break;

			case 0x56:	/* PAT LFO PMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x0F);
				num_events++;
				break;

			case 0x57:	/* PAT LFO FMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x10);
				num_events++;
				break;

			case 0x58:	/* PAT LFO AMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x11);
				num_events++;
				break;

			case 0x59:	/* AC1 Controller Number */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"AC1 Controller Number is not supported. (CH:%d VAL:%d)", p, *body);
				break;

			case 0x5A:	/* AC1 Pitch Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x2C);
				num_events++;
				break;

			case 0x5B:	/* AC1 Filter Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x2D);
				num_events++;
				break;

			case 0x5C:	/* AC1 Amplitude Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x2E);
				num_events++;
				break;

			case 0x5D:	/* AC1 LFO PMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x30);
				num_events++;
				break;

			case 0x5E:	/* AC1 LFO FMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x31);
				num_events++;
				break;

			case 0x5F:	/* AC1 LFO AMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x32);
				num_events++;
				break;

			case 0x60:	/* AC2 Controller Number */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"AC2 Controller Number is not supported. (CH:%d VAL:%d)", p, *body);
				break;

			case 0x61:	/* AC2 Pitch Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x37);
				num_events++;
				break;

			case 0x62:	/* AC2 Filter Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x38);
				num_events++;
				break;

			case 0x63:	/* AC2 Amplitude Control */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x39);
				num_events++;
				break;

			case 0x64:	/* AC2 LFO PMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x3B);
				num_events++;
				break;

			case 0x65:	/* AC2 LFO FMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x3C);
				num_events++;
				break;

			case 0x66:	/* AC2 LFO AMod Depth */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x3D);
				num_events++;
				break;

			case 0x67:	/* Portamento Switch */
				SETMIDIEVENT(evm[num_events], 0, ME_PORTAMENTO, p, *body, SYSEX_TAG);
				num_events++;

			case 0x68:	/* Portamento Time */
				SETMIDIEVENT(evm[num_events], 0, ME_PORTAMENTO_TIME_MSB, p, *body, SYSEX_TAG);
				num_events++;

			case 0x69:	/* Pitch EG Initial Level */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Pitch EG Initial Level is not supported. (CH:%d VAL:%d)", p, *body);
				break;

			case 0x6A:	/* Pitch EG Attack Time */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Pitch EG Attack Time is not supported. (CH:%d VAL:%d)", p, *body);
				break;

			case 0x6B:	/* Pitch EG Release Level */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Pitch EG Release Level is not supported. (CH:%d VAL:%d)", p, *body);
				break;

			case 0x6C:	/* Pitch EG Release Time */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Pitch EG Release Time is not supported. (CH:%d VAL:%d)", p, *body);
				break;

			case 0x6D:	/* Velocity Limit Low */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x44);
				num_events++;
				break;

			case 0x6E:	/* Velocity Limit High */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, p, *body, 0x45);
				num_events++;
				break;

			case 0x70:	/* Bend Pitch Low Control */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Bend Pitch Low Control is not supported. (CH:%d VAL:%d)", p, *body);
				break;

			case 0x71:	/* Filter EG Depth */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Filter EG Depth is not supported. (CH:%d VAL:%d)", p, *body);
				break;

			case 0x72:	/* EQ BASS */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x30, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x73:	/* EQ TREBLE */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x31, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x76:	/* EQ BASS frequency */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x34, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			case 0x77:	/* EQ TREBLE frequency */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, p, 0x35, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, p, *body, SYSEX_TAG);
				num_events += 3;
				break;

			default:
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported XG Bulk Dump SysEx. (ADDR:%02X %02X %02X VAL:%02X)", addhigh, addlow, ent, *body);
				continue;
				break;
			}
		}
	}

	/* XG Drum Setup */
	else if (len >= 10 &&
		val[0] == 0x43 && /* Yamaha ID */
		val[2] == 0x4C && /* XG Model ID */
		((val[1] <  0x10 && (val[5] & 0xF0) == 0x30) ||	/* Bulk Dump*/
		(val[1] >= 0x10 && (val[3] & 0xF0) == 0x30)))	/* Parameter Change */
	{
		uint8_t addhigh, addmid, addlow;		/* Addresses */
		const uint8_t *body;				/* SysEx body */
		uint8_t dp, note;				/* Channel part number [0..15] */
		int ent;				/* Entry # of sub-event */
		const uint8_t *body_end;			/* End of SysEx body */

		if (val[1] < 0x10)	/* Bulk Dump */
		{
			addhigh = val[5];
			addmid = val[6];
			addlow = val[7];
			body = val + 8;
			body_end = val + len - 3;
		}
		else			/* Parameter Change */
		{
			addhigh = val[3];
			addmid = val[4];
			addlow = val[5];
			body = val + 6;
			body_end = val + len - 2;
		}

		dp = drum_setup_xg[(addhigh & 0x0F) + 1];
		note = addmid;

		/* set the SYSEX_XG_MSB info */
		SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_MSB, dp, addhigh, addmid);
		num_events++;

		for (ent = addlow; body <= body_end; body++, ent++) {
			switch (ent) {
			case 0x00:	/* Pitch Coarse */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x18, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x01:	/* Pitch Fine */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x19, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x02:	/* Level */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1A, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x03:	/* Alternate Group */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Alternate Group is not supported. (CH:%d NOTE:%d VAL:%d)", dp, note, *body);
				break;
			case 0x04:	/* Pan */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1C, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x05:	/* Reverb Send */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1D, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x06:	/* Chorus Send */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1E, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x07:	/* Variation Send */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x1F, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x08:	/* Key Assign */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Key Assign is not supported. (CH:%d NOTE:%d VAL:%d)", dp, note, *body);
				break;
			case 0x09:	/* Rcv Note Off */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_MSB, dp, note, 0);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_SYSEX_LSB, dp, *body, 0x46);
				num_events += 2;
				break;
			case 0x0A:	/* Rcv Note On */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_MSB, dp, note, 0);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_SYSEX_LSB, dp, *body, 0x47);
				num_events += 2;
				break;
			case 0x0B:	/* Filter Cutoff Frequency */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x14, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x0C:	/* Filter Resonance */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x15, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x0D:	/* EG Attack */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x16, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x0E:	/* EG Decay1 */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, dp, *body, ent);
				num_events++;
				break;
			case 0x0F:	/* EG Decay2 */
				SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_XG_LSB, dp, *body, ent);
				num_events++;
				break;
			case 0x20:	/* EQ BASS */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x30, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x21:	/* EQ TREBLE */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x31, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x24:	/* EQ BASS frequency */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x34, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x25:	/* EQ TREBLE frequency */
				SETMIDIEVENT(evm[num_events], 0, ME_NRPN_MSB, dp, 0x35, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 1], 0, ME_NRPN_LSB, dp, note, SYSEX_TAG);
				SETMIDIEVENT(evm[num_events + 2], 0, ME_DATA_ENTRY_MSB, dp, *body, SYSEX_TAG);
				num_events += 3;
				break;
			case 0x50:	/* High Pass Filter Cutoff Frequency */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"High Pass Filter Cutoff Frequency is not supported. (CH:%d NOTE:%d VAL:%d)", dp, note, *body);
				break;
			case 0x60:	/* Velocity Pitch Sense */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Velocity Pitch Sense is not supported. (CH:%d NOTE:%d VAL:%d)", dp, note, *body);
				break;
			case 0x61:	/* Velocity LPF Cutoff Sense */
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Velocity LPF Cutoff Sense is not supported. (CH:%d NOTE:%d VAL:%d)", dp, note, *body);
				break;
			default:
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported XG Bulk Dump SysEx. (ADDR:%02X %02X %02X VAL:%02X)", addhigh, addmid, ent, *body);
				break;
			}
		}
	}

	/* parsing GS System Exclusive Message...
	*
	* val[4] == Parameter Address(High)
	* val[5] == Parameter Address(Middle)
	* val[6] == Parameter Address(Low)
	* val[7]... == Data...
	* val[last] == Checksum(== 128 - (sum of addresses&data bytes % 128))
	*/
	else if (len >= 9 &&
		val[0] == 0x41 && /* Roland ID */
		val[1] == 0x10 && /* Device ID */
		val[2] == 0x42 && /* GS Model ID */
		val[3] == 0x12) /* Data Set Command */
	{
		uint8_t p, dp, udn, gslen, port = 0;
		int i, addr, addr_h, addr_m, addr_l, checksum;
		p = block_to_part(val[5], midi_port_number);

		/* calculate checksum */
		checksum = 0;
		for (gslen = 9; gslen < len; gslen++)
			if (val[gslen] == 0xF7)
				break;
		for (i = 4; i<gslen - 1; i++) {
			checksum += val[i];
		}
		if (((128 - (checksum & 0x7F)) & 0x7F) != val[gslen - 1]) {
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"GS SysEx: Checksum Error.");
			return num_events;
		}

		/* drum channel */
		dp = rhythm_part[(val[5] & 0xF0) >> 4];

		/* calculate user drumset number */
		udn = (val[5] & 0xF0) >> 4;

		addr_h = val[4];
		addr_m = val[5];
		addr_l = val[6];
		if (addr_h == 0x50) {	/* for double module mode */
			port = 1;
			p = block_to_part(val[5], port);
			addr_h = 0x40;
		}
		else if (addr_h == 0x51) {
			port = 1;
			p = block_to_part(val[5], port);
			addr_h = 0x41;
		}
		addr = (((int32_t)addr_h) << 16 | ((int32_t)addr_m) << 8 | (int32_t)addr_l);

		switch (addr_h) {
		case 0x40:
			if ((addr & 0xFFF000) == 0x401000) {
				switch (addr & 0xFF) {
				case 0x00:	/* Tone Number */
					SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_MSB, p, val[7], SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_PROGRAM, p, val[8], SYSEX_TAG);
					num_events += 2;
					break;
				case 0x02:	/* Rx. Channel */
					if (val[7] == 0x10) {
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB,
							block_to_part(val[5],
								midi_port_number ^ port), 0x80, 0x45);
					}
					else {
						SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB,
							block_to_part(val[5],
								midi_port_number ^ port),
							MERGE_CHANNEL_PORT2(val[7],
								midi_port_number ^ port), 0x45);
					}
					num_events++;
					break;
				case 0x03:	/* Rx. Pitch Bend */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x48);
					num_events++;
					break;
				case 0x04:	/* Rx. Channel Pressure */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x49);
					num_events++;
					break;
				case 0x05:	/* Rx. Program Change */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4A);
					num_events++;
					break;
				case 0x06:	/* Rx. Control Change */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4B);
					num_events++;
					break;
				case 0x07:	/* Rx. Poly Pressure */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4C);
					num_events++;
					break;
				case 0x08:	/* Rx. Note Message */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4D);
					num_events++;
					break;
				case 0x09:	/* Rx. RPN */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4E);
					num_events++;
					break;
				case 0x0A:	/* Rx. NRPN */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x4F);
					num_events++;
					break;
				case 0x0B:	/* Rx. Modulation */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x50);
					num_events++;
					break;
				case 0x0C:	/* Rx. Volume */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x51);
					num_events++;
					break;
				case 0x0D:	/* Rx. Panpot */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x52);
					num_events++;
					break;
				case 0x0E:	/* Rx. Expression */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x53);
					num_events++;
					break;
				case 0x0F:	/* Rx. Hold1 */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x54);
					num_events++;
					break;
				case 0x10:	/* Rx. Portamento */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x55);
					num_events++;
					break;
				case 0x11:	/* Rx. Sostenuto */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x56);
					num_events++;
					break;
				case 0x12:	/* Rx. Soft */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x57);
					num_events++;
					break;
				case 0x13:	/* MONO/POLY Mode */
					if (val[7] == 0) { SETMIDIEVENT(evm[0], 0, ME_MONO, p, val[7], SYSEX_TAG); }
					else { SETMIDIEVENT(evm[0], 0, ME_POLY, p, val[7], SYSEX_TAG); }
					num_events++;
					break;
				case 0x14:	/* Assign Mode */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x24);
					num_events++;
					break;
				case 0x15:	/* Use for Rhythm Part */
					if (val[7]) {
						rhythm_part[val[7] - 1] = p;
					}
					break;
				case 0x16:	/* Pitch Key Shift (dummy. see parse_sysex_event()) */
					break;
				case 0x17:	/* Pitch Offset Fine */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x26);
					num_events++;
					break;
				case 0x19:	/* Part Level */
					SETMIDIEVENT(evm[0], 0, ME_MAINVOLUME, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x1A:	/* Velocity Sense Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x21);
					num_events++;
					break;
				case 0x1B:	/* Velocity Sense Offset */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x22);
					num_events++;
					break;
				case 0x1C:	/* Part Panpot */
					if (val[7] == 0) {
						SETMIDIEVENT(evm[0], 0, ME_RANDOM_PAN, p, 0, SYSEX_TAG);
					}
					else {
						SETMIDIEVENT(evm[0], 0, ME_PAN, p, val[7], SYSEX_TAG);
					}
					num_events++;
					break;
				case 0x1D:	/* Keyboard Range Low */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x42);
					num_events++;
					break;
				case 0x1E:	/* Keyboard Range High */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x43);
					num_events++;
					break;
				case 0x1F:	/* CC1 Controller Number */
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC1 Controller Number is not supported. (CH:%d VAL:%d)", p, val[7]);
					break;
				case 0x20:	/* CC2 Controller Number */
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"CC2 Controller Number is not supported. (CH:%d VAL:%d)", p, val[7]);
					break;
				case 0x21:	/* Chorus Send Level */
					SETMIDIEVENT(evm[0], 0, ME_CHORUS_EFFECT, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x22:	/* Reverb Send Level */
					SETMIDIEVENT(evm[0], 0, ME_REVERB_EFFECT, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x23:	/* Rx. Bank Select */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x58);
					num_events++;
					break;
				case 0x24:	/* Rx. Bank Select LSB */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x59);
					num_events++;
					break;
				case 0x2C:	/* Delay Send Level */
					SETMIDIEVENT(evm[0], 0, ME_CELESTE_EFFECT, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x2A:	/* Pitch Fine Tune */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x00, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					SETMIDIEVENT(evm[3], 0, ME_DATA_ENTRY_LSB, p, val[8], SYSEX_TAG);
					num_events += 4;
					break;
				case 0x30:	/* TONE MODIFY1: Vibrato Rate */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x08, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x31:	/* TONE MODIFY2: Vibrato Depth */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x09, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x32:	/* TONE MODIFY3: TVF Cutoff Freq */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x20, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x33:	/* TONE MODIFY4: TVF Resonance */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x21, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x34:	/* TONE MODIFY5: TVF&TVA Env.attack */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x63, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x35:	/* TONE MODIFY6: TVF&TVA Env.decay */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x64, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x36:	/* TONE MODIFY7: TVF&TVA Env.release */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x66, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x37:	/* TONE MODIFY8: Vibrato Delay */
					SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, p, 0x01, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, p, 0x0A, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, val[7], SYSEX_TAG);
					num_events += 3;
					break;
				case 0x40:	/* Scale Tuning */
					for (i = 0; i < 12; i++) {
						SETMIDIEVENT(evm[i],
							0, ME_SCALE_TUNING, p, i, val[i + 7] - 64);
					}
					num_events += 12;
					break;
				default:
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)", addr_h, addr_m, addr_l, val[7], val[8]);
					break;
				}
			}
			else if ((addr & 0xFFF000) == 0x402000) {
				switch (addr & 0xFF) {
				case 0x00:	/* MOD Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x16);
					num_events++;
					break;
				case 0x01:	/* MOD TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x17);
					num_events++;
					break;
				case 0x02:	/* MOD Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x18);
					num_events++;
					break;
				case 0x03:	/* MOD LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x19);
					num_events++;
					break;
				case 0x04:	/* MOD LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1A);
					num_events++;
					break;
				case 0x05:	/* MOD LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1B);
					num_events++;
					break;
				case 0x06:	/* MOD LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1C);
					num_events++;
					break;
				case 0x07:	/* MOD LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1D);
					num_events++;
					break;
				case 0x08:	/* MOD LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1E);
					num_events++;
					break;
				case 0x09:	/* MOD LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x1F);
					num_events++;
					break;
				case 0x0A:	/* MOD LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x20);
					num_events++;
					break;
				case 0x10:	/* !!!FIXME!!! Bend Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_RPN_MSB, p, 0, SYSEX_TAG);
					SETMIDIEVENT(evm[1], 0, ME_RPN_LSB, p, 0, SYSEX_TAG);
					SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, p, (val[7] - 0x40) & 0x7F, SYSEX_TAG);
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x21);
					num_events += 4;
					break;
				case 0x11:	/* Bend TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x22);
					num_events++;
					break;
				case 0x12:	/* Bend Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x23);
					num_events++;
					break;
				case 0x13:	/* Bend LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x24);
					num_events++;
					break;
				case 0x14:	/* Bend LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x25);
					num_events++;
					break;
				case 0x15:	/* Bend LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x26);
					num_events++;
					break;
				case 0x16:	/* Bend LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x27);
					num_events++;
					break;
				case 0x17:	/* Bend LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x28);
					num_events++;
					break;
				case 0x18:	/* Bend LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x29);
					num_events++;
					break;
				case 0x19:	/* Bend LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2A);
					num_events++;
					break;
				case 0x1A:	/* Bend LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2B);
					num_events++;
					break;
				case 0x20:	/* CAf Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x00);
					num_events++;
					break;
				case 0x21:	/* CAf TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x01);
					num_events++;
					break;
				case 0x22:	/* CAf Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x02);
					num_events++;
					break;
				case 0x23:	/* CAf LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x03);
					num_events++;
					break;
				case 0x24:	/* CAf LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x04);
					num_events++;
					break;
				case 0x25:	/* CAf LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x05);
					num_events++;
					break;
				case 0x26:	/* CAf LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x06);
					num_events++;
					break;
				case 0x27:	/* CAf LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x07);
					num_events++;
					break;
				case 0x28:	/* CAf LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x08);
					num_events++;
					break;
				case 0x29:	/* CAf LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x09);
					num_events++;
					break;
				case 0x2A:	/* CAf LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0A);
					num_events++;
					break;
				case 0x30:	/* PAf Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0B);
					num_events++;
					break;
				case 0x31:	/* PAf TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0C);
					num_events++;
					break;
				case 0x32:	/* PAf Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0D);
					num_events++;
					break;
				case 0x33:	/* PAf LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0E);
					num_events++;
					break;
				case 0x34:	/* PAf LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x0F);
					num_events++;
					break;
				case 0x35:	/* PAf LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x10);
					num_events++;
					break;
				case 0x36:	/* PAf LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x11);
					num_events++;
					break;
				case 0x37:	/* PAf LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x12);
					num_events++;
					break;
				case 0x38:	/* PAf LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x13);
					num_events++;
					break;
				case 0x39:	/* PAf LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x14);
					num_events++;
					break;
				case 0x3A:	/* PAf LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x15);
					num_events++;
					break;
				case 0x40:	/* CC1 Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2C);
					num_events++;
					break;
				case 0x41:	/* CC1 TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2D);
					num_events++;
					break;
				case 0x42:	/* CC1 Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2E);
					num_events++;
					break;
				case 0x43:	/* CC1 LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x2F);
					num_events++;
					break;
				case 0x44:	/* CC1 LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x30);
					num_events++;
					break;
				case 0x45:	/* CC1 LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x31);
					num_events++;
					break;
				case 0x46:	/* CC1 LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x32);
					num_events++;
					break;
				case 0x47:	/* CC1 LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x33);
					num_events++;
					break;
				case 0x48:	/* CC1 LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x34);
					num_events++;
					break;
				case 0x49:	/* CC1 LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x35);
					num_events++;
					break;
				case 0x4A:	/* CC1 LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x36);
					num_events++;
					break;
				case 0x50:	/* CC2 Pitch Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x37);
					num_events++;
					break;
				case 0x51:	/* CC2 TVF Cutoff Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x38);
					num_events++;
					break;
				case 0x52:	/* CC2 Amplitude Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x39);
					num_events++;
					break;
				case 0x53:	/* CC2 LFO1 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3A);
					num_events++;
					break;
				case 0x54:	/* CC2 LFO1 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3B);
					num_events++;
					break;
				case 0x55:	/* CC2 LFO1 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3C);
					num_events++;
					break;
				case 0x56:	/* CC2 LFO1 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3D);
					num_events++;
					break;
				case 0x57:	/* CC2 LFO2 Rate Control */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3E);
					num_events++;
					break;
				case 0x58:	/* CC2 LFO2 Pitch Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x3F);
					num_events++;
					break;
				case 0x59:	/* CC2 LFO2 TVF Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x40);
					num_events++;
					break;
				case 0x5A:	/* CC2 LFO2 TVA Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_LSB, p, val[7], 0x41);
					num_events++;
					break;
				default:
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)", addr_h, addr_m, addr_l, val[7], val[8]);
					break;
				}
			}
			else if ((addr & 0xFFFF00) == 0x400100) {
				switch (addr & 0xFF) {
				case 0x30:	/* Reverb Macro */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x05);
					num_events++;
					break;
				case 0x31:	/* Reverb Character */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x06);
					num_events++;
					break;
				case 0x32:	/* Reverb Pre-LPF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x07);
					num_events++;
					break;
				case 0x33:	/* Reverb Level */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x08);
					num_events++;
					break;
				case 0x34:	/* Reverb Time */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x09);
					num_events++;
					break;
				case 0x35:	/* Reverb Delay Feedback */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0A);
					num_events++;
					break;
				case 0x36:	/* Unknown Reverb Parameter */
					break;
				case 0x37:	/* Reverb Predelay Time */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0C);
					num_events++;
					break;
				case 0x38:	/* Chorus Macro */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0D);
					num_events++;
					break;
				case 0x39:	/* Chorus Pre-LPF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0E);
					num_events++;
					break;
				case 0x3A:	/* Chorus Level */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x0F);
					num_events++;
					break;
				case 0x3B:	/* Chorus Feedback */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x10);
					num_events++;
					break;
				case 0x3C:	/* Chorus Delay */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x11);
					num_events++;
					break;
				case 0x3D:	/* Chorus Rate */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x12);
					num_events++;
					break;
				case 0x3E:	/* Chorus Depth */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x13);
					num_events++;
					break;
				case 0x3F:	/* Chorus Send Level to Reverb */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x14);
					num_events++;
					break;
				case 0x40:	/* Chorus Send Level to Delay */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x15);
					num_events++;
					break;
				case 0x50:	/* Delay Macro */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x16);
					num_events++;
					break;
				case 0x51:	/* Delay Pre-LPF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x17);
					num_events++;
					break;
				case 0x52:	/* Delay Time Center */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x18);
					num_events++;
					break;
				case 0x53:	/* Delay Time Ratio Left */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x19);
					num_events++;
					break;
				case 0x54:	/* Delay Time Ratio Right */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1A);
					num_events++;
					break;
				case 0x55:	/* Delay Level Center */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1B);
					num_events++;
					break;
				case 0x56:	/* Delay Level Left */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1C);
					num_events++;
					break;
				case 0x57:	/* Delay Level Right */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1D);
					num_events++;
					break;
				case 0x58:	/* Delay Level */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1E);
					num_events++;
					break;
				case 0x59:	/* Delay Feedback */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x1F);
					num_events++;
					break;
				case 0x5A:	/* Delay Send Level to Reverb */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x20);
					num_events++;
					break;
				default:
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)", addr_h, addr_m, addr_l, val[7], val[8]);
					break;
				}
			}
			else if ((addr & 0xFFFF00) == 0x400200) {
				switch (addr & 0xFF) {	/* EQ Parameter */
				case 0x00:	/* EQ LOW FREQ */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x01);
					num_events++;
					break;
				case 0x01:	/* EQ LOW GAIN */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x02);
					num_events++;
					break;
				case 0x02:	/* EQ HIGH FREQ */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x03);
					num_events++;
					break;
				case 0x03:	/* EQ HIGH GAIN */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x04);
					num_events++;
					break;
				default:
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)", addr_h, addr_m, addr_l, val[7], val[8]);
					break;
				}
			}
			else if ((addr & 0xFFFF00) == 0x400300) {
				switch (addr & 0xFF) {	/* Insertion Effect Parameter */
				case 0x00:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x27);
					SETMIDIEVENT(evm[1], 0, ME_SYSEX_GS_LSB, p, val[8], 0x28);
					num_events += 2;
					break;
				case 0x03:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x29);
					num_events++;
					break;
				case 0x04:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x2A);
					num_events++;
					break;
				case 0x05:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x2B);
					num_events++;
					break;
				case 0x06:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x2C);
					num_events++;
					break;
				case 0x07:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x2D);
					num_events++;
					break;
				case 0x08:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x2E);
					num_events++;
					break;
				case 0x09:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x2F);
					num_events++;
					break;
				case 0x0A:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x30);
					num_events++;
					break;
				case 0x0B:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x31);
					num_events++;
					break;
				case 0x0C:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x32);
					num_events++;
					break;
				case 0x0D:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x33);
					num_events++;
					break;
				case 0x0E:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x34);
					num_events++;
					break;
				case 0x0F:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x35);
					num_events++;
					break;
				case 0x10:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x36);
					num_events++;
					break;
				case 0x11:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x37);
					num_events++;
					break;
				case 0x12:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x38);
					num_events++;
					break;
				case 0x13:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x39);
					num_events++;
					break;
				case 0x14:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x3A);
					num_events++;
					break;
				case 0x15:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x3B);
					num_events++;
					break;
				case 0x16:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x3C);
					num_events++;
					break;
				case 0x17:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x3D);
					num_events++;
					break;
				case 0x18:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x3E);
					num_events++;
					break;
				case 0x19:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x3F);
					num_events++;
					break;
				case 0x1B:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x40);
					num_events++;
					break;
				case 0x1C:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x41);
					num_events++;
					break;
				case 0x1D:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x42);
					num_events++;
					break;
				case 0x1E:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x43);
					num_events++;
					break;
				case 0x1F:
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x44);
					num_events++;
					break;
				default:
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)", addr_h, addr_m, addr_l, val[7], val[8]);
					break;
				}
			}
			else if ((addr & 0xFFF000) == 0x404000) {
				switch (addr & 0xFF) {
				case 0x00:	/* TONE MAP NUMBER */
					SETMIDIEVENT(evm[0], 0, ME_TONE_BANK_LSB, p, val[7], SYSEX_TAG);
					num_events++;
					break;
				case 0x01:	/* TONE MAP-0 NUMBER */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x25);
					num_events++;
					break;
				case 0x20:	/* EQ ON/OFF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x00);
					num_events++;
					break;
				case 0x22:	/* EFX ON/OFF */
					SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB, p, val[7], 0x23);
					num_events++;
					break;
				default:
					//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)", addr_h, addr_m, addr_l, val[7], val[8]);
					break;
				}
			}
			break;
		case 0x41:
			switch (addr & 0xF00) {
			case 0x100:	/* Play Note Number */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_MSB, dp, val[6], 0);
				SETMIDIEVENT(evm[1], 0, ME_SYSEX_GS_LSB, dp, val[7], 0x47);
				num_events += 2;
				break;
			case 0x200:
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1A, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x400:
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1C, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x500:
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1D, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x600:
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1E, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x700:	/* Rx. Note Off */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_MSB, dp, val[6], 0);
				SETMIDIEVENT(evm[1], 0, ME_SYSEX_LSB, dp, val[7], 0x46);
				num_events += 2;
				break;
			case 0x800:	/* Rx. Note On */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_MSB, dp, val[6], 0);
				SETMIDIEVENT(evm[1], 0, ME_SYSEX_LSB, dp, val[7], 0x47);
				num_events += 2;
				break;
			case 0x900:
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1F, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			default:
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)", addr_h, addr_m, addr_l, val[7], val[8]);
				break;
			}
			break;
		case 0x21:	/* User Drumset */
			switch (addr & 0xF00) {
			case 0x100:	/* Play Note */
				instruments->get_userdrum(64 + udn, val[6])->play_note = val[7];
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_MSB, dp, val[6], 0);
				SETMIDIEVENT(evm[1], 0, ME_SYSEX_GS_LSB, dp, val[7], 0x47);
				num_events += 2;
				break;
			case 0x200:	/* Level */
				instruments->get_userdrum(64 + udn, val[6])->level = val[7];
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1A, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x300:	/* Assign Group */
				instruments->get_userdrum(64 + udn, val[6])->assign_group = val[7];
				if (val[7] != 0) { instruments->recompute_userdrum_altassign(udn + 64, val[7]); }
				break;
			case 0x400:	/* Panpot */
				instruments->get_userdrum(64 + udn, val[6])->pan = val[7];
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1C, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x500:	/* Reverb Send Level */
				instruments->get_userdrum(64 + udn, val[6])->reverb_send_level = val[7];
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1D, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x600:	/* Chorus Send Level */
				instruments->get_userdrum(64 + udn, val[6])->chorus_send_level = val[7];
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1E, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0x700:	/* Rx. Note Off */
				instruments->get_userdrum(64 + udn, val[6])->rx_note_off = val[7];
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_MSB, dp, val[6], 0);
				SETMIDIEVENT(evm[1], 0, ME_SYSEX_LSB, dp, val[7], 0x46);
				num_events += 2;
				break;
			case 0x800:	/* Rx. Note On */
				instruments->get_userdrum(64 + udn, val[6])->rx_note_on = val[7];
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_MSB, dp, val[6], 0);
				SETMIDIEVENT(evm[1], 0, ME_SYSEX_LSB, dp, val[7], 0x47);
				num_events += 2;
				break;
			case 0x900:	/* Delay Send Level */
				instruments->get_userdrum(64 + udn, val[6])->delay_send_level = val[7];
				SETMIDIEVENT(evm[0], 0, ME_NRPN_MSB, dp, 0x1F, SYSEX_TAG);
				SETMIDIEVENT(evm[1], 0, ME_NRPN_LSB, dp, val[6], SYSEX_TAG);
				SETMIDIEVENT(evm[2], 0, ME_DATA_ENTRY_MSB, dp, val[7], SYSEX_TAG);
				num_events += 3;
				break;
			case 0xA00:	/* Source Map */
				instruments->get_userdrum(64 + udn, val[6])->source_map = val[7];
				break;
			case 0xB00:	/* Source Prog */
				instruments->get_userdrum(64 + udn, val[6])->source_prog = val[7];
				break;
#if !defined(TIMIDITY_TOOLS)
			case 0xC00:	/* Source Note */
				instruments->get_userdrum(64 + udn, val[6])->source_note = val[7];
				break;
#endif
			default:
				//ctl_cmsg(CMSG_INFO,VERB_NOISY,"Unsupported GS SysEx. (ADDR:%02X %02X %02X VAL:%02X %02X)", addr_h, addr_m, addr_l, val[7], val[8]);
				break;
			}
			break;
		case 0x00:	/* System */
			switch (addr & 0xfff0) {
			case 0x0100:	/* Channel Msg Rx Port (A) */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB,
					block_to_part(addr & 0xf, 0), val[7], 0x46);
				num_events++;
				break;
			case 0x0110:	/* Channel Msg Rx Port (B) */
				SETMIDIEVENT(evm[0], 0, ME_SYSEX_GS_LSB,
					block_to_part(addr & 0xf, 1), val[7], 0x46);
				num_events++;
				break;
			default:
				/*	ctl_cmsg(CMSG_INFO,VERB_NOISY, "Unsupported GS SysEx. "
				"(ADDR:%02X %02X %02X VAL:%02X %02X)",
				addr_h, addr_m, addr_l, val[7], val[8]);*/
				break;
			}
			break;
		}
	}

	/* Non-RealTime / RealTime Universal SysEx messages
	* 0 0x7e(Non-RealTime) / 0x7f(RealTime)
	* 1 SysEx device ID.  Could be from 0x00 to 0x7f.
	*   0x7f means disregard device.
	* 2 Sub ID
	* ...
	* E 0xf7
	*/
	else if (len > 4 && val[0] >= 0x7e)
		switch (val[2]) {
		case 0x01:	/* Sample Dump header */
		case 0x02:	/* Sample Dump packet */
		case 0x03:	/* Dump Request */
		case 0x04:	/* Device Control */
			if (val[3] == 0x05) {	/* Global Parameter Control */
				if (val[7] == 0x01 && val[8] == 0x01) {	/* Reverb */
					for (i = 9; i < len && val[i] != 0xf7; i += 2) {
						switch (val[i]) {
						case 0x00:	/* Reverb Type */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, 0, val[i + 1], 0x60);
							num_events++;
							break;
						case 0x01:	/* Reverb Time */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x09);
							num_events++;
							break;
						}
					}
				}
				else if (val[7] == 0x01 && val[8] == 0x02) {	/* Chorus */
					for (i = 9; i < len && val[i] != 0xf7; i += 2) {
						switch (val[i]) {
						case 0x00:	/* Chorus Type */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, 0, val[i + 1], 0x61);
							num_events++;
							break;
						case 0x01:	/* Modulation Rate */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x12);
							num_events++;
							break;
						case 0x02:	/* Modulation Depth */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x13);
							num_events++;
							break;
						case 0x03:	/* Feedback */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x10);
							num_events++;
							break;
						case 0x04:	/* Send To Reverb */
							SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_GS_LSB, 0, val[i + 1], 0x14);
							num_events++;
							break;
						}
					}
				}
			}
			break;
		case 0x05:	/* Sample Dump extensions */
		case 0x06:	/* Inquiry Message */
		case 0x07:	/* File Dump */
			break;
		case 0x08:	/* MIDI Tuning Standard */
			switch (val[3]) {
			case 0x01:
				SETMIDIEVENT(evm[0], 0, ME_BULK_TUNING_DUMP, 0, val[4], 0);
				for (i = 0; i < 128; i++) {
					SETMIDIEVENT(evm[i * 2 + 1], 0, ME_BULK_TUNING_DUMP,
						1, i, val[i * 3 + 21]);
					SETMIDIEVENT(evm[i * 2 + 2], 0, ME_BULK_TUNING_DUMP,
						2, val[i * 3 + 22], val[i * 3 + 23]);
				}
				num_events += 257;
				break;
			case 0x02:
				SETMIDIEVENT(evm[0], 0, ME_SINGLE_NOTE_TUNING,
					0, val[4], 0);
				for (i = 0; i < val[5]; i++) {
					SETMIDIEVENT(evm[i * 2 + 1], 0, ME_SINGLE_NOTE_TUNING,
						1, val[i * 4 + 6], val[i * 4 + 7]);
					SETMIDIEVENT(evm[i * 2 + 2], 0, ME_SINGLE_NOTE_TUNING,
						2, val[i * 4 + 8], val[i * 4 + 9]);
				}
				num_events += val[5] * 2 + 1;
				break;
			case 0x0b:
				channel_tt = ((val[4] & 0x03) << 14 | val[5] << 7 | val[6])
					<< ((val[4] >> 2) * 16);
				if (val[1] == 0x7f) {
					SETMIDIEVENT(evm[0], 0, ME_MASTER_TEMPER_TYPE,
						0, val[7], (val[0] == 0x7f));
					num_events++;
				}
				else {
					for (i = j = 0; i < 32; i++)
						if (channel_tt & 1 << i) {
							SETMIDIEVENT(evm[j], 0, ME_TEMPER_TYPE,
								MERGE_CHANNEL_PORT(i),
								val[7], (val[0] == 0x7f));
							j++;
						}
					num_events += j;
				}
				break;
			case 0x0c:
				SETMIDIEVENT(evm[0], 0, ME_USER_TEMPER_ENTRY,
					0, val[4], val[21]);
				for (i = 0; i < val[21]; i++) {
					SETMIDIEVENT(evm[i * 5 + 1], 0, ME_USER_TEMPER_ENTRY,
						1, val[i * 10 + 22], val[i * 10 + 23]);
					SETMIDIEVENT(evm[i * 5 + 2], 0, ME_USER_TEMPER_ENTRY,
						2, val[i * 10 + 24], val[i * 10 + 25]);
					SETMIDIEVENT(evm[i * 5 + 3], 0, ME_USER_TEMPER_ENTRY,
						3, val[i * 10 + 26], val[i * 10 + 27]);
					SETMIDIEVENT(evm[i * 5 + 4], 0, ME_USER_TEMPER_ENTRY,
						4, val[i * 10 + 28], val[i * 10 + 29]);
					SETMIDIEVENT(evm[i * 5 + 5], 0, ME_USER_TEMPER_ENTRY,
						5, val[i * 10 + 30], val[i * 10 + 31]);
				}
				num_events += val[21] * 5 + 1;
				break;
			}
			break;
		case 0x09:	/* General MIDI Message */
			switch (val[3]) {
			case 0x01:	/* Channel Pressure */
				for (i = 5; i < len && val[i] != 0xf7; i += 2) {
					switch (val[i]) {
					case 0x00:	/* Pitch Control */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x00);
						num_events++;
						break;
					case 0x01:	/* Filter Cutoff Control */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x01);
						num_events++;
						break;
					case 0x02:	/* Amplitude Control */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x02);
						num_events++;
						break;
					case 0x03:	/* LFO Pitch Depth */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x04);
						num_events++;
						break;
					case 0x04:	/* LFO Filter Depth */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x05);
						num_events++;
						break;
					case 0x05:	/* LFO Amplitude Depth */
						SETMIDIEVENT(evm[num_events], 0, ME_SYSEX_LSB, val[4], val[i + 1], 0x06);
						num_events++;
						break;
					}
				}
				break;
			}
			break;
		case 0x7b:	/* End of File */
		case 0x7c:	/* Handshaking Message: Wait */
		case 0x7d:	/* Handshaking Message: Cancel */
		case 0x7e:	/* Handshaking Message: NAK */
		case 0x7f:	/* Handshaking Message: ACK */
			break;
		}

	return(num_events);
}


int SysexConvert::parse_sysex_event(const uint8_t *val, int32_t len, MidiEvent *ev, Instruments *instruments)
{
	uint16_t vol;

	if (len >= 10 &&
		val[0] == 0x41 && /* Roland ID */
		val[1] == 0x10 && /* Device ID */
		val[2] == 0x42 && /* GS Model ID */
		val[3] == 0x12)   /* Data Set Command */
	{
		/* Roland GS-Based Synthesizers.
		* val[4..6] is address, val[7..len-2] is body.
		*
		* GS     Channel part number
		* 0      10
		* 1-9    1-9
		* 10-15  11-16
		*/

		int32_t addr, checksum, i;		/* SysEx address */
		const uint8_t *body;		/* SysEx body */
		uint8_t p, gslen;		/* Channel part number [0..15] */

								/* check Checksum */
		checksum = 0;
		for (gslen = 9; gslen < len; gslen++)
			if (val[gslen] == 0xF7)
				break;
		for (i = 4; i < gslen - 1; i++) {
			checksum += val[i];
		}
		if (((128 - (checksum & 0x7F)) & 0x7F) != val[gslen - 1]) {
			return 0;
		}

		addr = (((int32_t)val[4]) << 16 |
			((int32_t)val[5]) << 8 |
			(int32_t)val[6]);
		body = val + 7;
		p = (uint8_t)((addr >> 8) & 0xF);
		if (p == 0)
			p = 9;
		else if (p <= 9)
			p--;
		p = MERGE_CHANNEL_PORT(p);

		if (val[4] == 0x50) {	/* for double module mode */
			p += 16;
			addr = (((int32_t)0x40) << 16 |
				((int32_t)val[5]) << 8 |
				(int32_t)val[6]);
		}
		else {	/* single module mode */
			addr = (((int32_t)val[4]) << 16 |
				((int32_t)val[5]) << 8 |
				(int32_t)val[6]);
		}

		if ((addr & 0xFFF0FF) == 0x401015) /* Rhythm Parts */
		{
			/* GS drum part check from Masaaki Koyanagi's patch (GS_Drum_Part_Check()) */
			/* Modified by Masanao Izumo */
			SETMIDIEVENT(*ev, 0, ME_DRUMPART, p, *body, SYSEX_TAG);
			return 1;
		}

		if ((addr & 0xFFF0FF) == 0x401016) /* Key Shift */
		{
			SETMIDIEVENT(*ev, 0, ME_KEYSHIFT, p, *body, SYSEX_TAG);
			return 1;
		}

		if (addr == 0x400000) /* Master Tune, not for SMF */
		{
			uint16_t tune = ((body[1] & 0xF) << 8) | ((body[2] & 0xF) << 4) | (body[3] & 0xF);

			if (tune < 0x18)
				tune = 0x18;
			else if (tune > 0x7E8)
				tune = 0x7E8;
			SETMIDIEVENT(*ev, 0, ME_MASTER_TUNING, 0, tune & 0xFF, (tune >> 8) & 0x7F);
			return 1;
		}

		if (addr == 0x400004) /* Master Volume */
		{
			vol = gs_convert_master_vol(*body);
			SETMIDIEVENT(*ev, 0, ME_MASTER_VOLUME,
				0, vol & 0xFF, (vol >> 8) & 0xFF);
			return 1;
		}

		if ((addr & 0xFFF0FF) == 0x401019) /* Volume on/off */
		{
#if 0
			SETMIDIEVENT(*ev, 0, ME_VOLUME_ONOFF, p, *body >= 64, SYSEX_TAG);
#endif
			return 0;
		}

		if ((addr & 0xFFF0FF) == 0x401002) /* Receive channel on/off */
		{
#if 0
			SETMIDIEVENT(*ev, 0, ME_RECEIVE_CHANNEL, (uint8_t)p, *body >= 64, SYSEX_TAG);
#endif
			return 0;
		}

		if (0x402000 <= addr && addr <= 0x402F5A) /* Controller Routing */
			return 0;

		if ((addr & 0xFFF0FF) == 0x401040) /* Alternate Scale Tunings */
			return 0;

		if ((addr & 0xFFFFF0) == 0x400130) /* Changing Effects */
		{
#if 0
			struct chorus_text_gs_t *chorus_text = &(reverb->chorus_status_gs.text);
			switch (addr & 0xF)
			{
			case 0x8: /* macro */
				memcpy(chorus_text->macro, body, 3);
				break;
			case 0x9: /* PRE-LPF */
				memcpy(chorus_text->pre_lpf, body, 3);
				break;
			case 0xa: /* level */
				memcpy(chorus_text->level, body, 3);
				break;
			case 0xb: /* feed back */
				memcpy(chorus_text->feed_back, body, 3);
				break;
			case 0xc: /* delay */
				memcpy(chorus_text->delay, body, 3);
				break;
			case 0xd: /* rate */
				memcpy(chorus_text->rate, body, 3);
				break;
			case 0xe: /* depth */
				memcpy(chorus_text->depth, body, 3);
				break;
			case 0xf: /* send level */
				memcpy(chorus_text->send_level, body, 3);
				break;
			default: break;
			}
#endif
			return 0;
		}

		if ((addr & 0xFFF0FF) == 0x401003) /* Rx Pitch-Bend */
			return 0;

		if (addr == 0x400110) /* Voice Reserve */
		{
#if 0
			if (len >= 25)
				memcpy(reverb->chorus_status_gs.text.voice_reserve, body, 18);
#endif
			return 0;
		}

		if (addr == 0x40007F ||	/* GS Reset */
			addr == 0x00007F)	/* SC-88 Single Module */
		{
			SETMIDIEVENT(*ev, 0, ME_RESET, 0, GS_SYSTEM_MODE, SYSEX_TAG);
			return 1;
		}
		return 0;
	}

	if (len > 9 &&
		val[0] == 0x41 && /* Roland ID */
		val[1] == 0x10 && /* Device ID */
		val[2] == 0x45 &&
		val[3] == 0x12 &&
		val[4] == 0x10 &&
		val[5] == 0x00 &&
		val[6] == 0x00)
	{
		return 0;
	}

	if (len > 9 &&                     /* GS lcd event. by T.Nogami*/
		val[0] == 0x41 && /* Roland ID */
		val[1] == 0x10 && /* Device ID */
		val[2] == 0x45 &&
		val[3] == 0x12 &&
		val[4] == 0x10 &&
		val[5] == 0x01 &&
		val[6] == 0x00)
	{
		return 0;
	}

	/* val[1] can have values other than 0x10 for the XG ON event, which
	* work on real XG hardware.  I have several midi that use 0x1f instead
	* of 0x10.  playmidi.h lists 0x10 - 0x13 as MU50/80/90/100.  I don't
	* know what real world Device Number 0x1f would correspond to, but the
	* XG spec says the entire 0x1n range is valid, and 0x1f works on real
	* hardware, so I have modified the check below to accept the entire
	* 0x1n range.
	*
	* I think there are/were some hacks somewhere in playmidi.c (?) to work
	* around non- 0x10 values, but this fixes the root of the problem, which
	* allows the server mode to handle XG initialization properly as well.
	*/
	if (len >= 8 &&
		val[0] == 0x43 &&
		(val[1] >= 0x10 && val[1] <= 0x1f) &&
		val[2] == 0x4C)
	{
		int addr = (val[3] << 16) | (val[4] << 8) | val[5];

		if (addr == 0x00007E)	/* XG SYSTEM ON */
		{
			SETMIDIEVENT(*ev, 0, ME_RESET, 0, XG_SYSTEM_MODE, SYSEX_TAG);
			return 1;
		}
		else if (addr == 0x000000 && len >= 12)	/* XG Master Tune */
		{
			uint16_t tune = ((val[7] & 0xF) << 8) | ((val[8] & 0xF) << 4) | (val[9] & 0xF);

			if (tune > 0x7FF)
				tune = 0x7FF;
			SETMIDIEVENT(*ev, 0, ME_MASTER_TUNING, 0, tune & 0xFF, (tune >> 8) & 0x7F);
			return 1;
		}
	}

	if (len >= 7 && val[0] == 0x7F && val[1] == 0x7F)
	{
		if (val[2] == 0x04 && val[3] == 0x03)	/* GM2 Master Fine Tune */
		{
			uint16_t tune = (val[4] & 0x7F) | (val[5] << 7) | 0x4000;

			SETMIDIEVENT(*ev, 0, ME_MASTER_TUNING, 0, tune & 0xFF, (tune >> 8) & 0x7F);
			return 1;
		}
		if (val[2] == 0x04 && val[3] == 0x04)	/* GM2 Master Coarse Tune */
		{
			uint8_t tune = val[5];

			if (tune < 0x28)
				tune = 0x28;
			else if (tune > 0x58)
				tune = 0x58;
			SETMIDIEVENT(*ev, 0, ME_MASTER_TUNING, 0, tune, 0x80);
			return 1;
		}
	}

	/* Non-RealTime / RealTime Universal SysEx messages
	* 0 0x7e(Non-RealTime) / 0x7f(RealTime)
	* 1 SysEx device ID.  Could be from 0x00 to 0x7f.
	*   0x7f means disregard device.
	* 2 Sub ID
	* ...
	* E 0xf7
	*/
	if (len > 4 && val[0] >= 0x7e)
		switch (val[2]) {
		case 0x01:	/* Sample Dump header */
		case 0x02:	/* Sample Dump packet */
		case 0x03:	/* Dump Request */
			break;
		case 0x04:	/* MIDI Time Code Setup/Device Control */
			switch (val[3]) {
			case 0x01:	/* Master Volume */
				vol = gm_convert_master_vol(val[4], val[5]);
				if (val[1] == 0x7f) {
					SETMIDIEVENT(*ev, 0, ME_MASTER_VOLUME, 0,
						vol & 0xff, vol >> 8 & 0xff);
				}
				else {
					SETMIDIEVENT(*ev, 0, ME_MAINVOLUME,
						MERGE_CHANNEL_PORT(val[1]),
						vol >> 8 & 0xff, 0);
				}
				return 1;
			}
			break;
		case 0x05:	/* Sample Dump extensions */
		case 0x06:	/* Inquiry Message */
		case 0x07:	/* File Dump */
			break;
		case 0x08:	/* MIDI Tuning Standard */
			switch (val[3]) {
			case 0x0a:
				SETMIDIEVENT(*ev, 0, ME_TEMPER_KEYSIG, 0,
					val[4] - 0x40 + val[5] * 16, (val[0] == 0x7f));
				return 1;
			}
			break;
		case 0x09:	/* General MIDI Message */
					/* GM System Enable/Disable */
			if (val[3] == 1) {
				ctl_cmsg(CMSG_INFO, VERB_DEBUG, "SysEx: GM System On");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, GM_SYSTEM_MODE, 0);
			}
			else if (val[3] == 3) {
				ctl_cmsg(CMSG_INFO, VERB_DEBUG, "SysEx: GM2 System On");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, GM2_SYSTEM_MODE, 0);
			}
			else {
				ctl_cmsg(CMSG_INFO, VERB_DEBUG, "SysEx: GM System Off");
				SETMIDIEVENT(*ev, 0, ME_RESET, 0, DEFAULT_SYSTEM_MODE, 0);
			}
			return 1;
		case 0x7b:	/* End of File */
		case 0x7c:	/* Handshaking Message: Wait */
		case 0x7d:	/* Handshaking Message: Cancel */
		case 0x7e:	/* Handshaking Message: NAK */
		case 0x7f:	/* Handshaking Message: ACK */
			break;
		}

	return 0;
}
}