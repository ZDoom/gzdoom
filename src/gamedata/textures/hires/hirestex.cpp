// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** external hires texture management (i.e. Doomsday style texture packs)
**
*/

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

#include "w_wad.h"
#include "m_png.h"
#include "sbar.h"
#include "gi.h"
#include "cmdlib.h"
#include "bitmap.h"
#include "image.h"

#ifndef _WIN32
#define _access(a,b) access(a,b)
#endif

static int Doom2Wad = -1;

// quick'n dirty hack. Should be enough here...
static void SetDoom2Wad()
{
	if (Doom2Wad == -1)
	{
		FString iwad = Wads.GetWadFullName(Wads.GetIwadNum());
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
int FTexture::CheckDDPK3()
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
	ETextureType useType=UseType;

	if (useType==ETextureType::SkinSprite || useType==ETextureType::Decal || useType==ETextureType::FontChar)
	{
		return -3;
	}

	bool ispatch = (useType==ETextureType::MiscPatch || useType==ETextureType::Sprite) ;

	// for patches this doesn't work yet
	if (ispatch) return -3;

	if (!gameinfo.ConfigName.CompareNoCase("Doom"))
	{
		if (!(gameinfo.flags & GI_MAPxx))
		{
			checklist = useType==ETextureType::Flat? doomflatpath : doom1texpath;
		}
		else
		{
			SetDoom2Wad();
			if (Doom2Wad == 1)
				checklist = useType==ETextureType::Flat? doomflatpath : pluttexpath;
			else if (Doom2Wad == 2)
				checklist = useType==ETextureType::Flat? doomflatpath : tnttexpath;
			else
				checklist = useType==ETextureType::Flat? doomflatpath : doom2texpath;
		}
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Heretic"))
	{
		checklist = useType==ETextureType::Flat? hereticflatpath : heretictexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Hexen"))
	{
		checklist = useType==ETextureType::Flat? hexenflatpath : hexentexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Strife"))
	{
		checklist = useType==ETextureType::Flat? strifeflatpath : strifetexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Chex"))
	{
		checklist = useType==ETextureType::Flat? chexflatpath : chextexpath;
	}
	else
		return -3;

	while (*checklist)
	{
		static const char * extensions[] = { "PNG", "JPG", "TGA", "PCX", NULL };

		for (const char ** extp=extensions; *extp; extp++)
		{
			checkName.Format(*checklist, Name.GetChars(), *extp);
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
int FTexture::CheckExternalFile(bool & hascolorkey)
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
	ETextureType useType = UseType;

	if (useType == ETextureType::SkinSprite || useType == ETextureType::Decal || useType == ETextureType::FontChar)
	{
		return -3;
	}

	bool ispatch = (useType==ETextureType::MiscPatch || useType==ETextureType::Sprite) ;

	// for patches this doesn't work yet
	if (ispatch) return -3;

	if (!gameinfo.ConfigName.CompareNoCase("Doom"))
	{
		if (!(gameinfo.flags & GI_MAPxx))
		{
			checklist = ispatch ? doom1patchpath : useType==ETextureType::Flat? doom1flatpath : doom1texpath;
		}
		else
		{
			SetDoom2Wad();
			if (Doom2Wad == 1)
				checklist = ispatch ? plutpatchpath : useType==ETextureType::Flat? plutflatpath : pluttexpath;
			else if (Doom2Wad == 2)
				checklist = ispatch ? tntpatchpath : useType==ETextureType::Flat? tntflatpath : tnttexpath;
			else
				checklist = ispatch ? doom2patchpath : useType==ETextureType::Flat? doom2flatpath : doom2texpath;
		}
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Heretic"))
	{
		checklist = ispatch ? hereticpatchpath : useType==ETextureType::Flat? hereticflatpath : heretictexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Hexen"))
	{
		checklist = ispatch ? hexenpatchpath : useType==ETextureType::Flat? hexenflatpath : hexentexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Strife"))
	{
		checklist = ispatch ?strifepatchpath : useType==ETextureType::Flat? strifeflatpath : strifetexpath;
	}
	else if (!gameinfo.ConfigName.CompareNoCase("Chex"))
	{
		checklist = ispatch ?chexpatchpath : useType==ETextureType::Flat? chexflatpath : chextexpath;
	}
	else
		return -3;

	while (*checklist)
	{
		static const char * extensions[] = { "PNG", "JPG", "TGA", "PCX", NULL };

		for (const char ** extp=extensions; *extp; extp++)
		{
			checkName.Format(*checklist, progdir.GetChars(), Name.GetChars(), *extp);
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

//==========================================================================
//
// Checks for the presence of a hires texture replacement and loads it
//
//==========================================================================

bool FTexture::LoadHiresTexture(FTextureBuffer &texbuffer, bool checkonly)
{
	if (HiresLump == -1)
	{
		bHiresHasColorKey = false;
		HiresLump = CheckDDPK3();
		if (HiresLump < 0) HiresLump = CheckExternalFile(bHiresHasColorKey);

		if (HiresLump >= 0)
		{
			HiresTexture = FTexture::CreateTexture("", HiresLump, ETextureType::Any);
			TexMan.AddTexture(HiresTexture);	// let the texture manager manage this.
		}
	}
	if (HiresTexture != nullptr)
	{
		int w = HiresTexture->GetWidth();
		int h = HiresTexture->GetHeight();

		if (!checkonly)
		{
			unsigned char * buffer = new unsigned char[w*(h + 1) * 4];
			memset(buffer, 0, w * (h + 1) * 4);

			FBitmap bmp(buffer, w * 4, w, h);
			int trans;
			auto Pixels = HiresTexture->GetBgraBitmap(nullptr, &trans);
			bmp.Blit(0, 0, Pixels);

			HiresTexture->CheckTrans(buffer, w*h, trans);

			if (bHiresHasColorKey)
			{
				// This is a crappy Doomsday color keyed image
				// We have to remove the key manually. :(
				uint32_t * dwdata = (uint32_t*)buffer;
				for (int i = (w*h); i > 0; i--)
				{
					if (dwdata[i] == 0xffffff00 || dwdata[i] == 0xffff00ff) dwdata[i] = 0;
				}
			}
			texbuffer.mBuffer = buffer;
		}
		FContentIdBuilder contentId;
		contentId.id = 0;
		contentId.imageID = HiresTexture->GetImage()->GetId();
		texbuffer.mWidth = w;
		texbuffer.mHeight = h;
		texbuffer.mContentId = contentId.id;
		return true;
	}
	return false;
}

