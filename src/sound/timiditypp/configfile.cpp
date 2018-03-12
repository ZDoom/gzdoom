/*
TiMidity++ -- MIDI to WAVE converter and player
Copyright (C) 1999-2008 Masanao Izumo <iz@onicos.co.jp>
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
#include <string.h>
#include <stdint.h>

#include <time.h>

#include <signal.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "quantity.h"
#include "cmdlib.h"
#include "i_soundfont.h"



namespace TimidityPlus
{

#define MAXWORDS 130
#define CHECKERRLIMIT \
  if(++errcnt >= 10) { \
    ctl_cmsg(CMSG_ERROR, VERB_NORMAL, \
      "Too many errors... Give up read %s", name); \
    reuse_mblock(&varbuf); \
    tf_close(tf); return 1; }


typedef struct {
	const char *name;
	int mapid, isdrum;
} MapNameEntry;

static int mapnamecompare(const void *name, const void *entry)
{
	return strcmp((const char *)name, ((const MapNameEntry *)entry)->name);
}

static int mapname2id(char *name, int *isdrum)
{
	static const MapNameEntry data[] = {
		/* sorted in alphabetical order */
		{ "gm2",         GM2_TONE_MAP, 0 },
	{ "gm2drum",     GM2_DRUM_MAP, 1 },
	{ "sc55",        SC_55_TONE_MAP, 0 },
	{ "sc55drum",    SC_55_DRUM_MAP, 1 },
	{ "sc88",        SC_88_TONE_MAP, 0 },
	{ "sc8850",      SC_8850_TONE_MAP, 0 },
	{ "sc8850drum",  SC_8850_DRUM_MAP, 1 },
	{ "sc88drum",    SC_88_DRUM_MAP, 1 },
	{ "sc88pro",     SC_88PRO_TONE_MAP, 0 },
	{ "sc88prodrum", SC_88PRO_DRUM_MAP, 1 },
	{ "xg",          XG_NORMAL_MAP, 0 },
	{ "xgdrum",      XG_DRUM_MAP, 1 },
	{ "xgsfx126",    XG_SFX126_MAP, 1 },
	{ "xgsfx64",     XG_SFX64_MAP, 0 }
	};
	const MapNameEntry *found;

	found = (MapNameEntry *)bsearch(name, data, sizeof data / sizeof data[0], sizeof data[0], mapnamecompare);
	if (found != NULL)
	{
		*isdrum = found->isdrum;
		return found->mapid;
	}
	return -1;
}

static float *config_parse_tune(const char *cp, int *num)
{
	const char *p;
	float *tune_list;
	int i;

	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	tune_list = (float *)safe_malloc((*num) * sizeof(float));
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		tune_list[i] = atof(p);
		if (!(p = strchr(p, ',')))
			break;
	}
	return tune_list;
}

static int16_t *config_parse_int16(const char *cp, int *num)
{
	const char *p;
	int16_t *list;
	int i;

	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	list = (int16_t *)safe_malloc((*num) * sizeof(int16_t));
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		list[i] = atoi(p);
		if (!(p = strchr(p, ',')))
			break;
	}
	return list;
}

static int **config_parse_envelope(const char *cp, int *num)
{
	const char *p, *px;
	int **env_list;
	int i, j;

	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	env_list = (int **)safe_malloc((*num) * sizeof(int *));
	for (i = 0; i < *num; i++)
		env_list[i] = (int *)safe_malloc(6 * sizeof(int));
	/* init */
	for (i = 0; i < *num; i++)
		for (j = 0; j < 6; j++)
			env_list[i][j] = -1;
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		px = strchr(p, ',');
		for (j = 0; j < 6; j++, p++) {
			if (*p == ':')
				continue;
			env_list[i][j] = atoi(p);
			if (!(p = strchr(p, ':')))
				break;
			if (px && p > px)
				break;
		}
		if (!(p = px))
			break;
	}
	return env_list;
}

static Quantity **config_parse_modulation(const char *name, int line, const char *cp, int *num, int mod_type)
{
	const char *p, *px, *err;
	char buf[128], *delim;
	Quantity **mod_list;
	int i, j;
	static const char * qtypestr[] = { "tremolo", "vibrato" };
	static const uint16_t qtypes[] = {
		QUANTITY_UNIT_TYPE(TREMOLO_SWEEP), QUANTITY_UNIT_TYPE(TREMOLO_RATE), QUANTITY_UNIT_TYPE(DIRECT_INT),
		QUANTITY_UNIT_TYPE(VIBRATO_SWEEP), QUANTITY_UNIT_TYPE(VIBRATO_RATE), QUANTITY_UNIT_TYPE(DIRECT_INT)
	};

	/* count num */
	*num = 1, p = cp;
	while ((p = strchr(p, ',')) != NULL)
		(*num)++, p++;
	/* alloc */
	mod_list = (Quantity **)safe_malloc((*num) * sizeof(Quantity *));
	for (i = 0; i < *num; i++)
		mod_list[i] = (Quantity *)safe_malloc(3 * sizeof(Quantity));
	/* init */
	for (i = 0; i < *num; i++)
		for (j = 0; j < 3; j++)
			INIT_QUANTITY(mod_list[i][j]);
	buf[sizeof buf - 1] = '\0';
	/* regist */
	for (i = 0, p = cp; i < *num; i++, p++) {
		px = strchr(p, ',');
		for (j = 0; j < 3; j++, p++) {
			if (*p == ':')
				continue;
			if ((delim = strpbrk(strncpy(buf, p, sizeof buf - 1), ":,")) != NULL)
				*delim = '\0';
			if (*buf != '\0' && (err = string_to_quantity(buf, &mod_list[i][j], qtypes[mod_type * 3 + j])) != NULL) {
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: %s: parameter %d of item %d: %s (%s)",
					name, line, qtypestr[mod_type], j + 1, i + 1, err, buf);
				free_ptr_list(mod_list, *num);
				mod_list = NULL;
				*num = 0;
				return NULL;
			}
			if (!(p = strchr(p, ':')))
				break;
			if (px && p > px)
				break;
		}
		if (!(p = px))
			break;
	}
	return mod_list;
}


/*! copy bank and, if necessary, map appropriately */
void Instruments::copybank(ToneBank *to, ToneBank *from, int mapid, int bankmapfrom, int bankno)
{
	ToneBankElement *toelm, *fromelm;
	int i;

	if (from == NULL)
		return;
	for (i = 0; i < 128; i++)
	{
		toelm = &to->tone[i];
		fromelm = &from->tone[i];
		if (fromelm->name == NULL)
			continue;
		copy_tone_bank_element(toelm, fromelm);
		toelm->instrument = NULL;
		if (mapid != INST_NO_MAP)
			set_instrument_map(mapid, bankmapfrom, i, bankno, i);
	}
}

/*! copy the whole mapped bank. returns 0 if no error. */
int Instruments::copymap(int mapto, int mapfrom, int isdrum)
{
	ToneBank **tb = isdrum ? drumset : tonebank;
	int i, bankfrom, bankto;

	for (i = 0; i < 128; i++)
	{
		bankfrom = find_instrument_map_bank(isdrum, mapfrom, i);
		if (bankfrom <= 0) /* not mapped */
			continue;
		bankto = alloc_instrument_map_bank(isdrum, mapto, i);
		if (bankto == -1) /* failed */
			return 1;
		copybank(tb[bankto], tb[bankfrom], mapto, i, bankto);
	}
	return 0;
}

int Instruments::set_gus_patchconf_opts(const char *name, int line, char *opts, ToneBankElement *tone)
{
	char *cp;
	int k;

	if (!(cp = strchr(opts, '='))) {
		ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
			"%s: line %d: bad patch option %s", name, line, opts);
		return 1;
	}
	*cp++ = 0;
	if (!strcmp(opts, "amp")) {
		k = atoi(cp);
		if ((k < 0 || k > MAX_AMPLIFICATION) || (*cp < '0' || *cp > '9')) {
			ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: amplification must be between 0 and %d",
				name, line, MAX_AMPLIFICATION);
			return 1;
		}
		tone->amp = k;
	}
	else if (!strcmp(opts, "note")) {
		k = atoi(cp);
		if ((k < 0 || k > 127) || (*cp < '0' || *cp > '9')) {
			ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: note must be between 0 and 127",
				name, line);
			return 1;
		}
		tone->note = k;
		tone->scltune = config_parse_int16("100", &tone->scltunenum);
	}
	else if (!strcmp(opts, "pan")) {
		if (!strcmp(cp, "center"))
			k = 64;
		else if (!strcmp(cp, "left"))
			k = 0;
		else if (!strcmp(cp, "right"))
			k = 127;
		else {
			k = ((atoi(cp) + 100) * 100) / 157;
			if ((k < 0 || k > 127)
				|| (k == 0 && *cp != '-' && (*cp < '0' || *cp > '9'))) {
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: panning must be left, right, "
					"center, or between -100 and 100",
					name, line);
				return 1;
			}
		}
		tone->pan = k;
	}
	else if (!strcmp(opts, "tune"))
		tone->tune = config_parse_tune(cp, &tone->tunenum);
	else if (!strcmp(opts, "rate"))
		tone->envrate = config_parse_envelope(cp, &tone->envratenum);
	else if (!strcmp(opts, "offset"))
		tone->envofs = config_parse_envelope(cp, &tone->envofsnum);
	else if (!strcmp(opts, "keep")) {
		if (!strcmp(cp, "env"))
			tone->strip_envelope = 0;
		else if (!strcmp(cp, "loop"))
			tone->strip_loop = 0;
		else {
			ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: keep must be env or loop", name, line);
			return 1;
		}
	}
	else if (!strcmp(opts, "strip")) {
		if (!strcmp(cp, "env"))
			tone->strip_envelope = 1;
		else if (!strcmp(cp, "loop"))
			tone->strip_loop = 1;
		else if (!strcmp(cp, "tail"))
			tone->strip_tail = 1;
		else {
			ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: strip must be env, loop, or tail",
				name, line);
			return 1;
		}
	}
	else if (!strcmp(opts, "tremolo")) {
		if ((tone->trem = config_parse_modulation(name,
			line, cp, &tone->tremnum, 0)) == NULL)
			return 1;
	}
	else if (!strcmp(opts, "vibrato")) {
		if ((tone->vib = config_parse_modulation(name,
			line, cp, &tone->vibnum, 1)) == NULL)
			return 1;
	}
	else if (!strcmp(opts, "sclnote"))
		tone->sclnote = config_parse_int16(cp, &tone->sclnotenum);
	else if (!strcmp(opts, "scltune"))
		tone->scltune = config_parse_int16(cp, &tone->scltunenum);
	else if (!strcmp(opts, "comm")) {
		char *p;

		if (tone->comment)
			free(tone->comment);
		p = tone->comment = safe_strdup(cp);
		while (*p) {
			if (*p == ',')
				*p = ' ';
			p++;
		}
	}
	else if (!strcmp(opts, "modrate"))
		tone->modenvrate = config_parse_envelope(cp, &tone->modenvratenum);
	else if (!strcmp(opts, "modoffset"))
		tone->modenvofs = config_parse_envelope(cp, &tone->modenvofsnum);
	else if (!strcmp(opts, "envkeyf"))
		tone->envkeyf = config_parse_envelope(cp, &tone->envkeyfnum);
	else if (!strcmp(opts, "envvelf"))
		tone->envvelf = config_parse_envelope(cp, &tone->envvelfnum);
	else if (!strcmp(opts, "modkeyf"))
		tone->modenvkeyf = config_parse_envelope(cp, &tone->modenvkeyfnum);
	else if (!strcmp(opts, "modvelf"))
		tone->modenvvelf = config_parse_envelope(cp, &tone->modenvvelfnum);
	else if (!strcmp(opts, "trempitch"))
		tone->trempitch = config_parse_int16(cp, &tone->trempitchnum);
	else if (!strcmp(opts, "tremfc"))
		tone->tremfc = config_parse_int16(cp, &tone->tremfcnum);
	else if (!strcmp(opts, "modpitch"))
		tone->modpitch = config_parse_int16(cp, &tone->modpitchnum);
	else if (!strcmp(opts, "modfc"))
		tone->modfc = config_parse_int16(cp, &tone->modfcnum);
	else if (!strcmp(opts, "fc"))
		tone->fc = config_parse_int16(cp, &tone->fcnum);
	else if (!strcmp(opts, "q"))
		tone->reso = config_parse_int16(cp, &tone->resonum);
	else if (!strcmp(opts, "fckeyf"))		/* filter key-follow */
		tone->key_to_fc = atoi(cp);
	else if (!strcmp(opts, "fcvelf"))		/* filter velocity-follow */
		tone->vel_to_fc = atoi(cp);
	else if (!strcmp(opts, "qvelf"))		/* resonance velocity-follow */
		tone->vel_to_resonance = atoi(cp);
	else {
		ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
			"%s: line %d: bad patch option %s",
			name, line, opts);
		return 1;
	}
	return 0;
}


void Instruments::reinit_tone_bank_element(ToneBankElement *tone)
{
	free_tone_bank_element(tone);
	tone->note = tone->pan = -1;
	tone->strip_loop = tone->strip_envelope = tone->strip_tail = -1;
	tone->amp = -1;
	tone->rnddelay = 0;
	tone->loop_timeout = 0;
	tone->legato = tone->damper_mode = tone->key_to_fc = tone->vel_to_fc = 0;
	tone->reverb_send = tone->chorus_send = tone->delay_send = -1;
	tone->tva_level = -1;
	tone->play_note = -1;
}


int Instruments::set_gus_patchconf(const char *name, int line, ToneBankElement *tone, char *pat, char **opts)
{
	int j;
	reinit_tone_bank_element(tone);

	if (strcmp(pat, "%font") == 0) /* Font extention */
	{
		/* %font filename bank prog [note-to-use]
		* %font filename 128 bank key
		*/

		if (opts[0] == NULL || opts[1] == NULL || opts[2] == NULL ||
			(atoi(opts[1]) == 128 && opts[3] == NULL))
		{
			ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: Syntax error", name, line);
			return 1;
		}
		tone->name = safe_strdup(opts[0]);
		tone->instype = 1;
		if (atoi(opts[1]) == 128) /* drum */
		{
			tone->font_bank = 128;
			tone->font_preset = atoi(opts[2]);
			tone->font_keynote = atoi(opts[3]);
			opts += 4;
		}
		else
		{
			tone->font_bank = atoi(opts[1]);
			tone->font_preset = atoi(opts[2]);

			if (opts[3] && isdigit(opts[3][0]))
			{
				tone->font_keynote = atoi(opts[3]);
				opts += 4;
			}
			else
			{
				tone->font_keynote = -1;
				opts += 3;
			}
		}
	}
	else if (strcmp(pat, "%sample") == 0) /* Sample extention */
	{
		/* %sample filename */

		if (opts[0] == NULL)
		{
			ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: Syntax error", name, line);
			return 1;
		}
		tone->name = safe_strdup(opts[0]);
		tone->instype = 2;
		opts++;
	}
	else
	{
		tone->instype = 0;
		tone->name = safe_strdup(pat);
	}

	for (j = 0; opts[j] != NULL; j++)
	{
		int err;
		if ((err = set_gus_patchconf_opts(name, line, opts[j], tone)) != 0)
			return err;
	}
	if (tone->comment == NULL)
		tone->comment = safe_strdup(tone->name);
	return 0;
}



int Instruments::set_patchconf(const char *name, int line, ToneBank *bank, char *w[], int dr, int mapid, int bankmapfrom, int bankno)
{
	int i;

	i = atoi(w[0]);
	if (!dr)
		i -= progbase;
	if (i < 0 || i > 127)
	{
		if (dr)
			ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: Drum number must be between "
				"0 and 127",
				name, line);
		else
			ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
				"%s: line %d: Program must be between "
				"%d and %d",
				name, line, progbase, 127 + progbase);
		return 1;
	}
	if (!bank)
	{
		ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
			"%s: line %d: Must specify tone bank or drum set "
			"before assignment", name, line);
		return 1;
	}

	if (set_gus_patchconf(name, line, &bank->tone[i], w[1], w + 2))
		return 1;
	if (mapid != INST_NO_MAP)
		set_instrument_map(mapid, bankmapfrom, i, bankno, i);
	return 0;
}



/* string[0] should not be '#' */
int Instruments::strip_trailing_comment(char *string, int next_token_index)
{
	if (string[next_token_index - 1] == '#'	/* strip \1 in /^\S+(#*[ \t].*)/ */
		&& (string[next_token_index] == ' ' || string[next_token_index] == '\t'))
	{
		string[next_token_index] = '\0';	/* new c-string terminator */
		while (string[--next_token_index - 1] == '#')
			;
	}
	return next_token_index;
}

char *Instruments::expand_variables(char *string, MBlockList *varbuf, const char *basedir)
{
	char *p, *expstr;
	const char *copystr;
	int limlen, copylen, explen, varlen, braced;

	if ((p = strchr(string, '$')) == NULL)
		return string;
	varlen = (int)strlen(basedir);
	explen = limlen = 0;
	expstr = NULL;
	copystr = string;
	copylen = p - string;
	string = p;
	for (;;)
	{
		if (explen + copylen + 1 > limlen)
		{
			limlen += copylen + 128;
			expstr = (char*)memcpy(new_segment(varbuf, limlen), expstr, explen);
		}
		memcpy(&expstr[explen], copystr, copylen);
		explen += copylen;
		if (*string == '\0')
			break;
		else if (*string == '$')
		{
			braced = *++string == '{';
			if (braced)
			{
				if ((p = strchr(string + 1, '}')) == NULL)
					p = string;	/* no closing brace */
				else
					string++;
			}
			else
				for (p = string; isalnum(*p) || *p == '_'; p++);
			if (p == string)	/* empty */
			{
				copystr = "${";
				copylen = 1 + braced;
			}
			else
			{
				if (p - string == 7 && memcmp(string, "basedir", 7) == 0)
				{
					copystr = basedir;
					copylen = varlen;
				}
				else	/* undefined variable */
					copylen = 0;
				string = p + braced;
			}
		}
		else	/* search next */
		{
			p = strchr(string, '$');
			if (p == NULL)
				copylen = (int)strlen(string);
			else
				copylen = int(p - string);
			copystr = string;
			string += copylen;
		}
	}
	expstr[explen] = '\0';
	return expstr;
}


int Instruments::read_config_file(const char *name, int self, int allow_missing_file)
{
	struct timidity_file *tf;
	char buf[1024], *tmp, *w[MAXWORDS + 1], *cp;
	ToneBank *bank = NULL;
	int i, j, k, line = 0, words, errcnt = 0;
	static int rcf_count = 0;
	int dr = 0, bankno = 0, mapid = INST_NO_MAP, origbankno = 0x7FFFFFFF;
	int extension_flag, param_parse_err;
	MBlockList varbuf;
	const char *basedir;
	char *sep;

	if (rcf_count > 50)
	{
		ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
			"Probable source loop in configuration files");
		return READ_CONFIG_RECURSION;
	}

	tf = open_file(name, sfreader);
	if (tf == NULL)
		return allow_missing_file ? READ_CONFIG_FILE_NOT_FOUND :
		READ_CONFIG_ERROR;

	init_mblock(&varbuf);
	if (!self)
	{
		basedir = strdup_mblock(&varbuf, tf->filename.c_str());
		FixPathSeperator((char*)basedir);
		sep = (char*)strrchr(basedir, '/');
	}
	else
		sep = NULL;
	if (sep == NULL)
	{
		basedir = ".";
	}
	else
	{
		if ((cp = (char*)strchr(sep, '#')) != NULL)
			sep = cp + 1;	/* inclusive of '#' */
		*sep = '\0';
	}

	while (tf_gets(buf, sizeof(buf), tf))
	{
		line++;
		if (strncmp(buf, "#extension", 10) == 0) {
			extension_flag = 1;
			i = 10;
		}
		else
		{
			extension_flag = 0;
			i = 0;
		}

		while (isspace(buf[i]))			/* skip /^\s*(?#)/ */
			i++;
		if (buf[i] == '#' || buf[i] == '\0')	/* /^#|^$/ */
			continue;
		tmp = expand_variables(buf, &varbuf, basedir);
		j = (int)strcspn(tmp + i, " \t\r\n\240");
		if (j == 0)
			j = (int)strlen(tmp + i);
		j = strip_trailing_comment(tmp + i, j);
		tmp[i + j] = '\0';			/* terminate the first token */
		w[0] = tmp + i;
		i += j + 1;
		words = param_parse_err = 0;
		while (words < MAXWORDS - 1)		/* -1 : next arg */
		{
			char *terminator;

			while (isspace(tmp[i]))		/* skip /^\s*(?#)/ */
				i++;
			if (tmp[i] == '\0'
				|| tmp[i] == '#')		/* /\s#/ */
				break;
			if ((tmp[i] == '"' || tmp[i] == '\'')
				&& (terminator = strchr(tmp + i + 1, tmp[i])) != NULL)
			{
				if (!isspace(terminator[1]) && terminator[1] != '\0')
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: there must be at least one whitespace between "
						"string terminator (%c) and the next parameter", name, line, tmp[i]);
					CHECKERRLIMIT;
					param_parse_err = 1;
					break;
				}
				w[++words] = tmp + i + 1;
				i = terminator - tmp + 1;
				*terminator = '\0';
			}
			else	/* not terminated */
			{
				j = (int)strcspn(tmp + i, " \t\r\n\240");
				if (j > 0)
					j = strip_trailing_comment(tmp + i, j);
				w[++words] = tmp + i;
				i += j;
				if (tmp[i] != '\0')		/* unless at the end-of-string (i.e. EOF) */
					tmp[i++] = '\0';		/* terminate the token */
			}
		}
		if (param_parse_err)
			continue;
		w[++words] = NULL;

		/*
		* #extension [something...]
		*/

		/* #extension timeout program sec */
		if (strcmp(w[0], "timeout") == 0)
		{
			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension timeout "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			bank->tone[i].loop_timeout = atoi(w[2]);
		}
		/* #extension copydrumset drumset */
		else if (strcmp(w[0], "copydrumset") == 0)
		{
			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No copydrumset number given",
					name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension copydrumset "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or "
					"drum set before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			copybank(bank, drumset[i], mapid, origbankno, bankno);
		}
		/* #extension copybank bank */
		else if (strcmp(w[0], "copybank") == 0)
		{
			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No copybank number given",
					name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension copybank "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or "
					"drum set before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			copybank(bank, tonebank[i], mapid, origbankno, bankno);
		}
		/* #extension copymap tomapid frommapid */
		else if (strcmp(w[0], "copymap") == 0)
		{
			int mapto, mapfrom;
			int toisdrum, fromisdrum;

			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if ((mapto = mapname2id(w[1], &toisdrum)) == -1)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Invalid map name: %s", name, line, w[1]);
				CHECKERRLIMIT;
				continue;
			}
			if ((mapfrom = mapname2id(w[2], &fromisdrum)) == -1)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Invalid map name: %s", name, line, w[2]);
				CHECKERRLIMIT;
				continue;
			}
			if (toisdrum != fromisdrum)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Map type should be matched", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (copymap(mapto, mapfrom, toisdrum))
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No free %s available to map",
					name, line, toisdrum ? "drum set" : "tone bank");
				CHECKERRLIMIT;
				continue;
			}
		}
		/* #extension undef program */
		else if (strcmp(w[0], "undef") == 0)
		{
			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No undef number given",
					name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension undef "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or "
					"drum set before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			free_tone_bank_element(&bank->tone[i]);
		}
		/* #extension altassign numbers... */
		else if (strcmp(w[0], "altassign") == 0)
		{
			ToneBank *bk;

			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before altassign", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No alternate assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}

			if (!dr) {
				ctl_cmsg(CMSG_WARNING, VERB_NORMAL,
					"%s: line %d: Warning: Not a drumset altassign"
					" (ignored)",
					name, line);
				continue;
			}

			bk = drumset[bankno];
			bk->alt = add_altassign_string(bk->alt, w + 1, words - 1);
		}	/* #extension legato [program] [0 or 1] */
		else if (strcmp(w[0], "legato") == 0)
		{
			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension legato "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			bank->tone[i].legato = atoi(w[2]);
		}	/* #extension damper [program] [0 or 1] */
		else if (strcmp(w[0], "damper") == 0)
		{
			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension damper "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			bank->tone[i].damper_mode = atoi(w[2]);
		}	/* #extension rnddelay [program] [0 or 1] */
		else if (strcmp(w[0], "rnddelay") == 0)
		{
			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension rnddelay "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			bank->tone[i].rnddelay = atoi(w[2]);
		}	/* #extension level program tva_level */
		else if (strcmp(w[0], "level") == 0)
		{
			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[2]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension level "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			cp = w[1];
			do {
				if (string_to_7bit_range(cp, &j, &k))
				{
					while (j <= k)
						bank->tone[j++].tva_level = i;
				}
				cp = strchr(cp, ',');
			} while (cp++ != NULL);
		}	/* #extension reverbsend */
		else if (strcmp(w[0], "reverbsend") == 0)
		{
			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[2]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension reverbsend "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			cp = w[1];
			do {
				if (string_to_7bit_range(cp, &j, &k))
				{
					while (j <= k)
						bank->tone[j++].reverb_send = i;
				}
				cp = strchr(cp, ',');
			} while (cp++ != NULL);
		}	/* #extension chorussend */
		else if (strcmp(w[0], "chorussend") == 0)
		{
			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[2]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension chorussend "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			cp = w[1];
			do {
				if (string_to_7bit_range(cp, &j, &k))
				{
					while (j <= k)
						bank->tone[j++].chorus_send = i;
				}
				cp = strchr(cp, ',');
			} while (cp++ != NULL);
		}	/* #extension delaysend */
		else if (strcmp(w[0], "delaysend") == 0)
		{
			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[2]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension delaysend "
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			cp = w[1];
			do {
				if (string_to_7bit_range(cp, &j, &k))
				{
					while (j <= k)
						bank->tone[j++].delay_send = i;
				}
				cp = strchr(cp, ',');
			} while (cp++ != NULL);
		}	/* #extension playnote */
		else if (strcmp(w[0], "playnote") == 0)
		{
			if (words != 3)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL, "%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!bank)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify tone bank or drum set "
					"before assignment", name, line);
				CHECKERRLIMIT;
				continue;
			}
			i = atoi(w[2]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: extension playnote"
					"must be between 0 and 127", name, line);
				CHECKERRLIMIT;
				continue;
			}
			cp = w[1];
			do {
				if (string_to_7bit_range(cp, &j, &k))
				{
					while (j <= k)
						bank->tone[j++].play_note = i;
				}
				cp = strchr(cp, ',');
			} while (cp++ != NULL);
		}
		else if (!strcmp(w[0], "soundfont"))
		{
			int order, cutoff, isremove, reso, amp;
			char *sf_file;

			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No soundfont file given",
					name, line);
				CHECKERRLIMIT;
				continue;
			}

			sf_file = w[1];
			order = cutoff = reso = amp = -1;
			isremove = 0;
			for (j = 2; j < words; j++)
			{
				if (strcmp(w[j], "remove") == 0)
				{
					isremove = 1;
					break;
				}
				if (!(cp = strchr(w[j], '=')))
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: bad patch option %s",
						name, line, w[j]);
					CHECKERRLIMIT;
					break;
				}
				*cp++ = 0;
				k = atoi(cp);
				if (!strcmp(w[j], "order"))
				{
					if (k < 0 || (*cp < '0' || *cp > '9'))
					{
						ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
							"%s: line %d: order must be a digit",
							name, line);
						CHECKERRLIMIT;
						break;
					}
					order = k;
				}
				else if (!strcmp(w[j], "cutoff"))
				{
					if (k < 0 || (*cp < '0' || *cp > '9'))
					{
						ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
							"%s: line %d: cutoff must be a digit",
							name, line);
						CHECKERRLIMIT;
						break;
					}
					cutoff = k;
				}
				else if (!strcmp(w[j], "reso"))
				{
					if (k < 0 || (*cp < '0' || *cp > '9'))
					{
						ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
							"%s: line %d: reso must be a digit",
							name, line);
						CHECKERRLIMIT;
						break;
					}
					reso = k;
				}
				else if (!strcmp(w[j], "amp"))
				{
					amp = k;
				}
			}
			if (isremove)
				remove_soundfont(sf_file);
			else
				add_soundfont(sf_file, order, cutoff, reso, amp);
		}
		else if (!strcmp(w[0], "font"))
		{
			int bank, preset, keynote;
			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: no font command", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (!strcmp(w[1], "exclude"))
			{
				if (words < 3)
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: No bank/preset/key is given",
						name, line);
					CHECKERRLIMIT;
					continue;
				}
				bank = atoi(w[2]);
				if (words >= 4)
					preset = atoi(w[3]) - progbase;
				else
					preset = -1;
				if (words >= 5)
					keynote = atoi(w[4]);
				else
					keynote = -1;
				if (exclude_soundfont(bank, preset, keynote))
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: No soundfont is given",
						name, line);
					CHECKERRLIMIT;
				}
			}
			else if (!strcmp(w[1], "order"))
			{
				int order;
				if (words < 4)
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: No order/bank is given",
						name, line);
					CHECKERRLIMIT;
					continue;
				}
				order = atoi(w[2]);
				bank = atoi(w[3]);
				if (words >= 5)
					preset = atoi(w[4]) - progbase;
				else
					preset = -1;
				if (words >= 6)
					keynote = atoi(w[5]);
				else
					keynote = -1;
				if (order_soundfont(bank, preset, keynote, order))
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: No soundfont is given",
						name, line);
					CHECKERRLIMIT;
				}
			}
		}
		else if (!strcmp(w[0], "progbase"))
		{
			if (words < 2 || *w[1] < '0' || *w[1] > '9')
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			progbase = atoi(w[1]);
		}
		else if (!strcmp(w[0], "map")) /* map <name> set1 elem1 set2 elem2 */
		{
			int arg[5], isdrum;

			if (words != 6)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if ((arg[0] = mapname2id(w[1], &isdrum)) == -1)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Invalid map name: %s", name, line, w[1]);
				CHECKERRLIMIT;
				continue;
			}
			for (i = 2; i < 6; i++)
				arg[i - 1] = atoi(w[i]);
			if (isdrum)
			{
				arg[1] -= progbase;
				arg[3] -= progbase;
			}
			else
			{
				arg[2] -= progbase;
				arg[4] -= progbase;
			}

			for (i = 1; i < 5; i++)
				if (arg[i] < 0 || arg[i] > 127)
					break;
			if (i != 5)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Invalid parameter", name, line);
				CHECKERRLIMIT;
				continue;
			}
			set_instrument_map(arg[0], arg[1], arg[2], arg[3], arg[4]);
		}

		/*
		* Standard configurations
		*/
		else if (!strcmp(w[0], "dir"))
		{
			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No directory given", name, line);
				CHECKERRLIMIT;
				continue;
			}
			for (i = 1; i < words; i++)
                sfreader->AddPath(w[i]);
		}
		else if (!strcmp(w[0], "source") || !strcmp(w[0], "trysource"))
		{
			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No file name given", name, line);
				CHECKERRLIMIT;
				continue;
			}
			for (i = 1; i < words; i++)
			{
				int status;
				rcf_count++;
				status = read_config_file(w[i], 0, !strcmp(w[0], "trysource"));
				rcf_count--;
				switch (status) {
				case READ_CONFIG_SUCCESS:
					break;
				case READ_CONFIG_ERROR:
					CHECKERRLIMIT;
					continue;
				case READ_CONFIG_RECURSION:
					reuse_mblock(&varbuf);
					tf_close(tf);
					return READ_CONFIG_RECURSION;
				case READ_CONFIG_FILE_NOT_FOUND:
					break;
				}
			}
		}
		else if (!strcmp(w[0], "default"))
		{
			if (words != 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Must specify exactly one patch name",
					name, line);
				CHECKERRLIMIT;
				continue;
			}
			strncpy(def_instr_name, w[1], 255);
			def_instr_name[255] = '\0';
			default_instrument_name = def_instr_name;
		}
		/* drumset [mapid] num */
		else if (!strcmp(w[0], "drumset"))
		{
			int newmapid, isdrum, newbankno;

			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No drum set number given", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (words != 2 && !isdigit(*w[1]))
			{
				if ((newmapid = mapname2id(w[1], &isdrum)) == -1 || !isdrum)
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: Invalid drum set map name: %s", name, line, w[1]);
					CHECKERRLIMIT;
					continue;
				}
				words--;
				memmove(&w[1], &w[2], sizeof w[0] * words);
			}
			else
				newmapid = INST_NO_MAP;
			i = atoi(w[1]) - progbase;
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Drum set must be between %d and %d",
					name, line,
					progbase, progbase + 127);
				CHECKERRLIMIT;
				continue;
			}

			newbankno = i;
			i = alloc_instrument_map_bank(1, newmapid, i);
			if (i == -1)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No free drum set available to map",
					name, line);
				CHECKERRLIMIT;
				continue;
			}

			if (words == 2)
			{
				bank = drumset[i];
				bankno = i;
				mapid = newmapid;
				origbankno = newbankno;
				dr = 1;
			}
			else
			{
				if (words < 4 || *w[2] < '0' || *w[2] > '9')
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: syntax error", name, line);
					CHECKERRLIMIT;
					continue;
				}
				if (set_patchconf(name, line, drumset[i], &w[2], 1, newmapid, newbankno, i))
				{
					CHECKERRLIMIT;
					continue;
				}
			}
		}
		/* bank [mapid] num */
		else if (!strcmp(w[0], "bank"))
		{
			int newmapid, isdrum, newbankno;

			if (words < 2)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No bank number given", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (words != 2 && !isdigit(*w[1]))
			{
				if ((newmapid = mapname2id(w[1], &isdrum)) == -1 || isdrum)
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: Invalid bank map name: %s", name, line, w[1]);
					CHECKERRLIMIT;
					continue;
				}
				words--;
				memmove(&w[1], &w[2], sizeof w[0] * words);
			}
			else
				newmapid = INST_NO_MAP;
			i = atoi(w[1]);
			if (i < 0 || i > 127)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: Tone bank must be between 0 and 127",
					name, line);
				CHECKERRLIMIT;
				continue;
			}

			newbankno = i;
			i = alloc_instrument_map_bank(0, newmapid, i);
			if (i == -1)
			{
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: No free tone bank available to map",
					name, line);
				CHECKERRLIMIT;
				continue;
			}

			if (words == 2)
			{
				bank = tonebank[i];
				bankno = i;
				mapid = newmapid;
				origbankno = newbankno;
				dr = 0;
			}
			else
			{
				if (words < 4 || *w[2] < '0' || *w[2] > '9')
				{
					ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
						"%s: line %d: syntax error", name, line);
					CHECKERRLIMIT;
					continue;
				}
				if (set_patchconf(name, line, tonebank[i], &w[2], 0, newmapid, newbankno, i))
				{
					CHECKERRLIMIT;
					continue;
				}
			}
		}
		else
		{
			if (words < 2 || *w[0] < '0' || *w[0] > '9')
			{
				if (extension_flag)
					continue;
				ctl_cmsg(CMSG_ERROR, VERB_NORMAL,
					"%s: line %d: syntax error", name, line);
				CHECKERRLIMIT;
				continue;
			}
			if (set_patchconf(name, line, bank, w, dr, mapid, origbankno, bankno))
			{
				CHECKERRLIMIT;
				continue;
			}
		}
	}
	reuse_mblock(&varbuf);
	tf_close(tf);
	return (errcnt == 0) ? READ_CONFIG_SUCCESS : READ_CONFIG_ERROR;
}

}
