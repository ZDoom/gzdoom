/*
	common.h

	Midi Wavetable Processing library

    Copyright (C) Chris Ison 2001-2011
    Copyright (C) Bret Curtis 2013-2014

    This file is part of WildMIDI.

    WildMIDI is free software: you can redistribute and/or modify the player
    under the terms of the GNU General Public License and you can redistribute
    and/or modify the library under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation, either version 3 of
    the licenses, or(at your option) any later version.

    WildMIDI is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
    the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License and the
    GNU Lesser General Public License along with WildMIDI.  If not,  see
    <http://www.gnu.org/licenses/>.
*/

#ifndef __COMMON_H
#define __COMMON_H

#define SAMPLE_16BIT	0x01
#define SAMPLE_UNSIGNED	0x02
#define SAMPLE_LOOP		0x04
#define SAMPLE_PINGPONG	0x08
#define SAMPLE_REVERSE	0x10
#define SAMPLE_SUSTAIN	0x20
#define SAMPLE_ENVELOPE	0x40
#define SAMPLE_CLAMPED	0x80

#ifdef DEBUG_SAMPLES
#define SAMPLE_CONVERT_DEBUG(dx) printf("\r%s\n",dx)
#else
#define SAMPLE_CONVERT_DEBUG(dx)
#endif

extern unsigned short int _WM_SampleRate;

struct _sample {
	unsigned int data_length;
	unsigned int loop_start;
	unsigned int loop_end;
	unsigned int loop_size;
	unsigned char loop_fraction;
	unsigned short rate;
	unsigned int freq_low;
	unsigned int freq_high;
	unsigned int freq_root;
	unsigned char modes;
	signed int env_rate[7];
	signed int env_target[7];
	unsigned int inc_div;
	signed short *data;
	struct _sample *next;
};

struct _env {
	float time;
	float level;
	unsigned char set;
};

struct _patch {
	unsigned short patchid;
	unsigned char loaded;
	char *filename;
	signed short int amp;
	unsigned char keep;
	unsigned char remove;
	struct _env env[6];
	unsigned char note;
	unsigned long int inuse_count;
	struct _sample *first_sample;
	struct _patch *next;
};

/* Set our global defines here */
#ifndef M_PI
#define M_PI  3.14159265358979323846
#endif

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

#endif /* __COMMON_H */
