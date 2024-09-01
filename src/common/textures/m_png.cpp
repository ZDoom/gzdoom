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

#include "m_crc32.h"
#include "m_swap.h"
#if __has_include("c_cvars.h")
#include "c_cvars.h"
#endif
#include "m_png.h"
#include "basics.h"

#include "bitmap.h"

#include "vectors.h"

// MACROS ------------------------------------------------------------------

// The maximum size of an IDAT chunk ZDoom will write. This is also the
// size of the compression buffer it allocates on the stack.
#define PNG_WRITE_SIZE	32768

// TYPES -------------------------------------------------------------------

struct IHDR
{
	uint32_t		Width;
	uint32_t		Height;
	uint8_t		BitDepth;
	uint8_t		ColorType;
	uint8_t		Compression;
	uint8_t		Filter;
	uint8_t		Interlace;
};


// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static inline void MakeChunk (void *where, uint32_t type, size_t len);
static inline void StuffPalette (const PalEntry *from, uint8_t *to);
static bool WriteIDAT (FileWriter *file, const uint8_t *data, int len);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// allow this to compile without CVARs.
#if __has_include("c_cvars.h")
CUSTOM_CVAR(Int, png_level, 5, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 9)
		self = 9;
}
CVAR(Float, png_gamma, 0.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
#else
const int png_level = 5;
const float png_gamma = 0;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// M_CreatePNG
//
// Passed a newly-created file, writes the PNG signature and IHDR, gAMA, and
// PLTE chunks. Returns true if everything went as expected.
//
//==========================================================================

bool M_CreatePNG (FileWriter *file, const uint8_t *buffer, const PalEntry *palette,
				  ESSType color_type, int width, int height, int pitch, float gamma)
{
	uint8_t work[8 +				// signature
			  12+2*4+5 +		// IHDR
			  12+4 +			// gAMA
			  12+256*3];		// PLTE
	uint32_t *const sig = (uint32_t *)&work[0];
	IHDR *const ihdr = (IHDR *)&work[8 + 8];
	uint32_t *const gama = (uint32_t *)((uint8_t *)ihdr + 2*4+5 + 12);
	uint8_t *const plte = (uint8_t *)gama + 4 + 12;
	size_t work_len;

	sig[0] = MAKE_ID(137,'P','N','G');
	sig[1] = MAKE_ID(13,10,26,10);

	ihdr->Width = BigLong(width);
	ihdr->Height = BigLong(height);
	ihdr->BitDepth = 8;
	ihdr->ColorType = color_type == SS_PAL ? 3 : 2;
	ihdr->Compression = 0;
	ihdr->Filter = 0;
	ihdr->Interlace = 0;
	MakeChunk (ihdr, MAKE_ID('I','H','D','R'), 2*4+5);

	// Assume a display exponent of 2.2 (100000/2.2 ~= 45454.5)
	*gama = BigLong (int (45454.5f * (png_gamma == 0.f ? gamma : png_gamma)));
	MakeChunk (gama, MAKE_ID('g','A','M','A'), 4);

	if (color_type == SS_PAL)
	{
		StuffPalette (palette, plte);
		MakeChunk (plte, MAKE_ID('P','L','T','E'), 256*3);
		work_len = sizeof(work);
	}
	else
	{
		work_len = sizeof(work) - (12+256*3);
	}

	if (file->Write (work, work_len) != work_len)
		return false;

	return M_SaveBitmap (buffer, color_type, width, height, pitch, file);
}

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
// M_FinishPNG
//
// Writes an IEND chunk to a PNG file. The file is left opened.
//
//==========================================================================

bool M_FinishPNG (FileWriter *file)
{
	static const uint8_t iend[12] = { 0,0,0,0,73,69,78,68,174,66,96,130 };
	return file->Write (iend, 12) == 12;
}

//==========================================================================
//
// M_AppendPNGText
//
// Appends a PNG tEXt chunk to the file
//
//==========================================================================

bool M_AppendPNGText (FileWriter *file, const char *keyword, const char *text)
{
	struct { uint32_t len, id; char key[80]; } head;
	int len = (int)strlen (text);
	int keylen = min ((int)strlen (keyword), 79);
	uint32_t crc;

	head.len = BigLong(len + keylen + 1);
	head.id = MAKE_ID('t','E','X','t');
	memset (&head.key, 0, sizeof(head.key));
	strncpy (head.key, keyword, keylen);
	head.key[keylen] = 0;

	if ((int)file->Write (&head, keylen + 9) == keylen + 9 &&
		(int)file->Write (text, len) == len)
	{
		crc = CalcCRC32 ((uint8_t *)&head+4, keylen + 5);
		if (len != 0)
		{
			crc = AddCRC32 (crc, (uint8_t *)text, len);
		}
		crc = BigLong(crc);
		return file->Write (&crc, 4) == 4;
	}
	return false;
}

//==========================================================================
//
// PNG READING
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

	for(int i = 0; i < height; i++)
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

// PRIVATE CODE ------------------------------------------------------------

//==========================================================================
//
// MakeChunk
//
// Prepends the chunk length and type and appends the chunk's CRC32.
// There must be 8 bytes available before the chunk passed and 4 bytes
// after the chunk.
//
//==========================================================================

static inline void MakeChunk (void *where, uint32_t type, size_t len)
{
	uint8_t *const data = (uint8_t *)where;
	*(uint32_t *)(data - 8) = BigLong ((unsigned int)len);
	*(uint32_t *)(data - 4) = type;
	*(uint32_t *)(data + len) = BigLong ((unsigned int)CalcCRC32 (data-4, (unsigned int)(len+4)));
}

//==========================================================================
//
// StuffPalette
//
// Converts 256 4-byte palette entries to 3 bytes each.
//
//==========================================================================

static void StuffPalette (const PalEntry *from, uint8_t *to)
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

#define SelectFilter(x,y,z)		0

//==========================================================================
//
// M_SaveBitmap
//
// Given a bitmap, creates one or more IDAT chunks in the given file.
// Returns true on success.
//
//==========================================================================

bool M_SaveBitmap(const uint8_t *from, ESSType color_type, int width, int height, int pitch, FileWriter *file)
{
	TArray<Byte> temprow_storage;

	static const unsigned temprow_count = 1;

	const unsigned temprow_size = 1 + width * 3;
	temprow_storage.Resize(temprow_size * temprow_count);

	Byte* temprow[temprow_count];

	for (unsigned i = 0; i < temprow_count; ++i)
	{
		temprow[i] = &temprow_storage[temprow_size * i];
	}

	TArray<Byte> array(PNG_WRITE_SIZE, true);
	auto buffer = array.data();
	z_stream stream;
	int err;
	int y;

	stream.next_in = Z_NULL;
	stream.avail_in = 0;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	err = deflateInit (&stream, png_level);

	if (err != Z_OK)
	{
		return false;
	}

	y = height;
	stream.next_out = buffer;
	stream.avail_out = sizeof(buffer);

	temprow[0][0] = 0;

	while (y-- > 0 && err == Z_OK)
	{
		switch (color_type)
		{
		case SS_PAL:
			memcpy(&temprow[0][1], from, width);
			// always use filter type 0 for paletted images
			stream.next_in = temprow[0];
			stream.avail_in = width + 1;
			break;

		case SS_RGB:
			memcpy(&temprow[0][1], from, width*3);
			stream.next_in = temprow[SelectFilter(temprow, prior, width)];
			stream.avail_in = width * 3 + 1;
			break;

		case SS_BGRA:
			for (int x = 0; x < width; ++x)
			{
				temprow[0][x*3 + 1] = from[x*4 + 2];
				temprow[0][x*3 + 2] = from[x*4 + 1];
				temprow[0][x*3 + 3] = from[x*4];
			}
			stream.next_in = temprow[SelectFilter(temprow, prior, width)];
			stream.avail_in = width * 3 + 1;
			break;
		}

		from += pitch;

		err = deflate (&stream, (y == 0) ? Z_FINISH : 0);
		if (err != Z_OK)
		{
			break;
		}
		while (stream.avail_out == 0)
		{
			if (!WriteIDAT (file, buffer, sizeof(buffer)))
			{
				return false;
			}
			stream.next_out = buffer;
			stream.avail_out = sizeof(buffer);
			if (stream.avail_in != 0)
			{
				err = deflate (&stream, (y == 0) ? Z_FINISH : 0);
				if (err != Z_OK)
				{
					break;
				}
			}
		}
	}

	while (err == Z_OK)
	{
		err = deflate (&stream, Z_FINISH);
		if (err != Z_OK)
		{
			break;
		}
		if (stream.avail_out == 0)
		{
			if (!WriteIDAT (file, buffer, sizeof(buffer)))
			{
				return false;
			}
			stream.next_out = buffer;
			stream.avail_out = sizeof(buffer);
		}
	}

	deflateEnd (&stream);

	if (err != Z_STREAM_END)
	{
		return false;
	}
	return WriteIDAT (file, buffer, sizeof(buffer)-stream.avail_out);
}

//==========================================================================
//
// WriteIDAT
//
// Writes a single IDAT chunk to the file. Returns true on success.
//
//==========================================================================

static bool WriteIDAT (FileWriter *file, const uint8_t *data, int len)
{
	uint32_t foo[2], crc;

	foo[0] = BigLong (len);
	foo[1] = MAKE_ID('I','D','A','T');
	crc = CalcCRC32 ((uint8_t *)&foo[1], 4);
	crc = BigLong ((unsigned int)AddCRC32 (crc, data, len));

	if (file->Write (foo, 8) != 8 ||
		file->Write (data, len) != (size_t)len ||
		file->Write (&crc, 4) != 4)
	{
		return false;
	}
	return true;
}
