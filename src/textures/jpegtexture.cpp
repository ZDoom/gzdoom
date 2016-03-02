/*
** jpegtexture.cpp
** Texture class for JPEG images
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
**
*/

#include <stdio.h>
extern "C"
{
#include <jpeglib.h>
}

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "v_text.h"
#include "bitmap.h"
#include "v_video.h"
#include "textures/textures.h"


struct FLumpSourceMgr : public jpeg_source_mgr
{
	FileReader *Lump;
	JOCTET Buffer[4096];
	bool StartOfFile;

	FLumpSourceMgr (FileReader *lump, j_decompress_ptr cinfo);
	static void InitSource (j_decompress_ptr cinfo);
	static boolean FillInputBuffer (j_decompress_ptr cinfo);
	static void SkipInputData (j_decompress_ptr cinfo, long num_bytes);
	static void TermSource (j_decompress_ptr cinfo);
};


//==========================================================================
//
//
//
//==========================================================================

void FLumpSourceMgr::InitSource (j_decompress_ptr cinfo)
{
	((FLumpSourceMgr *)(cinfo->src))->StartOfFile = true;
}

//==========================================================================
//
//
//
//==========================================================================

boolean FLumpSourceMgr::FillInputBuffer (j_decompress_ptr cinfo)
{
	FLumpSourceMgr *me = (FLumpSourceMgr *)(cinfo->src);
	long nbytes = me->Lump->Read (me->Buffer, sizeof(me->Buffer));

	if (nbytes <= 0)
	{
		me->Buffer[0] = (JOCTET)0xFF;
		me->Buffer[1] = (JOCTET)JPEG_EOI;
		nbytes = 2;
	}
	me->next_input_byte = me->Buffer;
	me->bytes_in_buffer = nbytes;
	me->StartOfFile = false;
	return TRUE;
}

//==========================================================================
//
//
//
//==========================================================================

void FLumpSourceMgr::SkipInputData (j_decompress_ptr cinfo, long num_bytes)
{
	FLumpSourceMgr *me = (FLumpSourceMgr *)(cinfo->src);
	if (num_bytes <= (long)me->bytes_in_buffer)
	{
		me->bytes_in_buffer -= num_bytes;
		me->next_input_byte += num_bytes;
	}
	else
	{
		num_bytes -= (long)me->bytes_in_buffer;
		me->Lump->Seek (num_bytes, SEEK_CUR);
		FillInputBuffer (cinfo);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FLumpSourceMgr::TermSource (j_decompress_ptr cinfo)
{
}

//==========================================================================
//
//
//
//==========================================================================

FLumpSourceMgr::FLumpSourceMgr (FileReader *lump, j_decompress_ptr cinfo)
: Lump (lump)
{
	cinfo->src = this;
	init_source = InitSource;
	fill_input_buffer = FillInputBuffer;
	skip_input_data = SkipInputData;
	resync_to_restart = jpeg_resync_to_restart;
	term_source = TermSource;
	bytes_in_buffer = 0;
	next_input_byte = NULL;
}

//==========================================================================
//
//
//
//==========================================================================

void JPEG_ErrorExit (j_common_ptr cinfo)
{
	(*cinfo->err->output_message) (cinfo);
	throw -1;
}

//==========================================================================
//
//
//
//==========================================================================

void JPEG_OutputMessage (j_common_ptr cinfo)
{
	char buffer[JMSG_LENGTH_MAX];

	(*cinfo->err->format_message) (cinfo, buffer);
	Printf (TEXTCOLOR_ORANGE "JPEG failure: %s\n", buffer);
}

//==========================================================================
//
// A JPEG texture
//
//==========================================================================

class FJPEGTexture : public FTexture
{
public:
	FJPEGTexture (int lumpnum, int width, int height);
	~FJPEGTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	FTextureFormat GetFormat ();
	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf = NULL);
	bool UseBasePalette();

protected:

	BYTE *Pixels;
	Span DummySpans[2];

	void MakeTexture ();

	friend class FTexture;
};

//==========================================================================
//
//
//
//==========================================================================

FTexture *JPEGTexture_TryCreate(FileReader & data, int lumpnum)
{
	union
	{
		DWORD dw;
		WORD w[2];
		BYTE b[4];
	} first4bytes;

	data.Seek(0, SEEK_SET);
	if (data.Read(&first4bytes, 4) < 4) return NULL;

	if (first4bytes.b[0] != 0xFF || first4bytes.b[1] != 0xD8 || first4bytes.b[2] != 0xFF)
		return NULL;

	// Find the SOFn marker to extract the image dimensions,
	// where n is 0, 1, or 2 (other types are unsupported).
	while ((unsigned)first4bytes.b[3] - 0xC0 >= 3)
	{
		if (data.Read (first4bytes.w, 2) != 2)
		{
			return NULL;
		}
		data.Seek (BigShort(first4bytes.w[0]) - 2, SEEK_CUR);
		if (data.Read (first4bytes.b + 2, 2) != 2 || first4bytes.b[2] != 0xFF)
		{
			return NULL;
		}
	}
	if (data.Read (first4bytes.b, 3) != 3)
	{
		return NULL;
	}
	if (BigShort (first4bytes.w[0]) < 5)
	{
		return NULL;
	}
	if (data.Read (first4bytes.b, 4) != 4)
	{
		return NULL;
	}
	return new FJPEGTexture (lumpnum, BigShort(first4bytes.w[1]), BigShort(first4bytes.w[0]));
}

//==========================================================================
//
//
//
//==========================================================================

FJPEGTexture::FJPEGTexture (int lumpnum, int width, int height)
: FTexture(NULL, lumpnum), Pixels(0)
{
	UseType = TEX_MiscPatch;
	LeftOffset = 0;
	TopOffset = 0;
	bMasked = false;

	Width = width;
	Height = height;
	CalcBitSize ();

	DummySpans[0].TopOffset = 0;
	DummySpans[0].Length = Height;
	DummySpans[1].TopOffset = 0;
	DummySpans[1].Length = 0;
}

//==========================================================================
//
//
//
//==========================================================================

FJPEGTexture::~FJPEGTexture ()
{
	Unload ();
}

//==========================================================================
//
//
//
//==========================================================================

void FJPEGTexture::Unload ()
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

FTextureFormat FJPEGTexture::GetFormat()
{
	return TEX_RGB;
}

//==========================================================================
//
//
//
//==========================================================================

const BYTE *FJPEGTexture::GetColumn (unsigned int column, const Span **spans_out)
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
		*spans_out = DummySpans;
	}
	return Pixels + column*Height;
}

//==========================================================================
//
//
//
//==========================================================================

const BYTE *FJPEGTexture::GetPixels ()
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

void FJPEGTexture::MakeTexture ()
{
	FWadLump lump = Wads.OpenLumpNum (SourceLump);
	JSAMPLE *buff = NULL;

	jpeg_decompress_struct cinfo;
	jpeg_error_mgr jerr;

	Pixels = new BYTE[Width * Height];
	memset (Pixels, 0xBA, Width * Height);

	cinfo.err = jpeg_std_error(&jerr);
	cinfo.err->output_message = JPEG_OutputMessage;
	cinfo.err->error_exit = JPEG_ErrorExit;
	jpeg_create_decompress(&cinfo);
	try
	{
		FLumpSourceMgr sourcemgr(&lump, &cinfo);
		jpeg_read_header(&cinfo, TRUE);
		if (!((cinfo.out_color_space == JCS_RGB && cinfo.num_components == 3) ||
			  (cinfo.out_color_space == JCS_CMYK && cinfo.num_components == 4) ||
			  (cinfo.out_color_space == JCS_GRAYSCALE && cinfo.num_components == 1)))
		{
			Printf (TEXTCOLOR_ORANGE "Unsupported color format\n");
			throw -1;
		}

		jpeg_start_decompress(&cinfo);

		int y = 0;
		buff = new BYTE[cinfo.output_width * cinfo.output_components];

		while (cinfo.output_scanline < cinfo.output_height)
		{
			int num_scanlines = jpeg_read_scanlines(&cinfo, &buff, 1);
			BYTE *in = buff;
			BYTE *out = Pixels + y;
			switch (cinfo.out_color_space)
			{
			case JCS_RGB:
				for (int x = Width; x > 0; --x)
				{
					*out = RGB32k.RGB[in[0]>>3][in[1]>>3][in[2]>>3];
					out += Height;
					in += 3;
				}
				break;

			case JCS_GRAYSCALE:
				for (int x = Width; x > 0; --x)
				{
					*out = GrayMap[in[0]];
					out += Height;
					in += 1;
				}
				break;

			case JCS_CMYK:
				// What are you doing using a CMYK image? :)
				for (int x = Width; x > 0; --x)
				{
					// To be precise, these calculations should use 255, but
					// 256 is much faster and virtually indistinguishable.
					int r = in[3] - (((256-in[0])*in[3]) >> 8);
					int g = in[3] - (((256-in[1])*in[3]) >> 8);
					int b = in[3] - (((256-in[2])*in[3]) >> 8);
					*out = RGB32k.RGB[r >> 3][g >> 3][b >> 3];
					out += Height;
					in += 4;
				}
				break;

			default:
				// The other colorspaces were considered above and discarded,
				// but GCC will complain without a default for them here.
				break;
			}
			y++;
		}
		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
	}
	catch (int)
	{
		Printf (TEXTCOLOR_ORANGE "   in texture %s\n", Name.GetChars());
		jpeg_destroy_decompress(&cinfo);
	}
	if (buff != NULL)
	{
		delete[] buff;
	}
}


//===========================================================================
//
// FJPEGTexture::CopyTrueColorPixels
//
// Preserves the full color information (unlike software mode)
//
//===========================================================================

int FJPEGTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	PalEntry pe[256];

	FWadLump lump = Wads.OpenLumpNum (SourceLump);
	JSAMPLE *buff = NULL;

	jpeg_decompress_struct cinfo;
	jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	cinfo.err->output_message = JPEG_OutputMessage;
	cinfo.err->error_exit = JPEG_ErrorExit;
	jpeg_create_decompress(&cinfo);

	try
	{
		FLumpSourceMgr sourcemgr(&lump, &cinfo);
		jpeg_read_header(&cinfo, TRUE);

		if (!((cinfo.out_color_space == JCS_RGB && cinfo.num_components == 3) ||
			  (cinfo.out_color_space == JCS_CMYK && cinfo.num_components == 4) ||
			  (cinfo.out_color_space == JCS_GRAYSCALE && cinfo.num_components == 1)))
		{
			Printf (TEXTCOLOR_ORANGE "Unsupported color format\n");
			throw -1;
		}
		jpeg_start_decompress(&cinfo);

		int yc = 0;
		buff = new BYTE[cinfo.output_height * cinfo.output_width * cinfo.output_components];


		while (cinfo.output_scanline < cinfo.output_height)
		{
			BYTE * ptr = buff + cinfo.output_width * cinfo.output_components * yc;
			jpeg_read_scanlines(&cinfo, &ptr, 1);
			yc++;
		}

		switch (cinfo.out_color_space)
		{
		case JCS_RGB:
			bmp->CopyPixelDataRGB(x, y, buff, cinfo.output_width, cinfo.output_height, 
				3, cinfo.output_width * cinfo.output_components, rotate, CF_RGB, inf);
			break;

		case JCS_GRAYSCALE:
			for(int i=0;i<256;i++) pe[i]=PalEntry(255,i,i,i);	// default to a gray map
			bmp->CopyPixelData(x, y, buff, cinfo.output_width, cinfo.output_height, 
				1, cinfo.output_width, rotate, pe, inf);
			break;

		case JCS_CMYK:
			bmp->CopyPixelDataRGB(x, y, buff, cinfo.output_width, cinfo.output_height, 
				4, cinfo.output_width * cinfo.output_components, rotate, CF_CMYK, inf);
			break;

		default:
			assert(0);
			break;
		}
		jpeg_finish_decompress(&cinfo);
	}
	catch(int)
	{
		Printf (TEXTCOLOR_ORANGE "   in JPEG texture %s\n", Name.GetChars());
	}
	jpeg_destroy_decompress(&cinfo);
	if (buff != NULL) delete [] buff;
	return 0;
}


//===========================================================================
//
//
//===========================================================================

bool FJPEGTexture::UseBasePalette() 
{ 
	return false; 
}
