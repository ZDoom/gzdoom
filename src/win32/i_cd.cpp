/*
** i_cd.cpp
** Functions for controlling CD playback
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

#define _WIN32_WINNT	0x0400
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <ctype.h>
#include <stdlib.h>

#define USE_WINDOWS_DWORD
#include "doomtype.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "m_argv.h"
#include "i_system.h"
#include "version.h"

#include "i_cd.h"
#include "helperthread.h"

extern HWND Window;
extern HINSTANCE g_hInst;

enum
{
	CDM_Init,	// parm1 = device
	CDM_Close,
	CDM_Play,	// parm1 = track, parm2 = 1:looping
	CDM_PlayCD,	// parm1 = 1:looping
	CDM_Replay,	// Redos the most recent CDM_Play(CD)
	CDM_Stop,
	CDM_Eject,
	CDM_UnEject,
	CDM_Pause,
	CDM_Resume,
	CDM_GetMode,
	CDM_CheckTrack,	// parm1 = track
	CDM_GetMediaIdentity,
	CDM_GetMediaUPC
};

class FCDThread : public FHelperThread
{
public:
	FCDThread ();

protected:
	bool Init ();
	void Deinit ();
	const char *ThreadName ();
	DWORD Dispatch (DWORD method, DWORD parm1=0, DWORD parm2=0, DWORD parm3=0);
	void DefaultDispatch ();

	bool IsTrackAudio (DWORD track) const;
	DWORD GetTrackLength (DWORD track) const;
	DWORD GetNumTracks () const;

	static LRESULT CALLBACK CD_WndProc (HWND hWnd, UINT message,
		WPARAM wParam, LPARAM lParam);

	WNDCLASS CD_WindowClass;
	HWND CD_Window;
	ATOM CD_WindowAtom;

	bool Looping;
	DWORD PlayFrom;
	DWORD PlayTo;
	UINT DeviceID;
};

#define NOT_INITED		((signed)0x80000000)

static FCDThread *CDThread;
static int Inited = NOT_INITED;

//==========================================================================
//
// CVAR: cd_enabled
//
// Use the CD device? Can be overridden with -nocdaudio on the command line
//
//==========================================================================

CUSTOM_CVAR (Bool, cd_enabled, true, CVAR_ARCHIVE|CVAR_NOINITCALL|CVAR_GLOBALCONFIG)
{
	if (self)
		CD_Init ();
	else
		CD_Close ();
}

//==========================================================================
//
// CVAR: cd_drive
//
// Which drive (letter) to use for CD audio. If not a valid drive letter,
// let the operating system decide for us.
//
//==========================================================================

CUSTOM_CVAR (String, cd_drive, "", CVAR_ARCHIVE|CVAR_NOINITCALL|CVAR_GLOBALCONFIG)
{
	CD_Init ();
}

//==========================================================================
//
// ---Constructor---
//
//==========================================================================

FCDThread::FCDThread ()
{
	memset (&CD_WindowClass, 0, sizeof(CD_WindowClass));
	CD_Window = NULL;
	CD_WindowAtom = 0;
	Inited = NOT_INITED;
	Looping = false;
	PlayFrom = 0;
	PlayTo = 0;
	DeviceID = 0;
	LaunchThread ();
}

//==========================================================================
//
// ThreadName
//
//==========================================================================

const char *FCDThread::ThreadName ()
{
	return "CD Player";
}

//==========================================================================
//
// Init
//
//==========================================================================

bool FCDThread::Init ()
{
	CD_WindowClass.style = CS_NOCLOSE;
	CD_WindowClass.lpfnWndProc = CD_WndProc;
	CD_WindowClass.hInstance = g_hInst;
	CD_WindowClass.lpszClassName = GAMENAME " CD Player";
	CD_WindowAtom = RegisterClass (&CD_WindowClass);

	if (CD_WindowAtom == 0)
		return false;

	CD_Window = CreateWindow (
		(LPCTSTR)(INT_PTR)(int)CD_WindowAtom,
		GAMENAME " CD Player",
		0,
		0, 0, 10, 10,
		NULL,
		NULL,
		g_hInst,
		NULL);

	if (CD_Window == NULL)
	{
		UnregisterClass ((LPCTSTR)(INT_PTR)(int)CD_WindowAtom, g_hInst);
		CD_WindowAtom = 0;
		return false;
	}

#ifdef _WIN64
	SetWindowLongPtr (CD_Window, GWLP_USERDATA, (LONG_PTR)this);
#else
	SetWindowLong (CD_Window, GWL_USERDATA, (LONG)(LONG_PTR)this);
#endif
	SetThreadPriority (ThreadHandle, THREAD_PRIORITY_LOWEST);
	return true;
}

//==========================================================================
//
// Deinit
//
//==========================================================================

void FCDThread::Deinit ()
{
	if (CD_Window)
	{
		DestroyWindow (CD_Window);
		CD_Window = NULL;
	}
	if (CD_WindowAtom)
	{
		UnregisterClass ((LPCTSTR)(INT_PTR)(int)CD_WindowAtom, g_hInst);
		CD_WindowAtom = 0;
	}
	if (DeviceID)
	{
		Dispatch (CDM_Close);
	}
}

//==========================================================================
//
// DefaultDispatch
//
//==========================================================================

void FCDThread::DefaultDispatch ()
{
	MSG wndMsg;

	while (PeekMessage (&wndMsg, NULL, 0, 0, PM_REMOVE))
	{
		DispatchMessage (&wndMsg);
	}
}

//==========================================================================
//
// Dispatch
//
// Does the actual work of implementing the public CD interface
//
//==========================================================================

DWORD FCDThread::Dispatch (DWORD method, DWORD parm1, DWORD parm2, DWORD parm3)
{
	MCI_OPEN_PARMS mciOpen;
	MCI_SET_PARMS mciSetParms;
	MCI_PLAY_PARMS playParms;
	MCI_STATUS_PARMS statusParms;
	MCI_INFO_PARMS infoParms;
	DWORD firstTrack, lastTrack, numTracks;
	DWORD length;
	DWORD openFlags;
	char ident[32];

	switch (method)
	{
	case CDM_Init:
		mciOpen.lpstrDeviceType = (LPCSTR)MCI_DEVTYPE_CD_AUDIO;
		openFlags = MCI_OPEN_SHAREABLE|MCI_OPEN_TYPE|MCI_OPEN_TYPE_ID|MCI_WAIT;
		if ((signed)parm1 >= 0)
		{
			ident[0] = (char)(parm1 + 'A');
			ident[1] = ':';
			ident[2] = 0;
			mciOpen.lpstrElementName = ident;
			openFlags |= MCI_OPEN_ELEMENT;
		}

		while (mciSendCommand (0, MCI_OPEN, openFlags, (DWORD_PTR)&mciOpen) != 0)
		{
			if (!(openFlags & MCI_OPEN_ELEMENT))
			{
				return FALSE;
			}
			openFlags &= ~MCI_OPEN_ELEMENT;
		}

		DeviceID = mciOpen.wDeviceID;
		mciSetParms.dwTimeFormat = MCI_FORMAT_TMSF;
		if (mciSendCommand (DeviceID, MCI_SET, MCI_SET_TIME_FORMAT, (DWORD_PTR)&mciSetParms) == 0)
		{
			return TRUE;
		}

		mciSendCommand (DeviceID, MCI_CLOSE, 0, 0);
		return FALSE;

	case CDM_Close:
		Dispatch (CDM_Stop);
		mciSendCommand (DeviceID, MCI_CLOSE, 0, 0);
		DeviceID = 0;
		return 0;

	case CDM_Play:
		if (!IsTrackAudio (parm1))
		{
			//Printf ("Track %d is not audio\n", track);
			return FALSE;
		}

		length = GetTrackLength (parm1);

		if (length == 0)
		{ // Couldn't get length of track, so won't be able to play last track
			PlayTo = MCI_MAKE_TMSF (parm1+1, 0, 0, 0);
		}
		else
		{
			PlayTo = MCI_MAKE_TMSF (parm1,
				MCI_MSF_MINUTE(length),
				MCI_MSF_SECOND(length),
				MCI_MSF_FRAME(length));
		}
		PlayFrom = MCI_MAKE_TMSF (parm1, 0, 0, 0);
		Looping = parm2 & 1;

		// intentional fall-through

	case CDM_Replay:
		playParms.dwFrom = PlayFrom;
		playParms.dwTo = PlayTo;
		playParms.dwCallback = (DWORD_PTR)CD_Window;

		return mciSendCommand (DeviceID, MCI_PLAY, MCI_FROM | MCI_TO | MCI_NOTIFY,
			(DWORD_PTR)&playParms);

	case CDM_PlayCD:
		numTracks = GetNumTracks ();
		if (numTracks == 0)
			return FALSE;

		for (firstTrack = 1; firstTrack <= numTracks && !IsTrackAudio (firstTrack); firstTrack++)
			;
		for (lastTrack = firstTrack; lastTrack <= numTracks && IsTrackAudio (lastTrack); lastTrack++)
			;

		if (firstTrack > numTracks)
			return FALSE;
		if (lastTrack > numTracks)
			lastTrack = numTracks;

		length = GetTrackLength (lastTrack);
		if (length == 0)
			return FALSE;

		Looping = parm1 & 1;
		PlayFrom = MCI_MAKE_TMSF (firstTrack, 0, 0, 0);
		PlayTo = MCI_MAKE_TMSF (lastTrack,
			MCI_MSF_MINUTE(length),
			MCI_MSF_SECOND(length),
			MCI_MSF_FRAME(length));

		playParms.dwFrom = PlayFrom;
		playParms.dwTo = PlayTo;
		playParms.dwCallback = (DWORD_PTR)CD_Window;

		return mciSendCommand (DeviceID, MCI_PLAY, MCI_FROM|MCI_TO|MCI_NOTIFY,
			(DWORD_PTR)&playParms);

	case CDM_Stop:
		return mciSendCommand (DeviceID, MCI_STOP, 0, 0);

	case CDM_Eject:
		return mciSendCommand (DeviceID, MCI_SET, MCI_SET_DOOR_OPEN, 0);

	case CDM_UnEject:
		return mciSendCommand (DeviceID, MCI_SET, MCI_SET_DOOR_CLOSED, 0);

	case CDM_Pause:
		return mciSendCommand (DeviceID, MCI_PAUSE, 0, 0);

	case CDM_Resume:
		playParms.dwTo = PlayTo;
		playParms.dwCallback = (DWORD_PTR)CD_Window;
		return mciSendCommand (DeviceID, MCI_PLAY, MCI_TO | MCI_NOTIFY, (DWORD_PTR)&playParms);

	case CDM_GetMode:
		statusParms.dwItem = MCI_STATUS_MODE;
		if (mciSendCommand (DeviceID, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&statusParms))
		{
			return CDMode_Unknown;
		}
		else
		{
			switch (statusParms.dwReturn)
			{
			case MCI_MODE_NOT_READY:	return CDMode_NotReady;
			case MCI_MODE_PAUSE:		return CDMode_Pause;
			case MCI_MODE_PLAY:			return CDMode_Play;
			case MCI_MODE_STOP:			return CDMode_Stop;
			case MCI_MODE_OPEN:			return CDMode_Open;
			default:					return CDMode_Unknown;
			}
		}

	case CDM_CheckTrack:
		return IsTrackAudio (parm1) ? TRUE : FALSE;

	case CDM_GetMediaIdentity:
	case CDM_GetMediaUPC:
		char ident[32];

		infoParms.lpstrReturn = ident;
		infoParms.dwRetSize = sizeof(ident);
		if (mciSendCommand (DeviceID, MCI_INFO,
			method == CDM_GetMediaIdentity ? MCI_WAIT|MCI_INFO_MEDIA_IDENTITY
				: MCI_WAIT|MCI_INFO_MEDIA_UPC, (DWORD_PTR)&infoParms))
		{
			return 0;
		}
		else
		{
			return strtoul (ident, NULL, 0);
		}

	default:
		return 0;
	}
}

//==========================================================================
//
// KillThread
//
//==========================================================================

static void KillThread ()
{
	if (CDThread != NULL)
	{
		CDThread->DestroyThread ();
		Inited = NOT_INITED;
		delete CDThread;
	}
}

//==========================================================================
//
// CD_Init
//
//==========================================================================

bool CD_Init ()
{
	if ((cd_drive)[0] == 0 || (cd_drive)[1] != 0)
	{
		return CD_Init (-1);
	}
	else
	{
		char drive = toupper ((cd_drive)[0]);

		if (drive >= 'A' && drive <= 'Z' && !CD_Init (drive - 'A'))
			return CD_Init (-1);
	}
	return true;
}

bool CD_Init (int device)
{
	if (!cd_enabled || Args->CheckParm ("-nocdaudio"))
		return false;

	if (CDThread == NULL)
	{
		CDThread = new FCDThread;
		atterm (KillThread);
	}

	if (Inited != device)
	{
		CD_Close ();

		if (CDThread->SendMessage (CDM_Init, device, 0, 0, true))
		{
			Inited = device;
			return true;
		}
		else
		{
			return false;
		}
	}
	return true;
}

//==========================================================================
//
// CD_InitID
//
//==========================================================================

bool CD_InitID (unsigned int id, int guess)
{
	char drive;

	if (guess < 0 && Inited != NOT_INITED)
		guess = Inited;

	if (guess >= 0 && CD_Init (guess))
	{
		if (CDThread->SendMessage (CDM_GetMediaIdentity, 0, 0, 0, true) == id ||
			CDThread->SendMessage (CDM_GetMediaUPC, 0, 0, 0, true) == id)
		{
			return true;
		}
		CD_Close ();
	}

	for (drive = 'V'; drive < 'Z'; drive++)
	{
		if (CD_Init (drive - 'A'))
		{
			// I don't know which value is stored in a CDDA file, so I try
			// them both. All the CDs I've tested have had the same value
			// for both, so it probably doesn't matter.
			if (CDThread->SendMessage (CDM_GetMediaIdentity, 0, 0, 0, true) == id ||
				CDThread->SendMessage (CDM_GetMediaUPC, 0, 0, 0, true) == id)
			{
				return true;
			}
			CD_Close ();
		}
	}
	return false;
}

//==========================================================================
//
// CD_Close
//
//==========================================================================

void CD_Close ()
{
	if (Inited != NOT_INITED)
	{
		CDThread->SendMessage (CDM_Close, 0, 0, 0, true);
		Inited = NOT_INITED;
	}
}

//==========================================================================
//
// CD_Eject
//
//==========================================================================

void CD_Eject ()
{
	if (Inited != NOT_INITED)
		CDThread->SendMessage (CDM_Eject, 0, 0, 0, false);
}

//==========================================================================
//
// CD_UnEject
//
//==========================================================================

bool CD_UnEject ()
{
	if (Inited == NOT_INITED)
		return false;

	return CDThread->SendMessage (CDM_UnEject, 0, 0, 0, true) == 0;
}

//==========================================================================
//
// CD_Stop
//
//==========================================================================

void CD_Stop ()
{
	if (Inited != NOT_INITED)
		CDThread->SendMessage (CDM_Stop, 0, 0, 0, false);
}

//==========================================================================
//
// CD_Play
//
//==========================================================================

bool CD_Play (int track, bool looping)
{
	if (Inited == NOT_INITED)
		return false;

	return CDThread->SendMessage (CDM_Play, track, looping ? 1 : 0, 0, true) == 0;
}

//==========================================================================
//
// CD_PlayNoWait
//
//==========================================================================

void CD_PlayNoWait (int track, bool looping)
{
	if (Inited != NOT_INITED)
		CDThread->SendMessage (CDM_Play, track, looping ? 1 : 0, 0, false);
}

//==========================================================================
//
// CD_PlayCD
//
//==========================================================================

bool CD_PlayCD (bool looping)
{
	if (Inited == NOT_INITED)
		return false;

	return CDThread->SendMessage (CDM_PlayCD, looping ? 1 : 0, 0, 0, true) == 0;
}

//==========================================================================
//
// CD_PlayCDNoWait
//
//==========================================================================

void CD_PlayCDNoWait (bool looping)
{
	if (Inited != NOT_INITED)
		CDThread->SendMessage (CDM_PlayCD, looping ? 1 : 0, 0, 0, false);
}

//==========================================================================
//
// CD_Pause
//
//==========================================================================

void CD_Pause ()
{
	if (Inited != NOT_INITED)
		CDThread->SendMessage (CDM_Pause, 0, 0, 0, false);
}

//==========================================================================
//
// CD_Resume
//
//==========================================================================

bool CD_Resume ()
{
	if (Inited == NOT_INITED)
		return false;

	return CDThread->SendMessage (CDM_Resume, 0, 0, 0, false) == 0;
}

//==========================================================================
//
// CD_GetMode
//
//==========================================================================

ECDModes CD_GetMode ()
{
	if (Inited == NOT_INITED)
		return CDMode_Unknown;

	return (ECDModes)CDThread->SendMessage (CDM_GetMode, 0, 0, 0, true);
}

//==========================================================================
//
// CD_CheckTrack
//
//==========================================================================

bool CD_CheckTrack (int track)
{
	if (Inited == NOT_INITED)
		return false;

	return CDThread->SendMessage (CDM_CheckTrack, track, 0, 0, true) != 0;
}

// Functions called only by the helper thread -------------------------------

//==========================================================================
//
// IsTrackAudio
//
//==========================================================================

bool FCDThread::IsTrackAudio (DWORD track) const
{
	MCI_STATUS_PARMS statusParms;

	statusParms.dwItem = MCI_CDA_STATUS_TYPE_TRACK;
	statusParms.dwTrack = track;
	if (mciSendCommand (DeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK,
		(DWORD_PTR)&statusParms))
	{
		return FALSE;
	}
	return statusParms.dwReturn == MCI_CDA_TRACK_AUDIO;
}

//==========================================================================
//
// GetTrackLength
//
//==========================================================================

DWORD FCDThread::GetTrackLength (DWORD track) const
{
	MCI_STATUS_PARMS statusParms;

	statusParms.dwItem = MCI_STATUS_LENGTH;
	statusParms.dwTrack = track;
	if (mciSendCommand (DeviceID, MCI_STATUS, MCI_STATUS_ITEM | MCI_TRACK,
		(DWORD_PTR)&statusParms))
	{
		return 0;
	}
	else
	{
		return (DWORD)statusParms.dwReturn;
	}
}

//==========================================================================
//
// GetNumTracks
//
//==========================================================================

DWORD FCDThread::GetNumTracks () const
{
	MCI_STATUS_PARMS statusParms;

	statusParms.dwItem = MCI_STATUS_NUMBER_OF_TRACKS;
	if (mciSendCommand (DeviceID, MCI_STATUS, MCI_STATUS_ITEM, (DWORD_PTR)&statusParms))
	{
		return 0;
	}
	else
	{
		return (DWORD)statusParms.dwReturn;
	}
}

//==========================================================================
//
// CD_WndProc (static)
//
// Because MCI (under Win 9x anyway) can't notify windows owned by another
// thread, the helper thread creates its own window.
//
//==========================================================================

LRESULT CALLBACK FCDThread::CD_WndProc (HWND hWnd, UINT message,
	WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case MM_MCINOTIFY:
		if (wParam == MCI_NOTIFY_SUCCESSFUL)
		{
			FCDThread *self = (FCDThread *)(LONG_PTR)GetWindowLongPtr (hWnd, GWLP_USERDATA);
			// Using SendMessage could deadlock, so don't do that.
			self->Dispatch (self->Looping ? CDM_Replay : CDM_Stop);
		}
		return 0;

	default:
		return DefWindowProc (hWnd, message, wParam, lParam);
	}
}
