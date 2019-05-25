/*
** tgatexture.cpp
** Texture class for TGA images
**
**---------------------------------------------------------------------------
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
#include "templates.h"
#include "bitmap.h"
#include "v_video.h"
#include "imagehelpers.h"
#include "image.h"


//==========================================================================
//
// TGA file header
//
//==========================================================================

#pragma pack(1)

struct TGAHeader
{
	uint8_t		id_len;
	uint8_t		has_cm;
	uint8_t		img_type;
	int16_t		cm_first;
	int16_t		cm_length;
	uint8_t		cm_size;
	
	int16_t		x_origin;
	int16_t		y_origin;
	int16_t		width;
	int16_t		height;
	uint8_t		bpp;
	uint8_t		img_desc;
};

#pragma pack()

//==========================================================================
//
// a TGA texture
//
//==========================================================================

class FTGATexture : public FImageSource
{
public:
	FTGATexture (int lumpnum, TGAHeader *);

	int CopyPixels(FBitmap *bmp, int conversion) override;

protected:
	void ReadCompressed(FileReader &lump, uint8_t * buffer, int bytesperpixel);
	TArray<uint8_t> CreatePalettedPixels(int conversion) override;
};

//==========================================================================
//
//
//
//==========================================================================

FImageSource *TGAImage_TryCreate(FileReader & file, int lumpnum)
{
	TGAHeader hdr;

	if (file.GetLength() < (long)sizeof(hdr)) return NULL;
	
	file.Seek(0, FileReader::SeekSet);
	file.Read(&hdr, sizeof(hdr));
	hdr.width = LittleShort(hdr.width);
	hdr.height = LittleShort(hdr.height);
	
	// Not much that can be done here because TGA does not have a proper
	// header to be identified with.
	if (hdr.has_cm != 0 && hdr.has_cm != 1) return NULL;
	if (hdr.width <=0 || hdr.height <=0 || hdr.width > 2048 || hdr.height > 2048) return NULL;
	if (hdr.bpp != 8 && hdr.bpp != 15 && hdr.bpp != 16 && hdr.bpp !=24 && hdr.bpp !=32) return NULL;
	if (hdr.img_type <= 0 || hdr.img_type > 11) return NULL;
	if (hdr.img_type >=4  && hdr.img_type <= 8) return NULL;
	if ((hdr.img_desc & 16) != 0) return NULL;

	file.Seek(0, FileReader::SeekSet);
	file.Read(&hdr, sizeof(hdr));
	hdr.width = LittleShort(hdr.width);
	hdr.height = LittleShort(hdr.height);

	return new FTGATexture(lumpnum, &hdr);
}

//==========================================================================
//
//
//
//==========================================================================

FTGATexture::FTGATexture (int lumpnum, TGAHeader * hdr)
: FImageSource(lumpnum)
{
	Width = hdr->width;
	Height = hdr->height;
	// Alpha channel is used only for 32 bit RGBA and paletted images with RGBA palettes.
	bMasked = (hdr->img_desc&15)==8 && (hdr->bpp==32 || (hdr->img_type==1 && hdr->cm_size==32));
}

//==========================================================================
//
//
//
//==========================================================================

void FTGATexture::ReadCompressed(FileReader &lump, uint8_t * buffer, int bytesperpixel)
{
	uint8_t data[4];
	int Size = Width * Height;
	
	while (Size > 0) 
	{
		uint8_t b = lump.ReadUInt8();
		if (b & 128)
		{
			b&=~128;
			lump.Read(data, bytesperpixel);
			for (int i=MIN<int>(Size, (b+1)); i>0; i--)
			{
				buffer[0] = data[0];
				if (bytesperpixel>=2) buffer[1] = data[1];
				if (bytesperpixel>=3) buffer[2] = data[2];
				if (bytesperpixel==4) buffer[3] = data[3];
				buffer+=bytesperpixel;
			}
		}
		else 
		{
			lump.Read(buffer, MIN<int>(Size, (b+1))*bytesperpixel);
			buffer += (b+1)*bytesperpixel;
		}
		Size -= b+1;
	}
}

//==========================================================================
//
//
//
//==========================================================================

TArray<uint8_t> FTGATexture::CreatePalettedPixels(int conversion)
{
	uint8_t PaletteMap[256];
	auto lump = Wads.OpenLumpReader (SourceLump);
	TGAHeader hdr;
	uint16_t w;
	uint8_t r,g,b,a;

	TArray<uint8_t> Pixels(Width*Height, true);
	lump.Read(&hdr, sizeof(hdr));
	lump.Seek(hdr.id_len, FileReader::SeekCur);
	
	hdr.width = LittleShort(hdr.width);
	hdr.height = LittleShort(hdr.height);
	hdr.cm_first = LittleShort(hdr.cm_first);
	hdr.cm_length = LittleShort(hdr.cm_length);

	if (hdr.has_cm)
	{
		memset(PaletteMap, 0, 256);
		for (int i = hdr.cm_first; i < hdr.cm_first + hdr.cm_length && i < 256; i++)
		{
			switch (hdr.cm_size)
			{
			case 15:
			case 16:
				w = lump.ReadUInt16();
				r = (w & 0x001F) << 3;
				g = (w & 0x03E0) >> 2;
				b = (w & 0x7C00) >> 7;
				a = 255;
				break;
				
			case 24:
				b = lump.ReadUInt8();
				g = lump.ReadUInt8();
				r = lump.ReadUInt8();
				a=255;
				break;
				
			case 32:
				b = lump.ReadUInt8();
				g = lump.ReadUInt8();
				r = lump.ReadUInt8();
				a = lump.ReadUInt8();
				if ((hdr.img_desc&15)!=8) a=255;
				break;
				
			default:	// should never happen
				r=g=b=a=0;
				break;
			}
			PaletteMap[i] = ImageHelpers::RGBToPalettePrecise(conversion == luminance, r, g, b, a);
		}
    }
    
    int Size = Width * Height * (hdr.bpp>>3);
	TArray<uint8_t> buffer(Size, true);
   	
    if (hdr.img_type < 4)	// uncompressed
    {
    	lump.Read(buffer.Data(), Size);
    }
    else				// compressed
    {
    	ReadCompressed(lump, buffer.Data(), hdr.bpp>>3);
    }
    
	uint8_t * ptr = buffer.Data();
	int step_x = (hdr.bpp>>3);
	int Pitch = Width * step_x;

	/*
	if (hdr.img_desc&32)
	{
		ptr += (Width-1) * step_x;
		step_x =- step_x;
	}
	*/
	if (!(hdr.img_desc&32))
	{
		ptr += (Height-1) * Pitch;
		Pitch = -Pitch;
	}

    switch (hdr.img_type & 7)
    {
	case 1:	// paletted
		for(int y=0;y<Height;y++)
		{
			uint8_t * p = ptr + y * Pitch;
			for(int x=0;x<Width;x++)
			{
				Pixels[x*Height+y] = PaletteMap[*p];
				p+=step_x;
			}
		}
		break;

	case 2:	// RGB
		switch (hdr.bpp)
		{
		case 15:
		case 16:
			step_x>>=1;
			for(int y=0;y<Height;y++)
			{
				uint16_t * p = (uint16_t*)(ptr + y * Pitch);
				for(int x=0;x<Width;x++)
				{
					int v = LittleShort(*p);
					Pixels[x*Height + y] = ImageHelpers::RGBToPalette(conversion == luminance, ((v >> 10) & 0x1f) * 8, ((v >> 5) & 0x1f) * 8, (v & 0x1f) * 8);
					p+=step_x;
				}
			}
			break;
		
		case 24:
			for(int y=0;y<Height;y++)
			{
				uint8_t * p = ptr + y * Pitch;
				for(int x=0;x<Width;x++)
				{
					Pixels[x*Height + y] = ImageHelpers::RGBToPalette(conversion == luminance, p[2], p[1], p[0]);
					p+=step_x;
				}
			}
			break;
		
		case 32:
			if ((hdr.img_desc&15)!=8)	// 32 bits without a valid alpha channel
			{
				for(int y=0;y<Height;y++)
				{
					uint8_t * p = ptr + y * Pitch;
					for(int x=0;x<Width;x++)
					{
						Pixels[x*Height + y] = ImageHelpers::RGBToPalette(conversion == luminance, p[2], p[1], p[0]);
						p+=step_x;
					}
				}
			}
			else
			{
				for(int y=0;y<Height;y++)
				{
					uint8_t * p = ptr + y * Pitch;
					for(int x=0;x<Width;x++)
					{
						Pixels[x*Height + y] = ImageHelpers::RGBToPalette(conversion == luminance, p[2], p[1], p[0], p[3]);
						p+=step_x;
					}
				}
			}
			break;
		
		default:
			break;
		}
		break;
	
	case 3:	// Grayscale
	{
		auto remap = ImageHelpers::GetRemap(conversion == luminance, true);
		switch (hdr.bpp)
		{
		case 8:
			for (int y = 0; y < Height; y++)
			{
				uint8_t * p = ptr + y * Pitch;
				for (int x = 0; x < Width; x++)
				{
					Pixels[x*Height + y] = remap[*p];
					p += step_x;
				}
			}
			break;

		case 16:
			for (int y = 0; y < Height; y++)
			{
				uint8_t * p = ptr + y * Pitch;
				for (int x = 0; x < Width; x++)
				{
					Pixels[x*Height + y] = remap[p[1]];	// only use the high byte
					p += step_x;
				}
			}
			break;

		default:
			break;
		}
		break;
	}
	default:
		break;
    }
	return Pixels;
}	

//===========================================================================
//
// FTGATexture::CopyPixels
//
//===========================================================================

int FTGATexture::CopyPixels(FBitmap *bmp, int conversion)
{
	PalEntry pe[256];
	auto lump = Wads.OpenLumpReader (SourceLump);
	TGAHeader hdr;
	uint16_t w;
	uint8_t r,g,b,a;
	int transval = 0;

	lump.Read(&hdr, sizeof(hdr));
	lump.Seek(hdr.id_len, FileReader::SeekCur);
	
	hdr.width = LittleShort(hdr.width);
	hdr.height = LittleShort(hdr.height);
	hdr.cm_first = LittleShort(hdr.cm_first);
	hdr.cm_length = LittleShort(hdr.cm_length);

	if (hdr.has_cm)
	{
		memset(pe, 0, 256*sizeof(PalEntry));
		for (int i = hdr.cm_first; i < hdr.cm_first + hdr.cm_length && i < 256; i++)
		{
			switch (hdr.cm_size)
			{
			case 15:
			case 16:
				w = lump.ReadUInt16();
				r = (w & 0x001F) << 3;
				g = (w & 0x03E0) >> 2;
				b = (w & 0x7C00) >> 7;
				a = 255;
				break;

			case 24:
				b = lump.ReadUInt8();
				g = lump.ReadUInt8();
				r = lump.ReadUInt8();
				a = 255;
				break;

			case 32:
				b = lump.ReadUInt8();
				g = lump.ReadUInt8();
				r = lump.ReadUInt8();
				a = lump.ReadUInt8();
				if ((hdr.img_desc & 15) != 8) a = 255;
				else if (a != 0 && a != 255) transval = true;
				break;

			default:	// should never happen
				r=g=b=a=0;
				break;
			}
			pe[i] = PalEntry(a, r, g, b);
		}
    }
    
    int Size = Width * Height * (hdr.bpp>>3);
	TArray<uint8_t> sbuffer(Size);
   	
    if (hdr.img_type < 4)	// uncompressed
    {
    	lump.Read(sbuffer.Data(), Size);
    }
    else				// compressed
    {
    	ReadCompressed(lump, sbuffer.Data(), hdr.bpp>>3);
    }
    
	uint8_t * ptr = sbuffer.Data();
	int step_x = (hdr.bpp>>3);
	int Pitch = Width * step_x;

	/*
	if (hdr.img_desc&32)
	{
		ptr += (Width-1) * step_x;
		step_x =- step_x;
	}
	*/
	if (!(hdr.img_desc&32))
	{
		ptr += (Height-1) * Pitch;
		Pitch = -Pitch;
	}

    switch (hdr.img_type & 7)
    {
	case 1:	// paletted
		bmp->CopyPixelData(0, 0, ptr, Width, Height, step_x, Pitch, 0, pe);
		break;

	case 2:	// RGB
		switch (hdr.bpp)
		{
		case 15:
		case 16:
			bmp->CopyPixelDataRGB(0, 0, ptr, Width, Height, step_x, Pitch, 0, CF_RGB555);
			break;
		
		case 24:
			bmp->CopyPixelDataRGB(0, 0, ptr, Width, Height, step_x, Pitch, 0, CF_BGR);
			break;
		
		case 32:
			if ((hdr.img_desc&15)!=8)	// 32 bits without a valid alpha channel
			{
				bmp->CopyPixelDataRGB(0, 0, ptr, Width, Height, step_x, Pitch, 0, CF_BGR);
			}
			else
			{
				bmp->CopyPixelDataRGB(0, 0, ptr, Width, Height, step_x, Pitch, 0, CF_BGRA);
				transval = -1;
			}
			break;
		
		default:
			break;
		}
		break;
	
	case 3:	// Grayscale
		switch (hdr.bpp)
		{
		case 8:
			for(int i=0;i<256;i++) pe[i]=PalEntry(255,i,i,i);	// gray map
			bmp->CopyPixelData(0, 0, ptr, Width, Height, step_x, Pitch, 0, pe);
			break;
		
		case 16:
			bmp->CopyPixelDataRGB(0, 0, ptr, Width, Height, step_x, Pitch, 0, CF_I16);
			break;
		
		default:
			break;
		}
		break;

	default:
		break;
    }
	return transval;
}	
