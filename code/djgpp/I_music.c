#define TRUE 1
#define FALE 0
typedef unsigned long DWORD;
typedef unsigned int UINT;

#include "m_argv.h"
#include "i_music.h"
#include "w_wad.h"
#include "c_consol.h"
#include "c_dispch.h"
#include "z_zone.h"

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
	MIDASmodulePlayHandle handle;
	MIDASmodule module;
	BOOL looping;
};
typedef struct MusInfo info_t;

static info_t  *currSong;
static int		nomusic = 0;
static char		modName[512];
static int		musicvolume;
static DWORD	midivolume;
static cvar_t  *snd_mididevice;
static DWORD	nummididevices;
static UINT		mididevice;

void I_SetMIDIVolume (float volume)
{
	DWORD wooba = (DWORD)(volume * 0xffff) & 0xffff;
	midivolume = (wooba << 16) | wooba;
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
		Printf (PRINT_HIGH, "ID out of range. Using MIDI mapper.\n");
		SetCVarFloat (var, -1.0f);
		return;
	} else if (var->value < 0) {
		mididevice = ~0;
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
	if (nummididevices) {
		Printf (PRINT_HIGH, "-1. MIDI Mapper\n");
	} else {
		Printf (PRINT_HIGH, "No MIDI devices installed.\n");
	}
}

void I_InitMusic (void)
{
	char *temp;

	Printf (PRINT_HIGH, "I_InitMusic\n");
	
	snd_mididevice = cvar ("snd_mididevice", "-1", CVAR_ARCHIVE|CVAR_CALLBACK);
	snd_mididevice->u.callback = ChangeMIDIDevice;
	nummididevices = 0;
	ChangeMIDIDevice (snd_mididevice);
	C_RegisterCommand ("snd_listmididevices", ListMIDIDevices);

	nomusic = !!M_CheckParm("-nomusic") || !!M_CheckParm("-nosound");

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


void STACK_ARGS I_ShutdownMusic(void)
{
	if (currSong) {
		I_UnRegisterSong ((int)currSong);
		currSong = NULL;
	}
	remove (modName);
}


void I_PlaySong (int handle, int _looping)
{
	info_t *info = (info_t *)handle;

	if (!info || nomusic)
		return;

	info->status = STATE_STOPPED;
	info->looping = _looping;

	switch (info->type) {
		case TYPE_MOD:
			if ( (info->handle = MIDASplayModule (info->module, _looping)) ) {
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
	info_t *info;

	if (!(info = malloc (sizeof(info_t))))
		return 0;

	info->status = STATE_STOPPED;

	if (*(int *)data == (('M')|(('U')<<8)|(('S')<<16)|((0x1a)<<24))) {
		// This is a mus file
		free (info);
		return 0;
	} else if (*(int *)data == (('M')|(('T')<<8)|(('h')<<16)|(('d')<<24))) {
		// This is a midi file
		free (info);
		return 0;
	} else {
		FILE *f;

		if ((f = fopen (modName, "wb")) == NULL) {
			Printf (PRINT_HIGH, "Unable to open temporary music file %s\n", modName);
			free (info);
			return 0;
		}
		if (fwrite (data, musicLen, 1, f) != 1) {
			fclose (f);
			Printf (PRINT_HIGH, "Unable to write temporary music file\n");
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
			Printf (PRINT_HIGH, "midas failed to load song:\n%s\n", MIDASgetErrorMessage(MIDASgetLastError()));
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