/*
** v_palette.cpp
** Automatic colormap generation for "colored lights", etc.
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

#include "g_level.h"


#include "v_video.h"
#include "filesystem.h"
#include "i_video.h"
#include "c_dispatch.h"
#include "st_stuff.h"
#include "g_levellocals.h"
#include "m_png.h"
#include "v_colortables.h"

/* Current color blending values */
int		BlendR, BlendG, BlendB, BlendA;



void InitPalette ()
{
	uint8_t pal[768];
	
	ReadPalette(fileSystem.GetNumForName("PLAYPAL"), pal);

	GPalette.Init(NUM_TRANSLATION_TABLES, nullptr);
	GPalette.SetPalette (pal, -1);

	int lump = fileSystem.CheckNumForName("COLORMAP");
	if (lump == -1) lump = fileSystem.CheckNumForName("COLORMAP", ns_colormaps);
	if (lump != -1)
	{
		FileData cmap = fileSystem.ReadFile(lump);
		const unsigned char* cmapdata = (const unsigned char*)cmap.GetMem();
		GPalette.GenerateGlobalBrightmapFromColormap(cmapdata, 32);
		MakeGoodRemap((uint32_t*)GPalette.BaseColors, GPalette.Remap, cmapdata + 7936);	// last entry in colormap
	}
	else
		MakeGoodRemap ((uint32_t*)GPalette.BaseColors, GPalette.Remap);

	ColorMatcher.SetPalette ((uint32_t *)GPalette.BaseColors);

	if (GPalette.Remap[0] == 0)
	{ // No duplicates, so settle for something close to color 0
		GPalette.Remap[0] = BestColor ((uint32_t *)GPalette.BaseColors,
			GPalette.BaseColors[0].r, GPalette.BaseColors[0].g, GPalette.BaseColors[0].b, 1, 255);
	}
	GPalette.BaseColors[0] = 0;

	// Colormaps have to be initialized before actors are loaded,
	// otherwise Powerup.Colormap will not work.
	R_InitColormaps ();
	BuildTransTable (GPalette.BaseColors);

}

