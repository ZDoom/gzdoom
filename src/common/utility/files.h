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

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <functional>
#include "basics.h"
#include "m_swap.h"
#include "tarray.h"

// Zip compression methods, extended by some internal types to be passed to OpenDecompressor
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
	METHOD_TRANSFEROWNER = 0x8000,
};

class FileReader;

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


struct FResourceLump;

class FileReader
{
	friend struct FResourceLump;	// needs access to the private constructor.

	FileReaderInterface *mReader = nullptr;

	FileReader(const FileReader &r) = delete;
	FileReader &operator=(const FileReader &r) = delete;

public:

	explicit FileReader(FileReaderInterface *r)
	{
		mReader = r;
	}

	enum ESeek
	{
		SeekSet = SEEK_SET,
		SeekCur = SEEK_CUR,
		SeekEnd = SEEK_END
	};

	typedef ptrdiff_t Size;	// let's not use 'long' here.

	FileReader() {}

	FileReader(FileReader &&r)
	{
		mReader = r.mReader;
		r.mReader = nullptr;
	}

	FileReader& operator =(FileReader &&r)
	{
		Close();
		mReader = r.mReader;
		r.mReader = nullptr;
		return *this;
	}

	// This is for wrapping the actual reader for custom access where a managed FileReader won't work. 
	FileReaderInterface* GetInterface()
	{
		auto i = mReader;
		mReader = nullptr;
		return i;
	}


	~FileReader()
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
	bool OpenFilePart(FileReader &parent, Size start, Size length);
	bool OpenMemory(const void *mem, Size length);	// read directly from the buffer
	bool OpenMemoryArray(const void *mem, Size length);	// read from a copy of the buffer.
	bool OpenMemoryArray(std::function<bool(TArray<uint8_t>&)> getter);	// read contents to a buffer and return a reader to it
	bool OpenDecompressor(FileReader &parent, Size length, int method, bool seekable, const std::function<void(const char*)>& cb);	// creates a decompressor stream. 'seekable' uses a buffered version so that the Seek and Tell methods can be used.

	Size Tell() const
	{
		return mReader->Tell();
	}

	Size Seek(Size offset, ESeek origin)
	{
		return mReader->Seek((long)offset, origin);
	}

	Size Read(void *buffer, Size len) const
	{
		return mReader->Read(buffer, (long)len);
	}

	TArray<uint8_t> Read(size_t len)
	{
		TArray<uint8_t> buffer((int)len, true);
		Size length = mReader->Read(&buffer[0], (long)len);
		buffer.Clamp((int)length);
		return buffer;
	}

	TArray<uint8_t> Read()
	{
		TArray<uint8_t> buffer(mReader->Length, true);
		Size length = mReader->Read(&buffer[0], mReader->Length);
		if (length < mReader->Length) buffer.Clear();
		return buffer;
	}

	TArray<uint8_t> ReadPadded(int padding)
	{
		TArray<uint8_t> buffer(mReader->Length + padding, true);
		Size length = mReader->Read(&buffer[0], mReader->Length);
		if (length < mReader->Length) buffer.Clear();
		else memset(buffer.Data() + mReader->Length, 0, padding);
		return buffer;
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

	uint8_t ReadUInt8()
	{
		uint8_t v = 0;
		Read(&v, 1);
		return v;
	}

	int8_t ReadInt8()
	{
		int8_t v = 0;
		Read(&v, 1);
		return v;
	}

	uint16_t ReadUInt16()
	{
		uint16_t v = 0;
		Read(&v, 2);
		return LittleShort(v);
	}

	int16_t ReadInt16()
	{
		int16_t v = 0;
		Read(&v, 2);
		return LittleShort(v);
	}

	int16_t ReadInt16BE()
	{
		int16_t v = 0;
		Read(&v, 2);
		return BigShort(v);
	}

	uint32_t ReadUInt32()
	{
		uint32_t v = 0;
		Read(&v, 4);
		return LittleLong(v);
	}

	int32_t ReadInt32()
	{
		int32_t v = 0;
		Read(&v, 4);
		return LittleLong(v);
	}

	uint32_t ReadUInt32BE()
	{
		uint32_t v = 0;
		Read(&v, 4);
		return BigLong(v);
	}

	int32_t ReadInt32BE()
	{
		int32_t v = 0;
		Read(&v, 4);
		return BigLong(v);
	}

	uint64_t ReadUInt64()
	{
		uint64_t v = 0;
		Read(&v, 8);
		// Prove to me that there's a relevant 64 bit Big Endian architecture and I fix this! :P
		return v;
	}


	friend class FileSystem;
};

class DecompressorBase : public FileReaderInterface
{
	std::function<void(const char*)> ErrorCallback = nullptr;
public:
	// These do not work but need to be defined to satisfy the FileReaderInterface.
	// They will just error out when called.
	long Tell() const override;
	long Seek(long offset, int origin) override;
	char* Gets(char* strbuf, int len) override;
	void DecompressionError(const char* error, ...) const;
	void SetErrorCallback(const std::function<void(const char*)>& cb)
	{
		ErrorCallback = cb;
	}
	void SetOwnsReader();

protected:
	FileReader* File = nullptr;
	FileReader OwnedFile;
};


class FileWriter
{
protected:
	bool OpenDirect(const char *filename);

public:
	FileWriter(FILE *f = nullptr) // if passed, this writer will take over the file.
	{
		File = f;
	}
	virtual ~FileWriter()
	{
		Close();
	}

	static FileWriter *Open(const char *filename);

	virtual size_t Write(const void *buffer, size_t len);
	virtual long Tell();
	virtual long Seek(long offset, int mode);
	size_t Printf(const char *fmt, ...) GCCPRINTF(2,3);
	virtual void Close()
	{
		if (File != NULL) fclose(File);
		File = nullptr;
	}

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
	TArray<unsigned char>&& TakeBuffer() { return std::move(mBuffer); }
};


#endif
