#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <ctype.h>
#include <assert.h>

#include "doomtype.h"
#include "m_argv.h"
#include "i_music.h"
#include "w_wad.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "z_zone.h"
#include "mid2strm.h"
#include "mus2strm.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_cd.h"

#include <fmod.h>

EXTERN_CVAR (Int, snd_musicvolume)
extern int _nosound;

class MusInfo
{
public:
	MusInfo () { m_Status = STATE_Stopped; }
	virtual ~MusInfo () {}
	virtual void SetVolume (float volume) = 0;
	virtual void Play (bool looping) = 0;
	virtual void Pause () = 0;
	virtual void Resume () = 0;
	virtual void Stop () = 0;
	virtual bool IsPlaying () = 0;
	virtual bool IsMIDI () = 0;
	virtual bool IsValid () = 0;
	virtual bool SetPosition (int order) { return false; }

	enum EState
	{
		STATE_Stopped,
		STATE_Playing,
		STATE_Paused
	} m_Status;
	bool m_Looping;
};

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
	bool IsMIDI () { return true; }
	bool IsValid () { return m_Buffers != NULL; }
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

class MUSSong : public MIDISong
{
public:
	MUSSong (int handle, int pos, int len);
};

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
	bool IsMIDI () { return false; }
	bool IsValid () { return m_Module != NULL; }
	bool SetPosition (int order);

protected:
	FMUSIC_MODULE *m_Module;
};

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
	bool IsMIDI () { return false; }
	bool IsValid () { return m_Stream != NULL; }

protected:
	FSOUND_STREAM *m_Stream;
	int m_Channel;
	int m_Length;
	int m_LastPos;

	static int m_Volume;
};

int StreamSong::m_Volume = 255;

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
	bool IsMIDI () { return false; }
	bool IsValid () { return m_Inited; }

protected:
	CDSong () { m_Inited = false; }

	int m_Track;
	bool m_Inited;
};

class CDDAFile : public CDSong
{
public:
	CDDAFile (int handle, int pos, int len);
};

static MusInfo *currSong;
static HANDLE	BufferReturnEvent;
static int		nomusic = 0;
static int		musicvolume;
static DWORD	midivolume;
static DWORD	nummididevices;
static bool		nummididevicesset;
static UINT		mididevice;

CUSTOM_CVAR (Int, snd_mididevice, -1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	UINT oldmididev = mididevice;

	if (!nummididevicesset)
		return;

	if ((*var >= (signed)nummididevices) || (*var < -1.0f))
	{
		Printf ("ID out of range. Using MIDI mapper.\n");
		var = -1;
		return;
	}
	else if (*var < 0)
	{
		mididevice = MIDI_MAPPER;
	}
	else
	{
		mididevice = *var;
	}

	// If a song is playing, move it to the new device.
	if (oldmididev != mididevice && currSong)
	{
		MusInfo *song = currSong;
		I_StopSong ((long)song);
		I_PlaySong ((long)song, song->m_Looping);
	}
}

void I_SetMIDIVolume (float volume)
{
	if (currSong && currSong->IsMIDI ())
	{
		currSong->SetVolume (volume);
	}
	else
	{
		DWORD wooba = (DWORD)(volume * 0xffff) & 0xffff;
		midivolume = (wooba << 16) | wooba;
	}
}

void MIDISong::SetVolume (float volume)
{
	DWORD wooba = (DWORD)(volume * 0xffff) & 0xffff;
	midivolume = (wooba << 16) | wooba;
	midiOutSetVolume ((HMIDIOUT)m_MidiStream, midivolume);
}

void I_SetMusicVolume (int volume)
{
	if (volume)
	{
		// Internal state variable.
		musicvolume = volume;
		// Now set volume on output device.
		if (currSong && !currSong->IsMIDI ())
			currSong->SetVolume ((float)volume / 64.0f);
	}
}

void MODSong::SetVolume (float volume)
{
	FMUSIC_SetMasterVolume (m_Module, (int)(volume * 256));
}

void StreamSong::SetVolume (float volume)
{
	m_Volume = (int)(volume * 255);
	if (m_Channel)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, m_Volume);
	}
}

CCMD (snd_listmididevices)
{
	UINT id;
	MIDIOUTCAPS caps;
	MMRESULT res;

	if (nummididevices)
	{
		Printf ("-1. MIDI Mapper\n");
	}
	else
	{
		Printf ("No MIDI devices installed.\n");
		return;
	}

	for (id = 0; id < nummididevices; id++)
	{
		res = midiOutGetDevCaps (id, &caps, sizeof(caps));
		if (res == MMSYSERR_NODRIVER)
			strcpy (caps.szPname, "<Driver not installed>");
		else if (res == MMSYSERR_NOMEM)
			strcpy (caps.szPname, "<No memory for description>");
		else if (res != MMSYSERR_NOERROR)
			continue;

		Printf ("% 2d. %s\n", id, caps.szPname);
	}
}

void I_InitMusic (void)
{
	static bool setatterm = false;

	Printf ("I_InitMusic\n");
	
	nummididevices = midiOutGetNumDevs ();
	nummididevicesset = true;
	snd_mididevice.Callback ();
	snd_musicvolume.Callback ();

	nomusic = !!Args.CheckParm("-nomusic") || !!Args.CheckParm("-nosound") || !nummididevices;

	if (!nomusic)
	{
		if ((BufferReturnEvent = CreateEvent (NULL, FALSE, FALSE, NULL)) == NULL)
		{
			Printf ("Could not create MIDI callback event.\nMIDI music will be disabled.\n");
			nomusic = true;
		}
	}

	if (!setatterm)
	{
		setatterm = true;
		atterm (I_ShutdownMusic);
	}
}


void STACK_ARGS I_ShutdownMusic(void)
{
	if (currSong)
	{
		S_StopMusic (true);
		/*
		I_UnRegisterSong ((long)currSong);
		currSong = NULL;
		*/
		assert (currSong == NULL);
	}
	if (BufferReturnEvent)
	{
		CloseHandle (BufferReturnEvent);
		BufferReturnEvent = NULL;
	}
}


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
		{
			int i, j;

			DPrintf ("MIDI uses instruments:\n");
			for (i = j = 0; i < 127; i++)
				if (UsedPatches[i])
				{
					DPrintf (" %d", i);
					midiOutShortMsg ((HMIDIOUT)m_MidiStream,
									 MIDI_PRGMCHANGE | (i<<8) | j);
					midiOutShortMsg ((HMIDIOUT)m_MidiStream,
									 MIDI_NOTEON | (60<<8) | (1<<16) | j);
					if (++j == 10)
					{
						Sleep (250);
						for (j = 0; j < 10; j++)
							midiOutShortMsg ((HMIDIOUT)m_MidiStream,
											 MIDI_NOTEOFF | (60<<8) | (64<<16) | j);
						j = 0;
					}
				}
			if (j > 0)
			{
				Sleep (250);
				for (i = 0; i < j; i++)
					midiOutShortMsg ((HMIDIOUT)m_MidiStream,
									 MIDI_NOTEOFF | (60<<8) | (64<<16) | i);
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
		if (*snd_mididevice != -1)
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

void MODSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (FMUSIC_PlaySong (m_Module))
	{
		FMUSIC_SetMasterVolume (m_Module, musicvolume << 2);
		m_Status = STATE_Playing;
	}
}

void StreamSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	m_Channel = FSOUND_Stream_Play (FSOUND_FREE, m_Stream);
	if (m_Channel != -1)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, m_Volume);
		FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
		m_Status = STATE_Playing;
		m_LastPos = 0;
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

void MIDISong::Pause ()
{
	if (m_Status == STATE_Playing &&
		m_MidiStream &&
		midiStreamPause (m_MidiStream) == MMSYSERR_NOERROR)
	{
		m_Status = STATE_Paused;
	}
}

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
	if (m_Status == STATE_Playing)
	{
		if (FSOUND_Stream_SetPaused (m_Stream, TRUE))
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

void MIDISong::Resume ()
{
	if (m_Status == STATE_Paused &&
		m_MidiStream &&
		midiStreamRestart (m_MidiStream) == MMSYSERR_NOERROR)
	{
		m_Status = STATE_Playing;
	}
}

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
	if (m_Status == STATE_Paused)
	{
		if (FSOUND_Stream_SetPaused (m_Stream, FALSE))
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

MIDISong::~MIDISong ()
{
	Stop ();
	if (m_IsMUS)
		mus2strmCleanup ();
	else
		mid2strmCleanup ();
	m_Buffers = NULL;
}

MODSong::~MODSong ()
{
	Stop ();
	FMUSIC_FreeSong (m_Module);
	m_Module = NULL;
}

StreamSong::~StreamSong ()
{
	Stop ();
	FSOUND_Stream_Close (m_Stream);
	m_Stream = NULL;
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
		info = new MUSSong (handle, pos, len);
	}
	else if (id == (('M')|(('T')<<8)|(('h')<<16)|(('d')<<24)))
	{
		// This is a midi file
		info = new MIDISong (handle, pos, len);
	}
	else if (id == (('R')|(('I')<<8)|(('F')<<16)|(('F')<<24)))
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
	else if (!_nosound)	// no FSOUND => no modules/mp3s
	{
		// First try loading it as MOD, then as MP3
		info = new MODSong (handle, pos, len);
		if (!info->IsValid ())
		{
			delete info;
			info = new StreamSong (handle, pos, len);
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

MODSong::MODSong (int handle, int pos, int len)
{
	m_Module = FMUSIC_LoadSong ((char *)new FileHandle (handle, pos, len));
}

StreamSong::StreamSong (int handle, int pos, int len)
{
	m_Channel = -1;
	m_Stream = FSOUND_Stream_OpenFile ((char *)new FileHandle (handle, pos, len),
		FSOUND_LOOP_NORMAL|FSOUND_NORMAL, 0);
	if (m_Stream != NULL)
	{
		m_Length = FSOUND_Stream_GetLength (m_Stream);
	}
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

bool MIDISong::IsPlaying ()
{
	return m_Looping ? true : m_Status != STATE_Stopped;
}

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
