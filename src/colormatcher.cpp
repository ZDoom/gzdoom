/*
** colormatcher.cpp
** My attempt at a fast color matching system
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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
** This tries to be fast closest color finding system. It is, but the results
** are not as good as I would like, so I don't actually use it.
**
*/

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "colormatcher.h"
#include "i_system.h"

// Uncomment this to use the fast color lookup stuff.
// Unfortunately, it's not totally accurate. :-(
//#define BEFAST

struct FColorMatcher::Seed
{
	byte r, g, b;
	byte bad;
	byte color;
};

struct FColorMatcher::PalEntry
{
#ifndef __BIG_ENDIAN__
	byte b, g, r, a;
#else
	byte a, r, g, b;
#endif
};

extern int BestColor (const DWORD *palette, int r, int g, int b, int first = 0);

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
	memcpy (FirstColor, other.FirstColor, sizeof(FirstColor));
	memcpy (NextColor, other.NextColor, sizeof(NextColor));
	return *this;
}

void FColorMatcher::SetPalette (const DWORD *palette)
{
	Pal = (const PalEntry *)palette;

	// 0 is the transparent color, so it is never a valid color
	memset (FirstColor, 0, sizeof(FirstColor));
	memset (NextColor, 0, sizeof(NextColor));

#ifdef BEFAST
	Seed seeds[255];
	byte seedspread[CHISIZE+1][CHISIZE+1][CHISIZE+1];
	int numseeds;
	int i, radius;

	memset (seedspread, 255, sizeof(seedspread));
	numseeds = 0;

	// Plant each color from the palette as seeds in the color cube
	for (i = 1; i < 256; i++)
	{
		int r = (Pal[i].r + CLOSIZE/2) >> CLOBITS;
		int g = (Pal[i].g + CLOSIZE/2) >> CLOBITS;
		int b = (Pal[i].b + CLOSIZE/2) >> CLOBITS;

		if (FirstColor[r][g][b] == 0)
		{
			seeds[numseeds].r = r;
			seeds[numseeds].g = g;
			seeds[numseeds].b = b;
			seedspread[r][g][b] = numseeds;
			numseeds++;
		}
		else
		{
			NextColor[i] = FirstColor[r][g][b];
		}
		FirstColor[r][g][b] = i;
	}

	for (i = numseeds-1; i >= 0; i--)
	{
		seeds[i].color = FirstColor[seeds[i].r][seeds[i].g][seeds[i].b];
	}

	// Grow each seed outward as a cube until no seed can
	// grow any further.
	for (radius = 1; radius < CHISIZE; radius++)
	{
		int seedsused = 0;
		for (i = numseeds - 1; i >= 0; i--)
		{
			if (seeds[i].color == 0)
				continue;

			seedsused++;
										/*        _					*/
			int numhits = 0;			/*       _/| b (0,0,HISIZE)	*/
										/*     _/					*/
			int r1, r2, g1, g2, b1, b2;	/*   _/						*/
										/*  /						*/
			r1 = seeds[i].r - radius;	/* +-------> r (HISIZE,0,0)	*/
			r2 = seeds[i].r + radius;	/* |(0,0,0)					*/
			g1 = seeds[i].g - radius;	/* |						*/
			g2 = seeds[i].g + radius;	/* |						*/
			b1 = seeds[i].b - radius;	/* |						*/
			b2 = seeds[i].b + radius;	/* v g (0,HISIZE,0)			*/

			// Check to see which planes are acceptable
			byte bad = 0;
			if (r1 < 0)			bad |= 1,  r1 = 0;
			if (r2 > CHISIZE)	bad |= 2,  r2 = CHISIZE;
			if (g1 < 0)			bad |= 4,  g1 = 0;
			if (g2 > CHISIZE)	bad |= 8,  g2 = CHISIZE;
			if (b1 < 0)			bad |= 16, b1 = 0;
			if (b2 > CHISIZE)	bad |= 32, b2 = CHISIZE;

			bad |= seeds[i].bad;

			if (!(bad & 1))			// Do left green-blue plane
			{
				if (!FillPlane (r1, r1, g1, g2, b1, b2, seedspread, seeds, i))
					bad |= 1;
				r1++;
			}

			if (!(bad & 2))			// Do right green-blue plane
			{
				if (!FillPlane (r2, r2, g1, g2, b1, b2, seedspread, seeds, i))
					bad |= 2;
				r2--;
			}

			if (!(bad & 4))			// Do top red-blue plane
			{
				if (!FillPlane (r1, r2, g1, g1, b1, b2, seedspread, seeds, i))
					bad |= 4;
				g1++;
			}

			if (!(bad & 8))			// Do bottom red-blue plane
			{
				if (!FillPlane (r1, r2, g2, g2, b1, b2, seedspread, seeds, i))
					bad |= 8;
				g2--;
			}

			if (!(bad & 16))		// Do front red-green plane
			{
				if (!FillPlane (r1, r2, g1, g2, b1, b1, seedspread, seeds, i))
					bad |= 16;
			}

			if (!(bad & 32))		// Do back red-green plane
			{
				if (!FillPlane (r1, r2, g1, g2, b2, b2, seedspread, seeds, i))
					bad |= 32;
			}

			if (bad == 63)
			{ // This seed did not grow any further, so it can be deactivated
				seeds[i].color = 0;
			}
			else
			{ // Remember which directions were blocked, so we don't
			  // try growing in those directions again in later passes.
				seeds[i].bad = bad;
			}
		}
		if (seedsused == 0)
		{ // No seeds grew during this pass, so we're done.
			break;
		}
	}
#endif
}

int FColorMatcher::FillPlane (int r1, int r2, int g1, int g2, int b1, int b2,
	byte seedspread[CHISIZE+1][CHISIZE+1][CHISIZE+1],
	Seed *seeds, int thisseed)
{
	const Seed *secnd = seeds + thisseed;
	byte color = secnd->color;
	int r, g, b;
	int numhits = 0;

	for (r = r1; r <= r2; r++)
	{
		for (g = g1; g <= g2; g++)
		{
			for (b = b1; b <= b2; b++)
			{
				if (seedspread[r][g][b] == 255)
				{
					seedspread[r][g][b] = thisseed;
					FirstColor[r][g][b] = color;
					numhits++;
				}
				else
				{
					const Seed *first = seeds + seedspread[r][g][b];

					int fr = r-first->r;
					int fg = g-first->g;
					int fb = b-first->b;
					int sr = r-secnd->r;
					int sg = g-secnd->g;
					int sb = b-secnd->b;
					if (fr*fr+fg*fg+fb*fb > sr*sr+sg*sg+sb*sb)
					{
						FirstColor[r][g][b] = color;
						seedspread[r][g][b] = thisseed;
						numhits++;
					}
				}
			}
		}
	}
	return numhits;
}

byte FColorMatcher::Pick (int r, int g, int b)
{
	if (Pal == NULL)
		return 0;

#ifdef BEFAST
	byte bestcolor;
	int bestdist;

	byte color = FirstColor[(r+CLOSIZE/2)>>CLOBITS][(g+CLOSIZE/2)>>CLOBITS][(b+CLOSIZE/2)>>CLOBITS];
	if (NextColor[color] == 0)
		return color;

	bestcolor = 0;
	bestdist = 257*257+257*257+257*257;

	do
	{
		int dist = (r-Pal[color].r)*(r-Pal[color].r)+
				   (g-Pal[color].g)*(g-Pal[color].g)+
				   (b-Pal[color].b)*(b-Pal[color].b);
		if (dist < bestdist)
		{
			if (dist == 0)
				return color;

			bestdist = dist;
			bestcolor = color;
		}
		color = NextColor[color];
	} while (color != 0);
	return bestcolor;
#else
	return BestColor ((DWORD *)Pal, r, g, b);
#endif
}
