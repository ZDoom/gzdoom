#pragma once

#include "fs_files.h"

namespace FileSys {

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

}
