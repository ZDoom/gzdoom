#ifndef __I_CD_H__
#define __I_CD_H__

enum ECDModes
{
	CDMode_Unknown,
	CDMode_NotReady,
	CDMode_Pause,
	CDMode_Play,
	CDMode_Stop,
	CDMode_Open
};

// Opens a CD device. If device is non-negative, it specifies which device
// to open. 0 is drive A:, 1 is drive B:, etc. If device is not specified,
// the user's preference is used to decide which device to open.
bool CD_Init ();
bool CD_Init (int device);

// Open a CD device containing a specific CD. Tries device guess first.
bool CD_InitID (unsigned int id, int guess=-1);

// Closes a CD device previously opened with CD_Init
void CD_Close ();

// Plays a single track, possibly looping
bool CD_Play (int track, bool looping);

// Plays the first block of audio tracks on a CD, possibly looping
bool CD_PlayCD (bool looping);

// Versions of the above that return as soon as possible
void CD_PlayNoWait (int track, bool looping);
void CD_PlayCDNoWait (bool looping);

// Stops playing the CD
void CD_Stop ();

// Pauses CD playing
void CD_Pause ();

// Resumes CD playback after pausing
bool CD_Resume ();

// Eject the CD tray
void CD_Eject ();

// Close the CD tray
bool CD_UnEject ();

// Get the CD drive's status (mode)
ECDModes CD_GetMode ();

// Check if a track exists and is audio
bool CD_CheckTrack (int track);

#endif //__I_CD_H__
