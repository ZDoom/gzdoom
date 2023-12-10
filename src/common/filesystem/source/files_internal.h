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
	std::vector<uint8_t> buf;
	std::unique_ptr<FileReaderInterface> baseReader;
	ptrdiff_t bufferpos = 0;

	int FillBuffer(ptrdiff_t newpos);
public:
	BufferingReader(FileReaderInterface* base)
		: baseReader(base)
	{
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

template<class T>
class MemoryArrayReader : public MemoryReader
{
	T buf;

public:
	MemoryArrayReader()
	{
		FilePos = 0;
		Length = 0;
	}

	MemoryArrayReader(const char* buffer, ptrdiff_t length)
	{
		if (length > 0)
		{
			buf.resize(length);
			memcpy(&buf[0], buffer, length);
		}
		UpdateBuffer();
	}

	MemoryArrayReader(T& buffer)
	{
		buf = std::move(buffer);
		UpdateBuffer();
	}

	std::vector<uint8_t>& GetArray() { return buf; }

	void UpdateBuffer()
	{
		bufptr = (const char*)buf.data();
		FilePos = 0;
		Length = buf.size();
	}
};

bool OpenMemoryArray(std::vector<uint8_t>& data);	// take the given array


}
