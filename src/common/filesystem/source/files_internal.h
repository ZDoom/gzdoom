#pragma once

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

}
