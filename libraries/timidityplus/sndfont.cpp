/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2005 Masanao Izumo <iz@onicos.co.jp>
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

    This code from awesfx
    Modified by Masanao Izumo <mo@goice.co.jp>

    ================================================================
    parsesf.c
         parse SoundFont layers and convert it to AWE driver patch

    Copyright (C) 1996,1997 Takashi Iwai
    ================================================================
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "timidity.h"
#include "common.h"
#include "tables.h"
#include "instrum.h"
#include "playmidi.h"
#include "filter.h"
#include "freq.h"
#include "resample.h"

namespace TimidityPlus
{

#define SFMalloc(rec, count)      new_segment(&(rec)->pool, count)
#define SFStrdup(rec, s)          strdup_mblock(&(rec)->pool, s)

/*----------------------------------------------------------------
 * compile flags
 *----------------------------------------------------------------*/


/* return value */
#define AWE_RET_OK		0	/* successfully loaded */
#define AWE_RET_ERR		1	/* some fatal error occurs */
#define AWE_RET_SKIP		2	/* some fonts are skipped */
#define AWE_RET_NOMEM		3	/* out or memory; not all fonts loaded */
#define AWE_RET_NOT_FOUND	4	/* the file is not found */

/*----------------------------------------------------------------
 * local parameters
 *----------------------------------------------------------------*/

struct SFPatchRec 
{
	int preset, bank, keynote; /* -1 = matches all */
};

struct SampleList 
{
	Sample v;
	SampleList *next;
	int32_t start;
	int32_t len;
	int32_t cutoff_freq;
	int16_t resonance;
	int16_t root, tune;
	char low, high;		/* key note range */
	int8_t reverb_send, chorus_send;

	/* Depend on playback_rate */
	int32_t vibrato_freq;
	int32_t attack;
	int32_t hold;
	int32_t sustain;
	int32_t decay;
	int32_t release;

	int32_t modattack;
	int32_t modhold;
	int32_t modsustain;
	int32_t moddecay;
	int32_t modrelease;

	int bank, keynote;	/* for drum instruments */
};

struct InstList {
	SFPatchRec pat;
	int pr_idx;
	int samples;
	int order;
	SampleList *slist;
	InstList *next;
};

struct SFExclude {
	SFPatchRec pat;
	SFExclude *next;
};

struct SFOrder {
	SFPatchRec pat;
	int order;
	SFOrder *next;
};

#define INSTHASHSIZE 127
#define INSTHASH(bank, preset, keynote) \
	((int)(((unsigned)bank ^ (unsigned)preset ^ (unsigned)keynote) % INSTHASHSIZE))

struct SFInsts {
	struct timidity_file *tf;
	char *fname;
	int8_t def_order, def_cutoff_allowed, def_resonance_allowed;
	uint16_t version, minorversion;
	int32_t samplepos, samplesize;
	InstList *instlist[INSTHASHSIZE];
	char **inst_namebuf;
	SFExclude *sfexclude;
	SFOrder *sforder;
	SFInsts *next;
	double amptune;
	MBlockList pool;
};

/*----------------------------------------------------------------*/

/* prototypes */

#define P_GLOBAL	1
#define P_LAYER		2
#define def_drum_inst 0


/*----------------------------------------------------------------*/


SFInsts *Instruments::find_soundfont(char *sf_file)
{
    SFInsts *sf;

    for(sf = sfrecs; sf != NULL; sf = sf->next)
	if(sf->fname != NULL && strcmp(sf->fname, sf_file) == 0)
	    return sf;
    return NULL;
}

SFInsts *Instruments::new_soundfont(char *sf_file)
{
	SFInsts *sf, *prev;

	for(sf = sfrecs, prev = NULL; sf != NULL; prev = sf, sf = sf->next)
	{
		if(sf->fname == NULL)
		{
			/* remove the record from the chain to reuse */
			if (prev != NULL)
				prev->next = sf->next;
			else if (sfrecs == sf)
				sfrecs = sf->next;
			break;
		}
	}
	if(sf == NULL)
		sf = (SFInsts *)safe_malloc(sizeof(SFInsts));
	memset(sf, 0, sizeof(SFInsts));
	init_mblock(&sf->pool);
	sf->fname = SFStrdup(sf, sf_file);
	sf->def_order = DEFAULT_SOUNDFONT_ORDER;
	sf->amptune = 1.0;
	return sf;
}

void Instruments::add_soundfont(char *sf_file, int sf_order, int sf_cutoff, int sf_resonance, int amp)
{
    SFInsts *sf;

    if((sf = find_soundfont(sf_file)) == NULL)
    {
        sf = new_soundfont(sf_file);
        sf->next = sfrecs;
        sfrecs = sf;
    }

    if(sf_order >= 0)
        sf->def_order = sf_order;
    if(sf_cutoff >= 0)
        sf->def_cutoff_allowed = sf_cutoff;
    if(sf_resonance >= 0)
        sf->def_resonance_allowed = sf_resonance;
    if(amp >= 0)
        sf->amptune = (double)amp * 0.01;
    current_sfrec = sf;
}

void Instruments::remove_soundfont(char *sf_file)
{
    SFInsts *sf;

    if((sf = find_soundfont(sf_file)) != NULL)
	end_soundfont(sf);
}

void Instruments::free_soundfonts()
{
	SFInsts *sf, *next;
	
	for (sf = sfrecs; sf != NULL; sf = next) {
		if (sf->tf != nullptr) tf_close(sf->tf);
		sf->tf = nullptr;
		reuse_mblock(&sf->pool);
		next = sf->next;
		free(sf);
	}
}

char *Instruments::soundfont_preset_name(int bank, int preset, int keynote,
			    char **sndfile)
{
    SFInsts *rec;
    if(sndfile != NULL)
	*sndfile = NULL;
    for(rec = sfrecs; rec != NULL; rec = rec->next)
	if(rec->fname != NULL)
	{
	    int addr;
	    InstList *ip;

	    addr = INSTHASH(bank, preset, keynote);
	    for(ip = rec->instlist[addr]; ip; ip = ip->next)
		if(ip->pat.bank == bank && ip->pat.preset == preset &&
		   (keynote < 0 || keynote == ip->pat.keynote))
		    break;
	    if(ip != NULL)
	    {
		if(sndfile != NULL)
		    *sndfile = rec->fname;
		return rec->inst_namebuf[ip->pr_idx];
	    }
	}
    return NULL;
}

void Instruments::init_sf(SFInsts *rec)
{
	SFInfo sfinfo;
	int i;

	if ((rec->tf = open_file(rec->fname, sfreader)) == NULL) {
		ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
			  "Can't open soundfont file %s", rec->fname);
		end_soundfont(rec);
		return;
	}

	if(load_soundfont(&sfinfo, rec->tf))
	{
	    end_soundfont(rec);
	    return;
	}

	correct_samples(&sfinfo);
	current_sfrec = rec;
	for (i = 0; i < sfinfo.npresets; i++) {
		int bank = sfinfo.preset[i].bank;
		int preset = sfinfo.preset[i].preset;

		if (bank == 128)
		    /* FIXME: why not allow exclusion of drumsets? */
		    alloc_instrument_bank(1, preset);
		else {
			if (is_excluded(rec, bank, preset, -1))
				continue;
			alloc_instrument_bank(0, bank);
		}
		load_font(&sfinfo, i);
	}

	/* copy header info */
	rec->version = sfinfo.version;
	rec->minorversion = sfinfo.minorversion;
	rec->samplepos = sfinfo.samplepos;
	rec->samplesize = sfinfo.samplesize;
	rec->inst_namebuf =
	    (char **)SFMalloc(rec, sfinfo.npresets * sizeof(char *));
	for(i = 0; i < sfinfo.npresets; i++)
	    rec->inst_namebuf[i] =
		(char *)SFStrdup(rec, sfinfo.preset[i].hdr.name);
	free_soundfont(&sfinfo);

	if (opt_sf_close_each_file) {
		tf_close(rec->tf);
		rec->tf = NULL;
	}
}

void Instruments::init_load_soundfont(void)
{
    SFInsts *rec;
    for(rec = sfrecs; rec != NULL; rec = rec->next)
	if(rec->fname != NULL)
	    init_sf(rec);
}

void Instruments::end_soundfont(SFInsts *rec)
{
	if (rec->tf) {
		tf_close(rec->tf);
		rec->tf = NULL;
	}

	rec->fname = NULL;
	rec->inst_namebuf = NULL;
	rec->sfexclude = NULL;
	rec->sforder = NULL;
	reuse_mblock(&rec->pool);
}

Instrument *Instruments::extract_soundfont(char *sf_file, int bank, int preset,int keynote)
{
    SFInsts *sf;

    if((sf = find_soundfont(sf_file)) != NULL)
	return try_load_soundfont(sf, -1, bank, preset, keynote);
    sf = new_soundfont(sf_file);
    sf->next = sfrecs;
    sf->def_order = 2;
    sfrecs = sf;
    init_sf(sf);
    return try_load_soundfont(sf, -1, bank, preset, keynote);
}

/*----------------------------------------------------------------
 * get converted instrument info and load the wave data from file
 *----------------------------------------------------------------*/

Instrument *Instruments::try_load_soundfont(SFInsts *rec, int order, int bank,int preset, int keynote)
{
	InstList *ip;
	Instrument *inst = NULL;
	int addr;

	if (rec->tf == NULL) {
		if (rec->fname == NULL)
			return NULL;
		if ((rec->tf = open_file(rec->fname, sfreader)) == NULL)
		{
			ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
				  "Can't open soundfont file %s", rec->fname);
			end_soundfont(rec);
			return NULL;
		}
	}

	addr = INSTHASH(bank, preset, keynote);
	for (ip = rec->instlist[addr]; ip; ip = ip->next) {
		if (ip->pat.bank == bank && ip->pat.preset == preset &&
		    (keynote < 0 || ip->pat.keynote == keynote) &&
		    (order < 0 || ip->order == order))
			break;
	}

	if (ip && ip->samples)
		inst = load_from_file(rec, ip);

	if (opt_sf_close_each_file) {
		tf_close(rec->tf);
		rec->tf = NULL;
	}

	return inst;
}

Instrument *Instruments::load_soundfont_inst(int order, int bank, int preset, int keynote)
{
    SFInsts *rec;
    Instrument *ip;
    /*
     * Search through all ordered soundfonts
     */
    int o = order;

    for(rec = sfrecs; rec != NULL; rec = rec->next)
    {
	if(rec->fname != NULL)
	{
	    ip = try_load_soundfont(rec, o, bank, preset, keynote);
	    if(ip != NULL)
		return ip;
	    if (o > 0) o++;
	}
    }
    return NULL;
}

/*----------------------------------------------------------------*/
#define TO_MHZ(abscents) (int32_t)(8176.0 * pow(2.0,(double)(abscents)/1200.0))
#define TO_VOLUME(level)  (uint8_t)(255.0 - (level) * (255.0/1000.0))

double Instruments::calc_volume(LayerTable *tbl)
{
    int v;

    if(!tbl->set[SF_initAtten] || (int)tbl->val[SF_initAtten] == 0)
	return (double)1.0;

	v = (int)tbl->val[SF_initAtten];
    if(v < 0) {v = 0;}
    else if(v > 960) {v = 960;}
	return cb_to_amp_table[v];
}

/* convert from 16bit value to fractional offset (15.15) */
int32_t Instruments::to_offset(int32_t offset)
{
	return offset << 14;
}

#define SF_ENVRATE_MAX (0x3FFFFFFFL)
#define SF_ENVRATE_MIN (1L)

/* calculate ramp rate in fractional unit;
 * diff = 16bit, time = msec
 */
int32_t Instruments::calc_rate(int32_t diff, double msec)
{
    double rate;

    if(msec == 0) {return (int32_t)SF_ENVRATE_MAX + 1;}
    if(diff <= 0) {diff = 1;}
    diff <<= 14;
    rate = ((double)diff / playback_rate) * control_ratio * 1000.0 / msec;
    if(fast_decay) {rate *= 2;}
	if(rate > SF_ENVRATE_MAX) {rate = SF_ENVRATE_MAX;}
	else if(rate < SF_ENVRATE_MIN) {rate = SF_ENVRATE_MIN;}
    return (int32_t)rate;
}

/* calculate ramp rate in fractional unit;
 * diff = 16bit, timecent
 */
int32_t Instruments::to_rate(int32_t diff, int timecent)
{
    double rate;

    if(timecent == -12000)	/* instantaneous attack */
	{return (int32_t)SF_ENVRATE_MAX + 1;}
    if(diff <= 0) {diff = 1;}
    diff <<= 14;
    rate = (double)diff * control_ratio / playback_rate / pow(2.0, (double)timecent / 1200.0);
    if(fast_decay) {rate *= 2;}
	if(rate > SF_ENVRATE_MAX) {rate = SF_ENVRATE_MAX;}
	else if(rate < SF_ENVRATE_MIN) {rate = SF_ENVRATE_MIN;}
    return (int32_t)rate;
}

/*
 * convert timecents to sec
 */
double Instruments::to_msec(int timecent)
{
    return timecent == -12000 ? 0 : 1000.0 * pow(2.0, (double)timecent / 1200.0);
}

/*
 * Sustain level
 * sf: centibels
 * parm: 0x7f - sustain_level(dB) * 0.75
 */
int32_t Instruments::calc_sustain(int sust_cB)
{
	if(sust_cB <= 0) {return 65533;}
	else if(sust_cB >= 1000) {return 0;}
	else {return (1000 - sust_cB) * 65533 / 1000;}
}

Instrument *Instruments::load_from_file(SFInsts *rec, InstList *ip)
{
	SampleList *sp;
	Instrument *inst;
	int i;
	int32_t len;

	inst = (Instrument *)safe_malloc(sizeof(Instrument));
	inst->instname = rec->inst_namebuf[ip->pr_idx];
	inst->type = INST_SF2;
	inst->samples = ip->samples;
	inst->sample = (Sample *)safe_malloc(sizeof(Sample) * ip->samples);
	memset(inst->sample, 0, sizeof(Sample) * ip->samples);
	for (i = 0, sp = ip->slist; i < ip->samples && sp; i++, sp = sp->next) {
		Sample *sample = inst->sample + i;
		int32_t j;
#ifdef _BIG_ENDIAN_
		int32_t k;
		int16_t *tmp, s;
#endif
		memcpy(sample, &sp->v, sizeof(Sample));
		sample->data = NULL;
		sample->data_alloced = 0;

		if(i > 0 && (!sample->note_to_use ||
			     (sample->modes & MODES_LOOPING)))
		{
		    SampleList *sps;
		    Sample *found, *s;

		    found = NULL;
		    for(j = 0, sps = ip->slist, s = inst->sample; j < i && sps;
			j++, sps = sps->next, s++)
		    {
			if(s->data == NULL)
			    break;
			if(sp->start == sps->start)
			{
			    if(antialiasing_allowed)
			    {
				 if(sample->data_length != s->data_length ||
				    sample->sample_rate != s->sample_rate)
				     continue;
			    }
			    if(s->note_to_use && !(s->modes & MODES_LOOPING))
				continue;
			    found = s;
			    break;
			}
		    }
		    if(found)
		    {
			sample->data = found->data;
			sample->data_alloced = 0;
			continue;
		    }
		}

		sample->data = (sample_t *)safe_large_malloc(sp->len + 2 * 3);
		sample->data_alloced = 1;

		tf_seek(rec->tf, sp->start, SEEK_SET);
		tf_read(sample->data, sp->len, 1, rec->tf);

#ifdef _BIG_ENDIAN_
		tmp = (int16_t*)sample->data;
		k = sp->len / 2;
		for (j = 0; j < k; j++) {
			s = LE_SHORT(*tmp);
			*tmp++ = s;
		}
#endif
		/* set a small blank loop at the tail for avoiding abnormal loop. */
		len = sp->len / 2;
		sample->data[len] = sample->data[len + 1] = sample->data[len + 2] = 0;

		if (antialiasing_allowed)
		    antialiasing((int16_t *)sample->data,
				 sample->data_length >> FRACTION_BITS,
				 sample->sample_rate,
				 playback_rate);

		/* resample it if possible */
		if (sample->note_to_use && !(sample->modes & MODES_LOOPING))
			pre_resample(sample);

		/* do pitch detection on drums if surround chorus is used */
		if (ip->pat.bank == 128 && timidity_surround_chorus)
		{
			Freq freq;
		    sample->chord = -1;
		    sample->root_freq_detected =
		    	freq.freq_fourier(sample, &(sample->chord));
		    sample->transpose_detected =
			assign_pitch_to_freq(sample->root_freq_detected) -
			assign_pitch_to_freq(sample->root_freq / 1024.0);
		}
	}

	return inst;
}


/*----------------------------------------------------------------
 * excluded samples
 *----------------------------------------------------------------*/

int Instruments::exclude_soundfont(int bank, int preset, int keynote)
{
	SFExclude *exc;
	if(current_sfrec == NULL)
	    return 1;
	exc = (SFExclude*)SFMalloc(current_sfrec , sizeof(SFExclude));
	exc->pat.bank = bank;
	exc->pat.preset = preset;
	exc->pat.keynote = keynote;
	exc->next = current_sfrec->sfexclude;
	current_sfrec->sfexclude = exc;
	return 0;
}

/* check the instrument is specified to be excluded */
int Instruments::is_excluded(SFInsts *rec, int bank, int preset, int keynote)
{
	SFExclude *p;
	for (p = rec->sfexclude; p; p = p->next) {
		if (p->pat.bank == bank &&
		    (p->pat.preset < 0 || p->pat.preset == preset) &&
		    (p->pat.keynote < 0 || p->pat.keynote == keynote))
			return 1;
	}
	return 0;
}


/*----------------------------------------------------------------
 * ordered samples
 *----------------------------------------------------------------*/

int Instruments::order_soundfont(int bank, int preset, int keynote, int order)
{
	SFOrder *p;
	if(current_sfrec == NULL)
	    return 1;
	p = (SFOrder*)SFMalloc(current_sfrec, sizeof(SFOrder));
	p->pat.bank = bank;
	p->pat.preset = preset;
	p->pat.keynote = keynote;
	p->order = order;
	p->next = current_sfrec->sforder;
	current_sfrec->sforder = p;
	return 0;
}

/* check the instrument is specified to be ordered */
int Instruments::is_ordered(SFInsts *rec, int bank, int preset, int keynote)
{
	SFOrder *p;
	for (p = rec->sforder; p; p = p->next) {
		if (p->pat.bank == bank &&
		    (p->pat.preset < 0 || p->pat.preset == preset) &&
		    (p->pat.keynote < 0 || p->pat.keynote == keynote))
			return p->order;
	}
	return -1;
}


/*----------------------------------------------------------------*/

int Instruments::load_font(SFInfo *sf, int pridx)
{
	SFPresetHdr *preset = &sf->preset[pridx];
	int rc, j, nlayers;
	SFGenLayer *layp, *globalp;

	/* if layer is empty, skip it */
	if ((nlayers = preset->hdr.nlayers) <= 0 ||
	    (layp = preset->hdr.layer) == NULL)
		return AWE_RET_SKIP;
	/* check global layer */
	globalp = NULL;
	if (is_global(layp)) {
		globalp = layp;
		layp++;
		nlayers--;
	}
	/* parse for each preset layer */
	for (j = 0; j < nlayers; j++, layp++) {
		LayerTable tbl;

		/* set up table */
		clear_table(&tbl);
		if (globalp)
			set_to_table(sf, &tbl, globalp, P_GLOBAL);
		set_to_table(sf, &tbl, layp, P_LAYER);
		
		/* parse the instrument */
		rc = parse_layer(sf, pridx, &tbl, 0);
		if(rc == AWE_RET_ERR || rc == AWE_RET_NOMEM)
			return rc;
	}

	return AWE_RET_OK;
}


/*----------------------------------------------------------------*/

/* parse a preset layer and convert it to the patch structure */
int Instruments::parse_layer(SFInfo *sf, int pridx, LayerTable *tbl, int level)
{
	SFInstHdr *inst;
	int rc, i, nlayers;
	SFGenLayer *lay, *globalp;

	if (level >= 2) {
		fprintf(stderr, "parse_layer: too deep instrument level\n");
		return AWE_RET_ERR;
	}

	/* instrument must be defined */
	if (!tbl->set[SF_instrument])
		return AWE_RET_SKIP;

	inst = &sf->inst[tbl->val[SF_instrument]];

	/* if layer is empty, skip it */
	if ((nlayers = inst->hdr.nlayers) <= 0 ||
	    (lay = inst->hdr.layer) == NULL)
		return AWE_RET_SKIP;

	reset_last_sample_info();

	/* check global layer */
	globalp = NULL;
	if (is_global(lay)) {
		globalp = lay;
		lay++;
		nlayers--;
	}

	/* parse for each layer */
	for (i = 0; i < nlayers; i++, lay++) {
		LayerTable ctbl;
		clear_table(&ctbl);
		if (globalp)
			set_to_table(sf, &ctbl, globalp, P_GLOBAL);
		set_to_table(sf, &ctbl, lay, P_LAYER);

		if (!ctbl.set[SF_sampleId]) {
			/* recursive loading */
			merge_table(sf, &ctbl, tbl);
			if (! sanity_range(&ctbl))
				continue;
			rc = parse_layer(sf, pridx, &ctbl, level+1);
			if (rc != AWE_RET_OK && rc != AWE_RET_SKIP)
				return rc;

			reset_last_sample_info();
		} else {
			init_and_merge_table(sf, &ctbl, tbl);
			if (! sanity_range(&ctbl))
				continue;

			/* load the info data */
			if ((rc = make_patch(sf, pridx, &ctbl)) == AWE_RET_ERR)
				return rc;
		}
	}
	return AWE_RET_OK;
}


int Instruments::is_global(SFGenLayer *layer)
{
	int i;
	for (i = 0; i < layer->nlists; i++) {
		if (layer->list[i].oper == SF_instrument ||
		    layer->list[i].oper == SF_sampleId)
			return 0;
	}
	return 1;
}


/*----------------------------------------------------------------
 * layer table handlers
 *----------------------------------------------------------------*/

/* initialize layer table */
void Instruments::clear_table(LayerTable *tbl)
{
	memset(tbl->val, 0, sizeof(tbl->val));
	memset(tbl->set, 0, sizeof(tbl->set));
}

/* set items in a layer to the table */
void Instruments::set_to_table(SFInfo *sf, LayerTable *tbl, SFGenLayer *lay, int level)
{
	int i;
	for (i = 0; i < lay->nlists; i++) {
		SFGenRec *gen = &lay->list[i];
		/* copy the value regardless of its copy policy */
		tbl->val[gen->oper] = gen->amount;
		tbl->set[gen->oper] = level;
	}
}

/* add an item to the table */
void Instruments::add_item_to_table(LayerTable *tbl, int oper, int amount, int level)
{
	LayerItem *item = &layer_items[oper];
	int o_lo, o_hi, lo, hi;

	switch (item->copy) {
	case L_INHRT:
		tbl->val[oper] += amount;
		break;
	case L_OVWRT:
		tbl->val[oper] = amount;
		break;
	case L_PRSET:
	case L_INSTR:
		/* do not overwrite */
		if (!tbl->set[oper])
			tbl->val[oper] = amount;
		break;
	case L_RANGE:
		if (!tbl->set[oper]) {
			tbl->val[oper] = amount;
		} else {
			o_lo = LOWNUM(tbl->val[oper]);
			o_hi = HIGHNUM(tbl->val[oper]);
			lo = LOWNUM(amount);
			hi = HIGHNUM(amount);
			if (lo < o_lo) lo = o_lo;
			if (hi > o_hi) hi = o_hi;
			tbl->val[oper] = RANGE(lo, hi);
		}
		break;
	}
}

/* merge two tables */
void Instruments::merge_table(SFInfo *sf, LayerTable *dst, LayerTable *src)
{
	int i;
	for (i = 0; i < SF_EOF; i++) {
		if (src->set[i]) {
			if (sf->version == 1) {
				if (!dst->set[i] ||
				    i == SF_keyRange || i == SF_velRange)
					/* just copy it */
					dst->val[i] = src->val[i];
			}
			else
				add_item_to_table(dst, i, src->val[i], P_GLOBAL);
			dst->set[i] = P_GLOBAL;
		}
	}
}

/* merge and set default values */
void Instruments::init_and_merge_table(SFInfo *sf, LayerTable *dst, LayerTable *src)
{
	int i;

	/* default value is not zero */
	if (sf->version == 1) {
		layer_items[SF_sustainEnv1].defv = 1000;
		layer_items[SF_sustainEnv2].defv = 1000;
		layer_items[SF_freqLfo1].defv = -725;
		layer_items[SF_freqLfo2].defv = -15600;
	} else {
		layer_items[SF_sustainEnv1].defv = 0;
		layer_items[SF_sustainEnv2].defv = 0;
		layer_items[SF_freqLfo1].defv = 0;
		layer_items[SF_freqLfo2].defv = 0;
	}

	/* set default */
	for (i = 0; i < SF_EOF; i++) {
		if (!dst->set[i])
			dst->val[i] = layer_items[i].defv;
	}
	merge_table(sf, dst, src);
	/* convert from SBK to SF2 */
	if (sf->version == 1) {
		for (i = 0; i < SF_EOF; i++) {
			if (dst->set[i])
				dst->val[i] = sbk_to_sf2(i, dst->val[i], layer_items);
		}
	}
}


/*----------------------------------------------------------------
 * check key and velocity range
 *----------------------------------------------------------------*/

int Instruments::sanity_range(LayerTable *tbl)
{
	int lo, hi;

	lo = LOWNUM(tbl->val[SF_keyRange]);
	hi = HIGHNUM(tbl->val[SF_keyRange]);
	if (lo < 0 || lo > 127 || hi < 0 || hi > 127 || hi < lo)
		return 0;

	lo = LOWNUM(tbl->val[SF_velRange]);
	hi = HIGHNUM(tbl->val[SF_velRange]);
	if (lo < 0 || lo > 127 || hi < 0 || hi > 127 || hi < lo)
		return 0;

	return 1;
}


/*----------------------------------------------------------------
 * create patch record from the stored data table
 *----------------------------------------------------------------*/

int Instruments::make_patch(SFInfo *sf, int pridx, LayerTable *tbl)
{
    int bank, preset, keynote;
    int keynote_from, keynote_to, done;
    int addr, order;
    InstList *ip;
    SFSampleInfo *sample;
    SampleList *sp;

    sample = &sf->sample[tbl->val[SF_sampleId]];

	if(sample->sampletype & SF_SAMPLETYPE_ROM) /* is ROM sample? */
    {
	ctl_cmsg(CMSG_INFO, VERB_DEBUG, "preset %d is ROM sample: 0x%x",
		  pridx, sample->sampletype);
	return AWE_RET_SKIP;
    }

    bank = sf->preset[pridx].bank;
    preset = sf->preset[pridx].preset;
    if(bank == 128){
		keynote_from = LOWNUM(tbl->val[SF_keyRange]);
		keynote_to = HIGHNUM(tbl->val[SF_keyRange]);
    } else
	keynote_from = keynote_to = -1;

	done = 0;
	for(keynote=keynote_from;keynote<=keynote_to;keynote++){

    if(is_excluded(current_sfrec, bank, preset, keynote))
    {
	continue;
    } else
	done++;

    order = is_ordered(current_sfrec, bank, preset, keynote);
    if(order < 0)
	order = current_sfrec->def_order;

    addr = INSTHASH(bank, preset, keynote);

    for(ip = current_sfrec->instlist[addr]; ip; ip = ip->next)
    {
	if(ip->pat.bank == bank && ip->pat.preset == preset &&
	   (keynote < 0 || keynote == ip->pat.keynote))
	    break;
    }

    if(ip == NULL)
    {
	ip = (InstList*)SFMalloc(current_sfrec, sizeof(InstList));
	memset(ip, 0, sizeof(InstList));
	ip->pr_idx = pridx;
	ip->pat.bank = bank;
	ip->pat.preset = preset;
	ip->pat.keynote = keynote;
	ip->order = order;
	ip->samples = 0;
	ip->slist = NULL;
	ip->next = current_sfrec->instlist[addr];
	current_sfrec->instlist[addr] = ip;
    }

    /* new sample */
    sp = (SampleList *)SFMalloc(current_sfrec, sizeof(SampleList));
    memset(sp, 0, sizeof(SampleList));

	sp->bank = bank;
	sp->keynote = keynote;

	if(tbl->set[SF_keynum]) {
		sp->v.note_to_use = (int)tbl->val[SF_keynum];
	} else if(bank == 128) {
		sp->v.note_to_use = keynote;
	}
    make_info(sf, sp, tbl);

    /* add a sample */
    if(ip->slist == NULL)
	ip->slist = sp;
    else
    {
	SampleList *cur, *prev;
	int32_t start;

	/* Insert sample */
	start = sp->start;
	cur = ip->slist;
	prev = NULL;
	while(cur && cur->start <= start)
	{
	    prev = cur;
	    cur = cur->next;
	}
	if(prev == NULL)
	{
	    sp->next = ip->slist;
	    ip->slist = sp;
	}
	else
	{
	    prev->next = sp;
	    sp->next = cur;
	}
    }
    ip->samples++;

	} /* for(;;) */


	if(done==0)
	return AWE_RET_SKIP;
	else
	return AWE_RET_OK;
}

/*----------------------------------------------------------------
 *
 * Modified for TiMidity
 */

/* conver to Sample parameter */
void Instruments::make_info(SFInfo *sf, SampleList *vp, LayerTable *tbl)
{
	set_sample_info(sf, vp, tbl);
	set_init_info(sf, vp, tbl);
	set_rootkey(sf, vp, tbl);
	set_rootfreq(vp);

	/* tremolo & vibrato */
	convert_tremolo(vp, tbl);
	convert_vibrato(vp, tbl);
}

void Instruments::set_envelope_parameters(SampleList *vp)
{
		/* convert envelope parameters */
		vp->v.envelope_offset[0] = to_offset(65535);
		vp->v.envelope_rate[0] = vp->attack;

		vp->v.envelope_offset[1] = to_offset(65534);
		vp->v.envelope_rate[1] = vp->hold;

		vp->v.envelope_offset[2] = to_offset(vp->sustain);
		vp->v.envelope_rate[2] = vp->decay;

		vp->v.envelope_offset[3] = 0;
		vp->v.envelope_rate[3] = vp->release;

		vp->v.envelope_offset[4] = 0;
		vp->v.envelope_rate[4] = vp->release;

		vp->v.envelope_offset[5] = 0;
		vp->v.envelope_rate[5] = vp->release;

		/* convert modulation envelope parameters */
		vp->v.modenv_offset[0] = to_offset(65535);
		vp->v.modenv_rate[0] = vp->modattack;

		vp->v.modenv_offset[1] = to_offset(65534);
		vp->v.modenv_rate[1] = vp->modhold;

		vp->v.modenv_offset[2] = to_offset(vp->modsustain);
		vp->v.modenv_rate[2] = vp->moddecay;

		vp->v.modenv_offset[3] = 0;
		vp->v.modenv_rate[3] = vp->modrelease;

		vp->v.modenv_offset[4] = 0;
		vp->v.modenv_rate[4] = vp->modrelease;

		vp->v.modenv_offset[5] = 0;
		vp->v.modenv_rate[5] = vp->modrelease;
}

/* set sample address */
void Instruments::set_sample_info(SFInfo *sf, SampleList *vp, LayerTable *tbl)
{
    SFSampleInfo *sp = &sf->sample[tbl->val[SF_sampleId]];

    /* set sample position */
    vp->start = (tbl->val[SF_startAddrsHi] << 15)
	+ tbl->val[SF_startAddrs]
	+ sp->startsample;
    vp->len = (tbl->val[SF_endAddrsHi] << 15)
	+ tbl->val[SF_endAddrs]
	+ sp->endsample - vp->start;

	vp->start = abs(vp->start);
	vp->len = abs(vp->len);

    /* set loop position */
    vp->v.loop_start = (tbl->val[SF_startloopAddrsHi] << 15)
	+ tbl->val[SF_startloopAddrs]
	+ sp->startloop - vp->start;
    vp->v.loop_end = (tbl->val[SF_endloopAddrsHi] << 15)
	+ tbl->val[SF_endloopAddrs]
	+ sp->endloop - vp->start;

    /* set data length */
    vp->v.data_length = vp->len + 1;

	/* fix loop position */
	if (vp->v.loop_end > (splen_t)vp->len + 1)
		vp->v.loop_end = vp->len + 1;
	if (vp->v.loop_start > (splen_t)vp->len)
		vp->v.loop_start = vp->len;
	if (vp->v.loop_start >= vp->v.loop_end)
	{
		vp->v.loop_start = vp->len;
		vp->v.loop_end = vp->len + 1;
	}

    /* Sample rate */
	if(sp->samplerate > 50000) {sp->samplerate = 50000;}
	else if(sp->samplerate < 400) {sp->samplerate = 400;}
    vp->v.sample_rate = sp->samplerate;

    /* sample mode */
    vp->v.modes = MODES_16BIT;

    /* volume envelope & total volume */
    vp->v.volume = calc_volume(tbl) * current_sfrec->amptune;

	convert_volume_envelope(vp, tbl);
	set_envelope_parameters(vp);

    if(tbl->val[SF_sampleFlags] == 1 || tbl->val[SF_sampleFlags] == 3)
    {
		/* looping */
		vp->v.modes |= MODES_LOOPING | MODES_SUSTAIN;
		if(tbl->val[SF_sampleFlags] == 3)
			vp->v.data_length = vp->v.loop_end; /* strip the tail */
	}
    else
    {
		/* set a small blank loop at the tail for avoiding abnormal loop. */
		vp->v.loop_start = vp->len;
		vp->v.loop_end = vp->len + 1;
    }

    /* convert to fractional samples */
    vp->v.data_length <<= FRACTION_BITS;
    vp->v.loop_start <<= FRACTION_BITS;
    vp->v.loop_end <<= FRACTION_BITS;

    /* point to the file position */
    vp->start = vp->start * 2 + sf->samplepos;
    vp->len *= 2;

	vp->v.vel_to_fc = -2400;	/* SF2 default value */
	vp->v.key_to_fc = vp->v.vel_to_resonance = 0;
	vp->v.envelope_velf_bpo = vp->v.modenv_velf_bpo =
		vp->v.vel_to_fc_threshold = 64;
	vp->v.key_to_fc_bpo = 60;
	memset(vp->v.envelope_velf, 0, sizeof(vp->v.envelope_velf));
	memset(vp->v.modenv_velf, 0, sizeof(vp->v.modenv_velf));

	vp->v.inst_type = INST_SF2;
}

/*----------------------------------------------------------------*/

/* set global information */

void Instruments::set_init_info(SFInfo *sf, SampleList *vp, LayerTable *tbl)
{
    int val;
    SFSampleInfo *sample;
    sample = &sf->sample[tbl->val[SF_sampleId]];

    /* key range */
    if(tbl->set[SF_keyRange])
    {
	vp->low = LOWNUM(tbl->val[SF_keyRange]);
	vp->high = HIGHNUM(tbl->val[SF_keyRange]);
    }
    else
    {
	vp->low = 0;
	vp->high = 127;
    }
    vp->v.low_freq = freq_table[(int)vp->low];
    vp->v.high_freq = freq_table[(int)vp->high];

    /* velocity range */
    if(tbl->set[SF_velRange]) {
		vp->v.low_vel = LOWNUM(tbl->val[SF_velRange]);
		vp->v.high_vel = HIGHNUM(tbl->val[SF_velRange]);
    } else {
		vp->v.low_vel = 0;
		vp->v.high_vel = 127;
	}

    /* fixed key & velocity */
    if(tbl->set[SF_keynum])
		vp->v.note_to_use = (int)tbl->val[SF_keynum];
	if(tbl->set[SF_velocity] && (int)tbl->val[SF_velocity] != 0) {
		ctl_cmsg(CMSG_INFO,VERB_DEBUG,"error: fixed-velocity is not supported.");
	}

	vp->v.sample_type = sample->sampletype;
	vp->v.sf_sample_index = tbl->val[SF_sampleId];
	vp->v.sf_sample_link = sample->samplelink;

	/* Some sf2 files don't contain valid sample links, so see if the
	   previous sample was a matching Left / Right sample with the
	   link missing and add it */
	switch (sample->sampletype) {
	case SF_SAMPLETYPE_LEFT:
		if (vp->v.sf_sample_link == 0 &&
		    last_sample_type == SF_SAMPLETYPE_RIGHT &&
		    last_sample_instrument == tbl->val[SF_instrument] &&
		    last_sample_keyrange == tbl->val[SF_keyRange]) {
		    	/* The previous sample was a matching right sample
		    	   set the link */
		    	vp->v.sf_sample_link = last_sample_list->v.sf_sample_index;
		}
		break;
	case SF_SAMPLETYPE_RIGHT:
		if (last_sample_list &&
		    last_sample_list->v.sf_sample_link == 0 &&
		    last_sample_type == SF_SAMPLETYPE_LEFT &&
		    last_sample_instrument == tbl->val[SF_instrument] &&
		    last_sample_keyrange == tbl->val[SF_keyRange]) {
		    	/* The previous sample was a matching left sample
		    	   set the link on the previous sample*/
		    	last_sample_list->v.sf_sample_link = tbl->val[SF_sampleId];
		}
		break;
	}

	/* Remember this sample in case the next one is a match */
	last_sample_type = sample->sampletype;;
	last_sample_instrument = tbl->val[SF_instrument];
	last_sample_keyrange = tbl->val[SF_keyRange];
	last_sample_list = vp;

	/* panning position: 0 to 127 */
	val = (int)tbl->val[SF_panEffectsSend];
    if(sample->sampletype == SF_SAMPLETYPE_MONO || val != 0) {	/* monoSample = 1 */
		if(val < -500)
		vp->v.panning = 0;
		else if(val > 500)
		vp->v.panning = 127;
		else
		vp->v.panning = (int8_t)((val + 500) * 127 / 1000);
	} else if(sample->sampletype == SF_SAMPLETYPE_RIGHT) {	/* rightSample = 2 */
		vp->v.panning = 127;
	} else if(sample->sampletype == SF_SAMPLETYPE_LEFT) {	/* leftSample = 4 */
		vp->v.panning = 0;
	} else if(sample->sampletype == SF_SAMPLETYPE_LINKED) {	/* linkedSample = 8 */
		ctl_cmsg(CMSG_ERROR,VERB_NOISY,"error: linkedSample is not supported.");
	}

	memset(vp->v.envelope_keyf, 0, sizeof(vp->v.envelope_keyf));
	memset(vp->v.modenv_keyf, 0, sizeof(vp->v.modenv_keyf));
	if(tbl->set[SF_autoHoldEnv2]) {
		vp->v.envelope_keyf[1] = (int16_t)tbl->val[SF_autoHoldEnv2];
	}
	if(tbl->set[SF_autoDecayEnv2]) {
		vp->v.envelope_keyf[2] = (int16_t)tbl->val[SF_autoDecayEnv2];
	}
	if(tbl->set[SF_autoHoldEnv1]) {
		vp->v.modenv_keyf[1] = (int16_t)tbl->val[SF_autoHoldEnv1];
	}
	if(tbl->set[SF_autoDecayEnv1]) {
		vp->v.modenv_keyf[2] = (int16_t)tbl->val[SF_autoDecayEnv1];
	}

	current_sfrec->def_cutoff_allowed = 1;
	current_sfrec->def_resonance_allowed = 1;

    /* initial cutoff & resonance */
    vp->cutoff_freq = 0;
    if((int)tbl->val[SF_initialFilterFc] < 0)
	tbl->set[SF_initialFilterFc] = tbl->val[SF_initialFilterFc] = 0;
    if(current_sfrec->def_cutoff_allowed && tbl->set[SF_initialFilterFc]
		&& (int)tbl->val[SF_initialFilterFc] >= 1500 && (int)tbl->val[SF_initialFilterFc] <= 13500)
    {
    val = (int)tbl->val[SF_initialFilterFc];
	val = abscent_to_Hz(val);

	if(!timidity_modulation_envelope) {
		if(tbl->set[SF_env1ToFilterFc] && (int)tbl->val[SF_env1ToFilterFc] > 0)
		{
			val = int( val * pow(2.0,(double)tbl->val[SF_env1ToFilterFc] / 1200.0f));
			if(val > 20000) {val = 20000;}
		}
	}

	vp->cutoff_freq = val;
    }
	vp->v.cutoff_freq = vp->cutoff_freq;

	vp->resonance = 0;
    if(current_sfrec->def_resonance_allowed && tbl->set[SF_initialFilterQ])
    {
	val = (int)tbl->val[SF_initialFilterQ];
	vp->resonance = val;
	}
	vp->v.resonance = vp->resonance;
}

void Instruments::reset_last_sample_info(void)
{
    last_sample_list = NULL;
    last_sample_type = 0;
    /* Set last instrument and keyrange to a value which cannot be represented
       by LayerTable.val (which is a short) */
    last_sample_instrument = 0x80000000;
    last_sample_keyrange   = 0x80000000;
}

int Instruments::abscent_to_Hz(int abscents)
{
	return (int)(8.176 * pow(2.0, (double)abscents / 1200.0));
}

/*----------------------------------------------------------------*/

#define SF_MODENV_CENT_MAX 1200	/* Live! allows only +-1200cents. */

/* calculate root key & fine tune */
void Instruments::set_rootkey(SFInfo *sf, SampleList *vp, LayerTable *tbl)
{
	SFSampleInfo *sp = &sf->sample[tbl->val[SF_sampleId]];
	int temp;
	
	/* scale factor */
	vp->v.scale_factor = 1024 * (double) tbl->val[SF_scaleTuning] / 100 + 0.5;
	/* set initial root key & fine tune */
	if (sf->version == 1 && tbl->set[SF_samplePitch]) {
		/* set from sample pitch */
		vp->root = tbl->val[SF_samplePitch] / 100;
		vp->tune = -tbl->val[SF_samplePitch] % 100;
		if (vp->tune <= -50)
			vp->root++, vp->tune += 100;
	} else {
		/* from sample info */
		vp->root = sp->originalPitch;
		vp->tune = (int8_t) sp->pitchCorrection;
	}
	/* orverride root key */
	if (tbl->set[SF_rootKey])
		vp->root = tbl->val[SF_rootKey];
	else if (vp->bank == 128 && vp->v.scale_factor != 0)
		vp->tune += int16_t((vp->keynote - sp->originalPitch) * 100 * (double) vp->v.scale_factor / 1024);
	vp->tune += tbl->val[SF_coarseTune] * 100 + tbl->val[SF_fineTune];
	/* correct too high pitch */
	if (vp->root >= vp->high + 60)
		vp->root -= 60;
	vp->v.tremolo_to_pitch =
			(tbl->set[SF_lfo1ToPitch]) ? tbl->val[SF_lfo1ToPitch] : 0;
	vp->v.tremolo_to_fc =
			(tbl->set[SF_lfo1ToFilterFc]) ? tbl->val[SF_lfo1ToFilterFc] : 0;
	vp->v.modenv_to_pitch =
			(tbl->set[SF_env1ToPitch]) ? tbl->val[SF_env1ToPitch] : 0;
	/* correct tune with the sustain level of modulation envelope */
	temp = vp->v.modenv_to_pitch
			* (double) (1000 - tbl->val[SF_sustainEnv1]) / 1000 + 0.5;
	vp->tune += temp, vp->v.modenv_to_pitch -= temp;
	vp->v.modenv_to_fc =
			(tbl->set[SF_env1ToFilterFc]) ? tbl->val[SF_env1ToFilterFc] : 0;
}

void Instruments::set_rootfreq(SampleList *vp)
{
	int root = vp->root;
	int tune = 0.5 - 256 * (double) vp->tune / 100;
	
	/* 0 <= tune < 255 */
	while (tune < 0)
		root--, tune += 256;
	while (tune > 255)
		root++, tune -= 256;
	if (root < 0) {
		vp->v.root_freq = freq_table[0] * (double) bend_fine[tune]
				/ bend_coarse[-root] + 0.5;
		vp->v.scale_freq = 0;		/* scale freq */
	} else if (root > 127) {
		vp->v.root_freq = freq_table[127] * (double) bend_fine[tune]
				* bend_coarse[root - 127] + 0.5;
		vp->v.scale_freq = 127;		/* scale freq */
	} else {
		vp->v.root_freq = freq_table[root] * (double) bend_fine[tune] + 0.5;
		vp->v.scale_freq = root;	/* scale freq */
	}
}

/*----------------------------------------------------------------*/


/*Pseudo Reverb*/
extern int32_t modify_release;


/* volume envelope parameters */
void Instruments::convert_volume_envelope(SampleList *vp, LayerTable *tbl)
{
    vp->attack  = to_rate(65535, tbl->val[SF_attackEnv2]);
    vp->hold    = to_rate(1, tbl->val[SF_holdEnv2]);
    vp->sustain = calc_sustain(tbl->val[SF_sustainEnv2]);
    vp->decay   = to_rate(65533 - vp->sustain, tbl->val[SF_decayEnv2]);
    if(modify_release) /* Pseudo Reverb */
	vp->release = calc_rate(65535, modify_release);
    else
	vp->release = to_rate(65535, tbl->val[SF_releaseEnv2]);
	vp->v.envelope_delay = playback_rate * 
		to_msec(tbl->val[SF_delayEnv2]) * 0.001;

	/* convert modulation envelope */
    vp->modattack  = to_rate(65535, tbl->val[SF_attackEnv1]);
    vp->modhold    = to_rate(1, tbl->val[SF_holdEnv1]);
    vp->modsustain = calc_sustain(tbl->val[SF_sustainEnv1]);
    vp->moddecay   = to_rate(65533 - vp->modsustain, tbl->val[SF_decayEnv1]);
    if(modify_release) /* Pseudo Reverb */
	vp->modrelease = calc_rate(65535, modify_release);
    else
	vp->modrelease = to_rate(65535, tbl->val[SF_releaseEnv1]);
	vp->v.modenv_delay = playback_rate * 
		to_msec(tbl->val[SF_delayEnv1]) * 0.001;

    vp->v.modes |= MODES_ENVELOPE;
}


/*----------------------------------------------------------------
 * tremolo (LFO1) conversion
 *----------------------------------------------------------------*/

void Instruments::convert_tremolo(SampleList *vp, LayerTable *tbl)
{
	int32_t freq;
	double level;

	if (!tbl->set[SF_lfo1ToVolume])
		return;

	level = pow(10.0, (double)abs(tbl->val[SF_lfo1ToVolume]) / -200.0);
	vp->v.tremolo_depth = 256 * (1.0 - level);
	if ((int)tbl->val[SF_lfo1ToVolume] < 0) { vp->v.tremolo_depth = -vp->v.tremolo_depth; }

	/* frequency in mHz */
	if (!tbl->set[SF_freqLfo1])
		freq = 0;
	else
	{
		freq = (int)tbl->val[SF_freqLfo1];
		freq = TO_MHZ(freq);
	}

	/* convert mHz to sine table increment; 1024<<rate_shift=1wave */
	vp->v.tremolo_phase_increment = ((playback_rate / 1000 * freq) >> RATE_SHIFT) / control_ratio;
	vp->v.tremolo_delay = playback_rate *
		to_msec(tbl->val[SF_delayLfo1]) * 0.001;
}

/*----------------------------------------------------------------
 * vibrato (LFO2) conversion
 *----------------------------------------------------------------*/

void Instruments::convert_vibrato(SampleList *vp, LayerTable *tbl)
{
	int32_t shift, freq;

	if (!tbl->set[SF_lfo2ToPitch]) {
		vp->v.vibrato_control_ratio = 0;
		return;
	}

	shift = (int)tbl->val[SF_lfo2ToPitch];

	/* cents to linear; 400cents = 256 */
	shift = shift * 256 / 400;
	if (shift > 255) { shift = 255; }
	else if (shift < -255) { shift = -255; }
	vp->v.vibrato_depth = (int16_t)shift;

	/* frequency in mHz */
	if (!tbl->set[SF_freqLfo2])
		freq = 0;
	else
	{
		freq = (int)tbl->val[SF_freqLfo2];
		freq = TO_MHZ(freq);
		if (freq == 0) { freq = 1; }
		/* convert mHz to control ratio */
		vp->v.vibrato_control_ratio = (1000 * playback_rate) /
			(freq * 2 * VIBRATO_SAMPLE_INCREMENTS);
	}

	vp->v.vibrato_delay = playback_rate *
		to_msec(tbl->val[SF_delayLfo2]) * 0.001;
}

}
