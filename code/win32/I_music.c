#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "m_argv.h"
#include "i_music.h"
#include "w_wad.h"
#include "c_consol.h"
#include "c_dispch.h"
#include "z_zone.h"
#include "mid2strm.h"
#include "mus2strm.h"

#include "../midas/include/midasdll.h"

#define TYPE_MUS		0
#define TYPE_MIDI		1
#define TYPE_MOD		2

#define STATE_STOPPED	0
#define STATE_PLAYING	1
#define STATE_PAUSED	2

struct MusInfo {
	int type;
	int status;
	PSTREAMBUF currBuffer;
	enum {
		cb_play,
		cb_die
	} callbackStatus;
	union {
		MIDASmodulePlayHandle handle;
		HMIDISTRM midiStream;
	};
	union {
		MIDASmodule module;
		PSTREAMBUF buffers;
	};
	BOOL looping;
};
typedef struct MusInfo info_t;

static info_t  *currSong;
static HANDLE	BufferReturnEvent;
static int		nomusic = 0;
static char		modName[512];
static int		musicdies=-1;
static int		musicvolume;
static DWORD	midivolume;
static cvar_t  *snd_mididevice;
static DWORD	nummididevices;
static UINT		mididevice;

void I_SetMIDIVolume (float volume)
{
	DWORD wooba = (DWORD)(volume * 0xffff) & 0xffff;
	midivolume = (wooba << 16) | wooba;
	if (currSong && (currSong->type == TYPE_MIDI || currSong->type == TYPE_MUS))
		midiOutSetVolume ((HMIDIOUT)currSong->midiStream, midivolume);
}

void I_SetMusicVolume (int volume)
{
	if (volume != 127) {
		// Internal state variable.
		musicvolume = volume;
		// Now set volume on output device.
		if (currSong && currSong->type == TYPE_MOD)
			MIDASsetMusicVolume (currSong->handle, musicvolume);
	}
}

static void ChangeMIDIDevice (cvar_t *var)
{
	UINT oldmididev = mididevice;

	if (((int)var->value >= (signed)nummididevices) || (var->value < -1.0f)) {
		Printf ("ID out of range. Using MIDI mapper.\n");
		SetCVarFloat (var, -1.0f);
		return;
	} else if (var->value < 0) {
		mididevice = MIDI_MAPPER;
	} else {
		mididevice = (int)var->value;
	}

	// If a song is playing, move it to the new device.
	if (oldmididev != mididevice && currSong) {
		info_t *song = currSong;
		I_StopSong ((int)song);
		I_PlaySong ((int)song, song->looping);
	}
}

static void ListMIDIDevices (void *plyr, int argc, char **argv)
{
	UINT id;
	MIDIOUTCAPS caps;
	MMRESULT res;

	if (nummididevices) {
		Printf ("-1. MIDI Mapper\n");
	} else {
		Printf ("No MIDI devices installed.\n");
		return;
	}

	for (id = 0; id < nummididevices; id++) {
		res = midiOutGetDevCaps (id, &caps, sizeof(caps));
		if (res == MMSYSERR_NODRIVER) {
			strcpy (caps.szPname, "<Driver not installed>");
		} else if (res == MMSYSERR_NOMEM) {
			strcpy (caps.szPname, "<No memory for description>");
		} else if (res != MMSYSERR_NOERROR) {
			continue;
		}
		Printf ("% 2d. %s\n", id, caps.szPname);
	}
}

void I_InitMusic (void)
{
	char *temp;

	Printf ("I_InitMusic\n");
	
	snd_mididevice = cvar ("snd_mididevice", "-1", CVAR_ARCHIVE|CVAR_CALLBACK);
	snd_mididevice->u.callback = ChangeMIDIDevice;
	nummididevices = midiOutGetNumDevs ();
	ChangeMIDIDevice (snd_mididevice);
	C_RegisterCommand ("snd_listmididevices", ListMIDIDevices);

	nomusic = !!M_CheckParm("-nomusic") || !!M_CheckParm("-nosound") || !nummididevices;

	if (!nomusic) {
		if ((BufferReturnEvent = CreateEvent (NULL, FALSE, FALSE, NULL)) == NULL) {
			Printf ("Could not create MIDI callback event.\nMIDI music will be disabled.\n");
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

	atexit (I_ShutdownMusic);
}


void I_ShutdownMusic(void)
{
	if (currSong) {
		I_UnRegisterSong ((int)currSong);
		currSong = NULL;
	}
	remove (modName);
	if (BufferReturnEvent)
		CloseHandle (BufferReturnEvent);
}


static void MCIError (MMRESULT res, const char *descr)
{
	char errorStr[256];

	mciGetErrorString (res, errorStr, 255);
	Printf_Bold ("An error occured while %s:\n", descr);
	Printf ("%s\n", errorStr);
}

static void UnprepareMIDIHeaders (info_t *info)
{
	PSTREAMBUF buffer = info->buffers;

	while (buffer) {
		if (buffer->prepared) {
			MMRESULT res = midiOutUnprepareHeader ((HMIDIOUT)info->midiStream,
										&buffer->midiHeader, sizeof(buffer->midiHeader));
			if (res != MMSYSERR_NOERROR)
				MCIError (res, "unpreparing headers");
			else
				buffer->prepared = false;
		}

		buffer = buffer->pNext;
	}
}

static BOOL PrepareMIDIHeaders (info_t *info)
{
	MMRESULT res;
	PSTREAMBUF buffer = info->buffers;

	while (buffer) {
		if (!buffer->prepared) {
			memset (&buffer->midiHeader, 0, sizeof(MIDIHDR));
			buffer->midiHeader.lpData = buffer->pBuffer;
			buffer->midiHeader.dwBufferLength = CB_STREAMBUF;
			buffer->midiHeader.dwBytesRecorded = CB_STREAMBUF - buffer->cbLeft;
			res = midiOutPrepareHeader ((HMIDIOUT)info->midiStream,
										&buffer->midiHeader, sizeof(MIDIHDR));
			if (res != MMSYSERR_NOERROR) {
				MCIError (res, "preparing headers");
				Printf ("stream: %08p\n", info->midiStream);
				Printf ("data: %08p\n", buffer->midiHeader.lpData);
				Printf ("bufflen: %d\n", buffer->midiHeader.dwBufferLength);
				Printf ("recorded: %d\n", buffer->midiHeader.dwBytesRecorded);
				Printf ("sizeof: %d\n", sizeof(MIDIHDR));
				UnprepareMIDIHeaders (info);
				return false;
			} else
				buffer->prepared = true;
		}
		buffer = buffer->pNext;
	}

	return true;
}

static void SubmitMIDIBuffer (info_t *info)
{
	MMRESULT res;

	res = midiStreamOut (info->midiStream,
						 &info->currBuffer->midiHeader, sizeof(MIDIHDR));

	if (res != MMSYSERR_NOERROR)
		MCIError (res, "sending MIDI stream");

	info->currBuffer = info->currBuffer->pNext;
	if (info->currBuffer == NULL && info->looping)
		info->currBuffer = info->buffers;
}

static void AllChannelsOff (info_t *info)
{
	int i;

	for (i = 0; i < 16; i++)
		midiOutShortMsg ((HMIDIOUT)info->midiStream, MIDI_NOTEOFF | i | (64<<16) | (60<<8));
	Sleep (1);
}

static void CALLBACK MidiProc (HMIDIIN hMidi, UINT uMsg, info_t *info,
							  DWORD dwParam1, DWORD dwParam2 )
{
	switch (uMsg) {
		case MOM_DONE:
			if (info->callbackStatus == cb_die)
				SetEvent (BufferReturnEvent);
			else {
				if (info->currBuffer == info->buffers) {
					// Stop all notes before restarting the song
					// in case any are left hanging.
					AllChannelsOff (info);
				} else if (info->currBuffer == NULL) {
					SetEvent (BufferReturnEvent);
					return;
				}
				SubmitMIDIBuffer (info);
			}
			break;
	}
}

void I_PlaySong (int handle, int _looping)
{
	info_t *info = (info_t *)handle;

	if (!info || nomusic)
		return;

	info->status = STATE_STOPPED;
	info->looping = _looping;

	switch (info->type) {
		case TYPE_MUS:
		case TYPE_MIDI: {
			MMRESULT res;

			// note: midiStreamOpen changes mididevice if it's set to MIDI_MAPPER
			// (interesting undocumented behavior)
			if ((res = midiStreamOpen (&info->midiStream,
									   &mididevice,
									   (DWORD)1, (DWORD)MidiProc,
									   (DWORD)info,
									   CALLBACK_FUNCTION)) == MMSYSERR_NOERROR)
			{
				MIDIPROPTIMEDIV timedivProp;

				timedivProp.cbStruct = sizeof(timedivProp);
				timedivProp.dwTimeDiv = midTimeDiv;
				res = midiStreamProperty (info->midiStream, (LPBYTE)&timedivProp,
										  MIDIPROP_SET | MIDIPROP_TIMEDIV);
				if (res != MMSYSERR_NOERROR)
					MCIError (res, "setting time division");

				res = midiOutSetVolume ((HMIDIOUT)info->midiStream, midivolume);

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
						if (UsedPatches[i]) {
							DPrintf (" %d", i);
							midiOutShortMsg ((HMIDIOUT)info->midiStream,
											 MIDI_PRGMCHANGE | (i<<8) | j);
							midiOutShortMsg ((HMIDIOUT)info->midiStream,
											 MIDI_NOTEON | (60<<8) | (1<<16) | j);
							if (++j == 10) {
								Sleep (250);
								for (j = 0; j < 10; j++)
									midiOutShortMsg ((HMIDIOUT)info->midiStream,
													 MIDI_NOTEOFF | (60<<8) | (64<<16) | j);
								j = 0;
							}
						}
					if (j > 0) {
						Sleep (250);
						for (i = 0; i < j; i++)
							midiOutShortMsg ((HMIDIOUT)info->midiStream,
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

				if (PrepareMIDIHeaders (info)) {
					info->callbackStatus = cb_play;
					info->currBuffer = info->buffers;
					SubmitMIDIBuffer (info);
					res = midiStreamRestart (info->midiStream);
					if (res == MMSYSERR_NOERROR)
						info->status = STATE_PLAYING;
					else {
						MCIError (res, "starting playback");
						UnprepareMIDIHeaders (info);
						midiStreamClose (info->midiStream);
						info->midiStream = NULL;
					}
				} else {
					UnprepareMIDIHeaders (info);
					midiStreamClose (info->midiStream);
					info->midiStream = NULL;
				}
			} else {
				MCIError (res, "opening MIDI stream");
				if (snd_mididevice->value != -1) {
					Printf ("Trying again with MIDI mapper\n");
					SetCVarFloat (snd_mididevice, -1.0f);
				}
			}

			break;
			}
		case TYPE_MOD:
			if (info->handle = MIDASplayModule (info->module, _looping)) {
				MIDASsetMusicVolume (info->handle, musicvolume);
				info->status = STATE_PLAYING;
			}
			break;
	}
	currSong = info;
}

void I_PauseSong (int handle)
{
	info_t *info = (info_t *)handle;

	if (!info || info->status != STATE_PLAYING)
		return;

	switch (info->type) {
		case TYPE_MUS:
		case TYPE_MIDI:
			if (info->midiStream && midiStreamPause (info->midiStream) == MMSYSERR_NOERROR)
				info->status = STATE_PAUSED;
			break;
		case TYPE_MOD:
			break;
	}
}

void I_ResumeSong (int handle)
{
	info_t *info = (info_t *)handle;

	if (!info || info->status != STATE_PAUSED)
		return;

	switch (info->type) {
		case TYPE_MUS:
		case TYPE_MIDI:
			if (info->midiStream && midiStreamRestart (info->midiStream) == MMSYSERR_NOERROR)
				info->status = STATE_PLAYING;
			break;
		case TYPE_MOD:
			break;
	}
}

void I_StopSong (int handle)
{
	info_t *info = (info_t *)handle;
	
	if (!info || info->status == STATE_STOPPED)
		return;

	info->status = STATE_STOPPED;

	switch (info->type) {
		case TYPE_MUS:
		case TYPE_MIDI:
			info->callbackStatus = cb_die;
			if (info->midiStream) {
				midiStreamStop (info->midiStream);
				WaitForSingleObject (BufferReturnEvent, INFINITE);
				midiOutReset ((HMIDIOUT)info->midiStream);
				UnprepareMIDIHeaders (info);
				midiStreamClose (info->midiStream);
				info->midiStream = NULL;
			}
			break;
		case TYPE_MOD:
			MIDASstopModule (info->handle);
			info->handle = 0;
			break;
	}
	if (info == currSong)
		currSong = NULL;
}

void I_UnRegisterSong (int handle)
{
	info_t *info = (info_t *)handle;

	if (info) {
		I_StopSong (handle);
		switch (info->type) {
			case TYPE_MUS:
				mus2strmCleanup ();
				info->buffers = NULL;
				break;
			case TYPE_MIDI:
				mid2strmCleanup ();
				info->buffers = NULL;
				break;
			case TYPE_MOD:
				MIDASfreeModule (info->module);
				info->module = NULL;
				break;
		}
		free (info);
	}
}

int I_RegisterSong (void *data, int musicLen)
{
	int ow = 2;
	info_t *info;

	if (!(info = malloc (sizeof(info_t))))
		return 0;

	info->status = STATE_STOPPED;

	if (*(int *)data == (('M')|(('U')<<8)|(('S')<<16)|((0x1a)<<24))) {
		// This is a mus file
		info->type = TYPE_MUS;
		if (!(info->buffers = mus2strmConvert ((LPBYTE)data, musicLen))) {
			free (info);
			return 0;
		}
	} else if (*(int *)data == (('M')|(('T')<<8)|(('h')<<16)|(('d')<<24))) {
		// This is a midi file
		info->type = TYPE_MIDI;
		if (!(info->buffers = mid2strmConvert ((LPBYTE)data, musicLen))) {
			free (info);
			return 0;
		}
	} else {
		FILE *f;

		if ((f = fopen (modName, "wb")) == NULL) {
			Printf ("Unable to open temporary music file %s\n", modName);
			free (info);
			return 0;
		}
		if (fwrite (data, musicLen, 1, f) != 1) {
			fclose (f);
			Printf ("Unable to write temporary music file\n");
			free (info);
			return 0;
		}
		fclose(f);

		if ( (info->module = MIDASloadModule (modName)) ) {
			// This is a module
			info->type = TYPE_MOD;
		} else {
			// This is not any known music format
			// (or could not load mod)
			free (info);
			info = NULL;
			Printf ("midas failed to load song:\n%s\n", MIDASgetErrorMessage(MIDASgetLastError()));
		}
		remove (modName);
	}

	return (int)info;
}

// Is the song playing?
int I_QrySongPlaying (int handle)
{
	info_t *info = (info_t *)handle;

	if (!info)
		return 0;
	else if (info->looping == 1)
		return 1;
	else
		return info->status;
}