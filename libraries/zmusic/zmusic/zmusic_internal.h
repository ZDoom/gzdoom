#pragma once
#define ZMUSIC_INTERNAL

#define DLL_EXPORT // __declspec(dllexport)
#define DLL_IMPORT
typedef class MIDISource *ZMusic_MidiSource;
typedef class MusInfo *ZMusic_MusicStream;

#include "zmusic.h"

void SetError(const char *text);
