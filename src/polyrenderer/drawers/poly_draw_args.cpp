/*
**  Polygon Doom software renderer
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
#include "polyrenderer/poly_renderthread.h"

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

void PolyDrawArgs::DrawArray(PolyRenderThread *thread, const TriVertex *vertices, int vcount, PolyDrawMode mode)
{
	mVertices = vertices;
	mVertexCount = vcount;
	mDrawMode = mode;
	thread->DrawQueue->Push<DrawPolyTrianglesCommand>(*this, PolyTriangleDrawer::is_mirror());
}

void PolyDrawArgs::SetStyle(const FRenderStyle &renderstyle, double alpha, uint32_t fillcolor, uint32_t translationID, FTexture *tex, bool fullbright)
{
	bool forcePal = (renderstyle == LegacyRenderStyles[STYLE_Shaded] || renderstyle == LegacyRenderStyles[STYLE_AddShaded]);
	SetTexture(tex, translationID, forcePal);

	if (renderstyle == LegacyRenderStyles[STYLE_Normal] || (r_drawfuzz == 0 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, 1.0, 0.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Add] && fullbright && alpha == 1.0 && !Translation())
	{
		SetStyle(TriBlendMode::TextureAddSrcColor, 1.0, 1.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Add])
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, alpha, 1.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Subtract])
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedRevSub : TriBlendMode::TextureRevSub, alpha, 1.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_SoulTrans])
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, transsouls, 1.0 - transsouls);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Fuzzy] || (r_drawfuzz == 1 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetColor(0xff000000, 0);
		SetStyle(TriBlendMode::Fuzz);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Shadow] || (r_drawfuzz == 2 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, 0.0, 160 / 255.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_TranslucentStencil])
	{
		SetColor(0xff000000 | fillcolor, fillcolor >> 24);
		SetStyle(TriBlendMode::Stencil, alpha, 1.0 - alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_AddStencil])
	{
		SetColor(0xff000000 | fillcolor, fillcolor >> 24);
		SetStyle(TriBlendMode::AddStencil, alpha, 1.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Shaded])
	{
		SetColor(0xff000000 | fillcolor, fillcolor >> 24);
		SetStyle(TriBlendMode::Shaded, alpha, 1.0 - alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_AddShaded])
	{
		SetColor(0xff000000 | fillcolor, fillcolor >> 24);
		SetStyle(TriBlendMode::AddShaded, alpha, 1.0);
	}
	else
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, alpha, 1.0 - alpha);
	}
}

/////////////////////////////////////////////////////////////////////////////

void RectDrawArgs::SetTexture(const uint8_t *texels, int width, int height)
{
	mTexturePixels = texels;
	mTextureWidth = width;
	mTextureHeight = height;
	mTranslation = nullptr;
}

void RectDrawArgs::SetTexture(FTexture *texture)
{
	mTextureWidth = texture->GetWidth();
	mTextureHeight = texture->GetHeight();
	if (PolyRenderer::Instance()->RenderTarget->IsBgra())
		mTexturePixels = (const uint8_t *)texture->GetPixelsBgra();
	else
		mTexturePixels = texture->GetPixels();
	mTranslation = nullptr;
}

void RectDrawArgs::SetTexture(FTexture *texture, uint32_t translationID, bool forcePal)
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

void RectDrawArgs::SetLight(FSWColormap *base_colormap, uint32_t lightlevel)
{
	mLight = clamp<uint32_t>(lightlevel, 0, 255);
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

void RectDrawArgs::SetColor(uint32_t bgra, uint8_t palindex)
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

void RectDrawArgs::Draw(PolyRenderThread *thread, double x0, double x1, double y0, double y1, double u0, double u1, double v0, double v1)
{
	mX0 = (float)x0;
	mX1 = (float)x1;
	mY0 = (float)y0;
	mY1 = (float)y1;
	mU0 = (float)u0;
	mU1 = (float)u1;
	mV0 = (float)v0;
	mV1 = (float)v1;
	thread->DrawQueue->Push<DrawRectCommand>(*this);
}

void RectDrawArgs::SetStyle(const FRenderStyle &renderstyle, double alpha, uint32_t fillcolor, uint32_t translationID, FTexture *tex, bool fullbright)
{
	bool forcePal = (renderstyle == LegacyRenderStyles[STYLE_Shaded] || renderstyle == LegacyRenderStyles[STYLE_AddShaded]);
	SetTexture(tex, translationID, forcePal);

	if (renderstyle == LegacyRenderStyles[STYLE_Normal] || (r_drawfuzz == 0 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, 1.0, 0.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Add] && fullbright && alpha == 1.0 && !Translation())
	{
		SetStyle(TriBlendMode::TextureAddSrcColor, 1.0, 1.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Add])
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, alpha, 1.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Subtract])
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedRevSub : TriBlendMode::TextureRevSub, alpha, 1.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_SoulTrans])
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, transsouls, 1.0 - transsouls);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Fuzzy] || (r_drawfuzz == 1 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetColor(0xff000000, 0);
		SetStyle(TriBlendMode::Fuzz);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Shadow] || (r_drawfuzz == 2 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, 0.0, 160 / 255.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_TranslucentStencil])
	{
		SetColor(0xff000000 | fillcolor, fillcolor >> 24);
		SetStyle(TriBlendMode::Stencil, alpha, 1.0 - alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_AddStencil])
	{
		SetColor(0xff000000 | fillcolor, fillcolor >> 24);
		SetStyle(TriBlendMode::AddStencil, alpha, 1.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Shaded])
	{
		SetColor(0xff000000 | fillcolor, fillcolor >> 24);
		SetStyle(TriBlendMode::Shaded, alpha, 1.0 - alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_AddShaded])
	{
		SetColor(0xff000000 | fillcolor, fillcolor >> 24);
		SetStyle(TriBlendMode::AddShaded, alpha, 1.0);
	}
	else
	{
		SetStyle(Translation() ? TriBlendMode::TranslatedAdd : TriBlendMode::TextureAdd, alpha, 1.0 - alpha);
	}
}
