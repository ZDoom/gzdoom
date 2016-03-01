/*
** i_music.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __I_MUSIC_H__
#define __I_MUSIC_H__

#include "doomdef.h"

class FileReader;
struct FOptionValues;

//
//	MUSIC I/O
//
void I_InitMusic ();
void I_ShutdownMusic (bool onexit = false);
void I_ShutdownMusicExit ();
void I_BuildMIDIMenuList (FOptionValues *);
void I_UpdateMusic ();

// Volume.
void I_SetMusicVolume (float volume);

// Registers a song handle to song data.
class MusInfo;
struct MidiDeviceSetting;
MusInfo *I_RegisterSong (FileReader *reader, MidiDeviceSetting *device);
MusInfo *I_RegisterCDSong (int track, int cdid = 0);
MusInfo *I_RegisterURLSong (const char *url);

// The base music class. Everything is derived from this --------------------

class MusInfo
{
public:
	MusInfo ();
	virtual ~MusInfo ();
	virtual void MusicVolumeChanged();		// snd_musicvolume changed
	virtual void TimidityVolumeChanged();	// timidity_mastervolume changed
	virtual void Play (bool looping, int subsong) = 0;
	virtual void Pause () = 0;
	virtual void Resume () = 0;
	virtual void Stop () = 0;
	virtual bool IsPlaying () = 0;
	virtual bool IsMIDI () const;
	virtual bool IsValid () const = 0;
	virtual bool SetPosition (unsigned int ms);
	virtual bool SetSubsong (int subsong);
	virtual void Update();
	virtual FString GetStats();
	virtual MusInfo *GetOPLDumper(const char *filename);
	virtual MusInfo *GetWaveDumper(const char *filename, int rate);
	virtual void FluidSettingInt(const char *setting, int value);			// FluidSynth settings
	virtual void FluidSettingNum(const char *setting, double value);		// "
	virtual void FluidSettingStr(const char *setting, const char *value);	// "
	virtual void WildMidiSetOption(int opt, int set);

	void Start(bool loop, float rel_vol = -1.f, int subsong = 0);

	enum EState
	{
		STATE_Stopped,
		STATE_Playing,
		STATE_Paused
	} m_Status;
	bool m_Looping;
	bool m_NotStartedYet;	// Song has been created but not yet played
};
extern int nomusic;

#endif //__I_MUSIC_H__
