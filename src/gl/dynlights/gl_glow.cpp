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
** gl_glow.cpp
** Glowing flats like Doomsday
**
**/

#include "w_wad.h"
#include "sc_man.h"
#include "v_video.h"
#include "r_defs.h"
#include "textures/textures.h"

#include "gl/dynlights/gl_glow.h"

//===========================================================================
// 
//	Reads glow definitions from GLDEFS
//
//===========================================================================
void gl_InitGlow(FScanner &sc)
{
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		if (sc.Compare("FLATS"))
		{
			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				FTextureID flump=TexMan.CheckForTexture(sc.String, FTexture::TEX_Flat,FTextureManager::TEXMAN_TryAny);
				FTexture *tex = TexMan[flump];
				if (tex) tex->gl_info.bGlowing = tex->gl_info.bFullbright = true;
			}
		}
		else if (sc.Compare("WALLS"))
		{
			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				sc.MustGetString();
				FTextureID flump=TexMan.CheckForTexture(sc.String, FTexture::TEX_Wall,FTextureManager::TEXMAN_TryAny);
				FTexture *tex = TexMan[flump];
				if (tex) tex->gl_info.bGlowing = tex->gl_info.bFullbright = true;
			}
		}
		else if (sc.Compare("TEXTURE"))
		{
			sc.SetCMode(true);
			sc.MustGetString();
			FTextureID flump=TexMan.CheckForTexture(sc.String, FTexture::TEX_Flat,FTextureManager::TEXMAN_TryAny);
			FTexture *tex = TexMan[flump];
			sc.MustGetStringName(",");
			sc.MustGetString();
			PalEntry color = V_GetColor(NULL, sc.String);
			//sc.MustGetStringName(",");
			//sc.MustGetNumber();
			if (sc.CheckString(","))
			{
				if (sc.CheckNumber())
				{
					if (tex) tex->gl_info.GlowHeight = sc.Number;
					if (!sc.CheckString(",")) goto skip_fb;
				}

				sc.MustGetStringName("fullbright");
				if (tex) tex->gl_info.bFullbright = true;
			}
		skip_fb:
			sc.SetCMode(false);

			if (tex && color != 0)
			{
				tex->gl_info.bGlowing = true;
				tex->gl_info.GlowColor = color;
			}
		}
	}
}


//==========================================================================
//
// Checks whether a sprite should be affected by a glow
//
//==========================================================================
int gl_CheckSpriteGlow(sector_t *sec, int lightlevel, const DVector3 &pos)
{
	FTextureID floorpic = sec->GetTexture(sector_t::floor);
	FTexture *tex = TexMan[floorpic];
	if (tex != NULL && tex->isGlowing())
	{
		double floordiff = pos.Z - sec->floorplane.ZatPoint(pos);
		if (floordiff < tex->gl_info.GlowHeight && tex->gl_info.GlowHeight != 0)
		{
			int maxlight = (255 + lightlevel) >> 1;
			double lightfrac = floordiff / tex->gl_info.GlowHeight;
			if (lightfrac < 0) lightfrac = 0;
			lightlevel = int(lightfrac*lightlevel + maxlight*(1 - lightfrac));
		}
	}
	return lightlevel;
}

