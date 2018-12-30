/*
** pcxtexture.cpp
** Texture class for PCX images
**
**---------------------------------------------------------------------------
** Copyright 2005 David HENRY
** Copyright 2006 Christoph Oelckers
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
#include "bitmap.h"
#include "v_video.h"
#include "imagehelpers.h"
#include "image.h"

//==========================================================================
//
// PCX file header
//
//==========================================================================

#pragma pack(1)

struct PCXHeader
{
  uint8_t manufacturer;
  uint8_t version;
  uint8_t encoding;
  uint8_t bitsPerPixel;

  uint16_t xmin, ymin;
  uint16_t xmax, ymax;
  uint16_t horzRes, vertRes;

  uint8_t palette[48];
  uint8_t reserved;
  uint8_t numColorPlanes;

  uint16_t bytesPerScanLine;
  uint16_t paletteType;
  uint16_t horzSize, vertSize;

  uint8_t padding[54];

} FORCE_PACKED;
#pragma pack()

//==========================================================================
//
// a PCX texture
//
//==========================================================================

class FPCXTexture : public FImageSource
{
public:
	FPCXTexture (int lumpnum, PCXHeader &);

	int CopyPixels(FBitmap *bmp, int conversion) override;

protected:
	void ReadPCX1bit (uint8_t *dst, FileReader & lump, PCXHeader *hdr);
	void ReadPCX4bits (uint8_t *dst, FileReader & lump, PCXHeader *hdr);
	void ReadPCX8bits (uint8_t *dst, FileReader & lump, PCXHeader *hdr);
	void ReadPCX24bits (uint8_t *dst, FileReader & lump, PCXHeader *hdr, int planes);

	TArray<uint8_t> CreatePalettedPixels(int conversion) override;
};


//==========================================================================
//
//
//
//==========================================================================

FImageSource * PCXImage_TryCreate(FileReader & file, int lumpnum)
{
	PCXHeader hdr;


	file.Seek(0, FileReader::SeekSet);
	if (file.Read(&hdr, sizeof(hdr)) != sizeof(hdr))
	{
		return NULL;
	}

	hdr.xmin = LittleShort(hdr.xmin);
	hdr.xmax = LittleShort(hdr.xmax);
	hdr.bytesPerScanLine = LittleShort(hdr.bytesPerScanLine);

	if (hdr.manufacturer != 10 || hdr.encoding != 1) return NULL;
	if (hdr.version != 0 && hdr.version != 2 && hdr.version != 3 && hdr.version != 4 && hdr.version != 5) return NULL;
	if (hdr.bitsPerPixel != 1 && hdr.bitsPerPixel != 8 && hdr.bitsPerPixel != 4) return NULL; 
	if (hdr.bitsPerPixel == 1 && hdr.numColorPlanes !=1 && hdr.numColorPlanes != 4) return NULL;
	if (hdr.bitsPerPixel == 8 && hdr.bytesPerScanLine != ((hdr.xmax - hdr.xmin + 2)&~1)) return NULL;

	for (int i = 0; i < 54; i++) 
	{
		if (hdr.padding[i] != 0) return NULL;
	}

	file.Seek(0, FileReader::SeekSet);
	file.Read(&hdr, sizeof(hdr));

	return new FPCXTexture(lumpnum, hdr);
}

//==========================================================================
//
//
//
//==========================================================================

FPCXTexture::FPCXTexture(int lumpnum, PCXHeader & hdr)
: FImageSource(lumpnum)
{
	bMasked = false;
	Width = LittleShort(hdr.xmax) - LittleShort(hdr.xmin) + 1;
	Height = LittleShort(hdr.ymax) - LittleShort(hdr.ymin) + 1;
}

//==========================================================================
//
//
//
//==========================================================================

void FPCXTexture::ReadPCX1bit (uint8_t *dst, FileReader & lump, PCXHeader *hdr)
{
	int y, i, bytes;
	int rle_count = 0;
	uint8_t rle_value = 0;

	TArray<uint8_t> srcp = lump.Read(lump.GetLength() - sizeof(PCXHeader));
	uint8_t * src = srcp.Data();

	for (y = 0; y < Height; ++y)
	{
		uint8_t * ptr = &dst[y * Width];

		bytes = hdr->bytesPerScanLine;

		while (bytes--)
		{
			if (rle_count == 0)
			{
				if ( (rle_value = *src++) < 0xc0)
				{
					rle_count = 1;
				}
				else
				{
					rle_count = rle_value - 0xc0;
					rle_value = *src++;
				}
			}

			rle_count--;

			for (i = 7; i >= 0; --i, ptr ++)
			{
				// This can overflow for the last byte if not checked.
				if (ptr < dst+Width*Height)
					*ptr = ((rle_value & (1 << i)) > 0);
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FPCXTexture::ReadPCX4bits (uint8_t *dst, FileReader & lump, PCXHeader *hdr)
{
	int rle_count = 0, rle_value = 0;
	int x, y, c;
	int bytes;
	TArray<uint8_t> line(hdr->bytesPerScanLine, true);
	TArray<uint8_t> colorIndex(Width, true);

	TArray<uint8_t> srcp = lump.Read(lump.GetLength() - sizeof(PCXHeader));
	uint8_t * src = srcp.Data();

	for (y = 0; y < Height; ++y)
	{
		uint8_t * ptr = &dst[y * Width];
		memset (ptr, 0, Width * sizeof (uint8_t));

		for (c = 0; c < 4; ++c)
		{
			uint8_t * pLine = line.Data();

			bytes = hdr->bytesPerScanLine;

			while (bytes--)
			{
				if (rle_count == 0)
				{
					if ( (rle_value = *src++) < 0xc0)
					{
						rle_count = 1;
					}
					else
					{
						rle_count = rle_value - 0xc0;
						rle_value = *src++;
					}
				}

				rle_count--;
				*(pLine++) = rle_value;
			}
		}

		/* compute line's color indexes */
		for (x = 0; x < Width; ++x)
		{
			if (line[x / 8] & (128 >> (x % 8)))
				ptr[x] += (1 << c);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FPCXTexture::ReadPCX8bits (uint8_t *dst, FileReader & lump, PCXHeader *hdr)
{
	int rle_count = 0, rle_value = 0;
	int y, bytes;

	auto srcp = lump.Read(lump.GetLength() - sizeof(PCXHeader));
	uint8_t * src = srcp.Data();

	for (y = 0; y < Height; ++y)
	{
		uint8_t * ptr = &dst[y * Width];

		bytes = hdr->bytesPerScanLine;
		while (bytes--)
		{
			if (rle_count == 0)
			{
				if( (rle_value = *src++) < 0xc0)
				{
					rle_count = 1;
				}
				else
				{
					rle_count = rle_value - 0xc0;
					rle_value = *src++;
				}
			}

			rle_count--;
			*ptr++ = rle_value;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FPCXTexture::ReadPCX24bits (uint8_t *dst, FileReader & lump, PCXHeader *hdr, int planes)
{
	int rle_count = 0, rle_value = 0;
	int y, c;
	int bytes;

	auto srcp = lump.Read(lump.GetLength() - sizeof(PCXHeader));
	uint8_t * src = srcp.Data();

	for (y = 0; y < Height; ++y)
	{
		/* for each color plane */
		for (c = 0; c < planes; ++c)
		{
			uint8_t * ptr = &dst[y * Width * planes];
			bytes = hdr->bytesPerScanLine;

			while (bytes--)
			{
				if (rle_count == 0)
				{
					if( (rle_value = *src++) < 0xc0)
					{
						rle_count = 1;
					}
					else
					{
						rle_count = rle_value - 0xc0;
						rle_value = *src++;
					}
				}

				rle_count--;
				ptr[c] = (uint8_t)rle_value;
				ptr += planes;
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

TArray<uint8_t> FPCXTexture::CreatePalettedPixels(int conversion)
{
	uint8_t PaletteMap[256];
	PCXHeader header;
	int bitcount;

	auto lump = Wads.OpenLumpReader(SourceLump);

	lump.Read(&header, sizeof(header));

	bitcount = header.bitsPerPixel * header.numColorPlanes;
	TArray<uint8_t> Pixels(Width*Height, true);

	bool alphatex = conversion == luminance;
	if (bitcount < 24)
	{
		if (bitcount < 8)
		{
			switch (bitcount)
			{
			default:
			case 1:
				PaletteMap[0] = alphatex? 0 : ImageHelpers::GrayMap[0];
				PaletteMap[1] = alphatex? 255 : ImageHelpers::GrayMap[255];
				ReadPCX1bit (Pixels.Data(), lump, &header);
				break;

			case 4:
				for (int i = 0; i < 16; i++)
				{
					PaletteMap[i] = ImageHelpers::RGBToPalettePrecise(alphatex, header.palette[i * 3], header.palette[i * 3 + 1], header.palette[i * 3 + 2]);
				}
				ReadPCX4bits (Pixels.Data(), lump, &header);
				break;
			}
		}
		else if (bitcount == 8)
		{
			lump.Seek(-769, FileReader::SeekEnd);
			uint8_t c = lump.ReadUInt8();
			//if (c !=0x0c) memcpy(PaletteMap, GrayMap, 256);	// Fallback for files without palette
			//else 
			for(int i=0;i<256;i++)
			{
				uint8_t r = lump.ReadUInt8();
				uint8_t g = lump.ReadUInt8();
				uint8_t b = lump.ReadUInt8();
				PaletteMap[i] = ImageHelpers::RGBToPalettePrecise(alphatex, r, g, b);
			}
			lump.Seek(sizeof(header), FileReader::SeekSet);
			ReadPCX8bits (Pixels.Data(), lump, &header);
		}
		if (Width == Height)
		{
			ImageHelpers::FlipSquareBlockRemap(Pixels.Data(), Width, PaletteMap);
		}
		else
		{
			TArray<uint8_t> newpix(Width*Height, true);
			ImageHelpers::FlipNonSquareBlockRemap (newpix.Data(), Pixels.Data(), Width, Height, Width, PaletteMap);
			return newpix;
		}
	}
	else
	{
		TArray<uint8_t> buffer(Width*Height * 3, true);
		uint8_t * row = buffer.Data();
		ReadPCX24bits (row, lump, &header, 3);
		for(int y=0; y<Height; y++)
		{
			for(int x=0; x < Width; x++)
			{
				Pixels[y + Height * x] = ImageHelpers::RGBToPalette(alphatex, row[0], row[1], row[2]);
				row+=3;
			}
		}
	}
	return Pixels;
}

//===========================================================================
//
// FPCXTexture::CopyPixels
//
// Preserves the full color information (unlike software mode)
//
//===========================================================================

int FPCXTexture::CopyPixels(FBitmap *bmp, int conversion)
{
	PalEntry pe[256];
	PCXHeader header;
	int bitcount;
	TArray<uint8_t> Pixels;

	auto lump = Wads.OpenLumpReader(SourceLump);

	lump.Read(&header, sizeof(header));

	bitcount = header.bitsPerPixel * header.numColorPlanes;

	if (bitcount < 24)
	{
		Pixels.Resize(Width*Height);
		if (bitcount < 8)
		{
			switch (bitcount)
			{
			default:
			case 1:
				pe[0] = PalEntry(255, 0, 0, 0);
				pe[1] = PalEntry(255, 255, 255, 255);
				ReadPCX1bit (Pixels.Data(), lump, &header);
				break;

			case 4:
				for (int i = 0; i<16; i++)
				{
					pe[i] = PalEntry(255, header.palette[i * 3], header.palette[i * 3 + 1], header.palette[i * 3 + 2]);
				}
				ReadPCX4bits (Pixels.Data(), lump, &header);
				break;
			}
		}
		else if (bitcount == 8)
		{
			lump.Seek(-769, FileReader::SeekEnd);
			uint8_t c = lump.ReadUInt8();
			c=0x0c;	// Apparently there's many non-compliant PCXs out there...
			if (c !=0x0c) 
			{
				for(int i=0;i<256;i++) pe[i]=PalEntry(255,i,i,i);	// default to a gray map
			}
			else for(int i=0;i<256;i++)
			{
				uint8_t r = lump.ReadUInt8();
				uint8_t g = lump.ReadUInt8();
				uint8_t b = lump.ReadUInt8();
				pe[i] = PalEntry(255, r,g,b);
			}
			lump.Seek(sizeof(header), FileReader::SeekSet);
			ReadPCX8bits (Pixels.Data(), lump, &header);
		}
		bmp->CopyPixelData(0, 0, Pixels.Data(), Width, Height, 1, Width, 0, pe);
	}
	else
	{
		Pixels.Resize(Width*Height*4);
		ReadPCX24bits (Pixels.Data(), lump, &header, 3);
		bmp->CopyPixelDataRGB(0, 0, Pixels.Data(), Width, Height, 3, Width*3, 0, CF_RGB);
	}
	return 0;
}

