/*
** i_cd.h
** Defines the CD interface
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
