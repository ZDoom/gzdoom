/*
** r_spritedrawer.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2016 Magnus Norddahl
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

#include <stddef.h>
#include "r_spritedrawer.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	SpriteDrawerArgs::SpriteDrawerArgs()
	{
		colfunc = &SWPixelFormatDrawers::DrawColumn;
	}

	void SpriteDrawerArgs::DrawMasked(RenderThread* thread, double topZ, double scale, bool flipX, bool flipY, const FWallCoords& WallC, const ProjectedWallLight& light, FSoftwareTexture* tex, const short* mfloorclip, const short* mceilingclip, FRenderStyle style)
	{
		auto viewport = thread->Viewport.get();
		auto cameraLight = CameraLight::Instance();

		bool calclighting = cameraLight->FixedLightLevel() < 0 && !cameraLight->FixedColormap() && !light.IsSpriteLight();

		float wpos = 1.0f / WallC.sz1;
		float wstepX = (1.0f / WallC.sz2 - wpos) / (WallC.sx2 - WallC.sx1);

		float upos, ustepX;
		if (flipX)
		{
			upos = (1.0f - WallC.tx1) / WallC.sz1;
			ustepX = ((1.0f - WallC.tx2) / WallC.sz2 - upos) / (WallC.sx2 - WallC.sx1);
		}
		else
		{
			upos = WallC.tx1 / WallC.sz1;
			ustepX = (WallC.tx2 / WallC.sz2 - upos) / (WallC.sx2 - WallC.sx1);
		}

		float iscale = 1.0f / scale;
		if (flipY)
			iscale = -iscale;
		float vstepY = iscale / WallC.sz1 / (viewport->InvZtoScale / WallC.sz1);

		wpos += wstepX * 0.5f;
		upos += ustepX * 0.5f;

		int x1 = WallC.sx1;
		int x2 = WallC.sx2;

		float centerY = thread->Viewport->CenterY;
		topZ -= thread->Viewport->viewpoint.Pos.Z;

		if (flipY)
			topZ -= tex->GetHeight() * scale;

		int texwidth = tex->GetPhysicalWidth();
		int texheight = tex->GetPhysicalHeight();

		float lightpos = light.GetLightPos(x1);
		float lightstep = light.GetLightStep();

		dc_viewport = viewport;
		dc_textureheight = texheight;

		bool bgra = viewport->RenderTarget->IsBgra() && !drawer_needs_pal_input;

		for (int x = x1; x < x2; x++)
		{
			if (calclighting)
			{
				SetLight(lightpos, light.GetLightLevel(), light.GetFoggy(), thread->Viewport.get());
			}

			float w = 1.0f / wpos;
			float y1 = centerY - topZ * wpos * viewport->InvZtoScale;
			float u = upos * w;
			float scaleU = ustepX * w;
			float scaleV = vstepY * w;

			uint32_t texelX = (uint32_t)(int64_t)((u - std::floor(u)) * 0x1'0000'0000LL);
			uint32_t texelStepX = (uint32_t)(int64_t)(scaleU * 0x1'0000'0000LL);
			uint32_t texelStepY = (uint32_t)(int64_t)(scaleV * 0x1'0000'0000LL);

			DrawMaskedColumn(thread, x, y1, mceilingclip[x], mfloorclip[x], texelX, texelStepX, texelStepY, scaleV, flipY, tex, texwidth, texheight, bgra, style);

			upos += ustepX;
			wpos += wstepX;
			lightpos += lightstep;
		}
	}

	void SpriteDrawerArgs::DrawMasked2D(RenderThread* thread, double x0, double x1, double y0, double y1, FSoftwareTexture* tex, FRenderStyle style)
	{
		int sx0 = MAX((int)x0, 0);
		int sx1 = MIN((int)x1, viewwidth);
		int sy0 = MAX((int)y0, 0);
		int sy1 = MIN((int)y1, viewheight);

		if (sx0 >= sx1 || sy0 >= sy1)
			return;

		float ustepX = 1.0f / (x1 - x0);
		float vstepY = 1.0f / (y1 - y0);
		float upos = ustepX * (sx0 + 0.5f - x0);

		uint32_t texelStepX = (uint32_t)(int64_t)(ustepX * 0x1'0000'0000LL);
		uint32_t texelStepY = (uint32_t)(int64_t)(vstepY * 0x1'0000'0000LL);

		bool bgra = thread->Viewport->RenderTarget->IsBgra() && !drawer_needs_pal_input;
		int texwidth = tex->GetPhysicalWidth();
		int texheight = tex->GetPhysicalHeight();

		dc_viewport = thread->Viewport.get();
		dc_textureheight = texheight;

		vstepY *= texheight;

		for (int x = sx0; x < sx1; x++)
		{
			float u = upos;
			uint32_t texelX = (uint32_t)(int64_t)((u - std::floor(u)) * 0x1'0000'0000LL);

			DrawMaskedColumn(thread, x, sy0, sy0, sy1, texelX, texelStepX, texelStepY, vstepY, false, tex, texwidth, texheight, bgra, style);

			upos += ustepX;
		}
	}

	void SpriteDrawerArgs::DrawMaskedColumn(RenderThread* thread, int x, int y1, int cliptop, int clipbottom, uint32_t texelX, uint32_t texelStepX, uint32_t texelStepY, float scaleV, bool flipY, FSoftwareTexture* tex, int texwidth, int texheight, bool bgra, FRenderStyle style)
	{
		const FSoftwareTextureSpan* span;
		if (bgra)
		{
			double xmagnitude = fabs(static_cast<int32_t>(texelStepX)* (1.0 / 0x1'0000'0000LL));
			double ymagnitude = fabs(static_cast<int32_t>(texelStepY)* (1.0 / 0x1'0000'0000LL));
			double magnitude = MAX(ymagnitude, xmagnitude);
			double min_lod = -1000.0;
			double lod = MAX(log2(magnitude) + r_lod_bias, min_lod);
			bool magnifying = lod < 0.0f;

			int mipmap_offset = 0;
			int mip_width = texwidth;
			int mip_height = texheight;
			if (r_mipmap && tex->Mipmapped() && mip_width > 1 && mip_height > 1)
			{
				int level = (int)lod;
				while (level > 0 && mip_width > 1 && mip_height > 1)
				{
					mipmap_offset += mip_width * mip_height;
					level--;
					mip_width = MAX(mip_width >> 1, 1);
					mip_height = MAX(mip_height >> 1, 1);
				}
			}

			const uint32_t* pixels = tex->GetPixelsBgra() + mipmap_offset;
			fixed_t xoffset = (texelX >> 16)* mip_width;

			bool filter_nearest = (magnifying && !r_magfilter) || (!magnifying && !r_minfilter);
			if (filter_nearest)
			{
				xoffset = MAX(MIN(xoffset, (mip_width << FRACBITS) - 1), 0);

				int tx = xoffset >> FRACBITS;
				dc_source = (uint8_t*)(pixels + tx * mip_height);
				dc_source2 = nullptr;
				dc_textureheight = mip_height;
				dc_texturefracx = 0;
			}
			else
			{
				xoffset = MAX(MIN(xoffset - (FRACUNIT / 2), (mip_width << FRACBITS) - 1), 0);

				int tx0 = xoffset >> FRACBITS;
				int tx1 = MIN(tx0 + 1, mip_width - 1);
				dc_source = (uint8_t*)(pixels + tx0 * mip_height);
				dc_source2 = (uint8_t*)(pixels + tx1 * mip_height);
				dc_textureheight = mip_height;
				dc_texturefracx = (xoffset >> (FRACBITS - 4)) & 15;
			}

			int col = ((texelX >> 16)* texwidth) >> 16;
			tex->GetColumnBgra(col, &span);

			dc_iscale = (uint32_t)(int64_t)(scaleV / texheight * (1 << 30));
			dc_x = x;

			while (span->Length != 0)
			{
				const int length = span->Length;
				const int top = span->TopOffset;

				// calculate unclipped screen coordinates for post
				dc_yl = (int)(y1 + top / scaleV + 0.5f);
				dc_yh = (int)(y1 + (top + length) / scaleV + 0.5f);

				if (flipY)
					std::swap(dc_yl, dc_yh);

				dc_yl = std::max(dc_yl, cliptop);
				dc_yh = std::min(dc_yh, clipbottom);

				if (dc_yl <= dc_yh)
				{
					double v = ((dc_yl + 0.5f - y1) * scaleV) / texheight;
					dc_texturefrac = (uint32_t)(v * (1 << 30));

					SetDest(dc_viewport, dc_x, dc_yl);
					dc_count = dc_yh - dc_yl;
					dc_yl--;

					(thread->Drawers(dc_viewport)->*colfunc)(*this);
				}
				span++;
			}
		}
		else
		{
			int col = ((texelX >> 16)* texwidth) >> 16;
			dc_source = tex->GetColumn(style, col, &span);
			dc_source2 = nullptr;

			dc_iscale = FLOAT2FIXED(scaleV);
			dc_x = x;

			while (span->Length != 0)
			{
				const int length = span->Length;
				const int top = span->TopOffset;

				// calculate unclipped screen coordinates for post
				dc_yl = (int)(y1 + top / scaleV + 0.5f);
				dc_yh = (int)(y1 + (top + length) / scaleV + 0.5f);

				if (flipY)
					std::swap(dc_yl, dc_yh);

				dc_yl = std::max(dc_yl, cliptop);
				dc_yh = std::min(dc_yh, clipbottom);

				if (dc_yl < dc_yh)
				{
					dc_texturefrac = FLOAT2FIXED((dc_yl + 0.5f - y1) * scaleV);
					SetDest(thread->Viewport.get(), dc_x, dc_yl);
					dc_count = dc_yh - dc_yl;
					dc_yl--;

					fixed_t maxfrac = ((top + length) << FRACBITS) - 1;
					dc_texturefrac = MAX(dc_texturefrac, 0);
					dc_texturefrac = MIN(dc_texturefrac, maxfrac);
					if (dc_iscale > 0)
						dc_count = MIN(dc_count, (maxfrac - dc_texturefrac + dc_iscale - 1) / dc_iscale);
					else if (dc_iscale < 0)
						dc_count = MIN(dc_count, (dc_texturefrac - dc_iscale) / (-dc_iscale));

					(thread->Drawers(dc_viewport)->*colfunc)(*this);
				}
				span++;
			}
		}
	}

	bool SpriteDrawerArgs::SetBlendFunc(int op, fixed_t fglevel, fixed_t bglevel, int flags)
	{
		// r_drawtrans is a seriously bad thing to turn off. I wonder if I should
		// just remove it completely.
		if (!r_drawtrans || (op == STYLEOP_Add && fglevel == FRACUNIT && bglevel == 0 && !(flags & STYLEF_InvertSource)))
		{
			if (flags & STYLEF_ColorIsFixed)
			{
				colfunc = &SWPixelFormatDrawers::FillColumn;
			}
			else if (TranslationMap() == nullptr)
			{
				colfunc = &SWPixelFormatDrawers::DrawColumn;
			}
			else
			{
				colfunc = &SWPixelFormatDrawers::DrawTranslatedColumn;
				drawer_needs_pal_input = true;
			}
			return true;
		}
		if (flags & STYLEF_InvertSource)
		{
			dc_srcblend = Col2RGB8_Inverse[fglevel >> 10];
			dc_destblend = Col2RGB8_LessPrecision[bglevel >> 10];
			dc_srcalpha = fglevel;
			dc_destalpha = bglevel;
		}
		else if (op == STYLEOP_Add && fglevel + bglevel <= FRACUNIT)
		{
			dc_srcblend = Col2RGB8[fglevel >> 10];
			dc_destblend = Col2RGB8[bglevel >> 10];
			dc_srcalpha = fglevel;
			dc_destalpha = bglevel;
		}
		else
		{
			dc_srcblend = Col2RGB8_LessPrecision[fglevel >> 10];
			dc_destblend = Col2RGB8_LessPrecision[bglevel >> 10];
			dc_srcalpha = fglevel;
			dc_destalpha = bglevel;
		}
		switch (op)
		{
		case STYLEOP_Add:
			if (fglevel == 0 && bglevel == FRACUNIT)
			{
				return false;
			}
			if (fglevel + bglevel <= FRACUNIT)
			{ // Colors won't overflow when added
				if (flags & STYLEF_ColorIsFixed)
				{
					colfunc = &SWPixelFormatDrawers::FillAddColumn;
				}
				else if (TranslationMap() == nullptr)
				{
					colfunc = &SWPixelFormatDrawers::DrawAddColumn;
				}
				else
				{
					colfunc = &SWPixelFormatDrawers::DrawTranslatedAddColumn;
					drawer_needs_pal_input = true;
				}
			}
			else
			{ // Colors might overflow when added
				if (flags & STYLEF_ColorIsFixed)
				{
					colfunc = &SWPixelFormatDrawers::FillAddClampColumn;
				}
				else if (TranslationMap() == nullptr)
				{
					colfunc = &SWPixelFormatDrawers::DrawAddClampColumn;
				}
				else
				{
					colfunc = &SWPixelFormatDrawers::DrawAddClampTranslatedColumn;
					drawer_needs_pal_input = true;
				}
			}
			return true;

		case STYLEOP_Sub:
			if (flags & STYLEF_ColorIsFixed)
			{
				colfunc = &SWPixelFormatDrawers::FillSubClampColumn;
			}
			else if (TranslationMap() == nullptr)
			{
				colfunc = &SWPixelFormatDrawers::DrawSubClampColumn;
			}
			else
			{
				colfunc = &SWPixelFormatDrawers::DrawSubClampTranslatedColumn;
				drawer_needs_pal_input = true;
			}
			return true;

		case STYLEOP_RevSub:
			if (fglevel == 0 && bglevel == FRACUNIT)
			{
				return false;
			}
			if (flags & STYLEF_ColorIsFixed)
			{
				colfunc = &SWPixelFormatDrawers::FillRevSubClampColumn;
			}
			else if (TranslationMap() == nullptr)
			{
				colfunc = &SWPixelFormatDrawers::DrawRevSubClampColumn;
			}
			else
			{
				colfunc = &SWPixelFormatDrawers::DrawRevSubClampTranslatedColumn;
				drawer_needs_pal_input = true;
			}
			return true;

		default:
			return false;
		}
	}

	fixed_t SpriteDrawerArgs::GetAlpha(int type, fixed_t alpha)
	{
		switch (type)
		{
		case STYLEALPHA_Zero:		return 0;
		case STYLEALPHA_One:		return OPAQUE;
		case STYLEALPHA_Src:		return alpha;
		case STYLEALPHA_InvSrc:		return OPAQUE - alpha;
		default:					return 0;
		}
	}

	bool SpriteDrawerArgs::SetStyle(RenderViewport *viewport, FRenderStyle style, fixed_t alpha, int translation, uint32_t color, const ColormapLight &light)
	{
		if (light.BaseColormap)
			SetLight(light);

		FDynamicColormap *basecolormap = light.BaseColormap ? static_cast<FDynamicColormap*>(light.BaseColormap) : nullptr;
		fixed_t shadedlightshade = light.ColormapNum << FRACBITS;

		fixed_t fglevel, bglevel;

		drawer_needs_pal_input = false;

		style.CheckFuzz();

		if (style.BlendOp == STYLEOP_Shadow)
		{
			style = LegacyRenderStyles[STYLE_TranslucentStencil];
			alpha = OPAQUE / 3;
			color = 0;
		}

		if (style.Flags & STYLEF_TransSoulsAlpha)
		{
			alpha = fixed_t(transsouls * OPAQUE);
		}
		else if (style.Flags & STYLEF_Alpha1)
		{
			alpha = OPAQUE;
		}
		else
		{
			alpha = clamp<fixed_t>(alpha, 0, OPAQUE);
		}
		
		if (translation != -1)
		{
			SetTranslationMap(nullptr);
			if (translation != 0)
			{
				FRemapTable *table = TranslationToTable(translation);
				if (table != NULL && !table->Inactive)
				{
					if (viewport->RenderTarget->IsBgra())
						SetTranslationMap((uint8_t*)table->Palette);
					else
						SetTranslationMap(table->Remap);
				}
			}
		}

		// Check for special modes
		if (style.BlendOp == STYLEOP_Fuzz)
		{
			colfunc = &SWPixelFormatDrawers::DrawFuzzColumn;
			return true;
		}
		else if (style == LegacyRenderStyles[STYLE_Shaded] || style == LegacyRenderStyles[STYLE_AddShaded])
		{
			// Shaded drawer only gets 16 levels of alpha because it saves memory.
			if ((alpha >>= 12) == 0 || basecolormap == nullptr)
				return false;
			if (style == LegacyRenderStyles[STYLE_Shaded])
				colfunc = &SWPixelFormatDrawers::DrawShadedColumn;
			else
				colfunc = &SWPixelFormatDrawers::DrawAddClampShadedColumn;
			drawer_needs_pal_input = true;
			CameraLight *cameraLight = CameraLight::Instance();
			dc_color = cameraLight->FixedColormap() ? cameraLight->FixedColormap()->Maps[APART(color)] : basecolormap->Maps[APART(color)];
			dc_color_bgra = color;
			basecolormap = &ShadeFakeColormap[16 - alpha];
			if (cameraLight->FixedLightLevel() >= 0 && !cameraLight->FixedColormap())
			{
				fixed_t shade = shadedlightshade;
				if (shade == 0) shade = cameraLight->FixedLightLevelShade();
				SetBaseColormap(basecolormap);
				SetLight(0, shade);
			}
			else
			{
				SetBaseColormap(basecolormap);
				SetLight(0, shadedlightshade);
			}
			return true;
		}

		fglevel = GetAlpha(style.SrcAlpha, alpha);
		bglevel = GetAlpha(style.DestAlpha, alpha);

		if (style.Flags & STYLEF_ColorIsFixed)
		{
			uint32_t x = fglevel >> 10;
			uint32_t r = RPART(color);
			uint32_t g = GPART(color);
			uint32_t b = BPART(color);
			// dc_color is used by the rt_* routines. It is indexed into dc_srcblend.
			dc_color = RGB256k.RGB[r >> 2][g >> 2][b >> 2];
			dc_color_bgra = color;
			if (style.Flags & STYLEF_InvertSource)
			{
				r = 255 - r;
				g = 255 - g;
				b = 255 - b;
			}
			uint32_t alpha = clamp(fglevel >> (FRACBITS - 8), 0, 255);
			dc_srccolor_bgra = (alpha << 24) | (r << 16) | (g << 8) | b;
			// dc_srccolor is used by the R_Fill* routines. It is premultiplied
			// with the alpha.
			dc_srccolor = ((((r*x) >> 4) << 20) | ((g*x) >> 4) | ((((b)*x) >> 4) << 10)) & 0x3feffbff;
			SetBaseColormap(&identitycolormap);
			SetLight(0, 0);
		}

		return SpriteDrawerArgs::SetBlendFunc(style.BlendOp, fglevel, bglevel, style.Flags);
	}

	bool SpriteDrawerArgs::SetStyle(RenderViewport *viewport, FRenderStyle style, float alpha, int translation, uint32_t color, const ColormapLight &light)
	{
		return SetStyle(viewport, style, FLOAT2FIXED(alpha), translation, color, light);
	}

	void SpriteDrawerArgs::DrawVoxelBlocks(RenderThread *thread, const VoxelBlock *blocks, int blockcount)
	{
		SetDest(thread->Viewport.get(), 0, 0);
		thread->Drawers(dc_viewport)->DrawVoxelBlocks(*this, blocks, blockcount);
	}

	void SpriteDrawerArgs::SetDest(RenderViewport *viewport, int x, int y)
	{
		dc_dest = viewport->GetDest(x, y);
		dc_dest_y = y;
		dc_viewport = viewport;
	}
}
