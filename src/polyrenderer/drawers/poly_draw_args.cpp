/*
**  Triangle drawers
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stddef.h>
#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "poly_draw_args.h"
#include "swrenderer/viewport/r_viewport.h"

void PolyDrawArgs::SetClipPlane(float a, float b, float c, float d)
{
	clipPlane[0] = a;
	clipPlane[1] = b;
	clipPlane[2] = c;
	clipPlane[3] = d;
}

void PolyDrawArgs::SetTexture(FTexture *texture)
{
	textureWidth = texture->GetWidth();
	textureHeight = texture->GetHeight();
	auto viewport = swrenderer::RenderViewport::Instance();
	if (viewport->RenderTarget->IsBgra())
		texturePixels = (const uint8_t *)texture->GetPixelsBgra();
	else
		texturePixels = texture->GetPixels();
	translation = nullptr;
}

void PolyDrawArgs::SetTexture(FTexture *texture, uint32_t translationID, bool forcePal)
{
	if (translationID != 0xffffffff && translationID != 0)
	{
		FRemapTable *table = TranslationToTable(translationID);
		if (table != nullptr && !table->Inactive)
		{
			if (swrenderer::RenderViewport::Instance()->RenderTarget->IsBgra())
				translation = (uint8_t*)table->Palette;
			else
				translation = table->Remap;

			textureWidth = texture->GetWidth();
			textureHeight = texture->GetHeight();
			texturePixels = texture->GetPixels();
			return;
		}
	}
	
	if (forcePal)
	{
		textureWidth = texture->GetWidth();
		textureHeight = texture->GetHeight();
		texturePixels = texture->GetPixels();
	}
	else
	{
		SetTexture(texture);
	}
}

void PolyDrawArgs::SetColormap(FSWColormap *base_colormap)
{
	uniforms.light_red = base_colormap->Color.r * 256 / 255;
	uniforms.light_green = base_colormap->Color.g * 256 / 255;
	uniforms.light_blue = base_colormap->Color.b * 256 / 255;
	uniforms.light_alpha = base_colormap->Color.a * 256 / 255;
	uniforms.fade_red = base_colormap->Fade.r;
	uniforms.fade_green = base_colormap->Fade.g;
	uniforms.fade_blue = base_colormap->Fade.b;
	uniforms.fade_alpha = base_colormap->Fade.a;
	uniforms.desaturate = MIN(abs(base_colormap->Desaturate), 255) * 255 / 256;
	bool simple_shade = (base_colormap->Color.d == 0x00ffffff && base_colormap->Fade.d == 0x00000000 && base_colormap->Desaturate == 0);
	if (simple_shade)
		uniforms.flags |= TriUniforms::simple_shade;
	else
		uniforms.flags &= ~TriUniforms::simple_shade;
	colormaps = base_colormap->Maps;
}
