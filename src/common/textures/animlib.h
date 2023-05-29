//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment

This file is part of Duke Nukem 3D version 1.5 - Atomic Edition

Duke Nukem 3D is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

Original Source: 1996 - Todd Replogle
Prepared for public release: 03/21/2003 - Charlie Wiederhold, 3D Realms
Modifications for JonoF's port by Jonathon Fowler (jf@jonof.id.au)
*/
//-------------------------------------------------------------------------
#pragma once

#include <stdint.h>
/////////////////////////////////////////////////////////////////////////////
//
//      ANIMLIB.H
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef animlib_public_h_
#define animlib_public_h_

# pragma pack(push,1)

/* structure declarations for deluxe animate large page files, doesn't
   need to be in the header because there are no exposed functions
   that use any of this directly */

struct lpfileheader
{
	uint32_t id;              /* 4 uint8_tacter ID == "LPF " */
	uint16_t maxLps;          /* max # largePages allowed. 256 FOR NOW.   */
	uint16_t nLps;            /* # largePages in this file. */
	uint32_t nRecords;        /* # records in this file.  65534 is current limit + ring */
	uint16_t maxRecsPerLp;    /* # records permitted in an lp. 256 FOR NOW.   */
	uint16_t lpfTableOffset;  /* Absolute Seek position of lpfTable.  1280 FOR NOW. */
	uint32_t contentType;     /* 4 character ID == "ANIM" */
	uint16_t width;           /* Width of screen in pixels. */
	uint16_t height;          /* Height of screen in pixels. */
	uint8_t variant;          /* 0==ANIM. */
	uint8_t version;          /* 0==frame rate in 18/sec, 1= 70/sec */
	uint8_t hasLastDelta;     /* 1==Last record is a delta from last-to-first frame. */
	uint8_t lastDeltaValid;   /* 0==Ignore ring frame. */
	uint8_t pixelType;        /* 0==256 color. */
	uint8_t CompressionType;  /* 1==(RunSkipDump) Only one used FOR NOW. */
	uint8_t otherRecsPerFrm;  /* 0 FOR NOW. */
	uint8_t bitmaptype;       /* 1==320x200, 256-color.  Only one implemented so far. */
	uint8_t recordTypes[32];  /* Not yet implemented. */
	uint32_t nFrames;         /* Number of actual frames in the file, includes ring frame. */
	uint16_t framesPerSecond; /* Number of frames to play per second. */
	uint16_t pad2[29];        /* 58 bytes of filler to round up to 128 bytes total. */
};

// this is the format of a large page structure
struct lp_descriptor
{
	uint16_t baseRecord;   // Number of first record in this large page.
	uint16_t nRecords;        // Number of records in lp.
	// bit 15 of "nRecords" == "has continuation from previous lp".
	// bit 14 of "nRecords" == "final record continues on next lp".
	uint16_t nBytes;                  // Total number of bytes of contents, excluding header.
};

#pragma pack(pop)

#define IMAGEBUFFERSIZE 0x10000

struct anim_t
{
	uint16_t framecount;          // current frame of anim
	lpfileheader * lpheader;           // file header will be loaded into this structure
	lp_descriptor * LpArray; // arrays of large page structs used to find frames
	uint16_t curlpnum;               // initialize to an invalid Large page number
	lp_descriptor * curlp;        // header of large page currently in memory
	uint16_t * thepage;     // buffer where current large page is loaded
	uint8_t imagebuffer[IMAGEBUFFERSIZE]; // buffer where anim frame is decoded
	uint8_t * buffer;
	uint8_t pal[768];
	int32_t currentframe;
};

//****************************************************************************
//
//      ANIM_LoadAnim ()
//
// Setup internal anim data structure
//
//****************************************************************************

int32_t ANIM_LoadAnim(anim_t *anim, uint8_t *buffer, int32_t length);

//****************************************************************************
//
//      ANIM_NumFrames ()
//
// returns the number of frames in the current anim
//
//****************************************************************************

inline int32_t ANIM_NumFrames(anim_t* anim)
{
	return anim->lpheader->nRecords;
}

//****************************************************************************
//
//      ANIM_DrawFrame ()
//
// Draw the frame to a returned buffer
//
//****************************************************************************

uint8_t * ANIM_DrawFrame(anim_t* anim, int32_t framenumber);

//****************************************************************************
//
//      ANIM_GetPalette ()
//
// return the palette of the anim
//****************************************************************************

inline uint8_t* ANIM_GetPalette(anim_t* anim)
{
	return anim->pal;
}

#endif
