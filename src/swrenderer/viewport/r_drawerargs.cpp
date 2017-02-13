//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//

#include <stddef.h>
#include "r_drawerargs.h"

namespace swrenderer
{
	void DrawerArgs::SetLight(FSWColormap *base_colormap, float light, int shade)
	{
		mBaseColormap = base_colormap;
		mLight = light;
		mShade = shade;
	}

	void DrawerArgs::SetTranslationMap(lighttable_t *translation)
	{
		mTranslation = translation;
	}

	uint8_t *DrawerArgs::Colormap() const
	{
		if (mBaseColormap)
		{
			if (RenderViewport::Instance()->RenderTarget->IsBgra())
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
			shadeConstants.desaturate = MIN(abs(mBaseColormap->Desaturate), 255) * 255 / 256;
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
