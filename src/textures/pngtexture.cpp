/*
** pngtexture.cpp
** Texture class for PNG images
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
#include "m_png.h"
#include "bitmap.h"
#include "v_palette.h"
#include "textures/textures.h"

//==========================================================================
//
// A PNG texture
//
//==========================================================================

class FPNGTexture : public FTexture
{
public:
	FPNGTexture (FileReader &lump, int lumpnum, const FString &filename, int width, int height, BYTE bitdepth, BYTE colortype, BYTE interlace);
	~FPNGTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	FTextureFormat GetFormat ();
	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf = NULL);
	bool UseBasePalette();

protected:

	FString SourceFile;
	BYTE *Pixels;
	Span **Spans;

	BYTE BitDepth;
	BYTE ColorType;
	BYTE Interlace;
	bool HaveTrans;
	WORD NonPaletteTrans[3];

	BYTE *PaletteMap;
	int PaletteSize;
	DWORD StartOfIDAT;

	void MakeTexture ();

	friend class FTexture;
};


//==========================================================================
//
//
//
//==========================================================================

FTexture *PNGTexture_TryCreate(FileReader & data, int lumpnum)
{
	union
	{
		DWORD dw;
		WORD w[2];
		BYTE b[4];
	} first4bytes;

	DWORD width, height;
	BYTE bitdepth, colortype, compression, filter, interlace;

	// This is most likely a PNG, but make sure. (Note that if the
	// first 4 bytes match, but later bytes don't, we assume it's
	// a corrupt PNG.)

	data.Seek(0, SEEK_SET);
	if (data.Read (first4bytes.b, 4) != 4) return NULL;
	if (first4bytes.dw != MAKE_ID(137,'P','N','G'))	return NULL;
	if (data.Read (first4bytes.b, 4) != 4) return NULL;
	if (first4bytes.dw != MAKE_ID(13,10,26,10))		return NULL;
	if (data.Read (first4bytes.b, 4) != 4) return NULL;
	if (first4bytes.dw != MAKE_ID(0,0,0,13))		return NULL;
	if (data.Read (first4bytes.b, 4) != 4) return NULL;
	if (first4bytes.dw != MAKE_ID('I','H','D','R'))	return NULL;

	// The PNG looks valid so far. Check the IHDR to make sure it's a
	// type of PNG we support.
	data.Read(&width, 4);
	data.Read(&height, 4);
	data >> bitdepth >> colortype >> compression >> filter >> interlace;

	if (compression != 0 || filter != 0 || interlace > 1)
	{
		return NULL;
	}
	if (!((1 << colortype) & 0x5D))
	{
		return NULL;
	}
	if (!((1 << bitdepth) & 0x116))
	{
		return NULL;
	}

	// Just for completeness, make sure the PNG has something more than an IHDR.
	data.Seek (4, SEEK_CUR);
	data.Read (first4bytes.b, 4);
	if (first4bytes.dw == 0)
	{
		data.Read (first4bytes.b, 4);
		if (first4bytes.dw == MAKE_ID('I','E','N','D'))
		{
			return NULL;
		}
	}

	return new FPNGTexture (data, lumpnum, FString(), BigLong((int)width), BigLong((int)height),
		bitdepth, colortype, interlace);
}

//==========================================================================
//
//
//
//==========================================================================

FTexture *PNGTexture_CreateFromFile(PNGHandle *png, const FString &filename)
{
	DWORD width, height;
	BYTE bitdepth, colortype, compression, filter, interlace;

	if (M_FindPNGChunk(png, MAKE_ID('I','H','D','R')) == 0)
	{
		return NULL;
	}

	// Check the IHDR to make sure it's a type of PNG we support.
	png->File->Read(&width, 4);
	png->File->Read(&height, 4);
	(*png->File) >> bitdepth >> colortype >> compression >> filter >> interlace;

	if (compression != 0 || filter != 0 || interlace > 1)
	{
		return NULL;
	}
	if (!((1 << colortype) & 0x5D))
	{
		return NULL;
	}
	if (!((1 << bitdepth) & 0x116))
	{
		return NULL;
	}

	return new FPNGTexture (*png->File, -1, filename, BigLong((int)width), BigLong((int)height),
		bitdepth, colortype, interlace);
}

//==========================================================================
//
//
//
//==========================================================================

FPNGTexture::FPNGTexture (FileReader &lump, int lumpnum, const FString &filename, int width, int height,
						  BYTE depth, BYTE colortype, BYTE interlace)
: FTexture(NULL, lumpnum), SourceFile(filename), Pixels(0), Spans(0),
  BitDepth(depth), ColorType(colortype), Interlace(interlace), HaveTrans(false),
  PaletteMap(0), PaletteSize(0), StartOfIDAT(0)
{
	union
	{
		DWORD palette[256];
		BYTE pngpal[256][3];
	} p;
	BYTE trans[256];
	DWORD len, id;
	int i;

	UseType = TEX_MiscPatch;
	LeftOffset = 0;
	TopOffset = 0;
	bMasked = false;

	Width = width;
	Height = height;
	CalcBitSize ();

	memset(trans, 255, 256);

	// Parse pre-IDAT chunks. I skip the CRCs. Is that bad?
	lump.Seek(33, SEEK_SET);

	lump.Read(&len, 4);
	lump.Read(&id, 4);
	while (id != MAKE_ID('I','D','A','T') && id != MAKE_ID('I','E','N','D'))
	{
		len = BigLong((unsigned int)len);
		switch (id)
		{
		default:
			lump.Seek (len, SEEK_CUR);
			break;

		case MAKE_ID('g','r','A','b'):
			// This is like GRAB found in an ILBM, except coordinates use 4 bytes
			{
				DWORD hotx, hoty;
				int ihotx, ihoty;
				
				lump.Read(&hotx, 4);
				lump.Read(&hoty, 4);
				ihotx = BigLong((int)hotx);
				ihoty = BigLong((int)hoty);
				if (ihotx < -32768 || ihotx > 32767)
				{
					Printf ("X-Offset for PNG texture %s is bad: %d (0x%08x)\n", Wads.GetLumpFullName (lumpnum), ihotx, ihotx);
					ihotx = 0;
				}
				if (ihoty < -32768 || ihoty > 32767)
				{
					Printf ("Y-Offset for PNG texture %s is bad: %d (0x%08x)\n", Wads.GetLumpFullName (lumpnum), ihoty, ihoty);
					ihoty = 0;
				}
				LeftOffset = ihotx;
				TopOffset = ihoty;
			}
			break;

		case MAKE_ID('P','L','T','E'):
			PaletteSize = MIN<int> (len / 3, 256);
			lump.Read (p.pngpal, PaletteSize * 3);
			if (PaletteSize * 3 != (int)len)
			{
				lump.Seek (len - PaletteSize * 3, SEEK_CUR);
			}
			for (i = PaletteSize - 1; i >= 0; --i)
			{
				p.palette[i] = MAKERGB(p.pngpal[i][0], p.pngpal[i][1], p.pngpal[i][2]);
			}
			break;

		case MAKE_ID('t','R','N','S'):
			lump.Read (trans, len);
			HaveTrans = true;
			// Save for colortype 2
			NonPaletteTrans[0] = WORD(trans[0] * 256 + trans[1]);
			NonPaletteTrans[1] = WORD(trans[2] * 256 + trans[3]);
			NonPaletteTrans[2] = WORD(trans[4] * 256 + trans[5]);
			break;

		case MAKE_ID('a','l','P','h'):
			bAlphaTexture = true;
			bMasked = true;
			break;
		}
		lump.Seek(4, SEEK_CUR);		// Skip CRC
		lump.Read(&len, 4);
		id = MAKE_ID('I','E','N','D');
		lump.Read(&id, 4);
	}
	StartOfIDAT = lump.Tell() - 8;

	switch (colortype)
	{
	case 4:		// Grayscale + Alpha
		bMasked = true;
		// intentional fall-through

	case 0:		// Grayscale
		if (!bAlphaTexture)
		{
			if (colortype == 0 && HaveTrans && NonPaletteTrans[0] < 256)
			{
				bMasked = true;
				PaletteSize = 256;
				PaletteMap = new BYTE[256];
				memcpy (PaletteMap, GrayMap, 256);
				PaletteMap[NonPaletteTrans[0]] = 0;
			}
			else
			{
				PaletteMap = GrayMap;
			}
		}
		break;

	case 3:		// Paletted
		PaletteMap = new BYTE[PaletteSize];
		GPalette.MakeRemap (p.palette, PaletteMap, trans, PaletteSize);
		for (i = 0; i < PaletteSize; ++i)
		{
			if (trans[i] == 0)
			{
				bMasked = true;
				PaletteMap[i] = 0;
			}
		}
		break;

	case 6:		// RGB + Alpha
		bMasked = true;
		break;

	case 2:		// RGB
		bMasked = HaveTrans;
		break;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FPNGTexture::~FPNGTexture ()
{
	Unload ();
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
	if (PaletteMap != NULL && PaletteMap != GrayMap)
	{
		delete[] PaletteMap;
		PaletteMap = NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FPNGTexture::Unload ()
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

FTextureFormat FPNGTexture::GetFormat()
{
#if 0
	switch (ColorType)
	{
	case 3:		return TEX_Pal;
	case 0:		return TEX_Gray;
	default:	return TEX_RGB;
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

const BYTE *FPNGTexture::GetColumn (unsigned int column, const Span **spans_out)
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

const BYTE *FPNGTexture::GetPixels ()
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

void FPNGTexture::MakeTexture ()
{
	FileReader *lump;

	if (SourceLump >= 0)
	{
		lump = new FWadLump(Wads.OpenLumpNum(SourceLump));
	}
	else
	{
		lump = new FileReader(SourceFile.GetChars());
	}

	Pixels = new BYTE[Width*Height];
	if (StartOfIDAT == 0)
	{
		memset (Pixels, 0x99, Width*Height);
	}
	else
	{
		DWORD len, id;
		lump->Seek (StartOfIDAT, SEEK_SET);
		lump->Read(&len, 4);
		lump->Read(&id, 4);

		if (ColorType == 0 || ColorType == 3)	/* Grayscale and paletted */
		{
			M_ReadIDAT (lump, Pixels, Width, Height, Width, BitDepth, ColorType, Interlace, BigLong((unsigned int)len));

			if (Width == Height)
			{
				if (PaletteMap != NULL)
				{
					FlipSquareBlockRemap (Pixels, Width, Height, PaletteMap);
				}
				else
				{
					FlipSquareBlock (Pixels, Width, Height);
				}
			}
			else
			{
				BYTE *newpix = new BYTE[Width*Height];
				if (PaletteMap != NULL)
				{
					FlipNonSquareBlockRemap (newpix, Pixels, Width, Height, Width, PaletteMap);
				}
				else
				{
					FlipNonSquareBlock (newpix, Pixels, Width, Height, Width);
				}
				BYTE *oldpix = Pixels;
				Pixels = newpix;
				delete[] oldpix;
			}
		}
		else		/* RGB and/or Alpha present */
		{
			int bytesPerPixel = ColorType == 2 ? 3 : ColorType == 4 ? 2 : 4;
			BYTE *tempix = new BYTE[Width * Height * bytesPerPixel];
			BYTE *in, *out;
			int x, y, pitch, backstep;

			M_ReadIDAT (lump, tempix, Width, Height, Width*bytesPerPixel, BitDepth, ColorType, Interlace, BigLong((unsigned int)len));
			in = tempix;
			out = Pixels;

			// Convert from source format to paletted, column-major.
			// Formats with alpha maps are reduced to only 1 bit of alpha.
			switch (ColorType)
			{
			case 2:		// RGB
				pitch = Width * 3;
				backstep = Height * pitch - 3;
				for (x = Width; x > 0; --x)
				{
					for (y = Height; y > 0; --y)
					{
						if (!HaveTrans)
						{
							*out++ = RGB32k.RGB[in[0]>>3][in[1]>>3][in[2]>>3];
						}
						else
						{
							if (in[0] == NonPaletteTrans[0] &&
								in[1] == NonPaletteTrans[1] &&
								in[2] == NonPaletteTrans[2])
							{
								*out++ = 0;
							}
							else
							{
								*out++ = RGB32k.RGB[in[0]>>3][in[1]>>3][in[2]>>3];
							}
						}
						in += pitch;
					}
					in -= backstep;
				}
				break;

			case 4:		// Grayscale + Alpha
				pitch = Width * 2;
				backstep = Height * pitch - 2;
				if (PaletteMap != NULL)
				{
					for (x = Width; x > 0; --x)
					{
						for (y = Height; y > 0; --y)
						{
							*out++ = in[1] < 128 ? 0 : PaletteMap[in[0]];
							in += pitch;
						}
						in -= backstep;
					}
				}
				else
				{
					for (x = Width; x > 0; --x)
					{
						for (y = Height; y > 0; --y)
						{
							*out++ = in[1] < 128 ? 0 : in[0];
							in += pitch;
						}
						in -= backstep;
					}
				}
				break;

			case 6:		// RGB + Alpha
				pitch = Width * 4;
				backstep = Height * pitch - 4;
				for (x = Width; x > 0; --x)
				{
					for (y = Height; y > 0; --y)
					{
						*out++ = in[3] < 128 ? 0 : RGB32k.RGB[in[0]>>3][in[1]>>3][in[2]>>3];
						in += pitch;
					}
					in -= backstep;
				}
				break;
			}
			delete[] tempix;
		}
	}
	delete lump;
}

//===========================================================================
//
// FPNGTexture::CopyTrueColorPixels
//
//===========================================================================

int FPNGTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	// Parse pre-IDAT chunks. I skip the CRCs. Is that bad?
	PalEntry pe[256];
	DWORD len, id;
	FileReader *lump;
	static char bpp[] = {1, 0, 3, 1, 2, 0, 4};
	int pixwidth = Width * bpp[ColorType];
	int transpal = false;

	if (SourceLump >= 0)
	{
		lump = new FWadLump(Wads.OpenLumpNum(SourceLump));
	}
	else
	{
		lump = new FileReader(SourceFile.GetChars());
	}

	lump->Seek(33, SEEK_SET);
	for(int i = 0; i < 256; i++)	// default to a gray map
		pe[i] = PalEntry(255,i,i,i);

	lump->Read(&len, 4);
	lump->Read(&id, 4);
	while (id != MAKE_ID('I','D','A','T') && id != MAKE_ID('I','E','N','D'))
	{
		len = BigLong((unsigned int)len);
		switch (id)
		{
		default:
			lump->Seek (len, SEEK_CUR);
			break;

		case MAKE_ID('P','L','T','E'):
			for(int i = 0; i < PaletteSize; i++)
			{
				(*lump) >> pe[i].r >> pe[i].g >> pe[i].b;
			}
			break;

		case MAKE_ID('t','R','N','S'):
			if (ColorType == 3)
			{
				for(DWORD i = 0; i < len; i++)
				{
					(*lump) >> pe[i].a;
					if (pe[i].a != 0 && pe[i].a != 255)
						transpal = true;
				}
			}
			else
			{
				lump->Seek(len, SEEK_CUR);
			}
			break;
		}
		lump->Seek(4, SEEK_CUR);		// Skip CRC
		lump->Read(&len, 4);
		id = MAKE_ID('I','E','N','D');
		lump->Read(&id, 4);
	}

	if (ColorType == 0 && HaveTrans && NonPaletteTrans[0] < 256)
	{
		pe[NonPaletteTrans[0]].a = 0;
		transpal = true;
	}

	BYTE * Pixels = new BYTE[pixwidth * Height];

	lump->Seek (StartOfIDAT, SEEK_SET);
	lump->Read(&len, 4);
	lump->Read(&id, 4);
	M_ReadIDAT (lump, Pixels, Width, Height, pixwidth, BitDepth, ColorType, Interlace, BigLong((unsigned int)len));
	delete lump;

	switch (ColorType)
	{
	case 0:
	case 3:
		bmp->CopyPixelData(x, y, Pixels, Width, Height, 1, Width, rotate, pe, inf);
		break;

	case 2:
		if (!HaveTrans)
		{
			bmp->CopyPixelDataRGB(x, y, Pixels, Width, Height, 3, pixwidth, rotate, CF_RGB, inf);
		}
		else
		{
			bmp->CopyPixelDataRGB(x, y, Pixels, Width, Height, 3, pixwidth, rotate, CF_RGBT, inf,
				NonPaletteTrans[0], NonPaletteTrans[1], NonPaletteTrans[2]);
			transpal = true;
		}
		break;

	case 4:
		bmp->CopyPixelDataRGB(x, y, Pixels, Width, Height, 2, pixwidth, rotate, CF_IA, inf);
		transpal = -1;
		break;

	case 6:
		bmp->CopyPixelDataRGB(x, y, Pixels, Width, Height, 4, pixwidth, rotate, CF_RGBA, inf);
		transpal = -1;
		break;

	default:
		break;

	}
	delete[] Pixels;
	return transpal;
}


//===========================================================================
//
// This doesn't check if the palette is identical with the base palette
// I don't think it's worth the hassle because it's only of importance
// when compositing multipatch textures.
//
//===========================================================================

bool FPNGTexture::UseBasePalette() 
{ 
	return false; 
}
