/*
** i_input.cpp
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
#include <SDL.h>
#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_argv.h"
#include "v_video.h"

#include "d_main.h"
#include "d_event.h"
#include "d_gui.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "dikeys.h"
#include "events.h"
#include "g_game.h"
#include "g_levellocals.h"
#include "utf8.h"
#include "doomerrors.h"


static void I_CheckGUICapture ();
static void I_CheckNativeMouse ();

bool GUICapture;
static bool NativeMouse = true;

extern int paused;

CVAR (Bool,  use_mouse,				true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,  m_noprescale,			false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,	 m_filter,				false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)


extern int WaitingForKey, chatmodeon;
extern constate_e ConsoleState;

static const SDL_Keycode DIKToKeySym[256] =
{
	0, SDLK_ESCAPE, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6,
	SDLK_7, SDLK_8, SDLK_9, SDLK_0,SDLK_MINUS, SDLK_EQUALS, SDLK_BACKSPACE, SDLK_TAB,
	SDLK_q, SDLK_w, SDLK_e, SDLK_r, SDLK_t, SDLK_y, SDLK_u, SDLK_i,
	SDLK_o, SDLK_p, SDLK_LEFTBRACKET, SDLK_RIGHTBRACKET, SDLK_RETURN, SDLK_LCTRL, SDLK_a, SDLK_s,
	SDLK_d, SDLK_f, SDLK_g, SDLK_h, SDLK_j, SDLK_k, SDLK_l, SDLK_SEMICOLON,
	SDLK_QUOTE, SDLK_BACKQUOTE, SDLK_LSHIFT, SDLK_BACKSLASH, SDLK_z, SDLK_x, SDLK_c, SDLK_v,
	SDLK_b, SDLK_n, SDLK_m, SDLK_COMMA, SDLK_PERIOD, SDLK_SLASH, SDLK_RSHIFT, SDLK_KP_MULTIPLY,
	SDLK_LALT, SDLK_SPACE, SDLK_CAPSLOCK, SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5,
	SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_NUMLOCKCLEAR, SDLK_SCROLLLOCK, SDLK_KP_7,
	SDLK_KP_8, SDLK_KP_9, SDLK_KP_MINUS, SDLK_KP_4, SDLK_KP_5, SDLK_KP_6, SDLK_KP_PLUS, SDLK_KP_1,
	SDLK_KP_2, SDLK_KP_3, SDLK_KP_0, SDLK_KP_PERIOD, 0, 0, 0, SDLK_F11,
	SDLK_F12, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, SDLK_F13, SDLK_F14, SDLK_F15, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, SDLK_KP_EQUALS, 0, 0,
	0, SDLK_AT, SDLK_COLON, 0, 0, 0, 0, 0,
	0, 0, 0, 0, SDLK_KP_ENTER, SDLK_RCTRL, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, SDLK_KP_COMMA, 0, SDLK_KP_DIVIDE, 0, SDLK_SYSREQ,
	SDLK_RALT, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, SDLK_PAUSE, 0, SDLK_HOME,
	SDLK_UP, SDLK_PAGEUP, 0, SDLK_LEFT, 0, SDLK_RIGHT, 0, SDLK_END,
	SDLK_DOWN, SDLK_PAGEDOWN, SDLK_INSERT, SDLK_DELETE, 0, 0, 0, 0,
	0, 0, 0, SDLK_LGUI, SDLK_RGUI, SDLK_MENU, SDLK_POWER, SDLK_SLEEP,
	0, 0, 0, 0, 0, SDLK_AC_SEARCH, SDLK_AC_BOOKMARKS, SDLK_AC_REFRESH,
	SDLK_AC_STOP, SDLK_AC_FORWARD, SDLK_AC_BACK, SDLK_COMPUTER, SDLK_MAIL, SDLK_MEDIASELECT, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static const SDL_Scancode DIKToKeyScan[256] =
{
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6,
	SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0 ,SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_TAB,
	SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I,
	SDL_SCANCODE_O, SDL_SCANCODE_P, SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_RETURN, SDL_SCANCODE_LCTRL, SDL_SCANCODE_A, SDL_SCANCODE_S,
	SDL_SCANCODE_D, SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON,
	SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_GRAVE, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V,
	SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_KP_MULTIPLY,
	SDL_SCANCODE_LALT, SDL_SCANCODE_SPACE, SDL_SCANCODE_CAPSLOCK, SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5,
	SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_NUMLOCKCLEAR, SDL_SCANCODE_SCROLLLOCK, SDL_SCANCODE_KP_7,
	SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_MINUS, SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_KP_1,
	SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_F11,
	SDL_SCANCODE_F12, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_F13, SDL_SCANCODE_F14, SDL_SCANCODE_F15, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_KP_EQUALS, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_RCTRL, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_KP_COMMA, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_KP_DIVIDE, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_SYSREQ,
	SDL_SCANCODE_RALT, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_PAUSE, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_HOME,
	SDL_SCANCODE_UP, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_END,
	SDL_SCANCODE_DOWN, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_INSERT, SDL_SCANCODE_DELETE, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI, SDL_SCANCODE_MENU, SDL_SCANCODE_POWER, SDL_SCANCODE_SLEEP,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_AC_SEARCH, SDL_SCANCODE_AC_BOOKMARKS, SDL_SCANCODE_AC_REFRESH,
	SDL_SCANCODE_AC_STOP, SDL_SCANCODE_AC_FORWARD, SDL_SCANCODE_AC_BACK, SDL_SCANCODE_COMPUTER, SDL_SCANCODE_MAIL, SDL_SCANCODE_MEDIASELECT, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN,
	SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN
};

static TMap<SDL_Keycode, uint8_t> InitKeySymMap ()
{
	TMap<SDL_Keycode, uint8_t> KeySymToDIK;

	for (int i = 0; i < 256; ++i)
	{
		KeySymToDIK[DIKToKeySym[i]] = i;
	}
	KeySymToDIK[0] = 0;
	KeySymToDIK[SDLK_RSHIFT] = DIK_LSHIFT;
	KeySymToDIK[SDLK_RCTRL] = DIK_LCONTROL;
	KeySymToDIK[SDLK_RALT] = DIK_LMENU;
	// Depending on your Linux flavor, you may get SDLK_PRINT or SDLK_SYSREQ
	KeySymToDIK[SDLK_PRINTSCREEN] = DIK_SYSRQ;

	return KeySymToDIK;
}
static const TMap<SDL_Keycode, uint8_t> KeySymToDIK(InitKeySymMap());

static TMap<SDL_Scancode, uint8_t> InitKeyScanMap ()
{
	TMap<SDL_Scancode, uint8_t> KeyScanToDIK;

	for (int i = 0; i < 256; ++i)
	{
		KeyScanToDIK[DIKToKeyScan[i]] = i;
	}

	return KeyScanToDIK;
}
static const TMap<SDL_Scancode, uint8_t> KeyScanToDIK(InitKeyScanMap());

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

	// [ZZ] check active event handlers that want the UI processing
	if (!wantCapt && primaryLevel->localEventManager->CheckUiProcessors())
		wantCapt = true;

	if (wantCapt != GUICapture)
	{
		GUICapture = wantCapt;
		ResetButtonStates();
	}
}

void I_SetMouseCapture()
{
	// Clear out any mouse movement.
	SDL_GetRelativeMouseState (NULL, NULL);
	SDL_SetRelativeMouseMode (SDL_TRUE);
}

void I_ReleaseMouseCapture()
{
	SDL_SetRelativeMouseMode (SDL_FALSE);
}

static void PostMouseMove (int x, int y)
{
	static int lastx = 0, lasty = 0;
	event_t ev = { 0,0,0,0,0,0,0 };
	
	if (m_filter)
	{
		ev.x = (x + lastx) / 2;
		ev.y = (y + lasty) / 2;
	}
	else
	{
		ev.x = x;
		ev.y = y;
	}
	lastx = x;
	lasty = y;
	if (ev.x | ev.y)
	{
		ev.type = EV_Mouse;
		D_PostEvent (&ev);
	}
}

static void MouseRead ()
{
	int x, y;

	if (NativeMouse)
	{
		return;
	}

	SDL_GetRelativeMouseState (&x, &y);
	if (!m_noprescale)
	{
		x *= 3;
		y *= 2;
	}
	if (x | y)
	{
		PostMouseMove (x, -y);
	}
}

CUSTOM_CVAR(Int, mouse_capturemode, 1, CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
{
	if (self < 0) self = 0;
	else if (self > 2) self = 2;
}

static bool inGame()
{
	switch (mouse_capturemode)
	{
	default:
	case 0:
		return gamestate == GS_LEVEL;
	case 1:
		return gamestate == GS_LEVEL || gamestate == GS_INTERMISSION || gamestate == GS_FINALE;
	case 2:
		return true;
	}
}

static void I_CheckNativeMouse ()
{
	bool focus = SDL_GetKeyboardFocus() != NULL;
	bool fs = screen->IsFullscreen();
	
	bool wantNative = !focus || (!use_mouse || GUICapture || paused || demoplayback || !inGame());

	if (wantNative != NativeMouse)
	{
		NativeMouse = wantNative;
		SDL_ShowCursor (wantNative);
		if (wantNative)
			I_ReleaseMouseCapture ();
		else
			I_SetMouseCapture ();
	}
}

void MessagePump (const SDL_Event &sev)
{
	static int lastx = 0, lasty = 0;
	int x, y;
	event_t event = { 0,0,0,0,0,0,0 };
	
	switch (sev.type)
	{
	case SDL_QUIT:
		throw CExitEvent(0);

	case SDL_WINDOWEVENT:
		extern void ProcessSDLWindowEvent(const SDL_WindowEvent &);
		ProcessSDLWindowEvent(sev.window);
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if (!GUICapture)
		{
			event.type = sev.type == SDL_MOUSEBUTTONDOWN ? EV_KeyDown : EV_KeyUp;

			switch (sev.button.button)
			{
			case SDL_BUTTON_LEFT:	event.data1 = KEY_MOUSE1;		break;
			case SDL_BUTTON_MIDDLE:	event.data1 = KEY_MOUSE3;		break;
			case SDL_BUTTON_RIGHT:	event.data1 = KEY_MOUSE2;		break;
			case SDL_BUTTON_X1:		event.data1 = KEY_MOUSE4;		break;
			case SDL_BUTTON_X2:		event.data1 = KEY_MOUSE5;		break;
			case 6:		event.data1 = KEY_MOUSE6;		break;
			case 7:		event.data1 = KEY_MOUSE7;		break;
			case 8:		event.data1 = KEY_MOUSE8;		break;
			default:	printf("SDL mouse button %s %d\n",
				sev.type == SDL_MOUSEBUTTONDOWN ? "down" : "up", sev.button.button);	break;
			}

			if (event.data1 != 0)
			{
				D_PostEvent(&event);
			}
		}
		else if ((sev.button.button >= SDL_BUTTON_LEFT && sev.button.button <= SDL_BUTTON_X2))
		{
			int x, y;
			SDL_GetMouseState(&x, &y);

			event.type = EV_GUI_Event;
			event.data1 = x;
			event.data2 = y;

			screen->ScaleCoordsFromWindow(event.data1, event.data2);

			if (sev.type == SDL_MOUSEBUTTONDOWN)
			{
				switch(sev.button.button)
				{
				case SDL_BUTTON_LEFT:   event.subtype = EV_GUI_LButtonDown;    break;
				case SDL_BUTTON_MIDDLE: event.subtype = EV_GUI_MButtonDown;    break;
				case SDL_BUTTON_RIGHT:  event.subtype = EV_GUI_RButtonDown;    break;
				case SDL_BUTTON_X1:     event.subtype = EV_GUI_BackButtonDown; break;
				case SDL_BUTTON_X2:     event.subtype = EV_GUI_FwdButtonDown;  break;
				default: assert(false); event.subtype = EV_GUI_None;           break;
				}
			}
			else
			{
				switch(sev.button.button)
				{
				case SDL_BUTTON_LEFT:   event.subtype = EV_GUI_LButtonUp;    break;
				case SDL_BUTTON_MIDDLE: event.subtype = EV_GUI_MButtonUp;    break;
				case SDL_BUTTON_RIGHT:  event.subtype = EV_GUI_RButtonUp;    break;
				case SDL_BUTTON_X1:     event.subtype = EV_GUI_BackButtonUp; break;
				case SDL_BUTTON_X2:     event.subtype = EV_GUI_FwdButtonUp;  break;
				default: assert(false); event.subtype = EV_GUI_None;         break;
				}
			}

			SDL_Keymod kmod = SDL_GetModState();
			event.data3 = ((kmod & KMOD_SHIFT) ? GKM_SHIFT : 0) |
				((kmod & KMOD_CTRL) ? GKM_CTRL : 0) |
				((kmod & KMOD_ALT) ? GKM_ALT : 0);

			D_PostEvent(&event);
		}
		break;

	case SDL_MOUSEMOTION:
		if (GUICapture)
		{
			event.data1 = sev.motion.x;
			event.data2 = sev.motion.y;

			screen->ScaleCoordsFromWindow(event.data1, event.data2);

			event.type = EV_GUI_Event;
			event.subtype = EV_GUI_MouseMove;

			SDL_Keymod kmod = SDL_GetModState();
			event.data3 = ((kmod & KMOD_SHIFT) ? GKM_SHIFT : 0) |
			              ((kmod & KMOD_CTRL) ? GKM_CTRL : 0) |
			              ((kmod & KMOD_ALT) ? GKM_ALT : 0);

			D_PostEvent(&event);
		}
		break;

	case SDL_MOUSEWHEEL:
		if (GUICapture)
		{
			event.type = EV_GUI_Event;

			if (sev.wheel.y == 0)
				event.subtype = sev.wheel.x > 0 ? EV_GUI_WheelRight : EV_GUI_WheelLeft;
			else
				event.subtype = sev.wheel.y > 0 ? EV_GUI_WheelUp : EV_GUI_WheelDown;

			SDL_Keymod kmod = SDL_GetModState();
			event.data3 = ((kmod & KMOD_SHIFT) ? GKM_SHIFT : 0) |
				((kmod & KMOD_CTRL) ? GKM_CTRL : 0) |
				((kmod & KMOD_ALT) ? GKM_ALT : 0);

			D_PostEvent (&event);
		}
		else
		{
			event.type = EV_KeyDown;

			if (sev.wheel.y != 0)
				event.data1 = sev.wheel.y > 0 ? KEY_MWHEELUP : KEY_MWHEELDOWN;
			else
				event.data1 = sev.wheel.x > 0 ? KEY_MWHEELRIGHT : KEY_MWHEELLEFT;

			D_PostEvent (&event);
			event.type = EV_KeyUp;
			D_PostEvent (&event);
		}
		break;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		if (!GUICapture)
		{
			if (sev.key.repeat)
			{
				break;
			}
			
			event.type = sev.type == SDL_KEYDOWN ? EV_KeyDown : EV_KeyUp;

			// Try to look up our key mapped key for conversion to DirectInput.
			// If that fails, then we'll do a lookup against the scan code,
			// which may not return the right key, but at least the key should
			// work in the game.
			if (const uint8_t *dik = KeySymToDIK.CheckKey (sev.key.keysym.sym))
				event.data1 = *dik;
			else if (const uint8_t *dik = KeyScanToDIK.CheckKey (sev.key.keysym.scancode))
				event.data1 = *dik;

			if (event.data1)
			{
				if (sev.key.keysym.sym < 256)
				{
					event.data2 = sev.key.keysym.sym;
				}
				D_PostEvent (&event);
			}
		}
		else
		{
			event.type = EV_GUI_Event;
			event.subtype = sev.type == SDL_KEYDOWN ? EV_GUI_KeyDown : EV_GUI_KeyUp;
			SDL_Keymod kmod = SDL_GetModState();
			event.data3 = ((kmod & KMOD_SHIFT) ? GKM_SHIFT : 0) |
				((kmod & KMOD_CTRL) ? GKM_CTRL : 0) |
				((kmod & KMOD_ALT) ? GKM_ALT : 0);

			if (event.subtype == EV_GUI_KeyDown && sev.key.repeat)
			{
				event.subtype = EV_GUI_KeyRepeat;
			}

			switch (sev.key.keysym.sym)
			{
			case SDLK_KP_ENTER:	event.data1 = GK_RETURN;	break;
			case SDLK_PAGEUP:	event.data1 = GK_PGUP;		break;
			case SDLK_PAGEDOWN:	event.data1 = GK_PGDN;		break;
			case SDLK_END:		event.data1 = GK_END;		break;
			case SDLK_HOME:		event.data1 = GK_HOME;		break;
			case SDLK_LEFT:		event.data1 = GK_LEFT;		break;
			case SDLK_RIGHT:	event.data1 = GK_RIGHT;		break;
			case SDLK_UP:		event.data1 = GK_UP;		break;
			case SDLK_DOWN:		event.data1 = GK_DOWN;		break;
			case SDLK_DELETE:	event.data1 = GK_DEL;		break;
			case SDLK_ESCAPE:	event.data1 = GK_ESCAPE;	break;
			case SDLK_F1:		event.data1 = GK_F1;		break;
			case SDLK_F2:		event.data1 = GK_F2;		break;
			case SDLK_F3:		event.data1 = GK_F3;		break;
			case SDLK_F4:		event.data1 = GK_F4;		break;
			case SDLK_F5:		event.data1 = GK_F5;		break;
			case SDLK_F6:		event.data1 = GK_F6;		break;
			case SDLK_F7:		event.data1 = GK_F7;		break;
			case SDLK_F8:		event.data1 = GK_F8;		break;
			case SDLK_F9:		event.data1 = GK_F9;		break;
			case SDLK_F10:		event.data1 = GK_F10;		break;
			case SDLK_F11:		event.data1 = GK_F11;		break;
			case SDLK_F12:		event.data1 = GK_F12;		break;
			default:
				if (sev.key.keysym.sym < 256)
				{
					event.data1 = sev.key.keysym.sym;
				}
				break;
			}
			if (event.data1 < 128)
			{
				event.data1 = toupper(event.data1);
				D_PostEvent (&event);
			}
		}
		break;

	case SDL_TEXTINPUT:
		if (GUICapture)
		{
			int size;
			
			int unichar = utf8_decode((const uint8_t*)sev.text.text, &size);
			if (size != 4)
			{
				event.type = EV_GUI_Event;
				event.subtype = EV_GUI_Char;
				event.data1 = (int16_t)unichar;
				event.data2 = !!(SDL_GetModState() & KMOD_ALT);
				D_PostEvent (&event);
			}
		}
		break;

	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		if (!GUICapture)
		{
			event.type = sev.type == SDL_JOYBUTTONDOWN ? EV_KeyDown : EV_KeyUp;
			event.data1 = KEY_FIRSTJOYBUTTON + sev.jbutton.button;
			if(event.data1 != 0)
				D_PostEvent(&event);
		}
		break;
	}
}

void I_GetEvent ()
{
	SDL_Event sev;
	
	while (SDL_PollEvent (&sev))
	{
		MessagePump (sev);
	}
	if (use_mouse)
	{
		MouseRead ();
	}
}

void I_StartTic ()
{
	I_CheckGUICapture ();
	I_CheckNativeMouse ();
	I_GetEvent ();
}

void I_ProcessJoysticks ();
void I_StartFrame ()
{
	I_ProcessJoysticks();
}
