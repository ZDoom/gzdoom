#include <SDL/SDL.h>
#include "doomtype.h"
#include "c_dispatch.h"
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
#include "dikeys.h"

#include "s_sound.h"

static void I_CheckGUICapture ();
static void I_CheckNativeMouse ();

static bool GUICapture;
static bool NativeMouse = true;

extern BOOL paused;

CVAR (Bool,  i_remapkeypad,			true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,  use_mouse,				true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR (Bool,  use_joystick,			false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_speedmultiplier,	1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_xsensitivity,		1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_ysensitivity,		-1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_xthreshold,		0.15f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Float, joy_ythreshold,		0.15f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

EXTERN_CVAR (Bool, fullscreen)

extern int WaitingForKey, chatmodeon;
extern constate_e ConsoleState;

static BYTE KeySymToDIK[SDLK_LAST], DownState[SDLK_LAST];

static WORD DIKToKeySym[256] =
{
	0, SDLK_ESCAPE, '1', '2', '3', '4', '5', '6',
	'7', '8', '9', '0', '-', '=', SDLK_BACKSPACE, SDLK_TAB,
	'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', SDLK_RETURN, SDLK_LCTRL, 'a', 's',
	'd', 'f', 'g', 'h', 'j', 'k', 'l', SDLK_SEMICOLON,
	'\'', '`', SDLK_LSHIFT, '\\', 'z', 'x', 'c', 'v',
	'b', 'n', 'm', ',', '.', '/', SDLK_RSHIFT, SDLK_KP_MULTIPLY,
	SDLK_LALT, ' ', SDLK_CAPSLOCK, SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5,
	SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_NUMLOCK, SDLK_SCROLLOCK, SDLK_KP7,
	SDLK_KP8, SDLK_KP9, SDLK_KP_MINUS, SDLK_KP4, SDLK_KP5, SDLK_KP6, SDLK_KP_PLUS, SDLK_KP1,
	SDLK_KP2, SDLK_KP3, SDLK_KP0, SDLK_KP_PERIOD, 0, 0, 0, SDLK_F11,
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
	0, 0, 0, 0, 0, SDLK_KP_DIVIDE, 0, SDLK_SYSREQ,
	SDLK_RALT, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, SDLK_HOME,
	SDLK_UP, SDLK_PAGEUP, 0, SDLK_LEFT, 0, SDLK_RIGHT, 0, SDLK_END,
	SDLK_DOWN, SDLK_PAGEDOWN, SDLK_INSERT, SDLK_DELETE, 0, 0, 0, 0,
	0, 0, 0, SDLK_LSUPER, SDLK_RSUPER, SDLK_MENU, SDLK_POWER, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, SDLK_PAUSE
};

static void FlushDIKState (int low=0, int high=NUM_KEYS-1)
{
}

static void InitKeySymMap ()
{
	for (int i = 0; i < 256; ++i)
	{
		KeySymToDIK[DIKToKeySym[i]] = i;
	}
	KeySymToDIK[0] = 0;
	KeySymToDIK[SDLK_RSHIFT] = DIK_LSHIFT;
	KeySymToDIK[SDLK_RCTRL] = DIK_LCONTROL;
	KeySymToDIK[SDLK_RALT] = DIK_LMENU;
}

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
			memset (DownState, 0, sizeof(DownState));
			SDL_EnableKeyRepeat (SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
			SDL_EnableUNICODE (1);
		}
		else
		{
			SDL_EnableKeyRepeat (0, 0);
			SDL_EnableUNICODE (0);
		}
	}
}

static void I_CheckNativeMouse ()
{
	bool focus = (SDL_GetAppState() & (SDL_APPINPUTFOCUS|SDL_APPACTIVE))
			== (SDL_APPINPUTFOCUS|SDL_APPACTIVE);
	bool fs = (SDL_GetVideoSurface ()->flags & SDL_FULLSCREEN) != 0;
	
	bool wantNative = !focus || (!fs && (GUICapture || paused));

	if (wantNative != NativeMouse)
	{
		NativeMouse = wantNative;
		if (wantNative)
		{
			SDL_ShowCursor (1);
			SDL_WM_GrabInput (SDL_GRAB_OFF);
			FlushDIKState (KEY_MOUSE1, KEY_MOUSE4);
		}
		else
		{
			SDL_ShowCursor (0);
			SDL_WM_GrabInput (SDL_GRAB_ON);
		}
	}
}

void MessagePump (const SDL_Event &sev)
{
	event_t event = { 0, };
	
	switch (sev.type)
	{
	case SDL_QUIT:
		exit (0);

	case SDL_ACTIVEEVENT:
		if (sev.active.state == SDL_APPINPUTFOCUS)
		{
			if (sev.active.gain == 0)
			{ // kill focus
				FlushDIKState ();
				if (!paused)
					S_PauseSound ();
			}
			else
			{ // set focus
				if (!paused)
					S_ResumeSound ();
			}
		}
		break;

	case SDL_MOUSEMOTION:
		event.x = sev.motion.xrel << 2;
		event.y = -sev.motion.yrel << 1;
		event.type = EV_Mouse;
		D_PostEvent (&event);
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		event.type = sev.type == SDL_MOUSEBUTTONDOWN ? EV_KeyDown : EV_KeyUp;
		switch (sev.button.button)
		{
		case 1:		event.data1 = KEY_MOUSE1;		break;
		case 2:		event.data1 = KEY_MOUSE3;		break;
		case 3:		event.data1 = KEY_MOUSE2;		break;
		case 4:		event.data1 = KEY_MWHEELUP;		break;
		case 5:		event.data1 = KEY_MWHEELDOWN;	break;
		case 6:		event.data1 = KEY_MOUSE4;		break;	/* dunno */
		}
		//DIKState[ActiveDIKState][event.data1] = (event.type == EV_KeyDown);
		D_PostEvent (&event);
		break;
		
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		if (sev.key.keysym.sym >= SDLK_LAST)
			break;

		if (!GUICapture)
		{
			event.type = sev.type == SDL_KEYDOWN ? EV_KeyDown : EV_KeyUp;
			event.data1 = KeySymToDIK[sev.key.keysym.sym];
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
			event.data3 = ((sev.key.keysym.mod & KMOD_SHIFT) ? GKM_SHIFT : 0) |
						  ((sev.key.keysym.mod & KMOD_CTRL) ? GKM_CTRL : 0) |
						  ((sev.key.keysym.mod & KMOD_ALT) ? GKM_ALT : 0);

			if (sev.key.keysym.sym < SDLK_LAST)
			{
				if (event.subtype == EV_GUI_KeyDown)
				{
					if (DownState[sev.key.keysym.sym])
					{
						event.subtype = EV_GUI_KeyRepeat;
					}
					DownState[sev.key.keysym.sym] = 1;
				}
				else
				{
					DownState[sev.key.keysym.sym] = 0;
				}
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
			event.data2 = sev.key.keysym.unicode & 0xff;
			if (event.data1 < 128)
			{
				D_PostEvent (&event);
			}
			if (event.data2 >= 32 && event.subtype != EV_GUI_KeyUp)
			{
				event.subtype = EV_GUI_Char;
				event.data1 = event.data2;
				event.data2 = sev.key.keysym.mod & KMOD_ALT;
				event.data3 = 0;
				D_PostEvent (&event);
			}
		}
	}
}

void I_GetEvent ()
{
	SDL_Event sev;
	
	while (SDL_PollEvent (&sev))
	{
		MessagePump (sev);
	}
}

void I_StartTic ()
{
	I_CheckGUICapture ();
	I_CheckNativeMouse ();
	I_GetEvent ();
}

void I_StartFrame ()
{
	if (KeySymToDIK[SDLK_BACKSPACE] == 0)
	{
		InitKeySymMap ();
	}
}
