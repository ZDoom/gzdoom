#pragma once
#define ZMUSIC_INTERNAL

#if defined(_MSC_VER) && !defined(ZMUSIC_STATIC)
#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT __declspec(dllexport)	// without this the compiler complains.
#else
#define DLL_EXPORT
#define DLL_IMPORT
#endif

typedef class MIDISource *ZMusic_MidiSource;
typedef class MusInfo *ZMusic_MusicStream;

// Build two configurations - lite and full.
// Lite only  uses FluidSynth for MIDI playback and is licensed under the LGPL v2.1
// Full uses all MIDI synths and is licensed under the GPL v3.

#ifndef ZMUSIC_LITE
#define HAVE_GUS		// legally viable but not really useful
#define HAVE_TIMIDITY	// GPL v2.0
#define HAVE_OPL		// GPL v3.0
#define HAVE_ADL		// GPL v3.0
#define HAVE_OPN		// GPL v3.0
#define HAVE_WILDMIDI	// LGPL v3.0
#endif

#include "zmusic.h"
#include "fileio.h"

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


void ZMusic_Printf(int type, const char* msg, ...);

inline uint8_t ZMusic_SampleTypeSize(SampleType stype)
{
    switch(stype)
    {
    case SampleType_UInt8: return sizeof(uint8_t);
    case SampleType_Int16: return sizeof(int16_t);
    case SampleType_Float32: return sizeof(float);
    }
    return 0;
}

inline uint8_t ZMusic_ChannelCount(ChannelConfig chans)
{
    switch(chans)
    {
    case ChannelConfig_Mono: return 1;
    case ChannelConfig_Stereo: return 2;
    }
    return 0;
}

inline const char *ZMusic_ChannelConfigName(ChannelConfig chans)
{
    switch(chans)
    {
    case ChannelConfig_Mono: return "Mono";
    case ChannelConfig_Stereo: return "Stereo";
    }
    return "(unknown)";
}
