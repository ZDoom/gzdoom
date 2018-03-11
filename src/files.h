/*
** files.h
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

#ifndef FILES_H
#define FILES_H

#include <stdio.h>
#include <zlib.h>
#include <functional>
#include "bzlib.h"
#include "doomtype.h"
#include "m_swap.h"

// Zip compression methods, extended by some internal types to be passed to FileReader::OpenDecompressor
enum
{
	METHOD_STORED = 0,
	METHOD_SHRINK = 1,
	METHOD_IMPLODE = 6,
	METHOD_DEFLATE = 8,
	METHOD_BZIP2 = 12,
	METHOD_LZMA = 14,
	METHOD_PPMD = 98,
	METHOD_LZSS = 1337,	// not used in Zips - this is for Console Doom compression
	METHOD_ZLIB = 1338,	// Zlib stream with header, used by compressed nodes.
};

class FileReaderInterface
{
public:
	long Length = -1;
	virtual ~FileReaderInterface() {}
	virtual long Tell () const = 0;
	virtual long Seek (long offset, int origin) = 0;
	virtual long Read (void *buffer, long len) = 0;
	virtual char *Gets(char *strbuf, int len) = 0;
	virtual const char *GetBuffer() const { return nullptr; }
	long GetLength () const { return Length; }
};

class MemoryReader : public FileReaderInterface
{
protected:
	const char * bufptr = nullptr;
	long FilePos = 0;

	MemoryReader()
	{}

public:
	MemoryReader(const char *buffer, long length)
	{
		bufptr = buffer;
		Length = length;
		FilePos = 0;
	}

	long Tell() const override;
	long Seek(long offset, int origin) override;
	long Read(void *buffer, long len) override;
	char *Gets(char *strbuf, int len) override;
	virtual const char *GetBuffer() const override { return bufptr; }
};



class FileRdr	// this is just a temporary name, until the old FileReader hierarchy can be made private.
{
	FileReaderInterface *mReader = nullptr;

	FileRdr(const FileRdr &r) = delete;
	FileRdr &operator=(const FileRdr &r) = delete;

public:
	enum ESeek
	{
		SeekSet = SEEK_SET,
		SeekCur = SEEK_CUR,
		SeekEnd = SEEK_END
	};

	typedef ptrdiff_t Size;	// let's not use 'long' here.

	FileRdr() {}

	// These two functions are only needed as long as the FileReader has not been fully replaced throughout the code.
	explicit FileRdr(FileReaderInterface *r)
	{
		mReader = r;
	}

	FileRdr(FileRdr &&r)
	{
		mReader = r.mReader;
		r.mReader = nullptr;
	}

	FileRdr& operator =(FileRdr &&r)
	{
		Close();
		mReader = r.mReader;
		r.mReader = nullptr;
		return *this;
	}


	~FileRdr()
	{
		Close();
	}

	bool isOpen() const
	{
		return mReader != nullptr;
	}

	void Close()
	{
		if (mReader != nullptr) delete mReader;
		mReader = nullptr;
	}

	bool OpenFile(const char *filename, Size start = 0, Size length = -1);
	bool OpenFilePart(FileRdr &parent, Size start, Size length);
	bool OpenMemory(const void *mem, Size length);	// read directly from the buffer
	bool OpenMemoryArray(const void *mem, Size length);	// read from a copy of the buffer.
	bool OpenMemoryArray(std::function<bool(TArray<uint8_t>&)> getter);	// read contents to a buffer and return a reader to it
	bool OpenDecompressor(FileRdr &parent, Size length, int method, bool seekable);	// creates a decompressor stream. 'seekable' uses a buffered version so that the Seek and Tell methods can be used.

	Size Tell() const
	{
		return mReader->Tell();
	}

	Size Seek(Size offset, ESeek origin)
	{
		return mReader->Seek((long)offset, origin);
	}

	Size Read(void *buffer, Size len)
	{
		return mReader->Read(buffer, (long)len);
	}

	char *Gets(char *strbuf, Size len)
	{
		return mReader->Gets(strbuf, (int)len);
	}

	const char *GetBuffer()
	{
		return mReader->GetBuffer();
	}

	Size GetLength() const
	{
		return mReader->GetLength();
	}

	// Note: These need to go because they are fundamentally unsafe to use on a binary stream.
	FileRdr &operator>> (uint8_t &v)
	{
		mReader->Read(&v, 1);
		return *this;
	}

	FileRdr &operator>> (int8_t &v)
	{
		mReader->Read(&v, 1);
		return *this;
	}

	FileRdr &operator>> (uint16_t &v)
	{
		mReader->Read(&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileRdr &operator>> (int16_t &v)
	{
		mReader->Read(&v, 2);
		v = LittleShort(v);
		return *this;
	}

	FileRdr &operator>> (uint32_t &v)
	{
		mReader->Read(&v, 4);
		v = LittleLong(v);
		return *this;
	}

	FileRdr &operator>> (int32_t &v)
	{
		mReader->Read(&v, 4);
		v = LittleLong(v);
		return *this;
	}

	friend class FWadCollection;
};




class FileWriter
{
protected:
	bool OpenDirect(const char *filename);

	FileWriter()
	{
		File = NULL;
	}
public:
	virtual ~FileWriter()
	{
		if (File != NULL) fclose(File);
	}

	static FileWriter *Open(const char *filename);

	virtual size_t Write(const void *buffer, size_t len);
	virtual long Tell();
	virtual long Seek(long offset, int mode);
	size_t Printf(const char *fmt, ...) GCCPRINTF(2,3);

protected:

	FILE *File;

protected:
	bool CloseOnDestruct;
};

class BufferWriter : public FileWriter
{
protected:
	TArray<unsigned char> mBuffer;
public:

	BufferWriter() {}
	virtual size_t Write(const void *buffer, size_t len) override;
	TArray<unsigned char> *GetBuffer() { return &mBuffer; }
};

#endif
