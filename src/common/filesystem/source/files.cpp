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

#include <string>
#include <memory>
#include <algorithm>
#include <assert.h>
#include <string.h>
#include "files_internal.h"

namespace FileSys {
	
#ifdef _WIN32
std::wstring toWide(const char* str);
#endif

FILE *myfopen(const char *filename, const char *flags)
{
#ifndef _WIN32
	return fopen(filename, flags);
#else
	auto widename = toWide(filename);
	auto wideflags = toWide(flags);
	return _wfopen(widename.c_str(), wideflags.c_str());
#endif
}

#ifdef _WIN32
#define fseek _fseeki64
#define ftell _ftelli64
#endif

//==========================================================================
//
// StdFileReader
//
// reads data from an stdio FILE* or part of it.
//
//==========================================================================

class StdFileReader : public FileReaderInterface
{
	FILE *File = nullptr;
	ptrdiff_t StartPos = 0;
	ptrdiff_t FilePos = 0;

public:
	StdFileReader()
	{}

	~StdFileReader()
	{
		if (File != nullptr)
		{
			fclose(File);
		}
		File = nullptr;
	}

	bool Open(const char *filename, ptrdiff_t startpos = 0, ptrdiff_t len = -1)
	{
		File = myfopen(filename, "rb");
		if (File == nullptr) return false;
		FilePos = startpos;
		StartPos = startpos;
		Length = CalcFileLen();
		if (len >= 0 && len < Length) Length = len;
		if (startpos > 0) Seek(0, SEEK_SET);
		return true;
	}

	ptrdiff_t Tell() const override
	{
		return FilePos - StartPos;
	}

	ptrdiff_t Seek(ptrdiff_t offset, int origin) override
	{
		if (origin == SEEK_SET)
		{
			offset += StartPos;
		}
		else if (origin == SEEK_CUR)
		{
			offset += FilePos;
		}
		else if (origin == SEEK_END)
		{
			offset += StartPos + Length;
		}
		if (offset < StartPos || offset > StartPos + Length) return -1;	// out of scope

		if (0 == fseek(File, offset, SEEK_SET))
		{
			FilePos = offset;
			return 0;
		}
		return -1;
	}

	ptrdiff_t Read(void *buffer, ptrdiff_t len) override
	{
		assert(len >= 0);
		if (len <= 0) return 0;
		if (FilePos + len > StartPos + Length)
		{
			len = Length - FilePos + StartPos;
		}
		len = fread(buffer, 1, len, File);
		FilePos += len;
		return len;
	}

	char *Gets(char *strbuf, ptrdiff_t len) override
	{
		if (len <= 0 || len > 0x7fffffff || FilePos >= StartPos + Length) return nullptr;
		char *p = fgets(strbuf, (int)len, File);
		if (p != nullptr)
		{
			ptrdiff_t old = FilePos;
			FilePos = ftell(File);
			if (FilePos - StartPos > Length)
			{
				strbuf[Length - old + StartPos] = 0;
			}
		}
		return p;
	}

private:
	ptrdiff_t CalcFileLen() const
	{
		ptrdiff_t endpos;

		fseek(File, 0, SEEK_END);
		endpos = ftell(File);
		fseek(File, 0, SEEK_SET);
		return endpos;
	}
};

//==========================================================================
//
// FileReaderRedirect
//
// like the above, but uses another File reader as its backing data
//
//==========================================================================

class FileReaderRedirect : public FileReaderInterface
{
	FileReader *mReader = nullptr;
	ptrdiff_t StartPos = 0;
	ptrdiff_t FilePos = 0;

public:
	FileReaderRedirect(FileReader &parent, ptrdiff_t start, ptrdiff_t length)
	{
		mReader = &parent;
		FilePos = start;
		StartPos = start;
		Length = length;
		Seek(0, SEEK_SET);
	}

	virtual ptrdiff_t Tell() const override
	{
		return FilePos - StartPos;
	}

	virtual ptrdiff_t Seek(ptrdiff_t offset, int origin) override
	{
		switch (origin)
		{
		case SEEK_SET:
			offset += StartPos;
			break;

		case SEEK_END:
			offset += StartPos + Length;
			break;

		case SEEK_CUR:
			offset += mReader->Tell();
			break;
		}
		if (offset < StartPos || offset > StartPos + Length) return -1;	// out of scope
		if (mReader->Seek(offset, FileReader::SeekSet) == 0)
		{
			FilePos = offset;
			return 0;
		}
		return -1;
	}

	virtual ptrdiff_t Read(void *buffer, ptrdiff_t len) override
	{
		assert(len >= 0);
		if (len <= 0) return 0;
		if (FilePos + len > StartPos + Length)
		{
			len = Length - FilePos + StartPos;
		}
		len = mReader->Read(buffer, len);
		FilePos += len;
		return len;
	}

	virtual char *Gets(char *strbuf, ptrdiff_t len) override
	{
		if (len <= 0 || FilePos >= StartPos + Length) return nullptr;
		char *p = mReader->Gets(strbuf, len);
		if (p != nullptr)
		{
			ptrdiff_t old = FilePos;
			FilePos = mReader->Tell();
			if (FilePos - StartPos > Length)
			{
				strbuf[Length - old + StartPos] = 0;
			}
		}
		return p;
	}

};

//==========================================================================
//
// MemoryReader
//
// reads data from a block of memory
//
//==========================================================================

ptrdiff_t MemoryReader::Tell() const
{
	return FilePos;
}

ptrdiff_t MemoryReader::Seek(ptrdiff_t offset, int origin)
{
	switch (origin)
	{
	case SEEK_CUR:
		offset += FilePos;
		break;

	case SEEK_END:
		offset += Length;
		break;

	}
	if (offset < 0 || offset > Length) return -1;
	FilePos = std::clamp<ptrdiff_t>(offset, 0, Length);
	return 0;
}

ptrdiff_t MemoryReader::Read(void *buffer, ptrdiff_t len)
{
	if (len > Length - FilePos) len = Length - FilePos;
	if (len<0) len = 0;
	memcpy(buffer, bufptr + FilePos, len);
	FilePos += len;
	return len;
}

char *MemoryReader::Gets(char *strbuf, ptrdiff_t len)
{
	if (len>Length - FilePos) len = Length - FilePos;
	if (len <= 0) return nullptr;

	char *p = strbuf;
	while (len > 1)
	{
		if (bufptr[FilePos] == 0)
		{
			FilePos++;
			break;
		}
		if (bufptr[FilePos] != '\r')
		{
			*p++ = bufptr[FilePos];
			len--;
			if (bufptr[FilePos] == '\n')
			{
				FilePos++;
				break;
			}
		}
		FilePos++;
	}
	if (p == strbuf) return nullptr;
	*p++ = 0;
	return strbuf;
}

int BufferingReader::FillBuffer(ptrdiff_t newpos)
{
	if (newpos > Length) newpos = Length;
	if (newpos <= bufferpos) return 0;
	auto read = baseReader->Read(&buf.writable()[bufferpos], newpos - bufferpos);
	bufferpos += read;
	if (bufferpos == Length)
	{
		// we have read the entire file, so delete our data provider.
		baseReader.reset();
	}
	return newpos == bufferpos ? 0 : -1;
}

ptrdiff_t BufferingReader::Seek(ptrdiff_t offset, int origin)
{
	if (-1 == MemoryReader::Seek(offset, origin)) return -1;
	return FillBuffer(FilePos);
}

ptrdiff_t BufferingReader::Read(void* buffer, ptrdiff_t len)
{
	if (FillBuffer(FilePos + len) < 0) return 0;
	return MemoryReader::Read(buffer, len);
}

char* BufferingReader::Gets(char* strbuf, ptrdiff_t len)
{
	if (FillBuffer(FilePos + len) < 0) return nullptr;
	return MemoryReader::Gets(strbuf, len);
}

//==========================================================================
//
// FileReader
//
// this wraps the different reader types in an object with value semantics.
//
//==========================================================================

bool FileReader::OpenFile(const char *filename, FileReader::Size start, FileReader::Size length, bool buffered)
{
	auto reader = new StdFileReader;
	if (!reader->Open(filename, start, length))
	{
		delete reader;
		return false;
	}
	Close();
	if (buffered) mReader = new BufferingReader(reader);
	else mReader = reader;
	return true;
}

bool FileReader::OpenFilePart(FileReader &parent, FileReader::Size start, FileReader::Size length)
{
	auto reader = new FileReaderRedirect(parent, start, length);
	Close();
	mReader = reader;
	return true;
}

bool FileReader::OpenMemory(const void *mem, FileReader::Size length)
{
	Close();
	mReader = new MemoryReader((const char *)mem, length);
	return true;
}

bool FileReader::OpenMemoryArray(FileData& data)
{
	Close();
	if (data.size() > 0) mReader = new MemoryArrayReader(data);
	return true;
}

FileData FileReader::Read(size_t len)
{
	FileData buffer;
	if (len > 0)
	{
		Size length = mReader->Read(buffer.allocate(len), len);
		if ((size_t)length < len) buffer.allocate(length);
	}
	return buffer;
}

FileData FileReader::ReadPadded(size_t padding)
{
	auto len = GetLength();
	FileData buffer;

	if (len > 0)
	{
		auto p = (char*)buffer.allocate(len + padding);
		Size length = mReader->Read(p, len);
		if (length < len) buffer.clear();
		else memset(p + len, 0, padding);
	}
	return buffer;
}



//==========================================================================
//
// FileWriter (the motivation here is to have a buffer writing subclass)
//
//==========================================================================

bool FileWriter::OpenDirect(const char *filename)
{
	File = myfopen(filename, "wb");
	return (File != nullptr);
}

FileWriter *FileWriter::Open(const char *filename)
{
	FileWriter *fwrit = new FileWriter();
	if (fwrit->OpenDirect(filename))
	{
		return fwrit;
	}
	delete fwrit;
	return nullptr;
}

size_t FileWriter::Write(const void *buffer, size_t len)
{
	if (File != nullptr)
	{
		return fwrite(buffer, 1, len, File);
	}
	else
	{
		return 0;
	}
}

ptrdiff_t FileWriter::Tell()
{
	if (File != nullptr)
	{
		return ftell(File);
	}
	else
	{
		return 0;
	}
}

ptrdiff_t FileWriter::Seek(ptrdiff_t offset, int mode)
{
	if (File != nullptr)
	{
		return fseek(File, offset, mode);
	}
	else
	{
		return 0;
	}
}

size_t FileWriter::Printf(const char *fmt, ...)
{
	char c[300];
	va_list arglist;
	va_start(arglist, fmt);
	auto n = vsnprintf(c, 300, fmt, arglist);
	std::string buf;
	buf.resize(n + 1);
	va_start(arglist, fmt);
	vsnprintf(&buf.front(), n + 1, fmt, arglist);
	va_end(arglist);
	return Write(buf.c_str(), strlen(buf.c_str()));	// Make sure we write no nullptr bytes.
}

size_t BufferWriter::Write(const void *buffer, size_t len)
{
	size_t ofs = mBuffer.size();
	mBuffer.resize(ofs + len);
	memcpy(&mBuffer[ofs], buffer, len);
	return len;
}

}
