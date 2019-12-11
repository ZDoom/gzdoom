#pragma once

#include "../libraries/music_common/fileio.h"
#include "files.h"

struct FileReaderMusicInterface : public MusicIO::FileInterface
{
	FileReader fr;

	FileReaderMusicInterface(FileReader& fr_in)
	{
		fr = std::move(fr_in);
	}
	char* gets(char* buff, int n) override
	{
		if (!fr.isOpen()) return nullptr;
		return fr.Gets(buff, n);
	}
	long read(void* buff, int32_t size, int32_t nitems) override
	{
		if (!fr.isOpen()) return 0;
		return (long)fr.Read(buff, size * nitems) / size;
	}
	long seek(long offset, int whence) override
	{
		if (!fr.isOpen()) return 0;
		return (long)fr.Seek(offset, (FileReader::ESeek)whence);
	}
	long tell() override
	{
		if (!fr.isOpen()) return 0;
		return (long)fr.Tell();
	}
	FileReader& getReader()
	{
		return fr;
	}
};

