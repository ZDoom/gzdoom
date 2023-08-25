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

#include <string.h>
#include "animlib.h"
#include "m_swap.h"

//****************************************************************************
//
// LOCALS
//
//****************************************************************************

//****************************************************************************
//
//      findpage ()
//              - return the large page number a given frame resides in
//
//****************************************************************************

static inline uint16_t findpage(anim_t *anim, uint16_t framenumber)
{
	// curlpnum is initialized to 0xffff, obviously
	size_t i = anim->curlpnum & ~0xffff;
	size_t const nLps = anim->lpheader->nLps;
	bool j = true;

	if (framenumber < anim->currentframe)
		i = 0, j = false;

	// this scans the last used page and higher first and then scans the
	// previously accessed pages afterwards if it doesn't find anything
	do
	{
		for (; i < nLps; ++i)
		{
			lp_descriptor & lp = anim->LpArray[i];
			if (lp.baseRecord <= framenumber && framenumber < lp.baseRecord + lp.nRecords)
				return (uint16_t)i;
		}

		if (j && i == nLps)
		{
			// handle out of order pages... I don't think any Duke .ANM files
			// have them, but they're part of the file spec
			i = 0, j = false;
			continue;
		}

		break;
	}
	while (1);

	return (uint16_t)i;
}


//****************************************************************************
//
//      loadpage ()
//      - seek out and set pointers to the large page specified
//
//****************************************************************************

static inline void loadpage(anim_t *anim, uint16_t pagenumber, uint16_t **pagepointer)
{
	if (anim->curlpnum == pagenumber)
		return;

	anim->curlpnum = pagenumber;
	anim->curlp = &anim->LpArray[pagenumber];
	*pagepointer = (uint16_t *)(anim->buffer + 0xb00 + (pagenumber*IMAGEBUFFERSIZE) +
								sizeof(lp_descriptor) + sizeof(uint16_t));
}


//****************************************************************************
//
//      decodeframe ()
//      - I found this less obfuscated version of the .ANM "decompressor",
//        (c) 1998 "Jari Komppa aka Sol/Trauma".  This code is public domain
//        but has been mostly rewritten by me.
//
//      - As a side note, it looks like this format came about in 1989 and
//        never went anywhere after that, and appears to have been the format
//        used by Electronic Arts' DeluxePaint Animation, which never made it
//        past version 1.0.
//
//****************************************************************************

static void decodeframe(uint8_t * srcP, uint8_t * dstP)
{
	do
	{
		{
			/* short op */
			uint8_t count = *srcP++;

			if (!count) /* short RLE */
			{
				uint8_t color = *(srcP+1);
				count = *(uint8_t *)srcP;
				srcP += sizeof(int16_t);
				memset(dstP, color, count);
				dstP += count;
				continue;
			}
			else if ((count & 0x80) == 0) /* short copy */
			{
				memcpy(dstP, srcP, count);
				dstP += count;
				srcP += count;
				continue;
			}
			else if ((count &= ~0x80) > 0) /* short skip */
			{
				dstP += count;
				continue;
			}
		}

		{
			/* long op */
			uint16_t count = LittleShort((uint16_t)GetShort(srcP));
			srcP += sizeof(int16_t);

			if (!count) /* stop sign */
				return;
			else if ((count & 0x8000) == 0) /* long skip */
			{
				dstP += count;
				continue;
			}
			else if ((count &= ~0x8000) & 0x4000) /* long RLE */
			{
				uint8_t color = *srcP++;
				count &= ~0x4000;
				memset(dstP, color, count);
				dstP += count;
				continue;
			}

			/* long copy */
			memcpy(dstP, srcP, count);
			dstP += count;
			srcP += count;
		}
	}
	while (1);
}


//****************************************************************************
//
//      renderframe ()
//      - draw the frame sepcified from the large page in the buffer pointed to
//
//****************************************************************************

static void renderframe(anim_t *anim, uint16_t framenumber, uint16_t *pagepointer)
{
	uint16_t offset = 0;
	uint16_t frame = framenumber - anim->curlp->baseRecord;

	while (frame--) offset += LittleShort(pagepointer[frame]);

	if (offset >= anim->curlp->nBytes)
		return;

	uint8_t *ppointer = (uint8_t *)(pagepointer) + anim->curlp->nRecords*2 + offset + 4;

	if ((ppointer-4)[1])
	{
		uint16_t const temp = LittleShort(((uint16_t *)(ppointer-4))[1]);
		ppointer += temp + (temp & 1);
	}

	decodeframe((uint8_t *)ppointer, (uint8_t *)anim->imagebuffer);
}


//****************************************************************************
//
//      drawframe ()
//      - high level frame draw routine
//
//****************************************************************************

static inline void drawframe(anim_t *anim, uint16_t framenumber)
{
	loadpage(anim, findpage(anim, framenumber), &anim->thepage);
	renderframe(anim, framenumber, anim->thepage);
}

// <length> is the file size, for consistency checking.
int32_t ANIM_LoadAnim(anim_t *anim, const uint8_t *buffer, size_t length)
{
	if (memcmp(buffer, "LPF ", 4)) return -1;

	length -= sizeof(lpfileheader)+128+768;
	if (length < 0)
		return -1;

	anim->curlpnum = 0xffff;
	anim->currentframe = -1;

	// this just modifies the data in-place instead of copying it elsewhere now
	lpfileheader & lpheader = *(anim->lpheader = (lpfileheader *)(anim->buffer = buffer));

	lpheader.id              = LittleLong(lpheader.id);
	lpheader.maxLps          = LittleShort(lpheader.maxLps);
	lpheader.nLps            = LittleShort(lpheader.nLps);
	lpheader.nRecords        = LittleLong(lpheader.nRecords);
	lpheader.maxRecsPerLp    = LittleShort(lpheader.maxRecsPerLp);
	lpheader.lpfTableOffset  = LittleShort(lpheader.lpfTableOffset);
	lpheader.contentType     = LittleLong(lpheader.contentType);
	lpheader.width           = LittleShort(lpheader.width);
	lpheader.height          = LittleShort(lpheader.height);
	lpheader.nFrames         = LittleLong(lpheader.nFrames);
	lpheader.framesPerSecond = LittleShort(lpheader.framesPerSecond);

	length -= lpheader.nLps * sizeof(lp_descriptor);
	if (length < 0)
		return -2;

	buffer += sizeof(lpfileheader)+128;

	// load the color palette
	for (uint8_t * pal = anim->pal, * pal_end = pal+768; pal < pal_end; pal += 3, buffer += 4)
	{
		pal[2] = buffer[0];
		pal[1] = buffer[1];
		pal[0] = buffer[2];
	}

	// set up large page descriptors
	anim->LpArray = (lp_descriptor *)buffer;

	// theoretically we should be able to play files with more than 256 frames now
	// assuming the utilities to create them can make them that way
	for (lp_descriptor * lp = anim->LpArray, * lp_end = lp+lpheader.nLps; lp < lp_end; ++lp)
	{
		lp->baseRecord = LittleShort(lp->baseRecord);
		lp->nRecords   = LittleShort(lp->nRecords);
		lp->nBytes     = LittleShort(lp->nBytes);
	}
	return ANIM_NumFrames(anim) <= 0 ? -1 : 0;
}


uint8_t * ANIM_DrawFrame(anim_t *anim, int32_t framenumber)
{
	uint32_t cnt = anim->currentframe;

	// handle first play and looping or rewinding
	if (cnt > (uint32_t)framenumber)
		cnt = 0;

	do drawframe(anim, cnt++);
	while (cnt < (uint32_t)framenumber);

	anim->currentframe = framenumber;
	return anim->imagebuffer;
}


