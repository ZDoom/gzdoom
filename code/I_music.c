#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "m_argv.h"
#include "i_music.h"
#include "w_wad.h"
#include "c_consol.h"

#include "../midas/include/midasdll.h"

#define TYPE_MIDI		0
#define TYPE_MOD		1
#define TYPE_CD			2

#define STATE_STOPPED	0
#define STATE_PLAYING	1
#define STATE_PAUSED	2

struct MusInfo {
	int type;
	int status;
	union {
		MIDASmodule module;
		MCIDEVICEID mciDevice;
	};
	union {
		DWORD unpausePosition;
		int cdTrack;
		MIDASmodulePlayHandle handle;
	};
	char *filename;
	BOOL looping;
};
typedef struct MusInfo info_t;

static info_t *currSong;

extern HWND Window;

/* Music using Windows MIDI device - yuck */

static int		nomusic = 0;
static char		musName[512];
static char		midName[512];
static int		musicdies=-1;
static int		musicvolume;

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

void I_InitMusic (void)
{
	char *temp;

	Printf ("I_InitMusic\n");
	
	nomusic = !!M_CheckParm("-nomusic") || !!M_CheckParm("-nosound");

	/* Create temporary file names: */

	temp = getenv("TEMP");
	if ( temp == NULL )
		temp = ".";
	strcpy(musName, temp);

	while ( musName[strlen(musName)-1] == '\\' )
		musName[strlen(musName)-1] = 0;

	strcat(musName, "\\");
	strcpy(midName, musName);

	strcat(musName, "doomtemp.mus");
	strcat(midName, "doomtemp.mid");
}


void I_ShutdownMusic(void)
{
	if (currSong) {
		I_UnRegisterSong ((int)currSong);
		currSong = NULL;
	}
	remove (midName);
	remove (musName);
}



int SendMCI(MCIDEVICEID device, UINT msg, DWORD command, DWORD param)
{
	int res;

	res = mciSendCommand(device, msg, command, param);
	if (res) {
		char errorStr[256];

		mciGetErrorString(res, errorStr, 255);
		Printf_Bold ("MCI error:\n");
		Printf ("%s\n", errorStr);
	}
	
	return res;
}


void I_PlaySong (int handle, int _looping)
{
	info_t *info = (info_t *)handle;

	if (!info || nomusic)
		return;

	info->status = STATE_STOPPED;
	info->looping = _looping;

	switch (info->type) {
		case TYPE_MIDI: {
			MCI_OPEN_PARMS openParms;
			MCI_PLAY_PARMS playParms;

			openParms.lpstrDeviceType = "sequencer";
			openParms.lpstrElementName = info->filename;
			if (SendMCI (0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_ELEMENT, (DWORD)&openParms))
				return;

			playParms.dwCallback = (DWORD)Window;
			if(SendMCI (openParms.wDeviceID, MCI_PLAY, MCI_NOTIFY, (DWORD)&playParms)) {
				SendMCI (openParms.wDeviceID, MCI_CLOSE, 0, (DWORD)NULL);
				return;
			}

			info->mciDevice = openParms.wDeviceID;
			info->status = STATE_PLAYING;
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

void I_RestartSong (void)
{
	MCI_PLAY_PARMS playParms;

	if (!currSong ||
		currSong->type != TYPE_MIDI ||
		currSong->status != STATE_PLAYING)
		return;

	if (!currSong->looping) {
		I_StopSong ((int)currSong);
		return;
	}

	playParms.dwFrom = 0;
	playParms.dwCallback = (DWORD)Window;
	if (SendMCI (currSong->mciDevice, MCI_PLAY, MCI_NOTIFY | MCI_FROM, (DWORD)(LPVOID)&playParms)) {
		SendMCI (currSong->mciDevice, MCI_CLOSE, 0, (DWORD)NULL);
		currSong->mciDevice = 0;
		currSong = NULL;
	}
}

void I_PauseSong (int handle)
{
	info_t *info = (info_t *)handle;

	if (!info || info->status != STATE_PLAYING)
		return;

	switch (info->type) {
		case TYPE_MIDI: {
			MCI_SET_PARMS setParms;
			MCI_STATUS_PARMS statusParms;

			setParms.dwTimeFormat = MCI_FORMAT_MILLISECONDS;
			if (SendMCI (info->mciDevice, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)&setParms))
				return;

			statusParms.dwCallback = (DWORD)Window;
			statusParms.dwItem = MCI_STATUS_POSITION;
			if (SendMCI (info->mciDevice, MCI_STATUS, MCI_STATUS_ITEM, (DWORD)&statusParms))
				return;

			info->unpausePosition = statusParms.dwReturn;
			SendMCI (info->mciDevice, MCI_STOP, 0, (DWORD)NULL);
			info->status = STATE_PAUSED;
			}
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
		case TYPE_MIDI: {
			MCI_SET_PARMS setParms;
			MCI_PLAY_PARMS playParms;

			setParms.dwTimeFormat = MCI_FORMAT_MILLISECONDS;
			if (SendMCI (info->mciDevice, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD)&setParms))
				return;
			
			playParms.dwCallback = (DWORD)Window;
			playParms.dwTo = 0;
			playParms.dwFrom = info->unpausePosition;
			if (SendMCI (info->mciDevice, MCI_PLAY, MCI_NOTIFY | MCI_FROM, (DWORD)&playParms))
				return;

			info->status = STATE_PLAYING;
			}
			break;
		case TYPE_MOD:
			break;
	}
}

void I_StopSong(int handle)
{
	info_t *info = (info_t *)handle;
	
	if (!info || info->status == STATE_STOPPED)
		return;

	info->status = STATE_STOPPED;

	switch (info->type) {
		case TYPE_MIDI:
			SendMCI (info->mciDevice, MCI_STOP, 0, (DWORD)NULL);
			SendMCI (info->mciDevice, MCI_CLOSE, 0, (DWORD)NULL);
			info->mciDevice = 0;
			break;
		case TYPE_MOD:
			MIDASstopModule (info->handle);
			info->handle = 0;
			break;
	}
	if (info == currSong)
		currSong = NULL;
}

void I_UnRegisterSong(int handle)
{
	info_t *info = (info_t *)handle;

	if (info) {
		I_StopSong (handle);
		switch (info->type) {
			case TYPE_MIDI:
				if (info->filename)
					remove (info->filename);
				break;
			case TYPE_MOD:
				MIDASfreeModule (info->module);
				break;
		}
		free (info);
	}
}

extern int convert( const char *mus, const char *mid, int nodisplay, int div,
					int size, int nocomp, int *ow );


int I_RegisterSong(void* data, int musicLen)
{
	FILE *f;
	int ow = 2;
	info_t *info;

	if (!(info = malloc (sizeof(info_t))))
		return 0;

	info->status = STATE_STOPPED;
	info->unpausePosition = 0;

	if ( (f = fopen(musName, "wb")) == NULL ) {
		Printf ("Unable to open temporary music file %s", musName);
		free (info);
		return 0;
	}
	if ( fwrite(data, musicLen, 1, f) != 1 ) {
		fclose (f);
		Printf ("Unable to write temporary music file\n");
		free (info);
		return 0;
	}
	fclose(f);

	if (*(int *)data == (('M')|(('U')<<8)|(('S')<<16)|((0x1a)<<24))) {
		// This is a mus file
		info->type = TYPE_MIDI;

		convert(musName, midName, 1, 89, 0, 1, &ow);

		remove (musName);
		info->filename = midName;
	} else if (*(int *)data == (('M')|(('T')<<8)|(('h')<<16)|(('d')<<24))) {
		// This is a midi file
		info->type = TYPE_MIDI;
		remove (midName);
		rename (musName, midName);
		info->filename = midName;
	} else {
		if (info->module = MIDASloadModule (musName)) {
			// This is a module
			info->type = TYPE_MOD;
			remove (musName);
			info->filename = NULL;
		} else {
			// This is not any known music format
			// (or could not load mod)
			remove (musName);
			free (info);
			info = NULL;
		}
	}

	return (int)info;
}

// Is the song playing?
int I_QrySongPlaying(int handle)
{
	info_t *info = (info_t *)handle;

	if (!info)
		return 0;
	else if (info->looping == 1)
		return 1;
	else
		return info->status;
}