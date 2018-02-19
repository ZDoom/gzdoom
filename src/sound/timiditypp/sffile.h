/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*================================================================
 * sffile.h
 *	SoundFont file (SBK/SF2) format defintions
 *
 * Copyright (C) 1996,1997 Takashi Iwai
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *================================================================*/

/*
 * Modified by Masanao Izumo <mo@goice.co.jp>
 */

#ifndef SFFILE_H_DEF
#define SFFILE_H_DEF

namespace TimidityPlus
{

/* chunk record header */
typedef struct _SFChunk {
	char id[4];
	int32_t size;
} SFChunk;

/* generator record */
typedef struct _SFGenRec {
	int16_t oper;
	int16_t amount;
} SFGenRec;

/* layered generators record */
typedef struct _SFGenLayer {
	int nlists;
	SFGenRec *list;
} SFGenLayer;

/* header record */
typedef struct _SFHeader {
	char name[20];
	uint16_t bagNdx;
	/* layered stuff */
	int nlayers;
	SFGenLayer *layer;
} SFHeader;

/* preset header record */
typedef struct _SFPresetHdr {
	SFHeader hdr;
	uint16_t preset, bank;
	/*int32_t lib, genre, morphology;*/ /* not used */
} SFPresetHdr;

/* instrument header record */
typedef struct _SFInstHdr {
	SFHeader hdr;
} SFInstHdr;

/* sample info record */
typedef struct _SFSampleInfo {
	char name[20];
	int32_t startsample, endsample;
	int32_t startloop, endloop;
	/* ver.2 additional info */
	int32_t samplerate;
	uint8_t originalPitch;
	int8_t pitchCorrection;
	uint16_t samplelink;
	uint16_t sampletype;  /*1=mono, 2=right, 4=left, 8=linked, $8000=ROM*/
	/* optional info */
	int32_t size; /* sample size */
	int32_t loopshot; /* short-shot loop size */
} SFSampleInfo;


/*----------------------------------------------------------------
 * soundfont file info record
 *----------------------------------------------------------------*/

typedef struct _SFInfo {
	/* file name */
	char *sf_name;

	/* version of this file */
	uint16_t version, minorversion;
	/* sample position (from origin) & total size (in bytes) */
	int32_t samplepos;
	int32_t samplesize;

	/* raw INFO chunk list */
	int32_t infopos, infosize;

	/* preset headers */
	int npresets;
	SFPresetHdr *preset;
	
	/* sample infos */
	int nsamples;
	SFSampleInfo *sample;

	/* instrument headers */
	int ninsts;
	SFInstHdr *inst;

} SFInfo;


/*----------------------------------------------------------------
 * functions
 *----------------------------------------------------------------*/
}
#endif
