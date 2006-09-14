/*
** v_pfx.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __V_PFX_H__
#define __V_PFX_H__

//
// Pixel format conversion routines, for use with DFrameBuffer implementations
//

union PfxUnion
{
	BYTE Pal8[256];
	WORD Pal16[256];
	DWORD Pal32[256];
	BYTE Pal24[256][4];
};

struct PfxState
{
	union
	{
		struct
		{
			WORD Red;
			WORD Green;
			WORD Blue;
		} Bits16;
		struct
		{
			uint32 Red;
			uint32 Green;
			uint32 Blue;
		} Bits32;
	} Masks;
	BYTE RedShift;
	BYTE BlueShift;
	BYTE GreenShift;
	BITFIELD RedLeft:1;
	BITFIELD BlueLeft:1;
	BITFIELD GreenLeft:1;

	void SetFormat (int bits, uint32 redMask, uint32 greenMask, uint32 blueMask);
	void (*SetPalette) (const PalEntry *pal);
	void (*Convert) (BYTE *src, int srcpitch,
		void *dest, int destpitch, int destwidth, int destheight,
		fixed_t xstep, fixed_t ystep, fixed_t xfrac, fixed_t yfrac);
};

extern "C"
{
	extern PfxUnion GPfxPal;
	extern PfxState GPfx;
}

#endif //__V_PFX_H__
