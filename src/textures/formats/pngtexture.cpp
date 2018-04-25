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

//==========================================================================
//
// A PNG texture
//
//==========================================================================

class FPNGTexture : public FWorldTexture
{
public:
	FPNGTexture (FileReader &lump, int lumpnum, const FString &filename, int width, int height, uint8_t bitdepth, uint8_t colortype, uint8_t interlace);
	~FPNGTexture();

	FTextureFormat GetFormat () override;
	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf = NULL) override;
	bool UseBasePalette() override;
	uint8_t *MakeTexture(FRenderStyle style) override;

protected:
	void ReadAlphaRemap(FileReader *lump, uint8_t *alpharemap);

	FString SourceFile;
	FileReader fr;

	uint8_t BitDepth;
	uint8_t ColorType;
	uint8_t Interlace;
	bool HaveTrans;
	uint16_t NonPaletteTrans[3];

	uint8_t *PaletteMap = nullptr;
	int PaletteSize = 0;
	uint32_t StartOfIDAT = 0;
	uint32_t StartOfPalette = 0;
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
		uint32_t dw;
		uint16_t w[2];
		uint8_t b[4];
	} first4bytes;


	// This is most likely a PNG, but make sure. (Note that if the
	// first 4 bytes match, but later bytes don't, we assume it's
	// a corrupt PNG.)

	data.Seek(0, FileReader::SeekSet);
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
	int width = data.ReadInt32BE();
	int height = data.ReadInt32BE();
	uint8_t bitdepth = data.ReadUInt8();
	uint8_t colortype = data.ReadUInt8();
	uint8_t compression = data.ReadUInt8();
	uint8_t filter = data.ReadUInt8();
	uint8_t interlace = data.ReadUInt8();

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
	data.Seek (4, FileReader::SeekSet);
	data.Read (first4bytes.b, 4);
	if (first4bytes.dw == 0)
	{
		data.Read (first4bytes.b, 4);
		if (first4bytes.dw == MAKE_ID('I','E','N','D'))
		{
			return NULL;
		}
	}

	return new FPNGTexture (data, lumpnum, FString(), width, height, bitdepth, colortype, interlace);
}

//==========================================================================
//
//
//
//==========================================================================

FTexture *PNGTexture_CreateFromFile(PNGHandle *png, const FString &filename)
{

	if (M_FindPNGChunk(png, MAKE_ID('I','H','D','R')) == 0)
	{
		return NULL;
	}

	// Check the IHDR to make sure it's a type of PNG we support.
	auto &data = png->File;
	int width = data.ReadInt32BE();
	int height = data.ReadInt32BE();
	uint8_t bitdepth = data.ReadUInt8();
	uint8_t colortype = data.ReadUInt8();
	uint8_t compression = data.ReadUInt8();
	uint8_t filter = data.ReadUInt8();
	uint8_t interlace = data.ReadUInt8();

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

	return new FPNGTexture (png->File, -1, filename, width, height,	bitdepth, colortype, interlace);
}

//==========================================================================
//
//
//
//==========================================================================

FPNGTexture::FPNGTexture (FileReader &lump, int lumpnum, const FString &filename, int width, int height,
						  uint8_t depth, uint8_t colortype, uint8_t interlace)
: FWorldTexture(NULL, lumpnum), SourceFile(filename),
  BitDepth(depth), ColorType(colortype), Interlace(interlace), HaveTrans(false)
{
	union
	{
		uint32_t palette[256];
		uint8_t pngpal[256][3];
	} p;
	uint8_t trans[256];
	uint32_t len, id;
	int i;

	UseType = ETextureType::MiscPatch;
	bMasked = false;

	Width = width;
	Height = height;
	CalcBitSize ();

	memset(trans, 255, 256);

	// Parse pre-IDAT chunks. I skip the CRCs. Is that bad?
	lump.Seek(33, FileReader::SeekSet);

	lump.Read(&len, 4);
	lump.Read(&id, 4);
	while (id != MAKE_ID('I','D','A','T') && id != MAKE_ID('I','E','N','D'))
	{
		len = BigLong((unsigned int)len);
		switch (id)
		{
		default:
			lump.Seek (len, FileReader::SeekCur);
			break;

		case MAKE_ID('g','r','A','b'):
			// This is like GRAB found in an ILBM, except coordinates use 4 bytes
			{
				uint32_t hotx, hoty;
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
				_LeftOffset[1] = _LeftOffset[0] = ihotx;
				_TopOffset[1] = _TopOffset[0] = ihoty;
			}
			break;

		case MAKE_ID('P','L','T','E'):
			PaletteSize = MIN<int> (len / 3, 256);
			StartOfPalette = (uint32_t)lump.Tell();
			lump.Read (p.pngpal, PaletteSize * 3);
			if (PaletteSize * 3 != (int)len)
			{
				lump.Seek (len - PaletteSize * 3, FileReader::SeekCur);
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
			NonPaletteTrans[0] = uint16_t(trans[0] * 256 + trans[1]);
			NonPaletteTrans[1] = uint16_t(trans[2] * 256 + trans[3]);
			NonPaletteTrans[2] = uint16_t(trans[4] * 256 + trans[5]);
			break;
		}
		lump.Seek(4, FileReader::SeekCur);		// Skip CRC
		lump.Read(&len, 4);
		id = MAKE_ID('I','E','N','D');
		lump.Read(&id, 4);
	}
	StartOfIDAT = (uint32_t)lump.Tell() - 8;

	switch (colortype)
	{
	case 4:		// Grayscale + Alpha
		bMasked = true;
		// intentional fall-through

	case 0:		// Grayscale
		if (colortype == 0 && HaveTrans && NonPaletteTrans[0] < 256)
		{
			bMasked = true;
			PaletteSize = 256;
			PaletteMap = new uint8_t[256];
			memcpy (PaletteMap, GrayMap, 256);
			PaletteMap[NonPaletteTrans[0]] = 0;
		}
		else
		{
			PaletteMap = GrayMap;
		}
		break;

	case 3:		// Paletted
		PaletteMap = new uint8_t[PaletteSize];
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
	// If this is a savegame we must keep the reader.
	if (lumpnum == -1) fr = std::move(lump);
}

//==========================================================================
//
//
//
//==========================================================================

FPNGTexture::~FPNGTexture ()
{
	if (PaletteMap != nullptr && PaletteMap != FTexture::GrayMap)
	{
		delete[] PaletteMap;
		PaletteMap = nullptr;
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

void FPNGTexture::ReadAlphaRemap(FileReader *lump, uint8_t *alpharemap)
{
	auto p = lump->Tell();
	lump->Seek(StartOfPalette, FileReader::SeekSet);
	for (int i = 0; i < PaletteSize; i++)
	{
		uint8_t r = lump->ReadUInt8();
		uint8_t g = lump->ReadUInt8();
		uint8_t b = lump->ReadUInt8();
		alpharemap[i] = PaletteMap[i] == 0 ? 0 : Luminance(r, g, b);
	}
	lump->Seek(p, FileReader::SeekSet);
}

//==========================================================================
//
//
//
//==========================================================================

uint8_t *FPNGTexture::MakeTexture (FRenderStyle style)
{
	FileReader *lump;
	FileReader lfr;
	bool alphatex = !!(style.Flags & STYLEF_RedIsAlpha);

	if (SourceLump >= 0)
	{
		lfr = Wads.OpenLumpReader(SourceLump);
		lump = &lfr;
	}
	else
	{
		lump = &fr;
	}

	auto Pixels = new uint8_t[Width*Height];
	if (StartOfIDAT == 0)
	{
		memset (Pixels, 0x99, Width*Height);
	}
	else
	{
		uint32_t len, id;
		lump->Seek (StartOfIDAT, FileReader::SeekSet);
		lump->Read(&len, 4);
		lump->Read(&id, 4);

		if (ColorType == 0 || ColorType == 3)	/* Grayscale and paletted */
		{
			M_ReadIDAT (*lump, Pixels, Width, Height, Width, BitDepth, ColorType, Interlace, BigLong((unsigned int)len));

			if (Width == Height)
			{
				if (!alphatex)
				{
					FTexture::FlipSquareBlockRemap (Pixels, Width, Height, PaletteMap);
				}
				else if (ColorType == 0)
				{
					FTexture::FlipSquareBlock (Pixels, Width, Height);
				}
				else
				{
					uint8_t alpharemap[256];
					ReadAlphaRemap(lump, alpharemap);
					FTexture::FlipSquareBlockRemap(Pixels, Width, Height, alpharemap);
				}
			}
			else
			{
				uint8_t *newpix = new uint8_t[Width*Height];
				if (!alphatex)
				{
					FTexture::FlipNonSquareBlockRemap (newpix, Pixels, Width, Height, Width, PaletteMap);
				}
				else if (ColorType == 0)
				{
					FTexture::FlipNonSquareBlock (newpix, Pixels, Width, Height, Width);
				}
				else
				{
					uint8_t alpharemap[256];
					ReadAlphaRemap(lump, alpharemap);
					FTexture::FlipNonSquareBlockRemap(newpix, Pixels, Width, Height, Width, alpharemap);
				}
				uint8_t *oldpix = Pixels;
				Pixels = newpix;
				delete[] oldpix;
			}
		}
		else		/* RGB and/or Alpha present */
		{
			int bytesPerPixel = ColorType == 2 ? 3 : ColorType == 4 ? 2 : 4;
			uint8_t *tempix = new uint8_t[Width * Height * bytesPerPixel];
			uint8_t *in, *out;
			int x, y, pitch, backstep;

			M_ReadIDAT (*lump, tempix, Width, Height, Width*bytesPerPixel, BitDepth, ColorType, Interlace, BigLong((unsigned int)len));
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
						if (HaveTrans && in[0] == NonPaletteTrans[0] && in[1] == NonPaletteTrans[1] && in[2] == NonPaletteTrans[2])
						{
							*out++ = 0;
						}
						else
						{
							*out++ = RGBToPalette(alphatex, in[0], in[1], in[2]);
						}
						in += pitch;
					}
					in -= backstep;
				}
				break;

			case 4:		// Grayscale + Alpha
				pitch = Width * 2;
				backstep = Height * pitch - 2;
				for (x = Width; x > 0; --x)
				{
					for (y = Height; y > 0; --y)
					{
						*out++ = alphatex? ((in[0] * in[1]) / 255) : in[1] < 128 ? 0 : PaletteMap[in[0]];
						in += pitch;
					}
					in -= backstep;
				}
				break;

			case 6:		// RGB + Alpha
				pitch = Width * 4;
				backstep = Height * pitch - 4;
				for (x = Width; x > 0; --x)
				{
					for (y = Height; y > 0; --y)
					{
						*out++ = RGBToPalette(alphatex, in[0], in[1], in[2], in[3]);
						in += pitch;
					}
					in -= backstep;
				}
				break;
			}
			delete[] tempix;
		}
	}
	return Pixels;
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
	uint32_t len, id;
	static char bpp[] = {1, 0, 3, 1, 2, 0, 4};
	int pixwidth = Width * bpp[ColorType];
	int transpal = false;

	FileReader *lump;
	FileReader lfr;

	if (SourceLump >= 0)
	{
		lfr = Wads.OpenLumpReader(SourceLump);
		lump = &lfr;
	}
	else
	{
		lump = &fr;
	}

	lump->Seek(33, FileReader::SeekSet);
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
			lump->Seek (len, FileReader::SeekCur);
			break;

		case MAKE_ID('P','L','T','E'):
			for(int i = 0; i < PaletteSize; i++)
			{
				pe[i].r = lump->ReadUInt8();
				pe[i].g = lump->ReadUInt8();
				pe[i].b = lump->ReadUInt8();
			}
			break;

		case MAKE_ID('t','R','N','S'):
			if (ColorType == 3)
			{
				for(uint32_t i = 0; i < len; i++)
				{
					pe[i].a = lump->ReadUInt8();
					if (pe[i].a != 0 && pe[i].a != 255)
						transpal = true;
				}
			}
			else
			{
				lump->Seek(len, FileReader::SeekCur);
			}
			break;
		}
		lump->Seek(4, FileReader::SeekCur);	// Skip CRC
		lump->Read(&len, 4);
		id = MAKE_ID('I','E','N','D');
		lump->Read(&id, 4);
	}

	if (ColorType == 0 && HaveTrans && NonPaletteTrans[0] < 256)
	{
		pe[NonPaletteTrans[0]].a = 0;
		transpal = true;
	}

	uint8_t * Pixels = new uint8_t[pixwidth * Height];

	lump->Seek (StartOfIDAT, FileReader::SeekSet);
	lump->Read(&len, 4);
	lump->Read(&id, 4);
	M_ReadIDAT (*lump, Pixels, Width, Height, pixwidth, BitDepth, ColorType, Interlace, BigLong((unsigned int)len));

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
