/*
** i_music.cpp
** Plays music
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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
** I appologize for the mess that this file has become. It works, but it's
** not pretty. The SPC, Timidity, and OPL playback routines are also too
** FMOD-specific. I should have made some generic streaming class for them
** to utilize so that they can work with other sound systems, too.
*/

#ifndef NOSPC
#ifndef _WIN32
#define NOSPC
#endif
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "mid2strm.h"
#include "mus2strm.h"
#else
#include <SDL/SDL.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <wordexp.h>
#include <stdio.h>
#include "mus2midi.h"
#define FALSE 0
#define TRUE 1
#endif

#include <ctype.h>
#include <assert.h>
#include <stdio.h>

#include "doomtype.h"
#include "m_argv.h"
#include "i_music.h"
#include "w_wad.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "z_zone.h"
#include "i_system.h"
#include "i_sound.h"
#include "m_swap.h"
#include "i_cd.h"
#include "tempfiles.h"
#include "templates.h"
#include "oplsynth/opl_mus_player.h"

#include <fmod.h>

EXTERN_CVAR (Int, snd_samplerate)

void Enable_FSOUND_IO_Loader ();
void Disable_FSOUND_IO_Loader ();

static bool MusicDown = true;

extern bool nofmod;

class MusInfo
{
public:
	MusInfo () : m_Status(STATE_Stopped) {}
	virtual ~MusInfo () {}
	virtual void SetVolume (float volume) = 0;
	virtual void Play (bool looping) = 0;
	virtual void Pause () = 0;
	virtual void Resume () = 0;
	virtual void Stop () = 0;
	virtual bool IsPlaying () = 0;
	virtual bool IsMIDI () const = 0;
	virtual bool IsValid () const = 0;
	virtual bool SetPosition (int order) { return false; }

	enum EState
	{
		STATE_Stopped,
		STATE_Playing,
		STATE_Paused
	} m_Status;
	bool m_Looping;
};

static MusInfo *currSong;
static int		nomusic = 0;

//==========================================================================
//
// CVAR snd_musicvolume
//
// Maximum volume of MOD/stream music.
//==========================================================================

CUSTOM_CVAR (Float, snd_musicvolume, 0.3f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 1.f)
		self = 1.f;
	else if (currSong != NULL && !currSong->IsMIDI ())
		currSong->SetVolume (self);
}

#ifdef _WIN32
static HANDLE	BufferReturnEvent;
static DWORD	midivolume;
static DWORD	nummididevices;
static bool		nummididevicesset;
static UINT		mididevice;

//==========================================================================
//
// CVAR snd_midivolume
//
// Maximum volume of MIDI/MUS music through the MM subsystem.
//==========================================================================

CUSTOM_CVAR (Float, snd_midivolume, 0.5f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 1.f)
		self = 1.f;
	else
	{
		DWORD onechanvol = (DWORD)(self * 65535.f);
		midivolume = (onechanvol << 16) | onechanvol;
		if (currSong && currSong->IsMIDI ())
		{
			currSong->SetVolume (self);
		}
	}
}

CVAR (Bool, snd_midiprecache, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
#else
int ChildQuit;
#endif

#ifdef _WIN32
// MIDI file played through the multimedia system
class MIDISong : public MusInfo
{
public:
	MIDISong (int handle, int pos, int len);
	~MIDISong ();
	void SetVolume (float volume);
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return true; }
	bool IsValid () const { return m_Buffers != NULL; }
	void MidiProc (UINT uMsg);

protected:
	MIDISong ();
	void MCIError (MMRESULT res, const char *descr);
	void UnprepareHeaders ();
	bool PrepareHeaders ();
	void SubmitBuffer ();
	void AllChannelsOff ();
	void SetStreamVolume ();
	void UnsetStreamVolume ();

	bool m_IsMUS;

	enum
	{
		cb_play,
		cb_die,
		cb_dead
	} m_CallbackStatus;
	HMIDISTRM m_MidiStream;
	PSTREAMBUF m_Buffers;
	PSTREAMBUF m_CurrBuffer;
	bool m_bVolGood, m_bOldVolGood;

	DWORD m_OldVolume, m_LastSetVol;
};

// MUS file played through the multimedia system
class MUSSong : public MIDISong
{
public:
	MUSSong (int handle, int pos, int len);
};
#endif // _WIN32

// MOD file played with FMOD
class MODSong : public MusInfo
{
public:
	MODSong (int handle, int pos, int len);
	~MODSong ();
	void SetVolume (float volume);
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return false; }
	bool IsValid () const { return m_Module != NULL; }
	bool SetPosition (int order);

protected:
	MODSong () {}

	FMUSIC_MODULE *m_Module;
};

#ifdef _WIN32
// MIDI song played with DirectMusic by FMOD
class DMusSong : public MODSong
{
public:
	DMusSong (int handle, int pos, int len);

protected:
	FTempFileName DiskName;
};
#endif // _WIN32

// OGG/MP3/WAV or some other format streamed through FMOD
class StreamSong : public MusInfo
{
public:
	StreamSong (int handle, int pos, int len);
	~StreamSong ();
	void SetVolume (float volume);
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return false; }
	bool IsValid () const { return m_Stream != NULL; }

protected:
	StreamSong () : m_Stream (NULL), m_Channel (-1) {}

	FSOUND_STREAM *m_Stream;
	int m_Channel;
	int m_LastPos;
};

#ifndef NOSPC
// SPC file

typedef void (__stdcall *SNESAPUInfo_TYPE) (DWORD&, DWORD&, DWORD&);
typedef void (__stdcall *GetAPUData_TYPE) (void**, BYTE**, BYTE**, DWORD**, void**, void**, DWORD**, DWORD**);
typedef void (__stdcall *ResetAPU_TYPE) (DWORD);
typedef void (__stdcall *FixAPU_TYPE) (WORD, BYTE, BYTE, BYTE, BYTE, BYTE);
typedef void (__stdcall *SetAPUOpt_TYPE) (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef void *(__stdcall *EmuAPU_TYPE) (void *, DWORD, BYTE);

class SPCSong : public StreamSong
{
public:
	SPCSong (int handle, int pos, int len);
	~SPCSong ();
	void Play (bool looping);
	bool IsPlaying ();
	bool IsValid () const;

protected:
	bool LoadEmu ();
	void CloseEmu ();

	static signed char STACK_ARGS FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);

	HINSTANCE HandleAPU;

	SNESAPUInfo_TYPE SNESAPUInfo;
	GetAPUData_TYPE GetAPUData;
	ResetAPU_TYPE ResetAPU;
	FixAPU_TYPE FixAPU;
	SetAPUOpt_TYPE SetAPUOpt;
	EmuAPU_TYPE EmuAPU;
};
#endif

// MIDI file played with Timidity
class TimiditySong : public StreamSong
{
public:
	TimiditySong (int handle, int pos, int len);
	~TimiditySong ();
	void Play (bool looping);
	void Stop ();
	bool IsPlaying ();
	bool IsValid () const { return CommandLine != NULL; }

protected:
	void PrepTimidity ();
	bool LaunchTimidity ();

	FTempFileName DiskName;
#ifdef _WIN32
	HANDLE ReadWavePipe;
	HANDLE WriteWavePipe;
	HANDLE KillerEvent;
	HANDLE ChildProcess;
	bool Validated;
	bool ValidateTimidity ();
#else // _WIN32
	int WavePipe[2];
	pid_t ChildProcess;
#endif
	char *CommandLine;
	int LoopPos;

	static signed char STACK_ARGS FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);
#ifdef _WIN32
	static const char EventName[];
#endif
};

#ifdef _WIN32
const char TimiditySong::EventName[] = "TiMidity Killer";

CVAR (String, timidity_exe, "timidity.exe", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
#else
CVAR (String, timidity_exe, "timidity", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
#endif
CVAR (String, timidity_extargs, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// extra args to pass to Timidity
CVAR (String, timidity_chorus, "0", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, timidity_reverb, "0", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, timidity_stereo, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, timidity_8bit, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, timidity_byteswap, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (Int, timidity_pipe, 60, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // pipe size in ms
	if (timidity_pipe < 0)
	{ // a negative size makes no sense
		timidity_pipe = 0;
	}
}

CUSTOM_CVAR (Int, timidity_frequency, 22050, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // Clamp frequency to Timidity's limits
	if (self < 4000)
		self = 4000;
	else if (self > 65000)
		self = 65000;
}

// MUS file played using a software OPL2 synth
class OPLMUSSong : public StreamSong
{
public:
	OPLMUSSong (int handle, int pos, int len);
	~OPLMUSSong ();
	void Play (bool looping);
	bool IsPlaying ();
	bool IsValid () const;
	void ResetChips ();

protected:
	static signed char STACK_ARGS FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);

	OPLmusicBlock *Music;
};

CUSTOM_CVAR (Int, opl_frequency, 11025, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // Clamp frequency to FMOD's limits
	if (self < 4000)
		self = 4000;
	else if (self > 65535)
		self = 65535;
}

CVAR (Bool, opl_enable, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

static bool OPL_Active;

CUSTOM_CVAR (Bool, opl_onechip, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (OPL_Active && currSong != NULL)
	{
		static_cast<OPLMUSSong *>(currSong)->ResetChips ();
	}
}


// CD track/disk played through the multimedia system
class CDSong : public MusInfo
{
public:
	CDSong (int track, int id);
	~CDSong ();
	void SetVolume (float volume) {}
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return false; }
	bool IsValid () const { return m_Inited; }

protected:
	CDSong () : m_Inited(false) {}

	int m_Track;
	bool m_Inited;
};

// CD track on a specific disk played through the multimedia system
class CDDAFile : public CDSong
{
public:
	CDDAFile (int handle, int pos, int len);
};

#if !defined(_WIN32) && 0
// Under Linux, buffer output from Timidity to try to avoid "bubbles"
// in the sound output.
class FPipeBuffer
{
public:
	FPipeBuffer::FPipeBuffer (int fragSize, int nFrags, int pipe);
	~FPipeBuffer ();

	int ReadFrag (BYTE *Buf);

private:
	int PipeHandle;
	int FragSize;
	int BuffSize;
	int WritePos, ReadPos;
	BYTE *Buffer;
	bool GotFull;
	SDL_mutex *BufferMutex;
	SDL_thread *Reader;

	static int ThreadProc (void *data);
};
#endif

#ifdef _WIN32
CUSTOM_CVAR (Int, snd_mididevice, -1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	UINT oldmididev = mididevice;

	if (!nummididevicesset)
		return;

	if ((self >= (signed)nummididevices) || (self < -3))
	{
		Printf ("ID out of range. Using MIDI mapper.\n");
		self = -1;
		return;
	}
	else if (self < 0)
	{
		mididevice = MIDI_MAPPER;
	}
	else
	{
		mididevice = self;
	}

	// If a song is playing, move it to the new device.
	if (oldmididev != mididevice && currSong)
	{
		MusInfo *song = currSong;
		I_StopSong (song);
		I_PlaySong (song, song->m_Looping);
	}
}

void MIDISong::SetVolume (float volume)
{
	SetStreamVolume ();
}

// Hacks for stupid Creative drivers that won't let me simply call
// midiOutSetVolume when I want to affect the volume for a single stream.

void MIDISong::SetStreamVolume ()
{
	DWORD vol;
	MMRESULT ret;

#if 0 // Damn you, Creative! I can't send midiOutLongMsg to MIDI streams
	  // like the docs say because your drivers suck! It looks like I will
	  // have to write my own sequencer after all just so I can get volume
	  // changes working with Creative's cards. A MUS sequencer is easy.
	  // A MIDI file sequencer is a bit more complicated, so I want to delay
	  // that endeavor for another day.

	// Send a master volume SysEx message to the MIDI device.
	BYTE volmess[] = { 0xf0, 0x7f, 0x7f, 0x04, 0x01,
		midivolume & 0x7f, (midivolume>>7) & 0x7f, 0xf7 };
	MIDIHDR hdr = { (LPSTR)volmess, sizeof(volmess), };

	ret = midiOutPrepareHeader ((HMIDIOUT)m_MidiStream, &hdr, sizeof(hdr));
	if (ret == MMSYSERR_NOERROR)
	{
		midiStreamPause (m_MidiStream);
		ret = midiOutLongMsg ((HMIDIOUT)m_MidiStream, &hdr, sizeof(hdr));
		if (ret != MMSYSERR_NOERROR)
		{
			Printf ("long msg says: %08x\n", ret);
		}
		while (MIDIERR_STILLPLAYING == (ret = midiOutUnprepareHeader ((HMIDIOUT)m_MidiStream, &hdr, sizeof(hdr))))
		{
			Printf ("still playing\n");
			Sleep (10);
		}
		midiStreamRestart (m_MidiStream);
		if (ret != MMSYSERR_NOERROR)
		{
			Printf ("unprep says: %08x\n", ret);
		}
	}
	else
	{
		Printf ("prep says: %08x\n", ret);
	}

#elif 1
	ret = midiOutGetVolume ((HMIDIOUT)mididevice, &vol);
	if (m_bVolGood)
	{
		// Check the last set vol against the current volume.
		// If they're different, then the the current volume
		// becomes the volume to set when we're done.
		if (ret == MMSYSERR_NOERROR)
		{
			if (vol != m_LastSetVol)
			{
				m_OldVolume = vol;
				m_bOldVolGood = true;
			}
		}
		else
		{
			m_bOldVolGood = false;
		}
	}
	else
	{
		// The volume hasn't been set yet, so remember the
		// current volume so that we can return to it later.
		if (ret == MMSYSERR_NOERROR)
		{
			m_OldVolume = vol;
			m_bOldVolGood = true;
		}
		else
		{
			m_bOldVolGood = false;
		}
	}
	// Now set the new volume.
	ret = midiOutSetVolume ((HMIDIOUT)mididevice, midivolume);
	if (ret == MMSYSERR_NOERROR)
	{
		m_LastSetVol = midivolume;
		m_bVolGood = true;
	}
	else
	{
		char err[MAXERRORLENGTH];
		midiOutGetErrorText (ret, err, MAXERRORLENGTH);
		Printf (PRINT_BOLD, "Could not set MIDI volume:\n%s\n", err);
		m_bVolGood = false;
	}
#endif
}

void MIDISong::UnsetStreamVolume ()
{
	if (m_bVolGood && m_bOldVolGood)
	{
		midiOutSetVolume ((HMIDIOUT)mididevice, m_OldVolume);
		m_bVolGood = false;
		m_bOldVolGood = false;
	}
}
#endif // _WIN32

#ifndef _WIN32
void ChildSigHandler (int signum)
{
	ChildQuit = waitpid (-1, NULL, WNOHANG);
}
#endif

void MODSong::SetVolume (float volume)
{
	if (m_Module)
	{
		FMUSIC_SetMasterVolume (m_Module, (int)(volume * 256));
	}
}

void StreamSong::SetVolume (float volume)
{
	if (m_Channel)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, (int)(volume * 255));
	}
}

#ifdef _WIN32
#include "v_text.h"

static void PrintMidiDevice (int id, const char *name, WORD tech, DWORD support)
{
	if (id == snd_mididevice)
	{
		Printf (TEXTCOLOR_BOLD);
	}
	Printf ("% 2d. %s : ", id, name);
	switch (tech)
	{
	case MOD_MIDIPORT:		Printf ("MIDIPORT");		break;
	case MOD_SYNTH:			Printf ("SYNTH");			break;
	case MOD_SQSYNTH:		Printf ("SQSYNTH");			break;
	case MOD_FMSYNTH:		Printf ("FMSYNTH");			break;
	case MOD_MAPPER:		Printf ("MAPPER");			break;
#ifdef MOD_WAVETABLE
	case MOD_WAVETABLE:		Printf ("WAVETABLE");		break;
	case MOD_SWSYNTH:		Printf ("SWSYNTH");			break;
#endif
	}
	if (support & MIDICAPS_CACHE)
	{
		Printf (" CACHE");
	}
	if (support & MIDICAPS_LRVOLUME)
	{
		Printf (" LRVOLUME");
	}
	if (support & MIDICAPS_STREAM)
	{
		Printf (" STREAM");
	}
	if (support & MIDICAPS_VOLUME)
	{
		Printf (" VOLUME");
	}
	Printf (TEXTCOLOR_NORMAL "\n");
}

CCMD (snd_listmididevices)
{
	UINT id;
	MIDIOUTCAPS caps;
	MMRESULT res;

	PrintMidiDevice (-3, "DirectMusic", 0, 0);
	PrintMidiDevice (-2, "TiMidity++", 0, 0);
	if (nummididevices != 0)
	{
		PrintMidiDevice (-1, "MIDI Mapper", MOD_MAPPER, 0);
		for (id = 0; id < nummididevices; ++id)
		{
			res = midiOutGetDevCaps (id, &caps, sizeof(caps));
			if (res == MMSYSERR_NODRIVER)
				strcpy (caps.szPname, "<Driver not installed>");
			else if (res == MMSYSERR_NOMEM)
				strcpy (caps.szPname, "<No memory for description>");
			else if (res != MMSYSERR_NOERROR)
				continue;

			PrintMidiDevice (id, caps.szPname, caps.wTechnology, caps.dwSupport);
		}
	}
}

#include "m_menu.h"

void I_BuildMIDIMenuList (struct value_s **outValues, float *numValues)
{
	if (*outValues == NULL)
	{
		int count = 2 + nummididevices + (nummididevices > 0);
		value_t *values;

		*outValues = values = new value_t[count];

		values[0].name = "DirectMusic";
		values[0].value = -3.0;
		values[1].name = "TiMidity++";
		values[1].value = -2.0;
		if (nummididevices > 0)
		{
			UINT id;
			int p;

			values[2].name = "MIDI Mapper";
			values[2].value = -1.0;
			for (id = 0, p = 3; id < nummididevices; ++id)
			{
				MIDIOUTCAPS caps;
				MMRESULT res;

				res = midiOutGetDevCaps (id, &caps, sizeof(caps));
				if (res == MMSYSERR_NOERROR)
				{
					size_t len = strlen (caps.szPname) + 1;
					values[p].name = new char[len];
					values[p].value = (float)id;
					memcpy (values[p].name, caps.szPname, len);
					++p;
				}
			}
			*numValues = (float)p;
		}
		else
		{
			*numValues = 2.f;
		}
	}
}

#endif

void I_InitMusic (void)
{
	static bool setatterm = false;

#ifdef _WIN32	
	nummididevices = midiOutGetNumDevs ();
	nummididevicesset = true;
	snd_mididevice.Callback ();
#endif // _WIN32
	snd_musicvolume.Callback ();

	nomusic = !!Args.CheckParm("-nomusic") || !!Args.CheckParm("-nosound");

#ifdef _WIN32
	if (!nomusic)
	{
		if ((BufferReturnEvent = CreateEvent (NULL, FALSE, FALSE, NULL)) == NULL)
		{
			Printf ("Could not create MIDI callback event.\nMIDI music will be disabled.\n");
			nomusic = true;
		}
	}
#endif // _WIN32
	
	if (!setatterm)
	{
		setatterm = true;
		atterm (I_ShutdownMusic);
	
#ifndef _WIN32
		signal (SIGCHLD, ChildSigHandler);
#endif
	}
	MusicDown = false;
}


void STACK_ARGS I_ShutdownMusic(void)
{
	if (MusicDown)
		return;
	MusicDown = true;
	if (currSong)
	{
		S_StopMusic (true);
		assert (currSong == NULL);
	}
#ifdef _WIN32
	if (BufferReturnEvent)
	{
		CloseHandle (BufferReturnEvent);
		BufferReturnEvent = NULL;
	}
	// I don't know if this is an NT 4.0 bug or an FMOD bug, but if waveout
	// is used for sound, and a MIDI is also played, then when I quit, the OS
	// tells me a free block was modified after being freed. This is
	// apparently a synchronization issue between two threads, because if I
	// put this Sleep here after stopping the music but before shutting down
	// the entire sound system, the error does not happen. I don't think it's
	// a driver problem, because it happens with both a Vortex 2 and an Audigy.
	// Though if their drivers are both based off some common Microsoft sample
	// code, I suppose it could be a driver issue.
	Sleep (50);
#endif // _WIN32
}

#ifdef _WIN32
void MIDISong::MCIError (MMRESULT res, const char *descr)
{
	char errorStr[MAXERRORLENGTH];

	mciGetErrorString (res, errorStr, MAXERRORLENGTH);
	Printf (PRINT_BOLD, "An error occured while %s:\n", descr);
	Printf ("%s\n", errorStr);
}

void MIDISong::UnprepareHeaders ()
{
	PSTREAMBUF buffer = m_Buffers;

	while (buffer)
	{
		if (buffer->prepared)
		{
			MMRESULT res = midiOutUnprepareHeader ((HMIDIOUT)m_MidiStream,
				&buffer->midiHeader, sizeof(buffer->midiHeader));
			if (res != MMSYSERR_NOERROR)
				MCIError (res, "unpreparing headers");
			else
				buffer->prepared = false;
		}
		buffer = buffer->pNext;
	}
}

bool MIDISong::PrepareHeaders ()
{
	MMRESULT res;
	PSTREAMBUF buffer = m_Buffers;

	while (buffer)
	{
		if (!buffer->prepared)
		{
			memset (&buffer->midiHeader, 0, sizeof(MIDIHDR));
			buffer->midiHeader.lpData = (char *)buffer->pBuffer;
			buffer->midiHeader.dwBufferLength = CB_STREAMBUF;
			buffer->midiHeader.dwBytesRecorded = CB_STREAMBUF - buffer->cbLeft;
			res = midiOutPrepareHeader ((HMIDIOUT)m_MidiStream,
										&buffer->midiHeader, sizeof(MIDIHDR));
			if (res != MMSYSERR_NOERROR)
			{
				MCIError (res, "preparing headers");
				UnprepareHeaders ();
				return false;
			} else
				buffer->prepared = true;
		}
		buffer = buffer->pNext;
	}

	return true;
}

void MIDISong::SubmitBuffer ()
{
	MMRESULT res;

	res = midiStreamOut (m_MidiStream,
		 &m_CurrBuffer->midiHeader, sizeof(MIDIHDR));

	if (res != MMSYSERR_NOERROR)
		MCIError (res, "sending MIDI stream");

	m_CurrBuffer = m_CurrBuffer->pNext;
	if (m_CurrBuffer == NULL && m_Looping)
		m_CurrBuffer = m_Buffers;
}

void MIDISong::AllChannelsOff ()
{
	int i;

	for (i = 0; i < 16; i++)
		midiOutShortMsg ((HMIDIOUT)m_MidiStream, MIDI_NOTEOFF | i | (64<<16) | (60<<8));
	Sleep (1);
}

static void CALLBACK MidiProc (HMIDIIN hMidi, UINT uMsg, MIDISong *info,
							  DWORD dwParam1, DWORD dwParam2)
{
	info->MidiProc (uMsg);
}

void MIDISong::MidiProc (UINT uMsg)
{
	if (m_CallbackStatus == cb_dead)
		return;

	switch (uMsg)
	{
	case MOM_DONE:
		if (m_CallbackStatus == cb_die)
		{
			SetEvent (BufferReturnEvent);
			m_CallbackStatus = cb_dead;
		}
		else 
		{
			if (m_CurrBuffer == m_Buffers)
			{
				// Stop all notes before restarting the song
				// in case any are left hanging.
				AllChannelsOff ();
			}
			else if (m_CurrBuffer == NULL) 
			{
				SetEvent (BufferReturnEvent);
				return;
			}
			SubmitBuffer ();
		}
		break;
	}
}
#endif // _WIN32

void I_PlaySong (void *handle, int _looping)
{
	MusInfo *info = (MusInfo *)handle;

	if (!info || nomusic)
		return;

	info->Stop ();
	info->Play (_looping ? true : false);
	
	if (info->m_Status == MusInfo::STATE_Playing)
		currSong = info;
	else
		currSong = NULL;
}

#ifdef _WIN32
void MIDISong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	MMRESULT res;

	// note: midiStreamOpen changes mididevice if it's set to MIDI_MAPPER
	// (interesting undocumented behavior)
	if ((res = midiStreamOpen (&m_MidiStream,
							   &mididevice,
							   (DWORD)1,
							   (DWORD_PTR)::MidiProc, (DWORD_PTR)this,
							   CALLBACK_FUNCTION)) == MMSYSERR_NOERROR)
	{
		MIDIPROPTIMEDIV timedivProp;

		timedivProp.cbStruct = sizeof(timedivProp);
		timedivProp.dwTimeDiv = midTimeDiv;
		res = midiStreamProperty (m_MidiStream, (LPBYTE)&timedivProp,
								  MIDIPROP_SET | MIDIPROP_TIMEDIV);
		if (res != MMSYSERR_NOERROR)
			MCIError (res, "setting time division");

		SetStreamVolume ();

		// Preload all instruments into soundcard RAM (if necessary).
		// On my GUS PnP, this is necessary because it will fail to
		// play some instruments on some songs until the song loops
		// if the instrument isn't already in memory. (Why it doesn't
		// load them when they're needed is beyond me, because it's only
		// some instruments it doesn't play properly the first time--and
		// I don't know exactly which ones those are.) The 250 ms delay
		// between note on and note off is fairly lengthy, so I try and
		// get the instruments going on multiple channels to reduce the
		// number of times I have to sleep.
		if (snd_midiprecache)
		{
			int i, j;

			DPrintf ("MIDI uses instruments:\n");
			for (i = j = 0; i < 127; i++)
			{
				if (UsedPatches[i])
				{
					DPrintf (" %d", i);
					res = midiOutShortMsg ((HMIDIOUT)m_MidiStream,
									 MIDI_PRGMCHANGE | (i<<8) | j);
					res = midiOutShortMsg ((HMIDIOUT)m_MidiStream,
									 MIDI_NOTEON | (60<<8) | (1<<16) | j);
					++j;
					if (j == 10)
					{
						++j;
					}
					else if (j == 16)
					{
						Sleep (250);
						for (j = 0; j < 16; j++)
						{
							if (j != 10)
							{
								midiOutShortMsg ((HMIDIOUT)m_MidiStream,
												 MIDI_NOTEOFF | (60<<8) | (64<<16) | j);
							}
						}
						j = 0;
					}
				}
			}
			if (j > 0)
			{
				Sleep (250);
				for (i = 0; i < j; i++)
				{
					if (i != 10)
					{
						midiOutShortMsg ((HMIDIOUT)m_MidiStream,
										 MIDI_NOTEOFF | (60<<8) | (64<<16) | i);
					}
				}
			}

		/*
			DPrintf ("\nMIDI uses percussion keys:\n");
			for (i = 0; i < 127; i++)
				if (UsedPatches[i+128]) {
					DPrintf (" %d", i);
					midiOutShortMsg ((HMIDIOUT)info->midiStream,
									 MIDI_NOTEON | (i<<8) | (1<<16) | 10);
					Sleep (235);
					midiOutShortMsg ((HMIDIOUT)info->midiStream,
									 MIDI_NOTEOFF | (i<<8) | (64<<16));
				}
		*/
			DPrintf ("\n");
		}

		if (PrepareHeaders ())
		{
			m_CallbackStatus = cb_play;
			m_CurrBuffer = m_Buffers;
			SubmitBuffer ();
			res = midiStreamRestart (m_MidiStream);
			if (res == MMSYSERR_NOERROR)
			{
				m_Status = STATE_Playing;
			}
			else
			{
				MCIError (res, "starting playback");
				UnprepareHeaders ();
				midiStreamClose (m_MidiStream);
				m_MidiStream = NULL;
				m_Status = STATE_Stopped;
			}
		}
		else
		{
			UnprepareHeaders ();
			midiStreamClose (m_MidiStream);
			m_MidiStream = NULL;
			m_Status = STATE_Stopped;
		}
	}
	else
	{
		MCIError (res, "opening MIDI stream");
		if (snd_mididevice != -1)
		{
			Printf ("Trying again with MIDI mapper\n");
			snd_mididevice = -1;
		}
		else
		{
			m_Status = STATE_Stopped;
		}
	}
}
#endif // _WIN32

void MODSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (FMUSIC_PlaySong (m_Module))
	{
		FMUSIC_SetMasterVolume (m_Module, (int)(snd_musicvolume * 256));
		m_Status = STATE_Playing;
	}
}

void StreamSong::Play (bool looping)
{
	int volume = (int)(snd_musicvolume * 255);

	m_Status = STATE_Stopped;
	m_Looping = looping;

	m_Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, m_Stream, NULL, true);
	if (m_Channel != -1)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, volume);
		FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
		FSOUND_SetPaused (m_Channel, false);
		m_Status = STATE_Playing;
		m_LastPos = 0;
	}
}

void TimiditySong::Play (bool looping)
{
	int volume = (int)(snd_musicvolume * 255);

	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (LaunchTimidity ())
	{
		if (m_Stream != NULL)
		{
			m_Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, m_Stream, NULL, true);
			if (m_Channel != -1)
			{
				FSOUND_SetVolumeAbsolute (m_Channel, volume);
				FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
				FSOUND_SetPaused (m_Channel, false);
				m_Status = STATE_Playing;
			}
		}
		else
		{ // Assume success if not mixing with FMOD
			m_Status = STATE_Playing;
		}
	}
}

void CDSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;
	if (m_Track != 0 ? CD_Play (m_Track, looping) : CD_PlayCD (looping))
	{
		m_Status = STATE_Playing;
	}
}

void I_PauseSong (void *handle)
{
	MusInfo *info = (MusInfo *)handle;

	if (info)
		info->Pause ();
}

#ifdef _WIN32
void MIDISong::Pause ()
{
	if (m_Status == STATE_Playing &&
		m_MidiStream &&
		midiStreamPause (m_MidiStream) == MMSYSERR_NOERROR)
	{
		m_Status = STATE_Paused;
	}
}
#endif // _WIN32

void MODSong::Pause ()
{
	if (m_Status == STATE_Playing)
	{
		if (FMUSIC_SetPaused (m_Module, TRUE))
			m_Status = STATE_Paused;
	}
}

void StreamSong::Pause ()
{
	if (m_Status == STATE_Playing && m_Stream != NULL)
	{
		if (FSOUND_SetPaused (m_Channel, TRUE))
			m_Status = STATE_Paused;
	}
}

void CDSong::Pause ()
{
	if (m_Status == STATE_Playing)
	{
		CD_Pause ();
		m_Status = STATE_Paused;
	}
}

void I_ResumeSong (void *handle)
{
	MusInfo *info = (MusInfo *)handle;

	if (info)
		info->Resume ();
}

#ifdef _WIN32
void MIDISong::Resume ()
{
	if (m_Status == STATE_Paused &&
		m_MidiStream &&
		midiStreamRestart (m_MidiStream) == MMSYSERR_NOERROR)
	{
		m_Status = STATE_Playing;
	}
}
#endif // _WIN32

void MODSong::Resume ()
{
	if (m_Status == STATE_Paused)
	{
		if (FMUSIC_SetPaused (m_Module, FALSE))
			m_Status = STATE_Playing;
	}
}

void CDSong::Resume ()
{
	if (m_Status == STATE_Paused)
	{
		if (CD_Resume ())
			m_Status = STATE_Playing;
	}
}

void StreamSong::Resume ()
{
	if (m_Status == STATE_Paused && m_Stream != NULL)
	{
		if (FSOUND_SetPaused (m_Channel, FALSE))
			m_Status = STATE_Playing;
	}
}

void I_StopSong (void *handle)
{
	MusInfo *info = (MusInfo *)handle;
	
	if (info)
		info->Stop ();

	if (info == currSong)
		currSong = NULL;
}

#ifdef _WIN32
void MIDISong::Stop ()
{
	if (m_Status != STATE_Stopped && m_MidiStream)
	{
		if (m_CallbackStatus != cb_dead)
			m_CallbackStatus = cb_die;
		midiStreamStop (m_MidiStream);
		WaitForSingleObject (BufferReturnEvent, 5000);
		midiOutReset ((HMIDIOUT)m_MidiStream);
		UnprepareHeaders ();
		UnsetStreamVolume ();
		midiStreamClose (m_MidiStream);
		m_MidiStream = NULL;
	}
	m_Status = STATE_Stopped;
}
#endif // _WIN32

void MODSong::Stop ()
{
	if (m_Status != STATE_Stopped && m_Module)
	{
		FMUSIC_StopSong (m_Module);
	}
	m_Status = STATE_Stopped;
}

void StreamSong::Stop ()
{
	if (m_Status != STATE_Stopped && m_Stream)
	{
		FSOUND_Stream_Stop (m_Stream);
		m_Channel = -1;
	}
	m_Status = STATE_Stopped;
}

void TimiditySong::Stop ()
{
	if (m_Status != STATE_Stopped)
	{
		if (m_Stream != NULL)
		{
			FSOUND_Stream_Stop (m_Stream);
			m_Channel = -1;
		}
#ifdef _WIN32
		if (ChildProcess != INVALID_HANDLE_VALUE)
		{
			SetEvent (KillerEvent);
			WaitForSingleObject (ChildProcess, INFINITE);
			CloseHandle (ChildProcess);
			ChildProcess = INVALID_HANDLE_VALUE;
		}
#else
		if (ChildProcess != -1)
		{
			if (kill (ChildProcess, SIGTERM) != 0)
			{
				kill (ChildProcess, SIGKILL);
			}
			waitpid (ChildProcess, NULL, 0);
			ChildProcess = -1;
		}
#endif
	}
	m_Status = STATE_Stopped;
}

void CDSong::Stop ()
{
	if (m_Status != STATE_Stopped)
	{
		m_Status = STATE_Stopped;
		CD_Stop ();
	}
}

void I_UnRegisterSong (void *handle)
{
	MusInfo *info = (MusInfo *)handle;

	if (info)
	{
		delete info;
	}
}

#ifdef _WIN32
MIDISong::~MIDISong ()
{
	Stop ();
	if (m_IsMUS)
		mus2strmCleanup ();
	else
		mid2strmCleanup ();
	m_Buffers = NULL;
}
#endif // _WIN32

MODSong::~MODSong ()
{
	Stop ();
	if (m_Module != NULL)
	{
		FMUSIC_FreeSong (m_Module);
		m_Module = NULL;
	}
}

StreamSong::~StreamSong ()
{
	Stop ();
	if (m_Stream != NULL)
	{
		FSOUND_Stream_Close (m_Stream);
		m_Stream = NULL;
	}
}

TimiditySong::~TimiditySong ()
{
	Stop ();
#if _WIN32
	if (ReadWavePipe != INVALID_HANDLE_VALUE)
	{
		CloseHandle (ReadWavePipe);
		ReadWavePipe = INVALID_HANDLE_VALUE;
	}
	if (WriteWavePipe != INVALID_HANDLE_VALUE)
	{
		CloseHandle (WriteWavePipe);
		WriteWavePipe = INVALID_HANDLE_VALUE;
	}
	if (KillerEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle (KillerEvent);
		KillerEvent = INVALID_HANDLE_VALUE;
	}
#else
	if (WavePipe[0] != -1)
	{
		close (WavePipe[0]);
		WavePipe[0] = -1;
	}
	if (WavePipe[1] != -1)
	{
		close (WavePipe[1]);
		WavePipe[1] = -1;
	}
#endif
	if (CommandLine != NULL)
	{
		delete[] CommandLine;
	}
}

CDSong::~CDSong ()
{
	Stop ();
	m_Inited = false;
}

void *I_RegisterSong (int handle, int pos, int len)
{
	MusInfo *info = NULL;
	DWORD id;

	if (nomusic)
	{
		return 0;
	}

	lseek (handle, pos, SEEK_SET);
	read (handle, &id, 4);

	if (id == MAKE_ID('M','U','S',0x1a))
	{
		// This is a mus file
		if (!nofmod && opl_enable)
		{
			info = new OPLMUSSong (handle, pos, len);
		}
		if (info == NULL)
		{
#ifdef _WIN32
			if (snd_mididevice == -3)
			{
				info = new DMusSong (handle, pos, len);
			}
			else if (snd_mididevice != -2)
			{
				info = new MUSSong (handle, pos, len);
			}
			else if (!nofmod)
#endif // _WIN32
			{
				info = new TimiditySong (handle, pos, len);
			}
		}
	}
	else if (id == MAKE_ID('M','T','h','d'))
	{
		// This is a midi file
#ifdef _WIN32
		if (snd_mididevice == -3)
		{
			info = new DMusSong (handle, pos, len);
		}
		else if (snd_mididevice != -2)
		{
			info = new MIDISong (handle, pos, len);
		}
		else if (!nofmod)
#endif // _WIN32
		{
			info = new TimiditySong (handle, pos, len);
		}
	}
	else if (id == MAKE_ID('S','N','E','S') && len >= 66048)
	{
		char tag[0x23-4];

		if (read (handle, tag, 0x23-4) == 0x23-4 &&
			strncmp (tag, "-SPC700 Sound File Data", 23) == 0 &&
			tag[0x21-4] == 26 &&
			tag[0x22-4] == 26)
		{
			info = new SPCSong (handle, pos, len);
		}
	}

	if (info == NULL)
	{
		if (id == (('R')|(('I')<<8)|(('F')<<16)|(('F')<<24)))
		{
			DWORD subid = 0;
			lseek (handle, pos+8, SEEK_SET);
			read (handle, &subid, 4);
			if (subid == (('C')|(('D')<<8)|(('D')<<16)|(('A')<<24)))
			{
				// This is a CDDA file
				info = new CDDAFile (handle, pos, len);
			}
		}
		if (info == NULL && !nofmod)	// no FMOD => no modules/streams
		{
			// First try loading it as MOD, then as a stream
			info = new MODSong (handle, pos, len);
			if (!info->IsValid ())
			{
				delete info;
				info = new StreamSong (handle, pos, len);
			}
		}
	}

	if (info && !info->IsValid ())
	{
		delete info;
		info = NULL;
	}

	return info;
}

void *I_RegisterCDSong (int track, int id)
{
	MusInfo *info = new CDSong (track, id);

	if (info && !info->IsValid ())
	{
		delete info;
		info = NULL;
	}

	return info;
}

#ifdef _WIN32
MIDISong::MIDISong ()
: m_IsMUS (false), m_Buffers (NULL), m_bVolGood (false)
{
}

MIDISong::MIDISong (int handle, int pos, int len)
{
	m_Buffers = NULL;
	m_IsMUS = false;
	if (nummididevices > 0 && lseek (handle, pos, SEEK_SET) != -1)
	{
		byte *data = new byte[len];
		if (read (handle, data, len) == len)
		{
			m_Buffers = mid2strmConvert ((LPBYTE)data, len);
		}
		delete[] data;
	}
}

MUSSong::MUSSong (int handle, int pos, int len) : MIDISong ()
{
	m_Buffers = NULL;
	m_IsMUS = true;
	if (nummididevices > 0 && lseek (handle, pos, SEEK_SET) != -1)
	{
		byte *data = new byte[len];
		if (read (handle, data, len) == len)
		{
			m_Buffers = mus2strmConvert ((LPBYTE)data, len);
		}
		delete[] data;
	}
}
#endif // _WIN32

MODSong::MODSong (int handle, int pos, int len)
{
	BYTE *song = new BYTE[len];
	lseek (handle, SEEK_SET, pos);
	read (handle, song, len);
	m_Module = FMUSIC_LoadSongMemory (song, len);
	delete[] song;
	//m_Module = FMUSIC_LoadSong ((char *)new FileHandle (handle, pos, len));
}

#ifdef _WIN32
DMusSong::DMusSong (int handle, int pos, int len)
: DiskName ("zmid")
{
	FILE *f = fopen (DiskName, "wb");
	if (f == NULL)
	{
		Printf (PRINT_BOLD, "Could not open temp music file\n");
		return;
	}

	BYTE *buf = new BYTE[len];
	bool success;

	if (lseek (handle, pos, SEEK_SET) == -1 || read (handle, buf, len) != len)
	{
		fclose (f);
		Printf (PRINT_BOLD, "Could not read source music file\n");
		return;
	}

	// The file type has already been checked before this class instance was
	// created, so we only need to check one character to determine if this
	// is a MUS or MIDI file and write it to disk as appropriate.
	if (buf[1] == 'T')
	{
		success = (fwrite (buf, 1, len, f) == (size_t)len);
	}
	else
	{
		success = ProduceMIDI (buf, f);
	}
	fclose (f);
	delete[] buf;

	if (success)
	{
		Disable_FSOUND_IO_Loader ();
		m_Module = FMUSIC_LoadSong (DiskName);
		Enable_FSOUND_IO_Loader ();
	}
	else
	{
		m_Module = NULL;
	}
}
#endif

StreamSong::StreamSong (int handle, int pos, int len)
: m_Channel (-1)
{
	m_Stream = FSOUND_Stream_OpenFile ((char *)new FileHandle (handle, pos, len),
		FSOUND_LOOP_NORMAL|FSOUND_NORMAL|FSOUND_2D, 0);
}

TimiditySong::TimiditySong (int handle, int pos, int len)
	: DiskName ("zmid"),
#ifdef _WIN32
	  ReadWavePipe (INVALID_HANDLE_VALUE), WriteWavePipe (INVALID_HANDLE_VALUE),
	  KillerEvent (INVALID_HANDLE_VALUE),
	  ChildProcess (INVALID_HANDLE_VALUE),
	  Validated (false),
#else
	  ChildProcess (-1),
#endif
	  CommandLine (NULL)
{
	bool success;
	FILE *f;

#ifndef _WIN32
	WavePipe[0] = WavePipe[1] = -1;
#endif
	
	if (DiskName == NULL)
	{
		Printf (PRINT_BOLD, "Could not create temp music file\n");
		return;
	}

	f = fopen (DiskName, "wb");
	if (f == NULL)
	{
		Printf (PRINT_BOLD, "Could not open temp music file\n");
		return;
	}

	BYTE *buf = new BYTE[len];

	if (lseek (handle, pos, SEEK_SET) == -1 || read (handle, buf, len) != len)
	{
		fclose (f);
		Printf (PRINT_BOLD, "Could not read source music file\n");
		return;
	}

	// The file type has already been checked before this class instance was
	// created, so we only need to check one character to determine if this
	// is a MUS or MIDI file and write it to disk as appropriate.
	if (buf[1] == 'T')
	{
		success = (fwrite (buf, 1, len, f) == (size_t)len);
	}
	else
	{
		success = ProduceMIDI (buf, f);
	}
	fclose (f);
	delete[] buf;

	if (success)
	{
		PrepTimidity ();
	}
	else
	{
		Printf (PRINT_BOLD, "Could not write temp music file\n");
	}
}

void TimiditySong::PrepTimidity ()
{
	char cmdline[512];
	int cmdsize;
	int pipeSize;
#ifdef _WIN32
	static SECURITY_ATTRIBUTES inheritable = { sizeof(inheritable), NULL, TRUE };
	
	if (!Validated && !ValidateTimidity ())
	{
		return;
	}

	Validated = true;
	KillerEvent = CreateEvent (NULL, FALSE, FALSE, EventName);
	if (KillerEvent == INVALID_HANDLE_VALUE)
	{
		Printf (PRINT_BOLD, "Could not create TiMidity++ kill event.\n");
		return;
	}
#endif // WIN32

	cmdsize = sprintf (cmdline, "%s %s -EFchorus=%s -EFreverb=%s -s%d ",
		*timidity_exe, *timidity_extargs,
		*timidity_chorus, *timidity_reverb, *timidity_frequency);

	pipeSize = (timidity_pipe * timidity_frequency / 1000)
		<< (timidity_stereo + !timidity_8bit);

	if (pipeSize != 0)
	{
#ifdef _WIN32
		// Round pipe size up to nearest power of 2 to try and avoid partial
		// buffer reads in FillStream() under NT. This does not seem to be an
		// issue under 9x.
		int bitmask = pipeSize & -pipeSize;
		
		while (bitmask < pipeSize)
			bitmask <<= 1;
		pipeSize = bitmask;

		if (!CreatePipe (&ReadWavePipe, &WriteWavePipe, &inheritable, pipeSize))
#else // WIN32
		if (pipe (WavePipe) == -1)
#endif
		{
			Printf (PRINT_BOLD, "Could not create a data pipe for TiMidity++.\n");
			pipeSize = 0;
		}
		else
		{
			m_Stream = FSOUND_Stream_Create (FillStream, pipeSize,
				FSOUND_SIGNED | FSOUND_2D |
				(timidity_stereo ? FSOUND_STEREO : FSOUND_MONO) |
				(timidity_8bit ? FSOUND_8BITS : FSOUND_16BITS),
				timidity_frequency, (int)this);
			if (m_Stream == NULL)
			{
				Printf (PRINT_BOLD, "Could not create FMOD music stream.\n");
				pipeSize = 0;
#ifdef _WIN32
				CloseHandle (ReadWavePipe);
				CloseHandle (WriteWavePipe);
				ReadWavePipe = WriteWavePipe = INVALID_HANDLE_VALUE;
#else
				close (WavePipe[0]);
				close (WavePipe[1]);
				WavePipe[0] = WavePipe[1] = -1;
#endif
			}
		}
		
		if (pipeSize == 0)
		{
			Printf (PRINT_BOLD, "If your soundcard cannot play more than one\n"
								"wave at a time, you will hear no music.\n");
		}
		else
		{
			cmdsize += sprintf (cmdline + cmdsize, "-o - -Ors");
		}
	}

	if (pipeSize == 0)
	{
		cmdsize += sprintf (cmdline + cmdsize, "-Od");
	}

	cmdline[cmdsize++] = timidity_stereo ? 'S' : 'M';
	cmdline[cmdsize++] = timidity_8bit ? '8' : '1';
	if (timidity_byteswap)
	{
		cmdline[cmdsize++] = 'x';
	}

	LoopPos = cmdsize + 4;

	sprintf (cmdline + cmdsize, " -idl %s", DiskName.GetName ());

	CommandLine = copystring (cmdline);
}

#ifdef _WIN32
// Check that this TiMidity++ knows about the TiMidity Killer event.
// If not, then we can't use it, because Win32 provides no other way
// to conveniently signal it to quit. The check is done by simply
// searching for the event's name somewhere in the executable.
bool TimiditySong::ValidateTimidity ()
{
	char foundPath[MAX_PATH];
	char *filePart;
	DWORD pathLen;
	DWORD fileLen;
	HANDLE diskFile;
	HANDLE mapping;
	const BYTE *exeBase;
	const BYTE *exeEnd;
	const BYTE *exe;
	bool good;

	pathLen = SearchPath (NULL, timidity_exe, NULL, MAX_PATH, foundPath, &filePart);
	if (pathLen == 0)
	{
		Printf (PRINT_BOLD, "Please set the timidity_exe cvar to the location of TiMidity++\n");
		return false;
	}
	if (pathLen > MAX_PATH)
	{
		Printf (PRINT_BOLD, "The path to TiMidity++ is too long\n");
		return false;
	}

	diskFile = CreateFile (foundPath, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (diskFile == INVALID_HANDLE_VALUE)
	{
		Printf (PRINT_BOLD, "Could not access %s\n", foundPath);
		return false;
	}
	fileLen = GetFileSize (diskFile, NULL);
	mapping = CreateFileMapping (diskFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (mapping == NULL)
	{
		Printf (PRINT_BOLD, "Could not create mapping for %s\n", foundPath);
		CloseHandle (diskFile);
		return false;
	}
	exeBase = (const BYTE *)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
	if (exeBase == NULL)
	{
		Printf (PRINT_BOLD, "Could not map %s\n", foundPath);
		CloseHandle (mapping);
		CloseHandle (diskFile);
		return false;
	}

	good = false;
	try
	{
		for (exe = exeBase, exeEnd = exeBase+fileLen; exe < exeEnd; )
		{
			const char *tSpot = (const char *)memchr (exe, 'T', exeEnd - exe);
			if (tSpot == NULL)
			{
				break;
			}
			if (memcmp (tSpot+1, EventName+1, sizeof(EventName)-1) == 0)
			{
				good = true;
				break;
			}
			exe = (const BYTE *)tSpot + 1;
		}
	}
	catch (...)
	{
		Printf (PRINT_BOLD, "Error reading %s\n", foundPath);
	}
	if (!good)
	{
		Printf (PRINT_BOLD, "ZDoom requires a special version of TiMidity++\n");
	}

	UnmapViewOfFile (exeBase);
	CloseHandle (mapping);
	CloseHandle (diskFile);

	return good;
}
#endif // _WIN32

bool TimiditySong::LaunchTimidity ()
{
	if (CommandLine == NULL)
	{
		return false;
	}

	// Tell Timidity whether it should loop or not
	CommandLine[LoopPos] = m_Looping ? 'l' : ' ';
	DPrintf ("cmd: \x1cG%s\n", CommandLine);

#ifdef _WIN32
	STARTUPINFO startup = { sizeof(startup), };
	PROCESS_INFORMATION procInfo;

	startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

	startup.hStdInput = INVALID_HANDLE_VALUE;
	startup.hStdOutput = WriteWavePipe != INVALID_HANDLE_VALUE ?
						  WriteWavePipe : GetStdHandle (STD_OUTPUT_HANDLE);
	startup.hStdError = GetStdHandle (STD_ERROR_HANDLE);

	startup.lpTitle = "TiMidity (ZDoom Launched)";
	startup.wShowWindow = SW_SHOWMINNOACTIVE;

	if (CreateProcess (NULL, CommandLine, NULL, NULL, TRUE,
		/*HIGH_PRIORITY_CLASS|*/DETACHED_PROCESS, NULL, NULL, &startup, &procInfo))
	{
		ChildProcess = procInfo.hProcess;
		//SetThreadPriority (procInfo.hThread, THREAD_PRIORITY_HIGHEST);
		CloseHandle (procInfo.hThread);		// Don't care about the created thread
		return true;
	}

	char hres[9];
	LPTSTR msgBuf;
	HRESULT err = GetLastError ();

	if (!FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
						FORMAT_MESSAGE_FROM_SYSTEM |
						FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL, err, 0, (LPTSTR)&msgBuf, 0, NULL))
	{
		sprintf (hres, "%08x", err);
		msgBuf = hres;
	}

	Printf (PRINT_BOLD, "Could not run timidity with the command line:\n%s\n"
						"Reason: %s\n", CommandLine, msgBuf);
	if (msgBuf != hres)
	{
		LocalFree (msgBuf);
	}
	return false;
#else
	if (WavePipe[0] != -1 && WavePipe[1] == -1 && m_Stream != NULL)
	{
		// Timidity was previously launched, so the write end of the pipe
		// is closed, and the read end is still open. Close the pipe
		// completely and reopen it.
		
		close (WavePipe[0]);
		WavePipe[0] = -1;
		FSOUND_Stream_Close (m_Stream);
		m_Stream = NULL;
		PrepTimidity ();
	}
	
	int forkres;
	wordexp_t words;

	switch (wordexp (CommandLine, &words, 0))
	{
	case 0: // all good
		break;
		
	case WRDE_NOSPACE:
		wordfree (&words);
	default:
		return false;
	}

	forkres = fork ();

	if (forkres == 0)
	{
		close (WavePipe[0]);
		dup2 (WavePipe[1], STDOUT_FILENO);
		freopen ("/dev/null", "r", stdin);
		freopen ("/dev/null", "w", stderr);
		close (WavePipe[1]);
		
		printf ("exec %s\n", words.we_wordv[0]);
		execvp (words.we_wordv[0], words.we_wordv);
		exit (0);	// if execvp succeeds, we never get here
	}
	else if (forkres < 0)
	{
		Printf (PRINT_BOLD, "Could not fork when trying to start timidity\n");
	}
	else
	{
//		printf ("child is %d\n", forkres);
		ChildProcess = forkres;
		close (WavePipe[1]);
		WavePipe[1] = -1;
	}
	
	wordfree (&words);
	return ChildProcess != -1;
#endif // _WIN32
}

signed char STACK_ARGS TimiditySong::FillStream (FSOUND_STREAM *stream, void *buff, int len, int param)
{
	TimiditySong *song = (TimiditySong *)param;
	
#ifdef _WIN32
	DWORD avail, got, didget;

	if (!PeekNamedPipe (song->ReadWavePipe, NULL, 0, NULL, &avail, NULL) || avail == 0)
	{ // If nothing is available from the pipe, play silence.
		memset (buff, 0, len);
	}
	else
	{
		didget = 0;
		for (;;)
		{
			ReadFile (song->ReadWavePipe, (BYTE *)buff+didget, len-didget, &got, NULL);
			didget += got;
			if (didget >= len)
				break;

			// Give TiMidity a chance to output something more to the pipe
			Sleep (10);
			if (!PeekNamedPipe (song->ReadWavePipe, NULL, 0, NULL, &avail, NULL) || avail == 0)
			{
				memset ((BYTE *)buff+didget, 0, len-didget);
				break;
			}
		} 
	}
#else
	ssize_t got;

	got = read (song->WavePipe[0], (BYTE *)buff, len);
	if (got < len)
	{
		memset ((BYTE *)buff+got, 0, len-got);
	}

	if (ChildQuit == song->ChildProcess)
	{
		ChildQuit = 0;
//		printf ("child gone\n");
		song->ChildProcess = -1;
		return FALSE;
	}
#endif
	return TRUE;
}

CDSong::CDSong (int track, int id)
{
	bool success;

	m_Inited = false;

	if (id != 0)
	{
		success = CD_InitID (id);
	}
	else
	{
		success = CD_Init ();
	}

	if (success && track == 0 || CD_CheckTrack (track))
	{
		m_Inited = true;
		m_Track = track;
	}
}

CDDAFile::CDDAFile (int handle, int pos, int len)
	: CDSong ()
{
	DWORD chunk;
	WORD track;
	DWORD discid;
	int cursor = 12;

	// I_RegisterSong already identified this as a CDDA file, so we
	// just need to check the contents we're interested in.

	while (cursor < len - 8)
	{
		if (lseek (handle, pos+cursor, SEEK_SET) == -1)
			return;
		if (read (handle, &chunk, 4) != 4)
			return;
		if (chunk != (('f')|(('m')<<8)|(('t')<<16)|((' ')<<24)))
		{
			if (read (handle, &chunk, 4) != 4)
				return;
			cursor += LONG(chunk) + 8;
		}
		else
		{
			if (lseek (handle, pos+cursor+10, SEEK_SET) != -1)
			{
				if (read (handle, &track, 2) == 2 &&
					read (handle, &discid, 4) == 4)
				{
					track = SHORT(track);
					discid = LONG(discid);

					if (CD_InitID (discid) && CD_CheckTrack (track))
					{
						m_Inited = true;
						m_Track = track;
					}
				}
			}
			return;
		}
	}
}

// Is the song playing?
bool I_QrySongPlaying (void *handle)
{
	MusInfo *info = (MusInfo *)handle;

	return info ? info->IsPlaying () : false;
}

#ifdef _WIN32
bool MIDISong::IsPlaying ()
{
	return m_Looping ? true : m_Status != STATE_Stopped;
}
#endif // _WIN32

bool MODSong::IsPlaying ()
{
	if (m_Status != STATE_Stopped)
	{
		if (FMUSIC_IsPlaying (m_Module))
		{
			if (!m_Looping && FMUSIC_IsFinished (m_Module))
			{
				Stop ();
				return false;
			}
			return true;
		}
		else if (m_Looping)
		{
			Play (true);
			return m_Status != STATE_Stopped;
		}
	}
	return false;
}

bool StreamSong::IsPlaying ()
{
	if (m_Channel != -1)
	{
		if (m_Looping)
			return true;

		int pos = FSOUND_Stream_GetPosition (m_Stream);

		if (pos < m_LastPos)
		{
			Stop ();
			return false;
		}

		m_LastPos = pos;
		return true;
	}
	return false;
}

bool TimiditySong::IsPlaying ()
{
#ifdef _WIN32
	if (ChildProcess != INVALID_HANDLE_VALUE)
	{
		if (WaitForSingleObject (ChildProcess, 0) != WAIT_TIMEOUT)
		{ // Timidity has quit
			CloseHandle (ChildProcess);
			ChildProcess = INVALID_HANDLE_VALUE;
#else
	if (ChildProcess != -1)
	{
		if (waitpid (ChildProcess, NULL, WNOHANG) == ChildProcess)
		{
			ChildProcess = -1;
#endif
			if (m_Looping)
			{
				if (!LaunchTimidity ())
				{
					Stop ();
					return false;
				}
				return true;
			}
			return false;
		}
		return true;
	}
	return false;
}

bool CDSong::IsPlaying ()
{
	if (m_Status == STATE_Playing)
	{
		if (CD_GetMode () != CDMode_Play)
		{
			Stop ();
		}
	}
	return m_Status != STATE_Stopped;
}

// Change to a different part of the song
bool I_SetSongPosition (void *handle, int order)
{
	MusInfo *info = (MusInfo *)handle;
	return info ? info->SetPosition (order) : false;
}

bool MODSong::SetPosition (int order)
{
	return !!FMUSIC_SetOrder (m_Module, order);
}

#if !defined(_WIN32) && 0
FPipeBuffer::FPipeBuffer (int fragSize, int nFrags, int pipe)
	: PipeHandle (pipe),
	  FragSize (fragSize),
	  BuffSize (fragSize * nFrags),
	  WritePos (0), ReadPos (0), GotFull (false),
	  Reader (0), PleaseExit (0)
{
	Buffer = new BYTE[BuffSize];
	if (Buffer != NULL)
	{
		BufferMutex = SDL_CreateMutex ();
		if (BufferMutex == NULL)
		{
			Reader = SDL_CreateThread (ThreadProc, (void *)this);
		}
	}
}

FPipeBuffer::~FPipeBuffer ()
{
	if (Reader != NULL)
	{ // I like the Win32 IPC facilities better than SDL's
	  // Fortunately, this is a simple thread, so I can cheat
	  // like this.
		SDL_KillThread (ThreadProc);
	}
	if (BufferMutex != NULL)
	{
		SDL_DestroyMutex (BufferMutex);
	}
	if (Buffer != NULL)
	{
		delete[] Buffer;
	}
}

int FPipeBuffer::ReadFrag (BYTE *buf)
{
	int startavvail;
	int avail;
	int pos;
	int opos;
	
	if (SDL_mutexP (BufferMutex) == -1)
		return 0;
	
	if (WritePos > ReadPos)
	{
		avail = WritePos - ReadPos;
	}
	else
	{
		avail = BuffSize - ReadPos + WritePos;
	}
	if (avail > FragSize)
		avail = FragSize;
	
	startavail = avali;
	pos = ReadPos;
	opos = 0;
	
	while (avail != 0)
	{
		int thistime;
		
		thistime = (pos + avail > BuffSize) ? BuffSize - pos : avail;
		memcpy (buf + opos, Buffer + pos, thistime);
		if (thistime != avail)
		{
			pos = 0;
			avail -= thistime;
		}
		opos += thistime;
	}

	ReadPos = pos;
	
	SDL_mutexV (BufferMutex);
	
	return startavail;
}
#endif	// !_WIN32

#ifndef NOSPC
CVAR (Int, spc_amp, 30, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_8bit, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_stereo, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_lowpass, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_surround, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_oldsamples, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (Int, spc_quality, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (spc_quality < 0)
	{
		spc_quality = 0;
	}
	else if (spc_quality > 3)
	{
		spc_quality = 3;
	}
}

CUSTOM_CVAR (Int, spc_frequency, 32000, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (spc_frequency < 8000)
	{
		spc_frequency = 8000;
	}
	else if (spc_frequency > 65535)
	{
		spc_frequency = 65535;
	}
}

SPCSong::SPCSong (int handle, int pos, int len)
{
	if (!LoadEmu ())
	{
		return;
	}

	// No sense in using a higher frequency than the final output
	int freq = MIN (*spc_frequency, *snd_samplerate);

	m_Stream = FSOUND_Stream_Create (FillStream, 16384,
		FSOUND_SIGNED | FSOUND_2D |
		(spc_stereo ? FSOUND_STEREO : FSOUND_MONO) |
		(spc_8bit ? FSOUND_8BITS : FSOUND_16BITS),
		freq, (int)this);
	if (m_Stream == NULL)
	{
		Printf (PRINT_BOLD, "Could not create FMOD music stream.\n");
		CloseEmu ();
		return;
	}

	ResetAPU (spc_amp);
	SetAPUOpt (0, spc_stereo + 1, spc_8bit ? 8 : 16, freq, spc_quality,
		(spc_lowpass ? 1 : 0) | (spc_oldsamples ? 2 : 0) | (spc_surround ? 4 : 0));

	BYTE spcfile[66048];

	lseek (handle, pos, SEEK_SET);
	if (66048 != read (handle, spcfile, 66048))
	{
		CloseEmu ();
		return;
	}

	void *apuram;
	BYTE *extraram;
	void *dsp;

	GetAPUData (&apuram, &extraram, NULL, NULL, &dsp, NULL, NULL, NULL);

	memcpy (apuram, spcfile + 0x100, 65536);
	memcpy (dsp, spcfile + 0x10100, 128);
	memcpy (extraram, spcfile + 0x101c0, 64);

	FixAPU (spcfile[37]+spcfile[38]*256, spcfile[39], spcfile[41], spcfile[40], spcfile[42], spcfile[43]);
}

SPCSong::~SPCSong ()
{
	Stop ();
	CloseEmu ();
}

bool SPCSong::IsValid () const
{
	return HandleAPU != NULL;
}

bool SPCSong::IsPlaying ()
{
	return m_Status == STATE_Playing;
}

void SPCSong::Play (bool looping)
{
	int volume = (int)(snd_musicvolume * 255);

	m_Status = STATE_Stopped;
	m_Looping = true;

	m_Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, m_Stream, NULL, true);
	if (m_Channel != -1)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, volume);
		FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
		FSOUND_SetPaused (m_Channel, false);
		m_Status = STATE_Playing;
	}
}

signed char STACK_ARGS SPCSong::FillStream (FSOUND_STREAM *stream, void *buff, int len, int param)
{
	SPCSong *song = (SPCSong *)param;
	int div = 1 << (spc_stereo + !spc_8bit);
	song->EmuAPU (buff, len/div, 1);
	return TRUE;
}

bool SPCSong::LoadEmu ()
{
	HandleAPU = LoadLibraryA ("snesapu.dll");
	if (HandleAPU == NULL)
	{
		Printf ("Could not load snesapu.dll\n");
		return false;
	}

	DWORD ver, min, opt;

	if (!(SNESAPUInfo = (SNESAPUInfo_TYPE)GetProcAddress (HandleAPU, "SNESAPUInfo")) ||
		!(GetAPUData = (GetAPUData_TYPE)GetProcAddress (HandleAPU, "GetAPUData")) ||
		!(ResetAPU = (ResetAPU_TYPE)GetProcAddress (HandleAPU, "ResetAPU")) ||
		!(FixAPU = (FixAPU_TYPE)GetProcAddress (HandleAPU, "FixAPU")) ||
		!(SetAPUOpt = (SetAPUOpt_TYPE)GetProcAddress (HandleAPU, "SetAPUOpt")) ||
		!(EmuAPU = (EmuAPU_TYPE)GetProcAddress (HandleAPU, "EmuAPU")) ||
		(SNESAPUInfo (ver, min, opt), ((min>>8)&0xffff) > 0x095))
	{
		Printf ("snesapu.dll is wrong version\n");
		FreeLibrary (HandleAPU);
		HandleAPU = NULL;
		return false;
	}
	return true;
}

void SPCSong::CloseEmu ()
{
	if (HandleAPU != NULL)
	{
		FreeLibrary (HandleAPU);
		HandleAPU = NULL;
	}
}
#endif

OPLMUSSong::OPLMUSSong (int handle, int pos, int len)
{
	// No sense in using a sample rate higher than the output rate
	int rate = MIN (*opl_frequency, *snd_samplerate);

	Music = new OPLmusicBlock (handle, pos, len, rate, 16384/2);

	m_Stream = FSOUND_Stream_Create (FillStream, 16384,
		FSOUND_SIGNED | FSOUND_2D | FSOUND_MONO | FSOUND_16BITS,
		rate, (int)this);
	if (m_Stream == NULL)
	{
		Printf (PRINT_BOLD, "Could not create FMOD music stream.\n");
		delete Music;
		return;
	}
	OPL_Active = true;
}

OPLMUSSong::~OPLMUSSong ()
{
	OPL_Active = false;
	Stop ();
	if (Music != NULL)
	{
		delete Music;
	}
}

bool OPLMUSSong::IsValid () const
{
	return m_Stream != NULL;
}

void OPLMUSSong::ResetChips ()
{
	Music->ResetChips ();
}

bool OPLMUSSong::IsPlaying ()
{
	return m_Status == STATE_Playing;
}

void OPLMUSSong::Play (bool looping)
{
	int volume = (int)(snd_musicvolume * 255);

	m_Status = STATE_Stopped;
	m_Looping = looping;

	Music->SetLooping (looping);
	Music->Restart ();

	m_Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, m_Stream, NULL, true);
	if (m_Channel != -1)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, volume);
		FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
		FSOUND_SetPaused (m_Channel, false);
		m_Status = STATE_Playing;
	}
}

signed char STACK_ARGS OPLMUSSong::FillStream (FSOUND_STREAM *stream, void *buff, int len, int param)
{
	OPLMUSSong *song = (OPLMUSSong *)param;
	song->Music->ServiceStream (buff, len);
	return TRUE;
}
