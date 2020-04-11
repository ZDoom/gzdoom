#pragma once

#include <zmusic.h>
#include "files.h"


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