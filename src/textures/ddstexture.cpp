/*
** pngtexture.cpp
** Texture class for DDS images
**
**---------------------------------------------------------------------------
** Copyright 2006-2007 Randy Heit
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
** DDS is short for "DirectDraw Surface" and is essentially that. It's
** interesting to us because it is a standard file format for DXTC/S3TC
** encoded images. Look up "DDS File Reference" in the DirectX SDK or
** the online MSDN documentation to the specs for this file format. Look up
** "Compressed Texture Resources" for information about DXTC encoding.
**
** Perhaps the most important part of DXTC to realize is that every 4x4
** pixel block can only have four different colors, and only two of those
** are discrete. So depending on the texture, there may be very noticable
** quality degradation, or it may look virtually indistinguishable from
** the uncompressed texture.
**
** Note: Although this class supports reading RGB textures from a DDS,
** DO NOT use DDS images with plain RGB data. PNG does everything useful
** better. Since DDS lets the R, G, B, and A components lie anywhere in
** the pixel data, it is fairly inefficient to process.
*/

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "templates.h"
#include "bitmap.h"
#include "v_video.h"
#include "textures/textures.h"

// Since we want this to compile under Linux too, we need to define this
// stuff ourselves instead of including a DirectX header.

#define ID_DDS						MAKE_ID('D','D','S',' ')
#define ID_DXT1						MAKE_ID('D','X','T','1')
#define ID_DXT2						MAKE_ID('D','X','T','2')
#define ID_DXT3						MAKE_ID('D','X','T','3')
#define ID_DXT4						MAKE_ID('D','X','T','4')
#define ID_DXT5						MAKE_ID('D','X','T','5')

// Bits in dwFlags
#define DDSD_CAPS					0x00000001
#define DDSD_HEIGHT					0x00000002
#define DDSD_WIDTH					0x00000004
#define DDSD_PITCH					0x00000008
#define DDSD_PIXELFORMAT			0x00001000
#define DDSD_MIPMAPCOUNT			0x00020000
#define DDSD_LINEARSIZE				0x00080000
#define DDSD_DEPTH					0x00800000

// Bits in ddpfPixelFormat
#define DDPF_ALPHAPIXELS			0x00000001
#define DDPF_FOURCC					0x00000004
#define DDPF_RGB					0x00000040

// Bits in DDSCAPS2.dwCaps1
#define DDSCAPS_COMPLEX				0x00000008
#define DDSCAPS_TEXTURE				0x00001000
#define DDSCAPS_MIPMAP				0x00400000

// Bits in DDSCAPS2.dwCaps2
#define DDSCAPS2_CUBEMAP			0x00000200
#define DDSCAPS2_CUBEMAP_POSITIVEX	0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX	0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY	0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY	0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ	0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIZEZ	0x00008000
#define DDSCAPS2_VOLUME				0x00200000

//==========================================================================
//
//
//
//==========================================================================

struct DDPIXELFORMAT
{
	DWORD			Size;		// Must be 32
	DWORD			Flags;
	DWORD			FourCC;
	DWORD			RGBBitCount;
	DWORD			RBitMask, GBitMask, BBitMask;
	DWORD			RGBAlphaBitMask;
};

struct DDCAPS2
{
	DWORD			Caps1, Caps2;
	DWORD			Reserved[2];
};

struct DDSURFACEDESC2
{
	DWORD			Size;		// Must be 124. DevIL claims some writers set it to 'DDS ' instead.
	DWORD			Flags;
	DWORD			Height;
	DWORD			Width;
	union
	{
		SDWORD		Pitch;
		DWORD		LinearSize;
	};
	DWORD			Depth;
	DWORD			MipMapCount;
	DWORD			Reserved1[11];
	DDPIXELFORMAT	PixelFormat;
	DDCAPS2			Caps;
	DWORD			Reserved2;
};

struct DDSFileHeader
{
	DWORD			Magic;
	DDSURFACEDESC2	Desc;
};


//==========================================================================
//
// A DDS image, with DXTx compression
//
//==========================================================================

class FDDSTexture : public FTexture
{
public:
	FDDSTexture (FileReader &lump, int lumpnum, void *surfdesc);
	~FDDSTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	FTextureFormat GetFormat ();

protected:

	BYTE *Pixels;
	Span **Spans;

	DWORD Format;

	DWORD RMask, GMask, BMask, AMask;
	BYTE RShiftL, GShiftL, BShiftL, AShiftL;
	BYTE RShiftR, GShiftR, BShiftR, AShiftR;

	SDWORD Pitch;
	DWORD LinearSize;

	static void CalcBitShift (DWORD mask, BYTE *lshift, BYTE *rshift);

	void MakeTexture ();
	void ReadRGB (FWadLump &lump, BYTE *tcbuf = NULL);
	void DecompressDXT1 (FWadLump &lump, BYTE *tcbuf = NULL);
	void DecompressDXT3 (FWadLump &lump, bool premultiplied, BYTE *tcbuf = NULL);
	void DecompressDXT5 (FWadLump &lump, bool premultiplied, BYTE *tcbuf = NULL);

	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf = NULL);
	bool UseBasePalette();

	friend class FTexture;
};


//==========================================================================
//
//
//
//==========================================================================

static bool CheckDDS (FileReader &file)
{
	DDSFileHeader Header;

	file.Seek (0, SEEK_SET);
	if (file.Read (&Header, sizeof(Header)) != sizeof(Header))
	{
		return false;
	}
	return Header.Magic == ID_DDS &&
		(LittleLong(Header.Desc.Size) == sizeof(DDSURFACEDESC2) || Header.Desc.Size == ID_DDS) &&
		LittleLong(Header.Desc.PixelFormat.Size) == sizeof(DDPIXELFORMAT) &&
		(LittleLong(Header.Desc.Flags) & (DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT)) == (DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT) &&
		Header.Desc.Width != 0 &&
		Header.Desc.Height != 0;
}

//==========================================================================
//
//
//
//==========================================================================

FTexture *DDSTexture_TryCreate (FileReader &data, int lumpnum)
{
	union
	{
		DDSURFACEDESC2	surfdesc;
		DWORD			byteswapping[sizeof(DDSURFACEDESC2) / 4];
	};

	if (!CheckDDS(data)) return NULL;

	data.Seek (4, SEEK_SET);
	data.Read (&surfdesc, sizeof(surfdesc));

#ifdef __BIG_ENDIAN__
	// Every single element of the header is a DWORD
	for (unsigned int i = 0; i < sizeof(DDSURFACEDESC2) / 4; ++i)
	{
		byteswapping[i] = LittleLong(byteswapping[i]);
	}
	// Undo the byte swap for the pixel format
	surfdesc.PixelFormat.FourCC = LittleLong(surfdesc.PixelFormat.FourCC);
#endif

	if (surfdesc.PixelFormat.Flags & DDPF_FOURCC)
	{
		// Check for supported FourCC
		if (surfdesc.PixelFormat.FourCC != ID_DXT1 &&
			surfdesc.PixelFormat.FourCC != ID_DXT2 &&
			surfdesc.PixelFormat.FourCC != ID_DXT3 &&
			surfdesc.PixelFormat.FourCC != ID_DXT4 &&
			surfdesc.PixelFormat.FourCC != ID_DXT5)
		{
			return NULL;
		}
		if (!(surfdesc.Flags & DDSD_LINEARSIZE))
		{
			return NULL;
		}
	}
	else if (surfdesc.PixelFormat.Flags & DDPF_RGB)
	{
		if ((surfdesc.PixelFormat.RGBBitCount >> 3) < 1 ||
			(surfdesc.PixelFormat.RGBBitCount >> 3) > 4)
		{
			return NULL;
		}
		if ((surfdesc.Flags & DDSD_PITCH) && (surfdesc.Pitch <= 0))
		{
			return NULL;
		}
	}
	else
	{
		return NULL;
	}
	return new FDDSTexture (data, lumpnum, &surfdesc);
}

//==========================================================================
//
//
//
//==========================================================================

FDDSTexture::FDDSTexture (FileReader &lump, int lumpnum, void *vsurfdesc)
: FTexture(NULL, lumpnum), Pixels(0), Spans(0)
{
	DDSURFACEDESC2 *surf = (DDSURFACEDESC2 *)vsurfdesc;

	UseType = TEX_MiscPatch;
	LeftOffset = 0;
	TopOffset = 0;
	bMasked = false;

	Width = WORD(surf->Width);
	Height = WORD(surf->Height);
	CalcBitSize ();

	if (surf->PixelFormat.Flags & DDPF_FOURCC)
	{
		Format = surf->PixelFormat.FourCC;
		Pitch = 0;
		LinearSize = surf->LinearSize;
	}
	else	// DDPF_RGB
	{
		Format = surf->PixelFormat.RGBBitCount >> 3;
		CalcBitShift (RMask = surf->PixelFormat.RBitMask, &RShiftL, &RShiftR);
		CalcBitShift (GMask = surf->PixelFormat.GBitMask, &GShiftL, &GShiftR);
		CalcBitShift (BMask = surf->PixelFormat.BBitMask, &BShiftL, &BShiftR);
		if (surf->PixelFormat.Flags & DDPF_ALPHAPIXELS)
		{
			CalcBitShift (AMask = surf->PixelFormat.RGBAlphaBitMask, &AShiftL, &AShiftR);
		}
		else
		{
			AMask = 0;
			AShiftL = AShiftR = 0;
		}
		if (surf->Flags & DDSD_PITCH)
		{
			Pitch = surf->Pitch;
		}
		else
		{
			Pitch = (Width * Format + 3) & ~3;
		}
		LinearSize = Pitch * Height;
	}
}

//==========================================================================
//
// Returns the number of bits the color must be shifted to produce
// an 8-bit value, as in:
//
// c   = (color & mask) << lshift;
// c  |= c >> rshift;
// c >>= 24;
//
// For any color of at least 4 bits, this ensures that the result
// of the calculation for c will be fully saturated, given a maximum
// value for the input bit mask.
//
//==========================================================================

void FDDSTexture::CalcBitShift (DWORD mask, BYTE *lshiftp, BYTE *rshiftp)
{
	BYTE shift;

	if (mask == 0)
	{
		*lshiftp = *rshiftp = 0;
		return;
	}

	shift = 0;
	while ((mask & 0x80000000) == 0)
	{
		mask <<= 1;
		shift++;
	}
	*lshiftp = shift;

	shift = 0;
	while (mask & 0x80000000)
	{
		mask <<= 1;
		shift++;
	}
	*rshiftp = shift;
}

//==========================================================================
//
//
//
//==========================================================================

FDDSTexture::~FDDSTexture ()
{
	Unload ();
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDDSTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FTextureFormat FDDSTexture::GetFormat()
{
#if 0
	switch (Format)
	{
	case ID_DXT1:	return TEX_DXT1;
	case ID_DXT2:	return TEX_DXT2;
	case ID_DXT3:	return TEX_DXT3;
	case ID_DXT4:	return TEX_DXT4;
	case ID_DXT5:	return TEX_DXT5;
	default:		return TEX_RGB;
	}
#else
	// For now, create a true color texture to preserve all colors.
	return TEX_RGB;
#endif
}

//==========================================================================
//
//
//
//==========================================================================

const BYTE *FDDSTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	if ((unsigned)column >= (unsigned)Width)
	{
		if (WidthMask + 1 == Width)
		{
			column &= WidthMask;
		}
		else
		{
			column %= Width;
		}
	}
	if (spans_out != NULL)
	{
		if (Spans == NULL)
		{
			Spans = CreateSpans (Pixels);
		}
		*spans_out = Spans[column];
	}
	return Pixels + column*Height;
}

//==========================================================================
//
//
//
//==========================================================================

const BYTE *FDDSTexture::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

//==========================================================================
//
//
//
//==========================================================================

void FDDSTexture::MakeTexture ()
{
	FWadLump lump = Wads.OpenLumpNum (SourceLump);

	Pixels = new BYTE[Width*Height];

	lump.Seek (sizeof(DDSURFACEDESC2) + 4, SEEK_SET);

	if (Format >= 1 && Format <= 4)		// RGB: Format is # of bytes per pixel
	{
		ReadRGB (lump);
	}
	else if (Format == ID_DXT1)
	{
		DecompressDXT1 (lump);
	}
	else if (Format == ID_DXT3 || Format == ID_DXT2)
	{
		DecompressDXT3 (lump, Format == ID_DXT2);
	}
	else if (Format == ID_DXT5 || Format == ID_DXT4)
	{
		DecompressDXT5 (lump, Format == ID_DXT4);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDDSTexture::ReadRGB (FWadLump &lump, BYTE *tcbuf)
{
	DWORD x, y;
	DWORD amask = AMask == 0 ? 0 : 0x80000000 >> AShiftL;
	BYTE *linebuff = new BYTE[Pitch];

	for (y = Height; y > 0; --y)
	{
		BYTE *buffp = linebuff;
		BYTE *pixelp = tcbuf? tcbuf + 4*y*Height : Pixels + y;
		lump.Read (linebuff, Pitch);
		for (x = Width; x > 0; --x)
		{
			DWORD c;
			if (Format == 4)
			{
				c = LittleLong(*(DWORD *)buffp); buffp += 4;
			}
			else if (Format == 2)
			{
				c = LittleShort(*(WORD *)buffp); buffp += 2;
			}
			else if (Format == 3)
			{
				c = buffp[0] | (buffp[1] << 8) | (buffp[2] << 16); buffp += 3;
			}
			else //  Format == 1
			{
				c = *buffp++;
			}
			if (!tcbuf)
			{
				if (amask == 0 || (c & amask))
				{
					DWORD r = (c & RMask) << RShiftL; r |= r >> RShiftR;
					DWORD g = (c & GMask) << GShiftL; g |= g >> GShiftR;
					DWORD b = (c & BMask) << BShiftL; b |= b >> BShiftR;
					*pixelp = RGB32k.RGB[r >> 27][g >> 27][b >> 27];
				}
				else
				{
					*pixelp = 0;
					bMasked = true;
				}
				pixelp += Height;
			}
			else
			{
				DWORD r = (c & RMask) << RShiftL; r |= r >> RShiftR;
				DWORD g = (c & GMask) << GShiftL; g |= g >> GShiftR;
				DWORD b = (c & BMask) << BShiftL; b |= b >> BShiftR;
				DWORD a = (c & AMask) << AShiftL; a |= a >> AShiftR;
				pixelp[0] = (BYTE)(r>>24);
				pixelp[1] = (BYTE)(g>>24);
				pixelp[2] = (BYTE)(b>>24);
				pixelp[3] = (BYTE)(a>>24);
				pixelp+=4;
			}
		}
	}
	delete[] linebuff;
}

//==========================================================================
//
//
//
//==========================================================================

void FDDSTexture::DecompressDXT1 (FWadLump &lump, BYTE *tcbuf)
{
	const long blocklinelen = ((Width + 3) >> 2) << 3;
	BYTE *blockbuff = new BYTE[blocklinelen];
	BYTE *block;
	PalEntry color[4];
	BYTE palcol[4];
	int ox, oy, x, y, i;

	color[0].a = 255;
	color[1].a = 255;
	color[2].a = 255;

	for (oy = 0; oy < Height; oy += 4)
	{
		lump.Read (blockbuff, blocklinelen);
		block = blockbuff;
		for (ox = 0; ox < Width; ox += 4)
		{
			WORD color16[2] = { LittleShort(((WORD *)block)[0]), LittleShort(((WORD *)block)[1]) };

			// Convert color from R5G6B5 to R8G8B8.
			for (i = 1; i >= 0; --i)
			{
				color[i].r = ((color16[i] & 0xF800) >> 8) | (color16[i] >> 13);
				color[i].g = ((color16[i] & 0x07E0) >> 3) | ((color16[i] & 0x0600) >> 9);
				color[i].b = ((color16[i] & 0x001F) << 3) | ((color16[i] & 0x001C) >> 2);
			}
			if (color16[0] > color16[1])
			{ // Four-color block: derive the other two colors.
				color[2].r = (color[0].r + color[0].r + color[1].r + 1) / 3;
				color[2].g = (color[0].g + color[0].g + color[1].g + 1) / 3;
				color[2].b = (color[0].b + color[0].b + color[1].b + 1) / 3;

				color[3].r = (color[0].r + color[1].r + color[1].r + 1) / 3;
				color[3].g = (color[0].g + color[1].g + color[1].g + 1) / 3;
				color[3].b = (color[0].b + color[1].b + color[1].b + 1) / 3;
				color[3].a = 255;
			}
			else
			{ // Three-color block: derive the other color.
				color[2].r = (color[0].r + color[1].r) / 2;
				color[2].g = (color[0].g + color[1].g) / 2;
				color[2].b = (color[0].b + color[1].b) / 2;

				color[3].a = color[3].b = color[3].g = color[3].r = 0;

				// If you have a three-color block, presumably that transparent
				// color is going to be used.
				bMasked = true;
			}
			// Pick colors from the palette for each of the four colors.
			/*if (!tcbuf)*/ for (i = 3; i >= 0; --i)
			{
				palcol[i] = color[i].a ? RGB32k.RGB[color[i].r >> 3][color[i].g >> 3][color[i].b >> 3] : 0;
			}
			// Now decode this 4x4 block to the pixel buffer.
			for (y = 0; y < 4; ++y)
			{
				if (oy + y >= Height)
				{
					break;
				}
				BYTE yslice = block[4 + y];
				for (x = 0; x < 4; ++x)
				{
					if (ox + x >= Width)
					{
						break;
					}
					int ci = (yslice >> (x + x)) & 3;
					if (!tcbuf) 
					{
						Pixels[oy + y + (ox + x) * Height] = palcol[ci];
					}
					else
					{
						BYTE * tcp = &tcbuf[(ox + x)*4 + (oy + y) * Width*4];
						tcp[0] = color[ci].r;
						tcp[1] = color[ci].g;
						tcp[2] = color[ci].b;
						tcp[3] = color[ci].a;
					}
				}
			}
			block += 8;
		}
	}
	delete[] blockbuff;
}

//==========================================================================
//
// DXT3: Decompression is identical to DXT1, except every 64-bit block is
// preceded by another 64-bit block with explicit alpha values.
//
//==========================================================================

void FDDSTexture::DecompressDXT3 (FWadLump &lump, bool premultiplied, BYTE *tcbuf)
{
	const long blocklinelen = ((Width + 3) >> 2) << 4;
	BYTE *blockbuff = new BYTE[blocklinelen];
	BYTE *block;
	PalEntry color[4];
	BYTE palcol[4];
	int ox, oy, x, y, i;

	for (oy = 0; oy < Height; oy += 4)
	{
		lump.Read (blockbuff, blocklinelen);
		block = blockbuff;
		for (ox = 0; ox < Width; ox += 4)
		{
			WORD color16[2] = { LittleShort(((WORD *)block)[4]), LittleShort(((WORD *)block)[5]) };

			// Convert color from R5G6B5 to R8G8B8.
			for (i = 1; i >= 0; --i)
			{
				color[i].r = ((color16[i] & 0xF800) >> 8) | (color16[i] >> 13);
				color[i].g = ((color16[i] & 0x07E0) >> 3) | ((color16[i] & 0x0600) >> 9);
				color[i].b = ((color16[i] & 0x001F) << 3) | ((color16[i] & 0x001C) >> 2);
			}
			// Derive the other two colors.
			color[2].r = (color[0].r + color[0].r + color[1].r + 1) / 3;
			color[2].g = (color[0].g + color[0].g + color[1].g + 1) / 3;
			color[2].b = (color[0].b + color[0].b + color[1].b + 1) / 3;

			color[3].r = (color[0].r + color[1].r + color[1].r + 1) / 3;
			color[3].g = (color[0].g + color[1].g + color[1].g + 1) / 3;
			color[3].b = (color[0].b + color[1].b + color[1].b + 1) / 3;

			// Pick colors from the palette for each of the four colors.
			if (!tcbuf) for (i = 3; i >= 0; --i)
			{
				palcol[i] = RGB32k.RGB[color[i].r >> 3][color[i].g >> 3][color[i].b >> 3];
			}
			// Now decode this 4x4 block to the pixel buffer.
			for (y = 0; y < 4; ++y)
			{
				if (oy + y >= Height)
				{
					break;
				}
				BYTE yslice = block[12 + y];
				WORD yalphaslice = LittleShort(((WORD *)block)[y]);
				for (x = 0; x < 4; ++x)
				{
					if (ox + x >= Width)
					{
						break;
					}
					if (!tcbuf)
					{
						Pixels[oy + y + (ox + x) * Height] = ((yalphaslice >> (x*4)) & 15) < 8 ?
							(bMasked = true, 0) : palcol[(yslice >> (x + x)) & 3];
					}
					else
					{
						BYTE * tcp = &tcbuf[(ox + x)*4 + (oy + y) * Width*4];
						int c = (yslice >> (x + x)) & 3;
						tcp[0] = color[c].r;
						tcp[1] = color[c].g;
						tcp[2] = color[c].b;
						tcp[3] = color[c].a;
					}
				}
			}
			block += 16;
		}
	}
	delete[] blockbuff;
}

//==========================================================================
//
// DXT5: Decompression is identical to DXT3, except every 64-bit alpha block
// contains interpolated alpha values, similar to the 64-bit color block.
//
//==========================================================================

void FDDSTexture::DecompressDXT5 (FWadLump &lump, bool premultiplied, BYTE *tcbuf)
{
	const long blocklinelen = ((Width + 3) >> 2) << 4;
	BYTE *blockbuff = new BYTE[blocklinelen];
	BYTE *block;
	PalEntry color[4];
	BYTE palcol[4];
	DWORD yalphaslice = 0;
	int ox, oy, x, y, i;

	for (oy = 0; oy < Height; oy += 4)
	{
		lump.Read (blockbuff, blocklinelen);
		block = blockbuff;
		for (ox = 0; ox < Width; ox += 4)
		{
			WORD color16[2] = { LittleShort(((WORD *)block)[4]), LittleShort(((WORD *)block)[5]) };
			BYTE alpha[8];

			// Calculate the eight alpha values.
			alpha[0] = block[0];
			alpha[1] = block[1];

			if (alpha[0] > alpha[1])
			{ // Eight-alpha block: derive the other six alphas.
				for (i = 0; i < 6; ++i)
				{
					alpha[i + 2] = ((6 - i) * alpha[0] + (i + 1) * alpha[1] + 3) / 7;
				}
			}
			else
			{ // Six-alpha block: derive the other four alphas.
				for (i = 0; i < 4; ++i)
				{
					alpha[i + 2] = ((4 - i) * alpha[0] + (i + 1) * alpha[1] + 2) / 5;
				}
				alpha[6] = 0;
				alpha[7] = 255;
			}

			// Convert color from R5G6B5 to R8G8B8.
			for (i = 1; i >= 0; --i)
			{
				color[i].r = ((color16[i] & 0xF800) >> 8) | (color16[i] >> 13);
				color[i].g = ((color16[i] & 0x07E0) >> 3) | ((color16[i] & 0x0600) >> 9);
				color[i].b = ((color16[i] & 0x001F) << 3) | ((color16[i] & 0x001C) >> 2);
			}
			// Derive the other two colors.
			color[2].r = (color[0].r + color[0].r + color[1].r + 1) / 3;
			color[2].g = (color[0].g + color[0].g + color[1].g + 1) / 3;
			color[2].b = (color[0].b + color[0].b + color[1].b + 1) / 3;

			color[3].r = (color[0].r + color[1].r + color[1].r + 1) / 3;
			color[3].g = (color[0].g + color[1].g + color[1].g + 1) / 3;
			color[3].b = (color[0].b + color[1].b + color[1].b + 1) / 3;

			// Pick colors from the palette for each of the four colors.
			if (!tcbuf) for (i = 3; i >= 0; --i)
			{
				palcol[i] = RGB32k.RGB[color[i].r >> 3][color[i].g >> 3][color[i].b >> 3];
			}
			// Now decode this 4x4 block to the pixel buffer.
			for (y = 0; y < 4; ++y)
			{
				if (oy + y >= Height)
				{
					break;
				}
				// Alpha values are stored in 3 bytes for 2 rows
				if ((y & 1) == 0)
				{
					yalphaslice = block[y*3] | (block[y*3+1] << 8) | (block[y*3+2] << 16);
				}
				else
				{
					yalphaslice >>= 12;
				}
				BYTE yslice = block[12 + y];
				for (x = 0; x < 4; ++x)
				{
					if (ox + x >= Width)
					{
						break;
					}
					if (!tcbuf)
					{
						Pixels[oy + y + (ox + x) * Height] = alpha[((yalphaslice >> (x*3)) & 7)] < 128 ?
							(bMasked = true, 0) : palcol[(yslice >> (x + x)) & 3];
					}
					else
					{
						BYTE * tcp = &tcbuf[(ox + x)*4 + (oy + y) * Width*4];
						int c = (yslice >> (x + x)) & 3;
						tcp[0] = color[c].r;
						tcp[1] = color[c].g;
						tcp[2] = color[c].b;
						tcp[3] = alpha[((yalphaslice >> (x*3)) & 7)];
					}
				}
			}
			block += 16;
		}
	}
	delete[] blockbuff;
}

//===========================================================================
//
// FDDSTexture::CopyTrueColorPixels
//
//===========================================================================

int FDDSTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	FWadLump lump = Wads.OpenLumpNum (SourceLump);

	BYTE *TexBuffer = new BYTE[4*Width*Height];

	lump.Seek (sizeof(DDSURFACEDESC2) + 4, SEEK_SET);

	if (Format >= 1 && Format <= 4)		// RGB: Format is # of bytes per pixel
	{
		ReadRGB (lump, TexBuffer);
	}
	else if (Format == ID_DXT1)
	{
		DecompressDXT1 (lump, TexBuffer);
	}
	else if (Format == ID_DXT3 || Format == ID_DXT2)
	{
		DecompressDXT3 (lump, Format == ID_DXT2, TexBuffer);
	}
	else if (Format == ID_DXT5 || Format == ID_DXT4)
	{
		DecompressDXT5 (lump, Format == ID_DXT4, TexBuffer);
	}

	// All formats decompress to RGBA.
	bmp->CopyPixelDataRGB(x, y, TexBuffer, Width, Height, 4, Width*4, rotate, CF_RGBA, inf);
	delete [] TexBuffer;
	return -1;
}	

//===========================================================================
//
//
//===========================================================================

bool FDDSTexture::UseBasePalette() 
{ 
	return false; 
}
