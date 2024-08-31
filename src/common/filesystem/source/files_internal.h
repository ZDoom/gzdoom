#pragma once

#include <memory>
#include "fs_files.h"

namespace FileSys {

class MemoryReader : public FileReaderInterface
{
protected:
	const char * bufptr = nullptr;
	ptrdiff_t FilePos = 0;

	MemoryReader()
	{}

public:
	MemoryReader(const char *buffer, ptrdiff_t length)
	{
		bufptr = buffer;
		Length = length;
		FilePos = 0;
	}

	ptrdiff_t Tell() const override;
	ptrdiff_t Seek(ptrdiff_t offset, int origin) override;
	ptrdiff_t Read(void *buffer, ptrdiff_t len) override;
	char *Gets(char *strbuf, ptrdiff_t len) override;
	virtual const char *GetBuffer() const override { return bufptr; }
};

class BufferingReader : public MemoryReader
{
	FileData buf;
	std::unique_ptr<FileReaderInterface> baseReader;
	ptrdiff_t bufferpos = 0;

	int FillBuffer(ptrdiff_t newpos);
public:
	BufferingReader(FileReaderInterface* base)
		: baseReader(base)
	{
		Length = base->Length;
		buf.allocate(Length);
		bufptr = (const char*)buf.data();
	}

	ptrdiff_t Seek(ptrdiff_t offset, int origin) override;
	ptrdiff_t Read(void* buffer, ptrdiff_t len) override;
	char* Gets(char* strbuf, ptrdiff_t len) override;
};

//==========================================================================
//
// MemoryArrayReader
//
// reads data from an array of memory
//
//==========================================================================

class MemoryArrayReader : public MemoryReader
{
	FileData buf;

public:
	MemoryArrayReader()
	{
		FilePos = 0;
		Length = 0;
	}

	MemoryArrayReader(size_t len)
	{
		buf.allocate(len);
		UpdateBuffer();
	}

	MemoryArrayReader(FileData& buffer)
	{
		buf = std::move(buffer);
		UpdateBuffer();
	}

	void UpdateBuffer()
	{
		bufptr = (const char*)buf.data();
		FilePos = 0;
		Length = buf.size();
	}
};

}
