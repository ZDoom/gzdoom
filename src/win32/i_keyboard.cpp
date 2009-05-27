// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x800
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <dinput.h>

#define USE_WINDOWS_DWORD
#include "i_input.h"
#include "i_system.h"
#include "d_event.h"
#include "d_gui.h"
#include "c_cvars.h"
#include "doomdef.h"
#include "doomstat.h"
#include "win32iface.h"
#include "rawinput.h"

// MACROS ------------------------------------------------------------------

#define DINPUT_BUFFERSIZE	32

// TYPES -------------------------------------------------------------------

class FDInputKeyboard : public FInputDevice
{
public:
	FDInputKeyboard();
	~FDInputKeyboard();
	
	bool GetDevice();
	void ProcessInput();
	bool WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result);

protected:
	LPDIRECTINPUTDEVICE8 Device;
	BYTE KeyStates[256/8];

	int CheckKey(int keynum) const
	{
		return KeyStates[keynum >> 3] & (1 << (keynum & 7));
	}
	void SetKey(int keynum, bool down)
	{
		if (down)
		{
			KeyStates[keynum >> 3] |= 1 << (keynum & 7);
		}
		else
		{
			KeyStates[keynum >> 3] &= ~(1 << (keynum & 7));
		}
	}
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern LPDIRECTINPUT8 g_pdi;
extern LPDIRECTINPUT g_pdi3;
extern bool GUICapture;
extern bool HaveFocus;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Convert DIK_* code to ASCII using Qwerty keymap
static const BYTE Convert [256] =
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

FInputDevice *Keyboard;

// Set this to false to make keypad-enter a usable separate key.
CVAR (Bool, k_mergekeys, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FDInputKeyboard - Constructor
//
//==========================================================================

FDInputKeyboard::FDInputKeyboard()
{
	Device = NULL;
	memset(KeyStates, 0, sizeof(KeyStates));
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
	// Set cooperative level.
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
	int key;
	bool foreground = (GetForegroundWindow() == Window);

	event_t ev = { 0 };
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

		key = od.dwOfs;
//		Printf("buffer ofs=%02x data=%02x\n", od.dwOfs, od.dwData);
		if (key >= 1 && key <= 255)
		{
			if (k_mergekeys)
			{
				// "Merge" multiple keys that are considered to be the same.
				if (key == DIK_NUMPADENTER)
				{
					key = DIK_RETURN;
				}
				else if (key == DIK_RMENU)
				{
					key = DIK_LMENU;
				}
				else if (key == DIK_RCONTROL)
				{
					key = DIK_LCONTROL;
				}
				else if (key == DIK_RSHIFT)
				{
					key = DIK_LSHIFT;
				}
			}
			// Generate an event, but only if it isn't a repeat of the existing state.
			if (od.dwData & 0x80)
			{
				if (!foreground || GUICapture)
				{ // Do not generate key down events if we are in the background
				  // or in "GUI Capture" mode.
					continue;
				}
				if (CheckKey(key))
				{ // Key is already down.
					continue;
				}
				SetKey(key, true);
				ev.type = EV_KeyDown;
			}
			else
			{
				if (!CheckKey(key))
				{ // Key is already up.
					continue;
				}
				SetKey(key, false);
				ev.type = EV_KeyUp;
			}
			ev.data1 = key;
			ev.data2 = Convert[key];
			D_PostEvent(&ev);
		}
	}
}

//==========================================================================
//
// FDInputKeyboard :: WndProcHook
//
//==========================================================================

bool FDInputKeyboard::WndProcHook(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, LRESULT *result)
{
	return false;
}

//==========================================================================
//
// I_StartupKeyboard
//
//==========================================================================

void I_StartupKeyboard()
{
	Keyboard = new FDInputKeyboard;
	if (!Keyboard->GetDevice())
	{
		delete Keyboard;
	}
}

