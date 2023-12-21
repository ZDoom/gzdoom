/*
** files.h
** Implements classes for reading from files or memory blocks
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** Copyright 2005-2023 Christoph Oelckers
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
#include <stdarg.h>
#include <string.h>
#include <functional>
#include <vector>
#include "fs_swap.h"

namespace FileSys {
	
class FileSystemException : public std::exception
{
protected:
	static const int MAX_FSERRORTEXT = 1024;
	char m_Message[MAX_FSERRORTEXT];

public:
	FileSystemException(const char* error, ...)
	{
		va_list argptr;
		va_start(argptr, error);
		vsnprintf(m_Message, MAX_FSERRORTEXT, error, argptr);
		va_end(argptr);
	}
	FileSystemException(const char* error, va_list argptr)
	{
		vsnprintf(m_Message, MAX_FSERRORTEXT, error, argptr);
	}
	char const* what() const noexcept override
	{
		return m_Message;
	}
};

class FileReader;

// an opaque memory buffer to the file's content. Can either own the memory or just point to an external buffer.
class FileData
{
	void* memory;
	size_t length;
	bool owned;

public:
	using value_type = uint8_t;
	FileData() { memory = nullptr; length = 0; owned = true; }
	FileData(const void* memory_, size_t len, bool own = true)
	{
		length = len;
		if (own)
		{
			length = len;
			memory = malloc(len);
			owned = true;
			if (memory_) memcpy(memory, memory_, len);
		}
		else
		{
			memory = (void*)memory_;
			owned = false;
		}
	}
	uint8_t* writable() const { return owned? (uint8_t*)memory : nullptr; }
	const void* data() const { return memory; }
	size_t size() const { return length; }
	const char* string() const { return (const char*)memory; }
	const uint8_t* bytes() const { return (const uint8_t*)memory; }

	FileData& operator = (const FileData& copy)
	{
		if (owned && memory) free(memory);
		length = copy.length;
		owned = copy.owned;
		if (owned)
		{
			memory = malloc(length);
			memcpy(memory, copy.memory, length);
		}
		else memory = copy.memory;
		return *this;
	}

	FileData& operator = (FileData&& copy) noexcept
	{
		if (owned && memory) free(memory);
		length = copy.length;
		owned = copy.owned;
		memory = copy.memory;
		copy.memory = nullptr;
		copy.length = 0;
		copy.owned = true;
		return *this;
	}

	FileData(const FileData& copy)
	{
		memory = nullptr;
		*this = copy;
	}

	~FileData()
	{
		if (owned && memory) free(memory);
	}

	void* allocate(size_t len)
	{
		if (!owned) memory = nullptr;
		length = len;
		owned = true;
		memory = realloc(memory, length);
		return memory;
	}

	void set(const void* mem, size_t len)
	{
		memory = (void*)mem;
		length = len;
		owned = false;
	}

	void clear()
	{
		if (owned && memory) free(memory);
		memory = nullptr;
		length = 0;
		owned = true;
	}

};


class FileReaderInterface
{
public:
	ptrdiff_t Length = -1;
	virtual ~FileReaderInterface() {}
	virtual ptrdiff_t Tell () const = 0;
	virtual ptrdiff_t Seek (ptrdiff_t offset, int origin) = 0;
	virtual ptrdiff_t Read (void *buffer, ptrdiff_t len) = 0;
	virtual char *Gets(char *strbuf, ptrdiff_t len) = 0;
	virtual const char *GetBuffer() const { return nullptr; }
	ptrdiff_t GetLength () const { return Length; }
};

class FileReader
{
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

	FileReader(FileReader &&r) noexcept
	{
		mReader = r.mReader;
		r.mReader = nullptr;
	}

	FileReader& operator =(FileReader &&r) noexcept
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

	bool OpenFile(const char *filename, Size start = 0, Size length = -1, bool buffered = false);
	bool OpenFilePart(FileReader &parent, Size start, Size length);
	bool OpenMemory(const void *mem, Size length);	// read directly from the buffer
	bool OpenMemoryArray(FileData& data);	// take the given array

	Size Tell() const
	{
		return mReader->Tell();
	}

	Size Seek(Size offset, ESeek origin)
	{
		return mReader->Seek(offset, origin);
	}

	Size Read(void *buffer, Size len) const
	{
		return mReader->Read(buffer, len);
	}

	FileData Read(size_t len);
	FileData ReadPadded(size_t padding);

	FileData Read()
	{
		return Read(GetLength());
	}


	char *Gets(char *strbuf, Size len)
	{
		return mReader->Gets(strbuf, len);
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
		return byteswap::LittleShort(v);
	}

	int16_t ReadInt16()
	{
		return (int16_t)ReadUInt16();
	}

	int16_t ReadUInt16BE()
	{
		uint16_t v = 0;
		Read(&v, 2);
		return byteswap::BigShort(v);
	}

	int16_t ReadInt16BE()
	{
		return (int16_t)ReadUInt16BE();
	}

	uint32_t ReadUInt32()
	{
		uint32_t v = 0;
		Read(&v, 4);
		return byteswap::LittleLong(v);
	}

	int32_t ReadInt32()
	{
		return (int32_t)ReadUInt32();
	}

	uint32_t ReadUInt32BE()
	{
		uint32_t v = 0;
		Read(&v, 4);
		return byteswap::BigLong(v);
	}

	int32_t ReadInt32BE()
	{
		return (int32_t)ReadUInt32BE();
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
	virtual ptrdiff_t Tell();
	virtual ptrdiff_t Seek(ptrdiff_t offset, int mode);
	size_t Printf(const char *fmt, ...);

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
	std::vector<unsigned char> mBuffer;
public:

	BufferWriter() {}
	virtual size_t Write(const void *buffer, size_t len) override;
	std::vector<unsigned char> *GetBuffer() { return &mBuffer; }
	std::vector<unsigned char>&& TakeBuffer() { return std::move(mBuffer); }
};

}

#endif
