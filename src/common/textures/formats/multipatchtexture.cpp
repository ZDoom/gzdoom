/*
** multipatchtexture.cpp
** Texture class for standard Doom multipatch textures
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
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
**
*/

#include <ctype.h>
#include "files.h"
#include "filesystem.h"
#include "image.h"
#include "multipatchtexture.h"
#include "imagehelpers.h"

//==========================================================================
//
// FMultiPatchTexture :: FMultiPatchTexture
//
//==========================================================================

FMultiPatchTexture::FMultiPatchTexture(int w, int h, const TArray<TexPartBuild> &parts, bool complex, bool textual)
{
	Width = w;
	Height = h;
	bComplex = complex;
 	bTextual = textual;
	Parts = (TexPart*)ImageArena.Alloc(sizeof(TexPart) * parts.Size());
	NumParts = parts.Size();
	memcpy(Parts, parts.Data(), sizeof(TexPart) * parts.Size());
	for (unsigned i = 0; i < parts.Size(); i++)
	{
		Parts[i].Image = parts[i].TexImage->GetImage();
	}

	bUseGamePalette = false;
	if (!bComplex)
	{
		for (int i = 0; i < NumParts; i++)
		{
			if (!Parts[i].Image->UseGamePalette()) return;
		}
		bUseGamePalette = true;	// only if all patches use the game palette.
	}
}

//==========================================================================
//
// sky remapping will only happen if
// - the texture was defined through a TEXTUREx lump (this implies only trivial copies)
// - all patches use the base palette.
// - all patches are in a format that allows the remap.
// All other cases would not be able to properly deal with this special case.
// For textual definitions this hack isn't necessary.
//
//==========================================================================

bool FMultiPatchTexture::SupportRemap0()
{
	if (bTextual || UseGamePalette()) return false;
	for (int i = 0; i < NumParts; i++)
	{
		if (!Parts[i].Image->SupportRemap0()) return false;
	}
	return true;
}

//==========================================================================
//
// GetBlendMap
//
//==========================================================================

static uint8_t *GetBlendMap(PalEntry blend, uint8_t *blendwork)
{
	switch (blend.a==0 ? int(blend) : -1)
	{
	case BLEND_ICEMAP:
		return GPalette.IceMap.Remap;

	default:
		if (blend >= BLEND_SPECIALCOLORMAP1 && blend < BLEND_SPECIALCOLORMAP1 + SpecialColormaps.Size())
		{
			return SpecialColormaps[blend - BLEND_SPECIALCOLORMAP1].Colormap;
		}
		else if (blend >= BLEND_DESATURATE1 && blend <= BLEND_DESATURATE31)
		{
			return DesaturateColormap[blend - BLEND_DESATURATE1];
		}
		else 
		{
			blendwork[0]=0;
			if (blend.a == 255)
			{
				for(int i=1;i<256;i++)
				{
					int rr = (blend.r * GPalette.BaseColors[i].r) / 255;
					int gg = (blend.g * GPalette.BaseColors[i].g) / 255;
					int bb = (blend.b * GPalette.BaseColors[i].b) / 255;

					blendwork[i] = ColorMatcher.Pick(rr, gg, bb);
				}
				return blendwork;
			}
			else if (blend.a != 0)
			{
				for(int i=1;i<256;i++)
				{
					int rr = (blend.r * blend.a + GPalette.BaseColors[i].r * (255-blend.a)) / 255;
					int gg = (blend.g * blend.a + GPalette.BaseColors[i].g * (255-blend.a)) / 255;
					int bb = (blend.b * blend.a + GPalette.BaseColors[i].b * (255-blend.a)) / 255;

					blendwork[i] = ColorMatcher.Pick(rr, gg, bb);
				}
				return blendwork;
			}
		}
	}
	return nullptr;
}

//==========================================================================
//
// 
//
//==========================================================================

void FMultiPatchTexture::CopyToBlock(uint8_t *dest, int dwidth, int dheight, FImageSource *source, int xpos, int ypos, int rotate, const uint8_t *translation, int style)
{
	auto cimage = source->GetCachedPalettedPixels(style);	// should use composition cache
	auto &image = cimage.Pixels;
	const uint8_t *pixels = image.Data();
	int srcwidth = source->GetWidth();
	int srcheight = source->GetHeight();
	int step_x = source->GetHeight();
	int step_y = 1;
	FClipRect cr = { 0, 0, dwidth, dheight };
	if (style) translation = nullptr;	// do not apply translations to alpha textures.

	if (ClipCopyPixelRect(&cr, xpos, ypos, pixels, srcwidth, srcheight, step_x, step_y, rotate))
	{
		dest += ypos + dheight * xpos;
		if (translation == NULL)
		{
			for (int x = 0; x < srcwidth; x++)
			{
				int pos = x * dheight;
				for (int y = 0; y < srcheight; y++, pos++)
				{
					// the optimizer is doing a good enough job here so there's no need to optimize this by hand
					uint8_t v = pixels[y * step_y + x * step_x];
					if (v != 0) dest[pos] = v;
				}
			}
		}
		else
		{
			for (int x = 0; x < srcwidth; x++)
			{
				int pos = x * dheight;
				for (int y = 0; y < srcheight; y++, pos++)
				{
					uint8_t v = pixels[y * step_y + x * step_x];
					if (v != 0) dest[pos] = translation[v];
				}
			}
		}
	}
}

//==========================================================================
//
// FMultiPatchTexture :: MakeTexture
//
//==========================================================================

TArray<uint8_t> FMultiPatchTexture::CreatePalettedPixels(int conversion)
{
	int numpix = Width * Height;
	uint8_t blendwork[256];
	bool buildrgb = bComplex;

	TArray<uint8_t> Pixels(numpix, true);
	memset (Pixels.Data(), 0, numpix);

	if (conversion == luminance)
	{
		// For alpha textures, downconversion to the palette would lose too much precision if not all patches use the palette.
		buildrgb = !UseGamePalette();
	}
	else
	{
		// For regular textures we can use paletted compositing if all patches are just being copied because they all can create a paletted buffer.
		if (!buildrgb) for (int i = 0; i < NumParts; ++i)
		{
			if (Parts[i].op != OP_COPY)
			{
				buildrgb = true;
			}
		}
	}
	if (conversion == noremap0)
	{
		if (bTextual || !UseGamePalette()) conversion = normal;
	}

	if (!buildrgb)
	{	
		for (int i = 0; i < NumParts; ++i)
		{
			uint8_t *trans = Parts[i].Translation? Parts[i].Translation->Remap : nullptr;
			{
				if (Parts[i].Blend != 0)
				{
					trans = GetBlendMap(Parts[i].Blend, blendwork);
				}
				CopyToBlock (Pixels.Data(), Width, Height, Parts[i].Image, Parts[i].OriginX, Parts[i].OriginY, Parts[i].Rotate, trans, conversion);
			}
		}
	}
	else
	{
		// In case there are translucent patches let's do the composition in
		// True color to keep as much precision as possible before downconverting to the palette.
		FBitmap PixelsIn;
		PixelsIn.Create(Width, Height);
		CopyPixels(&PixelsIn, normal);
		for(int y = 0; y < Height; y++)
		{
			uint8_t *in = PixelsIn.GetPixels() + Width * y * 4;
			uint8_t *out = Pixels.Data() + y;
			for (int x = 0; x < Width; x++)
			{
				if (*out == 0 && in[3] != 0)
				{
					*out = ImageHelpers::RGBToPalette(conversion == luminance, in[2], in[1], in[0]);
				}
				out += Height;
				in += 4;
			}
		}
	}
	return Pixels;
}

//===========================================================================
//
// FMultipatchTexture::CopyTrueColorPixels
//
// Preserves the palettes of each individual patch
//
//===========================================================================

int FMultiPatchTexture::CopyPixels(FBitmap *bmp, int conversion)
{
	int retv = -1;

	if (conversion == noremap0)
	{
		if (bTextual || !UseGamePalette()) conversion = normal;
	}

	for(int i = 0; i < NumParts; i++)
	{
		int ret = -1;
		FCopyInfo info;

		memset (&info, 0, sizeof(info));
		info.alpha = Parts[i].Alpha;
		info.invalpha = BLENDUNIT - info.alpha;
		info.op = ECopyOp(Parts[i].op);
		PalEntry b = Parts[i].Blend;
		if (b.a == 0 && b != BLEND_NONE)
		{
			info.blend = EBlend(b.d);
		}
		else if (b.a != 0)
		{
			if (b.a == 255)
			{
				info.blendcolor[0] = b.r * BLENDUNIT / 255;
				info.blendcolor[1] = b.g * BLENDUNIT / 255;
				info.blendcolor[2] = b.b * BLENDUNIT / 255;
				info.blend = BLEND_MODULATE;
			}
			else
			{
				blend_t blendalpha = b.a * BLENDUNIT / 255;
				info.blendcolor[0] = b.r * blendalpha;
				info.blendcolor[1] = b.g * blendalpha;
				info.blendcolor[2] = b.b * blendalpha;
				info.blendcolor[3] = FRACUNIT - blendalpha;
				info.blend = BLEND_OVERLAY;
			}
		}

		auto trans = Parts[i].Translation ? Parts[i].Translation->Palette : nullptr;
		FBitmap Pixels = Parts[i].Image->GetCachedBitmap(trans, conversion, &ret);
		bmp->Blit(Parts[i].OriginX, Parts[i].OriginY, Pixels, Pixels.GetWidth(), Pixels.GetHeight(), Parts[i].Rotate, &info);
		// treat -1 (i.e. unknown) as absolute. We have no idea if this may have overwritten previous info so a real check needs to be done.
		if (ret == -1) retv = ret;
		else if (retv != -1 && ret > retv) retv = ret;
	}
	return retv;
}

//==========================================================================
//
//
//
//==========================================================================

void FMultiPatchTexture::CollectForPrecache(PrecacheInfo &info, bool requiretruecolor)
{
	FImageSource::CollectForPrecache(info, requiretruecolor);

	if (!requiretruecolor)
	{
		requiretruecolor = bComplex;

		if (!requiretruecolor) for (int i = 0; i < NumParts; ++i)
		{
			if (Parts[i].op != OP_COPY)	requiretruecolor = true;
		}
	}
	for (int i = 0; i < NumParts; ++i)
	{
		Parts[i].Image->CollectForPrecache(info, requiretruecolor);
	}
}


