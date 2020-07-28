/*
**
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x800
#include <windows.h>
#include <dinput.h>

#include "i_input.h"
#include "d_eventbase.h"
#include "d_gui.h"
#include "hardware.h"
#include "menu.h"
#include "menustate.h"
#include "keydef.h"
#include "i_interface.h"

// MACROS ------------------------------------------------------------------

#define DINPUT_BUFFERSIZE	32

// Compensate for w32api's lack
#ifndef GET_XBUTTON_WPARAM
#define GET_XBUTTON_WPARAM(wParam) (HIWORD(wParam))
#endif

// Only present in Vista SDK, and it probably isn't available with w32api,
// either.
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL	0x20e
#endif

// TYPES -------------------------------------------------------------------

class FRawMouse : public FMouse
{
public:
	FRawMouse();
	~FRawMouse();

	bool GetDevice();
	bool ProcessRawInput(RAWINPUT *rawinput, int code);
	bool WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result);
	void Grab();
	void Ungrab();

protected:
	bool Grabbed;
	POINT UngrabbedPointerPos;
};

class FDInputMouse : public FMouse
{
public:
	FDInputMouse();
	~FDInputMouse();
	
	bool GetDevice();
	void ProcessInput();
	bool WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result);
	void Grab();
	void Ungrab();

protected:
	LPDIRECTINPUTDEVICE8 Device;
	bool Grabbed;
};

class FWin32Mouse : public FMouse
{
public:
	FWin32Mouse();
	~FWin32Mouse();
	
	bool GetDevice();
	void ProcessInput();
	bool WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result);
	void Grab();
	void Ungrab();

protected:
	POINT UngrabbedPointerPos;
	LONG PrevX, PrevY;
	bool Grabbed;
};

enum EMouseMode
{
	MM_None,
	MM_Win32,
	MM_DInput,
	MM_RawInput
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void SetCursorState(bool visible);
static FMouse *CreateWin32Mouse();
static FMouse *CreateDInputMouse();
static FMouse *CreateRawMouse();
static void CenterMouse(int x, int y, LONG *centx, LONG *centy);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern LPDIRECTINPUT8 g_pdi;
extern LPDIRECTINPUT g_pdi3;
extern bool GUICapture;
extern int BlockMouseMove; 

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static EMouseMode MouseMode = MM_None;
static FMouse *(*MouseFactory[])() =
{
	CreateWin32Mouse,
	CreateDInputMouse,
	CreateRawMouse
};

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FMouse *Mouse;
bool NativeMouse;

bool CursorState;

CVAR (Bool, use_mouse,				true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, m_noprescale,			false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,	m_filter,				false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, m_hidepointer,			true, 0)

CUSTOM_CVAR (Int, in_mouse, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 3)
	{
		self = 3;
	}
	else
	{
		I_StartupMouse();
	}
}

// CODE --------------------------------------------------------------------

//==========================================================================
//
// SetCursorState
//
// Ensures the cursor is either visible or invisible.
//
//==========================================================================

static void SetCursorState(bool visible)
{
	CursorState = visible || !m_hidepointer;
	if (GetForegroundWindow() == Window)
	{
		if (CursorState)
		{
			SetCursor((HCURSOR)(intptr_t)GetClassLongPtr(Window, GCLP_HCURSOR));
		}
		else
		{
			SetCursor(NULL);
		}
	}
}

//==========================================================================
//
// CenterMouse
//
// Moves the mouse to the center of the window, but only if the current
// position isn't already in the center.
//
//==========================================================================

static void CenterMouse(int curx, int cury, LONG *centxp, LONG *centyp)
{
	RECT rect;

	GetWindowRect(Window, &rect);

	int centx = (rect.left + rect.right) >> 1;
	int centy = (rect.top + rect.bottom) >> 1;

	// Reduce the number of WM_MOUSEMOVE messages that get sent
	// by only calling SetCursorPos when we really need to.
	if (centx != curx || centy != cury)
	{
		if (centxp != NULL)
		{
			*centxp = centx;
			*centyp = centy;
		}
		SetCursorPos(centx, centy);
	}
}

//==========================================================================
//
// I_CheckNativeMouse
//
// Should we be capturing mouse input, or should we let the OS have normal
// control of it (i.e. native mouse)?
//
//==========================================================================

void I_CheckNativeMouse(bool preferNative, bool eventhandlerresult)
{
	bool windowed = (screen == NULL) || !screen->IsFullscreen();
	bool want_native;

	if (!windowed)
	{
		// ungrab mouse when in the menu with mouse control on.		
		want_native = m_use_mouse && (menuactive == MENU_On || menuactive == MENU_OnNoPause);
	}
	else
	{
		if ((GetForegroundWindow() != Window) || preferNative || !use_mouse)
		{
			want_native = true;
		}
		else if (menuactive == MENU_WaitKey)
		{
			want_native = false;
		}
		else
		{
			bool pauseState = false;
			bool captureModeInGame = sysCallbacks && sysCallbacks->CaptureModeInGame && sysCallbacks->CaptureModeInGame();
			want_native = ((!m_use_mouse || menuactive != MENU_WaitKey) &&
				(!captureModeInGame || GUICapture));
		}
	}

	if (!want_native && eventhandlerresult)
		want_native = true;

	//Printf ("%d %d %d\n", wantNative, preferNative, NativeMouse);

	if (want_native != NativeMouse)
	{
		if (Mouse != NULL)
		{
			NativeMouse = want_native;
			if (want_native)
			{
				BlockMouseMove = 3;
				Mouse->Ungrab();
			}
			else
			{
				Mouse->Grab();
			}
		}
	}
}

//==========================================================================
//
// FMouse - Constructor
//
//==========================================================================

FMouse::FMouse()
{
	LastX = LastY = 0;
	ButtonState = 0;
	WheelMove[0] = 0;
	WheelMove[1] = 0;
}

//==========================================================================
//
// FMouse :: PostMouseMove
//
// Posts a mouse movement event, potentially averaging it with the previous
// movement. If there is no movement to post, then no event is generated.
//
//==========================================================================

void FMouse::PostMouseMove(int x, int y)
{
	event_t ev = { 0 };

	if (m_filter)
	{
		ev.x = (x + LastX) / 2;
		ev.y = (y + LastY) / 2;
	}
	else
	{
		ev.x = x;
		ev.y = y;
	}
	LastX = x;
	LastY = y;
	if (ev.x | ev.y)
	{
		ev.type = EV_Mouse;
		D_PostEvent(&ev);
	}
}

//==========================================================================
//
// FMouse :: WheelMoved
//
// Generates events for a wheel move. Events are generated for every
// WHEEL_DELTA units that the wheel has moved. In normal mode, each move
// generates both a key down and a key up event. In GUI mode, only one
// event is generated for each unit of movement. Axis can be 0 for up/down
// or 1 for left/right.
//
//==========================================================================

void FMouse::WheelMoved(int axis, int wheelmove)
{
	assert(axis == 0 || axis == 1);
	event_t ev = { 0 };
	int dir;

	WheelMove[axis] += wheelmove;

	if (WheelMove[axis] < 0)
	{
		dir = WHEEL_DELTA;
		ev.data1 = KEY_MWHEELDOWN;
	}
	else
	{
		dir = -WHEEL_DELTA;
		ev.data1 = KEY_MWHEELUP;
	}
	ev.data1 += axis * 2;

	if (!GUICapture)
	{
		while (abs(WheelMove[axis]) >= WHEEL_DELTA)
		{
			ev.type = EV_KeyDown;
			D_PostEvent(&ev);
			ev.type = EV_KeyUp;
			D_PostEvent(&ev);
			WheelMove[axis] += dir;
		}
	}
	else
	{
		ev.type = EV_GUI_Event;
		ev.subtype = ev.data1 - KEY_MWHEELUP + EV_GUI_WheelUp;
		if (GetKeyState(VK_SHIFT) & 0x8000)		ev.data3 |= GKM_SHIFT;
		if (GetKeyState(VK_CONTROL) & 0x8000)	ev.data3 |= GKM_CTRL;
		if (GetKeyState(VK_MENU) & 0x8000)		ev.data3 |= GKM_ALT;
		ev.data1 = 0;
		while (abs(WheelMove[axis]) >= WHEEL_DELTA)
		{
			D_PostEvent(&ev);
			WheelMove[axis] += dir;
		}
	}
}

//==========================================================================
//
// FMouse :: PostButtonEvent
//
// Posts a mouse button up/down event. Down events are always posted. Up
// events will only be sent if the button is currently marked as down.
//
//==========================================================================

void FMouse::PostButtonEvent(int button, bool down)
{
	event_t ev = { 0 };
	int mask = 1 << button;

	ev.data1 = KEY_MOUSE1 + button;
	if (down)
	{
		ButtonState |= mask;
		ev.type = EV_KeyDown;
		D_PostEvent(&ev);
	}
	else if (ButtonState & mask)
	{
		ButtonState &= ~mask;
		ev.type = EV_KeyUp;
		D_PostEvent(&ev);
	}
}

//==========================================================================
//
// FMouse :: ClearButtonState
//
// Sends key up events for all buttons that are currently down and marks
// them as up. Used when focus is lost and we can no longer track up events,
// so get them marked up right away.
//
//==========================================================================

void FMouse::ClearButtonState()
{
	if (ButtonState != 0)
	{
		int i, mask;
		event_t ev = { 0 };

		ev.type = EV_KeyUp;
		for (i = sizeof(ButtonState) * 8, mask = 1; i > 0; --i, mask <<= 1)
		{
			if (ButtonState & mask)
			{
				ev.data1 = KEY_MOUSE1 + (int)sizeof(ButtonState) * 8 - i;
				D_PostEvent(&ev);
			}
		}
		ButtonState = 0;
	}
	// Reset mouse wheel accumulation to 0.
	WheelMove[0] = 0;
	WheelMove[1] = 0;
}

//==========================================================================
//
// CreateRawMouse
//
//==========================================================================

static FMouse *CreateRawMouse()
{
	return new FRawMouse;
}

//==========================================================================
//
// FRawMouse - Constructor
//
//==========================================================================

FRawMouse::FRawMouse()
{
	Grabbed = false;
	SetCursorState(true);
}

//==========================================================================
//
// FRawMouse - Destructor
//
//==========================================================================

FRawMouse::~FRawMouse()
{
	Ungrab();
}

//==========================================================================
//
// FRawMouse :: GetDevice
//
// Ensure the API is present and we can listen for mouse input.
//
//==========================================================================

bool FRawMouse::GetDevice()
{
	RAWINPUTDEVICE rid;

	rid.usUsagePage = HID_GENERIC_DESKTOP_PAGE;
	rid.usUsage = HID_GDP_MOUSE;
	rid.dwFlags = 0;
	rid.hwndTarget = Window;
	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
	{
		return false;
	}
	rid.dwFlags = RIDEV_REMOVE;
	rid.hwndTarget = NULL;	// Must be NULL for RIDEV_REMOVE.
	RegisterRawInputDevices(&rid, 1, sizeof(rid));
	return true;
}

//==========================================================================
//
// FRawMouse :: Grab
//
//==========================================================================

void FRawMouse::Grab()
{
	if (!Grabbed)
	{
		RAWINPUTDEVICE rid;

		rid.usUsagePage = HID_GENERIC_DESKTOP_PAGE;
		rid.usUsage = HID_GDP_MOUSE;
		rid.dwFlags = RIDEV_CAPTUREMOUSE | RIDEV_NOLEGACY;
		rid.hwndTarget = Window;
		if (RegisterRawInputDevices(&rid, 1, sizeof(rid)))
		{
			GetCursorPos(&UngrabbedPointerPos);
			Grabbed = true;
			SetCursorState(false);
			// By setting the cursor position, we force the pointer image
			// to change right away instead of having it delayed until
			// some time in the future.
			CenterMouse(-1, -1, NULL, NULL);
		}
	}
}

//==========================================================================
//
// FRawMouse :: Ungrab
//
//==========================================================================

void FRawMouse::Ungrab()
{
	if (Grabbed)
	{
		RAWINPUTDEVICE rid;

		rid.usUsagePage = HID_GENERIC_DESKTOP_PAGE;
		rid.usUsage = HID_GDP_MOUSE;
		rid.dwFlags = RIDEV_REMOVE;
		rid.hwndTarget = NULL;
		if (RegisterRawInputDevices(&rid, 1, sizeof(rid)))
		{
			Grabbed = false;
			ClearButtonState();
		}
		SetCursorState(true);
		SetCursorPos(UngrabbedPointerPos.x, UngrabbedPointerPos.y);
	}
}

//==========================================================================
//
// FRawMouse :: ProcessRawInput
//
//==========================================================================

bool FRawMouse::ProcessRawInput(RAWINPUT *raw, int code)
{
	if (!Grabbed || raw->header.dwType != RIM_TYPEMOUSE || !use_mouse)
	{
		return false;
	}
	// Check buttons. The up and down motions are stored in the usButtonFlags field.
	// The ulRawButtons field, unfortunately, is device-dependant, and may well
	// not contain any data at all. This means it is apparently impossible
	// to read more than five mouse buttons with Windows, because RI_MOUSE_WHEEL
	// gets in the way when trying to extrapolate to more than five.
	for (int i = 0, mask = 1; i < 5; ++i)
	{
		if (raw->data.mouse.usButtonFlags & mask)	// button down
		{
			PostButtonEvent(i, true);
		}
		mask <<= 1;
		if (raw->data.mouse.usButtonFlags & mask)	// button up
		{
			PostButtonEvent(i, false);
		}
		mask <<= 1;
	}
	if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
	{
		WheelMoved(0, (SHORT)raw->data.mouse.usButtonData);
	}
	else if (raw->data.mouse.usButtonFlags & 0x800)	// horizontal mouse wheel
	{
		WheelMoved(1, (SHORT)raw->data.mouse.usButtonData);
	}
	int x = m_noprescale ? raw->data.mouse.lLastX : raw->data.mouse.lLastX << 2;
	int y = -raw->data.mouse.lLastY;
	PostMouseMove(x, y);
	if (x | y)
	{
		CenterMouse(-1, -1, NULL, NULL);
	}
	return true;
}

//==========================================================================
//
// FRawMouse :: WndProcHook
//
//==========================================================================

bool FRawMouse::WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	if (!Grabbed)
	{
		return false;
	}
	if (message == WM_SYSCOMMAND)
	{
		wParam &= 0xFFF0;
		if (wParam == SC_MOVE || wParam == SC_SIZE)
		{
			return true;
		}
	}
	return false;
}

/**************************************************************************/
/**************************************************************************/

//==========================================================================
//
// CreateDInputMouse
//
//==========================================================================

static FMouse *CreateDInputMouse()
{
	return new FDInputMouse;
}

//==========================================================================
//
// FDInputMouse - Constructor
//
//==========================================================================

FDInputMouse::FDInputMouse()
{
	Device = NULL;
	Grabbed = false;
	SetCursorState(true);
}

//==========================================================================
//
// FDInputMouse - Destructor
//
//==========================================================================

FDInputMouse::~FDInputMouse()
{
	if (Device != NULL)
	{
		Device->Release();
		Device = NULL;
	}
}

//==========================================================================
//
// FDInputMouse :: GetDevice
//
// Create the device interface and initialize it.
//
//==========================================================================

bool FDInputMouse::GetDevice()
{
	HRESULT hr;

	if (g_pdi3 != NULL)
	{ // DirectInput3 interface
		hr = g_pdi3->CreateDevice(GUID_SysMouse, (LPDIRECTINPUTDEVICE*)&Device, NULL);
	}
	else if (g_pdi != NULL)
	{ // DirectInput8 interface
		hr = g_pdi->CreateDevice(GUID_SysMouse, &Device, NULL);
	}
	else
	{
		hr = -1;
	}
	if (FAILED(hr))
	{
		return false;
	}
	
	// How many buttons does this mouse have?
	DIDEVCAPS_DX3 caps = { sizeof(caps) };
	hr = Device->GetCapabilities((DIDEVCAPS *)&caps);
	// If that failed, just assume four buttons.
	if (FAILED(hr))
	{
		caps.dwButtons = 4;
	}
	// Now select the data format with enough buttons for this mouse.
	// (Can we use c_dfDIMouse2 with DirectInput3? If so, then we could just set
	// that unconditionally.)
	hr = Device->SetDataFormat(caps.dwButtons <= 4 ? &c_dfDIMouse : &c_dfDIMouse2);
	if (FAILED(hr))
	{
ufailit:
		Device->Release();
		Device = NULL;
		return false;
	}

	hr = Device->SetCooperativeLevel(Window, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	if (FAILED(hr))
	{
		goto ufailit;
	}
	// Set buffer size so we can use buffered input.
	DIPROPDWORD prop;
	prop.diph.dwSize = sizeof(prop);
	prop.diph.dwHeaderSize = sizeof(prop.diph);
	prop.diph.dwObj = 0;
	prop.diph.dwHow = DIPH_DEVICE;
	prop.dwData = DINPUT_BUFFERSIZE;
	hr = Device->SetProperty(DIPROP_BUFFERSIZE, &prop.diph);
	if (FAILED(hr))
	{
		goto ufailit;
	}
	return true;
}

//==========================================================================
//
// FDInputMouse::WndProcHook
//
//==========================================================================

bool FDInputMouse::WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	// Do not allow window sizing or moving activity while the mouse is
	// grabbed, because they will never be able to complete, causing mouse
	// input to hang until the mouse is ungrabbed (e.g. by alt-tabbing away).
	if (Grabbed && message == WM_SYSCOMMAND)
	{
		wParam &= 0xFFF0;
		if (wParam == SC_MOVE || wParam == SC_SIZE)
		{
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// FDInputMouse :: ProcessInput
//
// Posts any events that have accumulated since the previous call.
//
//==========================================================================

void FDInputMouse::ProcessInput()
{
	DIDEVICEOBJECTDATA od;
	DWORD dwElements;
	HRESULT hr;
	int dx, dy;

	dx = 0;
	dy = 0;

	if (!Grabbed || !use_mouse)
		return;

	event_t ev = { 0 };
	for (;;)
	{
		DWORD cbObjectData = g_pdi3 ? sizeof(DIDEVICEOBJECTDATA_DX3) : sizeof(DIDEVICEOBJECTDATA);
		dwElements = 1;
		hr = Device->GetDeviceData(cbObjectData, &od, &dwElements, 0);
		if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
		{
			Grab();
			hr = Device->GetDeviceData(cbObjectData, &od, &dwElements, 0);
		}

		/* Unable to read data or no data available */
		if (FAILED(hr) || !dwElements)
			break;

		/* Look at the element to see what happened */
		// GCC does not like putting the DIMOFS_ macros in case statements,
		// so use ifs instead.
		if (od.dwOfs == (DWORD)DIMOFS_X)
		{
			dx += od.dwData;
		}
		else if (od.dwOfs == (DWORD)DIMOFS_Y)
		{
			dy += od.dwData;
		}
		else if (od.dwOfs == (DWORD)DIMOFS_Z)
		{
			WheelMoved(0, od.dwData);
		}
		else if (od.dwOfs >= (DWORD)DIMOFS_BUTTON0 && od.dwOfs <= (DWORD)DIMOFS_BUTTON7)
		{
			/* [RH] Mouse button events mimic keydown/up events */
			if (!GUICapture)
			{
				PostButtonEvent(od.dwOfs - DIMOFS_BUTTON0, !!(od.dwData & 0x80));
			}
		}
	}
	PostMouseMove(m_noprescale ? dx : dx<<2, -dy);
}

//==========================================================================
//
// FDInputMouse :: Grab
//
//==========================================================================

void FDInputMouse::Grab()
{
	if (SUCCEEDED(Device->Acquire()))
	{
		Grabbed = true;
		SetCursorState(false);
	}
}

//==========================================================================
//
// FDInputMouse :: Ungrab
//
//==========================================================================

void FDInputMouse::Ungrab()
{
	Device->Unacquire();
	Grabbed = false;
	SetCursorState(true);
	ClearButtonState();
}

/**************************************************************************/
/**************************************************************************/

//==========================================================================
//
// CreateWin32Mouse
//
//==========================================================================

static FMouse *CreateWin32Mouse()
{
	return new FWin32Mouse;
}

//==========================================================================
//
// FWin32Mouse - Constructor
//
//==========================================================================

FWin32Mouse::FWin32Mouse()
{
	GetCursorPos(&UngrabbedPointerPos);
	Grabbed = false;
	SetCursorState(true);
}

//==========================================================================
//
// FWin32Mouse - Destructor
//
//==========================================================================

FWin32Mouse::~FWin32Mouse()
{
	Ungrab();
}

//==========================================================================
//
// FWin32Mouse :: GetDevice
//
// The Win32 mouse is always available, since it is the lowest common
// denominator. (Even if it's not connected, it is still considered as
// "available"; it just won't generate any events.)
//
//==========================================================================

bool FWin32Mouse::GetDevice()
{
	return true;
}

//==========================================================================
//
// FWin32Mouse :: ProcessInput
//
// Get current mouse position and post events if the mouse has moved from
// last time.
//
//==========================================================================

void FWin32Mouse::ProcessInput()
{
	POINT pt;
	int x, y;

	if (!Grabbed || !use_mouse || !GetCursorPos(&pt))
	{
		return;
	}

	x = pt.x - PrevX;
	y = PrevY - pt.y;

	if (!m_noprescale)
	{
		x *= 3;
		y *= 2;
	}
	if (x | y)
	{
		CenterMouse(pt.x, pt.y, &PrevX, &PrevY);
	}
	PostMouseMove(x, y);
}

//==========================================================================
//
// FWin32Mouse :: WndProcHook
//
// Intercepts mouse-related window messages if the mouse is grabbed.
//
//==========================================================================

bool FWin32Mouse::WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	if (!Grabbed)
	{
		return false;
	}

	if (message == WM_SIZE)
	{
		if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
		{
			CenterMouse(-1, -1, &PrevX, &PrevY);
			return true;
		}
	}
	else if (message == WM_MOVE)
	{
		CenterMouse(-1, -1, &PrevX, &PrevY);
		return true;
	}
	else if (message == WM_SYSCOMMAND)
	{
		// Do not enter the window moving and sizing modes while grabbed,
		// because those require the mouse.
		wParam &= 0xFFF0;
		if (wParam == SC_MOVE || wParam == SC_SIZE)
		{
			return true;
		}
	}
	else if (!use_mouse)
	{
		// all following messages should only be processed if the mouse is in use
		return false;
	}
	else if (message == WM_MOUSEWHEEL)
	{
		WheelMoved(0, (SHORT)HIWORD(wParam));
		return true;
	}
	else if (message == WM_MOUSEHWHEEL)
	{
		WheelMoved(1, (SHORT)HIWORD(wParam));
		return true;
	}
	else if (message >= WM_LBUTTONDOWN && message <= WM_MBUTTONUP)
	{
		int action = (message - WM_LBUTTONDOWN) % 3;
		int button = (message - WM_LBUTTONDOWN) / 3;

		if (action == 2)
		{ // double clicking we care not about.
			return false;
		}
		event_t ev = { 0 };
		ev.type = action ? EV_KeyUp : EV_KeyDown;
		ev.data1 = KEY_MOUSE1 + button;
		if (action)
		{
			ButtonState &= ~(1 << button);
		}
		else
		{
			ButtonState |= 1 << button;
		}
		D_PostEvent(&ev);
		return true;
	}
	else if (message >= WM_XBUTTONDOWN && message <= WM_XBUTTONUP)
	{
		// Microsoft's (lack of) documentation for the X buttons is unclear on whether
		// or not simultaneous pressing of multiple X buttons will ever be merged into
		// a single message. Winuser.h describes the button field as being filled with
		// flags, which suggests that it could merge them. My testing
		// indicates it does not, but I will assume it might in the future.
		auto xbuttons = GET_XBUTTON_WPARAM(wParam);
		event_t ev = { 0 };

		ev.type = (message == WM_XBUTTONDOWN) ? EV_KeyDown : EV_KeyUp;

		// There are only two X buttons defined presently, so I extrapolate from
		// the current winuser.h values to support up to 8 mouse buttons.
		for (int i = 0; i < 5; ++i, xbuttons >>= 1)
		{
			if (xbuttons & 1)
			{
				ev.data1 = KEY_MOUSE4 + i;
				if (ev.type == EV_KeyDown)
				{
					ButtonState |= 1 << (i + 4);
				}
				else
				{
					ButtonState &= ~(1 << (i + 4));
				}
				D_PostEvent(&ev);
			}
		}
		*result = TRUE;
		return true;
	}
	return false;
}

//==========================================================================
//
// FWin32Mouse :: Grab
//
// Hides the mouse and locks it inside the window boundaries.
//
//==========================================================================

void FWin32Mouse::Grab()
{
	RECT rect;

	if (Grabbed)
	{
		return;
	}

	GetCursorPos(&UngrabbedPointerPos);
	ClipCursor(NULL);		// helps with Win95?
	GetClientRect(Window, &rect);

	// Reposition the rect so that it only covers the client area.
	ClientToScreen(Window, (LPPOINT)&rect.left);
	ClientToScreen(Window, (LPPOINT)&rect.right);

	ClipCursor(&rect);
	SetCursorState(false);
	CenterMouse(-1, -1, &PrevX, &PrevY);
	Grabbed = true;
}

//==========================================================================
//
// FWin32Mouse :: Ungrab
//
// Shows the mouse and lets it roam free.
//
//==========================================================================

void FWin32Mouse::Ungrab()
{
	if (!Grabbed)
	{
		return;
	}

	ClipCursor(NULL);
	SetCursorPos(UngrabbedPointerPos.x, UngrabbedPointerPos.y);
	SetCursorState(true);
	Grabbed = false;
	ClearButtonState();
}

/**************************************************************************/
/**************************************************************************/

//==========================================================================
//
// I_StartupMouse
//
// Called during game init and whenever in_mouse changes.
//
//==========================================================================

void I_StartupMouse ()
{
	EMouseMode new_mousemode;

	switch(in_mouse)
	{
	case 1:
		new_mousemode = MM_Win32;
		break;

	case 2:
		new_mousemode = MM_DInput;
		break;

	default:
	case 3:
		new_mousemode = MM_RawInput;
		break;
	}
	if (new_mousemode != MouseMode)
	{
		if (Mouse != NULL)
		{
			delete Mouse;
		}
		do
		{
			Mouse = MouseFactory[new_mousemode - MM_Win32]();
			if (Mouse != NULL)
			{
				if (Mouse->GetDevice())
				{
					break;
				}
				delete Mouse;
				Mouse = NULL;
			}
			new_mousemode = (EMouseMode)(new_mousemode - 1);
		}
		while (new_mousemode != MM_None);
		MouseMode = new_mousemode;
		NativeMouse = true;
	}
}
