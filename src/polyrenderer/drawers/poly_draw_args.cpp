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
#include "swrenderer/r_swcolormaps.h"
#include "poly_draw_args.h"
#include "swrenderer/viewport/r_viewport.h"

void PolyDrawArgs::SetClipPlane(const PolyClipPlane &plane)
{
	mClipPlane[0] = plane.A;
	mClipPlane[1] = plane.B;
	mClipPlane[2] = plane.C;
	mClipPlane[3] = plane.D;
}

void PolyDrawArgs::SetTexture(const uint8_t *texels, int width, int height)
{
	mTexturePixels = texels;
	mTextureWidth = width;
	mTextureHeight = height;
	mTranslation = nullptr;
}

void PolyDrawArgs::SetTexture(FTexture *texture)
{
	mTextureWidth = texture->GetWidth();
	mTextureHeight = texture->GetHeight();
	if (PolyRenderer::Instance()->RenderTarget->IsBgra())
		mTexturePixels = (const uint8_t *)texture->GetPixelsBgra();
	else
		mTexturePixels = texture->GetPixels();
	mTranslation = nullptr;
}

void PolyDrawArgs::SetTexture(FTexture *texture, uint32_t translationID, bool forcePal)
{
	if (translationID != 0xffffffff && translationID != 0)
	{
		FRemapTable *table = TranslationToTable(translationID);
		if (table != nullptr && !table->Inactive)
		{
			if (PolyRenderer::Instance()->RenderTarget->IsBgra())
				mTranslation = (uint8_t*)table->Palette;
			else
				mTranslation = table->Remap;

			mTextureWidth = texture->GetWidth();
			mTextureHeight = texture->GetHeight();
			mTexturePixels = texture->GetPixels();
			return;
		}
	}
	
	if (forcePal)
	{
		mTextureWidth = texture->GetWidth();
		mTextureHeight = texture->GetHeight();
		mTexturePixels = texture->GetPixels();
	}
	else
	{
		SetTexture(texture);
	}
}

void PolyDrawArgs::SetLight(FSWColormap *base_colormap, uint32_t lightlevel, double globVis, bool fixed)
{
	mGlobVis = (float)globVis;

	PolyCameraLight *cameraLight = PolyCameraLight::Instance();
	if (cameraLight->FixedLightLevel() >= 0 || cameraLight->FixedColormap())
	{
		lightlevel = cameraLight->FixedLightLevel() >= 0 ? cameraLight->FixedLightLevel() : 255;
		fixed = true;
	}

	mLight = clamp<uint32_t>(lightlevel, 0, 255);
	mFixedLight = fixed;
	mLightRed = base_colormap->Color.r * 256 / 255;
	mLightGreen = base_colormap->Color.g * 256 / 255;
	mLightBlue = base_colormap->Color.b * 256 / 255;
	mLightAlpha = base_colormap->Color.a * 256 / 255;
	mFadeRed = base_colormap->Fade.r;
	mFadeGreen = base_colormap->Fade.g;
	mFadeBlue = base_colormap->Fade.b;
	mFadeAlpha = base_colormap->Fade.a;
	mDesaturate = MIN(abs(base_colormap->Desaturate), 255) * 255 / 256;
	mSimpleShade = (base_colormap->Color.d == 0x00ffffff && base_colormap->Fade.d == 0x00000000 && base_colormap->Desaturate == 0);
	mColormaps = base_colormap->Maps;
}

void PolyDrawArgs::SetColor(uint32_t bgra, uint8_t palindex)
{
	if (PolyRenderer::Instance()->RenderTarget->IsBgra())
	{
		mColor = bgra;
	}
	else
	{
		mColor = palindex;
	}
}

void PolyDrawArgs::DrawArray(const TriVertex *vertices, int vcount, PolyDrawMode mode)
{
	mVertices = vertices;
	mVertexCount = vcount;
	mDrawMode = mode;
	PolyRenderer::Instance()->DrawQueue->Push<DrawPolyTrianglesCommand>(*this, PolyTriangleDrawer::is_mirror());
}
