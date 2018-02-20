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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "tables.h"
#include "reverb.h"
#include "aq.h"
#include "tarray.h"
#include <math.h>

namespace TimidityPlus
{

struct MidiEventList
{
	MidiEvent event;
	MidiEventList *next;
	MidiEventList *prev;
};

const int midi_port_number = 0;
const int play_system_mode = 0;

MBlockList tmpbuffer;
Player *gplayer;

#define MAX_MIDI_EVENT ((MAX_SAFE_MALLOC_SIZE / sizeof(MidiEvent)) - 1)
#define MARKER_START_CHAR	'('
#define MARKER_END_CHAR		')'

enum
{
    CHORUS_ST_NOT_OK = 0,
    CHORUS_ST_OK
};

static int readmidi_error_flag = 0;

/* Mingw gcc3 and Borland C hack */
/* If these are not NULL initialized cause Hang up */
/* why ?  I dont know. (Keishi Suenaga) */
static MidiEventList *evlist=NULL, *current_midi_point=NULL;
static int32_t event_count;
static MBlockList mempool;
static int current_read_track;
static int karaoke_format, karaoke_title_flag;
static MidiEvent timesig[256];


extern void free_readmidi(void);


static SysexConvert sc;

/* These would both fit into 32 bits, but they are often added in
   large multiples, so it's simpler to have two roomy ints */
static int32_t sample_increment, sample_correction; /*samples per MIDI delta-t*/

static inline void SETMIDIEVENT(MidiEvent &e, int32_t at, uint32_t t, uint32_t ch, uint32_t pa, uint32_t pb)
{
	(e).time = (at);
	(e).type = (t);
	(e).channel = (uint8_t)(ch);
	(e).a = (uint8_t)(pa);
	(e).b = (uint8_t)(pb);
}

void readmidi_add_event(MidiEvent *a_event);

static inline void MIDIEVENT(int32_t at, uint32_t t, uint32_t ch, uint32_t pa, uint32_t pb)
{ 
	MidiEvent event; 
	SETMIDIEVENT(event, at, t, ch, pa, pb);
	readmidi_add_event(&event); 
}




void Player::skip_to(int32_t until_time, MidiEvent *evt_start)
{
	int ch;

	current_event = NULL;

	if (current_sample > until_time)
		current_sample = 0;

	change_system_mode(DEFAULT_SYSTEM_MODE);
	reset_midi(0);

	buffered_count = 0;
	buffer_pointer = common_buffer;
	current_event = evt_start;
	current_play_tempo = 500000; /* 120 BPM */

	for (ch = 0; ch < MAX_CHANNELS; ch++)
		channel[ch].lasttime = current_sample;

}


int Player::start_midi(MidiEvent *eventlist, int32_t samples)
{
	int rc;
	int i, j;

	sample_count = samples;
	lost_notes = cut_notes = 0;
	check_eot_flag = 1;
	note_key_offset = key_adjust;
	midi_time_ratio = tempo_adjust;

	/* Reset key & speed each files */
	current_keysig = (opt_init_keysig == 8) ? 0 : opt_init_keysig;
	i = current_keysig + ((current_keysig < 8) ? 7 : -9), j = 0;
	while (i != 7)
		i += (i < 7) ? 5 : -7, j++;
	j += key_adjust, j -= int(floor(j / 12.0) * 12);
	current_freq_table = j;

	CLEAR_CHANNELMASK(channel_mute);
	if (temper_type_mute & 1)
		FILL_CHANNELMASK(channel_mute);


	reset_midi(0);

	rc = aq->flush(0);

	skip_to(0, eventlist);

	return RC_OK;
}

static MidiEventList *alloc_midi_event()
{
	return (MidiEventList *)new_segment(&mempool, sizeof(MidiEventList));
}

static int32_t readmidi_set_track(int trackno, int rewindp)
{
    current_read_track = trackno;
    if(karaoke_format == 1 && current_read_track == 2)
	karaoke_format = 2; /* Start karaoke lyric */
    else if(karaoke_format == 2 && current_read_track == 3)
	karaoke_format = 3; /* End karaoke lyric */

    if(evlist == NULL)
	return 0;
    if(rewindp)
	current_midi_point = evlist;
    else
    {
	/* find the last event in the list */
	while(current_midi_point->next != NULL)
	    current_midi_point = current_midi_point->next;
    }
    return current_midi_point->event.time;
}


static void readmidi_add_event(MidiEvent *a_event)
{
    MidiEventList *newev;
    int32_t at;

    if(event_count == MAX_MIDI_EVENT)
    {
	if(!readmidi_error_flag)
	{
	    readmidi_error_flag = 1;
	    ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
		      "Maxmum number of events is exceeded");
	}
	return;
    }
    event_count++;

    at = a_event->time;
    newev = alloc_midi_event();
    newev->event = *a_event;	/* assign by value!!! */
    if(at < 0)	/* for safety */
	at = newev->event.time = 0;

    if(at >= current_midi_point->event.time)
    {
	/* Forward scan */
	MidiEventList *next = current_midi_point->next;
	while (next && (next->event.time <= at))
	{
	    current_midi_point = next;
	    next = current_midi_point->next;
	}
	newev->prev = current_midi_point;
	newev->next = next;
	current_midi_point->next = newev;
	if (next)
	    next->prev = newev;
    }
    else
    {
	/* Backward scan -- symmetrical to the one above */
	MidiEventList *prev = current_midi_point->prev;
	while (prev && (prev->event.time > at)) {
	    current_midi_point = prev;
	    prev = current_midi_point->prev;
	}
	newev->prev = prev;
	newev->next = current_midi_point;
	current_midi_point->prev = newev;
	if (prev)
	    prev->next = newev;
    }
    current_midi_point = newev;
}

static void readmidi_add_ctl_event(int32_t at, int ch, int a, int b)
{
    MidiEvent ev;

    if(convert_midi_control_change(ch, a, b, &ev))
    {
	ev.time = at;
	readmidi_add_event(&ev);
    }
    else
	ctl_cmsg(CMSG_INFO, VERB_DEBUG, "(Control ch=%d %d: %d)", ch, a, b);
}

/* Computes how many (fractional) samples one MIDI delta-time unit contains */
static void compute_sample_increment(int32_t tempo, int32_t divisions)
{
  double a;
  a = (double) (tempo) * (double) (playback_rate) * (65536.0/1000000.0) /
    (double)(divisions);

  sample_correction = (int32_t)(a) & 0xFFFF;
  sample_increment = (int32_t)(a) >> 16;

  ctl_cmsg(CMSG_INFO, VERB_DEBUG, "Samples per delta-t: %d (correction %d)",
       sample_increment, sample_correction);
}


/* Free the linked event list from memory. */
static void free_midi_list(void)
{
    if(evlist != NULL)
    {
	reuse_mblock(&mempool);
	evlist = NULL;
    }
}

static void move_channels(int *chidx)
{
	int i, ch, maxch, newch;
	MidiEventList *e;
	
	for (i = 0; i < 256; i++)
		chidx[i] = -1;
	/* check channels */
	for (i = maxch = 0, e = evlist; i < event_count; i++, e = e->next)
		if (! GLOBAL_CHANNEL_EVENT_TYPE(e->event.type)) {
			if ((ch = e->event.channel) < REDUCE_CHANNELS)
				chidx[ch] = ch;
			if (maxch < ch)
				maxch = ch;
		}
	if (maxch >= REDUCE_CHANNELS)
		/* Move channel if enable */
		for (i = maxch = 0, e = evlist; i < event_count; i++, e = e->next)
			if (! GLOBAL_CHANNEL_EVENT_TYPE(e->event.type)) {
				if (chidx[ch = e->event.channel] != -1)
					ch = e->event.channel = chidx[ch];
				else {	/* -1 */
					if (ch >= MAX_CHANNELS) {
						newch = ch % REDUCE_CHANNELS;
						while (newch < ch && newch < MAX_CHANNELS) {
							if (chidx[newch] == -1) {
								ctl_cmsg(CMSG_INFO, VERB_VERBOSE,
										"channel %d => %d", ch, newch);
								ch = e->event.channel = chidx[ch] = newch;
								break;
							}
							newch += REDUCE_CHANNELS;
						}
					}
					if (chidx[ch] == -1) {
						if (ch < MAX_CHANNELS)
							chidx[ch] = ch;
						else {
							newch = ch % MAX_CHANNELS;
							ctl_cmsg(CMSG_WARNING, VERB_VERBOSE,
									"channel %d => %d (mixed)", ch, newch);
							ch = e->event.channel = chidx[ch] = newch;
						}
					}
				}
				if (maxch < ch)
					maxch = ch;
			}
	for (i = 0, e = evlist; i < event_count; i++, e = e->next)
		if (e->event.type == ME_SYSEX_GS_LSB) {
			if (e->event.b == 0x45 || e->event.b == 0x46)
				if (maxch < e->event.channel)
					maxch = e->event.channel;
		} else if (e->event.type == ME_SYSEX_XG_LSB) {
			if (e->event.b == 0x99)
				if (maxch < e->event.channel)
					maxch = e->event.channel;
		}
		auto current_file_info = gplayer->get_midi_file_info("", 0);
		current_file_info->max_channel = maxch;
}

/* Allocate an array of MidiEvents and fill it from the linked list of
   events, marking used instruments for loading. Convert event times to
   samples: handle tempo changes. Strip unnecessary events from the list.
   Free the linked list. */
MidiEvent *groom_list(int32_t divisions, int32_t *eventsp, int32_t *samplesp)
{
	auto current_file_info = gplayer->get_midi_file_info("", 0);
	MidiEvent *groomed_list, *lp;
	MidiEventList *meep;
	int32_t i, j, our_event_count, tempo, skip_this_event;
	int32_t sample_cum, samples_to_do, at, st, dt, counting_time;
	int ch, gch;
	uint8_t current_set[MAX_CHANNELS],
		warn_tonebank[128 + MAP_BANK_COUNT], warn_drumset[128 + MAP_BANK_COUNT];
	int8_t bank_lsb[MAX_CHANNELS], bank_msb[MAX_CHANNELS], mapID[MAX_CHANNELS];
	int current_program[MAX_CHANNELS];
	int chidx[256];
	int newbank, newprog;

	move_channels(chidx);

	COPY_CHANNELMASK(gplayer->drumchannels, current_file_info->drumchannels);
	COPY_CHANNELMASK(gplayer->drumchannel_mask, current_file_info->drumchannel_mask);

	/* Move drumchannels */
	for (ch = REDUCE_CHANNELS; ch < MAX_CHANNELS; ch++)
	{
		i = chidx[ch];
		if (i != -1 && i != ch && !IS_SET_CHANNELMASK(gplayer->drumchannel_mask, i))
		{
			if (IS_SET_CHANNELMASK(gplayer->drumchannels, ch))
				SET_CHANNELMASK(gplayer->drumchannels, i);
			else
				UNSET_CHANNELMASK(gplayer->drumchannels, i);
		}
	}

	memset(warn_tonebank, 0, sizeof(warn_tonebank));
	if (special_tonebank >= 0)
		newbank = special_tonebank;
	else
		newbank = default_tonebank;
	for (j = 0; j < MAX_CHANNELS; j++)
	{
		if (gplayer->ISDRUMCHANNEL(j))
			current_set[j] = 0;
		else
		{
			if (gplayer->instruments->toneBank(newbank) == NULL)
			{
				if (warn_tonebank[newbank] == 0)
				{
					ctl_cmsg(CMSG_WARNING, VERB_VERBOSE,
						"Tone bank %d is undefined", newbank);
					warn_tonebank[newbank] = 1;
				}
				newbank = 0;
			}
			current_set[j] = newbank;
		}
		bank_lsb[j] = bank_msb[j] = 0;
		if (play_system_mode == XG_SYSTEM_MODE && j % 16 == 9)
			bank_msb[j] = 127; /* Use MSB=127 for XG */
		current_program[j] = gplayer->instruments->defaultProgram(j);
	}

	memset(warn_drumset, 0, sizeof(warn_drumset));
	tempo = 500000;
	compute_sample_increment(tempo, divisions);

	/* This may allocate a bit more than we need */
	groomed_list = lp =
		(MidiEvent *)safe_malloc(sizeof(MidiEvent) * (event_count + 1));
	meep = evlist;

	our_event_count = 0;
	st = at = sample_cum = 0;
	counting_time = 2; /* We strip any silence before the first NOTE ON. */
	gplayer->change_system_mode(DEFAULT_SYSTEM_MODE);

	for (j = 0; j < MAX_CHANNELS; j++)
		mapID[j] = gplayer->get_default_mapID(j);

	for (i = 0; i < event_count; i++)
	{
		skip_this_event = 0;
		ch = meep->event.channel;
		gch = GLOBAL_CHANNEL_EVENT_TYPE(meep->event.type);
		if (!gch && ch >= MAX_CHANNELS) /* For safety */
			meep->event.channel = ch = ch % MAX_CHANNELS;

		switch (meep->event.type)
		{
		case ME_NONE:
			skip_this_event = 1;
			break;
		case ME_RESET:
			gplayer->change_system_mode(meep->event.a);
			//ctl_cmsg(CMSG_INFO,VERB_NOISY,"MIDI reset at %d sec", (int)((double)st / playback_rate + 0.5));
			for (j = 0; j < MAX_CHANNELS; j++)
			{
				if (play_system_mode == XG_SYSTEM_MODE && j % 16 == 9)
					mapID[j] = XG_DRUM_MAP;
				else
					mapID[j] = gplayer->get_default_mapID(j);
				if (gplayer->ISDRUMCHANNEL(j))
					current_set[j] = 0;
				else
				{
					if (special_tonebank >= 0)
						current_set[j] = special_tonebank;
					else
						current_set[j] = default_tonebank;

					if (gplayer->instruments->toneBank(current_set[j]) == NULL)
						current_set[j] = 0;
				}
				bank_lsb[j] = bank_msb[j] = 0;
				if (play_system_mode == XG_SYSTEM_MODE && j % 16 == 9)
					bank_msb[j] = 127; /* Use MSB=127 for XG */
				current_program[j] = gplayer->instruments->defaultProgram(j);
			}
			break;

		case ME_PROGRAM:
			if (gplayer->ISDRUMCHANNEL(ch))
				newbank = current_program[ch];
			else
				newbank = current_set[ch];
			newprog = meep->event.a;
			switch (play_system_mode) {
			case GS_SYSTEM_MODE:	/* GS */
				switch (bank_lsb[ch]) {
				case 0:		/* No change */
					break;
				case 1:
					ctl_cmsg(CMSG_INFO, VERB_DEBUG, "(GS ch=%d SC-55 MAP)", ch);
					mapID[ch] = (gplayer->ISDRUMCHANNEL(ch)) ? SC_55_DRUM_MAP
						: SC_55_TONE_MAP;
					break;
				case 2:
					ctl_cmsg(CMSG_INFO, VERB_DEBUG, "(GS ch=%d SC-88 MAP)", ch);
					mapID[ch] = (gplayer->ISDRUMCHANNEL(ch)) ? SC_88_DRUM_MAP
						: SC_88_TONE_MAP;
					break;
				case 3:
					ctl_cmsg(CMSG_INFO, VERB_DEBUG, "(GS ch=%d SC-88Pro MAP)", ch);
					mapID[ch] = (gplayer->ISDRUMCHANNEL(ch)) ? SC_88PRO_DRUM_MAP
						: SC_88PRO_TONE_MAP;
					break;
				case 4:
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(GS ch=%d SC-8820/SC-8850 MAP)", ch);
					mapID[ch] = (gplayer->ISDRUMCHANNEL(ch)) ? SC_8850_DRUM_MAP
						: SC_8850_TONE_MAP;
					break;
				default:
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(GS: ch=%d Strange bank LSB %d)",
						ch, bank_lsb[ch]);
					break;
				}
				newbank = bank_msb[ch];
				break;
			case XG_SYSTEM_MODE:	/* XG */
				switch (bank_msb[ch]) {
				case 0:		/* Normal */
					if (ch == 9 && bank_lsb[ch] == 127
						&& mapID[ch] == XG_DRUM_MAP)
						/* FIXME: Why this part is drum?  Is this correct? */
						ctl_cmsg(CMSG_WARNING, VERB_NORMAL, "Warning: XG bank 0/127 is found. It may be not correctly played.");
					else {
						ctl_cmsg(CMSG_INFO, VERB_DEBUG, "(XG ch=%d Normal voice)", ch);
						gplayer->midi_drumpart_change(ch, 0);
						mapID[ch] = XG_NORMAL_MAP;
					}
					break;
				case 64:	/* SFX voice */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG, "(XG ch=%d SFX voice)", ch);
					gplayer->midi_drumpart_change(ch, 0);
					mapID[ch] = XG_SFX64_MAP;
					break;
				case 126:	/* SFX kit */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG, "(XG ch=%d SFX kit)", ch);
					gplayer->midi_drumpart_change(ch, 1);
					mapID[ch] = XG_SFX126_MAP;
					break;
				case 127:	/* Drum kit */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(XG ch=%d Drum kit)", ch);
					gplayer->midi_drumpart_change(ch, 1);
					mapID[ch] = XG_DRUM_MAP;
					break;
				default:
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(XG: ch=%d Strange bank MSB %d)",
						ch, bank_msb[ch]);
					break;
				}
				newbank = bank_lsb[ch];
				break;
			case GM2_SYSTEM_MODE:	/* GM2 */
				ctl_cmsg(CMSG_INFO, VERB_DEBUG, "(GM2 ch=%d)", ch);
				if ((bank_msb[ch] & 0xfe) == 0x78)	/* 0x78/0x79 */
					gplayer->midi_drumpart_change(ch, bank_msb[ch] == 0x78);
				mapID[ch] = (gplayer->ISDRUMCHANNEL(ch)) ? GM2_DRUM_MAP
					: GM2_TONE_MAP;
				newbank = bank_lsb[ch];
				break;
			default:
				newbank = bank_msb[ch];
				break;
			}
			if (gplayer->ISDRUMCHANNEL(ch))
				current_set[ch] = newprog;
			else {
				if (special_tonebank >= 0)
					newbank = special_tonebank;
				if (current_program[ch] == SPECIAL_PROGRAM)
					skip_this_event = 1;
				current_set[ch] = newbank;
			}
			current_program[ch] = newprog;
			break;

		case ME_NOTEON:
			if (counting_time)
				counting_time = 1;
			if (gplayer->ISDRUMCHANNEL(ch))
			{
				newbank = current_set[ch];
				newprog = meep->event.a;
				gplayer->instruments->instrument_map(mapID[ch], &newbank, &newprog);

				if (!gplayer->instruments->drumSet(newbank)) /* Is this a defined drumset? */
				{
					if (warn_drumset[newbank] == 0)
					{
						ctl_cmsg(CMSG_WARNING, VERB_VERBOSE,
							"Drum set %d is undefined", newbank);
						warn_drumset[newbank] = 1;
					}
					newbank = 0;
				}

				/* Mark this instrument to be loaded */
				gplayer->instruments->mark_drumset(newbank, newprog);
			}
			else
			{
				if (current_program[ch] == SPECIAL_PROGRAM)
					break;
				newbank = current_set[ch];
				newprog = current_program[ch];
				gplayer->instruments->instrument_map(mapID[ch], &newbank, &newprog);
				if (gplayer->instruments->toneBank(newbank) == NULL)
				{
					if (warn_tonebank[newbank] == 0)
					{
						ctl_cmsg(CMSG_WARNING, VERB_VERBOSE,
							"Tone bank %d is undefined", newbank);
						warn_tonebank[newbank] = 1;
					}
					newbank = 0;
				}

				/* Mark this instrument to be loaded */
				gplayer->instruments->mark_instrument(newbank, newprog);
			}
			break;

		case ME_TONE_BANK_MSB:
			bank_msb[ch] = meep->event.a;
			break;

		case ME_TONE_BANK_LSB:
			bank_lsb[ch] = meep->event.a;
			break;

		case ME_CHORUS_TEXT:
		case ME_LYRIC:
		case ME_MARKER:
		case ME_INSERT_TEXT:
		case ME_TEXT:
		case ME_KARAOKE_LYRIC:
			if ((meep->event.a | meep->event.b) == 0)
				skip_this_event = 1;
			break;

		case ME_GSLCD:
			skip_this_event = 1;
			break;

		case ME_DRUMPART:
			gplayer->midi_drumpart_change(ch, meep->event.a);
			break;

		case ME_WRD:
			break;

		case ME_SHERRY:
			break;

		case ME_NOTE_STEP:
			if (counting_time == 2)
				skip_this_event = 1;
			break;
		}

		/* Recompute time in samples*/
		if ((dt = meep->event.time - at) && !counting_time)
		{
			samples_to_do = sample_increment * dt;
			sample_cum += sample_correction * dt;
			if (sample_cum & 0xFFFF0000)
			{
				samples_to_do += ((sample_cum >> 16) & 0xFFFF);
				sample_cum &= 0x0000FFFF;
			}
			st += samples_to_do;
			if (st < 0)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"Overflow the sample counter");
				free(groomed_list);
				return NULL;
			}
		}
		else if (counting_time == 1)
			counting_time = 0;

		if (meep->event.type == ME_TEMPO)
		{
			tempo = ch + meep->event.b * 256 + meep->event.a * 65536;
			compute_sample_increment(tempo, divisions);
		}

		if (!skip_this_event)
		{
			/* Add the event to the list */
			*lp = meep->event;
			lp->time = st;
			lp++;
			our_event_count++;
		}
		at = meep->event.time;
		meep = meep->next;
	}
	/* Add an End-of-Track event */
	lp->time = st;
	lp->type = ME_EOT;
	our_event_count++;
	free_midi_list();

	*eventsp = our_event_count;
	*samplesp = st;
	return groomed_list;
}


static void readmidi_read_init(void)
{
	static int first = 1;


    /* Put a do-nothing event first in the list for easier processing */
    evlist = current_midi_point = alloc_midi_event();
    evlist->event.time = 0;
    evlist->event.type = ME_NONE;
    evlist->event.channel = 0;
    evlist->event.a = 0;
    evlist->event.b = 0;
    evlist->prev = NULL;
    evlist->next = NULL;
    readmidi_error_flag = 0;
    event_count = 1;

    karaoke_format = 0;
}

static void insert_note_steps(void)
{
	MidiEventList *e;
	int32_t i, n, at, lasttime, meas, beat;
	uint8_t num = 0, denom = 1, a, b;
	
	e = evlist;
	for (i = n = 0; i < event_count - 1 && n < 256 - 1; i++, e = e->next)
		if (e->event.type == ME_TIMESIG && e->event.channel == 0) {
			if (n == 0 && e->event.time > 0) {	/* 4/4 is default */
				SETMIDIEVENT(timesig[n], 0, ME_TIMESIG, 0, 4, 4);
				n++;
			}
			if (n > 0 && e->event.a == timesig[n - 1].a
					&& e->event.b == timesig[n - 1].b)
				continue;	/* unchanged */
			if (n > 0 && e->event.time == timesig[n - 1].time)
				n--;	/* overwrite previous timesig */
			timesig[n++] = e->event;
		}
	if (n == 0) {
		SETMIDIEVENT(timesig[n], 0, ME_TIMESIG, 0, 4, 4);
		n++;
	}
	timesig[n] = timesig[n - 1];
	timesig[n].time = 0x7fffffff;	/* stopper */
	lasttime = e->event.time;
	readmidi_set_track(0, 1);
	at = n = meas = beat = 0;
	auto current_file_info = gplayer->get_midi_file_info("", 0);
	while (at < lasttime && ! readmidi_error_flag) {
		if (at >= timesig[n].time) {
			if (beat != 0)
				meas++, beat = 0;
			num = timesig[n].a, denom = timesig[n].b, n++;
		}
		a = (meas + 1) & 0xff;
		b = (((meas + 1) >> 8) & 0x0f) + ((beat + 1) << 4);
		MIDIEVENT(at, ME_NOTE_STEP, 0, a, b);
		if (++beat == num)
			meas++, beat = 0;
		at += current_file_info->divisions * 4 / denom;
	}
}


static void free_readmidi(void)
{
	reuse_mblock(&mempool);
	gplayer->instruments->free_userdrum();
	gplayer->instruments->free_userinst();
}

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
// From here: functions for loading a .mid file.
// Will not be needed when all is done!


/* Read variable-length number (7 bits per byte, MSB first) */
static int32_t getvl(struct timidity_file *tf)
{
	int32_t l;
	int c;

	l = 0;

	/* 1 */
	if ((c = tf_getc(tf)) == EOF)
		goto eof;
	if (!(c & 0x80)) return l | c;
	l = (l | (c & 0x7f)) << 7;

	/* 2 */
	if ((c = tf_getc(tf)) == EOF)
		goto eof;
	if (!(c & 0x80)) return l | c;
	l = (l | (c & 0x7f)) << 7;

	/* 3 */
	if ((c = tf_getc(tf)) == EOF)
		goto eof;
	if (!(c & 0x80)) return l | c;
	l = (l | (c & 0x7f)) << 7;

	/* 4 */
	if ((c = tf_getc(tf)) == EOF)
		goto eof;
	if (!(c & 0x80)) return l | c;

	/* Error */
	ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
		"%s: Illigal Variable-length quantity format.",
		tf->filename.c_str());
	return -2;

eof:
	return -1;
}

/* Print a string from the file, followed by a newline. Any non-ASCII
or unprintable characters will be converted to periods. */
static char *dumpstring(int type, int32_t len, const char *label, int allocp,
	struct timidity_file *tf)
{
	char *si, *so;
	int s_maxlen = SAFE_CONVERT_LENGTH(len);
	int llen, solen;

	si = (char *)new_segment(&tmpbuffer, len + 1);
	so = (char *)new_segment(&tmpbuffer, s_maxlen);

	if (len != tf_read(si, 1, len, tf))
	{
		reuse_mblock(&tmpbuffer);
		return NULL;
	}
	si[len] = '\0';

	auto current_file_info = gplayer->get_midi_file_info("", 0);
	if (type == 1 &&
		current_file_info->format == 1 &&
		(strncmp(si, "@K", 2) == 0))
		/* Karaoke string should be "@KMIDI KARAOKE FILE" */
		karaoke_format = 1;

	llen = (int)strlen(label);
	solen = (int)strlen(si);
	if (llen + solen >= MIN_MBLOCK_SIZE)
		si[MIN_MBLOCK_SIZE - llen - 1] = '\0';

	if (allocp)
	{
		so = safe_strdup(si);
		reuse_mblock(&tmpbuffer);
		return so;
	}
	reuse_mblock(&tmpbuffer);
	return NULL;
}

static int read_sysex_event(int32_t at, int me, int32_t len,
	struct timidity_file *tf)
{
	uint8_t *val;
	MidiEvent ev, evm[260]; /* maximum number of XG bulk dump events */
	int ne, i;

	if (len == 0)
		return 0;
	if (me != 0xF0)
	{
		skip(tf, len);
		return 0;
	}

	val = (uint8_t *)new_segment(&tmpbuffer, len);
	if (tf_read(val, 1, len, tf) != len)
	{
		reuse_mblock(&tmpbuffer);
		return -1;
	}
	if (sc.parse_sysex_event(val, len, &ev, gplayer->instruments))
	{
		ev.time = at;
		readmidi_add_event(&ev);
	}
	if ((ne = sc.parse_sysex_event_multi(val, len, evm, gplayer->instruments)))
	{
		for (i = 0; i < ne; i++) {
			evm[i].time = at;
			readmidi_add_event(&evm[i]);
		}
	}

	reuse_mblock(&tmpbuffer);

	return 0;
}

static char *fix_string(char *s)
{
	int i, j, w;
	char c;

	if (s == NULL)
		return NULL;
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		s++;

	/* s =~ tr/ \t\r\n/ /s; */
	w = 0;
	for (i = j = 0; (c = s[i]) != '\0'; i++)
	{
		if (c == '\t' || c == '\r' || c == '\n')
			c = ' ';
		if (w)
			w = (c == ' ');
		if (!w)
		{
			s[j++] = c;
			w = (c == ' ');
		}
	}

	/* s =~ s/ $//; */
	if (j > 0 && s[j - 1] == ' ')
		j--;

	s[j] = '\0';
	return s;
}

static void smf_time_signature(int32_t at, struct timidity_file *tf, int len)
{
	int n, d, c, b;

	/* Time Signature (nn dd cc bb)
	* [0]: numerator
	* [1]: denominator
	* [2]: number of MIDI clocks in a metronome click
	* [3]: number of notated 32nd-notes in a MIDI
	*      quarter-note (24 MIDI Clocks).
	*/

	if (len != 4)
	{
		ctl_cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid time signature");
		skip(tf, len);
		return;
	}

	n = tf_getc(tf);
	d = (1 << tf_getc(tf));
	c = tf_getc(tf);
	b = tf_getc(tf);

	if (n == 0 || d == 0)
	{
		ctl_cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid time signature");
		return;
	}

	MIDIEVENT(at, ME_TIMESIG, 0, n, d);
	MIDIEVENT(at, ME_TIMESIG, 1, c, b);
	ctl_cmsg(CMSG_INFO, VERB_NOISY,
		"Time signature: %d/%d %d clock %d q.n.", n, d, c, b);
	auto current_file_info = gplayer->get_midi_file_info("", 0);
	if (current_file_info->time_sig_n == -1)
	{
		current_file_info->time_sig_n = n;
		current_file_info->time_sig_d = d;
		current_file_info->time_sig_c = c;
		current_file_info->time_sig_b = b;
	}
}

static void smf_key_signature(int32_t at, struct timidity_file *tf, int len)
{
	int8_t sf, mi;
	/* Key Signature (sf mi)
	* sf = -7:  7 flats
	* sf = -1:  1 flat
	* sf = 0:   key of C
	* sf = 1:   1 sharp
	* sf = 7:   7 sharps
	* mi = 0:  major key
	* mi = 1:  minor key
	*/

	if (len != 2) {
		ctl_cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid key signature");
		skip(tf, len);
		return;
	}
	sf = tf_getc(tf);
	mi = tf_getc(tf);
	if (sf < -7 || sf > 7) {
		ctl_cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid key signature");
		return;
	}
	if (mi != 0 && mi != 1) {
		ctl_cmsg(CMSG_WARNING, VERB_VERBOSE, "Invalid key signature");
		return;
	}
	MIDIEVENT(at, ME_KEYSIG, 0, sf, mi);
	ctl_cmsg(CMSG_INFO, VERB_NOISY,
		"Key signature: %d %s %s", abs(sf),
		(sf < 0) ? "flat(s)" : "sharp(s)", (mi) ? "minor" : "major");
}

/* Read a SMF track */
static int read_smf_track(struct timidity_file *tf, int trackno, int rewindp)
{
	int32_t len, next_pos, pos;
	char tmp[4];
	int lastchan, laststatus;
	int me, type, a, b, c;
	int i;
	int32_t smf_at_time;
	int note_seen = (!opt_preserve_silence);

	smf_at_time = readmidi_set_track(trackno, rewindp);

	/* Check the formalities */
	if ((tf_read(tmp, 1, 4, tf) != 4) || (tf_read(&len, 4, 1, tf) != 1))
	{
		ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
			"%s: Can't read track header.", tf->filename.c_str());
		return -1;
	}
	len = BE_LONG(len);
	next_pos = tf_tell(tf) + len;
	if (strncmp(tmp, "MTrk", 4))
	{
		ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
			"%s: Corrupt MIDI file.", tf->filename.c_str());
		return -2;
	}

	lastchan = laststatus = 0;
	auto current_file_info = gplayer->get_midi_file_info("", 0);

	for (;;)
	{
		if (readmidi_error_flag)
			return -1;
		if ((len = getvl(tf)) < 0)
			return -1;
		smf_at_time += len;
		if ((i = tf_getc(tf)) == EOF)
		{
			return -1;
		}

		me = (uint8_t)i;
		if (me == 0xF0 || me == 0xF7) /* SysEx event */
		{
			if ((len = getvl(tf)) < 0)
				return -1;
			if ((i = read_sysex_event(smf_at_time, me, len, tf)) != 0)
				return i;
		}
		else if (me == 0xFF) /* Meta event */
		{
			type = tf_getc(tf);
			if ((len = getvl(tf)) < 0)
				return -1;
			if (type > 0 && type < 16)
			{
				static const char *label[] =
				{
					"Text event: ", "Text: ", "Copyright: ", "Track name: ",
					"Instrument: ", "Lyric: ", "Marker: ", "Cue point: "
				};

				if (type == 3 && /* Sequence or Track Name */
					(current_file_info->format == 0 ||
					(current_file_info->format == 1 &&
						current_read_track == 0)))
				{
					if (current_file_info->seq_name == NULL) {
						char *name = dumpstring(3, len, "Sequence: ", 1, tf);
						current_file_info->seq_name = safe_strdup(fix_string(name));
						free(name);
					}
					else
						dumpstring(3, len, "Sequence: ", 0, tf);
				}
				else if (type == 1 &&
					current_file_info->first_text == NULL &&
					(current_file_info->format == 0 ||
					(current_file_info->format == 1 &&
						current_read_track == 0))) {
					char *name = dumpstring(1, len, "Text: ", 1, tf);
					current_file_info->first_text = safe_strdup(fix_string(name));
					free(name);
				}
				else
					dumpstring(type, len, label[(type>7) ? 0 : type], 0, tf);
			}
			else
			{
				switch (type)
				{
				case 0x00:
					if (len == 2)
					{
						a = tf_getc(tf);
						b = tf_getc(tf);
						ctl_cmsg(CMSG_INFO, VERB_DEBUG,
							"(Sequence Number %02x %02x)", a, b);
					}
					else
						ctl_cmsg(CMSG_INFO, VERB_DEBUG,
							"(Sequence Number len=%d)", len);
					break;

				case 0x2F: /* End of Track */
					pos = tf_tell(tf);
					if (pos < next_pos)
						tf_seek(tf, next_pos - pos, SEEK_CUR);
					return 0;

				case 0x51: /* Tempo */
					a = tf_getc(tf);
					b = tf_getc(tf);
					c = tf_getc(tf);
					MIDIEVENT(smf_at_time, ME_TEMPO, c, a, b);
					break;

				case 0x54:
					/* SMPTE Offset (hr mn se fr ff)
					* hr: hours&type
					*     0     1     2     3    4    5    6    7   bits
					*     0  |<--type -->|<---- hours [0..23]---->|
					* type: 00: 24 frames/second
					*       01: 25 frames/second
					*       10: 30 frames/second (drop frame)
					*       11: 30 frames/second (non-drop frame)
					* mn: minis [0..59]
					* se: seconds [0..59]
					* fr: frames [0..29]
					* ff: fractional frames [0..99]
					*/
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(SMPTE Offset meta event)");
					skip(tf, len);
					break;

				case 0x58: /* Time Signature */
					smf_time_signature(smf_at_time, tf, len);
					break;

				case 0x59: /* Key Signature */
					smf_key_signature(smf_at_time, tf, len);
					break;

				case 0x7f: /* Sequencer-Specific Meta-Event */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Sequencer-Specific meta event, length %ld)",
						len);
					skip(tf, len);
					break;

				case 0x20: /* MIDI channel prefix (SMF v1.0) */
					if (len == 1)
					{
						int midi_channel_prefix = tf_getc(tf);
						ctl_cmsg(CMSG_INFO, VERB_DEBUG,
							"(MIDI channel prefix %d)",
							midi_channel_prefix);
					}
					else
						skip(tf, len);
					break;

				case 0x21: /* MIDI port number */
					/*
					if (len == 1)
					{
						if ((midi_port_number = tf_getc(tf))
							== EOF)
						{
							ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
								"Warning: %s: Too shorten midi file.",
								tf->filename.c_str());
							return -1;
						}
						midi_port_number &= 0xF;
					}
					else
					*/
						skip(tf, len);
					break;

				default:
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Meta event type 0x%02x, length %ld)",
						type, len);
					skip(tf, len);
					break;
				}
			}
		}
		else /* MIDI event */
		{
			a = me;
			if (a & 0x80) /* status byte */
			{
				#define MERGE_CHANNEL_PORT(ch) ((int)(ch) | (midi_port_number << 4))

				lastchan = MERGE_CHANNEL_PORT(a & 0x0F);
				laststatus = (a >> 4) & 0x07;
				if (laststatus != 7)
					a = tf_getc(tf) & 0x7F;
			}
			switch (laststatus)
			{
			case 0: /* Note off */
				b = tf_getc(tf) & 0x7F;
				MIDIEVENT(smf_at_time, ME_NOTEOFF, lastchan, a, b);
				break;

			case 1: /* Note on */
				b = tf_getc(tf) & 0x7F;
				if (b)
				{
					if (!note_seen && smf_at_time > 0)
					{
						MIDIEVENT(0, ME_NOTEON, lastchan, a, 0);
						MIDIEVENT(0, ME_NOTEOFF, lastchan, a, 0);
						note_seen = 1;
					}
					MIDIEVENT(smf_at_time, ME_NOTEON, lastchan, a, b);
				}
				else /* b == 0 means Note Off */
				{
					MIDIEVENT(smf_at_time, ME_NOTEOFF, lastchan, a, 0);
				}
				break;

			case 2: /* Key Pressure */
				b = tf_getc(tf) & 0x7F;
				MIDIEVENT(smf_at_time, ME_KEYPRESSURE, lastchan, a, b);
				break;

			case 3: /* Control change */
				b = tf_getc(tf);
				readmidi_add_ctl_event(smf_at_time, lastchan, a, b);
				break;

			case 4: /* Program change */
				MIDIEVENT(smf_at_time, ME_PROGRAM, lastchan, a, 0);
				break;

			case 5: /* Channel pressure */
				MIDIEVENT(smf_at_time, ME_CHANNEL_PRESSURE, lastchan, a, 0);
				break;

			case 6: /* Pitch wheel */
				b = tf_getc(tf) & 0x7F;
				MIDIEVENT(smf_at_time, ME_PITCHWHEEL, lastchan, a, b);
				break;

			default: /* case 7: */
					 /* Ignore this event */
				switch (lastchan & 0xF)
				{
				case 2: /* Sys Com Song Position Pntr */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Sys Com Song Position Pntr)");
					tf_getc(tf);
					tf_getc(tf);
					break;

				case 3: /* Sys Com Song Select(Song #) */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Sys Com Song Select(Song #))");
					tf_getc(tf);
					break;

				case 6: /* Sys Com tune request */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Sys Com tune request)");
					break;
				case 8: /* Sys real time timing clock */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Sys real time timing clock)");
					break;
				case 10: /* Sys real time start */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Sys real time start)");
					break;
				case 11: /* Sys real time continue */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Sys real time continue)");
					break;
				case 12: /* Sys real time stop */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Sys real time stop)");
					break;
				case 14: /* Sys real time active sensing */
					ctl_cmsg(CMSG_INFO, VERB_DEBUG,
						"(Sys real time active sensing)");
					break;
#if 0
				case 15: /* Meta */
				case 0: /* SysEx */
				case 7: /* SysEx */
#endif
				default: /* 1, 4, 5, 9, 13 */
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"*** Can't happen: status 0x%02X channel 0x%02X",
						laststatus, lastchan & 0xF);
					break;
				}
			}
		}
	}
	/*NOTREACHED*/
}

static int read_smf_file(struct timidity_file *tf)
{
	int32_t len, divisions;
	int16_t format, tracks, divisions_tmp;
	int i;

	karaoke_title_flag = 0;

	if (tf_read(&len, 4, 1, tf) != 1)
	{
		return 1;
	}
	len = BE_LONG(len);

	tf_read(&format, 2, 1, tf);
	tf_read(&tracks, 2, 1, tf);
	tf_read(&divisions_tmp, 2, 1, tf);
	format = BE_SHORT(format);
	tracks = BE_SHORT(tracks);
	divisions_tmp = BE_SHORT(divisions_tmp);

	if (divisions_tmp < 0)
	{
		/* SMPTE time -- totally untested. Got a MIDI file that uses this? */
		divisions =
			(int32_t)(-(divisions_tmp / 256)) * (int32_t)(divisions_tmp & 0xFF);
	}
	else
		divisions = (int32_t)divisions_tmp;

	if (len > 6)
	{
		ctl_cmsg(CMSG_WARNING, VERB_NORMAL,
			"%s: MIDI file header size %ld bytes",
			tf->filename.c_str(), len);
		skip(tf, len - 6); /* skip the excess */
	}
	if (format < 0 || format > 2)
	{
		ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
			"%s: Unknown MIDI file format %d", tf->filename.c_str(), format);
		return 1;
	}
	ctl_cmsg(CMSG_INFO, VERB_VERBOSE, "Format: %d  Tracks: %d  Divisions: %d",
		format, tracks, divisions);

	auto current_file_info = gplayer->get_midi_file_info("", 0);
	current_file_info->format = format;
	current_file_info->tracks = tracks;
	current_file_info->divisions = divisions;
	current_file_info->hdrsiz = (int16_t)tf_tell(tf);

	switch (format)
	{
	case 0:
		if (read_smf_track(tf, 0, 1))
		{
		}
		break;

	case 1:
		for (i = 0; i < tracks; i++)
		{
			if (read_smf_track(tf, i, 1))
			{
			}
		}
		break;

	case 2: /* We simply play the tracks sequentially */
		for (i = 0; i < tracks; i++)
		{
			if (read_smf_track(tf, i, 0))
			{
			}
		}
		break;
	}
	return 0;
}

static MidiEvent *read_midi_file(struct timidity_file *tf, int32_t *count, int32_t *sp,
	char *fn)
{
	char magic[4];
	MidiEvent *ev;
	int err, macbin_check;

	macbin_check = 1;
	auto current_file_info = gplayer->get_midi_file_info("", 0);

#if 0
	COPY_CHANNELMASK(drumchannels, current_file_info->drumchannels);
	COPY_CHANNELMASK(drumchannel_mask, current_file_info->drumchannel_mask);


#if MAX_CHANNELS > 16
	for (i = 16; i < MAX_CHANNELS; i++)
	{
		if (!IS_SET_CHANNELMASK(drumchannel_mask, i))
		{
			if (IS_SET_CHANNELMASK(drumchannels, i & 0xF))
				SET_CHANNELMASK(drumchannels, i);
			else
				UNSET_CHANNELMASK(drumchannels, i);
		}
	}
#endif
#endif

	if (tf_read(magic, 1, 4, tf) != 4)
	{
		return NULL;
	}

	if (memcmp(magic, "MThd", 4) == 0)
	{
		readmidi_read_init();
		err = read_smf_file(tf);
	}
	else err = 1;

	if (err)
	{
		free_midi_list();
		return NULL;
	}



	insert_note_steps();
	ev = groom_list(current_file_info->divisions, count, sp);
	if (ev == NULL)
	{
		free_midi_list();
		return NULL;
	}
	current_file_info->samples = *sp;
	if (current_file_info->first_text == NULL)
		current_file_info->first_text = safe_strdup("");
	current_file_info->readflag = 1;
	return ev;
}


//////////////////////////////////////// MID file player

static int play_midi_load_file(FileReader *fr,
	MidiEvent **event,
	int32_t *nsamples)
{
	int rc;
	struct timidity_file *tf;
	int32_t nevents;

	*event = NULL;

	tf = new timidity_file;
	tf->filename = "zdoom";
	tf->url = fr;

	*event = NULL;
	rc = RC_OK;

	*event = read_midi_file(tf, &nevents, nsamples, "zdoom");
	close_file(tf);

	if (*event == NULL)
	{
		return RC_ERROR;
	}

	ctl_cmsg(CMSG_INFO, VERB_NOISY,
		"%d supported events, %d samples, time %d:%02d",
		nevents, *nsamples,
		*nsamples / playback_rate / 60,
		(*nsamples / playback_rate) % 60);

	if ((play_mode->flag&PF_PCM_STREAM))
	{
		/* Load instruments
		* If opt_realtime_playing, the instruments will be loaded later.
		*/
		//if (!opt_realtime_playing)
		{
			rc = RC_OK;
			gplayer->instruments->load_missing_instruments(&rc);
			if (RC_IS_SKIP_FILE(rc))
			{
				/* Instrument loading is terminated */
				gplayer->instruments->clear_magic_instruments();
				return rc;
			}
		}
	}
	else
		gplayer->instruments->clear_magic_instruments();	/* Clear load markers */

	return RC_OK;
}



Instruments *instruments;
CRITICAL_SECTION critSect;


int load_midi_file(FileReader *fr, TimidityPlus::Player *p)
{
	int rc;
	static int last_rc = RC_OK;
	MidiEvent *event;
	int32_t nsamples;


	gplayer = p;
	InitializeCriticalSection(&critSect);
	if (play_mode->open_output() < 0)
	{
		return RC_ERROR;
	}

	/* Set current file information */
	auto current_file_info = gplayer->get_midi_file_info("zdoom", 1);

	rc = play_midi_load_file(fr, &event, &nsamples);
	if (RC_IS_SKIP_FILE(rc))
		return RC_ERROR; /* skip playing */

	return gplayer->start_midi(event, nsamples);
}


void Player::run_midi(int samples)
{
	auto time = current_event->time + samples;
	while (current_event->time < time)
	{
		if (play_event(current_event) != RC_OK)
			return;
		current_event++;
	}
}


void timidity_close()
{
	play_mode->close_output();
	DeleteCriticalSection(&critSect);
	free_global_mblock();
}


}