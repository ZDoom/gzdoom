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

#define DINPUT_BUFFERSIZE	32

extern HINSTANCE g_hInst;

static void KeyRead ();
static BOOL DI_Init2 ();
static void MouseRead_DI ();
static void MouseRead_Win32 ();
static void GrabMouse_Win32 ();
static void UngrabMouse_Win32 ();
static BOOL I_GetDIMouse ();
static void I_GetWin32Mouse ();
static void CenterMouse_Win32 ();
static void WheelMoved ();
static void DI_Acquire (LPDIRECTINPUTDEVICE mouse);
static void DI_Unacquire (LPDIRECTINPUTDEVICE mouse);
static void SetCursorState (int visible);

static bool GUICapture;
static BOOL mousepaused;
static BOOL WindowActive;
static BOOL MakeMouseEvents;

extern BOOL vidactive;
extern HWND Window;

EXTERN_CVAR (Bool, fullscreen)
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
static BOOL havefocus = FALSE;
static BOOL noidle = FALSE;
static int WheelMove;

static LPDIRECTINPUT			g_pdi;
static LPDIRECTINPUTDEVICE		g_pKey;
static LPDIRECTINPUTDEVICE		g_pMouse;

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
	if (*var < 0)
	{
		var = 0;
	}
	else if (*var > 2)
	{
		var = 2;
	}
	else if (!g_pdi)
	{
		return;
	}
	else
	{
		int new_mousemode;

		if (*var == 1 || (*var == 0 && OSPlatform == os_WinNT))
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
			if (!*fullscreen && mousepaused)
				I_PauseMouse ();
		}
	}
}

static BYTE KeyState[256];
static BYTE DIKState[2][NUM_KEYS];
static int ActiveDIKState;

// Convert DIK_* code to ASCII using Qwerty keymap
static const byte Convert [] = {
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

static void FlushDIKState ()
{
	int i;
	event_t event;
	BYTE *state = DIKState[ActiveDIKState];

	memset (&event, 0, sizeof(event));
	event.type = EV_KeyUp;
	for (i = 0; i < NUM_KEYS; i++)
	{
		if (state[i])
		{
			event.data1 = i;
			event.data2 = Convert[i];
			D_PostEvent (&event);
		}
	}
	memset (state, 0, NUM_KEYS);
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
		return DefWindowProc (hWnd, message, wParam, lParam);

	case WM_SETTINGCHANGE:
		// In case regional settings were changed, reget preferred languages
		language.Callback ();
		return 0;

	case WM_KILLFOCUS:
		if (g_pKey) g_pKey->Unacquire ();
		if (g_pMouse) DI_Unacquire (g_pMouse);
		
		FlushDIKState ();
		havefocus = FALSE;
		if (!paused)
			S_PauseSound ();
		if (!noidle)
			SetPriorityClass (GetCurrentProcess(), IDLE_PRIORITY_CLASS);
		break;

	case WM_SETFOCUS:
#ifdef _DEBUG
		SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#else
		SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS);
#endif
		if (g_pKey)
			DI_Acquire (g_pKey);
		havefocus = TRUE;
		if (g_pMouse && (!mousepaused || *fullscreen))
			I_ResumeMouse ();
		if (!paused)
			S_ResumeSound ();
		break;

	case WM_ACTIVATE:
		if (LOWORD(wParam))
		{
			WindowActive = TRUE;
			if (mousemode == win32 &&
				gamestate != GS_STARTUP &&
				(!mousepaused || *fullscreen))
			{
				GrabMouse_Win32 ();
			}
		}
		else
		{
			WindowActive = FALSE;
			if (mousemode == win32 && gamestate != GS_STARTUP)
			{
				UngrabMouse_Win32 ();
			}
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

	case WM_SYSKEYDOWN:
		SendMessage (hWnd, WM_KEYDOWN, wParam, lParam);
		break;

	case WM_SYSKEYUP:
		SendMessage (hWnd, WM_KEYUP, wParam, lParam);
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
		if (screen)
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

		xdead = (int)((float)JoyBias.X * *joy_xthreshold);
		ydead = (int)((float)JoyBias.Y * *joy_ythreshold);
		xscale = (int)(16777216 / ((float)JoyBias.X * (1 - *joy_xthreshold)) * *joy_xsensitivity * *joy_speedmultiplier);
		yscale = (int)(16777216 / ((float)JoyBias.Y * (1 - *joy_ythreshold)) * *joy_ysensitivity * *joy_speedmultiplier);

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
	if (!mousepaused)
		SetCursorState (FALSE);
	else
		SetCursorState (TRUE);
}

static void DI_Unacquire (LPDIRECTINPUTDEVICE mouse)
{
	mouse->Unacquire ();
	SetCursorState (TRUE);
}

void I_PauseMouse ()
{
	if (*fullscreen)
		return;

	mousepaused = TRUE;
	if (g_pMouse)
	{
		DI_Unacquire (g_pMouse);
	}
	else
	{
		UngrabMouse_Win32 ();
	}
}

void I_ResumeMouse ()
{
	if (!*fullscreen && gamestate == GS_FULLCONSOLE && !menuactive)
		return;

	mousepaused = FALSE;
	if (!g_pMouse)
	{
		GrabMouse_Win32 ();
	}
	else
	{
		DI_Acquire (g_pMouse);
	}
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

	if (g_pMouse) {
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

	if (*in_mouse == 1 || (*in_mouse == 0 && OSPlatform == os_WinNT))
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

	noidle = Args.CheckParm ("-noidle");

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

static void CenterMouse_Win32 ()
{
	RECT rect;

	GetWindowRect (Window, &rect);
	PrevX = (rect.left + rect.right) >> 1;
	PrevY = (rect.top + rect.bottom) >> 1;
	SetCursorPos (PrevX, PrevY);
}

static void SetCursorState (int visible)
{
	int count;
	BOOL direction = visible--;

	do
	{
		count = ShowCursor (direction);
		if (visible == 0 && count > 0)
			direction = FALSE;
		else if (visible < 0 && count < -1)
			direction = TRUE;
	} while (count != visible);
}

static void GrabMouse_Win32 ()
{
	RECT rect;

	ClipCursor (NULL);		// helps with Win95?
	GetWindowRect (Window, &rect);
	ClipCursor (&rect);
	SetCursorState (FALSE);
	CenterMouse_Win32 ();
	MakeMouseEvents = TRUE;
}

static void UngrabMouse_Win32 ()
{
	ClipCursor (NULL);
	SetCursorState (TRUE);
	MakeMouseEvents = FALSE;
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

	if (!WindowActive || !MakeMouseEvents || !GetCursorPos (&pt))
		return;

	x = (pt.x - PrevX) * 3;
	y = (PrevY - pt.y) << 1;

	CenterMouse_Win32 ();

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
	memcpy (toState + KEY_PAUSE, fromState + KEY_PAUSE, NUM_KEYS - KEY_PAUSE);
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

	if (*i_remapkeypad)
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
		for (i = 1; i < KEY_PAUSE; i++)
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

	if (*use_mouse)
	{
		if (mousemode == dinput)
			MouseRead_DI ();
		else
			MouseRead_Win32 ();
	}
	if (*use_joystick)
		DI_JoyCheck ();
}


//
// I_StartTic
//
void I_StartTic ()
{
	I_CheckGUICapture ();
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
