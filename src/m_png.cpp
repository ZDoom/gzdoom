/*
** m_png.cpp
** Routines for manipulating PNG files.
**
**---------------------------------------------------------------------------
** Copyright 2002 Randy Heit
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
** Currently, only 256-color paletted images are supported, as these are all
** that ZDoom needs to support. Expect this to become more complete in the
** future if I decide to add support for graphic patches stored as PNGs.
*/

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif

#include "m_crc32.h"
#include "m_swap.h"
#include "c_cvars.h"
#include "v_video.h"
#include "m_png.h"
#include "templates.h"

// MACROS ------------------------------------------------------------------

// The maximum size of an IDAT chunk ZDoom will write. This is also the
// size of the compression buffer it allocates on the stack.
#define PNG_WRITE_SIZE	32768

// TYPES -------------------------------------------------------------------

struct IHDR
{
	DWORD		Width;
	DWORD		Height;
	BYTE		BitDepth;
	BYTE		ColorType;
	BYTE		Compression;
	BYTE		Filter;
	BYTE		Interlace;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static inline void MakeChunk (void *where, DWORD type, size_t len);
static inline void StuffPalette (const PalEntry *from, BYTE *to);
static bool StuffBitmap (const DCanvas *canvas, FILE *file);
static bool WriteIDAT (FILE *file, const BYTE *data, int len);
static void UnfilterRow (int width, BYTE *dest, BYTE *stream, BYTE *prev);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Int, png_level, 5, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
	else if (self > 9)
		self = 9;
}
CVAR(Float, png_gamma, 0.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

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

bool M_CreatePNG (FILE *file, const DCanvas *canvas, const PalEntry *palette)
{
	BYTE work[8 +				// signature
			  12+2*4+5 +		// IHDR
			  12+4 +			// gAMA
			  12+256*3];		// PLTE
	DWORD *const sig = (DWORD *)&work[0];
	IHDR *const ihdr = (IHDR *)&work[8 + 8];
	DWORD *const gama = (DWORD *)((BYTE *)ihdr + 2*4+5 + 12);
	BYTE *const plte = (BYTE *)gama + 4 + 12;

	sig[0] = MAKE_ID(137,'P','N','G');
	sig[1] = MAKE_ID(13,10,26,10);

	ihdr->Width = BELONG (canvas->GetWidth ());
	ihdr->Height = BELONG (canvas->GetHeight ());
	ihdr->BitDepth = 8;
	ihdr->ColorType = 3;
	ihdr->Compression = 0;
	ihdr->Filter = 0;
	ihdr->Interlace = 0;
	MakeChunk (ihdr, MAKE_ID('I','H','D','R'), 2*4+5);

	// Assume a display exponent of 2.2 (100000/2.2 ~= 45454.5)
	*gama = BELONG (int (45454.5f * (png_gamma == 0.f ? Gamma : png_gamma)));
	MakeChunk (gama, MAKE_ID('g','A','M','A'), 4);

	StuffPalette (palette, plte);
	MakeChunk (plte, MAKE_ID('P','L','T','E'), 256*3);

	if (fwrite (work, 1, sizeof(work), file) != sizeof(work))
		return false;

	return StuffBitmap (canvas, file);
}

//==========================================================================
//
// M_CreateDummyPNG
//
// Like M_CreatePNG, but the image is always a grayscale 1x1 blacksquare.
//
//==========================================================================

bool M_CreateDummyPNG (FILE *file)
{
	static const BYTE dummyPNG[] =
	{
		137,'P','N','G',13,10,26,10,
		0,0,0,13,'I','H','D','R',
		0,0,0,1,0,0,0,1,8,0,0,0,0,0x3a,0x7e,0x9b,0x55,
		0,0,0,10,'I','D','A','T',
		104,222,99,96,0,0,0,2,0,1,0x9f,0x65,0x0e,0x18
	};
	return fwrite (dummyPNG, 1, sizeof(dummyPNG), file) == sizeof(dummyPNG);
}


//==========================================================================
//
// M_FinishPNG
//
// Writes an IEND chunk to a PNG file. The file is left opened.
//
//==========================================================================

bool M_FinishPNG (FILE *file)
{
	static const BYTE iend[12] = { 0,0,0,0,73,69,78,68,174,66,96,130 };
	return fwrite (iend, 1, 12, file) == 12;
}

//==========================================================================
//
// M_AppendPNGChunk
//
// Writes a PNG-compliant chunk to the file.
//
//==========================================================================

bool M_AppendPNGChunk (FILE *file, DWORD chunkID, const BYTE *chunkData, DWORD len)
{
	DWORD head[2] = { BELONG((unsigned int)len), chunkID };
	DWORD crc;

	if (fwrite (head, 1, 8, file) == 8 &&
		(len == 0 || fwrite (chunkData, 1, len, file) == len))
	{
		crc = CalcCRC32 ((BYTE *)&head[1], 4);
		if (len != 0)
		{
			crc = AddCRC32 (crc, chunkData, len);
		}
		crc = BELONG((unsigned int)crc);
		return fwrite (&crc, 1, 4, file) == 4;
	}
	return false;
}

//==========================================================================
//
// M_AppendPNGText
//
// Appends a PNG tEXt chunk to the file
//
//==========================================================================

bool M_AppendPNGText (FILE *file, const char *keyword, const char *text)
{
	struct { DWORD len, id; char key[80]; } head;
	int len = (int)strlen (text);
	int keylen = MIN ((int)strlen (keyword), 79);
	DWORD crc;

	head.len = BELONG(len + keylen + 1);
	head.id = MAKE_ID('t','E','X','t');
	memset (&head.key, 0, sizeof(head.key));
	strncpy (head.key, keyword, keylen);
	head.key[keylen] = 0;

	if ((int)fwrite (&head, 1, keylen + 9, file) == keylen + 9 &&
		(int)fwrite (text, 1, len, file) == len)
	{
		crc = CalcCRC32 ((BYTE *)&head+4, keylen + 5);
		if (len != 0)
		{
			crc = AddCRC32 (crc, (BYTE *)text, len);
		}
		crc = BELONG((unsigned int)crc);
		return fwrite (&crc, 1, 4, file) == 4;
	}
	return false;
}

//==========================================================================
//
// M_FindPNGChunk
//
// Finds a chunk in a PNG file. The file pointer will be positioned at the
// beginning of the chunk data, and its length will be returned. A return
// value of 0 indicates the chunk was either not present or had 0 length.
// This means there is no way to conclusively determine if a chunk is not
// present in a PNG file with this function, but since we're only
// interested in chunks with content, that's okay.
//
//==========================================================================

size_t M_FindPNGChunk (FILE *file, DWORD id)
{
	if (fseek (file, 8, SEEK_SET) != 0)
	{
		return 0;
	}
	return M_NextPNGChunk (file, id);
}

//==========================================================================
//
// M_NextPNGChunk
//
// Like M_FindPNGChunk, but it starts it search at the current chunk. This
// means the file pointer must be positioned at the length DWORD for a valid
// chunk.
//
//==========================================================================

size_t M_NextPNGChunk (FILE *file, DWORD id)
{
	DWORD data[2];

	while (fread (&data, 1, 8, file) == 8)
	{
		long len = BELONG((unsigned int)data[0]);

		if (data[1] == id)
		{ // Found the chunk
			return len;
		}
		if (data[1] == MAKE_ID('I','E','N','D'))
		{ // Chunk was not present
			return 0;
		}
		// Advance to next chunk
		if (fseek (file, len+4, SEEK_CUR) != 0)
		{
			return 0;
		}
	}
	// End-of-file
	return 0;
}

//==========================================================================
//
// M_FindPNGText
//
// Find a PNG text chunk with the given signature. The file will be positioned
// at the start of the text (after the keyword), and the length of the text
// is returned. This only supports tEXt, not zTXt.
//
//==========================================================================

size_t M_FindPNGText (FILE *file, const char *keyword)
{
	DWORD data[2];

	if (fseek (file, 8, SEEK_SET) != 0)
	{
		return 0;
	}
	while (fread (&data, 1, 8, file) == 8)
	{
		long len = BELONG((unsigned int)data[0]);

		if (data[1] == MAKE_ID('t','E','X','t'))
		{ // Found a tEXt chunk, so check the keyword
			char keychar;
			int keypos;

			for (keypos = 0; keypos < 80 && len-- > 0; ++keypos)
			{
				keychar = fgetc (file);
				if (keyword[keypos] != keychar)
				{ // Not the right keyword
					break;
				}
				if (keychar == 0)
				{ // End of keyword, so it is found
					return len;
				}
			}
		}
		if (data[1] == MAKE_ID('I','E','N','D'))
		{ // Text was not present
			return 0;
		}
		// Advance to next chunk
		if (fseek (file, len+4, SEEK_CUR) != 0)
		{
			return 0;
		}
	}
	// End-of-file
	return 0;
}

//==========================================================================
//
// M_GetPNGText
//
// Finds a PNG text chunk with the given signature and returns a pointer
// to a NULL-terminated string if present. Returns NULL on failure.
//
//==========================================================================

char *M_GetPNGText (FILE *file, const char *keyword)
{
	size_t len = M_FindPNGText (file, keyword);
	if (len != 0)
	{
		char *str = new char[len+1];
		if (fread (str, 1, len, file) == len)
		{
			str[len] = 0;
			return str;
		}
		delete[] str;
	}
	return NULL;
}

//==========================================================================
//
// M_VerifyPNG
//
// Returns true if the file is a PNG or false if not. CRC checking of chunks
// is not done in order to save time.
//
//==========================================================================

bool M_VerifyPNG (FILE *file)
{
	DWORD data[2];
	bool sawIDAT = false;

	if (fread (&data, 1, 8, file) != 8)
	{
		return false;
	}
	if (data[0] != MAKE_ID(137,'P','N','G') || data[1] != MAKE_ID(13,10,26,10))
	{ // Does not have PNG signature
		return false;
	}
	if (fread (&data, 1, 8, file) != 8)
	{
		return false;
	}
	if (data[1] != MAKE_ID('I','H','D','R'))
	{ // IHDR must be the first chunk
		return false;
	}
	while (fseek (file, BELONG((unsigned int)data[0])+4, SEEK_CUR) == 0)
	{
		// If the file ended before an IEND was encountered, it's not a PNG.
		if (fread (&data, 1, 8, file) != 8)
		{
			return false;
		}
		// An IEND chunk terminates the PNG and must be empty
		if (data[1] == MAKE_ID('I','E','N','D'))
		{
			return data[0] == 0 && sawIDAT;
		}
		// A PNG must include an IDAT chunk
		if (data[1] == MAKE_ID('I','D','A','T'))
		{
			sawIDAT = true;
		}
	}
	
	return false;
}

//==========================================================================
//
// M_CreateCanvasFromPNG
//
// Creates a simple canvas containing the contents of the PNG file's IDAT
// chunk(s). Only 8-bit images are supported.
//
//==========================================================================

DCanvas *M_CreateCanvasFromPNG (FILE *file)
{
	IHDR imageHeader;
	DSimpleCanvas *canvas;
	int width, height;
	size_t chunklen;

	if (M_FindPNGChunk (file, MAKE_ID('I','H','D','R')) == 0)
	{
		return NULL;
	}
	if (fread (&imageHeader, sizeof(IHDR), 1, file) != 1)
	{
		return NULL;
	}
	if (imageHeader.Width == 0 ||
		imageHeader.Height == 0 ||
		// Only images that M_CreatePNG can write are supported
		imageHeader.BitDepth != 8 ||
		imageHeader.ColorType != 3 ||
		imageHeader.Compression != 0 ||
		imageHeader.Filter != 0 ||
		imageHeader.Interlace != 0
		)
	{
		return NULL;
	}
	chunklen = M_FindPNGChunk (file, MAKE_ID('I','D','A','T'));
	if (chunklen == 0)
	{
		return NULL;
	}

	width = BELONG((int)imageHeader.Width);
	height = BELONG((int)imageHeader.Height);
	canvas = new DSimpleCanvas (width, height);
	if (canvas == NULL)
	{
		return NULL;
	}
	canvas->Lock ();

	Byte *inputLine, *prev, *curr;
	Byte chunkbuffer[4096];
	z_stream stream;
	int err;
	int y;
	bool lastIDAT;

	inputLine = (Byte *)alloca (1+width*2);
	prev = inputLine + 1 + width;
	memset (prev, 0, width);

	stream.next_in = Z_NULL;
	stream.avail_in = 0;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	err = inflateInit (&stream);
	if (err != Z_OK)
	{
		delete canvas;
		return NULL;
	}
	y = 0;
	curr = canvas->GetBuffer ();
	stream.next_out = inputLine;
	stream.avail_out = width+1;
	lastIDAT = false;

	do
	{
		while (err != Z_STREAM_END)
		{
			if (stream.avail_in == 0 && chunklen > 0)
			{
				stream.next_in = chunkbuffer;
				stream.avail_in = (uInt)fread (chunkbuffer, 1, MIN(chunklen,sizeof(chunkbuffer)), file);
				chunklen -= stream.avail_in;
			}

			err = inflate (&stream, Z_SYNC_FLUSH);
			if (err != Z_OK && err != Z_STREAM_END)
			{ // something unexpected happened
				inflateEnd (&stream);
				delete canvas;
				return NULL;
			}

			if (stream.avail_out == 0)
			{
				UnfilterRow (width, curr, inputLine, prev);
				prev = curr;
				curr += canvas->GetPitch ();
				y++;
				stream.next_out = inputLine;
				stream.avail_out = width+1;
			}

			if (chunklen == 0 && !lastIDAT)
			{
				DWORD x[3];

				if (fread (x, 1, 12, file) != 12)
				{
					lastIDAT = true;
				}
				else if (x[2] != MAKE_ID('I','D','A','T'))
				{
					lastIDAT = true;
				}
				else
				{
					chunklen = BELONG((unsigned int)x[1]);
				}
			}
		}
	} while (err == Z_OK && y < height);

	inflateEnd (&stream);
	canvas->Unlock ();
	return canvas;
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

static inline void MakeChunk (void *where, DWORD type, size_t len)
{
	BYTE *const data = (BYTE *)where;
	*(DWORD *)(data - 8) = BELONG ((unsigned int)len);
	*(DWORD *)(data - 4) = type;
	*(DWORD *)(data + len) = BELONG ((unsigned int)CalcCRC32 (data-4, (unsigned int)(len+4)));
}

//==========================================================================
//
// StuffPalette
//
// Converts 256 4-byte palette entries to 3 bytes each.
//
//==========================================================================

static inline void StuffPalette (const PalEntry *from, BYTE *to)
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
// StuffBitmap
//
// Given a bitmap, creates one or more IDAT chunks in the given file.
// Returns true on success.
//
//==========================================================================

static bool StuffBitmap (const DCanvas *canvas, FILE *file)
{
	const int pitch = canvas->GetPitch();
	const int width = canvas->GetWidth();
	const int height = canvas->GetHeight();
	BYTE *from = canvas->GetBuffer();

	Byte buffer[PNG_WRITE_SIZE];
	Byte zero = 0;
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

	while (y > 0 && err == Z_OK)
	{
		y--;
		for (int i = 2; i && err == Z_OK; --i)
		{
			const int flushiness = (y == 0 && i == 1) ? Z_FINISH : 0;
			if (i == 2)
			{ // always use filter type 0
				stream.next_in = &zero;
				stream.avail_in = 1;
			}
			else
			{
				stream.next_in = from;
				stream.avail_in = width;
				from += pitch;
			}
			err = deflate (&stream, flushiness);
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
					err = deflate (&stream, flushiness);
					if (err != Z_OK)
					{
						break;
					}
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

	y = sizeof(buffer) - stream.avail_out;
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

static bool WriteIDAT (FILE *file, const BYTE *data, int len)
{
	DWORD foo[2], crc;

	foo[0] = BELONG (len);
	foo[1] = MAKE_ID('I','D','A','T');
	crc = CalcCRC32 ((BYTE *)&foo[1], 4);
	crc = BELONG ((unsigned int)AddCRC32 (crc, data, len));

	if (fwrite (foo, 1, 8, file) != 8 ||
		fwrite (data, 1, len, file) != (size_t)len ||
		fwrite (&crc, 1, 4, file) != 4)
	{
		return false;
	}
	return true;
}

//==========================================================================
//
// UnfilterRow
//
// Unfilters the given row. Unknown filter types are silently ignored.
//
//==========================================================================

void UnfilterRow (int width, BYTE *dest, BYTE *row, BYTE *prev)
{
	int x;

	switch (*row++)
	{
	case 1:		// Sub
		dest[0] = row[0];
		for (x = 1; x < width; ++x)
		{
			dest[x] = row[x] + dest[x-1];
		}
		break;

	case 2:		// Up
		for (x = 0; x < width; ++x)
		{
			dest[x] = row[x] + prev[x];
		}
		break;

	case 3:		// Average
		dest[0] = row[0] + prev[0]/2;
		for (x = 1; x < width; ++x)
		{
			dest[x] = row[x] + (BYTE)(((unsigned)dest[x-1] + (unsigned)prev[x])/2);
		}
		break;

	case 4:		// Paeth
		dest[0] = row[0] + prev[0];
		for (x = 1; x < width; ++x)
		{
			int a, b, c, pa, pb, pc;

			a = dest[x-1];
			b = prev[x];
			c = prev[x-1];
			pa = abs (b - c);
			pb = abs (a - c);
			pc = abs (a + b - c - c);
			dest[x] = row[x] + (BYTE)((pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c);
		}
		break;

	default:	// Treat everything else as filter type 0 (none)
		memcpy (dest, row, width);
		break;
	}
}
