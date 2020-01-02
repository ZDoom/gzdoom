#pragma once
#define ZMUSIC_INTERNAL

#define DLL_EXPORT // __declspec(dllexport)
#define DLL_IMPORT
typedef class MIDISource *ZMusic_MidiSource;
typedef class MusInfo *ZMusic_MusicStream;

#include "zmusic.h"
#include "../../music_common/fileio.h"

void SetError(const char *text);

struct CustomFileReader : public MusicIO::FileInterface
{
	ZMusicCustomReader* cr;

	CustomFileReader(ZMusicCustomReader* zr) : cr(zr) {}
	virtual char* gets(char* buff, int n) { return cr->gets(cr, buff, n); }
	virtual long read(void* buff, int32_t size) { return cr->read(cr, buff, size); }
	virtual long seek(long offset, int whence) { return cr->seek(cr, offset, whence); }
	virtual long tell() { return cr->tell(cr); }
	virtual void close()
	{
		cr->close(cr);
		delete this;
	}

};

