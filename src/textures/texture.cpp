/*
** texture.cpp
** The base texture class
**
**---------------------------------------------------------------------------
** Copyright 2004-2007 Randy Heit
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
#include "textures/textures.h"
#include "v_palette.h"

typedef FTexture * (*CreateFunc)(FileReader & file, int lumpnum);

struct TexCreateInfo
{
	CreateFunc TryCreate;
	ETextureType usetype;
};

uint8_t FTexture::GrayMap[256];

void FTexture::InitGrayMap()
{
	for (int i = 0; i < 256; ++i)
	{
		GrayMap[i] = ColorMatcher.Pick (i, i, i);
	}
}

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


FTexture::FTexture (const char *name, int lumpnum)
: LeftOffset(0), TopOffset(0),
  WidthBits(0), HeightBits(0), Scale(1,1), SourceLump(lumpnum),
  UseType(ETextureType::Any), bNoDecals(false), bNoRemap0(false), bWorldPanning(false),
  bMasked(true), bAlphaTexture(false), bHasCanvas(false), bWarped(0), bComplex(false), bMultiPatch(false), bKeepAround(false),
	Rotations(0xFFFF), SkyOffset(0), Width(0), Height(0), WidthMask(0)
{
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
	if (link == this) Wads.SetLinkedTexture(SourceLump, NULL);
	KillNative();
}

void FTexture::Unload()
{
	PixelsBgra = std::vector<uint32_t>();
}

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

// Converts a texture between row-major and column-major format
// by flipping it about the X=Y axis.

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

FNativeTexture *FTexture::GetNative(FTextureFormat fmt, bool wrapping)
{
	if (Native[fmt] != NULL)
	{
		if (!Native[fmt]->CheckWrapping(wrapping))
		{ // Texture's wrapping mode is not compatible.
		  // Destroy it and get a new one.
			delete Native[fmt];
		}
		else
		{
			if (CheckModified(DefaultRenderStyle()))
			{
				Native[fmt]->Update();
			}
			return Native[fmt];
		}
	}
	Native[fmt] = screen->CreateTexture(this, fmt, wrapping);
	return Native[fmt];
}

void FTexture::KillNative()
{
	for (auto &nat : Native)
	{
		if (nat != nullptr)
		{
			delete nat;
			nat = nullptr;
		}
	}
}

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

bool FTexture::UseBasePalette() 
{ 
	return true; 
}

FTexture *FTexture::GetRedirect(bool wantwarped)
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
	PalEntry col;

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

// These only get called from the texture precacher which discards the result.
const uint8_t *FDummyTexture::GetColumn(FRenderStyle style, unsigned int column, const Span **spans_out)
{
	return nullptr;
}

const uint8_t *FDummyTexture::GetPixels(FRenderStyle style)
{
	return nullptr;
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


