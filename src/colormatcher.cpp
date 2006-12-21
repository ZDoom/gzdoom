/*
** colormatcher.cpp
** My attempt at a fast color matching system
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

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "colormatcher.h"
#include "v_palette.h"

FColorMatcher::FColorMatcher ()
{
	Pal = NULL;
}

FColorMatcher::FColorMatcher (const DWORD *palette)
{
	SetPalette (palette);
}

FColorMatcher::FColorMatcher (const FColorMatcher &other)
{
	*this = other;
}

FColorMatcher &FColorMatcher::operator= (const FColorMatcher &other)
{
	Pal = other.Pal;
	return *this;
}

void FColorMatcher::SetPalette (const DWORD *palette)
{
	Pal = (const PalEntry *)palette;
}

BYTE FColorMatcher::Pick (int r, int g, int b)
{
	if (Pal == NULL)
		return 1;

	return (BYTE)BestColor ((uint32 *)Pal, r, g, b);
}
