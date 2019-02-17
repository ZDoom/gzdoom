/*
** files.cpp
** Implements classes for reading from files or memory blocks
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** Copyright 2005-2008 Christoph Oelckers
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

// This also pulls in windows.h
#include "LzmaDec.h"
#include <zlib.h>
#include <bzlib.h>

#include "files.h"
#include "doomerrors.h"
#include "templates.h"
#include "m_misc.h"


long DecompressorBase::Tell () const
{
	I_Error("Cannot get position of decompressor stream");
	return 0;
}
long DecompressorBase::Seek (long offset, int origin)
{
	I_Error("Cannot seek in decompressor stream");
	return 0;
}
char *DecompressorBase::Gets(char *strbuf, int len)
{
	I_Error("Cannot use Gets on decompressor stream");
	return nullptr;
}

//==========================================================================
//
// DecompressorZ
//
// The zlib wrapper
// reads data from a ZLib compressed stream
//
//==========================================================================

class DecompressorZ : public DecompressorBase
{
	enum { BUFF_SIZE = 4096 };

	FileReader &File;
	bool SawEOF;
	z_stream Stream;
	uint8_t InBuff[BUFF_SIZE];
	
public:
	DecompressorZ (FileReader &file, bool zip)
	: File(file), SawEOF(false)
	{
		int err;

		FillBuffer ();

		Stream.zalloc = Z_NULL;
		Stream.zfree = Z_NULL;

		if (!zip) err = inflateInit (&Stream);
		else err = inflateInit2 (&Stream, -MAX_WBITS);

		if (err != Z_OK)
		{
			I_Error ("DecompressorZ: inflateInit failed: %s\n", M_ZLibError(err).GetChars());
		}
	}

	~DecompressorZ ()
	{
		inflateEnd (&Stream);
	}

	long Read (void *buffer, long len) override
	{
		int err;

		Stream.next_out = (Bytef *)buffer;
		Stream.avail_out = len;

		do
		{
			err = inflate (&Stream, Z_SYNC_FLUSH);
			if (Stream.avail_in == 0 && !SawEOF)
			{
				FillBuffer ();
			}
		} while (err == Z_OK && Stream.avail_out != 0);

		if (err != Z_OK && err != Z_STREAM_END)
		{
			I_Error ("Corrupt zlib stream");
		}

		if (Stream.avail_out != 0)
		{
			I_Error ("Ran out of data in zlib stream");
		}

		return len - Stream.avail_out;
	}

	void FillBuffer ()
	{
		auto numread = File.Read (InBuff, BUFF_SIZE);

		if (numread < BUFF_SIZE)
		{
			SawEOF = true;
		}
		Stream.next_in = InBuff;
		Stream.avail_in = (uInt)numread;
	}
};


//==========================================================================
//
// DecompressorZ
//
// The bzip2 wrapper
// reads data from a libbzip2 compressed stream
//
//==========================================================================

class DecompressorBZ2 : public DecompressorBase
{
	enum { BUFF_SIZE = 4096 };

	FileReader &File;
	bool SawEOF;
	bz_stream Stream;
	uint8_t InBuff[BUFF_SIZE];
	
public:
	DecompressorBZ2 (FileReader &file)
	: File(file), SawEOF(false)
	{
		int err;

		FillBuffer ();

		Stream.bzalloc = NULL;
		Stream.bzfree = NULL;
		Stream.opaque = NULL;

		err = BZ2_bzDecompressInit(&Stream, 0, 0);

		if (err != BZ_OK)
		{
			I_Error ("DecompressorBZ2: bzDecompressInit failed: %d\n", err);
		}
	}

	~DecompressorBZ2 ()
	{
		BZ2_bzDecompressEnd (&Stream);
	}

	long Read (void *buffer, long len) override
	{
		int err;

		Stream.next_out = (char *)buffer;
		Stream.avail_out = len;

		do
		{
			err = BZ2_bzDecompress(&Stream);
			if (Stream.avail_in == 0 && !SawEOF)
			{
				FillBuffer ();
			}
		} while (err == BZ_OK && Stream.avail_out != 0);

		if (err != BZ_OK && err != BZ_STREAM_END)
		{
			I_Error ("Corrupt bzip2 stream");
		}

		if (Stream.avail_out != 0)
		{
			I_Error ("Ran out of data in bzip2 stream");
		}

		return len - Stream.avail_out;
	}

	void FillBuffer ()
	{
		auto numread = File.Read(InBuff, BUFF_SIZE);

		if (numread < BUFF_SIZE)
		{
			SawEOF = true;
		}
		Stream.next_in = (char *)InBuff;
		Stream.avail_in = (unsigned)numread;
	}

};

//==========================================================================
//
// bz_internal_error
//
// libbzip2 wants this, since we build it with BZ_NO_STDIO set.
//
//==========================================================================

extern "C" void bz_internal_error (int errcode)
{
	I_FatalError("libbzip2: internal error number %d\n", errcode);
}

//==========================================================================
//
// DecompressorLZMA
//
// The lzma wrapper
// reads data from a LZMA compressed stream
//
//==========================================================================

static void *SzAlloc(ISzAllocPtr, size_t size) { return malloc(size); }
static void SzFree(ISzAllocPtr, void *address) { free(address); }
ISzAlloc g_Alloc = { SzAlloc, SzFree };

// Wraps around a Decompressor to decompress a lzma stream
class DecompressorLZMA : public DecompressorBase
{
	enum { BUFF_SIZE = 4096 };

	FileReader &File;
	bool SawEOF;
	CLzmaDec Stream;
	size_t Size;
	size_t InPos, InSize;
	size_t OutProcessed;
	uint8_t InBuff[BUFF_SIZE];

public:

	DecompressorLZMA (FileReader &file, size_t uncompressed_size)
	: File(file), SawEOF(false)
	{
		uint8_t header[4 + LZMA_PROPS_SIZE];
		int err;

		Size = uncompressed_size;
		OutProcessed = 0;

		// Read zip LZMA properties header
		if (File.Read(header, sizeof(header)) < (long)sizeof(header))
		{
			I_Error("DecompressorLZMA: File too short\n");
		}
		if (header[2] + header[3] * 256 != LZMA_PROPS_SIZE)
		{
			I_Error("DecompressorLZMA: LZMA props size is %d (expected %d)\n",
				header[2] + header[3] * 256, LZMA_PROPS_SIZE);
		}

		FillBuffer();

		LzmaDec_Construct(&Stream);
		err = LzmaDec_Allocate(&Stream, header + 4, LZMA_PROPS_SIZE, &g_Alloc);

		if (err != SZ_OK)
		{
			I_Error("DecompressorLZMA: LzmaDec_Allocate failed: %d\n", err);
		}

		LzmaDec_Init(&Stream);
	}

	~DecompressorLZMA ()
	{
		LzmaDec_Free(&Stream, &g_Alloc);
	}

	long Read (void *buffer, long len) override
	{
		int err;
		Byte *next_out = (Byte *)buffer;

		do
		{
			ELzmaFinishMode finish_mode = LZMA_FINISH_ANY;
			ELzmaStatus status;
			size_t out_processed = len;
			size_t in_processed = InSize;

			err = LzmaDec_DecodeToBuf(&Stream, next_out, &out_processed, InBuff + InPos, &in_processed, finish_mode, &status);
			InPos += in_processed;
			InSize -= in_processed;
			next_out += out_processed;
			len = (long)(len - out_processed);
			if (err != SZ_OK)
			{
				I_Error ("Corrupt LZMA stream");
			}
			if (in_processed == 0 && out_processed == 0)
			{
				if (status != LZMA_STATUS_FINISHED_WITH_MARK)
				{
					I_Error ("Corrupt LZMA stream");
				}
			}
			if (InSize == 0 && !SawEOF)
			{
				FillBuffer ();
			}
		} while (err == SZ_OK && len != 0);

		if (err != Z_OK && err != Z_STREAM_END)
		{
			I_Error ("Corrupt LZMA stream");
		}

		if (len != 0)
		{
			I_Error ("Ran out of data in LZMA stream");
		}

		return (long)(next_out - (Byte *)buffer);
	}

	void FillBuffer ()
	{
		auto numread = File.Read(InBuff, BUFF_SIZE);

		if (numread < BUFF_SIZE)
		{
			SawEOF = true;
		}
		InPos = 0;
		InSize = numread;
	}

};

//==========================================================================
//
// Console Doom LZSS wrapper.
//
//==========================================================================

class DecompressorLZSS : public DecompressorBase
{
	enum { BUFF_SIZE = 4096, WINDOW_SIZE = 4096, INTERNAL_BUFFER_SIZE = 128 };

	FileReader &File;
	bool SawEOF;
	uint8_t InBuff[BUFF_SIZE];

	enum StreamState
	{
		STREAM_EMPTY,
		STREAM_BITS,
		STREAM_FLUSH,
		STREAM_FINAL
	};
	struct
	{
		StreamState State;

		uint8_t *In;
		unsigned int AvailIn;
		unsigned int InternalOut;

		uint8_t CFlags, Bits;

		uint8_t Window[WINDOW_SIZE+INTERNAL_BUFFER_SIZE];
		const uint8_t *WindowData;
		uint8_t *InternalBuffer;
	} Stream;

	void FillBuffer()
	{
		if(Stream.AvailIn)
			memmove(InBuff, Stream.In, Stream.AvailIn);

		auto numread = File.Read(InBuff+Stream.AvailIn, BUFF_SIZE-Stream.AvailIn);

		if (numread < BUFF_SIZE)
		{
			SawEOF = true;
		}
		Stream.In = InBuff;
		Stream.AvailIn = (unsigned)numread+Stream.AvailIn;
	}

	// Reads a flag byte.
	void PrepareBlocks()
	{
		assert(Stream.InternalBuffer == Stream.WindowData);
		Stream.CFlags = *Stream.In++;
		--Stream.AvailIn;
		Stream.Bits = 0xFF;
		Stream.State = STREAM_BITS;
	}

	// Reads the next chunk in the block. Returns true if successful and
	// returns false if it ran out of input data.
	bool UncompressBlock()
	{
		if(Stream.CFlags & 1)
		{
			// Check to see if we have enough input
			if(Stream.AvailIn < 2)
				return false;
			Stream.AvailIn -= 2;

			uint16_t pos = BigShort(*(uint16_t*)Stream.In);
			uint8_t len = (pos & 0xF)+1;
			pos >>= 4;
			Stream.In += 2;
			if(len == 1)
			{
				// We've reached the end of the stream.
				Stream.State = STREAM_FINAL;
				return true;
			}

			const uint8_t* copyStart = Stream.InternalBuffer-pos-1;

			// Complete overlap: Single byte repeated
			if(pos == 0)
				memset(Stream.InternalBuffer, *copyStart, len);
			// No overlap: One copy
			else if(pos >= len)
				memcpy(Stream.InternalBuffer, copyStart, len);
			else
			{
				// Partial overlap: Copy in 2 or 3 chunks.
				do
				{
					unsigned int copy = MIN<unsigned int>(len, pos+1);
					memcpy(Stream.InternalBuffer, copyStart, copy);
					Stream.InternalBuffer += copy;
					Stream.InternalOut += copy;
					len -= copy;
					pos += copy; // Increase our position since we can copy twice as much the next round.
				}
				while(len);
			}

			Stream.InternalOut += len;
			Stream.InternalBuffer += len;
		}
		else
		{
			// Uncompressed byte.
			*Stream.InternalBuffer++ = *Stream.In++;
			--Stream.AvailIn;
			++Stream.InternalOut;
		}

		Stream.CFlags >>= 1;
		Stream.Bits >>= 1;

		// If we're done with this block, flush the output
		if(Stream.Bits == 0)
			Stream.State = STREAM_FLUSH;

		return true;
	}

public:
	DecompressorLZSS(FileReader &file) : File(file), SawEOF(false)
	{
		Stream.State = STREAM_EMPTY;
		Stream.WindowData = Stream.InternalBuffer = Stream.Window+WINDOW_SIZE;
		Stream.InternalOut = 0;
		Stream.AvailIn = 0;

		FillBuffer();
	}

	~DecompressorLZSS()
	{
	}

	long Read(void *buffer, long len) override
	{

		uint8_t *Out = (uint8_t*)buffer;
		long AvailOut = len;

		do
		{
			while(Stream.AvailIn)
			{
				if(Stream.State == STREAM_EMPTY)
					PrepareBlocks();
				else if(Stream.State == STREAM_BITS && !UncompressBlock())
					break;
				else
					break;
			}

			unsigned int copy = MIN<unsigned int>(Stream.InternalOut, AvailOut);
			if(copy > 0)
			{
				memcpy(Out, Stream.WindowData, copy);
				Out += copy;
				AvailOut -= copy;

				// Slide our window
				memmove(Stream.Window, Stream.Window+copy, WINDOW_SIZE+INTERNAL_BUFFER_SIZE-copy);
				Stream.InternalBuffer -= copy;
				Stream.InternalOut -= copy;
			}

			if(Stream.State == STREAM_FINAL)
				break;

			if(Stream.InternalOut == 0 && Stream.State == STREAM_FLUSH)
				Stream.State = STREAM_EMPTY;

			if(Stream.AvailIn < 2)
				FillBuffer();
		}
		while(AvailOut && Stream.State != STREAM_FINAL);

		assert(AvailOut == 0);
		return (long)(Out - (uint8_t*)buffer);
	}
};


bool FileReader::OpenDecompressor(FileReader &parent, Size length, int method, bool seekable)
{
	DecompressorBase *dec = nullptr;
	switch (method)
	{
		case METHOD_DEFLATE:
		case METHOD_ZLIB:
			dec = new DecompressorZ(parent, method == METHOD_DEFLATE);
			break;

		case METHOD_BZIP2:
			dec = new DecompressorBZ2(parent);
			break;

		case METHOD_LZMA:
			dec = new DecompressorLZMA(parent, length);
			break;

		case METHOD_LZSS:
			dec = new DecompressorLZSS(parent);
			break;
			
		// todo: METHOD_IMPLODE, METHOD_SHRINK
		default:
			return false;
	}
	dec->Length = (long)length;
	if (!seekable)
	{
		Close();
		mReader = dec;
		return true;
	}
	else
	{
		// todo: create a wrapper. for now this fails
		delete dec;
		return false;
	}
}
