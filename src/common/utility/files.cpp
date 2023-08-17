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

#include "files.h"
#include "utf8.h"
#include "stb_sprintf.h"


FILE *myfopen(const char *filename, const char *flags)
{
#ifndef _WIN32
	return fopen(filename, flags);
#else
	auto widename = WideString(filename);
	auto wideflags = WideString(flags);
	return _wfopen(widename.c_str(), wideflags.c_str());
#endif
}


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
	long StartPos = 0;
	long FilePos = 0;

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

	bool Open(const char *filename, long startpos = 0, long len = -1)
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

	long Tell() const override
	{
		return FilePos - StartPos;
	}

	long Seek(long offset, int origin) override
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

	long Read(void *buffer, long len) override
	{
		assert(len >= 0);
		if (len <= 0) return 0;
		if (FilePos + len > StartPos + Length)
		{
			len = Length - FilePos + StartPos;
		}
		len = (long)fread(buffer, 1, len, File);
		FilePos += len;
		return len;
	}

	char *Gets(char *strbuf, int len) override
	{
		if (len <= 0 || FilePos >= StartPos + Length) return NULL;
		char *p = fgets(strbuf, len, File);
		if (p != NULL)
		{
			int old = FilePos;
			FilePos = ftell(File);
			if (FilePos - StartPos > Length)
			{
				strbuf[Length - old + StartPos] = 0;
			}
		}
		return p;
	}

private:
	long CalcFileLen() const
	{
		long endpos;

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
	long StartPos = 0;
	long FilePos = 0;

public:
	FileReaderRedirect(FileReader &parent, long start, long length)
	{
		mReader = &parent;
		FilePos = start;
		StartPos = start;
		Length = length;
		Seek(0, SEEK_SET);
	}

	virtual long Tell() const override
	{
		return FilePos - StartPos;
	}

	virtual long Seek(long offset, int origin) override
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
			offset += (long)mReader->Tell();
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

	virtual long Read(void *buffer, long len) override
	{
		assert(len >= 0);
		if (len <= 0) return 0;
		if (FilePos + len > StartPos + Length)
		{
			len = Length - FilePos + StartPos;
		}
		len = (long)mReader->Read(buffer, len);
		FilePos += len;
		return len;
	}

	virtual char *Gets(char *strbuf, int len) override
	{
		if (len <= 0 || FilePos >= StartPos + Length) return NULL;
		char *p = mReader->Gets(strbuf, len);
		if (p != NULL)
		{
			int old = FilePos;
			FilePos = (long)mReader->Tell();
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

long MemoryReader::Tell() const
{
	return FilePos;
}

long MemoryReader::Seek(long offset, int origin)
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
	FilePos = clamp<long>(offset, 0, Length);
	return 0;
}

long MemoryReader::Read(void *buffer, long len)
{
	if (len>Length - FilePos) len = Length - FilePos;
	if (len<0) len = 0;
	memcpy(buffer, bufptr + FilePos, len);
	FilePos += len;
	return len;
}

char *MemoryReader::Gets(char *strbuf, int len)
{
	if (len>Length - FilePos) len = Length - FilePos;
	if (len <= 0) return NULL;

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
	if (p == strbuf) return NULL;
	*p++ = 0;
	return strbuf;
}

//==========================================================================
//
// MemoryArrayReader
//
// reads data from an array of memory
//
//==========================================================================

class MemoryArrayReader : public MemoryReader
{
	TArray<uint8_t> buf;

public:
	MemoryArrayReader(const char *buffer, long length)
	{
		if (length > 0)
		{
			buf.Resize(length);
			memcpy(&buf[0], buffer, length);
		}
		UpdateBuffer();
	}

	TArray<uint8_t> &GetArray() { return buf; }

	void UpdateBuffer() 
	{ 
		bufptr = (const char*)&buf[0];
		FilePos = 0;
		Length = buf.Size();
	}
};



//==========================================================================
//
// FileReader
//
// this wraps the different reader types in an object with value semantics.
//
//==========================================================================

bool FileReader::OpenFile(const char *filename, FileReader::Size start, FileReader::Size length)
{
	auto reader = new StdFileReader;
	if (!reader->Open(filename, (long)start, (long)length))
	{
		delete reader;
		return false;
	}
	Close();
	mReader = reader;
	return true;
}

bool FileReader::OpenFilePart(FileReader &parent, FileReader::Size start, FileReader::Size length)
{
	auto reader = new FileReaderRedirect(parent, (long)start, (long)length);
	Close();
	mReader = reader;
	return true;
}

bool FileReader::OpenMemory(const void *mem, FileReader::Size length)
{
	Close();
	mReader = new MemoryReader((const char *)mem, (long)length);
	return true;
}

bool FileReader::OpenMemoryArray(const void *mem, FileReader::Size length)
{
	Close();
	mReader = new MemoryArrayReader((const char *)mem, (long)length);
	return true;
}

bool FileReader::OpenMemoryArray(std::function<bool(TArray<uint8_t>&)> getter)
{
	auto reader = new MemoryArrayReader(nullptr, 0);
	if (getter(reader->GetArray()))
	{
		Close();
		reader->UpdateBuffer();
		mReader = reader;
		return true;
	}
	else
	{
		// This will keep the old buffer, if one existed
		delete reader;
		return false;
	}
}


//==========================================================================
//
// FileWriter (the motivation here is to have a buffer writing subclass)
//
//==========================================================================

bool FileWriter::OpenDirect(const char *filename)
{
	File = myfopen(filename, "wb");
	return (File != NULL);
}

FileWriter *FileWriter::Open(const char *filename)
{
	FileWriter *fwrit = new FileWriter();
	if (fwrit->OpenDirect(filename))
	{
		return fwrit;
	}
	delete fwrit;
	return NULL;
}

size_t FileWriter::Write(const void *buffer, size_t len)
{
	if (File != NULL)
	{
		return fwrite(buffer, 1, len, File);
	}
	else
	{
		return 0;
	}
}

long FileWriter::Tell()
{
	if (File != NULL)
	{
		return ftell(File);
	}
	else
	{
		return 0;
	}
}

long FileWriter::Seek(long offset, int mode)
{
	if (File != NULL)
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
	char workbuf[STB_SPRINTF_MIN];
	va_list arglist;
	va_start(arglist, fmt);
	auto r = stbsp_vsprintfcb([](const char* cstr, void* data, int len) -> char*
		{
			auto fr = (FileWriter*)data;
			auto writ = fr->Write(cstr, len);
			return writ == (size_t)len? (char*)cstr : nullptr; // abort if writing caused an error.
		}, this, workbuf, fmt, arglist);

	va_end(arglist);
	return r;
}

size_t BufferWriter::Write(const void *buffer, size_t len)
{
	unsigned int ofs = mBuffer.Reserve((unsigned)len);
	memcpy(&mBuffer[ofs], buffer, len);
	return len;
}
