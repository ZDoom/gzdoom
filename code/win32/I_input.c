#define DIRECTINPUT_VERSION 0x300	// We want to support DX 3.0, and
#define _WIN32_WINNT 0x0400			// we want to support the mouse wheel

#define WIN32_LEAN_AND_MEAN
#define __BYTEBOOL__
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <dinput.h>

#include "doomdef.h"
#include "doomstat.h"
#include "m_argv.h"
#include "i_input.h"
#include "v_video.h"

#include "d_main.h"
#include "c_consol.h"
#include "c_cvars.h"
#include "i_system.h"
#include "i_video.h"

#include "s_sound.h"


#define DINPUT_BUFFERSIZE	32

extern HINSTANCE g_hInst;

static void KeyRead (void);
static BOOL DI_Init2 (void);
static void MouseRead_DI (void);
static void MouseRead_Win32 (void);
static void GrabMouse_Win32 (void);
static void UngrabMouse_Win32 (void);
static BOOL I_GetDIMouse (void);
static void I_GetWin32Mouse (void);
static void CenterMouse_Win32 (void);
static void WheelMoved (void);

static BOOL mousepaused;
static BOOL WindowActive;
static BOOL MakeMouseEvents;
extern BOOL menuactive;
extern BOOL vidactive;
extern HWND Window;


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

// Used by the console for making keys repeat
int KeyRepeatDelay;
int KeyRepeatRate;

static LPDIRECTINPUT			g_pdi;
static LPDIRECTINPUTDEVICE		g_pKey;
static LPDIRECTINPUTDEVICE		g_pMouse;

//Other globals
static int GDx,GDy;

extern constate_e ConsoleState;

cvar_t *i_remapkeypad;

cvar_t *usemouse;
cvar_t *in_mouse;

cvar_t *usejoystick;
cvar_t *joy_speedmultiplier;
cvar_t *joy_ythreshold;
cvar_t *joy_ysensitivity;
cvar_t *joy_xthreshold;
cvar_t *joy_xsensitivity;

// Convert DIK_* code to ASCII using Qwerty keymap
static const byte Convert []={
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

// Convert DIK_* code to ASCII using user keymap (built at run-time)
// New on 19.7.1998 - Now has 8 tables for each possible combination
// of CTRL, ALT, and SHIFT.
static byte Convert2[256][8];

#define SHIFT_SHIFT 0
#define CTRL_SHIFT 1
#define ALT_SHIFT 2

static BOOL altdown, shiftdown, ctrldown;

LRESULT CALLBACK WndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	event_t event;

	event.data1 = event.data2 = event.data3 = 0;

	switch(message) {
		case WM_DESTROY:
			SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
			//PostQuitMessage (0);
			exit (0);
			break;

		case WM_HOTKEY:
			break;

		case WM_PAINT:
			return DefWindowProc (hWnd, message, wParam, lParam);

		case WM_KILLFOCUS:
			if (g_pKey) IDirectInputDevice_Unacquire (g_pKey);
			if (g_pMouse) IDirectInputDevice_Unacquire (g_pMouse);
			if (altdown) {
				altdown = FALSE;
				event.type = ev_keyup;
				event.data1 = DIK_LALT;
				D_PostEvent (&event);
			}
			if (shiftdown) {
				shiftdown = FALSE;
				event.type = ev_keyup;
				event.data1 = DIK_LSHIFT;
				D_PostEvent (&event);
			}
			if (ctrldown) {
				ctrldown = FALSE;
				event.type = ev_keyup;
				event.data1 = DIK_LCONTROL;
				D_PostEvent (&event);
			}
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
			if (g_pKey) IDirectInputDevice_Acquire (g_pKey);
			havefocus = TRUE;
			if (g_pMouse && (Fullscreen || !mousepaused))
				I_ResumeMouse ();
			if (!paused)
				S_ResumeSound ();
			break;

		case WM_ACTIVATE:
			if (LOWORD(wParam)) {
				WindowActive = TRUE;
				if (mousemode == win32 && MakeMouseEvents &&
					(!mousepaused || Fullscreen)) {
					GrabMouse_Win32 ();
				}
			} else {
				WindowActive = FALSE;
				if (mousemode == win32) {
					UngrabMouse_Win32 ();
				}
			}
			break;

		// Being forced to separate my keyboard input handler into
		// two pieces like this really stinks. (IMHO)
		case WM_KEYDOWN:
		case WM_KEYUP:
			if (message == WM_KEYUP) {
				event.type = ev_keyup;
			} else {
				if (lParam & 0x40000000)
					return 0;
				else
					event.type = ev_keydown;
			}

			switch (wParam) {
				case VK_PAUSE:
					event.data1 = KEY_PAUSE;
					break;
				case VK_TAB:
					event.data1 = DIK_TAB;
					event.data2 = '\t';
					event.data3 = Convert2[DIK_TAB][(shiftdown << SHIFT_SHIFT) |
													(ctrldown << CTRL_SHIFT) |
													(altdown << ALT_SHIFT)];
					break;
				case VK_NUMLOCK:
					event.data1 = DIK_NUMLOCK;
					break;
				case VK_SHIFT:
					event.data1 = KEY_LSHIFT;
					shiftdown = (event.type == ev_keydown);
					break;
				case VK_CONTROL:
					event.data1 = KEY_LCTRL;
					ctrldown = (event.type == ev_keydown);
					break;
			}
			if (event.data1)
				D_PostEvent (&event);
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
			if (MakeMouseEvents && mousemode == win32) {
				event.type = ((message - WM_LBUTTONDOWN) % 3) ? ev_keyup : ev_keydown;
				event.data1 = KEY_MOUSE1 + (message - WM_LBUTTONDOWN) / 3;
				event.data2 = event.data3 = 0;
				D_PostEvent (&event);
			}
			break;

		case WM_MOUSEWHEEL:
			if (MakeMouseEvents && mousemode == win32) {
				WheelMove += (short) HIWORD(wParam);
				WheelMoved ();
			}
			break;

		default:
			return DefWindowProc (hWnd, message, wParam, lParam);
	}

	return 0;
}

static void BuildCvt2Table (void)
{
	byte vk2scan[256];
	int i;

	memset (vk2scan, 0, 256);
	for (i = 0; i < 256; i++)
		vk2scan[MapVirtualKey (i, 1)] = i;

	vk2scan[0] = 0;

	for (i = 0; i < 256; i++) {
		int code = VkKeyScan ((TCHAR)i);
		Convert2[vk2scan[code & 0xff]][(code >> 8) & 7] =  i;
	}
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

	if (JoyActive) {
		JoyStats.dwFlags = JOY_RETURNALL;
		if (joyGetPosEx (JoyDevice, &JoyStats)) {
			JoyActive = 0;
			return;
		}
		joyevent.type = ev_joystick;
		joyevent.data1 = 0;
		joyevent.data2 = JoyStats.dwXpos - JoyBias.X;
		joyevent.data3 = JoyStats.dwYpos - JoyBias.Y;

		xdead = (int)((float)JoyBias.X * joy_xthreshold->value);
		ydead = (int)((float)JoyBias.Y * joy_ythreshold->value);
		xscale = (int)(16777216 / ((float)JoyBias.X * (1 - joy_xthreshold->value)) * joy_xsensitivity->value * joy_speedmultiplier->value);
		yscale = (int)(16777216 / ((float)JoyBias.Y * (1 - joy_ythreshold->value)) * joy_ysensitivity->value * joy_speedmultiplier->value);

		if (abs (joyevent.data2) < xdead)
			joyevent.data2 = 0;
		else if (joyevent.data2 > 0) {
			joyevent.data2 = FixedMul (joyevent.data2 - xdead, xscale);
		} else if (joyevent.data2 < 0) {
			joyevent.data2 = FixedMul (joyevent.data2 + xdead, xscale);
		}
		if (joyevent.data2 > 256)
			joyevent.data2 = 256;
		else if (joyevent.data2 < -256)
			joyevent.data2 = -256;

		if (abs (joyevent.data3) < ydead)
			joyevent.data3 = 0;
		else if (joyevent.data3 > 0) {
			joyevent.data3 = FixedMul (joyevent.data3 - ydead, yscale);
		} else if (joyevent.data3 < 0) {
			joyevent.data3 = FixedMul (joyevent.data3 + ydead, yscale);
		}
		if (joyevent.data3 > 256)
			joyevent.data3 = 256;
		else if (joyevent.data3 < -256)
			joyevent.data3 = -256;
/*
		joyevent.type = ev_joystick;
		joyevent.data1 = 0;
		joyevent.data2 = JoyStats.dwXpos - JoyBias.X;

		//Doom thinks it is digital.  ugh.
		if (joyevent.data2 > JoyBias.X / 2){
			joyevent.data2 = 1;
		}
		else if(joyevent.data2 < -JoyBias.X / 2){
			joyevent.data2 = -1;
		}
		else
			joyevent.data2 = 0;

		joyevent.data3 = JoyStats.dwYpos - JoyBias.Y;
		if (joyevent.data3 > JoyBias.Y / 2) {
			joyevent.data3 = 1;
		}
		else if (joyevent.data3 < -JoyBias.Y / 2) {
			joyevent.data3 = -1;
		}
		else
			joyevent.data3 = 0;
*/
		D_PostEvent (&joyevent);

		{	/* Send out button up/down events */
			static DWORD oldButtons = 0;
			int i;
			DWORD buttons, mask;

			buttons = JoyStats.dwButtons;
			mask = buttons ^ oldButtons;

			joyevent.data2 = joyevent.data3 = 0;
			for (i = 0; i < 32; i++, buttons >>= 1, mask >>= 1) {
				if (mask & 1) {
					joyevent.data1 = KEY_JOY1 + i;
					if (buttons & 1)
						joyevent.type = ev_keydown;
					else
						joyevent.type = ev_keyup;
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

	for (i = JOYSTICKID1; i <= JOYSTICKID2; i++) {
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

void I_PauseMouse (void)
{
	if (Fullscreen)
		return;

	mousepaused = TRUE;
	if (g_pMouse) {
		IDirectInputDevice_Unacquire (g_pMouse);
	} else {
		UngrabMouse_Win32 ();
	}
}

void I_ResumeMouse (void)
{
	if (!Fullscreen && gamestate == GS_FULLCONSOLE)
		return;

	mousepaused = FALSE;
	if (!g_pMouse) {
		GrabMouse_Win32 ();
	} else {
		IDirectInputDevice_Acquire (g_pMouse);
	}
}

/****** Stuff from Andy Bay's mymouse.c ******

/****************************************************************************
 *
 *		DIInit
 *
 *		Initialize the DirectInput variables.
 *
 ****************************************************************************/

// [RH] Obtain the mouse using standard Win32 calls. Should always work.
static void I_GetWin32Mouse (void)
{
	mousemode = win32;

	if (g_pMouse) {
		IDirectInputDevice_Unacquire (g_pMouse);
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
	}
	GrabMouse_Win32 ();
}

// [RH] Used to obtain DirectInput access to the mouse.
//		(Preferred for Win95, but buggy under NT.)
static BOOL I_GetDIMouse (void)
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

	if (in_mouse->value == 1 || (in_mouse->value == 0 && OSPlatform == os_WinNT))
		return FALSE;

	// Obtain an interface to the system mouse device.
	hr = IDirectInput_CreateDevice (g_pdi, &GUID_SysMouse, &g_pMouse, NULL);

	if (FAILED(hr))
		return FALSE;

	// Set the data format to "mouse format".
	hr = IDirectInputDevice_SetDataFormat (g_pMouse, &c_dfDIMouse);

	if (FAILED(hr)) {
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
		return FALSE;
	}

	// Set the cooperative level.
	hr = IDirectInputDevice_SetCooperativeLevel (g_pMouse,(HWND)Window,
									   DISCL_EXCLUSIVE | DISCL_FOREGROUND);

	if (FAILED(hr)) {
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
		return FALSE;
	}


	// Set the buffer size to DINPUT_BUFFERSIZE elements.
	// The buffer size is a DWORD property associated with the device.
	hr = IDirectInputDevice_SetProperty (g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);

	if (FAILED(hr)) {
		Printf (PRINT_HIGH, "Could not set mouse buffer size");
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
		return FALSE;
	}

	IDirectInputDevice_Acquire (g_pMouse);

	mousemode = dinput;
	return TRUE;
}

static void in_mouse_changed (cvar_t *var)
{
	if (var->value < 0) {
		SetCVarFloat (var, 0);
	} else if (var->value > 2) {
		SetCVarFloat (var, 2);
	} else {
		int new_mousemode;

		if (var->value == 1 || (var->value == 0 && OSPlatform == os_WinNT))
			new_mousemode = win32;
		else
			new_mousemode = dinput;

		if (new_mousemode != mousemode) {
			if (new_mousemode == win32)
				I_GetWin32Mouse ();
			else
				if (!I_GetDIMouse ())
					I_GetWin32Mouse ();
		}
	}
}

BOOL I_InitInput (void *hwnd)
{
	DIRECTINPUTCREATE_FUNCTION DirectInputCreateFunction;
	HRESULT hr;

	atexit (I_ShutdownInput);

	noidle = M_CheckParm ("-noidle");

	in_mouse = cvar ("in_mouse", "0", CVAR_ARCHIVE|CVAR_CALLBACK);
	in_mouse->u.callback = in_mouse_changed;

	joy_speedmultiplier = cvar ("joy_speedmultiplier", "1", CVAR_ARCHIVE);
	joy_xsensitivity = cvar ("joy_xsensitivity", "1", CVAR_ARCHIVE);
	joy_ysensitivity = cvar ("joy_ysensitivity", "-1", CVAR_ARCHIVE);
	joy_xthreshold = cvar ("joy_xthreshold", "0.15", CVAR_ARCHIVE);
	joy_ythreshold = cvar ("joy_ythreshold", "0.15", CVAR_ARCHIVE);

	// [RH] Removed dependence on existance of dinput.lib when linking.
	DirectInputInstance = (HMODULE)LoadLibrary ("dinput.dll");
	if (!DirectInputInstance)
		I_FatalError ("Sorry, you need Microsoft's DirectX installed.\n\n"
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
void STACK_ARGS I_ShutdownInput (void)
{
	if (g_pKey) {
		IDirectInputDevice_Unacquire (g_pKey);
		IDirectInputDevice_Release (g_pKey);
		g_pKey = NULL;
	}
	if (g_pMouse) {
		IDirectInputDevice_Unacquire (g_pMouse);
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
	}
	UngrabMouse_Win32 ();
	if (g_pdi) {
		IDirectInput_Release(g_pdi);
		g_pdi = NULL;
	}
	// [RH] Close dinput.dll
	if (DirectInputInstance) {
		FreeLibrary (DirectInputInstance);
		DirectInputInstance = NULL;
	}
}

static LONG PrevX, PrevY;

static void CenterMouse_Win32 (void) {
	RECT rect;

	GetWindowRect (Window, &rect);
	PrevX = (rect.left + rect.right) >> 1;
	PrevY = (rect.top + rect.bottom) >> 1;
	SetCursorPos (PrevX, PrevY);
}

static void SetCursorState (int visible) {
	int count;
	BOOL direction = visible;
	
	do {
		count = ShowCursor (direction);
		if (visible && count > 0)
			direction = FALSE;
		else if (!visible && count < -1)
			direction = TRUE;
	} while (count != visible - 1);
}

static void GrabMouse_Win32 (void) {
	RECT rect;

	ClipCursor (NULL);		// helps with Win95?
	GetWindowRect (Window, &rect);
	ClipCursor (&rect);
	SetCursorState (FALSE);
	CenterMouse_Win32 ();
	MakeMouseEvents = TRUE;
}

static void UngrabMouse_Win32 (void) {
	ClipCursor (NULL);
	SetCursorState (TRUE);
	MakeMouseEvents = FALSE;
}

static void WheelMoved (void)
{
	event_t event;
	int dir;

	event.data2 = event.data3 = 0;
	if (WheelMove < 0) {
		dir = WHEEL_DELTA;
		event.data1 = KEY_MWHEELDOWN;
	} else {
		dir = -WHEEL_DELTA;
		event.data1 = KEY_MWHEELUP;
	}
	while (abs (WheelMove) >= WHEEL_DELTA) {
		event.type = ev_keydown;
		D_PostEvent (&event);
		event.type = ev_keyup;
		D_PostEvent (&event);
		WheelMove += dir;
	}
}

static void MouseRead_Win32 (void) {
	POINT pt;
	event_t ev;

	if (!WindowActive || !MakeMouseEvents || !GetCursorPos (&pt))
		return;

	ev.data2 = (pt.x - PrevX) * 3;
	ev.data3 = (PrevY - pt.y) << 1;

	CenterMouse_Win32 ();

	if (ev.data2 || ev.data3) {
		ev.type = ev_mouse;
		ev.data1 = 0;
		D_PostEvent (&ev);
	}
}

static void MouseRead_DI (void) {
	static int lastx = 0, lasty = 0;
	DIDEVICEOBJECTDATA od;
	DWORD dwElements;
	HRESULT hr;
	int count = 0;

	event_t event;
	GDx=0;
	GDy=0;

	if (!g_pMouse)
		return;

	event.data2 = event.data3 = 0;
	while (1) {
		dwElements = 1;
		hr = IDirectInputDevice_GetDeviceData (g_pMouse,
							 sizeof(DIDEVICEOBJECTDATA), &od,
							 &dwElements,
							 0);
		if (hr == DIERR_INPUTLOST) {
			IDirectInputDevice_Acquire (g_pMouse);
			hr = IDirectInputDevice_GetDeviceData (g_pMouse,
								 sizeof(DIDEVICEOBJECTDATA), &od,
								 &dwElements,
								 0);
		}

		/* Unable to read data or no data available */
		if (FAILED(hr) || !dwElements)
			break;

		count++;

		/* Look at the element to see what happened */
		switch (od.dwOfs) {

		/* DIMOFS_X: Mouse horizontal motion */
			case DIMOFS_X:
				GDx += od.dwData;
				break;
		/* DIMOFS_Y: Mouse vertical motion */
			case DIMOFS_Y:
				GDy += od.dwData;
				break;

		/* DIMOFS_Z: Mouse wheel motion */
			case DIMOFS_Z:
				WheelMove += od.dwData;
				WheelMoved ();
				break;

		/* [RH] Mouse button events now mimic keydown/up events */
			case DIMOFS_BUTTON0:
				if(od.dwData & 0x80) {
					event.type = ev_keydown;
				} else {
					event.type = ev_keyup;
				}
				event.data1 = KEY_MOUSE1;
				D_PostEvent (&event);
				break;
			case DIMOFS_BUTTON1:
				if(od.dwData & 0x80) {
					event.type = ev_keydown;
				} else {
					event.type = ev_keyup;
				}
				event.data1 = KEY_MOUSE2;
				D_PostEvent (&event);
				break;
			case DIMOFS_BUTTON2:
				if(od.dwData & 0x80) {
					event.type = ev_keydown;
				} else {
					event.type = ev_keyup;
				}
				event.data1 = KEY_MOUSE3;
				D_PostEvent (&event);
				break;
			case DIMOFS_BUTTON3:
				if(od.dwData & 0x80) {
					event.type = ev_keydown;
				} else {
					event.type = ev_keyup;
				}
				event.data1 = KEY_MOUSE4;
				D_PostEvent (&event);
				break;
		}
	}

	if (count) {
		event.type = ev_mouse;
		event.data1 = 0;
		event.data2 = GDx<<2;
		event.data3 = -GDy;
		D_PostEvent (&event);
	}
}

// Initialize the keyboard
static BOOL DI_Init2 (void)
{
	int hr;
	DWORD repeatStuff;
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

	BuildCvt2Table ();

	// [RH] The timing values for these SPI_* parameters are described in
	//		MS Knowledge Base article Q102978.
	if (SystemParametersInfo (SPI_GETKEYBOARDDELAY, 0, &repeatStuff, 0)) {
		// 0 = 250 ms, 3 = 1000 ms
		KeyRepeatDelay = ((repeatStuff * 250 + 250) * TICRATE) / 1000;
	} else {
		KeyRepeatDelay = (250 * TICRATE) / 1000;
	}
	DPrintf ("KeyRepeatDelay = %u tics\n", KeyRepeatDelay);

	if (SystemParametersInfo (SPI_GETKEYBOARDSPEED, 0, &repeatStuff, 0)) {
		// 0 = 2/sec, 31 = 30/sec
		KeyRepeatRate = TICRATE / (2 + repeatStuff);
	} else {
		KeyRepeatRate = TICRATE / 15;
	}
	DPrintf ("KeyRepeatRate = %u tics\n", KeyRepeatRate);

	// Obtain an interface to the system key device.
	hr = IDirectInput_CreateDevice (g_pdi, &GUID_SysKeyboard, &g_pKey, NULL);

	if (FAILED(hr)) {
		I_FatalError ("Could not create keyboard device");
	}

	// [RH] Prepare c_dfDIKeyboard for use.
	InitKeyboardObjectData ();

	// Set the data format to "keyboard format".
	hr = IDirectInputDevice_SetDataFormat (g_pKey,&c_dfDIKeyboard);

	if (FAILED(hr)) {
		I_FatalError ("Could not set keyboard data format");
	}


	// Set the cooperative level.
	hr = IDirectInputDevice_SetCooperativeLevel(g_pKey, Window,
									   DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	if (FAILED(hr)) {
		I_FatalError("Could not set keyboard cooperative level");
	}

	// Set the buffer size to DINPUT_BUFFERSIZE elements.
	// The buffer size is a DWORD property associated with the device.
	hr = IDirectInputDevice_SetProperty (g_pKey, DIPROP_BUFFERSIZE, &dipdw.diph);

	if (FAILED(hr)) {
		I_FatalError ("Could not set keyboard buffer size");
	}

	IDirectInputDevice_Acquire (g_pKey);

	DI_InitJoy ();
	return TRUE;
}

static void KeyRead (void) {
	HRESULT  hr;
	event_t event;

	DIDEVICEOBJECTDATA data[DINPUT_BUFFERSIZE];
	DWORD dwElements, elem;

	do {
		dwElements = DINPUT_BUFFERSIZE;

		hr = IDirectInputDevice_GetDeviceData (g_pKey, sizeof(DIDEVICEOBJECTDATA), data, &dwElements, 0);
		if (hr == DIERR_INPUTLOST) {
			hr = IDirectInputDevice_Acquire (g_pKey);
			hr = IDirectInputDevice_GetDeviceData (g_pKey, sizeof(DIDEVICEOBJECTDATA), data, &dwElements, 0);
		}

		if (SUCCEEDED (hr) && dwElements) {
			int key;

			for (elem = 0; elem < dwElements; elem++) {
				key = data[elem].dwOfs;

				if (data[elem].dwData & 0x80) {
					event.type = ev_keydown;
				} else {
					event.type = ev_keyup;
				}

				switch (key) {
					case DIK_NUMPADENTER:	// These keys always translated
						key = DIK_RETURN;
						break;
					case DIK_RMENU:
						key = DIK_LMENU;
						break;
					case DIK_RCONTROL:		// These keys are handled by the message handler
					case DIK_LCONTROL:
					case DIK_RSHIFT:
					case DIK_LSHIFT:
					case DIK_TAB:
					case DIK_NUMLOCK:
						key = 0;
						break;
					default:
						if (!menuactive &&
							(ConsoleState == c_falling || ConsoleState == c_down)) {
							switch (key) {
								case DIK_NUMPAD4:
									key = DIK_4;
									break;
								case DIK_NUMPAD6:
									key = DIK_6;
									break;
								case DIK_NUMPAD8:
									key = DIK_8;
									break;
								case DIK_NUMPAD2:
									key = DIK_2;
									break;
								case DIK_NUMPAD7:
									key = DIK_7;
									break;
								case DIK_NUMPAD9:
									key = DIK_9;
									break;
								case DIK_NUMPAD3:
									key = DIK_3;
									break;
								case DIK_NUMPAD1:
									key = DIK_1;
									break;
								case DIK_NUMPAD0:
									key = DIK_0;
									break;
								case DIK_NUMPAD5:
									key = DIK_5;
									break;
							}
						} else if (i_remapkeypad->value) {
							switch (key) {
								case DIK_NUMPAD4:
									key = DIK_LEFT;
									break;
								case DIK_NUMPAD6:
									key = DIK_RIGHT;
									break;
								case DIK_NUMPAD8:
									key = DIK_UP;
									break;
								case DIK_NUMPAD2:
									key = DIK_DOWN;
									break;
								case DIK_NUMPAD7:
									key = DIK_HOME;
									break;
								case DIK_NUMPAD9:
									key = DIK_PRIOR;
									break;
								case DIK_NUMPAD3:
									key = DIK_NEXT;
									break;
								case DIK_NUMPAD1:
									key = DIK_END;
									break;
								case DIK_NUMPAD0:
									key = DIK_INSERT;
									break;
								case DIK_DECIMAL:
									key = DIK_DELETE;
									break;
							}
						}
				}

				if (key) {
					event.data1 = key;
					if (ConsoleState == c_falling || ConsoleState == c_down) {
						switch (event.data1) {
							case DIK_DIVIDE:
								event.data2 = event.data3 = '/';
								break;
							case DIK_MULTIPLY:
								event.data2 = event.data3 = '*';
								break;
							case DIK_ADD:
								event.data2 = event.data3 = '+';
								break;
							case DIK_SUBTRACT:
								event.data2 = event.data3 = '-';
								break;
							case DIK_DECIMAL:
								event.data2 = event.data3 = '.';
								break;
							default:
								event.data2 = Convert[key];
								event.data3 = Convert2[key][(altdown << ALT_SHIFT) |
															(shiftdown << SHIFT_SHIFT) |
															(ctrldown << CTRL_SHIFT)];
						}
					} else {
						event.data2 = Convert[key];
						event.data3 = Convert2[key][(altdown << ALT_SHIFT) |
													(shiftdown << SHIFT_SHIFT) |
													(ctrldown << CTRL_SHIFT)];
					}
					D_PostEvent (&event);
					if (key == DIK_LALT)
						altdown = (event.type == ev_keydown);
				}
			}
		}
	} while (SUCCEEDED (hr) && dwElements);
}

void I_GetEvent(void)
{
	MSG mess;

//	while (1) {
		while (PeekMessage (&mess, NULL, 0, 0, PM_REMOVE)) {
			if (mess.message == WM_QUIT)
				exit (mess.wParam);
			TranslateMessage (&mess);
			DispatchMessage (&mess);
		}
//		if (havefocus || netgame || gamestate != GS_LEVEL)
//			break;
//	}

	KeyRead ();

	if (usemouse->value) {
		if (mousemode == dinput)
			MouseRead_DI ();
		else
			MouseRead_Win32 ();
	}
	if (usejoystick->value)
		DI_JoyCheck ();
}


//
// I_StartTic
//
void I_StartTic (void)
{
	I_GetEvent ();
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
}
