//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------

#include <stddef.h>
#include "r_drawerargs.h"

namespace swrenderer
{
	void DrawerArgs::SetBaseColormap(FSWColormap *base_colormap)
	{
		mBaseColormap = base_colormap;
		assert(mBaseColormap->Maps != nullptr);
	}

	void DrawerArgs::SetLight(float light, int lightlevel, bool foggy, RenderViewport *viewport)
	{
		mLight = light;
		mShade = LightVisibility::LightLevelToShade(lightlevel, foggy, viewport);
	}

	void DrawerArgs::SetLight(const ColormapLight &light)
	{
		SetBaseColormap(light.BaseColormap);
		SetLight(0.0f, light.ColormapNum << FRACBITS);
	}

	void DrawerArgs::SetLight(float light, int shade)
	{
		mLight = light;
		mShade = shade;
	}

	void DrawerArgs::SetTranslationMap(lighttable_t *translation)
	{
		mTranslation = translation;
	}

	uint8_t *DrawerArgs::Colormap(RenderViewport *viewport) const
	{
		if (mBaseColormap)
		{
			if (viewport->RenderTarget->IsBgra())
				return mBaseColormap->Maps;
			else
				return mBaseColormap->Maps + (GETPALOOKUP(mLight, mShade) << COLORMAPSHIFT);
		}
		else
		{
			return mTranslation;
		}
	}

	ShadeConstants DrawerArgs::ColormapConstants() const
	{
		ShadeConstants shadeConstants;
		if (mBaseColormap)
		{
			shadeConstants.light_red = mBaseColormap->Color.r * 256 / 255;
			shadeConstants.light_green = mBaseColormap->Color.g * 256 / 255;
			shadeConstants.light_blue = mBaseColormap->Color.b * 256 / 255;
			shadeConstants.light_alpha = mBaseColormap->Color.a * 256 / 255;
			shadeConstants.fade_red = mBaseColormap->Fade.r;
			shadeConstants.fade_green = mBaseColormap->Fade.g;
			shadeConstants.fade_blue = mBaseColormap->Fade.b;
			shadeConstants.fade_alpha = mBaseColormap->Fade.a;
			shadeConstants.desaturate = min(abs(mBaseColormap->Desaturate), 255) * 255 / 256;
			shadeConstants.simple_shade = (mBaseColormap->Color.d == 0x00ffffff && mBaseColormap->Fade.d == 0x00000000 && mBaseColormap->Desaturate == 0);
		}
		else
		{
			shadeConstants.light_red = 256;
			shadeConstants.light_green = 256;
			shadeConstants.light_blue = 256;
			shadeConstants.light_alpha = 256;
			shadeConstants.fade_red = 0;
			shadeConstants.fade_green = 0;
			shadeConstants.fade_blue = 0;
			shadeConstants.fade_alpha = 256;
			shadeConstants.desaturate = 0;
			shadeConstants.simple_shade = true;
		}
		return shadeConstants;
	}
}
