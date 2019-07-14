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
	mTexture = nullptr;
	mTexturePixels = texels;
	mTextureWidth = width;
	mTextureHeight = height;
	mTranslation = nullptr;
}

void PolyDrawArgs::SetTexture(FSoftwareTexture *texture, FRenderStyle style)
{
	mTexture = texture;
	mTextureWidth = texture->GetPhysicalWidth();
	mTextureHeight = texture->GetPhysicalHeight();
	if (PolyTriangleDrawer::IsBgra())
		mTexturePixels = (const uint8_t *)texture->GetPixelsBgra();
	else
		mTexturePixels = texture->GetPixels(style);
	mTranslation = nullptr;
}

void PolyDrawArgs::SetTexture(FSoftwareTexture *texture, uint32_t translationID, FRenderStyle style)
{
	// Alphatexture overrides translations.
	if (translationID != 0xffffffff && translationID != 0 && !(style.Flags & STYLEF_RedIsAlpha))
	{
		FRemapTable *table = TranslationToTable(translationID);
		if (table != nullptr && !table->Inactive)
		{
			if (PolyTriangleDrawer::IsBgra())
				mTranslation = (uint8_t*)table->Palette;
			else
				mTranslation = table->Remap;

			mTexture = texture;
			mTextureWidth = texture->GetPhysicalWidth();
			mTextureHeight = texture->GetPhysicalHeight();
			mTexturePixels = texture->GetPixels(style);
			return;
		}
	}
	
	if (style.Flags & STYLEF_RedIsAlpha)
	{
		mTexture = texture;
		mTextureWidth = texture->GetPhysicalWidth();
		mTextureHeight = texture->GetPhysicalHeight();
		mTexturePixels = texture->GetPixels(style);
	}
	else
	{
		SetTexture(texture, style);
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
	mLightRed = base_colormap->Color.r;
	mLightRed += mLightRed >> 7;
	mLightGreen = base_colormap->Color.g;
	mLightGreen += mLightGreen >> 7;
	mLightBlue = base_colormap->Color.b;
	mLightBlue += mLightBlue >> 7;
	mLightAlpha = base_colormap->Color.a;
	mLightAlpha += mLightAlpha >> 7;
	mFadeRed = base_colormap->Fade.r;
	mFadeRed += mFadeRed >> 7;
	mFadeGreen = base_colormap->Fade.g;
	mFadeGreen += mFadeGreen >> 7;
	mFadeBlue = base_colormap->Fade.b;
	mFadeBlue += mFadeBlue >> 7;
	mFadeAlpha = base_colormap->Fade.a;
	mFadeAlpha += mFadeAlpha >> 7;
	mDesaturate = MIN(abs(base_colormap->Desaturate), 255);
	mDesaturate += mDesaturate >> 7;
	mSimpleShade = (base_colormap->Color.d == 0x00ffffff && base_colormap->Fade.d == 0x00000000 && base_colormap->Desaturate == 0);
	mColormaps = base_colormap->Maps;
}

void PolyDrawArgs::SetColor(uint32_t bgra, uint8_t palindex)
{
	if (PolyTriangleDrawer::IsBgra())
	{
		mColor = bgra;
	}
	else
	{
		mColor = palindex;
	}
}

void PolyDrawArgs::SetStyle(const FRenderStyle &renderstyle, double alpha, uint32_t fillcolor, uint32_t translationID, FSoftwareTexture *tex, bool fullbright)
{
	SetTexture(tex, translationID, renderstyle);
	SetColor(0xff000000 | fillcolor, fillcolor >> 24);
	
	if (renderstyle == LegacyRenderStyles[STYLE_Normal] || (r_drawfuzz == 0 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetStyle(Translation() ? TriBlendMode::NormalTranslated : TriBlendMode::Normal, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Add] && fullbright && alpha == 1.0 && !Translation())
	{
		SetStyle(TriBlendMode::SrcColor, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_SoulTrans])
	{
		SetStyle(Translation() ? TriBlendMode::AddTranslated : TriBlendMode::Add, transsouls);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Fuzzy] || (r_drawfuzz == 1 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetColor(0xff000000, 0);
		SetStyle(TriBlendMode::Fuzzy);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Shadow] || (r_drawfuzz == 2 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetColor(0xff000000, 0);
		SetStyle(Translation() ? TriBlendMode::TranslucentStencilTranslated : TriBlendMode::TranslucentStencil, 1.0 - 160 / 255.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Stencil])
	{
		SetStyle(Translation() ? TriBlendMode::StencilTranslated : TriBlendMode::Stencil, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Translucent])
	{
		SetStyle(Translation() ? TriBlendMode::TranslucentTranslated : TriBlendMode::Translucent, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Add])
	{
		SetStyle(Translation() ? TriBlendMode::AddTranslated : TriBlendMode::Add, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Shaded])
	{
		SetStyle(Translation() ? TriBlendMode::ShadedTranslated : TriBlendMode::Shaded, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_TranslucentStencil])
	{
		SetStyle(Translation() ? TriBlendMode::TranslucentStencilTranslated : TriBlendMode::TranslucentStencil, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Subtract])
	{
		SetStyle(Translation() ? TriBlendMode::SubtractTranslated : TriBlendMode::Subtract, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_AddStencil])
	{
		SetStyle(Translation() ? TriBlendMode::AddStencilTranslated : TriBlendMode::AddStencil, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_AddShaded])
	{
		SetStyle(Translation() ? TriBlendMode::AddShadedTranslated : TriBlendMode::AddShaded, alpha);
	}
}

/////////////////////////////////////////////////////////////////////////////

void RectDrawArgs::SetTexture(FSoftwareTexture *texture, FRenderStyle style)
{
	mTexture = texture;
	mTextureWidth = texture->GetWidth();
	mTextureHeight = texture->GetHeight();
	if (PolyTriangleDrawer::IsBgra())
		mTexturePixels = (const uint8_t *)texture->GetPixelsBgra();
	else
		mTexturePixels = texture->GetPixels(style);
	mTranslation = nullptr;
}

void RectDrawArgs::SetTexture(FSoftwareTexture *texture, uint32_t translationID, FRenderStyle style)
{
	// Alphatexture overrides translations.
	if (translationID != 0xffffffff && translationID != 0 && !(style.Flags & STYLEF_RedIsAlpha))
	{
		FRemapTable *table = TranslationToTable(translationID);
		if (table != nullptr && !table->Inactive)
		{
			if (PolyTriangleDrawer::IsBgra())
				mTranslation = (uint8_t*)table->Palette;
			else
				mTranslation = table->Remap;

			mTexture = texture;
			mTextureWidth = texture->GetWidth();
			mTextureHeight = texture->GetHeight();
			mTexturePixels = texture->GetPixels(style);
			return;
		}
	}

	if (style.Flags & STYLEF_RedIsAlpha)
	{
		mTexture = texture;
		mTextureWidth = texture->GetWidth();
		mTextureHeight = texture->GetHeight();
		mTexturePixels = texture->GetPixels(style);
	}
	else
	{
		SetTexture(texture, style);
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
	if (PolyTriangleDrawer::IsBgra())
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

void RectDrawArgs::SetStyle(const FRenderStyle &renderstyle, double alpha, uint32_t fillcolor, uint32_t translationID, FSoftwareTexture *tex, bool fullbright)
{
	SetTexture(tex, translationID, renderstyle);
	SetColor(0xff000000 | fillcolor, fillcolor >> 24);

	if (renderstyle == LegacyRenderStyles[STYLE_Normal] || (r_drawfuzz == 0 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetStyle(Translation() ? TriBlendMode::NormalTranslated : TriBlendMode::Normal, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Add] && fullbright && alpha == 1.0 && !Translation())
	{
		SetStyle(TriBlendMode::SrcColor, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_SoulTrans])
	{
		SetStyle(Translation() ? TriBlendMode::AddTranslated : TriBlendMode::Add, transsouls);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Fuzzy] || (r_drawfuzz == 1 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetColor(0xff000000, 0);
		SetStyle(TriBlendMode::Fuzzy);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Shadow] || (r_drawfuzz == 2 && renderstyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		SetColor(0xff000000, 0);
		SetStyle(Translation() ? TriBlendMode::TranslucentStencilTranslated : TriBlendMode::TranslucentStencil, 1.0 - 160 / 255.0);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Stencil])
	{
		SetStyle(Translation() ? TriBlendMode::StencilTranslated : TriBlendMode::Stencil, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Translucent])
	{
		SetStyle(Translation() ? TriBlendMode::TranslucentTranslated : TriBlendMode::Translucent, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Add])
	{
		SetStyle(Translation() ? TriBlendMode::AddTranslated : TriBlendMode::Add, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Shaded])
	{
		SetStyle(Translation() ? TriBlendMode::ShadedTranslated : TriBlendMode::Shaded, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_TranslucentStencil])
	{
		SetStyle(Translation() ? TriBlendMode::TranslucentStencilTranslated : TriBlendMode::TranslucentStencil, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_Subtract])
	{
		SetStyle(Translation() ? TriBlendMode::SubtractTranslated : TriBlendMode::Subtract, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_AddStencil])
	{
		SetStyle(Translation() ? TriBlendMode::AddStencilTranslated : TriBlendMode::AddStencil, alpha);
	}
	else if (renderstyle == LegacyRenderStyles[STYLE_AddShaded])
	{
		SetStyle(Translation() ? TriBlendMode::AddShadedTranslated : TriBlendMode::AddShaded, alpha);
	}
}
