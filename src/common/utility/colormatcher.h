/*
** colormatcher.h
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
** Once upon a time, this tried to be a fast closest color finding system.
** It was, but the results were not as good as I would like, so I didn't
** actually use it. But I did keep the code around in case I ever felt like
** revisiting the problem. I never did, so now it's relegated to the mists
** of SVN history, and this is just a thin wrapper around BestColor().
**
*/

#ifndef __COLORMATCHER_H__
#define __COLORMATCHER_H__

#include "palutil.h"

int BestColor (const uint32_t *pal_in, int r, int g, int b, int first, int num);

class FColorMatcher
{
public:
	FColorMatcher () = default;
	FColorMatcher (const uint32_t *palette) { Pal = reinterpret_cast<const PalEntry*>(palette); }
	FColorMatcher (const FColorMatcher &other) = default;

	void SetPalette(PalEntry* palette) { Pal = palette; }
	void SetPalette (const uint32_t *palette) { Pal = reinterpret_cast<const PalEntry*>(palette); }
	uint8_t Pick (int r, int g, int b)
	{
		if (Pal == nullptr)
			return 1;

		return (uint8_t)BestColor ((uint32_t *)Pal, r, g, b, 1, 255);
	}
	
	uint8_t Pick (PalEntry pe)
	{
		return Pick(pe.r, pe.g, pe.b);
	}

	FColorMatcher &operator= (const FColorMatcher &other) = default;

private:
	const PalEntry *Pal;
};

extern FColorMatcher ColorMatcher;


#endif //__COLORMATCHER_H__
