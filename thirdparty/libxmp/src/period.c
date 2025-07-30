/* Extended Module Player
 * Copyright (C) 1996-2021 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "common.h"
#include "period.h"

#include <math.h>

#ifdef LIBXMP_PAULA_SIMULATOR
/*
 * Period table from the Protracker V2.1A play routine
 */
static uint16 pt_period_table[16][36] = {
	/* Tuning 0, Normal */
	{
		856,808,762,720,678,640,604,570,538,508,480,453,
		428,404,381,360,339,320,302,285,269,254,240,226,
		214,202,190,180,170,160,151,143,135,127,120,113
	},
	/* Tuning 1 */
	{
		850,802,757,715,674,637,601,567,535,505,477,450,
		425,401,379,357,337,318,300,284,268,253,239,225,
		213,201,189,179,169,159,150,142,134,126,119,113
	},
	/* Tuning 2 */
	{
		844,796,752,709,670,632,597,563,532,502,474,447,
		422,398,376,355,335,316,298,282,266,251,237,224,
		211,199,188,177,167,158,149,141,133,125,118,112
	},
	/* Tuning 3 */
	{
		838,791,746,704,665,628,592,559,528,498,470,444,
		419,395,373,352,332,314,296,280,264,249,235,222,
		209,198,187,176,166,157,148,140,132,125,118,111
	},
	/* Tuning 4 */
	{
		832,785,741,699,660,623,588,555,524,495,467,441,
		416,392,370,350,330,312,294,278,262,247,233,220,
		208,196,185,175,165,156,147,139,131,124,117,110
	},
	/* Tuning 5 */
	{
		826,779,736,694,655,619,584,551,520,491,463,437,
		413,390,368,347,328,309,292,276,260,245,232,219,
		206,195,184,174,164,155,146,138,130,123,116,109
	},
	/* Tuning 6 */
	{
		820,774,730,689,651,614,580,547,516,487,460,434,
		410,387,365,345,325,307,290,274,258,244,230,217,
		205,193,183,172,163,154,145,137,129,122,115,109
	},
	/* Tuning 7 */
	{
		814,768,725,684,646,610,575,543,513,484,457,431,
		407,384,363,342,323,305,288,272,256,242,228,216,
		204,192,181,171,161,152,144,136,128,121,114,108
	},
	/* Tuning -8 */
	{
		907,856,808,762,720,678,640,604,570,538,508,480,
		453,428,404,381,360,339,320,302,285,269,254,240,
		226,214,202,190,180,170,160,151,143,135,127,120
	},
	/* Tuning -7 */
	{
		900,850,802,757,715,675,636,601,567,535,505,477,
		450,425,401,379,357,337,318,300,284,268,253,238,
		225,212,200,189,179,169,159,150,142,134,126,119
	},
	/* Tuning -6 */
	{
		894,844,796,752,709,670,632,597,563,532,502,474,
		447,422,398,376,355,335,316,298,282,266,251,237,
		223,211,199,188,177,167,158,149,141,133,125,118
	},
	/* Tuning -5 */
	{
		887,838,791,746,704,665,628,592,559,528,498,470,
		444,419,395,373,352,332,314,296,280,264,249,235,
		222,209,198,187,176,166,157,148,140,132,125,118
	},
	/* Tuning -4 */
	{
		881,832,785,741,699,660,623,588,555,524,494,467,
		441,416,392,370,350,330,312,294,278,262,247,233,
		220,208,196,185,175,165,156,147,139,131,123,117
	},
	/* Tuning -3 */
	{
		875,826,779,736,694,655,619,584,551,520,491,463,
		437,413,390,368,347,328,309,292,276,260,245,232,
		219,206,195,184,174,164,155,146,138,130,123,116
	},
	/* Tuning -2 */
	{
		868,820,774,730,689,651,614,580,547,516,487,460,
		434,410,387,365,345,325,307,290,274,258,244,230,
		217,205,193,183,172,163,154,145,137,129,122,115
	},
	/* Tuning -1 */
	{
		862,814,768,725,684,646,610,575,543,513,484,457,
		431,407,384,363,342,323,305,288,272,256,242,228,
		216,203,192,181,171,161,152,144,136,128,121,114
	}
};
#endif

#ifndef M_LN2
#define M_LN2	0.69314718055994530942
#endif

static inline double libxmp_round(double val)
{
	return (val >= 0.0)? floor(val + 0.5) : ceil(val - 0.5);
}

#ifdef LIBXMP_PAULA_SIMULATOR
/* Get period from note using Protracker tuning */
static inline int libxmp_note_to_period_pt(int n, int f)
{
	if (n < MIN_NOTE_MOD || n > MAX_NOTE_MOD) {
		return -1;
	}

	n -= 48;
	f >>= 4;
	if (f < -8 || f > 7) {
		return 0;
	}

	if (f < 0) {
		f += 16;
	}

	return (int)pt_period_table[f][n];
}
#endif

/* Get period from note */
double libxmp_note_to_period(struct context_data *ctx, int n, int f, double adj)
{
	double d, per;
	struct module_data *m = &ctx->m;
#ifdef LIBXMP_PAULA_SIMULATOR
	struct player_data *p = &ctx->p;

	/* If mod replayer, modrng and Amiga mixing are active */
	if (p->flags & XMP_FLAGS_A500) {
		if (IS_AMIGA_MOD()) {
			return libxmp_note_to_period_pt(n, f);
		}
	}
#endif

	d = (double)n + (double)f / 128;

	switch (m->period_type) {
	case PERIOD_LINEAR:
		per = (240.0 - d) * 16;				/* Linear */
		break;
	case PERIOD_CSPD:
		per = 8363.0 * pow(2, n / 12.0) / 32 + f;	/* Hz */
		break;
	default:
		per = PERIOD_BASE / pow(2, d / 12);		/* Amiga */
	}

#ifndef LIBXMP_CORE_PLAYER
	if (adj > 0.1) {
		per *= adj;
	}
#endif

	return per;
}

/* For the software mixer */
double libxmp_note_to_period_mix(int n, int b)
{
	double d = (double)n + (double)b / 12800;
	return PERIOD_BASE / pow(2, d / 12);
}

/* Get note from period */
/* This function is used only by the MOD loader */
int libxmp_period_to_note(int p)
{
	if (p <= 0) {
		return 0;
	}

	return libxmp_round(12.0 * log(PERIOD_BASE / p) / M_LN2) + 1;
}

/* Get pitchbend from base note and amiga period */
int libxmp_period_to_bend(struct context_data *ctx, double p, int n, double adj)
{
	struct module_data *m = &ctx->m;
	double d;

	if (n == 0 || p < 0.1) {
		return 0;
	}

	switch (m->period_type) {
	case PERIOD_LINEAR:
		return 100 * (8 * (((240 - n) << 4) - p));
	case PERIOD_CSPD:
		d = libxmp_note_to_period(ctx, n, 0, adj);
		return libxmp_round(100.0 * (1536.0 / M_LN2) * log(p / d));
	default:
		/* Amiga */
		d = libxmp_note_to_period(ctx, n, 0, adj);
		return libxmp_round(100.0 * (1536.0 / M_LN2) * log(d / p));
	}
}

/* Convert finetune = 1200 * log2(C2SPD/8363))
 *
 *      c = (1200.0 * log(c2spd) - 1200.0 * log(c4_rate)) / M_LN2;
 *      xpo = c/100;
 *      fin = 128 * (c%100) / 100;
 */
void libxmp_c2spd_to_note(int c2spd, int *n, int *f)
{
	int c;

	if (c2spd <= 0) {
		*n = *f = 0;
		return;
	}

	c = (int)(1536.0 * log((double)c2spd / 8363) / M_LN2);
	*n = c / 128;
	*f = c % 128;
}

#ifndef LIBXMP_CORE_PLAYER
/* Gravis Ultrasound frequency increments in steps of Hz/1024, where Hz is the
 * current rate of the card and is dependent on the active channel count.
 * For <=14 channels, the rate is 44100. For 15 to 32 channels, the rate is
 * round(14 * 44100 / active_channels).
 */
static const double GUS_rates[19] = {
	/* <= 14 */ 44100.0,
	/* 15-20 */ 41160.0,  38587.5,  36317.65, 34300.0, 32494.74, 30870.0,
	/* 21-26 */ 29400.0,  28063.64, 26843.48, 25725.0, 24696.0,  23746.15,
	/* 27-32 */ 22866.67, 22050.0,  21289.66, 20580.0, 19916.13, 19294.75
};

/* Get a Gravis Ultrasound frequency offset in Hz for a given number of steps.
 */
double libxmp_gus_frequency_steps(int num_steps, int num_channels_active)
{
	CLAMP(num_channels_active, 14, 32);
	return (num_steps * GUS_rates[num_channels_active - 14]) / 1024.0;
}
#endif
