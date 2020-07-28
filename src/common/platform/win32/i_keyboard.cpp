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


// MACROS ------------------------------------------------------------------

#define DINPUT_BUFFERSIZE	32

// MinGW-w64 (TDM5.1 - 2016/11/21)
#ifndef DIK_PREVTRACK
#define DIK_PREVTRACK DIK_CIRCUMFLEX
#endif

// TYPES -------------------------------------------------------------------

class FDInputKeyboard : public FKeyboard
{
public:
	FDInputKeyboard();
	~FDInputKeyboard();
	
	bool GetDevice();
	void ProcessInput();

protected:
	LPDIRECTINPUTDEVICE8 Device;
};

class FRawKeyboard : public FKeyboard
{
public:
	FRawKeyboard();
	~FRawKeyboard();

	bool GetDevice();
	bool ProcessRawInput(RAWINPUT *rawinput, int code);

protected:
	USHORT E1Prefix;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern LPDIRECTINPUT8 g_pdi;
extern LPDIRECTINPUT g_pdi3;
extern bool GUICapture;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Convert DIK_* code to ASCII using Qwerty keymap
static const uint8_t Convert[256] =
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

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FKeyboard *Keyboard;

// Set this to false to make keypad-enter a usable separate key.
CVAR (Bool, k_mergekeys, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FKeyboard - Constructor
//
//==========================================================================

FKeyboard::FKeyboard()
{
	memset(KeyStates, 0, sizeof(KeyStates));
}

//==========================================================================
//
// FKeyboard - Destructor
//
//==========================================================================

FKeyboard::~FKeyboard()
{
	AllKeysUp();
}

//==========================================================================
//
// FKeyboard :: CheckAndSetKey
//
// Returns true if the key was already in the desired state, false if it
// wasn't.
//
//==========================================================================

bool FKeyboard::CheckAndSetKey(int keynum, INTBOOL down)
{
	uint8_t *statebyte = &KeyStates[keynum >> 3];
	uint8_t mask = 1 << (keynum & 7);
	if (down)
	{
		if (*statebyte & mask)
		{
			return true;
		}
		*statebyte |= mask;
		return false;
	}
	else
	{
		if (*statebyte & mask)
		{
			*statebyte &= ~mask;
			return false;
		}
		return true;
	}
}

//==========================================================================
//
// FKeyboard :: AllKeysUp
//
// For every key currently marked as down, send a key up event and clear it.
//
//==========================================================================

void FKeyboard::AllKeysUp()
{
	event_t ev = { 0 };
	ev.type = EV_KeyUp;

	for (int i = 0; i < 256/8; ++i)
	{
		if (KeyStates[i] != 0)
		{
			uint8_t states = KeyStates[i];
			int j = 0;
			KeyStates[i] = 0;
			do
			{
				if (states & 1)
				{
					ev.data1 = (i << 3) + j;
					ev.data2 = Convert[ev.data1];
					D_PostEvent(&ev);
				}
				states >>= 1;
				++j;
			}
			while (states != 0);
		}
	}
}

//==========================================================================
//
// FKeyboard :: PostKeyEvent
//
// Posts a keyboard event, but only if the state is different from what we
// currently think it is. (For instance, raw keyboard input sends key
// down events every time the key automatically repeats, so we want to
// discard those.)
//
//==========================================================================

void FKeyboard::PostKeyEvent(int key, INTBOOL down, bool foreground)
{
	event_t ev = { 0 };

//	Printf("key=%02x down=%02x\n", key, down);
	// "Merge" multiple keys that are considered to be the same. If the
	// original unmerged key is down, it also needs to go up. (In case
	// somebody was holding the key down when they changed this setting.)
	if (k_mergekeys)
	{
		if (key == DIK_NUMPADENTER || key == DIK_RMENU || key == DIK_RCONTROL)
		{
			k_mergekeys = false;
			PostKeyEvent(key, false, foreground);
			k_mergekeys = true;
			key &= 0x7F;
		}
		else if (key == DIK_RSHIFT)
		{
			k_mergekeys = false;
			PostKeyEvent(key, false, foreground);
			k_mergekeys = true;
			key = DIK_LSHIFT;
		}
	}
	if (key == 0x59)
	{ // Turn kp= on a Mac keyboard into kp= on a PC98 keyboard.
		key = DIK_NUMPADEQUALS;
	}

	// Generate the event, if appropriate.
	if (down)
	{
		if (!foreground || GUICapture)
		{ // Do not generate key down events if we are in the background
		  // or in "GUI Capture" mode.
			return;
		}
		ev.type = EV_KeyDown;
	}
	else
	{
		ev.type = EV_KeyUp;
	}
	if (CheckAndSetKey(key, down))
	{ // Key is already down or up.
		return;
	}
	ev.data1 = key;
	ev.data2 = Convert[key];
	D_PostEvent(&ev);
}

//==========================================================================
//
// FDInputKeyboard - Constructor
//
//==========================================================================

FDInputKeyboard::FDInputKeyboard()
{
	Device = NULL;
}

//==========================================================================
//
// FDInputKeyboard - Destructor
//
//==========================================================================

FDInputKeyboard::~FDInputKeyboard()
{
	if (Device != NULL)
	{
		Device->Release();
		Device = NULL;
	}
}

//==========================================================================
//
// FDInputKeyboard :: GetDevice
//
// Create the device interface and initialize it.
//
//==========================================================================

bool FDInputKeyboard::GetDevice()
{
	HRESULT hr;

	if (g_pdi3 != NULL)
	{ // DirectInput3 interface
		hr = g_pdi3->CreateDevice(GUID_SysKeyboard, (LPDIRECTINPUTDEVICE*)&Device, NULL);
	}
	else if (g_pdi != NULL)
	{ // DirectInput8 interface
		hr = g_pdi->CreateDevice(GUID_SysKeyboard, &Device, NULL);
	}
	else
	{
		hr = -1;
	}
	if (FAILED(hr))
	{
		return false;
	}

	// Yes, this is a keyboard.
	hr = Device->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(hr))
	{
ufailit:
		Device->Release();
		Device = NULL;
		return false;
	}

	hr = Device->SetCooperativeLevel(Window, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
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
	Device->Acquire();
	return true;
}

//==========================================================================
//
// FDInputKeyboard :: ProcessInput
//
//==========================================================================

void FDInputKeyboard::ProcessInput()
{
	DIDEVICEOBJECTDATA od;
	DWORD dwElements;
	HRESULT hr;
	bool foreground = (GetForegroundWindow() == Window);

	for (;;)
	{
		DWORD cbObjectData = g_pdi3 ? sizeof(DIDEVICEOBJECTDATA_DX3) : sizeof(DIDEVICEOBJECTDATA);
		dwElements = 1;
		hr = Device->GetDeviceData(cbObjectData, &od, &dwElements, 0);
		if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
		{
			Device->Acquire();
			hr = Device->GetDeviceData(cbObjectData, &od, &dwElements, 0);
		}
		if (FAILED(hr) || !dwElements)
		{
			break;
		}

		if (od.dwOfs >= 1 && od.dwOfs <= 255)
		{
			PostKeyEvent(od.dwOfs, od.dwData & 0x80, foreground);
		}
	}
}

/**************************************************************************/
/**************************************************************************/

//==========================================================================
//
// FRawKeyboard - Constructor
//
//==========================================================================

FRawKeyboard::FRawKeyboard()
{
	E1Prefix = 0;
}

//==========================================================================
//
// FRawKeyboard - Destructor
//
//==========================================================================

FRawKeyboard::~FRawKeyboard()
{
	RAWINPUTDEVICE rid;
	rid.usUsagePage = HID_GENERIC_DESKTOP_PAGE;
	rid.usUsage = HID_GDP_KEYBOARD;
	rid.dwFlags = RIDEV_REMOVE;
	rid.hwndTarget = NULL;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

//==========================================================================
//
// FRawKeyboard :: GetDevice
//
// Ensure the API is present and we can listen for keyboard input.
//
//==========================================================================

bool FRawKeyboard::GetDevice()
{
	RAWINPUTDEVICE rid;

	rid.usUsagePage = HID_GENERIC_DESKTOP_PAGE;
	rid.usUsage = HID_GDP_KEYBOARD;
	rid.dwFlags = RIDEV_INPUTSINK;
	rid.hwndTarget = Window;
	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
	{
		return false;
	}
	return true;
}

//==========================================================================
//
// FRawKeyboard :: ProcessRawInput
//
// Convert scan codes to DirectInput key codes. For the most part, this is
// straight forward: Scan codes without any prefix are passed unmodified.
// Scan codes with an 0xE0 prefix byte are generally passed by ORing them
// with 0x80. And scan codes with an 0xE1 prefix are the annoying Pause key
// which will generate another scan code that looks like Num Lock.
//
// This is a bit complicated only because the state of PC key codes is a bit
// of a mess. Keyboards may use simpler codes internally, but for the sake
// of compatibility, programs are presented with XT-compatible codes. This
// means that keys which were originally a shifted form of another key and
// were split off into a separate key all their own, or which were formerly
// a separate key and are now part of another key (most notable PrtScn and
// SysRq), will still generate code sequences that XT-era software will
// still perceive as the original sequences to use those keys.
//
//==========================================================================

bool FRawKeyboard::ProcessRawInput(RAWINPUT *raw, int code)
{
	if (raw->header.dwType != RIM_TYPEKEYBOARD)
	{
		return false;
	}
	int keycode = raw->data.keyboard.MakeCode;
	if (keycode == 0 && (raw->data.keyboard.Flags & RI_KEY_E0))
	{ // Even if the make code is 0, we might still be able to extract a
	  // useful key from the message.
		if (raw->data.keyboard.VKey >= VK_BROWSER_BACK && raw->data.keyboard.VKey <= VK_LAUNCH_APP2)
		{
			static const uint8_t MediaKeys[VK_LAUNCH_APP2 - VK_BROWSER_BACK + 1] =
			{
				DIK_WEBBACK, DIK_WEBFORWARD, DIK_WEBREFRESH, DIK_WEBSTOP,
				DIK_WEBSEARCH, DIK_WEBFAVORITES, DIK_WEBHOME,

				DIK_MUTE, DIK_VOLUMEDOWN, DIK_VOLUMEUP,
				DIK_NEXTTRACK, DIK_PREVTRACK, DIK_MEDIASTOP, DIK_PLAYPAUSE,

				DIK_MAIL, DIK_MEDIASELECT, DIK_MYCOMPUTER, DIK_CALCULATOR
			};
			keycode = MediaKeys[raw->data.keyboard.VKey - VK_BROWSER_BACK];
		}
	}
	if (keycode < 1 || keycode > 0xFF)
	{
		return false;
	}
	if (raw->data.keyboard.Flags & RI_KEY_E1)
	{
		E1Prefix = raw->data.keyboard.MakeCode;
		return false;
	}
	if (raw->data.keyboard.Flags & RI_KEY_E0)
	{
		if (keycode == DIK_LSHIFT || keycode == DIK_RSHIFT)
		{ // Ignore fake shifts.
			return false;
		}
		keycode |= 0x80;
	}
	// The sequence for an unshifted pause is E1 1D 45 (E1 Prefix +
	// Control + Num Lock).
	if (E1Prefix)
	{
		if (E1Prefix == 0x1D && keycode == DIK_NUMLOCK)
		{
			keycode = DIK_PAUSE;
			E1Prefix = 0;
		}
		else
		{
			E1Prefix = 0;
			return false;
		}
	}
	// If you press Ctrl+Pause, the keyboard sends the Break make code
	// E0 46 instead of the Pause make code.
	if (keycode == 0xC6)
	{
		keycode = DIK_PAUSE;
	}
	// If you press Ctrl+PrtScn (to get SysRq), the keyboard sends
	// the make code E0 37. If you press PrtScn without any modifiers,
	// it sends E0 2A E0 37. And if you press Alt+PrtScn, it sends 54
	// (which is undefined in the charts I can find.)
	if (keycode == 0x54)
	{
		keycode = DIK_SYSRQ;
	}
	// If you press any keys in the island between the main keyboard
	// and the numeric keypad with Num Lock turned on, they generate
	// a fake shift before their actual codes. They do not generate this
	// fake shift if Num Lock is off. We unconditionally discard fake
	// shifts above, so we don't need to do anything special for these,
	// since they are also prefixed by E0 so we can tell them apart from
	// their keypad counterparts.

	// Okay, we're done translating the keycode. Post it (or ignore it.)
	PostKeyEvent(keycode, !(raw->data.keyboard.Flags & RI_KEY_BREAK), code == RIM_INPUT);
	return true;
}

//==========================================================================
//
// I_StartupKeyboard
//
//==========================================================================

void I_StartupKeyboard()
{
	Keyboard = new FRawKeyboard;
	if (Keyboard->GetDevice())
	{
		return;
	}
	delete Keyboard;
	Keyboard = new FDInputKeyboard;
	if (!Keyboard->GetDevice())
	{
		delete Keyboard;
	}
}

