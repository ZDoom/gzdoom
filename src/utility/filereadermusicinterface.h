#pragma once

#include "../libraries/music_common/fileio.h"
#include "zmusic/zmusic.h"
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
	long read(void* buff, int32_t size) override
	{
		if (!fr.isOpen()) return 0;
		return (long)fr.Read(buff, size);
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


inline ZMusicCustomReader *GetMusicReader(FileReader& fr)
{
	auto zcr = new ZMusicCustomReader;

	zcr->handle = fr.GetInterface();
	zcr->gets = [](ZMusicCustomReader* zr, char* buff, int n) { return reinterpret_cast<FileReaderInterface*>(zr->handle)->Gets(buff, n); };
	zcr->read = [](ZMusicCustomReader* zr, void* buff, int32_t size) { return reinterpret_cast<FileReaderInterface*>(zr->handle)->Read(buff, (long)size); };
	zcr->seek = [](ZMusicCustomReader* zr, long offset, int whence) { return reinterpret_cast<FileReaderInterface*>(zr->handle)->Seek(offset, whence); };
	zcr->tell = [](ZMusicCustomReader* zr) { return reinterpret_cast<FileReaderInterface*>(zr->handle)->Tell(); };
	zcr->close = [](ZMusicCustomReader* zr)
	{
		delete reinterpret_cast<FileReaderInterface*>(zr->handle);
		delete zr;
	};
	return zcr;
}