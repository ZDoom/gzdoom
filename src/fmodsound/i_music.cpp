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
*/

#ifndef FMOD_333
#define FMOD_333 0
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

#include <fmod.h>

void Enable_FSOUND_IO_Loader ();
void Disable_FSOUND_IO_Loader ();

static bool MusicDown = true;

extern int _nosound;

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
		I_StopSong ((long)song);
		I_PlaySong ((long)song, song->m_Looping);
	}
}

void MIDISong::SetVolume (float volume)
{
	midiOutSetVolume ((HMIDIOUT)m_MidiStream, midivolume);
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
static void PrintMidiDevice (int id, const char *name)
{
	if (id == snd_mididevice)
	{
		Printf_Bold ("% 2d. %s\n", id, name);
	}
	else
	{
		Printf ("% 2d. %s\n", id, name);
	}
}

CCMD (snd_listmididevices)
{
	UINT id;
	MIDIOUTCAPS caps;
	MMRESULT res;

	PrintMidiDevice (-3, "DirectMusic");
	PrintMidiDevice (-2, "TiMidity++");
	if (nummididevices != 0)
	{
		PrintMidiDevice (-1, "MIDI Mapper");
		for (id = 0; id < nummididevices; id++)
		{
			res = midiOutGetDevCaps (id, &caps, sizeof(caps));
			if (res == MMSYSERR_NODRIVER)
				strcpy (caps.szPname, "<Driver not installed>");
			else if (res == MMSYSERR_NOMEM)
				strcpy (caps.szPname, "<No memory for description>");
			else if (res != MMSYSERR_NOERROR)
				continue;

			PrintMidiDevice (id, caps.szPname);
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
	char errorStr[256];

	mciGetErrorString (res, errorStr, 255);
	Printf_Bold ("An error occured while %s:\n", descr);
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

void I_PlaySong (long handle, int _looping)
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
							   (DWORD)1, (DWORD)::MidiProc,
							   (DWORD)this,
							   CALLBACK_FUNCTION)) == MMSYSERR_NOERROR)
	{
		MIDIPROPTIMEDIV timedivProp;

		timedivProp.cbStruct = sizeof(timedivProp);
		timedivProp.dwTimeDiv = midTimeDiv;
		res = midiStreamProperty (m_MidiStream, (LPBYTE)&timedivProp,
								  MIDIPROP_SET | MIDIPROP_TIMEDIV);
		if (res != MMSYSERR_NOERROR)
			MCIError (res, "setting time division");

		res = midiOutSetVolume ((HMIDIOUT)m_MidiStream, midivolume);

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
					midiOutShortMsg ((HMIDIOUT)m_MidiStream,
									 MIDI_PRGMCHANGE | (i<<8) | j);
					midiOutShortMsg ((HMIDIOUT)m_MidiStream,
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

#if FMOD_333
	m_Channel = FSOUND_Stream_Play3DAttrib (FSOUND_FREE, m_Stream, -1, volume, FSOUND_STEREOPAN, NULL, NULL);
#else
	m_Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, m_Stream, NULL, true);
#endif
	if (m_Channel != -1)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, volume);
#if !FMOD_333
		FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
		FSOUND_SetPaused (m_Channel, false);
#endif
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
#if FMOD_333
			m_Channel = FSOUND_Stream_Play3DAttrib (FSOUND_FREE, m_Stream, -1, volume, FSOUND_STEREOPAN, NULL, NULL);
#else
			m_Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, m_Stream, NULL, true);
#endif
			if (m_Channel != -1)
			{
				FSOUND_SetVolumeAbsolute (m_Channel, volume);
#if !FMOD_333
				FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
				FSOUND_SetPaused (m_Channel, false);
#endif
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

void I_PauseSong (long handle)
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

void I_ResumeSong (long handle)
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

void I_StopSong (long handle)
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

void I_UnRegisterSong (long handle)
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

long I_RegisterSong (int handle, int pos, int len)
{
	MusInfo *info = NULL;
	DWORD id;

	if (nomusic)
	{
		return 0;
	}

	lseek (handle, pos, SEEK_SET);
	read (handle, &id, 4);

	if (id == (('M')|(('U')<<8)|(('S')<<16)|((0x1a)<<24)))
	{
		// This is a mus file
#ifdef _WIN32
		if (snd_mididevice == -3)
		{
			info = new DMusSong (handle, pos, len);
		}
		else if (snd_mididevice != -2)
		{
			info = new MUSSong (handle, pos, len);
		}
		else if (!_nosound)
#endif // _WIN32
		{
			info = new TimiditySong (handle, pos, len);
		}
	}
	else if (id == (('M')|(('T')<<8)|(('h')<<16)|(('d')<<24)))
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
		else if (!_nosound)
#endif // _WIN32
		{
			info = new TimiditySong (handle, pos, len);
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
		if (info == NULL && !_nosound)	// no FSOUND => no modules/streams
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

	return info ? (long)info : 0;
}

long I_RegisterCDSong (int track, int id)
{
	MusInfo *info = new CDSong (track, id);

	if (info && !info->IsValid ())
	{
		delete info;
		info = NULL;
	}

	return info ? (long)info : 0;
}

#ifdef _WIN32
MIDISong::MIDISong ()
{
	m_Buffers = NULL;
	m_IsMUS = false;
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
	m_Module = FMUSIC_LoadSong ((char *)new FileHandle (handle, pos, len));
}

#ifdef _WIN32
DMusSong::DMusSong (int handle, int pos, int len)
: DiskName ("zmid")
{
	FILE *f = fopen (DiskName, "wb");
	if (f == NULL)
	{
		Printf_Bold ("Could not open temp music file\n");
		return;
	}

	BYTE *buf = new BYTE[len];
	bool success;

	if (lseek (handle, pos, SEEK_SET) == -1 || read (handle, buf, len) != len)
	{
		fclose (f);
		Printf_Bold ("Could not read source music file\n");
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
		Printf_Bold ("Could not create temp music file\n");
		return;
	}

	f = fopen (DiskName, "wb");
	if (f == NULL)
	{
		Printf_Bold ("Could not open temp music file\n");
		return;
	}

	BYTE *buf = new BYTE[len];

	if (lseek (handle, pos, SEEK_SET) == -1 || read (handle, buf, len) != len)
	{
		fclose (f);
		Printf_Bold ("Could not read source music file\n");
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
		Printf_Bold ("Could not write temp music file\n");
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
		Printf_Bold ("Could not create TiMidity++ kill event.\n");
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
			Printf_Bold ("Could not create a data pipe for TiMidity++.\n");
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
				Printf_Bold ("Could not create FMOD music stream.\n");
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
			Printf_Bold ("If your soundcard cannot play more than one\n"
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
		Printf_Bold ("Please set the timidity_exe cvar to the location of TiMidity++\n");
		return false;
	}
	if (pathLen > MAX_PATH)
	{
		Printf_Bold ("TiMidity++ is in a path too long\n");
		return false;
	}

	diskFile = CreateFile (foundPath, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (diskFile == INVALID_HANDLE_VALUE)
	{
		Printf_Bold ("Could not access %s\n", foundPath);
		return false;
	}
	fileLen = GetFileSize (diskFile, NULL);
	mapping = CreateFileMapping (diskFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (mapping == NULL)
	{
		Printf_Bold ("Could not create mapping for %s\n", foundPath);
		CloseHandle (diskFile);
		return false;
	}
	exeBase = (const BYTE *)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
	if (exeBase == NULL)
	{
		Printf_Bold ("Could not map %s\n", foundPath);
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
		Printf_Bold ("Error reading %s\n", foundPath);
	}
	if (!good)
	{
		Printf_Bold ("ZDoom requires a special version of TiMidity++\n");
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

	Printf_Bold ("Could not run timidity with the command line:\n%s\n"
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
		Printf_Bold ("Could not fork when trying to start timidity\n");
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
bool I_QrySongPlaying (long handle)
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
	if (m_Status != STATE_Stopped && FMUSIC_IsPlaying (m_Module))
	{
		if (!m_Looping && FMUSIC_IsFinished (m_Module))
		{
			Stop();
			return false;
		}
		return true;
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
bool I_SetSongPosition (long handle, int order)
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
