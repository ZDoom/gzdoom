/* i_sound.cpp: System interface for sound, uses fmod.dll
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "resource.h"
extern HWND Window;
extern HINSTANCE hInstance;

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "doomtype.h"
#include "m_alloc.h"
#include <math.h>


#include <fmod.h>

#include "m_swap.h"
#include "z_zone.h"
#include "stats.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_music.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "i_video.h"
#include "s_sound.h"

#include "doomdef.h"

#define SCALE3D		96.f	// FSOUND_3D_Listener_SetDistanceFactor() does not work
#define INVSCALE3D	(1.f/SCALE3D)
#define ROLLOFF3D	0.5f
#define DOPPLER3D	1.f

static const char *OutputNames[] =
{
	"No sound",
	"Windows Multimedia",
	"DirectSound",
	"A3D"
};
static const char *MixerNames[] =
{
	"Auto",
	"Non-MMX blendmode",
	"Pentium MMX",
	"PPro MMX",
	"Quality auto",
	"Quality FPU",
	"Quality Pentium MMX",
	"Quality PPro MMX"
};

EXTERN_CVAR (Int, snd_sfxvolume)
CVAR (Int, snd_samplerate, 44100, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, snd_buffersize, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Int, snd_driver, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, snd_output, "default", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_3d, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// killough 2/21/98: optionally use varying pitched sounds
CVAR (Bool, snd_pitched, false, CVAR_ARCHIVE)
#define PITCH(f,x) (*snd_pitched ? ((f)*(x))/128 : (f))

// Maps sfx channels onto FMOD channels
static struct ChanMap
{
	int soundID;		// sfx playing on this channel
	long channelID;
	bool bIsLooping;
	bool bIs3D;
	unsigned int lastPos;
} *ChannelMap;

int _nosound = 0;
bool Sound3D;

static int numChannels;
static unsigned int DriverCaps;
static int OutputType;
static float ListenPos[3], ListenVel[3];

static const char *FmodErrors[] =
{
	"No errors",
	"Cannot call this command after FSOUND_Init.  Call FSOUND_Close first.",
	"This command failed because FSOUND_Init was not called",
	"Error initializing output device.",
	"Error initializing output device, but more specifically, the output device is already in use and cannot be reused.",
	"Playing the sound failed.",
	"Soundcard does not support the features needed for this soundsystem (16bit stereo output)",
	"Error setting cooperative level for hardware.",
	"Error creating hardware sound buffer.",
	"File not found",
	"Unknown file format",
	"Error loading file",
	"Not enough memory ",
	"The version number of this file format is not supported",
	"Incorrect mixer selected",
	"An invalid parameter was passed to this function",
	"Tried to use a3d and not an a3d hardware card, or dll didnt exist, try another output type.",
	"Tried to use an EAX command on a non EAX enabled channel or output.",
	"Failed to allocate a new channel"
};

// Simple file loader for FSOUND using an already-opened
// file handle. Also supports loading from a WAD opened
// in w_wad.cpp.

static unsigned int _cdecl FIO_Open (const char *name)
{
	return (unsigned int)name;
}

static void _cdecl FIO_Close (unsigned int handle)
{
	if (handle)
	{
		delete (FileHandle *)handle;
	}
}

static int _cdecl FIO_Read (void *buffer, int size, unsigned int handle)
{
	if (handle)
	{
		FileHandle *file = (FileHandle *)handle;
		if (size + file->pos > file->len)
			size = file->len - file->pos;
		if (size < 0)
		{
			size = 0;
		}
		else if (size > 0)
		{
			if (lseek (file->handle, file->base + file->pos, SEEK_SET) == -1)
				return 0;
			size = read (file->handle, buffer, size);
			file->pos += size;
		}
		return size;
	}
	return 0;
}

static int _cdecl FIO_Seek (unsigned int handle, int pos, signed char mode)
{
	if (handle)
	{
		FileHandle *file = (FileHandle *)handle;

		switch (mode)
		{
		case SEEK_SET:
			file->pos = pos;
			break;
		case SEEK_CUR:
			file->pos += pos;
			break;
		case SEEK_END:
			file->pos = file->len + pos;
			break;
		}
		if (file->pos < 0)
		{
			file->pos = 0;
			return -1;
		}
		if (file->pos > file->len)
		{
			file->pos = file->len;
			return -1;
		}
		return 0;
	}
	return -1;
}

static int _cdecl FIO_Tell (unsigned int handle)
{
	return handle ? ((FileHandle *)handle)->pos : 0;
}

void Enable_FSOUND_IO_Loader ()
{
	typedef unsigned int	(_cdecl *OpenCallback)(const char *name);
	typedef void			(_cdecl *CloseCallback)(unsigned int handle);
	typedef int				(_cdecl *ReadCallback)(void *buffer, int size, unsigned int handle);
	typedef int				(_cdecl *SeekCallback)(unsigned int handle, int pos, signed char mode);
	typedef int				(_cdecl *TellCallback)(unsigned int handle);
	
	FSOUND_File_SetCallbacks	
		((OpenCallback)FIO_Open,
		 (CloseCallback)FIO_Close,
		 (ReadCallback)FIO_Read,
		 (SeekCallback)FIO_Seek,
		 (TellCallback)FIO_Tell);
}

void Disable_FSOUND_IO_Loader ()
{
	FSOUND_File_SetCallbacks (NULL, NULL, NULL, NULL, NULL);
}

// FSOUND_Sample_Upload seems to mess up the signedness of sound data when
// uploading to hardware buffers. The pattern is not particularly predictable,
// so this is a replacement for it that loads the data manually. Source data
// is mono, unsigned, 8-bit. Output is mono, signed, 8- or 16-bit.

static int PutSampleData (FSOUND_SAMPLE *sample, BYTE *data, int len,
	unsigned int mode)
{
	if (mode & FSOUND_2D)
	{
		return FSOUND_Sample_Upload (sample, data,
			FSOUND_8BITS|FSOUND_MONO|FSOUND_UNSIGNED);
	}
	else if (FSOUND_Sample_GetMode (sample) & FSOUND_8BITS)
	{
		void *ptr1, *ptr2;
		unsigned int len1, len2;

		if (FSOUND_Sample_Lock (sample, 0, len, &ptr1, &ptr2, &len1, &len2))
		{
			int i;
			BYTE *ptr;
			int len;

			for (i = 0, ptr = (BYTE *)ptr1, len = len1;
				 i < 2 && ptr && len;
				i++, ptr = (BYTE *)ptr2, len = len2)
			{
				int j;
				for (j = 0; j < len; j++)
				{
					ptr[j] = *data++ - 128;
				}
			}
			FSOUND_Sample_Unlock (sample, ptr1, ptr2, len1, len2);
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		void *ptr1, *ptr2;
		unsigned int len1, len2;

		if (FSOUND_Sample_Lock (sample, 0, len*2, &ptr1, &ptr2, &len1, &len2))
		{
			int i;
			SWORD *ptr;
			int len;

			for (i = 0, ptr = (SWORD *)ptr1, len = len1/2;
				 i < 2 && ptr && len;
				i++, ptr = (SWORD *)ptr2, len = len2/2)
			{
				int j;
				for (j = 0; j < len; j++)
				{
					ptr[j] = ((*data<<8)|(*data)) - 32768;
					data++;
				}
			}
			FSOUND_Sample_Unlock (sample, ptr1, ptr2, len1, len2);
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	return TRUE;
}

static void DoLoad (void **slot, sfxinfo_t *sfx)
{
	int size;
	int errcount;
	unsigned long samplemode;

	samplemode = Sound3D ? FSOUND_HW3D : FSOUND_2D;

	errcount = 0;
	while (errcount < 2)
	{
		if (errcount)
			sfx->lumpnum = W_GetNumForName ("dsempty");

		size = W_LumpLength (sfx->lumpnum);
		if (size == 0)
		{
			errcount++;
			continue;
		}

		// Assume the sample is some format FMOD understands.
		// If that fails, load it as DMX.
		*slot = FSOUND_Sample_Load (FSOUND_FREE, (char *)new FileHandle (sfx->lumpnum), samplemode, 0);
		if (*slot == NULL)
		{ // DMX sound (presumably)
			byte *sfxdata = (byte *)W_CacheLumpNum (sfx->lumpnum, PU_CACHE);
			SDWORD len = ((SDWORD *)sfxdata)[1];
			FSOUND_SAMPLE *sample;
			unsigned int bits;

			if (len > size - 8)
			{
				Printf (PRINT_HIGH, "%s is missing %d bytes\n", sfx->name, sfx->length - size + 8);
				len = size - 8;
			}

			sfx->frequency = ((WORD *)sfxdata)[1];
			if (sfx->frequency == 0)
				sfx->frequency = 11025;
			sfx->ms = sfx->length = len;

			bits = FSOUND_8BITS;
			do
			{
				sample = FSOUND_Sample_Alloc (FSOUND_FREE, len,
					samplemode|bits|FSOUND_LOOP_OFF|FSOUND_MONO,
					sfx->frequency, 255, FSOUND_STEREOPAN, 255);
			} while (sample == NULL && (bits <<= 1) == FSOUND_16BITS);

			if (sample == NULL)
			{
				DPrintf ("Failed to allocate sample: %d\n", FSOUND_GetError ());
				errcount++;
				continue;
			}

			if (!PutSampleData (sample, sfxdata+8, len, samplemode))
			{
				DPrintf ("Failed to upload sample: %d\n", FSOUND_GetError ());
				FSOUND_Sample_Free (sample);
				sample = NULL;
				errcount++;
				continue;
			}
			*slot = sample;
		}
		else
		{
			int probe;

			FSOUND_Sample_GetDefaults ((FSOUND_SAMPLE *)sfx->data, &probe,
				NULL, NULL, NULL);

			sfx->frequency = probe;
			sfx->ms = FSOUND_Sample_GetLength ((FSOUND_SAMPLE *)sfx->data);
			sfx->length = sfx->ms;
		}
		break;
	}

	if (sfx->data)
	{
		sfx->ms = (sfx->ms * 1000) / (sfx->frequency);
		DPrintf ("sound loaded: %d Hz %d samples\n", sfx->frequency, sfx->length);
	}
}

static void getsfx (sfxinfo_t *sfx)
{
	char sndtemp[128];
	unsigned int i;

	// Get the sound data from the WAD and register it with sound library

	// If the sound doesn't exist, try a generic male sound (if
	// this is a player sound) or the empty sound.
	if (sfx->lumpnum == -1)
	{
		char *basename;
		int sfx_id;

		if (!strnicmp (sfx->name, "player/", 7) &&
			 (basename = strchr (sfx->name + 7, '/')))
		{
			sprintf (sndtemp, "player/male/%s", basename+1);
			sfx_id = S_FindSound (sndtemp);
			if (sfx_id != -1)
				sfx->lumpnum = S_sfx[sfx_id].lumpnum;
		}

		if (sfx->lumpnum == -1)
			sfx->lumpnum = W_GetNumForName ("dsempty");
	}
	
	// See if there is another sound already initialized with this lump. If so,
	// then set this one up as a link, and don't load the sound again.
	for (i = 0; i < S_sfx.Size (); i++)
	{
		if (S_sfx[i].data && S_sfx[i].link == -1 && S_sfx[i].lumpnum == sfx->lumpnum)
		{
			DPrintf ("Linked to %s (%d)\n", S_sfx[i].name, i);
			sfx->link = i;
			sfx->ms = S_sfx[i].ms;
			sfx->data = S_sfx[i].data;
			sfx->altdata = S_sfx[i].altdata;
			return;
		}
	}

	sfx->bHaveLoop = false;
	sfx->normal = 0;
	sfx->looping = 0;
	sfx->altdata = NULL;
	DoLoad (&sfx->data, sfx);
}


// Right now, FMOD's biggest shortcoming compared to MIDAS is that it does not
// support multiple samples with the same sample data. Thus, if we want to
// play a looped and non-looped version of the same sound, we need to create
// two copies of it. Fortunately, most sounds will either be played looped or
// not, but not both at the same time, so this really isn't too much of a
// problem. This function juggles the sample between looping and non-looping,
// creating a copy if necessary. It also increments the appropriate use
// counter.
static FSOUND_SAMPLE *CheckLooping (sfxinfo_t *sfx, BOOL looped)
{
	if (looped)
	{
		sfx->looping++;
		if (sfx->bHaveLoop)
		{
			return (FSOUND_SAMPLE *)(sfx->altdata ? sfx->altdata : sfx->data);
		}
		else
		{
			if (sfx->normal == 0)
			{
				sfx->bHaveLoop = true;
				FSOUND_Sample_SetLoopMode ((FSOUND_SAMPLE *)sfx->data,
					FSOUND_LOOP_NORMAL);
				return (FSOUND_SAMPLE *)sfx->data;
			}
		}
	}
	else
	{
		sfx->normal++;
		if (sfx->altdata || !sfx->bHaveLoop)
		{
			return (FSOUND_SAMPLE *)sfx->data;
		}
		else
		{
			if (sfx->looping == 0)
			{
				sfx->bHaveLoop = false;
				FSOUND_Sample_SetLoopMode ((FSOUND_SAMPLE *)sfx->data,
					FSOUND_LOOP_OFF);
				return (FSOUND_SAMPLE *)sfx->data;
			}
		}
	}

	// If we get here, we need to create an alternate version of the sample.
	FSOUND_Sample_SetLoopMode ((FSOUND_SAMPLE *)sfx->data,
		FSOUND_LOOP_OFF);
	DoLoad (&sfx->altdata, sfx);
	FSOUND_Sample_SetLoopMode ((FSOUND_SAMPLE *)sfx->altdata,
		FSOUND_LOOP_NORMAL);
	sfx->bHaveLoop = true;
	return (FSOUND_SAMPLE *)(looped ? sfx->altdata : sfx->data);
}

void UncheckSound (sfxinfo_t *sfx, BOOL looped)
{
	if (looped)
	{
		if (sfx->looping > 0)
			sfx->looping--;
	}
	else
	{
		if (sfx->normal > 0)
			sfx->normal--;
	}
}

//
// SFX API
//
 
void I_SetSfxVolume (int volume)
{
	// volume range is 0-15, but FMOD wants 0-255
	FSOUND_SetSFXMasterVolume ((volume << 4) | volume);
	// FMOD apparently resets absolute volume channels when setting master vol
	snd_musicvolume.Callback ();
}


//
// Starting a sound means adding it
//		to the current list of active sounds
//		in the internal channels.
// As the SFX info struct contains
//		e.g. a pointer to the raw data,
//		it is ignored.
//
// vol range is 0-255
// sep range is 0-255, -1 for surround, -2 for full vol middle
long I_StartSound (sfxinfo_t *sfx, int vol, int sep, int pitch, int channel, BOOL looping)
{
	if (_nosound)
		return 0;

	int id = sfx - &S_sfx[0];
	long volume;
	long pan;
	long freq;
	long chan;

	if (sep < 0)
	{
		pan = 128; // FSOUND_STEREOPAN is too loud relative to everything else
	}
	else
	{
		pan = sep;
	}

	freq = PITCH(sfx->frequency,pitch);
	volume = vol;

	chan = FSOUND_PlaySound3DAttrib (FSOUND_FREE, CheckLooping (sfx, looping),
		freq, vol, pan, NULL, NULL);
	
	if (chan != -1)
	{
		FSOUND_SetReserved (chan, TRUE);
		FSOUND_SetSurround (chan, sep == -1 ? TRUE : FALSE);
		ChannelMap[channel].channelID = chan;
		ChannelMap[channel].soundID = id;
		ChannelMap[channel].bIsLooping = looping ? true : false;
		ChannelMap[channel].lastPos = 0;
		ChannelMap[channel].bIs3D = false;
		return channel + 1;
	}

	DPrintf ("Sound failed to play: %d\n", FSOUND_GetError ());
	return 0;
}

long I_StartSound3D (sfxinfo_t *sfx, float vol, int pitch, int channel,
	BOOL looping, float pos[3], float vel[3])
{
	if (_nosound || !Sound3D)
		return 0;

	float lpos[3], lvel[3];
	int id = sfx - &S_sfx[0];
	long freq;
	long chan;

	freq = PITCH(sfx->frequency,pitch);

	lpos[0] = pos[0] * INVSCALE3D;
	lpos[1] = pos[1] * INVSCALE3D;
	lpos[2] = pos[2] * INVSCALE3D;
	lvel[0] = vel[0] * INVSCALE3D;
	lvel[1] = vel[1] * INVSCALE3D;
	lvel[2] = vel[2] * INVSCALE3D;

	chan = FSOUND_PlaySound3DAttrib (FSOUND_FREE, CheckLooping (sfx, looping),
		freq, (int)(vol * 255.f), -1, lpos, lvel);

	if (chan != -1)
	{
		FSOUND_SetReserved (chan, TRUE);
		ChannelMap[channel].channelID = chan;
		ChannelMap[channel].soundID = id;
		ChannelMap[channel].bIsLooping = looping ? true : false;
		ChannelMap[channel].lastPos = 0;
		ChannelMap[channel].bIs3D = true;
		return channel + 1;
	}
	else
	{
		DPrintf ("Sound failed to play: %d\n", FSOUND_GetError ());
	}

	return 0;
}

void I_StopSound (int handle)
{
	if (_nosound || !handle)
		return;

	handle--;
	if (ChannelMap[handle].soundID != -1)
	{
		FSOUND_StopSound (ChannelMap[handle].channelID);
		FSOUND_SetReserved (ChannelMap[handle].channelID, FALSE);
		UncheckSound (&S_sfx[ChannelMap[handle].soundID], ChannelMap[handle].bIsLooping);
		ChannelMap[handle].soundID = -1;
	}
}


int I_SoundIsPlaying (int handle)
{
	if (_nosound || !handle)
		return 0;

	handle--;
	if (ChannelMap[handle].soundID == -1)
	{
		return 0;
	}
	else
	{
		int is;

		// FSOUND_IsPlaying does not work with A3D
		if (OutputType != FSOUND_OUTPUT_A3D)
		{
			is = FSOUND_IsPlaying (ChannelMap[handle].channelID);
		}
		else
		{
			unsigned int pos;
			if (ChannelMap[handle].bIsLooping)
			{
				is = TRUE;
			}
			else
			{
				pos = FSOUND_GetCurrentPosition (ChannelMap[handle].channelID);
				is = pos >= ChannelMap[handle].lastPos &&
					pos <= S_sfx[ChannelMap[handle].soundID].length;
				ChannelMap[handle].lastPos = pos;
			}
		}
		if (!is)
		{
			FSOUND_SetReserved (ChannelMap[handle].channelID, FALSE);
			UncheckSound (&S_sfx[ChannelMap[handle].soundID], ChannelMap[handle].bIsLooping);
			ChannelMap[handle].soundID = -1;
		}
		return is;
	}
}


void I_UpdateSoundParams (int handle, int vol, int sep, int pitch)
{
	if (_nosound || !handle)
		return;

	handle--;
	if (ChannelMap[handle].soundID == -1)
		return;

	long volume;
	long pan;
	long freq;

	freq = PITCH(S_sfx[ChannelMap[handle].soundID].frequency, pitch);
	volume = vol;

	if (sep < 0)
	{
		pan = 128; //FSOUND_STEREOPAN
	}
	else
	{
		pan = sep;
	}

	FSOUND_SetSurround (ChannelMap[handle].channelID, sep == -1 ? TRUE : FALSE);
	FSOUND_SetPan (ChannelMap[handle].channelID, pan);
	FSOUND_SetVolume (ChannelMap[handle].channelID, volume);
	FSOUND_SetFrequency (ChannelMap[handle].channelID, freq);
}

void I_UpdateSoundParams3D (int handle, float pos[3], float vel[3])
{
	if (_nosound || !handle)
		return;

	handle--;
	if (ChannelMap[handle].soundID == -1)
		return;

	float lpos[3], lvel[3];
	lpos[0] = pos[0]/SCALE3D;
	lpos[1] = pos[1]/SCALE3D;
	lpos[2] = pos[2]/SCALE3D;
	lvel[0] = vel[0]/SCALE3D;
	lvel[1] = vel[1]/SCALE3D;
	lvel[2] = vel[2]/SCALE3D;
	FSOUND_3D_SetAttributes (ChannelMap[handle].channelID, lpos, lvel);
}

void I_UpdateListener (float pos[3], float vel[3], float forward[3], float up[3])
{
	if (Sound3D)
	{
		int i;

		for (i = 0; i < 3; i++)
		{
			ListenPos[i] = pos[i]/SCALE3D;
			ListenVel[i] = vel[i]/SCALE3D;
		}
		for (i = 0; i < numChannels; i++)
		{
			if (ChannelMap[i].soundID != -1 && !ChannelMap[i].bIs3D)
			{
				FSOUND_3D_SetAttributes (ChannelMap[i].channelID,
					ListenPos, ListenVel);
			}
		}

		FSOUND_3D_Listener_SetAttributes (ListenPos, ListenVel,
			forward[0], forward[1], forward[2], up[0], up[1], up[2]);

		FSOUND_3D_Update ();

	}
}

void I_LoadSound (struct sfxinfo_struct *sfx)
{
	if (_nosound)
		return;

	if (!sfx->data)
	{
		int i = sfx - &S_sfx[0];
		DPrintf ("loading sound \"%s\" (%d)\n", sfx->name, i);
		getsfx (sfx);
	}
}

// [RH] Dialog procedure for the error dialog that appears if FMOD
//		could not be initialized for some reason.
BOOL CALLBACK InitBoxCallback (HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
		if (wParam == IDOK ||
			wParam == IDC_NOSOUND ||
			wParam == IDCANCEL)
		{
			EndDialog (hwndDlg, wParam);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

static char FModLog (char success)
{
	if (success)
	{
		Printf (PRINT_HIGH, " succeeded\n");
	}
	else
	{
		Printf (PRINT_HIGH, " failed (error %d)\n", FSOUND_GetError());
	}
	return success;
}

void I_InitSound ()
{
	static const FSOUND_OUTPUTTYPES outtypes[3] =
		{ FSOUND_OUTPUT_DSOUND, FSOUND_OUTPUT_WINMM, FSOUND_OUTPUT_DSOUND };

	/* Get command line options: */
	_nosound = !!Args.CheckParm ("-nosfx") || !!Args.CheckParm ("-nosound");

	if (_nosound)
	{
		return;
	}

	int outindex;
	int trynum;
	bool trya3d = false;

	if (stricmp (*snd_output, "dsound") == 0)
	{
		outindex = 0;
	}
	else if (stricmp (*snd_output, "winmm") == 0)
	{
		outindex = 1;
	}
	else
	{
		outindex = (OSPlatform == os_WinNT) ? 1 : 0;
		if (*snd_3d || stricmp (*snd_output, "a3d") == 0)
		{
			trya3d = true;
		}
	}

	Printf (PRINT_HIGH, "I_InitSound: Initializing FMOD\n");
	FSOUND_SetHWND (Window);

	while (!_nosound)
	{
		trynum = 0;
		while (trynum < 2)
		{
			long outtype = trya3d ? FSOUND_OUTPUT_A3D
				: outtypes[outindex+trynum];

			Printf (PRINT_HIGH, "  Setting %s output", OutputNames[outtype]);
			FModLog (FSOUND_SetOutput (outtype));
			if (FSOUND_GetOutput() != outtype)
			{
				Printf (PRINT_HIGH, "  Got %s output instead.\n",
					OutputNames[FSOUND_GetOutput()]);
				if (trya3d)
				{
					trya3d = false;
				}
				else
				{
					trynum++;
				}
				continue;
			}
			Printf (PRINT_HIGH, "  Setting driver %d", *snd_driver);
			FModLog (FSOUND_SetDriver (*snd_driver));
			if (FSOUND_GetOutput() != outtype)
			{
				Printf (PRINT_HIGH, "   Output changed to %s\n   Trying driver 0",
					OutputNames[FSOUND_GetOutput()]);
				FSOUND_SetOutput (outtype);
				FModLog (FSOUND_SetDriver (0));
			}
//			FSOUND_GetDriverCaps (FSOUND_GetDriver(), &DriverCaps);
			if (*snd_buffersize)
			{
				Printf (PRINT_HIGH, "  Setting buffer size %d", *snd_buffersize);
				FModLog (FSOUND_SetBufferSize (*snd_buffersize));
			}
//FSOUND_SetMinHardwareChannels (32);
			Printf (PRINT_HIGH, "  Initialization");
			if (!FModLog (FSOUND_Init (*snd_samplerate, 64, 0)))
			{
				if (trya3d)
				{
					trya3d = false;
				}
				else
				{
					trynum++;
				}
			}
			else
			{
				break;
			}
		}
		if (trynum < 2)
		{ // Initialized successfully
			break;
		}
		// If sound cannot be initialized, give the user some options.
		switch (DialogBox (hInstance,
						   MAKEINTRESOURCE(IDD_FMODINITFAILED),
						   (HWND)Window,
						   (DLGPROC)InitBoxCallback))
		{
		case IDC_NOSOUND:
			_nosound = true;
			break;

		case IDCANCEL:
			exit (0);
			break;
		}
	}

	if (!_nosound)
	{
		Enable_FSOUND_IO_Loader ();
		OutputType = FSOUND_GetOutput ();
		if (*snd_3d)
		{
			Sound3D = true;
			//FSOUND_Reverb_SetEnvironment (FSOUND_PRESET_GENERIC);
			FSOUND_3D_Listener_SetRolloffFactor (ROLLOFF3D);
			FSOUND_3D_Listener_SetDopplerFactor (DOPPLER3D);
			if (!(DriverCaps & FSOUND_CAPS_HARDWARE))
			{
				Printf (PRINT_HIGH, "Using software 3D sound\n");
			}
		}
		else
		{
			Sound3D = false;
		}

		static bool didthis = false;
		if (!didthis)
		{
			didthis = true;
			atterm (I_ShutdownSound);
		}
	}

	I_InitMusic ();

	snd_sfxvolume.Callback ();
}

void I_SetChannels (int numchannels)
{
	int i;

	if (_nosound)
		return;

	ChannelMap = new ChanMap[numchannels];
	for (i = 0; i < numchannels; i++)
	{
		ChannelMap[i].soundID = -1;
	}

	numChannels = numchannels;
}

void STACK_ARGS I_ShutdownSound (void)
{
	if (!_nosound)
	{
		unsigned int i, c = 0;
		size_t len = 0;

		FSOUND_StopSound (FSOUND_ALL);

		if (ChannelMap)
		{
			delete[] ChannelMap;
			ChannelMap = NULL;
		}

		// Free all loaded samples
		for (i = 0; i < S_sfx.Size (); i++)
		{
#if 0
			// Since the samples were not allocated with FSOUND_UNMANAGED,
			// FSOUND_Close () will free them for us.
			if (S_sfx[i].link == -1 && S_sfx[i].data)
			{
				if (S_sfx[i].data)
				{
					FSOUND_Sample_Free ((FSOUND_SAMPLE *)S_sfx[i].data);
					len += S_sfx[i].length;
					c++;
				}
				if (S_sfx[i].altdata)
				{
					FSOUND_Sample_Free ((FSOUND_SAMPLE *)S_sfx[i].altdata);
				}
			}
#endif
			S_sfx[i].data = NULL;
			S_sfx[i].altdata = NULL;
			S_sfx[i].bHaveLoop = false;
		}

		FSOUND_Close ();
	}
}

CCMD (snd_status)
{
	if (_nosound)
	{
		Printf (PRINT_HIGH, "sound is not active\n");
		return;
	}

	int output = FSOUND_GetOutput ();
	int driver = FSOUND_GetDriver ();
	int mixer = FSOUND_GetMixer ();

	Printf (PRINT_HIGH, "Output: %s\n", OutputNames[output]);
	Printf (PRINT_HIGH, "Driver: %d (%s)\n", driver,
		FSOUND_GetDriverName (driver));
	Printf (PRINT_HIGH, "Mixer: %s\n", MixerNames[mixer]);
	if (DriverCaps)
	{
		Printf (PRINT_HIGH, "Driver caps: 0x%x\n", DriverCaps);
	}
	if (Sound3D)
	{
		Printf (PRINT_HIGH, "Using 3D sound\n");
	}
}

CCMD (snd_reset)
{
	I_ShutdownMusic ();
	I_ShutdownSound ();
	I_InitSound ();
	I_SetChannels (numChannels);
	S_RestartMusic ();
}

CCMD (snd_listdrivers)
{
	long numdrivers = FSOUND_GetNumDrivers ();
	long i;

	for (i = 0; i < numdrivers; i++)
	{
		Printf (PRINT_HIGH, "%ld. %s\n", i, FSOUND_GetDriverName (i));
	}
}

ADD_STAT (sound, out)
{
	if (_nosound)
	{
		strcpy (out, "no sound");
	}
	else
	{
		sprintf (out, "%d channels, %.2f%% CPU", FSOUND_GetChannelsPlaying(),
			FSOUND_GetCPUUsage());
	}
}
