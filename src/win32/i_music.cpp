#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdexcpt.h>

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

#include "../midas/include/midasdll.h"

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
	MIDISong (void *data, int len);
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
	virtual bool IsMUS () { return false; }

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
	MUSSong (void *data, int len);
protected:
	bool IsMUS () { return true; }
};

class MODSong : public MusInfo
{
public:
	MODSong (void *data, int len);
	~MODSong ();
	void SetVolume (float volume);
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () { return false; }
	bool IsValid () { return m_Module != NULL; }

protected:
	MIDASmodulePlayHandle m_Handle;
	MIDASmodule m_Module;
};

static MusInfo *currSong;
static HANDLE	BufferReturnEvent;
static int		nomusic = 0;
static char		modName[512];
static int		musicvolume;
static DWORD	midivolume;
static DWORD	nummididevices;
static bool		nummididevicesset;
static UINT		mididevice;

BEGIN_CUSTOM_CVAR (snd_mididevice, "-1", CVAR_ARCHIVE)
{
	UINT oldmididev = mididevice;

	if (!nummididevicesset)
		return;

	if (((int)var.value >= (signed)nummididevices) || (var.value < -1.0f))
	{
		Printf (PRINT_HIGH, "ID out of range. Using MIDI mapper.\n");
		var.Set (-1.0f);
		return;
	}
	else if (var.value < 0)
	{
		mididevice = MIDI_MAPPER;
	}
	else
	{
		mididevice = (int)var.value;
	}

	// If a song is playing, move it to the new device.
	if (oldmididev != mididevice && currSong)
	{
		MusInfo *song = currSong;
		I_StopSong ((int)song);
		I_PlaySong ((int)song, song->m_Looping);
	}
}
END_CUSTOM_CVAR (snd_mididevice)

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
	if (volume != 127)
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
	MIDASsetMusicVolume (m_Handle, (int)(volume * 64));
}

BEGIN_COMMAND (snd_listmididevices)
{
	UINT id;
	MIDIOUTCAPS caps;
	MMRESULT res;

	if (nummididevices)
	{
		Printf (PRINT_HIGH, "-1. MIDI Mapper\n");
	}
	else
	{
		Printf (PRINT_HIGH, "No MIDI devices installed.\n");
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

		Printf (PRINT_HIGH, "% 2d. %s\n", id, caps.szPname);
	}
}
END_COMMAND (snd_listmididevices)

void I_InitMusic (void)
{
	char *temp;

	Printf (PRINT_HIGH, "I_InitMusic\n");
	
	nummididevices = midiOutGetNumDevs ();
	nummididevicesset = true;
	snd_mididevice.Callback ();

	nomusic = !!Args.CheckParm("-nomusic") || !!Args.CheckParm("-nosound") || !nummididevices;

	if (!nomusic)
	{
		if ((BufferReturnEvent = CreateEvent (NULL, FALSE, FALSE, NULL)) == NULL)
		{
			Printf (PRINT_HIGH, "Could not create MIDI callback event.\nMIDI music will be disabled.\n");
			nomusic = true;
		}
	}

	/* Create temporary file name for MIDAS */
	temp = getenv("TEMP");
	if (temp == NULL)
		temp = ".";
	strcpy (modName, temp);
	FixPathSeperator (modName);

	while (modName[strlen(modName)-1] == '/')
		modName[strlen(modName)-1] = 0;

	strcat (modName, "/doommus.tmp");

	atterm (I_ShutdownMusic);
}


void STACK_ARGS I_ShutdownMusic(void)
{
	if (currSong)
	{
		I_UnRegisterSong ((int)currSong);
		currSong = NULL;
	}
	remove (modName);
	if (BufferReturnEvent)
		CloseHandle (BufferReturnEvent);
}


void MIDISong::MCIError (MMRESULT res, const char *descr)
{
	char errorStr[256];

	mciGetErrorString (res, errorStr, 255);
	Printf_Bold ("An error occured while %s:\n", descr);
	Printf (PRINT_HIGH, "%s\n", errorStr);
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

void I_PlaySong (int handle, int _looping)
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
		if (snd_mididevice.value != -1)
		{
			Printf (PRINT_HIGH, "Trying again with MIDI mapper\n");
			snd_mididevice.Set (-1.0f);
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

	if ( (m_Handle = MIDASplayModule (m_Module, looping)) )
	{
		MIDASsetMusicVolume (m_Handle, musicvolume);
		m_Status = STATE_Playing;
	}
}

void I_PauseSong (int handle)
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
		// FIXME
	}
}

void I_ResumeSong (int handle)
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
		// FIXME
	}
}

void I_StopSong (int handle)
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
		m_Status = STATE_Stopped;
		if (m_CallbackStatus != cb_dead)
			m_CallbackStatus = cb_die;
		midiStreamStop (m_MidiStream);
		WaitForSingleObject (BufferReturnEvent, 5000);
		midiOutReset ((HMIDIOUT)m_MidiStream);
		UnprepareHeaders ();
		midiStreamClose (m_MidiStream);
		m_MidiStream = NULL;
	}
}

void MODSong::Stop ()
{
	if (m_Status != STATE_Stopped)
	{
		m_Status = STATE_Stopped;
		MIDASstopModule (m_Handle);
		m_Handle = 0;
	}
}

void I_UnRegisterSong (int handle)
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
	if (IsMUS ())
		mus2strmCleanup ();
	else
		mid2strmCleanup ();
	m_Buffers = NULL;
}

MODSong::~MODSong ()
{
	Stop ();
	MIDASfreeModule (m_Module);
	m_Module = 0;
}

int I_RegisterSong (void *data, int musicLen)
{
	MusInfo *info;

	if (*(int *)data == (('M')|(('U')<<8)|(('S')<<16)|((0x1a)<<24)))
	{
		// This is a mus file
		info = new MUSSong (data, musicLen);
	}
	else if (*(int *)data == (('M')|(('T')<<8)|(('h')<<16)|(('d')<<24)))
	{
		// This is a midi file
		info = new MIDISong (data, musicLen);
	}
	else if (!_nosound)	// no MIDAS => no MODs
	{
		// This is probably a module
		info = new MODSong (data, musicLen);
	}

	if (info && !info->IsValid ())
	{
		delete info;
		info = NULL;
	}

	return info ? (int)info : 0;
}

MIDISong::MIDISong ()
{
	m_Buffers = NULL;
}

MIDISong::MIDISong (void *data, int len)
{
	m_Buffers = NULL;
	if (nummididevices > 0)
		m_Buffers = mid2strmConvert ((LPBYTE)data, len);
}

MUSSong::MUSSong (void *data, int len) : MIDISong ()
{
	if (nummididevices > 0)
		m_Buffers = mus2strmConvert ((LPBYTE)data, len);
}

MODSong::MODSong (void *data, int len)
{
	m_Handle = 0;
	m_Module = NULL;

	FILE *f;

	if ((f = fopen (modName, "wb")) == NULL)
	{
		Printf (PRINT_HIGH, "Unable to open temporary music file %s\n", modName);
		return;
	}
	if (fwrite (data, len, 1, f) != 1)
	{
		fclose (f);
		remove (modName);
		Printf (PRINT_HIGH, "Unable to write temporary music file\n");
		return;
	}
	fclose (f);

	if ((m_Module = MIDASloadModule (modName)) == NULL)
	{
		// This is not any known music format
		// (or could not load mod)
		Printf (PRINT_HIGH, "MIDAS failed to load song:\n%s\n",
			MIDASgetErrorMessage (MIDASgetLastError()));
	}
	remove (modName);
}

// Is the song playing?
bool I_QrySongPlaying (int handle)
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
	return m_Looping ? true : m_Status != STATE_Stopped;
}
