/*
** i_input.cpp
** Handles input from keyboard, mouse, and joystick
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

// DI3 only supports up to 4 mouse buttons, and I want the joystick to
// be read using DirectInput instead of winmm.

#define DIRECTINPUT_VERSION 0x800
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0501)
#define _WIN32_WINNT 0x0501			// Support the mouse wheel and session notification.
#endif

#define WIN32_LEAN_AND_MEAN
#define __BYTEBOOL__
#ifndef __GNUC__
#define INITGUID
#endif
#include <windows.h>
#include <mmsystem.h>
#include <dbt.h>
#include <dinput.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

// Compensate for w32api's lack
#ifndef WM_WTSSESSION_CHANGE
#define WM_WTSSESSION_CHANGE 0x02B1
#define WTS_CONSOLE_CONNECT 1
#define WTS_CONSOLE_DISCONNECT 2
#define WTS_SESSION_LOCK 7
#define WTS_SESSION_UNLOCK 8
#endif
#ifndef PBT_APMSUSPEND
// w32api does not #define the PBT_ macros in winuser.h like the PSDK does
#include <pbt.h>
#endif
#ifndef GET_RAWINPUT_CODE_WPARAM
#define GET_RAWINPUT_CODE_WPARAM(wParam)	((wParam) & 0xff)
#endif


#define USE_WINDOWS_DWORD
#include "c_dispatch.h"
#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_argv.h"
#include "i_input.h"
#include "v_video.h"
#include "i_sound.h"
#include "m_menu.h"
#include "g_game.h"
#include "d_main.h"
#include "d_gui.h"
#include "c_console.h"
#include "c_cvars.h"
#include "i_system.h"
#include "s_sound.h"
#include "m_misc.h"
#include "gameconfigfile.h"
#include "win32iface.h"
#include "templates.h"
#include "cmdlib.h"
#include "d_event.h"
#include "v_text.h"

// Prototypes and declarations.
#include "rawinput.h"
// Definitions
#define RIF(name, ret, args) \
	name##Proto My##name;
#include "rawinput.h"



#ifdef _DEBUG
#define INGAME_PRIORITY_CLASS	NORMAL_PRIORITY_CLASS
#else
//#define INGAME_PRIORITY_CLASS	HIGH_PRIORITY_CLASS
#define INGAME_PRIORITY_CLASS	NORMAL_PRIORITY_CLASS
#endif

static void FindRawInputFunctions();
BOOL DI_InitJoy (void);

extern HINSTANCE g_hInst;
extern DWORD SessionID;

extern void ShowEAXEditor ();
extern bool SpawnEAXWindow;

static HMODULE DInputDLL;

static HRESULT InitJoystick ();

bool GUICapture;
extern FMouse *Mouse;
extern FKeyboard *Keyboard;

bool VidResizing;

extern bool SpawnEAXWindow;
extern BOOL vidactive;
extern HWND Window, ConWindow;
extern HWND EAXEditWindow;

extern void UpdateJoystickMenu ();
extern menu_t JoystickMenu;

EXTERN_CVAR (String, language)
EXTERN_CVAR (Bool, lookstrafe)

static int WheelDelta;

extern BOOL paused;
static bool noidle = false;

LPDIRECTINPUT8			g_pdi;
LPDIRECTINPUT			g_pdi3;

static LPDIRECTINPUTDEVICE8		g_pJoy;

TArray<GUIDName> JoystickNames;

static DIDEVCAPS JoystickCaps;

float JoyAxes[6];
static int JoyActive;
static BYTE JoyButtons[128];
static BYTE JoyPOV[4];
static BYTE JoyAxisMap[8];
static float JoyAxisThresholds[8];
char *JoyAxisNames[8];
static const size_t Axes[8] =
{
	myoffsetof(DIJOYSTATE2,lX),
	myoffsetof(DIJOYSTATE2,lY),
	myoffsetof(DIJOYSTATE2,lZ),
	myoffsetof(DIJOYSTATE2,lRx),
	myoffsetof(DIJOYSTATE2,lRy),
	myoffsetof(DIJOYSTATE2,lRz),
	myoffsetof(DIJOYSTATE2,rglSlider[0]),
	myoffsetof(DIJOYSTATE2,rglSlider[1])
};
static const BYTE POVButtons[9] = { 0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09, 0x00 };

BOOL AppActive = TRUE;
int SessionState = 0;

CVAR (Bool,  use_joystick,			false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (GUID, joy_guid,		NULL, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (g_pJoy != NULL)
	{
		DIDEVICEINSTANCE inst = { sizeof(DIDEVICEINSTANCE), };

		if (SUCCEEDED(g_pJoy->GetDeviceInfo (&inst)) && self != inst.guidInstance)
		{
			DI_InitJoy ();
			UpdateJoystickMenu ();
		}
	}
	else
	{
		DI_InitJoy ();
		UpdateJoystickMenu ();
	}
}

static void MapAxis (FIntCVar &var, int num)
{
	if (var < JOYAXIS_NONE || var > JOYAXIS_UP)
	{
		var = JOYAXIS_NONE;
	}
	else
	{
		JoyAxisMap[num] = var;
	}
}

CUSTOM_CVAR (Int, joy_xaxis,	JOYAXIS_YAW,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 0);
}
CUSTOM_CVAR (Int, joy_yaxis,	JOYAXIS_FORWARD, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 1);
}
CUSTOM_CVAR (Int, joy_zaxis,	JOYAXIS_SIDE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 2);
}
CUSTOM_CVAR (Int, joy_xrot,		JOYAXIS_NONE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 3);
}
CUSTOM_CVAR (Int, joy_yrot,		JOYAXIS_NONE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 4);
}
CUSTOM_CVAR (Int, joy_zrot,		JOYAXIS_PITCH,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 5);
}
CUSTOM_CVAR (Int, joy_slider,	JOYAXIS_NONE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 6);
}
CUSTOM_CVAR (Int, joy_dial,		JOYAXIS_NONE,	 CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	MapAxis (self, 7);
}

CUSTOM_CVAR (Float, joy_xthreshold,		0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[0] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_ythreshold,		0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[1] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_zthreshold,		0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[2] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_xrotthreshold,	0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[3] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_yrotthreshold,	0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[4] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_zrotthreshold,	0.15f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[5] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_sliderthreshold,	0.f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[6] = clamp (self * 256.f, 0.f, 256.f);
}
CUSTOM_CVAR (Float, joy_dialthreshold,	0.f,	CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	JoyAxisThresholds[7] = clamp (self * 256.f, 0.f, 256.f);
}

CVAR (Float, joy_speedmultiplier,1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_yawspeed,		-1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_pitchspeed,	-.75f,CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_forwardspeed,	-1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_sidespeed,		 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_upspeed,		-1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR (Bool, k_allowfullscreentoggle, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

static FBaseCVar * const JoyConfigVars[] =
{
	&joy_xaxis, &joy_yaxis, &joy_zaxis, &joy_xrot, &joy_yrot, &joy_zrot, &joy_slider, &joy_dial,
	&joy_xthreshold, &joy_ythreshold, &joy_zthreshold, &joy_xrotthreshold, &joy_yrotthreshold, &joy_zrotthreshold, &joy_sliderthreshold, &joy_dialthreshold,
	&joy_speedmultiplier, &joy_yawspeed, &joy_pitchspeed, &joy_forwardspeed, &joy_sidespeed,
	&joy_upspeed
};

extern int chatmodeon;

static void I_CheckGUICapture ()
{
	bool wantCapt;

	if (menuactive == MENU_Off)
	{
		wantCapt = ConsoleState == c_down || ConsoleState == c_falling || chatmodeon;
	}
	else
	{
		wantCapt = (menuactive == MENU_On || menuactive == MENU_OnNoPause);
	}

	if (wantCapt != GUICapture)
	{
		GUICapture = wantCapt;
		if (wantCapt && Keyboard != NULL)
		{
			Keyboard->AllKeysUp();
		}
	}
}

bool GUIWndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	event_t ev = { EV_GUI_Event };

	*result = 0;

	switch (message)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		if (message == WM_KEYUP || message == WM_SYSKEYUP)
		{
			ev.subtype = EV_GUI_KeyUp;
		}
		else
		{
			ev.subtype = (lParam & 0x40000000) ? EV_GUI_KeyRepeat : EV_GUI_KeyDown;
		}
		if (GetKeyState(VK_SHIFT) & 0x8000)		ev.data3 |= GKM_SHIFT;
		if (GetKeyState(VK_CONTROL) & 0x8000)	ev.data3 |= GKM_CTRL;
		if (GetKeyState(VK_MENU) & 0x8000)		ev.data3 |= GKM_ALT;
		if ( (ev.data1 = MapVirtualKey(wParam, 2)) )
		{
			D_PostEvent(&ev);
		}
		else
		{
			switch (wParam)
			{
			case VK_PRIOR:	ev.data1 = GK_PGUP;		break;
			case VK_NEXT:	ev.data1 = GK_PGDN;		break;
			case VK_END:	ev.data1 = GK_END;		break;
			case VK_HOME:	ev.data1 = GK_HOME;		break;
			case VK_LEFT:	ev.data1 = GK_LEFT;		break;
			case VK_RIGHT:	ev.data1 = GK_RIGHT;	break;
			case VK_UP:		ev.data1 = GK_UP;		break;
			case VK_DOWN:	ev.data1 = GK_DOWN;		break;
			case VK_DELETE:	ev.data1 = GK_DEL;		break;
			case VK_ESCAPE:	ev.data1 = GK_ESCAPE;	break;
			case VK_F1:		ev.data1 = GK_F1;		break;
			case VK_F2:		ev.data1 = GK_F2;		break;
			case VK_F3:		ev.data1 = GK_F3;		break;
			case VK_F4:		ev.data1 = GK_F4;		break;
			case VK_F5:		ev.data1 = GK_F5;		break;
			case VK_F6:		ev.data1 = GK_F6;		break;
			case VK_F7:		ev.data1 = GK_F7;		break;
			case VK_F8:		ev.data1 = GK_F8;		break;
			case VK_F9:		ev.data1 = GK_F9;		break;
			case VK_F10:	ev.data1 = GK_F10;		break;
			case VK_F11:	ev.data1 = GK_F11;		break;
			case VK_F12:	ev.data1 = GK_F12;		break;
			}
			if (ev.data1 != 0)
			{
				D_PostEvent(&ev);
			}
		}
		// Return false for key downs so that we can handle special hotkeys
		// in the main WndProc.
		return ev.subtype == EV_GUI_KeyUp;

	case WM_CHAR:
	case WM_SYSCHAR:
		if (wParam >= ' ')		// only send displayable characters
		{
			ev.subtype = EV_GUI_Char;
			ev.data1 = wParam;
			ev.data2 = (message == WM_SYSCHAR);
			D_PostEvent(&ev);
			return true;
		}
		break;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		if (message >= WM_LBUTTONDOWN && message <= WM_LBUTTONDBLCLK)
		{
			ev.subtype = message - WM_LBUTTONDOWN + EV_GUI_LButtonDown;
		}
		else if (message >= WM_RBUTTONDOWN && message <= WM_RBUTTONDBLCLK)
		{
			ev.subtype = message - WM_RBUTTONDOWN + EV_GUI_RButtonDown;
		}
		else if (message >= WM_MBUTTONDOWN && message <= WM_MBUTTONDBLCLK)
		{
			ev.subtype = message - WM_MBUTTONDOWN + EV_GUI_MButtonDown;
		}
		D_PostEvent(&ev);
		return true;

	// Note: If the mouse is grabbed, it sends the mouse wheel events itself.
	case WM_MOUSEWHEEL:
		if (wParam & MK_SHIFT)				ev.data3 |= GKM_SHIFT;
		if (wParam & MK_CONTROL)			ev.data3 |= GKM_CTRL;
		if (GetKeyState(VK_MENU) & 0x8000)	ev.data3 |= GKM_ALT;
		WheelDelta += (SHORT)HIWORD(wParam);
		if (WheelDelta < 0)
		{
			ev.subtype = EV_GUI_WheelDown;
			while (WheelDelta <= -WHEEL_DELTA)
			{
				D_PostEvent(&ev);
				WheelDelta += WHEEL_DELTA;
			}
		}
		else
		{
			ev.subtype = EV_GUI_WheelUp;
			while (WheelDelta >= WHEEL_DELTA)
			{
				D_PostEvent(&ev);
				WheelDelta -= WHEEL_DELTA;
			}
		}
		return true;
	}
	return false;
}

bool CallHook(FInputDevice *device, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	if (device == NULL)
	{
		return false;
	}
	*result = 0;
	return device->WndProcHook(hWnd, message, wParam, lParam, result);
}

LRESULT CALLBACK WndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result;

	if (message == WM_INPUT)
	{
		if (MyGetRawInputData != NULL)
		{
			UINT size;

			if (!MyGetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER)) &&
				size != 0)
			{
				BYTE *buffer = (BYTE *)alloca(size);
				if (MyGetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == size)
				{
					int code = GET_RAWINPUT_CODE_WPARAM(wParam);
					if (Keyboard == NULL || !Keyboard->ProcessRawInput((RAWINPUT *)buffer, code))
					{
						if (Mouse != NULL)
						{
							Mouse->ProcessRawInput((RAWINPUT *)buffer, code);
						}
					}
				}
			}
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	if (CallHook(Keyboard, hWnd, message, wParam, lParam, &result))
	{
		return result;
	}
	if (CallHook(Mouse, hWnd, message, wParam, lParam, &result))
	{
		return result;
	}
	if (GUICapture && GUIWndProcHook(hWnd, message, wParam, lParam, &result))
	{
		return result;
	}

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
		if (screen != NULL && 0)
		{
			static_cast<BaseWinFB *> (screen)->PaintToWindow ();
		}
		return DefWindowProc (hWnd, message, wParam, lParam);

	case WM_SETTINGCHANGE:
		// If regional settings were changed, reget preferred languages
		if (wParam == 0 && lParam != 0 && strcmp ((const char *)lParam, "intl") == 0)
		{
			language.Callback ();
		}
		return 0;

	case WM_KILLFOCUS:
		I_CheckNativeMouse (true);	// Make sure mouse gets released right away
		break;

	case WM_SETFOCUS:
		I_CheckNativeMouse (false);
		break;

	case WM_SIZE:
		InvalidateRect (Window, NULL, FALSE);
		break;

	case WM_KEYDOWN:
		// When the EAX editor is open, pressing Ctrl+Tab will switch to it
		if (EAXEditWindow != 0 && wParam == VK_TAB && !(lParam & 0x40000000) &&
			(GetKeyState (VK_CONTROL) & 0x8000))
		{
			SetForegroundWindow (EAXEditWindow);
		}
		break;

	case WM_SYSKEYDOWN:
		// Pressing Alt+Enter can toggle between fullscreen and windowed.
		if (wParam == VK_RETURN && k_allowfullscreentoggle && !(lParam & 0x40000000))
		{
			ToggleFullscreen = !ToggleFullscreen;
		}
		break;

	case WM_SYSCOMMAND:
		// Prevent activation of the window menu with Alt+Space
		if ((wParam & 0xFFF0) != SC_KEYMENU)
		{
			return DefWindowProc (hWnd, message, wParam, lParam);
		}
		break;

	case WM_DISPLAYCHANGE:
		if (SpawnEAXWindow)
		{
			SpawnEAXWindow = false;
			ShowEAXEditor ();
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
		if (wParam)
		{
			SetPriorityClass (GetCurrentProcess (), INGAME_PRIORITY_CLASS);
		}
		else if (!noidle && !netgame)
		{
			SetPriorityClass (GetCurrentProcess (), IDLE_PRIORITY_CLASS);
		}
		S_SetSoundPaused (wParam);
		break;

	case WM_WTSSESSION_CHANGE:
	case WM_POWERBROADCAST:
		{
			int oldstate = SessionState;

			if (message == WM_WTSSESSION_CHANGE && lParam == (LPARAM)SessionID)
			{
#ifdef _DEBUG
				OutputDebugString ("SessionID matched\n");
#endif
				// When using fast user switching, XP will lock a session before
				// disconnecting it, and the session will be unlocked before reconnecting it.
				// For our purposes, video output will only happen when the session is
				// both unlocked and connected (that is, SessionState is 0).
				switch (wParam)
				{
				case WTS_SESSION_LOCK:
					SessionState |= 1;
					break;
				case WTS_SESSION_UNLOCK:
					SessionState &= ~1;
					break;
				case WTS_CONSOLE_DISCONNECT:
					SessionState |= 2;
					//I_MovieDisableSound ();
					break;
				case WTS_CONSOLE_CONNECT:
					SessionState &= ~2;
					//I_MovieResumeSound ();
					break;
				}
			}
			else if (message == WM_POWERBROADCAST)
			{
				switch (wParam)
				{
				case PBT_APMSUSPEND:
					SessionState |= 4;
					break;
				case PBT_APMRESUMESUSPEND:
					SessionState &= ~4;
					break;
				}
			}

			if (GSnd != NULL)
			{
#if 0
				// Do we actually need this here?
				if (!oldstate && SessionState)
				{
					GSnd->SuspendSound ();
				}
				else if (oldstate && !SessionState)
				{
					GSnd->MovieResumeSound ();
				}
#endif
			}
#ifdef _DEBUG
			char foo[256];
			mysnprintf (foo, countof(foo), "Session Change: %ld %d\n", lParam, wParam);
			OutputDebugString (foo);
#endif
		}
		break;

	case WM_DEVICECHANGE:
		if (wParam == DBT_DEVNODES_CHANGED ||
			wParam == DBT_DEVICEARRIVAL ||
			wParam == DBT_CONFIGCHANGED)
		{
			unsigned int i;
			TArray<GUID> oldjoys;

			for (i = 0; i < JoystickNames.Size(); ++i)
			{
				oldjoys.Push (JoystickNames[i].ID);
			}

			DI_EnumJoy ();

			// If a new joystick was added and the joystick menu is open,
			// switch to it.
			if (menuactive != MENU_Off && CurrentMenu == &JoystickMenu)
			{
				for (i = 0; i < JoystickNames.Size(); ++i)
				{
					bool wasListed = false;

					for (unsigned int j = 0; j < oldjoys.Size(); ++j)
					{
						if (oldjoys[j] == JoystickNames[i].ID)
						{
							wasListed = true;
							break;
						}
					}
					if (!wasListed)
					{
						joy_guid = JoystickNames[i].ID;
						break;
					}
				}
			}

			// If the current joystick was removed,
			// try to switch to a different one.
			if (g_pJoy != NULL)
			{
				DIDEVICEINSTANCE inst = { sizeof(DIDEVICEINSTANCE), };

				if (SUCCEEDED(g_pJoy->GetDeviceInfo (&inst)))
				{
					for (i = 0; i < JoystickNames.Size(); ++i)
					{
						if (JoystickNames[i].ID == inst.guidInstance)
						{
							break;
						}
					}
					if (i == JoystickNames.Size ())
					{
						DI_InitJoy ();
					}
				}
			}
			else
			{
				DI_InitJoy ();
			}
			UpdateJoystickMenu ();
		}
		break;

	case WM_PALETTECHANGED:
		if ((HWND)wParam == Window)
			break;
		if (screen != NULL)
		{
			screen->PaletteChanged ();
		}
		return DefWindowProc (hWnd, message, wParam, lParam);

	case WM_QUERYNEWPALETTE:
		if (screen != NULL)
		{
			return screen->QueryNewPalette ();
		}
		return DefWindowProc (hWnd, message, wParam, lParam);

	case WM_ERASEBKGND:
		return true;

	default:
		return DefWindowProc (hWnd, message, wParam, lParam);
	}

	return 0;
}

/****** Joystick stuff ******/

void DI_JoyCheck ()
{
	float mul;
	event_t event;
	HRESULT hr;
	DIJOYSTATE2 js;
	int i;
	BYTE pov;

	if (g_pJoy == NULL)
	{
		return;
	}

	hr = g_pJoy->Poll ();
	if (FAILED(hr))
	{
		do
		{
			hr = g_pJoy->Acquire ();
		}
		while (hr == DIERR_INPUTLOST);
		if (FAILED(hr))
			return;
	}

	hr = g_pJoy->GetDeviceState (sizeof(DIJOYSTATE2), &js);
	if (FAILED(hr))
		return;

	mul = joy_speedmultiplier;
	if (Button_Speed.bDown)
	{
		mul *= 0.5f;
	}

	for (i = 0; i < 6; ++i)
	{
		JoyAxes[i] = 0.f;
	}

	for (i = 0; i < 8; ++i)
	{
		int vaxis = JoyAxisMap[i];

		if (vaxis != JOYAXIS_NONE)
		{
			if (vaxis == JOYAXIS_YAW && (Button_Strafe.bDown ||
				(Button_Mlook.bDown && lookstrafe)))
			{
				vaxis = JOYAXIS_SIDE;
			}
			else if (vaxis == JOYAXIS_FORWARD && Button_Mlook.bDown)
			{
				vaxis = JOYAXIS_PITCH;
			}

			float axisval = *((LONG *)((BYTE *)&js + Axes[i]));
			if (fabsf(axisval) > JoyAxisThresholds[i])
			{
				if (axisval > 0.f)
				{
					axisval -= JoyAxisThresholds[i];
				}
				else
				{
					axisval += JoyAxisThresholds[i];
				}
				JoyAxes[vaxis] += axisval * mul * 256.f / (256.f - JoyAxisThresholds[i]);
			}
		}
	}

	JoyAxes[JOYAXIS_YAW] *= joy_yawspeed;
	JoyAxes[JOYAXIS_PITCH] *= joy_pitchspeed;
	JoyAxes[JOYAXIS_FORWARD] *= joy_forwardspeed;
	JoyAxes[JOYAXIS_SIDE] *= joy_sidespeed;
	JoyAxes[JOYAXIS_UP] *= joy_upspeed;

	event.data2 = event.data3 = 0;

	// Send button up/down events

	for (i = 0; i < 128; ++i)
	{
		if ((js.rgbButtons[i] ^ JoyButtons[i]) & 0x80)
		{
			event.data1 = KEY_FIRSTJOYBUTTON + i;
			if (JoyButtons[i])
			{
				event.type = EV_KeyUp;
				JoyButtons[i] = 0;
			}
			else
			{
				event.type = EV_KeyDown;
				JoyButtons[i] = 0x80;
			}
			D_PostEvent (&event);
		}
	}

	for (i = 0; i < 4; ++i)
	{
		if (LOWORD(js.rgdwPOV[i]) == 0xFFFF)
		{
			pov = 8;
		}
		else
		{
			pov = ((js.rgdwPOV[i] + 2250) % 36000) / 4500;
		}
		pov = POVButtons[pov];
		for (int j = 0; j < 4; ++j)
		{
			BYTE mask = 1 << j;

			if ((JoyPOV[i] ^ pov) & mask)
			{
				event.data1 = KEY_JOYPOV1_UP + i*4 + j;
				event.type = (pov & mask) ? EV_KeyDown : EV_KeyUp;
				D_PostEvent (&event);
			}
		}
		JoyPOV[i] = pov;
	}
}

bool SetJoystickSection (bool create)
{
	DIDEVICEINSTANCE inst = { sizeof(DIDEVICEINSTANCE), };
	char section[80] = "Joystick.";

	if (g_pJoy != NULL && SUCCEEDED(g_pJoy->GetDeviceInfo (&inst)))
	{
		FormatGUID (section + 9, countof(section) - 9, inst.guidInstance);
		strcpy (section + 9 + 38, ".Axes");
		return GameConfig->SetSection (section, create);
	}
	else
	{
		return false;
	}
}

void LoadJoystickConfig ()
{
	if (SetJoystickSection (false))
	{
		for (size_t i = 0; i < countof(JoyConfigVars); ++i)
		{
			const char *val = GameConfig->GetValueForKey (JoyConfigVars[i]->GetName());
			UCVarValue cval;

			if (val != NULL)
			{
				cval.String = const_cast<char *>(val);
				JoyConfigVars[i]->SetGenericRep (cval, CVAR_String);
			}
		}
	}
}

void SaveJoystickConfig ()
{
	if (SetJoystickSection (true))
	{
		GameConfig->ClearCurrentSection ();
		for (size_t i = 0; i < countof(JoyConfigVars); ++i)
		{
			UCVarValue cval = JoyConfigVars[i]->GetGenericRep (CVAR_String);
			GameConfig->SetValueForKey (JoyConfigVars[i]->GetName(), cval.String);
		}
	}
}

BOOL CALLBACK EnumJoysticksCallback (LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
	GUIDName name;

	JoyActive++;
	name.ID = lpddi->guidInstance;
	name.Name = copystring (lpddi->tszInstanceName);
	JoystickNames.Push (name);
	return DIENUM_CONTINUE;
}

void DI_EnumJoy ()
{
	unsigned int i;

	for (i = 0; i < JoystickNames.Size(); ++i)
	{
		delete[] JoystickNames[i].Name;
	}

	JoyActive = 0;
	JoystickNames.Clear ();

	if (g_pdi != NULL && !Args->CheckParm ("-nojoy"))
	{
		g_pdi->EnumDevices (DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, NULL, DIEDFL_ALLDEVICES);
	}
}

BOOL DI_InitJoy (void)
{
	HRESULT hr;
	unsigned int i;

	if (g_pdi == NULL)
	{
		return TRUE;
	}

	if (g_pJoy != NULL)
	{
		SaveJoystickConfig ();
		g_pJoy->Release ();
		g_pJoy = NULL;
	}

	if (JoystickNames.Size() == 0)
	{
		return TRUE;
	}

	// Try to obtain the joystick specified by joy_guid
	for (i = 0; i < JoystickNames.Size(); ++i)
	{
		if (JoystickNames[i].ID == joy_guid)
		{
			hr = g_pdi->CreateDevice (JoystickNames[i].ID, &g_pJoy, NULL);
			if (FAILED(hr))
			{
				i = JoystickNames.Size();
			}
			break;
		}
	}

	// If the preferred joystick could not be obtained, grab the first
	// one available.
	if (i == JoystickNames.Size())
	{
		for (i = 0; i <= JoystickNames.Size(); ++i)
		{
			hr = g_pdi->CreateDevice (JoystickNames[i].ID, &g_pJoy, NULL);
			if (SUCCEEDED(hr))
			{
				break;
			}
		}
	}

	if (i == JoystickNames.Size())
	{
		JoyActive = 0;
		return TRUE;
	}

	if (FAILED (InitJoystick ()))
	{
		JoyActive = 0;
		g_pJoy->Release ();
		g_pJoy = NULL;
	}
	else
	{
		LoadJoystickConfig ();
		joy_guid = JoystickNames[i].ID;
	}

	return TRUE;
}

BOOL CALLBACK EnumAxesCallback (LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef)
{
	DIPROPRANGE diprg =
	{
		{
			sizeof (DIPROPRANGE),
			sizeof (DIPROPHEADER),
			lpddoi->dwType,
			DIPH_BYID
		},
		-256,
		+256
	};
	if (lpddoi->wUsagePage == 1)
	{
		if (lpddoi->wUsage >= 0x30 && lpddoi->wUsage <= 0x37)
		{
			JoyAxisNames[lpddoi->wUsage-0x30] = copystring (lpddoi->tszName);
		}
	}
	if (FAILED(g_pJoy->SetProperty (DIPROP_RANGE, &diprg.diph)))
	{
		return DIENUM_STOP;
	}
	else
	{
		return DIENUM_CONTINUE;
	}
}

static HRESULT InitJoystick ()
{
	HRESULT hr;

	memset (JoyPOV, 9, sizeof(JoyPOV));
	for (int i = 0; i < 8; ++i)
	{
		if (JoyAxisNames[i])
		{
			delete[] JoyAxisNames[i];
			JoyAxisNames[i] = NULL;
		}
	}

	hr = g_pJoy->SetDataFormat (&c_dfDIJoystick2);
	if (FAILED(hr))
	{
		Printf (PRINT_BOLD, "Could not set joystick data format.\n");
		return hr;
	}

	hr = g_pJoy->SetCooperativeLevel (Window, DISCL_EXCLUSIVE|DISCL_FOREGROUND);
	if (FAILED(hr))
	{
		Printf (PRINT_BOLD, "Could not set joystick cooperative level.\n");
		return hr;
	}

	JoystickCaps.dwSize = sizeof(JoystickCaps);
	hr = g_pJoy->GetCapabilities (&JoystickCaps);
	if (FAILED(hr))
	{
		Printf (PRINT_BOLD, "Could not query joystick capabilities.\n");
		return hr;
	}

	hr = g_pJoy->EnumObjects (EnumAxesCallback, NULL, DIDFT_AXIS);
	if (FAILED(hr))
	{
		Printf (PRINT_BOLD, "Could not set joystick axes ranges.\n");
		return hr;
	}

	g_pJoy->Acquire ();

	return S_OK;
}

/****** Stuff from Andy Bay's mymouse.c ******/

/****************************************************************************
 *
 *		DIInit
 *
 *		Initialize the DirectInput variables.
 *
 ****************************************************************************/


// [RH] Used to obtain DirectInput access to the mouse.
//		(Preferred for Win95, but buggy under NT 4.)

bool I_InitInput (void *hwnd)
{
	HRESULT hr;

	Printf ("I_InitInput\n");
	atterm (I_ShutdownInput);

	noidle = !!Args->CheckParm ("-noidle");
	g_pdi = NULL;
	g_pdi3 = NULL;

	FindRawInputFunctions();

	// Try for DirectInput 8 first, then DirectInput 3 for NT 4's benefit.
	DInputDLL = LoadLibrary("dinput8.dll");
	if (DInputDLL != NULL)
	{
		typedef HRESULT (WINAPI *blah)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
		blah di8c = (blah)GetProcAddress(DInputDLL, "DirectInput8Create");
		if (di8c != NULL)
		{
			hr = di8c(g_hInst, DIRECTINPUT_VERSION, IID_IDirectInput8A, (void **)&g_pdi, NULL);
			if (FAILED(hr))
			{
				Printf(TEXTCOLOR_ORANGE "DirectInput8Create failed: %08lx", hr);
				g_pdi = NULL;	// Just to be sure DirectInput8Create didn't change it
			}
		}
		else
		{
			Printf(TEXTCOLOR_ORANGE "Could not find DirectInput8Create in dinput8.dll\n");
		}
	}

	if (g_pdi == NULL)
	{
		if (DInputDLL != NULL)
		{
			FreeLibrary(DInputDLL);
		}
		DInputDLL = LoadLibrary ("dinput.dll");
		if (DInputDLL == NULL)
		{
			I_FatalError ("Could not load dinput.dll: %08lx", GetLastError());
		}

		typedef HRESULT (WINAPI *blah)(HINSTANCE, DWORD, LPDIRECTINPUT*, LPUNKNOWN);
		blah dic = (blah)GetProcAddress (DInputDLL, "DirectInputCreateA");

		if (dic == NULL)
		{
			I_FatalError ("dinput.dll is corrupt");
		}

		hr = dic (g_hInst, 0x0300, &g_pdi3, NULL);
		if (FAILED(hr))
		{
			I_FatalError ("DirectInputCreate failed: %08lx", hr);
		}
	}

	Printf ("I_StartupMouse\n");
	I_StartupMouse();

	Printf ("I_StartupJoystick\n");
	DI_EnumJoy ();
	DI_InitJoy ();

	Printf ("I_StartupKeyboard\n");
	I_StartupKeyboard();

	return TRUE;
}


// Free all input resources
void I_ShutdownInput ()
{
	if (Keyboard != NULL)
	{
		delete Keyboard;
		Keyboard = NULL;
	}
	if (Mouse != NULL)
	{
		delete Mouse;
		Mouse = NULL;
	}
	if (g_pJoy)
	{
		SaveJoystickConfig ();
		g_pJoy->Release ();
		g_pJoy = NULL;
	}
	if (g_pdi)
	{
		g_pdi->Release ();
		g_pdi = NULL;
	}
	if (g_pdi3)
	{
		g_pdi3->Release ();
		g_pdi3 = NULL;
	}
	if (DInputDLL != NULL)
	{
		FreeLibrary (DInputDLL);
		DInputDLL = NULL;
	}
}

void I_GetEvent ()
{
	MSG mess;

	// Briefly enter an alertable state so that if a secondary thread
	// crashed, we will execute the APC it sent now.
	SleepEx (0, TRUE);

	while (PeekMessage (&mess, NULL, 0, 0, PM_REMOVE))
	{
		if (mess.message == WM_QUIT)
			exit (mess.wParam);
		if (EAXEditWindow == 0 || !IsDialogMessage (EAXEditWindow, &mess))
		{
			TranslateMessage (&mess);
			DispatchMessage (&mess);
		}
	}

	if (Keyboard != NULL)
	{
		Keyboard->ProcessInput();
	}
	if (Mouse != NULL)
	{
		Mouse->ProcessInput();
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
	if (use_joystick)
	{
		DI_JoyCheck ();
	}
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

FString I_GetFromClipboard (bool return_nothing)
{
	FString retstr;
	HGLOBAL cliphandle;
	char *clipstr;
	char *nlstr;

	if (return_nothing || !IsClipboardFormatAvailable (CF_TEXT) || !OpenClipboard (Window))
		return retstr;

	cliphandle = GetClipboardData (CF_TEXT);
	if (cliphandle != NULL)
	{
		clipstr = (char *)GlobalLock (cliphandle);
		if (clipstr != NULL)
		{
			// Convert CR-LF pairs to just LF while copying to the FString
			for (nlstr = clipstr; *nlstr != '\0'; ++nlstr)
			{
				if (nlstr[0] == '\r' && nlstr[1] == '\n')
				{
					nlstr++;
				}
				retstr += *nlstr;
			}
			GlobalUnlock (clipstr);
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

//==========================================================================
//
// FInputDevice - Destructor
//
//==========================================================================

FInputDevice::~FInputDevice()
{
}

//==========================================================================
//
// FInputDevice :: ProcessInput
//
// Gives subclasses an opportunity to do input handling that doesn't involve
// window messages.
//
//==========================================================================

void FInputDevice::ProcessInput()
{
}

//==========================================================================
//
// FInputDevice :: ProcessRawInput
//
// Gives subclasses a chance to handle WM_INPUT messages. This is not part
// of WndProcHook so that we only need to fill the RAWINPUT buffer once
// per message and be sure it gets cleaned up properly.
//
//==========================================================================

bool FInputDevice::ProcessRawInput(RAWINPUT *raw, int code)
{
	return false;
}

//==========================================================================
//
// FInputDevice :: WndProcHook
//
// Gives subclasses a chance to intercept window messages. 
//
//==========================================================================

bool FInputDevice::WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	return false;
}

//==========================================================================
//
// FindRawInputFunctions
//
// Finds functions for raw input, if available.
//
//==========================================================================

static void FindRawInputFunctions()
{
	HMODULE user32 = GetModuleHandle("user32.dll");

	if (user32 == NULL)
	{
		return;		// WTF kind of broken system are we running on?
	}
#define RIF(name,ret,args) \
	My##name = (name##Proto)GetProcAddress(user32, #name);
#include "rawinput.h"
}
