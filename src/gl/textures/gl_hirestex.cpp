/*
** Hires texture management
**
**---------------------------------------------------------------------------
** Copyright 2005 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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

#ifdef _MSC_VER
#define    F_OK    0    /* Check for file existence */
#define    W_OK    2    /* Check for write permission */
#define    R_OK    4    /* Check for read permission */
#include <io.h>
#else
#include <unistd.h>
#endif

#include "w_wad.h"
#include "m_png.h"
#include "sbar.h"
#include "gi.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "doomstat.h"
#include "d_main.h"
#include "zstring.h"

#ifndef _WIN32
#define _access(a,b)	access(a,b)
#endif

static int Doom2Wad = -1;

// quick'n dirty hack. Should be enough here...
static void SetDoom2Wad()
{
	if (Doom2Wad == -1)
	{
		FString iwad = Wads.GetWadFullName(1);
		iwad.ToLower();
		if (iwad.IndexOf("plutonia") >= 0) Doom2Wad = 1;
		else if (iwad.IndexOf("tnt") >= 0) Doom2Wad = 2;
		else Doom2Wad = 0;
	}
}
//==========================================================================
//
// Checks for the presence of a hires texture replacement in a Doomsday style PK3
//
//==========================================================================
int CheckDDPK3(FTexture *tex)
{
	static const char * doom1texpath[]= {
		"data/jdoom/textures/doom/%s.%s", "data/jdoom/textures/doom-ult/%s.%s", "data/jdoom/textures/doom1/%s.%s", "data/jdoom/textures/%s.%s", NULL };

	static const char * doom2texpath[]= {
		"data/jdoom/textures/doom2/%s.%s", "data/jdoom/textures/%s.%s", NULL };

	static const char * pluttexpath[]= {
		"data/jdoom/textures/doom2-plut/%s.%s", "data/jdoom/textures/plutonia/%s.%s", "data/jdoom/textures/%s.%s", NULL };

	static const char * tnttexpath[]= {
		"data/jdoom/textures/doom2-tnt/%s.%s", "data/jdoom/textures/tnt/%s.%s", "data/jdoom/textures/%s.%s", NULL };

	static const char * heretictexpath[]= {
		"data/jheretic/textures/%s.%s", NULL };

	static const char * hexentexpath[]= {
		"data/jhexen/textures/%s.%s", NULL };

	static const char * strifetexpath[]= {
		"data/jstrife/textures/%s.%s", NULL };

	static const char * chextexpath[]= {
		"data/jchex/textures/%s.%s", NULL };

	static const char * doomflatpath[]= {
		"data/jdoom/flats/%s.%s", NULL };

	static const char * hereticflatpath[]= {
		"data/jheretic/flats/%s.%s", NULL };

	static const char * hexenflatpath[]= {
		"data/jhexen/flats/%s.%s", NULL };

	static const char * strifeflatpath[]= {
		"data/jstrife/flats/%s.%s", NULL };

	static const char * chexflatpath[]= {
		"data/jchex/flats/%s.%s", NULL };


	FString checkName;
	const char ** checklist;
	BYTE useType=tex->UseType;

	if (useType==FTexture::TEX_SkinSprite || useType==FTexture::TEX_Decal || useType==FTexture::TEX_FontChar)
	{
		return -3;
	}

	bool ispatch = (useType==FTexture::TEX_MiscPatch || useType==FTexture::TEX_Sprite) ;

	// for patches this doesn't work yet
	if (ispatch) return -3;

	if (!gameinfo.ConfigName.CompareNoCase("Doom"))
	{
		if (!(gameinfo.flags & GI_MAPxx))
		{
			checklist = useType==FTexture::TEX_Flat? doomflatpath : doom1texpath;
		}
		else
		{
			SetDoom2Wad();
			if (Doom2Wad == 1)
				checklist = useType==FTexture::TEX_Flat? doomflatpath : pluttexpath;
			else if (Doom2Wad == 2)
				checklist = useType==FTexture::TEX_Flat? doomflatpath : tnttexpath;
			else
				checklist = useType==FTexture::TEX_Flat? doomflatpath : doom2texpath;
		}
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Heretic"))
	{
		checklist = useType==FTexture::TEX_Flat? hereticflatpath : heretictexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Hexen"))
	{
		checklist = useType==FTexture::TEX_Flat? hexenflatpath : hexentexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Strife"))
	{
		checklist = useType==FTexture::TEX_Flat? strifeflatpath : strifetexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Chex"))
	{
		checklist = useType==FTexture::TEX_Flat? chexflatpath : chextexpath;
	}
	else
		return -3;

	while (*checklist)
	{
		static const char * extensions[] = { "PNG", "JPG", "TGA", "PCX", NULL };

		for (const char ** extp=extensions; *extp; extp++)
		{
			checkName.Format(*checklist, tex->Name, *extp);
			int lumpnum = Wads.CheckNumForFullName(checkName);
			if (lumpnum >= 0) return lumpnum;
		}
		checklist++;
	}
	return -3;
}


//==========================================================================
//
// Checks for the presence of a hires texture replacement
//
//==========================================================================
int CheckExternalFile(FTexture *tex, bool & hascolorkey)
{
	static const char * doom1texpath[]= {
		"%stextures/doom/doom1/%s.%s", "%stextures/doom/doom1/%s-ck.%s", 
			"%stextures/doom/%s.%s", "%stextures/doom/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * doom2texpath[]= {
		"%stextures/doom/doom2/%s.%s", "%stextures/doom/doom2/%s-ck.%s", 
			"%stextures/doom/%s.%s", "%stextures/doom/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * pluttexpath[]= {
		"%stextures/doom/plut/%s.%s", "%stextures/doom/plut/%s-ck.%s", 
		"%stextures/doom/doom2-plut/%s.%s", "%stextures/doom/doom2-plut/%s-ck.%s", 
			"%stextures/doom/%s.%s", "%stextures/doom/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * tnttexpath[]= {
		"%stextures/doom/tnt/%s.%s", "%stextures/doom/tnt/%s-ck.%s", 
		"%stextures/doom/doom2-tnt/%s.%s", "%stextures/doom/doom2-tnt/%s-ck.%s", 
			"%stextures/doom/%s.%s", "%stextures/doom/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * heretictexpath[]= {
		"%stextures/heretic/%s.%s", "%stextures/heretic/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * hexentexpath[]= {
		"%stextures/hexen/%s.%s", "%stextures/hexen/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * strifetexpath[]= {
		"%stextures/strife/%s.%s", "%stextures/strife/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * chextexpath[]= {
		"%stextures/chex/%s.%s", "%stextures/chex/%s-ck.%s", "%stextures/%s.%s", "%stextures/%s-ck.%s", NULL
	};

	static const char * doom1flatpath[]= {
		"%sflats/doom/doom1/%s.%s", "%stextures/doom/doom1/flat-%s.%s", 
			"%sflats/doom/%s.%s", "%stextures/doom/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * doom2flatpath[]= {
		"%sflats/doom/doom2/%s.%s", "%stextures/doom/doom2/flat-%s.%s", 
			"%sflats/doom/%s.%s", "%stextures/doom/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * plutflatpath[]= {
		"%sflats/doom/plut/%s.%s", "%stextures/doom/plut/flat-%s.%s", 
		"%sflats/doom/doom2-plut/%s.%s", "%stextures/doom/doom2-plut/flat-%s.%s", 
			"%sflats/doom/%s.%s", "%stextures/doom/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * tntflatpath[]= {
		"%sflats/doom/tnt/%s.%s", "%stextures/doom/tnt/flat-%s.%s", 
		"%sflats/doom/doom2-tnt/%s.%s", "%stextures/doom/doom2-tnt/flat-%s.%s", 
			"%sflats/doom/%s.%s", "%stextures/doom/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * hereticflatpath[]= {
		"%sflats/heretic/%s.%s", "%stextures/heretic/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * hexenflatpath[]= {
		"%sflats/hexen/%s.%s", "%stextures/hexen/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * strifeflatpath[]= {
		"%sflats/strife/%s.%s", "%stextures/strife/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * chexflatpath[]= {
		"%sflats/chex/%s.%s", "%stextures/chex/flat-%s.%s", "%sflats/%s.%s", "%stextures/flat-%s.%s", NULL
	};

	static const char * doom1patchpath[]= {
		"%spatches/doom/doom1/%s.%s", "%spatches/doom/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * doom2patchpath[]= {
		"%spatches/doom/doom2/%s.%s", "%spatches/doom/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * plutpatchpath[]= {
		"%spatches/doom/plut/%s.%s", "%spatches/doom/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * tntpatchpath[]= {
		"%spatches/doom/tnt/%s.%s", "%spatches/doom/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * hereticpatchpath[]= {
		"%spatches/heretic/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * hexenpatchpath[]= {
		"%spatches/hexen/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * strifepatchpath[]= {
		"%spatches/strife/%s.%s", "%spatches/%s.%s", NULL
	};

	static const char * chexpatchpath[]= {
		"%spatches/chex/%s.%s", "%spatches/%s.%s", NULL
	};

	FString checkName;
	const char ** checklist;
	BYTE useType=tex->UseType;

	if (useType==FTexture::TEX_SkinSprite || useType==FTexture::TEX_Decal || useType==FTexture::TEX_FontChar)
	{
		return -3;
	}

	bool ispatch = (useType==FTexture::TEX_MiscPatch || useType==FTexture::TEX_Sprite) ;

	// for patches this doesn't work yet
	if (ispatch) return -3;

	if (!gameinfo.ConfigName.CompareNoCase("Doom"))
	{
		if (!(gameinfo.flags & GI_MAPxx))
		{
			checklist = ispatch ? doom1patchpath : useType==FTexture::TEX_Flat? doom1flatpath : doom1texpath;
		}
		else
		{
			SetDoom2Wad();
			if (Doom2Wad == 1)
				checklist = ispatch ? plutpatchpath : useType==FTexture::TEX_Flat? plutflatpath : pluttexpath;
			else if (Doom2Wad == 2)
				checklist = ispatch ? tntpatchpath : useType==FTexture::TEX_Flat? tntflatpath : tnttexpath;
			else
				checklist = ispatch ? doom2patchpath : useType==FTexture::TEX_Flat? doom2flatpath : doom2texpath;
		}
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Heretic"))
	{
		checklist = ispatch ? hereticpatchpath : useType==FTexture::TEX_Flat? hereticflatpath : heretictexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Hexen"))
	{
		checklist = ispatch ? hexenpatchpath : useType==FTexture::TEX_Flat? hexenflatpath : hexentexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Strife"))
	{
		checklist = ispatch ?strifepatchpath : useType==FTexture::TEX_Flat? strifeflatpath : strifetexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Chex"))
	{
		checklist = ispatch ?chexpatchpath : useType==FTexture::TEX_Flat? chexflatpath : chextexpath;
	}
	else
		return -3;

	while (*checklist)
	{
		static const char * extensions[] = { "PNG", "JPG", "TGA", "PCX", NULL };

		for (const char ** extp=extensions; *extp; extp++)
		{
			checkName.Format(*checklist, progdir.GetChars(), tex->Name, *extp);
			if (_access(checkName, 0) == 0) 
			{
				hascolorkey = !!strstr(checkName, "-ck.");
				return Wads.AddExternalFile(checkName);
			}
		}
		checklist++;
	}
	return -3;
}


