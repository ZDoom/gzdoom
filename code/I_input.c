#define DIRECTINPUT_VERSION 0x300

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include "i_input.h"

#define __BYTEBOOL__
#include "i_music.h"

#include "d_main.h"
#include "c_cvars.h"
#include "i_system.h"
#include "doomdef.h"

extern void *DDBack;
extern HWND Window;

LPDIRECTINPUTDEVICE g_pMouse;

cvar_t *i_remapkeypad;
cvar_t *usejoystick;
cvar_t *usemouse;

static char Convert2[256];
static const char Convert []={
0,
27,
'1',
'2',
'3',
'4',
'5',
'6',
'7',
'8',
'9',
'0',
'-',
'=',
8,//Backspace
9,//Tab
'q',
'w',
'e',
'r',
't',
'y',
'u',
'i',
'o',
'p',
'[',
']',
13,
0,//DIK_LCONTROL		0x1D
'a',
's',
'd',
'f',
'g',
'h',
'j',
'k',
'l',
';',
39,//'
'`',//DIK_GRAVE 		  0x29	  /* accent grave */
0,//DIK_LSHIFT			0x2A
'\\',//DIK_BACKSLASH	   0x2B
'z',
'x',
'c',
'v',
'b',
'n',
'm',
',',
'.',
'/',
0,//DIK_RSHIFT			0x36
'*',//DIK_MULTIPLY		  0x37	  /* * on numeric keypad */
0,//DIK_LMENU			0x38	/* left Alt */
' ',//DIK_SPACE 		  0x39
0,//DIK_CAPITAL 		0x3A
0,//DIK_F1				0x3B
0,//DIK_F2				0x3C
0,//DIK_F3				0x3D
0,//DIK_F4				0x3E
0,//DIK_F5				0x3F
0,//DIK_F6				0x40
0,//DIK_F7				0x41
0,//DIK_F8				0x42
0,//DIK_F9				0x43
0,//DIK_F10 			0x44
0,//DIK_NUMLOCK 		0x45
0,//DIK_SCROLL			0x46	/* Scroll Lock */
'7',//DIK_NUMPAD7		  0x47
'8',//DIK_NUMPAD8		  0x48
'9',//DIK_NUMPAD9		  0x49
'-',//DIK_SUBTRACT		  0x4A	  /* - on numeric keypad */
'4',//DIK_NUMPAD4		  0x4B
'5',//DIK_NUMPAD5		  0x4C
'6',//DIK_NUMPAD6		  0x4D
'+',//DIK_ADD			  0x4E	  /* + on numeric keypad */
'1',//DIK_NUMPAD1		  0x4F
'2',//DIK_NUMPAD2		  0x50
'3',//DIK_NUMPAD3		  0x51
'0',//DIK_NUMPAD0		  0x52
'.',//DIK_DECIMAL		  0x53	  /* . on numeric keypad */
0,//DIK_NULL			 0x54
0,//DIK_NULL			 0x55
0,//DIK_NULL			 0x56
0,//DIK_F11 			0x57
0,//DIK_F12 			0x58
0,//DIK_NULL			 0x59
0,//DIK_NULL			 0x5a
0,//DIK_NULL			 0x5b
0,//DIK_NULL			 0x5c
0,//DIK_NULL			 0x5d
0,//DIK_NULL			 0x5e
0,//DIK_NULL			 0x5f
0,//DIK_NULL			 0x60
0,//DIK_NULL			 0x61
0,//DIK_NULL			 0x62
0,//DIK_NULL			 0x63
0,//DIK_F13 			0x64	/*					   (NEC PC98) */
0,//DIK_F14 			0x65	/*					   (NEC PC98) */
0,//DIK_F15 			0x66	/*					   (NEC PC98) */
0,//DIK_NULL			 0x67
0,//DIK_NULL			 0x68
0,//DIK_NULL			 0x69
0,//DIK_NULL			 0x6a
0,//DIK_NULL			 0x6b
0,//DIK_NULL			 0x6c
0,//DIK_NULL			 0x6d
0,//DIK_NULL			 0x6e
0,//DIK_NULL			 0x6f
0,//DIK_KANA			0x70	/* (Japanese keyboard)			  */
0,//DIK_NULL			 0x71
0,//DIK_NULL			 0x72
0,//DIK_NULL			 0x73
0,//DIK_NULL			 0x74
0,//DIK_NULL			 0x75
0,//DIK_NULL			 0x76
0,//DIK_NULL			 0x77
0,//DIK_NULL			 0x78
0,//DIK_CONVERT 		0x79	/* (Japanese keyboard)			  */
0,//DIK_NULL			 0x7a
0,//DIK_NOCONVERT		0x7B	/* (Japanese keyboard)			  */
0,//DIK_NULL			 0x7c
0,//DIK_YEN 			0x7D	/* (Japanese keyboard)			  */
0,//DIK_NULL			 0x7e
0,//DIK_NULL			 0x7f
0,//DIK_NULL			 0x80
0,//DIK_NULL			 0x81
0,//DIK_NULL			 0x82
0,//DIK_NULL			 0x83
0,//DIK_NULL			 0x84
0,//DIK_NULL			 0x85
0,//DIK_NULL			 0x86
0,//DIK_NULL			 0x87
0,//DIK_NULL			 0x88
0,//DIK_NULL			 0x89
0,//DIK_NULL			 0x8a
0,//DIK_NULL			 0x8b
0,//DIK_NULL			 0x8c

'=',//DIK_NUMPADEQUALS	0x8D	/* = on numeric keypad (NEC PC98) */
0,//DIK_NULL			 0x8e
0,//DIK_NULL			 0x8f
0,//DIK_CIRCUMFLEX		0x90	/* (Japanese keyboard)			  */
'@',//DIK_AT				0x91	/*					   (NEC PC98) */
':',//DIK_COLON 		  0x92	  /*					 (NEC PC98) */
'_',//DIK_UNDERLINE 	  0x93	  /*					 (NEC PC98) */
0,//DIK_KANJI			0x94	/* (Japanese keyboard)			  */
0,//DIK_STOP			0x95	/*					   (NEC PC98) */
0,//DIK_AX				0x96	/*					   (Japan AX) */
0,//DIK_UNLABELED		0x97	/*						  (J3100) */
0,//DIK_NULL			 0x98
0,//DIK_NULL			 0x99
0,//DIK_NULL			 0x9a
0,//DIK_NULL			 0x9b
13,//DIK_NUMPADENTER 	0x9C	/* Enter on numeric keypad */
0,//DIK_RCONTROL		0x9D
0,//DIK_NULL			 0x9e
0,//DIK_NULL			 0x9f
0,//DIK_NULL			 0xa0
0,//DIK_NULL			 0xa1
0,//DIK_NULL			 0xa2
0,//DIK_NULL			 0xa3
0,//DIK_NULL			 0xa4
0,//DIK_NULL			 0xa5
0,//DIK_NULL			 0xa6
0,//DIK_NULL			 0xa7
0,//DIK_NULL			 0xa8
0,//DIK_NULL			 0xa9
0,//DIK_NULL			 0xaa
0,//DIK_NULL			 0xab
0,//DIK_NULL			 0xac
0,//DIK_NULL			 0xad
0,//DIK_NULL			 0xae
0,//DIK_NULL			 0xaf
0,//DIK_NULL			 0xb0
0,//DIK_NULL			 0xb1
0,//DIK_NULL			 0xb2
',',//DIK_NUMPADCOMMA	  0xB3	  /* , on numeric keypad (NEC PC98) */
0,//DIK_NULL			 0xb4
'/',//DIK_DIVIDE		  0xB5	  /* / on numeric keypad */
0,//DIK_NULL			 0xb6
0,//DIK_SYSRQ			 0xB7
0,//DIK_RMENU			 0xB8	 /* right Alt */
0,//DIK_NULL			 0xb9
0,//DIK_NULL			 0xc0
0,//DIK_NULL			 0xc1
0,//DIK_NULL			 0xc2
0,//DIK_NULL			 0xc3
0,//DIK_NULL			 0xc4
0,//DIK_NULL			 0xc5
0,//DIK_NULL			 0xc6
0,//DIK_HOME			 0xC7	 /* Home on arrow keypad */
0,//DIK_UP				 0xC8	 /* UpArrow on arrow keypad */
0,//DIK_PRIOR			 0xC9	 /* PgUp on arrow keypad */
0,//DIK_NULL			 0xca
0,//DIK_LEFT			 0xCB	 /* LeftArrow on arrow keypad */
0,//DIK_NULL			 0xcc
0,//DIK_RIGHT			 0xCD	 /* RightArrow on arrow keypad */
0,//DIK_NULL			 0xce
0,//DIK_END 			 0xCF	 /* End on arrow keypad */
0,//DIK_DOWN			 0xD0	 /* DownArrow on arrow keypad */
0,//DIK_NEXT			 0xD1	 /* PgDn on arrow keypad */
0,//DIK_INSERT			 0xD2	 /* Insert on arrow keypad */
0,//DIK_DELETE			 0xD3	 /* Delete on arrow keypad */
0,//DIK_NULL			 0xd4
0,//DIK_NULL			 0xd5
0,//DIK_NULL			 0xd6
0,//DIK_NULL			 0xd7
0,//DIK_NULL			 0xd8
0,//DIK_NULL			 0xd9
0,//DIK_NULL			 0xda
0,//DIK_LWIN			 0xDB	 /* Left Windows key */
0,//DIK_RWIN			 0xDC	 /* Right Windows key */
0//DIK_APPS 			 0xDD	 /* AppMenu key */

};

static BOOL altdown = FALSE;

LRESULT CALLBACK WndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	event_t event;

	event.data1 = event.data2 = event.data3 = 0;

	switch(message) {
		case WM_DESTROY:
			PostQuitMessage (0);
			exit (0);
			break;

		case WM_PAINT:
			if (!DDBack) {
				I_PaintConsole ();
			} else {
				return DefWindowProc (hWnd, message, wParam, lParam);
			}
			break;

		case MM_MCINOTIFY:
			if (wParam == MCI_NOTIFY_SUCCESSFUL)
				I_RestartSong ();
			break;
/*
		case WM_ACTIVATE:
			{
				WORD fActive = LOWORD (wParam);
			//	BOOL fMinimized = (BOOL) HIWORD (wParam);

				if (fActive) {
					if (g_pMouse) IDirectInputDevice_Acquire (g_pMouse);
					if (g_pKey) IDirectInputDevice_Acquire (g_pKey);
				} else {
					if (g_pMouse) IDirectInputDevice_Unacquire (g_pMouse);
					if (g_pKey) IDirectInputDevice_Unacquire (g_pKey);
				}
			}
			break;
*/
		case WM_KILLFOCUS:
			if (g_pKey) IDirectInputDevice_Unacquire (g_pKey);
			if (g_pMouse) IDirectInputDevice_Unacquire (g_pMouse);
			if (altdown) {
				altdown = FALSE;
				event.type = ev_keyup;
				event.data1 = DIK_LALT;
				D_PostEvent (&event);
			}
//			paused = 1;
			break;

		case WM_SETFOCUS:
			if (g_pKey) IDirectInputDevice_Acquire (g_pKey);
			if (g_pMouse) IDirectInputDevice_Acquire (g_pMouse);
			break;

		// Being forced to separate my keyboard input handler into
		// two pieces like this really stinks. (IMHO)
		case WM_KEYDOWN:
		case WM_KEYUP:
			if (message == WM_KEYUP) {
				event.type = ev_keyup;
			} else {
				if (lParam & 0x40000000)
					event.type = ev_keyrepeat;
				else
					event.type = ev_keydown;
			}

			switch (wParam) {
				case VK_PAUSE:
					event.data1 = KEY_PAUSE;
					break;
				case VK_TAB:
					event.data1 = DIK_TAB;
					break;
				case VK_NUMLOCK:
					event.data1 = DIK_NUMLOCK;
					break;
				case VK_SHIFT:
					event.data1 = KEY_LSHIFT;
					break;
				case VK_CONTROL:
					event.data1 = KEY_LCTRL;
					break;
			}
			if (event.data1)
				D_PostEvent (&event);
			break;

		case WM_SYSKEYDOWN:
			SendMessage (hWnd, WM_KEYDOWN, wParam, lParam);
			return 0;

		case WM_SYSKEYUP:
			SendMessage (hWnd, WM_KEYUP, wParam, lParam);
			return 0;

		default:
			return DefWindowProc (hWnd, message, wParam, lParam);
	}

	return (long) 0;
}

static void BuildCvt2Table (void)
{
	int i;

	for (i = 0; i < 256; i++)
		Convert2[i] = MapVirtualKey (MapVirtualKey (i, 1), 2);
}

/****** Stuff from Andy Bay's myjoy.c ******/

struct {
	int X,Y,Z,R,U,V;
}			JoyBias;
int 		JoyActive;
JOYINFOEX	JoyStats;
JOYCAPS 	JoyCaps;


void JoyFixBias (void)
{
	JoyBias.X = (JoyCaps.wXmin + JoyCaps.wXmax) / 2;
	JoyBias.Y = (JoyCaps.wYmin + JoyCaps.wYmax) / 2;
	JoyBias.Z = (JoyCaps.wZmin + JoyCaps.wZmax) / 2;
	JoyBias.R = (JoyCaps.wRmin + JoyCaps.wRmax) / 2;
	JoyBias.U = (JoyCaps.wUmin + JoyCaps.wUmax) / 2;
	JoyBias.V = (JoyCaps.wVmin + JoyCaps.wVmax) / 2;
}

void DI_JoyCheck (void)
{
	event_t joyevent;

	if (JoyActive) {		//need a real joystick
		JoyStats.dwFlags = JOY_RETURNALL;
		if (joyGetPosEx (0, &JoyStats)) {
			JoyActive = 0;
			return;
		}

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
	Printf ("DI_InitJoy: Initialize joystick\n");
	JoyActive = joyGetNumDevs ();
	if (JoyActive) {
		JoyStats.dwSize = sizeof(JOYINFOEX);
		joyGetDevCaps (0, &JoyCaps, sizeof(JOYCAPS));
		JoyFixBias();
	}
	return TRUE;
}

/****** Stuff from Andy Bay's mymouse.c ******

/****************************************************************************
 *
 *		DIInit
 *
 *		Initialize the DirectInput variables.
 *
 ****************************************************************************/

BOOL DI_Init(HWND hwnd)
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

	Printf ("DI_Init: Initialize mouse\n");

	/*
	 *	Register with DirectInput and get an IDirectInput to play with.
	 */
	hr = DirectInputCreate (g_hInst, DIRECTINPUT_VERSION, &g_pdi, NULL);

	if (FAILED(hr)) {
		I_Error ("Could not obtain DirectInput interface\n");
	}

	/*
	 *	Obtain an interface to the system mouse device.
	 */
	hr = IDirectInput_CreateDevice (g_pdi, &GUID_SysMouse, &g_pMouse, NULL);

	if (FAILED(hr)) {
		Printf ("Warning: Could not create mouse device\n");
		goto nextinit;
	}

	/*
	 *	Set the data format to "mouse format".
	 */
	hr = IDirectInputDevice_SetDataFormat (g_pMouse, &c_dfDIMouse);

	if (FAILED(hr)) {
		Printf ("Waring: Could not set mouse data format\n");
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
		goto nextinit;
	}

	/*
	 *	Set the cooperative level.
	 */
	hr = IDirectInputDevice_SetCooperativeLevel (g_pMouse,hwnd,
									   DISCL_EXCLUSIVE | DISCL_FOREGROUND);

	if (FAILED(hr)) {
		Printf ("Warning: Could not set mouse cooperative level\n");
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
		goto nextinit;
	}


	/*
	 *	Set the buffer size to DINPUT_BUFFERSIZE elements.
	 *	The buffer size is a DWORD property associated with the device.
	 */
	hr = IDirectInputDevice_SetProperty (g_pMouse, DIPROP_BUFFERSIZE, &dipdw.diph);

	if (FAILED(hr)) {
		Printf ("Could not set mouse buffer size\n");
		IDirectInputDevice_Release (g_pMouse);
		g_pMouse = NULL;
		goto nextinit;
	}

	IDirectInputDevice_Acquire (g_pMouse);

nextinit:
	DI_Init2();

	return TRUE;
}

/****************************************************************************
 *
 *		DITerm
 *
 *		Terminate our usage of DirectInput.
 *
 ****************************************************************************/

void DITerm(void)
{
	if (g_pKey) 		{ IDirectInputDevice_Release(g_pKey);		g_pKey		= NULL; }
	if (g_pMouse)		{ IDirectInputDevice_Release(g_pMouse); 	g_pMouse	= NULL; }
	if (g_hevtMouse)	{ CloseHandle(g_hevtMouse); 				g_hevtMouse = NULL; }
	if (g_pdi)			{ IDirectInput_Release(g_pdi);				g_pdi		= NULL; }
}

void UpdateCursorPosition(int dx, int dy)
{

	/*
	 *	Pick up any leftover fuzz from last time.  This is important
	 *	when scaling down mouse motions.  Otherwise, the user can
	 *	drag to the right extremely slow for the length of the table
	 *	and not get anywhere.
	 */

	MouseCurX += dx;
	MouseCurY -= dy;
	if (dx) GDx += dx;
	if (dy) GDy += dy;
	/* Clip the cursor to our client area */
	if (MouseCurX < 0)				MouseCurX = 0;
	if (MouseCurX >= SCREENWIDTH)	MouseCurX = SCREENWIDTH;

	if (MouseCurY < 0)				MouseCurY = 0;
	if (MouseCurY >= SCREENHEIGHT)	MouseCurY = SCREENHEIGHT;

}

void MouseRead (HWND hwnd) {
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
				UpdateCursorPosition(od.dwData, 0);
				break;
		/* DIMOFS_Y: Mouse vertical motion */
			case DIMOFS_Y:
				UpdateCursorPosition(0, od.dwData);
				break;

		/* Mouse button events now mimic keydown/up events */
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

	return;
}

/****** Stuff from Any Bay's mykey.c ******/
/******   (Most of it changed now)   ******/

/****************************************************************************
 *
 *		DIInit
 *
 *		Initialize the DirectInput variables.
 *
 ****************************************************************************/

BOOL DI_Init2(void)
{
	int hr;
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

	Printf ("DI_Init2: Initialize keyboard\n");

	BuildCvt2Table ();

	/*
	 *	Obtain an interface to the system key device.
	 */
	hr = IDirectInput_CreateDevice (g_pdi, &GUID_SysKeyboard, &g_pKey, NULL);

	if (FAILED(hr)) {
		I_FatalError ("Could not create keyboard device");
	}

	/*
	 *	Set the data format to "keyboard format".
	 */
	hr = IDirectInputDevice_SetDataFormat (g_pKey,&c_dfDIKeyboard);

	if (FAILED(hr)) {
		I_FatalError ("Could not set keyboard data format");
	}


	/*
	 *	Set the cooperative level.
	 */
	hr = IDirectInputDevice_SetCooperativeLevel(g_pKey, Window,
									   DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	if (FAILED(hr)) {
		I_FatalError("Could not set keyboard cooperative level");
	}

	/*
	 *	Set the buffer size to DINPUT_BUFFERSIZE elements.
	 *	The buffer size is a DWORD property associated with the device.
	 */
	hr = IDirectInputDevice_SetProperty (g_pKey, DIPROP_BUFFERSIZE, &dipdw.diph);

	if (FAILED(hr)) {
		I_FatalError ("Could not set keyboard buffer size");
	}

	IDirectInputDevice_Acquire (g_pKey);

	DI_InitJoy ();
	return TRUE;
}

void KeyRead() {
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

				event.data3 = Convert2[key];
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
						if (i_remapkeypad->value) {
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
					event.data2 = Convert[key];
					event.data3 = Convert2[key];
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

	while (PeekMessage (&mess, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage (&mess);
		DispatchMessage (&mess);
	}

	MouseRead (Window);
	KeyRead ();
	DI_JoyCheck ();
}


//
// I_StartTic
//
void I_StartTic (void)
{
	I_GetEvent ();
}
