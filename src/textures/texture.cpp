/*
** texture.cpp
** The base texture class
**
**---------------------------------------------------------------------------
** Copyright 2004-2007 Randy Heit
** Copyright 2006-2018 Christoph Oelckers
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

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "templates.h"
#include "i_system.h"
#include "r_data/r_translate.h"
#include "bitmap.h"
#include "colormatcher.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "m_fixed.h"
#include "textures/warpbuffer.h"
#include "hwrenderer/textures/hw_material.h"
#include "hwrenderer/textures/hw_ihwtexture.h"

FTexture *CreateBrightmapTexture(FTexture*);

// Make sprite offset adjustment user-configurable per renderer.
int r_spriteadjustSW, r_spriteadjustHW;
CUSTOM_CVAR(Int, r_spriteadjust, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	r_spriteadjustHW = !!(self & 2);
	r_spriteadjustSW = !!(self & 1);
	TexMan.SpriteAdjustChanged();
}

//==========================================================================
//
// 
//
//==========================================================================

uint8_t FTexture::GrayMap[256];

void FTexture::InitGrayMap()
{
	for (int i = 0; i < 256; ++i)
	{
		GrayMap[i] = ColorMatcher.Pick(i, i, i);
	}
}


//==========================================================================
//
//
//
//==========================================================================

typedef FTexture * (*CreateFunc)(FileReader & file, int lumpnum);

struct TexCreateInfo
{
	CreateFunc TryCreate;
	ETextureType usetype;
};

FTexture *IMGZTexture_TryCreate(FileReader &, int lumpnum);
FTexture *PNGTexture_TryCreate(FileReader &, int lumpnum);
FTexture *JPEGTexture_TryCreate(FileReader &, int lumpnum);
FTexture *DDSTexture_TryCreate(FileReader &, int lumpnum);
FTexture *PCXTexture_TryCreate(FileReader &, int lumpnum);
FTexture *TGATexture_TryCreate(FileReader &, int lumpnum);
FTexture *RawPageTexture_TryCreate(FileReader &, int lumpnum);
FTexture *FlatTexture_TryCreate(FileReader &, int lumpnum);
FTexture *PatchTexture_TryCreate(FileReader &, int lumpnum);
FTexture *EmptyTexture_TryCreate(FileReader &, int lumpnum);
FTexture *AutomapTexture_TryCreate(FileReader &, int lumpnum);


// Examines the lump contents to decide what type of texture to create,
// and creates the texture.
FTexture * FTexture::CreateTexture (int lumpnum, ETextureType usetype)
{
	static TexCreateInfo CreateInfo[]={
		{ IMGZTexture_TryCreate,		ETextureType::Any },
		{ PNGTexture_TryCreate,			ETextureType::Any },
		{ JPEGTexture_TryCreate,		ETextureType::Any },
		{ DDSTexture_TryCreate,			ETextureType::Any },
		{ PCXTexture_TryCreate,			ETextureType::Any },
		{ TGATexture_TryCreate,			ETextureType::Any },
		{ RawPageTexture_TryCreate,		ETextureType::MiscPatch },
		{ FlatTexture_TryCreate,		ETextureType::Flat },
		{ PatchTexture_TryCreate,		ETextureType::Any },
		{ EmptyTexture_TryCreate,		ETextureType::Any },
		{ AutomapTexture_TryCreate,		ETextureType::MiscPatch },
	};

	if (lumpnum == -1) return NULL;

	auto data = Wads.OpenLumpReader (lumpnum);

	for(size_t i = 0; i < countof(CreateInfo); i++)
	{
		if ((CreateInfo[i].usetype == usetype || CreateInfo[i].usetype == ETextureType::Any))
		{
			FTexture * tex = CreateInfo[i].TryCreate(data, lumpnum);
			if (tex != NULL) 
			{
				tex->UseType = usetype;
				if (usetype == ETextureType::Flat) 
				{
					int w = tex->GetWidth();
					int h = tex->GetHeight();

					// Auto-scale flats with dimensions 128x128 and 256x256.
					// In hindsight, a bad idea, but RandomLag made it sound better than it really is.
					// Now we're stuck with this stupid behaviour.
					if (w==128 && h==128) 
					{
						tex->Scale.X = tex->Scale.Y = 2;
						tex->bWorldPanning = true;
					}
					else if (w==256 && h==256) 
					{
						tex->Scale.X = tex->Scale.Y = 4;
						tex->bWorldPanning = true;
					}
				}
				return tex;
			}
		}
	}
	return NULL;
}

FTexture * FTexture::CreateTexture (const char *name, int lumpnum, ETextureType usetype)
{
	FTexture *tex = CreateTexture(lumpnum, usetype);
	if (tex != NULL && name != NULL) {
		tex->Name = name;
		tex->Name.ToUpper();
	}
	return tex;
}

//==========================================================================
//
// 
//
//==========================================================================

FTexture::FTexture (const char *name, int lumpnum)
	: 
  WidthBits(0), HeightBits(0), Scale(1,1), SourceLump(lumpnum),
  UseType(ETextureType::Any), bNoDecals(false), bNoRemap0(false), bWorldPanning(false),
  bMasked(true), bAlphaTexture(false), bHasCanvas(false), bWarped(0), bComplex(false), bMultiPatch(false), bKeepAround(false), bFullNameTexture(false),
	Rotations(0xFFFF), SkyOffset(0), Width(0), Height(0), WidthMask(0)
{
	bBrightmapChecked = false;
	bGlowing = false;
	bAutoGlowing = false;
	bFullbright = false;
	bDisableFullbright = false;
	bSkybox = false;
	bNoCompress = false;
	bNoExpand = false;
	bTranslucent = -1;


	_LeftOffset[0] = _LeftOffset[1] = _TopOffset[0] = _TopOffset[1] = 0;
	id.SetInvalid();
	if (name != NULL)
	{
		Name = name;
		Name.ToUpper();
	}
	else if (lumpnum < 0)
	{
		Name = FString();
	}
	else
	{
		Wads.GetLumpName (Name, lumpnum);
	}
}

FTexture::~FTexture ()
{
	FTexture *link = Wads.GetLinkedTexture(SourceLump);
	if (link == this) Wads.SetLinkedTexture(SourceLump, nullptr);
	if (areas != nullptr) delete[] areas;
	areas = nullptr;

	for (int i = 0; i < 2; i++)
	{
		if (Material[i] != nullptr) delete Material[i];
		Material[i] = nullptr;

		if (SystemTexture[i] != nullptr) delete SystemTexture[i];
		SystemTexture[i] = nullptr;
	}

}

void FTexture::Unload()
{
	PixelsBgra = std::vector<uint32_t>();
}

//==========================================================================
//
// 
//
//==========================================================================

const uint32_t *FTexture::GetColumnBgra(unsigned int column, const Span **spans_out)
{
	const uint32_t *pixels = GetPixelsBgra();
	if (pixels == nullptr) return nullptr;

	column %= Width;

	if (spans_out != nullptr)
		GetColumn(DefaultRenderStyle(), column, spans_out);	// This isn't the right way to create the spans.
	return pixels + column * Height;
}

const uint32_t *FTexture::GetPixelsBgra()
{
	if (PixelsBgra.empty() || CheckModified(DefaultRenderStyle()))
	{
		if (!GetColumn(DefaultRenderStyle(), 0, nullptr))
			return nullptr;

		FBitmap bitmap;
		bitmap.Create(GetWidth(), GetHeight());
		CopyTrueColorPixels(&bitmap, 0, 0);
		GenerateBgraFromBitmap(bitmap);
	}
	return PixelsBgra.data();
}

//==========================================================================
//
// 
//
//==========================================================================

bool FTexture::CheckModified (FRenderStyle)
{
	return false;
}

FTextureFormat FTexture::GetFormat()
{
	return TEX_Pal;
}

void FTexture::SetFrontSkyLayer ()
{
	bNoRemap0 = true;
}

//==========================================================================
//
// 
//
//==========================================================================

void FTexture::CalcBitSize ()
{
	// WidthBits is rounded down, and HeightBits is rounded up
	int i;

	for (i = 0; (1 << i) < Width; ++i)
	{ }

	WidthBits = i;

	// Having WidthBits that would allow for columns past the end of the
	// texture is not allowed, even if it means the entire texture is
	// not drawn.
	if (Width < (1 << WidthBits))
	{
		WidthBits--;
	}
	WidthMask = (1 << WidthBits) - 1;

	// <hr>The minimum height is 2, because we cannot shift right 32 bits.</hr>
	// Scratch that. Somebody actually made a 1x1 texture, so now we have to handle it.
	for (i = 0; (1 << i) < Height; ++i)
	{ }

	HeightBits = i;
}

//==========================================================================
//
// 
//
//==========================================================================

FTexture::Span **FTexture::CreateSpans (const uint8_t *pixels) const
{
	Span **spans, *span;

	if (!bMasked)
	{ // Texture does not have holes, so it can use a simpler span structure
		spans = (Span **)M_Malloc (sizeof(Span*)*Width + sizeof(Span)*2);
		span = (Span *)&spans[Width];
		for (int x = 0; x < Width; ++x)
		{
			spans[x] = span;
		}
		span[0].Length = Height;
		span[0].TopOffset = 0;
		span[1].Length = 0;
		span[1].TopOffset = 0;
	}
	else
	{ // Texture might have holes, so build a complete span structure
		int numcols = Width;
		int numrows = Height;
		int numspans = numcols;	// One span to terminate each column
		const uint8_t *data_p;
		bool newspan;
		int x, y;

		data_p = pixels;

		// Count the number of spans in this texture
		for (x = numcols; x > 0; --x)
		{
			newspan = true;
			for (y = numrows; y > 0; --y)
			{

				if (*data_p++ == 0)
				{
					if (!newspan)
					{
						newspan = true;
					}
				}
				else if (newspan)
				{
					newspan = false;
					numspans++;
				}
			}
		}

		// Allocate space for the spans
		spans = (Span **)M_Malloc (sizeof(Span*)*numcols + sizeof(Span)*numspans);

		// Fill in the spans
		for (x = 0, span = (Span *)&spans[numcols], data_p = pixels; x < numcols; ++x)
		{
			newspan = true;
			spans[x] = span;
			for (y = 0; y < numrows; ++y)
			{
				if (*data_p++ == 0)
				{
					if (!newspan)
					{
						newspan = true;
						span++;
					}
				}
				else
				{
					if (newspan)
					{
						newspan = false;
						span->TopOffset = y;
						span->Length = 1;
					}
					else
					{
						span->Length++;
					}
				}
			}
			if (!newspan)
			{
				span++;
			}
			span->TopOffset = 0;
			span->Length = 0;
			span++;
		}
	}
	return spans;
}

void FTexture::FreeSpans (Span **spans) const
{
	M_Free (spans);
}

//==========================================================================
//
// 
//
//==========================================================================

void FTexture::GenerateBgraFromBitmap(const FBitmap &bitmap)
{
	CreatePixelsBgraWithMipmaps();

	// Transpose
	const uint32_t *src = (const uint32_t *)bitmap.GetPixels();
	uint32_t *dest = PixelsBgra.data();
	for (int x = 0; x < Width; x++)
	{
		for (int y = 0; y < Height; y++)
		{
			dest[y + x * Height] = src[x + y * Width];
		}
	}

	GenerateBgraMipmaps();
}

void FTexture::CreatePixelsBgraWithMipmaps()
{
	int levels = MipmapLevels();
	int buffersize = 0;
	for (int i = 0; i < levels; i++)
	{
		int w = MAX(Width >> i, 1);
		int h = MAX(Height >> i, 1);
		buffersize += w * h;
	}
	PixelsBgra.resize(buffersize, 0xffff0000);
}

int FTexture::MipmapLevels() const
{
	int widthbits = 0;
	while ((Width >> widthbits) != 0) widthbits++;

	int heightbits = 0;
	while ((Height >> heightbits) != 0) heightbits++;

	return MAX(widthbits, heightbits);
}

//==========================================================================
//
// 
//
//==========================================================================

void FTexture::GenerateBgraMipmaps()
{
	struct Color4f
	{
		float a, r, g, b;
		Color4f operator*(const Color4f &v) const { return Color4f{ a * v.a, r * v.r, g * v.g, b * v.b }; }
		Color4f operator/(const Color4f &v) const { return Color4f{ a / v.a, r / v.r, g / v.g, b / v.b }; }
		Color4f operator+(const Color4f &v) const { return Color4f{ a + v.a, r + v.r, g + v.g, b + v.b }; }
		Color4f operator-(const Color4f &v) const { return Color4f{ a - v.a, r - v.r, g - v.g, b - v.b }; }
		Color4f operator*(float s) const { return Color4f{ a * s, r * s, g * s, b * s }; }
		Color4f operator/(float s) const { return Color4f{ a / s, r / s, g / s, b / s }; }
		Color4f operator+(float s) const { return Color4f{ a + s, r + s, g + s, b + s }; }
		Color4f operator-(float s) const { return Color4f{ a - s, r - s, g - s, b - s }; }
	};

	int levels = MipmapLevels();
	std::vector<Color4f> image(PixelsBgra.size());

	// Convert to normalized linear colorspace
	{
		for (int x = 0; x < Width; x++)
		{
			for (int y = 0; y < Height; y++)
			{
				uint32_t c8 = PixelsBgra[x * Height + y];
				Color4f c;
				c.a = powf(APART(c8) * (1.0f / 255.0f), 2.2f);
				c.r = powf(RPART(c8) * (1.0f / 255.0f), 2.2f);
				c.g = powf(GPART(c8) * (1.0f / 255.0f), 2.2f);
				c.b = powf(BPART(c8) * (1.0f / 255.0f), 2.2f);
				image[x * Height + y] = c;
			}
		}
	}

	// Generate mipmaps
	{
		std::vector<Color4f> smoothed(Width * Height);
		Color4f *src = image.data();
		Color4f *dest = src + Width * Height;
		for (int i = 1; i < levels; i++)
		{
			int srcw = MAX(Width >> (i - 1), 1);
			int srch = MAX(Height >> (i - 1), 1);
			int w = MAX(Width >> i, 1);
			int h = MAX(Height >> i, 1);

			// Downscale
			for (int x = 0; x < w; x++)
			{
				int sx0 = x * 2;
				int sx1 = MIN((x + 1) * 2, srcw - 1);
				for (int y = 0; y < h; y++)
				{
					int sy0 = y * 2;
					int sy1 = MIN((y + 1) * 2, srch - 1);

					Color4f src00 = src[sy0 + sx0 * srch];
					Color4f src01 = src[sy1 + sx0 * srch];
					Color4f src10 = src[sy0 + sx1 * srch];
					Color4f src11 = src[sy1 + sx1 * srch];
					Color4f c = (src00 + src01 + src10 + src11) * 0.25f;

					dest[y + x * h] = c;
				}
			}

			// Sharpen filter with a 3x3 kernel:
			for (int x = 0; x < w; x++)
			{
				for (int y = 0; y < h; y++)
				{
					Color4f c = { 0.0f, 0.0f, 0.0f, 0.0f };
					for (int kx = -1; kx < 2; kx++)
					{
						for (int ky = -1; ky < 2; ky++)
						{
							int a = y + ky;
							int b = x + kx;
							if (a < 0) a = h - 1;
							if (a == h) a = 0;
							if (b < 0) b = w - 1;
							if (b == w) b = 0;
							c = c + dest[a + b * h];
						}
					}
					c = c * (1.0f / 9.0f);
					smoothed[y + x * h] = c;
				}
			}
			float k = 0.08f;
			for (int j = 0; j < w * h; j++)
				dest[j] = dest[j] + (dest[j] - smoothed[j]) * k;

			src = dest;
			dest += w * h;
		}
	}

	// Convert to bgra8 sRGB colorspace
	{
		Color4f *src = image.data() + Width * Height;
		uint32_t *dest = PixelsBgra.data() + Width * Height;
		for (int i = 1; i < levels; i++)
		{
			int w = MAX(Width >> i, 1);
			int h = MAX(Height >> i, 1);
			for (int j = 0; j < w * h; j++)
			{
				uint32_t a = (uint32_t)clamp(powf(MAX(src[j].a, 0.0f), 1.0f / 2.2f) * 255.0f + 0.5f, 0.0f, 255.0f);
				uint32_t r = (uint32_t)clamp(powf(MAX(src[j].r, 0.0f), 1.0f / 2.2f) * 255.0f + 0.5f, 0.0f, 255.0f);
				uint32_t g = (uint32_t)clamp(powf(MAX(src[j].g, 0.0f), 1.0f / 2.2f) * 255.0f + 0.5f, 0.0f, 255.0f);
				uint32_t b = (uint32_t)clamp(powf(MAX(src[j].b, 0.0f), 1.0f / 2.2f) * 255.0f + 0.5f, 0.0f, 255.0f);
				dest[j] = (a << 24) | (r << 16) | (g << 8) | b;
			}
			src += w * h;
			dest += w * h;
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FTexture::GenerateBgraMipmapsFast()
{
	uint32_t *src = PixelsBgra.data();
	uint32_t *dest = src + Width * Height;
	int levels = MipmapLevels();
	for (int i = 1; i < levels; i++)
	{
		int srcw = MAX(Width >> (i - 1), 1);
		int srch = MAX(Height >> (i - 1), 1);
		int w = MAX(Width >> i, 1);
		int h = MAX(Height >> i, 1);

		for (int x = 0; x < w; x++)
		{
			int sx0 = x * 2;
			int sx1 = MIN((x + 1) * 2, srcw - 1);

			for (int y = 0; y < h; y++)
			{
				int sy0 = y * 2;
				int sy1 = MIN((y + 1) * 2, srch - 1);

				uint32_t src00 = src[sy0 + sx0 * srch];
				uint32_t src01 = src[sy1 + sx0 * srch];
				uint32_t src10 = src[sy0 + sx1 * srch];
				uint32_t src11 = src[sy1 + sx1 * srch];

				uint32_t alpha = (APART(src00) + APART(src01) + APART(src10) + APART(src11) + 2) / 4;
				uint32_t red = (RPART(src00) + RPART(src01) + RPART(src10) + RPART(src11) + 2) / 4;
				uint32_t green = (GPART(src00) + GPART(src01) + GPART(src10) + GPART(src11) + 2) / 4;
				uint32_t blue = (BPART(src00) + BPART(src01) + BPART(src10) + BPART(src11) + 2) / 4;

				dest[y + x * h] = (alpha << 24) | (red << 16) | (green << 8) | blue;
			}
		}

		src = dest;
		dest += w * h;
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FTexture::CopyToBlock (uint8_t *dest, int dwidth, int dheight, int xpos, int ypos, int rotate, const uint8_t *translation, FRenderStyle style)
{
	const uint8_t *pixels = GetPixels(style);
	int srcwidth = Width;
	int srcheight = Height;
	int step_x = Height;
	int step_y = 1;
	FClipRect cr = {0, 0, dwidth, dheight};
	if (style.Flags & STYLEF_RedIsAlpha) translation = nullptr;	// do not apply translations to alpha textures.

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
// Converts a texture between row-major and column-major format
// by flipping it about the X=Y axis.
//
//==========================================================================

void FTexture::FlipSquareBlock (uint8_t *block, int x, int y)
{
	int i, j;

	if (x != y) return;

	for (i = 0; i < x; ++i)
	{
		uint8_t *corner = block + x*i + i;
		int count = x - i;
		if (count & 1)
		{
			count--;
			swapvalues<uint8_t> (corner[count], corner[count*x]);
		}
		for (j = 0; j < count; j += 2)
		{
			swapvalues<uint8_t> (corner[j], corner[j*x]);
			swapvalues<uint8_t> (corner[j+1], corner[(j+1)*x]);
		}
	}
}

void FTexture::FlipSquareBlockBgra(uint32_t *block, int x, int y)
{
	int i, j;

	if (x != y) return;

	for (i = 0; i < x; ++i)
	{
		uint32_t *corner = block + x*i + i;
		int count = x - i;
		if (count & 1)
		{
			count--;
			swapvalues<uint32_t>(corner[count], corner[count*x]);
		}
		for (j = 0; j < count; j += 2)
		{
			swapvalues<uint32_t>(corner[j], corner[j*x]);
			swapvalues<uint32_t>(corner[j + 1], corner[(j + 1)*x]);
		}
	}
}

void FTexture::FlipSquareBlockRemap (uint8_t *block, int x, int y, const uint8_t *remap)
{
	int i, j;
	uint8_t t;

	if (x != y) return;

	for (i = 0; i < x; ++i)
	{
		uint8_t *corner = block + x*i + i;
		int count = x - i;
		if (count & 1)
		{
			count--;
			t = remap[corner[count]];
			corner[count] = remap[corner[count*x]];
			corner[count*x] = t;
		}
		for (j = 0; j < count; j += 2)
		{
			t = remap[corner[j]];
			corner[j] = remap[corner[j*x]];
			corner[j*x] = t;
			t = remap[corner[j+1]];
			corner[j+1] = remap[corner[(j+1)*x]];
			corner[(j+1)*x] = t;
		}
	}
}

void FTexture::FlipNonSquareBlock (uint8_t *dst, const uint8_t *src, int x, int y, int srcpitch)
{
	int i, j;

	for (i = 0; i < x; ++i)
	{
		for (j = 0; j < y; ++j)
		{
			dst[i*y+j] = src[i+j*srcpitch];
		}
	}
}

void FTexture::FlipNonSquareBlockBgra(uint32_t *dst, const uint32_t *src, int x, int y, int srcpitch)
{
	int i, j;

	for (i = 0; i < x; ++i)
	{
		for (j = 0; j < y; ++j)
		{
			dst[i*y + j] = src[i + j*srcpitch];
		}
	}
}

void FTexture::FlipNonSquareBlockRemap (uint8_t *dst, const uint8_t *src, int x, int y, int srcpitch, const uint8_t *remap)
{
	int i, j;

	for (i = 0; i < x; ++i)
	{
		for (j = 0; j < y; ++j)
		{
			dst[i*y+j] = remap[src[i+j*srcpitch]];
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FTexture::FillBuffer(uint8_t *buff, int pitch, int height, FTextureFormat fmt)
{
	const uint8_t *pix;
	int x, y, w, h, stride;

	w = GetWidth();
	h = GetHeight();

	switch (fmt)
	{
	case TEX_Pal:
	case TEX_Gray:
		pix = GetPixels(fmt == TEX_Pal? DefaultRenderStyle() : LegacyRenderStyles[STYLE_Shaded]);
		stride = pitch - w;
		for (y = 0; y < h; ++y)
		{
			const uint8_t *pix2 = pix;
			for (x = 0; x < w; ++x)
			{
				*buff++ = *pix2;
				pix2 += h;
			}
			pix++;
			buff += stride;
		}
		break;

	case TEX_RGB:
	{
		FCopyInfo inf = {OP_OVERWRITE, BLEND_NONE, {0}, 0, 0};
		FBitmap bmp(buff, pitch, pitch/4, height);
		CopyTrueColorPixels(&bmp, 0, 0, 0, &inf); 
		break;
	}

	default:
		I_Error("FTexture::FillBuffer: Unsupported format %d", fmt);
	}
}

//===========================================================================
//
// FTexture::CopyTrueColorPixels 
//
// this is the generic case that can handle
// any properly implemented texture for software rendering.
// Its drawback is that it is limited to the base palette which is
// why all classes that handle different palettes should subclass this
// method
//
//===========================================================================

int FTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	PalEntry *palette = screen->GetPalette();
	for(int i=1;i<256;i++) palette[i].a = 255;	// set proper alpha values
	bmp->CopyPixelData(x, y, GetPixels(DefaultRenderStyle()), Width, Height, Height, 1, rotate, palette, inf);
	for(int i=1;i<256;i++) palette[i].a = 0;
	return 0;
}

int FTexture::CopyTrueColorTranslated(FBitmap *bmp, int x, int y, int rotate, PalEntry *remap, FCopyInfo *inf)
{
	bmp->CopyPixelData(x, y, GetPixels(DefaultRenderStyle()), Width, Height, Height, 1, rotate, remap, inf);
	return 0;
}

//==========================================================================
//
// 
//
//==========================================================================

bool FTexture::UseBasePalette()
{ 
	return true; 
}

FTexture *FTexture::GetRedirect()
{
	return this;
}

FTexture *FTexture::GetRawTexture()
{
	return this;
}

void FTexture::SetScaledSize(int fitwidth, int fitheight)
{
	Scale.X = double(Width) / fitwidth;
	Scale.Y =double(Height) / fitheight;
	// compensate for roundoff errors
	if (int(Scale.X * fitwidth) != Width) Scale.X += (1 / 65536.);
	if (int(Scale.Y * fitheight) != Height) Scale.Y += (1 / 65536.);
}

//===========================================================================
// 
//	Gets the average color of a texture for use as a sky cap color
//
//===========================================================================

PalEntry FTexture::averageColor(const uint32_t *data, int size, int maxout)
{
	int				i;
	unsigned int	r, g, b;

	// First clear them.
	r = g = b = 0;
	if (size == 0)
	{
		return PalEntry(255, 255, 255);
	}
	for (i = 0; i < size; i++)
	{
		b += BPART(data[i]);
		g += GPART(data[i]);
		r += RPART(data[i]);
	}

	r = r / size;
	g = g / size;
	b = b / size;

	int maxv = MAX(MAX(r, g), b);

	if (maxv && maxout)
	{
		r = ::Scale(r, maxout, maxv);
		g = ::Scale(g, maxout, maxv);
		b = ::Scale(b, maxout, maxv);
	}
	return PalEntry(255, r, g, b);
}

PalEntry FTexture::GetSkyCapColor(bool bottom)
{
	if (!bSWSkyColorDone)
	{
		bSWSkyColorDone = true;

		FBitmap bitmap;
		bitmap.Create(GetWidth(), GetHeight());
		CopyTrueColorPixels(&bitmap, 0, 0);
		int w = GetWidth();
		int h = GetHeight();

		const uint32_t *buffer = (const uint32_t *)bitmap.GetPixels();
		if (buffer)
		{
			CeilingSkyColor = averageColor((uint32_t *)buffer, w * MIN(30, h), 0);
			if (h>30)
			{
				FloorSkyColor = averageColor(((uint32_t *)buffer) + (h - 30)*w, w * 30, 0);
			}
			else FloorSkyColor = CeilingSkyColor;
		}
	}
	return bottom ? FloorSkyColor : CeilingSkyColor;
}

//====================================================================
//
// CheckRealHeight
//
// Checks the posts in a texture and returns the lowest row (plus one)
// of the texture that is actually used.
//
//====================================================================

int FTexture::CheckRealHeight()
{
	const FTexture::Span *span;
	int maxy = 0, miny = GetHeight();

	for (int i = 0; i < GetWidth(); ++i)
	{
		GetColumn(DefaultRenderStyle(), i, &span);
		while (span->Length != 0)
		{
			if (span->TopOffset < miny)
			{
				miny = span->TopOffset;
			}
			if (span->TopOffset + span->Length > maxy)
			{
				maxy = span->TopOffset + span->Length;
			}
			span++;
		}
	}
	// Scale maxy before returning it
	maxy = int((maxy * 2) / Scale.Y);
	maxy = (maxy >> 1) + (maxy & 1);
	return maxy;
}

//==========================================================================
//
// Search auto paths for extra material textures
//
//==========================================================================

void FTexture::AddAutoMaterials()
{
	struct AutoTextureSearchPath
	{
		const char *path;
		FTexture *FTexture::*pointer;
	};

	static AutoTextureSearchPath autosearchpaths[] =
	{
		{ "brightmaps/", &FTexture::Brightmap }, // For backwards compatibility, only for short names
	{ "materials/brightmaps/", &FTexture::Brightmap },
	{ "materials/normalmaps/", &FTexture::Normal },
	{ "materials/specular/", &FTexture::Specular },
	{ "materials/metallic/", &FTexture::Metallic },
	{ "materials/roughness/", &FTexture::Roughness },
	{ "materials/ao/", &FTexture::AmbientOcclusion }
	};


	int startindex = bFullNameTexture ? 1 : 0;
	FString searchname = Name;

	if (bFullNameTexture)
	{
		auto dot = searchname.LastIndexOf('.');
		auto slash = searchname.LastIndexOf('/');
		if (dot > slash) searchname.Truncate(dot);
	}

	for (size_t i = 0; i < countof(autosearchpaths); i++)
	{
		auto &layer = autosearchpaths[i];
		if (this->*(layer.pointer) == nullptr)	// only if no explicit assignment had been done.
		{
			FStringf lookup("%s%s%s", layer.path, bFullNameTexture ? "" : "auto/", searchname.GetChars());
			auto lump = Wads.CheckNumForFullName(lookup, false, ns_global, true);
			if (lump != -1)
			{
				auto bmtex = TexMan.FindTexture(Wads.GetLumpFullName(lump), ETextureType::Any, FTextureManager::TEXMAN_TryAny);
				if (bmtex != nullptr)
				{
					bmtex->bMasked = false;
					this->*(layer.pointer) = bmtex;
				}
			}
		}
	}
}

//===========================================================================
// 
// Checks if the texture has a default brightmap and creates it if so
//
//===========================================================================
void FTexture::CreateDefaultBrightmap()
{
	if (!bBrightmapChecked)
	{
		// Check for brightmaps
		if (UseBasePalette() && TexMan.HasGlobalBrightmap &&
			UseType != ETextureType::Decal && UseType != ETextureType::MiscPatch && UseType != ETextureType::FontChar &&
			Brightmap == NULL && bWarped == 0 &&
			GetPixels(DefaultRenderStyle())
			)
		{
			// May have one - let's check when we use this texture
			const uint8_t *texbuf = GetPixels(DefaultRenderStyle());
			const int white = ColorMatcher.Pick(255, 255, 255);

			int size = GetWidth() * GetHeight();
			for (int i = 0; i<size; i++)
			{
				if (TexMan.GlobalBrightmap.Remap[texbuf[i]] == white)
				{
					// Create a brightmap
					DPrintf(DMSG_NOTIFY, "brightmap created for texture '%s'\n", Name.GetChars());
					Brightmap = CreateBrightmapTexture(this);
					bBrightmapChecked = true;
					TexMan.AddTexture(Brightmap);
					return;
				}
			}
			// No bright pixels found
			DPrintf(DMSG_SPAMMY, "No bright pixels found in texture '%s'\n", Name.GetChars());
			bBrightmapChecked = 1;
		}
		else
		{
			// does not have one so set the flag to 'done'
			bBrightmapChecked = 1;
		}
	}
}


//==========================================================================
//
// Calculates glow color for a texture
//
//==========================================================================

void FTexture::GetGlowColor(float *data)
{
	if (bGlowing && GlowColor == 0)
	{
		int w = Width, h = Height;
		auto buffer = new uint8_t[w * h * 4];
		if (buffer)
		{
			FillBuffer(buffer, w * 4, h, TEX_RGB);
			GlowColor = averageColor((uint32_t *)buffer, w*h, 153);
			delete[] buffer;
		}

		// Black glow equals nothing so switch glowing off
		if (GlowColor == 0) bGlowing = false;
	}
	data[0] = GlowColor.r / 255.0f;
	data[1] = GlowColor.g / 255.0f;
	data[2] = GlowColor.b / 255.0f;
}

//===========================================================================
// 
//	Finds gaps in the texture which can be skipped by the renderer
//  This was mainly added to speed up one area in E4M6 of 007LTSD
//
//===========================================================================

bool FTexture::FindHoles(const unsigned char * buffer, int w, int h)
{
	const unsigned char * li;
	int y, x;
	int startdraw, lendraw;
	int gaps[5][2];
	int gapc = 0;


	// already done!
	if (areacount) return false;
	if (UseType == ETextureType::Flat) return false;	// flats don't have transparent parts
	areacount = -1;	//whatever happens next, it shouldn't be done twice!

							// large textures are excluded for performance reasons
	if (h>512) return false;

	startdraw = -1;
	lendraw = 0;
	for (y = 0; y<h; y++)
	{
		li = buffer + w * y * 4 + 3;

		for (x = 0; x<w; x++, li += 4)
		{
			if (*li != 0) break;
		}

		if (x != w)
		{
			// non - transparent
			if (startdraw == -1)
			{
				startdraw = y;
				// merge transparent gaps of less than 16 pixels into the last drawing block
				if (gapc && y <= gaps[gapc - 1][0] + gaps[gapc - 1][1] + 16)
				{
					gapc--;
					startdraw = gaps[gapc][0];
					lendraw = y - startdraw;
				}
				if (gapc == 4) return false;	// too many splits - this isn't worth it
			}
			lendraw++;
		}
		else if (startdraw != -1)
		{
			if (lendraw == 1) lendraw = 2;
			gaps[gapc][0] = startdraw;
			gaps[gapc][1] = lendraw;
			gapc++;

			startdraw = -1;
			lendraw = 0;
		}
	}
	if (startdraw != -1)
	{
		gaps[gapc][0] = startdraw;
		gaps[gapc][1] = lendraw;
		gapc++;
	}
	if (startdraw == 0 && lendraw == h) return false;	// nothing saved so don't create a split list

	if (gapc > 0)
	{
		FloatRect * rcs = new FloatRect[gapc];

		for (x = 0; x < gapc; x++)
		{
			// gaps are stored as texture (u/v) coordinates
			rcs[x].width = rcs[x].left = -1.0f;
			rcs[x].top = (float)gaps[x][0] / (float)h;
			rcs[x].height = (float)gaps[x][1] / (float)h;
		}
		areas = rcs;
	}
	else areas = nullptr;
	areacount = gapc;

	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FTexture::CheckTrans(unsigned char * buffer, int size, int trans)
{
	if (bTranslucent == -1)
	{
		bTranslucent = trans;
		if (trans == -1)
		{
			uint32_t * dwbuf = (uint32_t*)buffer;
			for (int i = 0; i<size; i++)
			{
				uint32_t alpha = dwbuf[i] >> 24;

				if (alpha != 0xff && alpha != 0)
				{
					bTranslucent = 1;
					return;
				}
			}
			bTranslucent = 0;
		}
	}
}


//===========================================================================
// 
// smooth the edges of transparent fields in the texture
//
//===========================================================================

#ifdef WORDS_BIGENDIAN
#define MSB 0
#define SOME_MASK 0xffffff00
#else
#define MSB 3
#define SOME_MASK 0x00ffffff
#endif

#define CHKPIX(ofs) (l1[(ofs)*4+MSB]==255 ? (( ((uint32_t*)l1)[0] = ((uint32_t*)l1)[ofs]&SOME_MASK), trans=true ) : false)

bool FTexture::SmoothEdges(unsigned char * buffer, int w, int h)
{
	int x, y;
	bool trans = buffer[MSB] == 0; // If I set this to false here the code won't detect textures 
								   // that only contain transparent pixels.
	bool semitrans = false;
	unsigned char * l1;

	if (h <= 1 || w <= 1) return false;  // makes (a) no sense and (b) doesn't work with this code!

	l1 = buffer;


	if (l1[MSB] == 0 && !CHKPIX(1)) CHKPIX(w);
	else if (l1[MSB]<255) semitrans = true;
	l1 += 4;
	for (x = 1; x<w - 1; x++, l1 += 4)
	{
		if (l1[MSB] == 0 && !CHKPIX(-1) && !CHKPIX(1)) CHKPIX(w);
		else if (l1[MSB]<255) semitrans = true;
	}
	if (l1[MSB] == 0 && !CHKPIX(-1)) CHKPIX(w);
	else if (l1[MSB]<255) semitrans = true;
	l1 += 4;

	for (y = 1; y<h - 1; y++)
	{
		if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(1)) CHKPIX(w);
		else if (l1[MSB]<255) semitrans = true;
		l1 += 4;
		for (x = 1; x<w - 1; x++, l1 += 4)
		{
			if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1) && !CHKPIX(1) && !CHKPIX(-w - 1) && !CHKPIX(-w + 1) && !CHKPIX(w - 1) && !CHKPIX(w + 1)) CHKPIX(w);
			else if (l1[MSB]<255) semitrans = true;
		}
		if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(w);
		else if (l1[MSB]<255) semitrans = true;
		l1 += 4;
	}

	if (l1[MSB] == 0 && !CHKPIX(-w)) CHKPIX(1);
	else if (l1[MSB]<255) semitrans = true;
	l1 += 4;
	for (x = 1; x<w - 1; x++, l1 += 4)
	{
		if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(1);
		else if (l1[MSB]<255) semitrans = true;
	}
	if (l1[MSB] == 0 && !CHKPIX(-w)) CHKPIX(-1);
	else if (l1[MSB]<255) semitrans = true;

	return trans || semitrans;
}

//===========================================================================
// 
// Post-process the texture data after the buffer has been created
//
//===========================================================================

bool FTexture::ProcessData(unsigned char * buffer, int w, int h, bool ispatch)
{
	if (bMasked)
	{
		bMasked = SmoothEdges(buffer, w, h);
		if (bMasked && !ispatch) FindHoles(buffer, w, h);
	}
	return true;
}

//===========================================================================
// 
//	Initializes the buffer for the texture data
//
//===========================================================================

unsigned char * FTexture::CreateTexBuffer(int translation, int & w, int & h, int flags)
{
	unsigned char * buffer = nullptr;
	int W, H;
	int isTransparent = -1;


	if (flags & CTF_CheckHires)
	{
		buffer = LoadHiresTexture(&w, &h);
		if (buffer != nullptr)
			return buffer;
	}

	int exx = !!(flags & CTF_Expand);

	W = w = GetWidth() + 2 * exx;
	H = h = GetHeight() + 2 * exx;


	buffer = new unsigned char[W*(H + 1) * 4];
	memset(buffer, 0, W * (H + 1) * 4);

	FBitmap bmp(buffer, W * 4, W, H);

	if (translation <= 0)
	{
		int trans = CopyTrueColorPixels(&bmp, exx, exx, 0, nullptr);
		CheckTrans(buffer, W*H, trans);
		isTransparent = bTranslucent;
	}
	else
	{
		// When using translations everything must be mapped to the base palette.
		// so use CopyTrueColorTranslated
		CopyTrueColorTranslated(&bmp, exx, exx, 0, FUniquePalette::GetPalette(translation));
		isTransparent = 0;
		// This is not conclusive for setting the texture's transparency info.
	}

	if (flags & CTF_ProcessData) 
	{
		buffer = CreateUpsampledTextureBuffer(buffer, W, H, w, h, !!isTransparent);
		ProcessData(buffer, w, h, false);
	}

	return buffer;
}

//===========================================================================
// 
// Dummy texture for the 0-entry.
//
//===========================================================================

bool FTexture::GetTranslucency()
{
	if (bTranslucent == -1)
	{
		if (!bHasCanvas)
		{
			int w, h;
			unsigned char *buffer = CreateTexBuffer(0, w, h);
			delete[] buffer;
		}
		else
		{
			bTranslucent = 0;
		}
	}
	return !!bTranslucent;
}

//===========================================================================
// 
// Sprite adjust has changed.
// This needs to alter the material's sprite rect.
//
//===========================================================================

void FTexture::SetSpriteAdjust()
{
	for (auto mat : Material)
	{
		if (mat != nullptr) mat->SetSpriteRect();
	}
}

//===========================================================================
// 
// empty stubs to be overloaded by child classes.
//
//===========================================================================

const uint8_t *FTexture::GetColumn(FRenderStyle style, unsigned int column, const Span **spans_out)
{
	return nullptr;
}

const uint8_t *FTexture::GetPixels(FRenderStyle style)
{
	return nullptr;
}

//===========================================================================
// 
// Dummy texture for the 0-entry.
//
//===========================================================================

FDummyTexture::FDummyTexture ()
{
	Width = 64;
	Height = 64;
	HeightBits = 6;
	WidthBits = 6;
	WidthMask = 63;
	UseType = ETextureType::Null;
}

void FDummyTexture::SetSize (int width, int height)
{
	Width = width;
	Height = height;
	CalcBitSize ();
}

//==========================================================================
//
// Debug stuff
//
//==========================================================================

#ifdef _DEBUG
// Prints the spans generated for a texture. Only needed for debugging.
CCMD (printspans)
{
	if (argv.argc() != 2)
		return;

	FTextureID picnum = TexMan.CheckForTexture (argv[1], ETextureType::Any);
	if (!picnum.Exists())
	{
		Printf ("Unknown texture %s\n", argv[1]);
		return;
	}
	FTexture *tex = TexMan[picnum];
	for (int x = 0; x < tex->GetWidth(); ++x)
	{
		const FTexture::Span *spans;
		Printf ("%4d:", x);
		tex->GetColumn(DefaultRenderStyle(), x, &spans);
		while (spans->Length != 0)
		{
			Printf (" (%4d,%4d)", spans->TopOffset, spans->TopOffset+spans->Length-1);
			spans++;
		}
		Printf ("\n");
	}
}

#endif


