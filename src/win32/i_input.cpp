/*
** i_input.cpp
** Handles input from keyboard, mouse, and joystick
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

#define DIRECTINPUT_VERSION 0x300	// We want to support DX 3.0, and
#define _WIN32_WINNT 0x0400			// we want to support the mouse wheel

#define WIN32_LEAN_AND_MEAN
#define __BYTEBOOL__
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <dinput.h>

#include "c_dispatch.h"
#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_argv.h"
#include "i_input.h"
#include "v_video.h"

#include "d_main.h"
#include "d_gui.h"
#include "c_console.h"
#include "c_cvars.h"
#include "i_system.h"

#include "s_sound.h"
#include "win32iface.h"

#define DINPUT_BUFFERSIZE	32

#ifdef _DEBUG
#define INGAME_PRIORITY_CLASS	NORMAL_PRIORITY_CLASS
#else
#define INGAME_PRIORITY_CLASS	HIGH_PRIORITY_CLASS
#endif

extern HINSTANCE g_hInst;

static void KeyRead ();
static BOOL DI_Init2 ();
static void MouseRead_DI ();
static void MouseRead_Win32 ();
static void GrabMouse_Win32 ();
static void UngrabMouse_Win32 ();
static BOOL I_GetDIMouse ();
static void I_GetWin32Mouse ();
static void CenterMouse_Win32 (LONG curx, LONG cury);
static void WheelMoved ();
static void DI_Acquire (LPDIRECTINPUTDEVICE mouse);
static void DI_Unacquire (LPDIRECTINPUTDEVICE mouse);
static void SetCursorState (int visible);

static bool GUICapture;
static bool NativeMouse;
static bool MakeMouseEvents;
static POINT UngrabbedPointerPos;

bool VidResizing;

extern BOOL vidactive;
extern HWND Window;

EXTERN_CVAR (String, language)


// [RH] As of 1.14, ZDoom no longer needs to be linked with dinput.lib.
//		We now go straight to the DLL ourselves.

#ifndef __CYGNUS__
typedef HRESULT (WINAPI *DIRECTINPUTCREATE_FUNCTION) (HINSTANCE, DWORD, LPDIRECTINPUT*, LPUNKNOWN);
#else
typedef HRESULT WINAPI (*DIRECTINPUTCREATE_FUNCTION) (HINSTANCE, DWORD, LPDIRECTINPUT*, LPUNKNOWN);
#endif

static HMODULE DirectInputInstance;

void InitKeyboardObjectData (void);

typedef enum { win32, dinput } mousemode_t;
static mousemode_t mousemode;

extern BOOL paused;
static bool HaveFocus = false;
static bool noidle = false;
static int WheelMove;

static LPDIRECTINPUT			g_pdi;
static LPDIRECTINPUTDEVICE		g_pKey;
static LPDIRECTINPUTDEVICE		g_pMouse;

HCURSOR TheArrowCursor, TheInvisibleCursor;

//Other globals
static int GDx,GDy;

extern constate_e ConsoleState;

BOOL AppActive = TRUE;

CVAR (Bool,  i_remapkeypad,			true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,  use_mouse,				true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR (Bool,  use_joystick,			false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_speedmultiplier,	1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_xsensitivity,		1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_ysensitivity,		-1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_xthreshold,		0.15f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_ythreshold,		0.15f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (Int, in_mouse, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 2)
	{
		self = 2;
	}
	else if (!g_pdi)
	{
		return;
	}
	else
	{
		int new_mousemode;

		if (self == 1 || (self == 0 && OSPlatform == os_WinNT))
			new_mousemode = win32;
		else
			new_mousemode = dinput;

		if (new_mousemode != mousemode)
		{
			if (new_mousemode == win32)
				I_GetWin32Mouse ();
			else
				if (!I_GetDIMouse ())
					I_GetWin32Mouse ();
			NativeMouse = false;
		}
	}
}

static BYTE KeyState[256];
static BYTE DIKState[2][NUM_KEYS];
static int ActiveDIKState;

// Convert DIK_* code to ASCII using Qwerty keymap
static const byte Convert [256] =
{
  //  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
	  0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',   8,   9, // 0
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',  13,   0, 'a', 's', // 1
	'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',  39, '`',   0,'\\', 'z', 'x', 'c', 'v', // 2
	'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0, // 3
	  0,   0,   0,   0,   0,   0,   0, '7', '8', '9', '-', '4', '5', '6', '+', '1', // 4
	'2', '3', '0', '.',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 5
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 6
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 7

	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, '=',   0,   0, // 8
	  0, '@', ':', '_',   0,   0,   0,   0,   0,   0,   0,   0,  13,   0,   0,   0, // 9
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // A
	  0,   0,   0, ',',   0, '/',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // B
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // C
	  0,   0,   0,   0,   0,   0,   0,   0

};

static void FlushDIKState (int low=0, int high=NUM_KEYS-1)
{
	int i;
	event_t event;
	BYTE *state = DIKState[ActiveDIKState];

	memset (&event, 0, sizeof(event));
	event.type = EV_KeyUp;
	for (i = low; i <= high; ++i)
	{
		if (state[i])
		{
			state[i] = 0;
			event.data1 = i;
			event.data2 = i < 256 ? Convert[i] : 0;
			D_PostEvent (&event);
		}
	}
}

extern int WaitingForKey, chatmodeon;

static void I_CheckGUICapture ()
{
	bool wantCapt;

	wantCapt = 
		ConsoleState == c_down ||
		ConsoleState == c_falling ||
		(menuactive && !WaitingForKey) ||
		chatmodeon;

	if (wantCapt != GUICapture)
	{
		GUICapture = wantCapt;
		if (wantCapt)
		{
			FlushDIKState ();
		}
	}
}

void I_CheckNativeMouse (bool preferNative)
{
	bool wantNative = !HaveFocus ||
		((!screen || !screen->IsFullscreen()) && (GUICapture || paused || preferNative));

//		Printf ("%d -> %d\n", NativeMouse, wantNative);
	if (wantNative != NativeMouse)
	{
		NativeMouse = wantNative;
		if (wantNative)
		{
			if (mousemode == dinput)
			{
				DI_Unacquire (g_pMouse);
			}
			else
			{
				UngrabMouse_Win32 ();
				SetCursorPos (UngrabbedPointerPos.x, UngrabbedPointerPos.y);
			}
			FlushDIKState (KEY_MOUSE1, KEY_MOUSE4);
		}
		else
		{
			if (mousemode == win32)
			{
				GetCursorPos (&UngrabbedPointerPos);
				GrabMouse_Win32 ();
			}
			else
			{
				DI_Acquire (g_pMouse);
			}
		}
	}
}

LRESULT CALLBACK WndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	event_t event;

	memset (&event, 0, sizeof(event));

	switch (message)
	{
	case WM_DESTROY:
		SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		//PostQuitMessage (0);
		exit (0);
		break;

	case WM_HOTKEY:
		break;

	case WM_PAINT:
		if (screen != NULL)
		{
			static_cast<DDrawFB *> (screen)->PaintToWindow ();
		}
		return DefWindowProc (hWnd, message, wParam, lParam);

	case WM_SETTINGCHANGE:
		// In case regional settings were changed, reget preferred languages
		language.Callback ();
		return 0;

	case WM_KILLFOCUS:
		if (g_pKey) g_pKey->Unacquire ();
		
		FlushDIKState ();
		HaveFocus = false;
		I_CheckNativeMouse (true);	// Make sure mouse gets released right away
		if (paused == 0)
		{
			S_PauseSound ();
			if (!netgame)
			{
				paused = -1;
			}
		}
		if (!noidle)
			SetPriorityClass (GetCurrentProcess(), IDLE_PRIORITY_CLASS);
		break;

	case WM_SETFOCUS:
		SetPriorityClass (GetCurrentProcess(), INGAME_PRIORITY_CLASS);
		if (g_pKey)
			DI_Acquire (g_pKey);
		HaveFocus = true;
		if (paused <= 0)
		{
			S_ResumeSound ();
			if (!netgame)
			{
				paused = 0;
			}
		}
		break;

	case WM_SIZE:
		if (mousemode == win32 && !NativeMouse &&
			(wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED))
		{
			CenterMouse_Win32 (-1, -1);
			return 0;
		}
		InvalidateRect (Window, NULL, FALSE);
		break;

	case WM_MOVE:
		if (mousemode == win32 && !NativeMouse)
		{
			CenterMouse_Win32 (-1, -1);
			return 0;
		}
		break;

	// Being forced to separate my keyboard input handler into
	// two pieces like this really stinks. (IMHO)
	case WM_KEYDOWN:
	case WM_KEYUP:
		GetKeyboardState (KeyState);
		if (GUICapture)
		{
			event.type = EV_GUI_Event;
			if (message == WM_KEYUP)
			{
				event.subtype = EV_GUI_KeyUp;
			}
			else
			{
				event.subtype = (lParam & 0x40000000) ? EV_GUI_KeyRepeat : EV_GUI_KeyDown;
			}
			event.data3 = ((KeyState[VK_SHIFT]&128) ? GKM_SHIFT : 0) |
						  ((KeyState[VK_CONTROL]&128) ? GKM_CTRL : 0) |
						  ((KeyState[VK_MENU]&128) ? GKM_ALT : 0);
			if ( (event.data1 = MapVirtualKey (wParam, 2)) )
			{
				ToAscii (wParam, (lParam >> 16) & 255, KeyState, (LPWORD)&event.data2, 0);
				D_PostEvent (&event);
			}
			else
			{
				switch (wParam)
				{
				case VK_PRIOR:	event.data1 = GK_PGUP;		break;
				case VK_NEXT:	event.data1 = GK_PGDN;		break;
				case VK_END:	event.data1 = GK_END;		break;
				case VK_HOME:	event.data1 = GK_HOME;		break;
				case VK_LEFT:	event.data1 = GK_LEFT;		break;
				case VK_RIGHT:	event.data1 = GK_RIGHT;		break;
				case VK_UP:		event.data1 = GK_UP;		break;
				case VK_DOWN:	event.data1 = GK_DOWN;		break;
				case VK_DELETE:	event.data1 = GK_DEL;		break;
				case VK_ESCAPE:	event.data1 = GK_ESCAPE;	break;
				case VK_F1:		event.data1 = GK_F1;		break;
				case VK_F2:		event.data1 = GK_F2;		break;
				case VK_F3:		event.data1 = GK_F3;		break;
				case VK_F4:		event.data1 = GK_F4;		break;
				case VK_F5:		event.data1 = GK_F5;		break;
				case VK_F6:		event.data1 = GK_F6;		break;
				case VK_F7:		event.data1 = GK_F7;		break;
				case VK_F8:		event.data1 = GK_F8;		break;
				case VK_F9:		event.data1 = GK_F9;		break;
				case VK_F10:	event.data1 = GK_F10;		break;
				case VK_F11:	event.data1 = GK_F11;		break;
				case VK_F12:	event.data1 = GK_F12;		break;
				}
				if (event.data1 != 0)
				{
					event.data2 = event.data1;
					D_PostEvent (&event);
				}
			}
		}
		else
		{
			if (message == WM_KEYUP)
			{
				event.type = EV_KeyUp;
			}
			else
			{
				if (lParam & 0x40000000)
					return 0;
				else
					event.type = EV_KeyDown;
			}

			switch (wParam)
			{
				case VK_PAUSE:
					event.data1 = KEY_PAUSE;
					break;
				case VK_TAB:
					event.data1 = DIK_TAB;
					event.data2 = '\t';
					break;
				case VK_NUMLOCK:
					event.data1 = DIK_NUMLOCK;
					break;
			}
			if (event.data1)
			{
				DIKState[ActiveDIKState][event.data1] = (event.type == EV_KeyDown);
				D_PostEvent (&event);
			}
		}
		break;

	case WM_CHAR:
		if (GUICapture && wParam >= ' ')	// only send displayable characters
		{
			event.type = EV_GUI_Event;
			event.subtype = EV_GUI_Char;
			event.data1 = wParam;
			D_PostEvent (&event);
		}
		break;

	case WM_SYSCHAR:
		if (GUICapture && wParam >= '0' && wParam <= '9')	// make chat macros work
		{
			event.type = EV_GUI_Event;
			event.subtype = EV_GUI_Char;
			event.data1 = wParam;
			event.data2 = 1;
			D_PostEvent (&event);
		}
		break;

	case WM_SYSCOMMAND:
		{
			WPARAM cmdType = wParam & 0xfff0;

			// Prevent activation of the window menu with Alt-Space
			if (cmdType != SC_KEYMENU)
				return DefWindowProc (hWnd, message, wParam, lParam);
		}
		break;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		if (MakeMouseEvents && mousemode == win32)
		{
			event.type = ((message - WM_LBUTTONDOWN) % 3) ? EV_KeyUp : EV_KeyDown;
			event.data1 = KEY_MOUSE1 + (message - WM_LBUTTONDOWN) / 3;
			DIKState[ActiveDIKState][event.data1] = (event.type == EV_KeyDown);
			D_PostEvent (&event);
		}
		break;

	case WM_MOUSEWHEEL:
		if (MakeMouseEvents && mousemode == win32)
		{
			WheelMove += (short) HIWORD(wParam);
			WheelMoved ();
		}
		break;

	case WM_GETMINMAXINFO:
		if (screen && !VidResizing)
		{
			LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;
			mmi->ptMinTrackSize.x = SCREENWIDTH + GetSystemMetrics (SM_CXSIZEFRAME) * 2;
			mmi->ptMinTrackSize.y = SCREENHEIGHT + GetSystemMetrics (SM_CYSIZEFRAME) * 2 +
									GetSystemMetrics (SM_CYCAPTION);
			return 0;
		}
		break;

	case WM_ACTIVATEAPP:
		AppActive = wParam;
		break;

	case WM_PALETTECHANGED:
		if ((HWND)wParam == Window)
			break;
		// intentional fall-through

	case WM_QUERYNEWPALETTE:
		if (screen != NULL)
		{
			return screen->QueryNewPalette ();
		}
		// intentional fall-through

	default:
		return DefWindowProc (hWnd, message, wParam, lParam);
	}

	return 0;
}

/****** Stuff from Andy Bay's myjoy.c ******/

static struct {
	int X,Y,Z,R,U,V;
}					JoyBias;
static int 			JoyActive;
static int			JoyDevice;
static JOYINFOEX	JoyStats;
static JOYCAPS 		JoyCaps;

void JoyFixBias (void)
{
	JoyBias.X = (JoyCaps.wXmin + JoyCaps.wXmax) >> 1;
	JoyBias.Y = (JoyCaps.wYmin + JoyCaps.wYmax) >> 1;
//	JoyBias.Z = (JoyCaps.wZmin + JoyCaps.wZmax) >> 1;
//	JoyBias.R = (JoyCaps.wRmin + JoyCaps.wRmax) >> 1;
//	JoyBias.U = (JoyCaps.wUmin + JoyCaps.wUmax) >> 1;
//	JoyBias.V = (JoyCaps.wVmin + JoyCaps.wVmax) >> 1;
}

void DI_JoyCheck (void)
{
	event_t joyevent;
	fixed_t xscale, yscale;
	int xdead, ydead;

	if (JoyActive)
	{
		JoyStats.dwFlags = JOY_RETURNALL;
		if (joyGetPosEx (JoyDevice, &JoyStats))
		{
			JoyActive = 0;
			return;
		}
		memset (&joyevent, 0, sizeof(joyevent));
		joyevent.type = EV_Joystick;
		joyevent.x = JoyStats.dwXpos - JoyBias.X;
		joyevent.y = JoyStats.dwYpos - JoyBias.Y;

		xdead = (int)((float)JoyBias.X * joy_xthreshold);
		ydead = (int)((float)JoyBias.Y * joy_ythreshold);
		xscale = (int)(16777216 / ((float)JoyBias.X * (1 - joy_xthreshold)) * joy_xsensitivity * joy_speedmultiplier);
		yscale = (int)(16777216 / ((float)JoyBias.Y * (1 - joy_ythreshold)) * joy_ysensitivity * joy_speedmultiplier);

		if (abs (joyevent.x) < xdead)
		{
			joyevent.x = 0;
		}
		else if (joyevent.x > 0)
		{
			joyevent.x = FixedMul (joyevent.x - xdead, xscale);
		}
		else if (joyevent.x < 0)
		{
			joyevent.x = FixedMul (joyevent.x + xdead, xscale);
		}
		if (joyevent.x > 256)
			joyevent.x = 256;
		else if (joyevent.x < -256)
			joyevent.x = -256;

		if (abs (joyevent.y) < ydead)
		{
			joyevent.y = 0;
		}
		else if (joyevent.y > 0)
		{
			joyevent.y = FixedMul (joyevent.y - ydead, yscale);
		}
		else if (joyevent.y < 0)
		{
			joyevent.y = FixedMul (joyevent.y + ydead, yscale);
		}
		if (joyevent.y > 256)
			joyevent.y = 256;
		else if (joyevent.y < -256)
			joyevent.y = -256;

		D_PostEvent (&joyevent);

		{	/* Send out button up/down events */
			static DWORD oldButtons = 0;
			int i;
			DWORD buttons, mask;

			buttons = JoyStats.dwButtons;
			mask = buttons ^ oldButtons;

			joyevent.data2 = joyevent.data3 = 0;
			for (i = 0; i < 32; i++, buttons >>= 1, mask >>= 1)
			{
				if (mask & 1)
				{
					joyevent.data1 = KEY_JOY1 + i;
					if (buttons & 1)
						joyevent.type = EV_KeyDown;
					else
						joyevent.type = EV_KeyUp;
					D_PostEvent (&joyevent);
				}
			}
			oldButtons = JoyStats.dwButtons;
		}
	}
}

BOOL DI_InitJoy (void)
{
	int i;

	JoyActive = joyGetNumDevs ();
	JoyStats.dwSize = sizeof(JOYINFOEX);
	JoyDevice = -1;

	for (i = JOYSTICKID1; i <= JOYSTICKID2; i++)
	{
		if (joyGetDevCaps (i, &JoyCaps, sizeof(JOYCAPS)) != JOYERR_NOERROR)
			continue;

		JoyStats.dwFlags = JOY_RETURNALL;
		if (joyGetPosEx (i, &JoyStats) != JOYERR_NOERROR)
			continue;

		JoyDevice = i;
		break;
	}

	if (JoyDevice == -1)
		JoyActive = 0;
	else
		JoyFixBias();

	return TRUE;
}

static void DI_Acquire (LPDIRECTINPUTDEVICE mouse)
{
	mouse->Acquire ();
	if (!NativeMouse)
		SetCursorState (FALSE);
	else
		SetCursorState (TRUE);
}

static void DI_Unacquire (LPDIRECTINPUTDEVICE mouse)
{
	mouse->Unacquire ();
	SetCursorState (TRUE);
}

/****** Stuff from Andy Bay's mymouse.c ******/

/****************************************************************************
 *
 *		DIInit
 *
 *		Initialize the DirectInput variables.
 *
 ****************************************************************************/

// [RH] Obtain the mouse using standard Win32 calls. Should always work.
static void I_GetWin32Mouse ()
{
	mousemode = win32;

	if (g_pMouse)
	{
		DI_Unacquire (g_pMouse);
		g_pMouse->Release ();
		g_pMouse = NULL;
	}
	GrabMouse_Win32 ();
}

// [RH] Used to obtain DirectInput access to the mouse.
//		(Preferred for Win95, but buggy under NT.)
static BOOL I_GetDIMouse ()
{
	HRESULT hr;
	DIPROPDWORD dipdw =
		{
			{
				sizeof(DIPROPDWORD),		// diph.dwSize
				sizeof(DIPROPHEADER),		// diph.dwHeaderSize
				0,							// diph.dwObj
				DIPH_DEVICE,				// diph.dwHow
			},
			DINPUT_BUFFERSIZE,				// dwData
		};

	if (mousemode == dinput)
		return FALSE;

	mousemode = win32;	// Assume failure
	UngrabMouse_Win32 ();

	if (in_mouse == 1 || (in_mouse == 0 && OSPlatform == os_WinNT))
		return FALSE;

	// Obtain an interface to the system mouse device.
	hr = g_pdi->CreateDevice (GUID_SysMouse, &g_pMouse, NULL);

	if (FAILED(hr))
		return FALSE;

	// Set the data format to "mouse format".
	hr = g_pMouse->SetDataFormat (&c_dfDIMouse);

	if (FAILED(hr))
	{
		g_pMouse->Release ();
		g_pMouse = NULL;
		return FALSE;
	}

	// Set the cooperative level.
	hr = g_pMouse->SetCooperativeLevel ((HWND)Window,
									   DISCL_EXCLUSIVE | DISCL_FOREGROUND);

	if (FAILED(hr))
	{
		g_pMouse->Release ();
		g_pMouse = NULL;
		return FALSE;
	}


	// Set the buffer size to DINPUT_BUFFERSIZE elements.
	// The buffer size is a DWORD property associated with the device.
	hr = g_pMouse->SetProperty (DIPROP_BUFFERSIZE, &dipdw.diph);

	if (FAILED(hr))
	{
		Printf ("Could not set mouse buffer size");
		g_pMouse->Release ();
		g_pMouse = NULL;
		return FALSE;
	}

	DI_Acquire (g_pMouse);

	mousemode = dinput;
	return TRUE;
}

BOOL I_InitInput (void *hwnd)
{
	DIRECTINPUTCREATE_FUNCTION DirectInputCreateFunction;
	HRESULT hr;

	atterm (I_ShutdownInput);

	NativeMouse = true;

	noidle = !!Args.CheckParm ("-noidle");

	// [RH] Removed dependence on existance of dinput.lib when linking.
	DirectInputInstance = (HMODULE)LoadLibrary ("dinput.dll");
	if (!DirectInputInstance)
		I_FatalError ("Sorry, you need Microsoft's DirectX 3 or higher installed.\n\n"
					  "Go grab it at http://www.microsoft.com/directx\n");

	DirectInputCreateFunction = (DIRECTINPUTCREATE_FUNCTION)GetProcAddress (DirectInputInstance, "DirectInputCreateA");
	if (!DirectInputCreateFunction)
		I_FatalError ("Could not get address of DirectInputCreateA");

	// Register with DirectInput and get an IDirectInput to play with.
	hr = DirectInputCreateFunction (g_hInst, DIRECTINPUT_VERSION, &g_pdi, NULL);

	if (FAILED(hr))
		I_FatalError ("Could not obtain DirectInput interface");

	if (!I_GetDIMouse ())
		I_GetWin32Mouse ();

	DI_Init2();

	return TRUE;
}


// Free all input resources
void STACK_ARGS I_ShutdownInput ()
{
	if (g_pKey)
	{
		g_pKey->Unacquire ();
		g_pKey->Release ();
		g_pKey = NULL;
	}
	if (g_pMouse)
	{
		DI_Unacquire (g_pMouse);
		g_pMouse->Release ();
		g_pMouse = NULL;
	}
	UngrabMouse_Win32 ();
	if (g_pdi)
	{
		g_pdi->Release ();
		g_pdi = NULL;
	}
	// [RH] Close dinput.dll
	if (DirectInputInstance)
	{
		FreeLibrary (DirectInputInstance);
		DirectInputInstance = NULL;
	}
}

static LONG PrevX, PrevY;

static void CenterMouse_Win32 (LONG curx, LONG cury)
{
	RECT rect;

	GetWindowRect (Window, &rect);

	const LONG centx = (rect.left + rect.right) >> 1;
	const LONG centy = (rect.top + rect.bottom) >> 1;

	// Reduce the number of WM_MOUSEMOVE messages that get sent
	// by only calling SetCursorPos when we really need to.
	if (centx != curx || centy != cury)
	{
		PrevX = centx;
		PrevY = centy;
		SetCursorPos (centx, centy);
	}
}

static void SetCursorState (int visible)
{
	HCURSOR usingCursor = visible ? TheArrowCursor : TheInvisibleCursor;
	SetClassLong (Window, GCL_HCURSOR, (LONG)usingCursor);
	if (HaveFocus)
	{
		SetCursor (usingCursor);
	}
}

static void GrabMouse_Win32 ()
{
	RECT rect;

	ClipCursor (NULL);		// helps with Win95?
	GetWindowRect (Window, &rect);

	// Reposition the rect so that it only covers the client area.
	// Is there some way to actually get this information from
	// the window itself instead of assuming that window metrics
	// are the same across all windows?
	rect.left += GetSystemMetrics (SM_CXFRAME);
	rect.right -= GetSystemMetrics (SM_CXFRAME);
	rect.top += GetSystemMetrics (SM_CYFRAME) + GetSystemMetrics (SM_CYCAPTION);
	rect.bottom -= GetSystemMetrics (SM_CYFRAME);

	ClipCursor (&rect);
	SetCursorState (FALSE);
	CenterMouse_Win32 (-1, -1);
	MakeMouseEvents = true;
}

static void UngrabMouse_Win32 ()
{
	ClipCursor (NULL);
	SetCursorState (TRUE);
	MakeMouseEvents = false;
}

static void WheelMoved ()
{
	event_t event;
	int dir;

	memset (&event, 0, sizeof(event));
	if (GUICapture)
	{
		event.type = EV_GUI_Event;
		if (WheelMove < 0)
		{
			dir = WHEEL_DELTA;
			event.subtype = EV_GUI_WheelDown;
		}
		else
		{
			dir = -WHEEL_DELTA;
			event.subtype = EV_GUI_WheelUp;
		}
		while (abs (WheelMove) >= WHEEL_DELTA)
		{
			D_PostEvent (&event);
			WheelMove += dir;
		}
	}
	else
	{
		if (WheelMove < 0)
		{
			dir = WHEEL_DELTA;
			event.data1 = KEY_MWHEELDOWN;
		}
		else
		{
			dir = -WHEEL_DELTA;
			event.data1 = KEY_MWHEELUP;
		}
		while (abs (WheelMove) >= WHEEL_DELTA)
		{
			event.type = EV_KeyDown;
			D_PostEvent (&event);
			event.type = EV_KeyUp;
			D_PostEvent (&event);
			WheelMove += dir;
		}
	}
}

static void MouseRead_Win32 ()
{
	POINT pt;
	event_t ev;
	int x, y;

	if (!HaveFocus || !MakeMouseEvents || !GetCursorPos (&pt))
		return;

	x = (pt.x - PrevX) * 3;
	y = (PrevY - pt.y) << 1;

	CenterMouse_Win32 (pt.x, pt.y);

	if (x | y)
	{
		memset (&ev, 0, sizeof(ev));
		ev.x = x;
		ev.y = y;
		ev.type = EV_Mouse;
		D_PostEvent (&ev);
	}
}

static void MouseRead_DI ()
{
	DIDEVICEOBJECTDATA od;
	DWORD dwElements;
	HRESULT hr;
	int count = 0;

	event_t event;
	GDx=0;
	GDy=0;

	if (!g_pMouse)
		return;

	memset (&event, 0, sizeof(event));
	for (;;)
	{
		dwElements = 1;
		hr = g_pMouse->GetDeviceData (sizeof(DIDEVICEOBJECTDATA), &od,
							 &dwElements, 0);
		if (hr == DIERR_INPUTLOST)
		{
			DI_Acquire (g_pMouse);
			hr = g_pMouse->GetDeviceData (sizeof(DIDEVICEOBJECTDATA), &od,
								 &dwElements, 0);
		}

		/* Unable to read data or no data available */
		if (FAILED(hr) || !dwElements)
			break;

		count++;

		/* Look at the element to see what happened */
		switch (od.dwOfs)
		{
		case DIMOFS_X:	GDx += od.dwData;						break;
		case DIMOFS_Y:	GDy += od.dwData;						break;
		case DIMOFS_Z:	WheelMove += od.dwData; WheelMoved ();	break;

		/* [RH] Mouse button events now mimic keydown/up events */
		case DIMOFS_BUTTON0:
		case DIMOFS_BUTTON1:
		case DIMOFS_BUTTON2:
		case DIMOFS_BUTTON3:
			if (!GUICapture)
			{
				event.type = (od.dwData & 0x80) ? EV_KeyDown : EV_KeyUp;
				event.data1 = KEY_MOUSE1 + (od.dwOfs - DIMOFS_BUTTON0);
				DIKState[ActiveDIKState][event.data1] = (event.type == EV_KeyDown);
				D_PostEvent (&event);
			}
			break;
		}
	}

	if (count)
	{
		memset (&event, 0, sizeof(event));
		event.type = EV_Mouse;
		event.x = GDx<<2;
		event.y = -GDy;
		D_PostEvent (&event);
	}
}

// Initialize the keyboard
static BOOL DI_Init2 (void)
{
	int hr;

	// Obtain an interface to the system key device.
	hr = g_pdi->CreateDevice (GUID_SysKeyboard, &g_pKey, NULL);

	if (FAILED(hr))
	{
		I_FatalError ("Could not create keyboard device");
	}

	// [RH] Prepare c_dfDIKeyboard for use.
	InitKeyboardObjectData ();

	// Set the data format to "keyboard format".
	hr = g_pKey->SetDataFormat (&c_dfDIKeyboard);

	if (FAILED(hr))
	{
		I_FatalError ("Could not set keyboard data format");
	}

	// Set the cooperative level.
	hr = g_pKey->SetCooperativeLevel (Window, DISCL_FOREGROUND|DISCL_NONEXCLUSIVE);

	if (FAILED(hr))
	{
		I_FatalError("Could not set keyboard cooperative level");
	}

	g_pKey->Acquire ();

	DI_InitJoy ();
	return TRUE;
}

static void KeyRead ()
{
	HRESULT hr;
	event_t event;
	BYTE *fromState, *toState;
	int i;

	memset (&event, 0, sizeof(event));
	fromState = DIKState[ActiveDIKState];
	toState = DIKState[ActiveDIKState ^ 1];

	hr = g_pKey->GetDeviceState (256, toState);
	if (hr == DIERR_INPUTLOST)
	{
		hr = g_pKey->Acquire ();
		if (hr != DI_OK)
		{
			return;
		}
		hr = g_pKey->GetDeviceState (256, toState);
	}
	if (hr != DI_OK)
	{
		return;
	}

	// Successfully got the buffer
	ActiveDIKState ^= 1;

	// Copy key states not handled here from the old to the new buffer
	memcpy (toState + 256, fromState + 256, NUM_KEYS - 256);
	toState[DIK_TAB] = fromState[DIK_TAB];
	toState[DIK_NUMLOCK] = fromState[DIK_NUMLOCK];

	// "Merge" multiple keys that are considered to be the same.
	// Also clear out the alternate versions after merging.
	toState[DIK_RETURN]		|= toState[DIK_NUMPADENTER];
	toState[DIK_LMENU]		|= toState[DIK_RMENU];
	toState[DIK_LCONTROL]	|= toState[DIK_RCONTROL];
	toState[DIK_LSHIFT]		|= toState[DIK_RSHIFT];

	toState[DIK_NUMPADENTER] = 0;
	toState[DIK_RMENU]		 = 0;
	toState[DIK_RCONTROL]	 = 0;
	toState[DIK_RSHIFT]		 = 0;

	if (i_remapkeypad)
	{
		toState[DIK_LEFT]	|= toState[DIK_NUMPAD4];
		toState[DIK_RIGHT]	|= toState[DIK_NUMPAD6];
		toState[DIK_UP]		|= toState[DIK_NUMPAD8];
		toState[DIK_DOWN]	|= toState[DIK_NUMPAD2];
		toState[DIK_HOME]	|= toState[DIK_NUMPAD7];
		toState[DIK_PRIOR]	|= toState[DIK_NUMPAD9];
		toState[DIK_NEXT]	|= toState[DIK_NUMPAD3];
		toState[DIK_END]	|= toState[DIK_NUMPAD1];
		toState[DIK_INSERT]	|= toState[DIK_NUMPAD0];
		toState[DIK_DELETE]	|= toState[DIK_DECIMAL];

		toState[DIK_NUMPAD4] = 0;
		toState[DIK_NUMPAD6] = 0;
		toState[DIK_NUMPAD8] = 0;
		toState[DIK_NUMPAD2] = 0;
		toState[DIK_NUMPAD7] = 0;
		toState[DIK_NUMPAD9] = 0;
		toState[DIK_NUMPAD3] = 0;
		toState[DIK_NUMPAD1] = 0;
		toState[DIK_NUMPAD0] = 0;
		toState[DIK_DECIMAL] = 0;
	}

	// Now generate events for any keys that differ between the states
	if (!GUICapture)
	{
		for (i = 1; i < 256; i++)
		{
			if (toState[i] != fromState[i])
			{
				event.type = toState[i] ? EV_KeyDown : EV_KeyUp;
				event.data1 = i;
				event.data2 = Convert[i];
				D_PostEvent (&event);
			}
		}
	}
}

void I_GetEvent ()
{
	MSG mess;

//	for (;;) {
		while (PeekMessage (&mess, NULL, 0, 0, PM_REMOVE))
		{
			if (mess.message == WM_QUIT)
				exit (mess.wParam);
			TranslateMessage (&mess);
			DispatchMessage (&mess);
		}
//		if (havefocus || netgame || gamestate != GS_LEVEL)
//			break;
//	}

	KeyRead ();

	if (use_mouse)
	{
		if (mousemode == dinput)
			MouseRead_DI ();
		else
			MouseRead_Win32 ();
	}
	if (use_joystick)
	{
		DI_JoyCheck ();
	}
}


//
// I_StartTic
//
void I_StartTic ()
{
	ResetButtonTriggers ();
	I_CheckGUICapture ();
	I_CheckNativeMouse (false);
	I_GetEvent ();
}

//
// I_StartFrame
//
void I_StartFrame ()
{
}

void I_PutInClipboard (const char *str)
{
	if (str == NULL || !OpenClipboard (Window))
		return;
	EmptyClipboard ();

	HGLOBAL cliphandle = GlobalAlloc (GMEM_DDESHARE, strlen (str) + 1);
	if (cliphandle != NULL)
	{
		char *ptr = (char *)GlobalLock (cliphandle);
		strcpy (ptr, str);
		GlobalUnlock (cliphandle);
		SetClipboardData (CF_TEXT, cliphandle);
	}
	CloseClipboard ();
}

char *I_GetFromClipboard ()
{
	char *retstr = NULL;
	HGLOBAL cliphandle;
	char *clipstr;
	char *nlstr;

	if (!IsClipboardFormatAvailable (CF_TEXT) || !OpenClipboard (Window))
		return NULL;

	cliphandle = GetClipboardData (CF_TEXT);
	if (cliphandle != NULL)
	{
		clipstr = (char *)GlobalLock (cliphandle);
		if (clipstr != NULL)
		{
			retstr = copystring (clipstr);
			GlobalUnlock (clipstr);
			nlstr = retstr;

			// Convert CR-LF pairs to just LF
			while ( (nlstr = strstr (retstr, "\r\n")) )
			{
				memmove (nlstr, nlstr + 1, strlen (nlstr) - 1);
			}
		}
	}

	CloseClipboard ();
	return retstr;
}

#include "i_movie.h"

CCMD (playmovie)
{
	if (argv.argc() != 2)
	{
		Printf ("Usage: playmovie <movie name>\n");
		return;
	}
	I_PlayMovie (argv[1]);
}
