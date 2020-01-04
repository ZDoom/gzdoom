#pragma once
#define ZMUSIC_INTERNAL

#define DLL_EXPORT // __declspec(dllexport)
#define DLL_IMPORT
typedef class MIDISource *ZMusic_MidiSource;
typedef class MusInfo *ZMusic_MusicStream;

// Some MIDI backends are not compatible with LGPLv2, so with this #define the support level can be set.
// License can be:
// 0: LGPLv2.  FluidSynth and GUS only.
// 1: GPLv2. Adds Timidity++ and OPL.
// 2: GPLv3. Adds ADL, OPN and WildMidi.
// 

// Default is GPLv3. Most Open Source games can comply with this.
// The notable exceptions are the Build engine games and Descent which are under a no profit clause.
// For these games LGPLv2 is the only viable choice.
#ifndef LICENSE
#define LICENSE 2
#endif

// Devices under a permissive license, or at most LGPLv2+
#define HAVE_GUS
// FluidSynth is a configuration option

#if LICENSE >= 1
#define HAVE_TIMIDITY
#define HAVE_OPL
#endif

#if LICENSE >= 2
// MIDI Devices that cannot be licensed as LGPLv2.
#define HAVE_ADL
#define HAVE_OPN
#define HAVE_WILDMIDI
#endif

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

