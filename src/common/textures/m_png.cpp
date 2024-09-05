/*
** m_png.cpp
** Routines for manipulating PNG files.
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include <algorithm>
#include <stdlib.h>
#include <miniz.h>
#include <stdint.h>
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif

#include <png.h>

#include <csetjmp>

#include "m_swap.h"
#if __has_include("c_cvars.h")
#include "c_cvars.h"
#endif
#include "m_png.h"
#include "basics.h"

#include "bitmap.h"

#include "vectors.h"

// TYPES -------------------------------------------------------------------

//==========================================================================
//
// M_CreateDummyPNG
//
// Like M_CreatePNG, but the image is always a grayscale 1x1 black square.
//
//==========================================================================

bool M_CreateDummyPNG (FileWriter *file)
{
	static const uint8_t dummyPNG[] =
	{
		137,'P','N','G',13,10,26,10,
		0,0,0,13,'I','H','D','R',
		0,0,0,1,0,0,0,1,8,0,0,0,0,0x3a,0x7e,0x9b,0x55,
		0,0,0,10,'I','D','A','T',
		104,222,99,96,0,0,0,2,0,1,0x9f,0x65,0x0e,0x18
	};
	return file->Write (dummyPNG, sizeof(dummyPNG)) == sizeof(dummyPNG);
}

//==========================================================================
//
// PNG Reading
//
//==========================================================================

static void PNGRead(png_structp readp, png_bytep dest, png_size_t count)
{
	FileReader * fr = static_cast<FileReader*>(png_get_io_ptr(readp));

	if(fr->Read(dest, count) != count)
	{
		png_error(readp, "PNG file too short");
	}
}

static void PNGWrite(png_structp readp, png_bytep dest, png_size_t count)
{
	FileWriter * fw = static_cast<FileWriter*>(png_get_io_ptr(readp));

	if(fw->Write(dest, count) != count)
	{
		png_error(readp, "PNG file too short");
	}
}

static void PNGFlush(png_structp readp)
{
	FileWriter * fw = static_cast<FileWriter*>(png_get_io_ptr(readp));

	fw->Flush();
}

static bool Internal_ReadPNG (FileReader * fr, FBitmap * out, int(*callback)(png_structp, png_unknown_chunkp), void * data, int start)
{
	TArray<uint8_t *> rows;

	if(!out) return false;

	png_structp readp = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if(!readp)
	{
		return false;
	}

	png_infop infop = png_create_info_struct(readp);

	if(!infop)
	{
		png_destroy_read_struct(&readp, NULL, NULL);
		return false;
	}

	png_infop end_infop = png_create_info_struct(readp);

	if(!end_infop)
	{
		png_destroy_read_struct(&readp, &infop, NULL);
		return false;
	}

	if(setjmp(png_jmpbuf(readp)))
	{
		png_destroy_read_struct(&readp, &infop, &end_infop);
		return false;
	}

	png_set_read_fn(readp, fr, &PNGRead);
	png_set_sig_bytes(readp, 8);

	if(callback) png_set_read_user_chunk_fn(readp, data, callback);

	png_read_info(readp, infop);

	uint32_t width = 0;
	uint32_t height = 0;
	int bitDepth = 0;
	int colorType = 0;

	if(png_get_IHDR(readp, infop, &width, &height, &bitDepth, &colorType, nullptr, nullptr, nullptr) != 1)
	{
		png_destroy_read_struct(&readp, &infop, &end_infop);
	}

	png_set_expand(readp);
	png_set_strip_16(readp);
	png_set_gray_to_rgb(readp);
	png_set_bgr(readp);
	png_set_filler(readp, 0xff, PNG_FILLER_AFTER);
	
	out->Destroy();
	out->Create(width, height);

	rows.Resize(height);

	for(unsigned i = 0; i < height; i++)
	{
		rows[i] = out->GetPixels() + (4 * width * i);
	}
	
	png_read_image(readp, rows.Data());
	png_read_end(readp, end_infop);
	png_destroy_read_struct(&readp, &infop, &end_infop);
	
	return true;
}

bool M_IsPNG(FileReader &fr)
{
	png_byte header[8];
	return (png_sig_cmp(header, 0, fr.Read(header, 8)) == 0);
}

#define IS_CHUNK(chunk, a, b, c, d) \
	((chunk)->name[0] == (a) && (chunk)->name[1] == (b) && (chunk)->name[2] == (c) && (chunk)->name[3] == (d) && (chunk)->name[4] == 0)

int ofs_ReadChunkCallback(png_structp png_ptr, png_unknown_chunkp chunk)
{
	if(IS_CHUNK(chunk, 'g','r','A','b') && chunk->size == 8)
	{
		TVector2<int32_t*> *ofs = static_cast<TVector2<int32_t*>*>(png_get_user_chunk_ptr(png_ptr));

		if(ofs->X) (*ofs->X)=(chunk->data[0]<<24)|(chunk->data[1]<<16)|(chunk->data[2]<<8)|(chunk->data[3]);
		if(ofs->Y) (*ofs->Y)=(chunk->data[4]<<24)|(chunk->data[5]<<16)|(chunk->data[6]<<8)|(chunk->data[7]);

		return 8;
	}
	return 0;
}

bool M_GetPNGSize(FileReader &fr, uint32_t &width, uint32_t &height, int32_t *ofs_x, int32_t *ofs_y, bool *isMasked)
{
	if(!M_IsPNG(fr))
	{ // Does not have PNG signature
		return false;
	}

	png_structp readp = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if(!readp)
	{
		return false;
	}

	png_infop infop = png_create_info_struct(readp);

	if(!infop)
	{
		png_destroy_read_struct(&readp, NULL, NULL);
		return false;
	}

	if(setjmp(png_jmpbuf(readp)))
	{
		png_destroy_read_struct(&readp, &infop, nullptr);
		return false;
	}

	png_set_read_fn(readp, &fr, &PNGRead);
	png_set_sig_bytes(readp, 8);

	TVector2<int32_t*> ofs(ofs_x, ofs_y);

	if(ofs_x != nullptr || ofs_y != nullptr) png_set_read_user_chunk_fn(readp, &ofs, ofs_ReadChunkCallback);

	png_read_info(readp, infop);

	int colorType = 0;

	if(png_get_IHDR(readp, infop, &width, &height, nullptr, &colorType, nullptr, nullptr, nullptr) != 1)
	{
		png_destroy_read_struct(&readp, &infop, nullptr);
		return false;
	}

	if(isMasked != nullptr)
	{
		(*isMasked) = (colorType & PNG_COLOR_MASK_ALPHA) || png_get_valid(readp, infop, PNG_INFO_tRNS);
	}

	png_destroy_read_struct(&readp, &infop, nullptr);

	return true;
}

bool M_ReadPNG(FileReader &&fr, FBitmap * out)
{
	if(!M_IsPNG(fr))
	{ // Does not have PNG signature
		return false;
	}

	FileReader png(std::move(fr));

	return Internal_ReadPNG(&png, out, nullptr, nullptr, 8);
}


//==========================================================================
//
// StuffPalette
//
// Converts 256 4-byte palette entries to 3 bytes each.
//
//==========================================================================

static void StuffPalette(const PalEntry *from, uint8_t *to)
{
	for (int i = 256; i > 0; --i)
	{
		to[0] = from->r;
		to[1] = from->g;
		to[2] = from->b;
		from += 1;
		to += 3;
	}
}

//==========================================================================
//
// PNG Writing
//
//==========================================================================

bool M_WritePNG(const uint8_t * buffer, uint32_t width, uint32_t height, const PalEntry (*palette)[256], double gamma, bool bgra, bool upside_down, const TArray<std::pair<FString, FString>> &text, FileWriter &out)
{
	TArray<const uint8_t *> rows;
	TArray<png_text> txt;
	TArray<uint8_t> pal;

	png_structp writep = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

	if(!writep)
	{
		return false;
	}

	png_infop infop = png_create_info_struct(writep);

	if(!infop)
	{
		png_destroy_write_struct(&writep, nullptr);
		return false;
	}

	if(setjmp(png_jmpbuf(writep)))
	{
		png_destroy_write_struct(&writep, &infop);
		return false;
	}

	png_set_write_fn(writep, &out, &PNGWrite, &PNGFlush);

	png_set_compression_level(writep, Z_BEST_COMPRESSION);

	png_set_IHDR(writep, infop, width, height, 8, palette ? PNG_COLOR_TYPE_PALETTE : (bgra ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB), PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	if(palette)
	{
		pal.Resize(3 * 256);
		StuffPalette(*palette, pal.Data());
		png_set_PLTE(writep, infop, (const png_color*) pal.Data(), 256);
	}

	png_set_gAMA(writep, infop, gamma);

	uint8_t trans = 0;

	png_set_tRNS(writep, infop, &trans, 1, 0);

	txt.Resize(text.Size());
	for(unsigned i = 0; i < text.Size(); i++)
	{
		txt[i].compression = -1;
		txt[i].key = (char*) text[i].first.GetChars();
		txt[i].text = (char*) text[i].second.GetChars();
		txt[i].text_length = text[i].second.Len();
		txt[i].itxt_length = 0;
		txt[i].lang = nullptr;
		txt[i].lang_key = nullptr;
	}

	png_set_text(writep, infop, txt.Data(), txt.Size());

	rows.Resize(height);

	int bpp = palette ? 1 : (bgra ? 4 : 3);

	if(upside_down)
	{
		for(unsigned i = height; i > 0; i--)
		{
			rows[i] = buffer + (bpp * width * (i - 1));
		}
	}
	else
	{
		for(unsigned i = 0; i < height; i++)
		{
			rows[i] = buffer + (bpp * width * i);
		}
	}

	png_set_rows(writep, infop, (png_bytepp) rows.Data());
	png_write_png(writep, infop, (!palette && bgra) ? PNG_TRANSFORM_BGR : 0, nullptr);
	png_write_end(writep, infop);

	png_destroy_write_struct(&writep, &infop);

	return true;
}

