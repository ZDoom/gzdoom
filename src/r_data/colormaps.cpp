/*
** colormaps.cpp
** common Colormap handling 
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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
**
*/

#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>


#include "filesystem.h"
#include "r_sky.h"
#include "colormaps.h"
#include "templates.h"
#include "c_cvars.h"

CUSTOM_CVAR(Bool, cl_customizeinvulmap, false, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	R_UpdateInvulnerabilityColormap();
}
CUSTOM_CVAR(Color, cl_custominvulmapcolor1, 0x00001a, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (cl_customizeinvulmap)
		R_UpdateInvulnerabilityColormap();
}
CUSTOM_CVAR(Color, cl_custominvulmapcolor2, 0xa6a67a, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (cl_customizeinvulmap)
		R_UpdateInvulnerabilityColormap();
}


TArray<FakeCmap> fakecmaps;

static void FreeSpecialLights();



//==========================================================================
//
// R_DeinitColormaps
//
//==========================================================================

void R_DeinitColormaps ()
{
	SpecialColormaps.Clear();
	fakecmaps.Clear();
}

//==========================================================================
//
// R_InitColormaps
//
//==========================================================================

void R_InitColormaps (bool allowCustomColormap)
{
	// [RH] Try and convert BOOM colormaps into blending values.
	//		This is a really rough hack, but it's better than
	//		not doing anything with them at all (right?)

	FakeCmap cm;

	R_DeinitColormaps();

	cm.name[0] = 0;
	cm.blend = 0;
	fakecmaps.Push(cm);

	uint32_t NumLumps = fileSystem.GetNumEntries();

	for (uint32_t i = 0; i < NumLumps; i++)
	{
		if (fileSystem.GetFileNamespace(i) == ns_colormaps)
		{
			char name[9];
			name[8] = 0;
			fileSystem.GetFileShortName (name, i);

			if (fileSystem.CheckNumForName (name, ns_colormaps) == (int)i)
			{
				strncpy(cm.name, name, 8);
				cm.blend = 0;
				cm.lump = i;
				fakecmaps.Push(cm);
			}
		}
	}

	int rr = 0, gg = 0, bb = 0;
	for(int x=0;x<256;x++)
	{
		rr += GPalette.BaseColors[x].r;
		gg += GPalette.BaseColors[x].g;
		bb += GPalette.BaseColors[x].b;
	}
	rr >>= 8;
	gg >>= 8;
	bb >>= 8;

	int palette_brightness = (rr*77 + gg*143 + bb*35) / 255;

	// To calculate the blend it will just average the colors of the first map
	if (fakecmaps.Size() > 1)
	{
		uint8_t map[256];

		for (unsigned j = 1; j < fakecmaps.Size(); j++)
		{
			if (fileSystem.FileLength (fakecmaps[j].lump) >= 256)
			{
				int k, r, g, b;
				auto lump = fileSystem.OpenFileReader (fakecmaps[j].lump);
				lump.Read(map, 256);
				r = g = b = 0;

				for (k = 0; k < 256; k++)
				{
					r += GPalette.BaseColors[map[k]].r;
					g += GPalette.BaseColors[map[k]].g;
					b += GPalette.BaseColors[map[k]].b;
				}
				r /= 256;
				g /= 256;
				b /= 256;
				// The calculated average is too dark so brighten it according to the palettes's overall brightness
				int maxcol = MAX<int>(MAX<int>(palette_brightness, r), MAX<int>(g, b));
				
				fakecmaps[j].blend = PalEntry (255, r * 255 / maxcol, g * 255 / maxcol, b * 255 / maxcol);
			}
		}
	}

	// build default special maps (e.g. invulnerability)
	InitSpecialColormaps(GPalette.BaseColors);
	R_UpdateInvulnerabilityColormap();
}

//==========================================================================
//
// [RH] Returns an index into realcolormaps. Multiply it by
//		256*NUMCOLORMAPS to find the start of the colormap to use.
//		WATERMAP is an exception and returns a blending value instead.
//
//==========================================================================

uint32_t R_ColormapNumForName (const char *name)
{
	if (strnicmp (name, "COLORMAP", 8))
	{	// COLORMAP always returns 0
		for(int i=fakecmaps.Size()-1; i > 0; i--)
		{
			if (!strnicmp(name, fakecmaps[i].name, 8))
			{
				return i;
			}
		}
				
		if (!strnicmp (name, "WATERMAP", 8))
			return MAKEARGB (128,0,0x4f,0xa5);
	}
	return 0;
}

//==========================================================================
//
// R_BlendForColormap
//
//==========================================================================

uint32_t R_BlendForColormap (uint32_t map)
{
	return APART(map) ? map : 
		map < fakecmaps.Size() ? uint32_t(fakecmaps[map].blend) : 0;
}


//==========================================================================
//
// R_UpdateInvulnerabilityColormap
//
//==========================================================================

void R_UpdateInvulnerabilityColormap()
{
	// some of us really don't like Doom's idea of an invulnerability sphere colormap
	// this hack will override that
	if (cl_customizeinvulmap)
	{
		uint32_t color1 = cl_custominvulmapcolor1;
		uint32_t color2 = cl_custominvulmapcolor2;
		float r1 = (float)((color1 & 0xff0000) >> 16) / 128.f;
		float g1 = (float)((color1 & 0x00ff00) >> 8) / 128.f;
		float b1 = (float)((color1 & 0x0000ff) >> 0) / 128.f;
		float r2 = (float)((color2 & 0xff0000) >> 16) / 128.f;
		float g2 = (float)((color2 & 0x00ff00) >> 8) / 128.f;
		float b2 = (float)((color2 & 0x0000ff) >> 0) / 128.f;
		UpdateSpecialColormap(GPalette.BaseColors, 0, r1, g1, b1, r2, g2, b2);
	}
	else
	{
		SpecialColormaps[INVERSECOLORMAP] = SpecialColormaps[REALINVERSECOLORMAP];
	}

}

