#include <SDL.h>
#include <ctype.h>
#include "doomtype.h"
#include "c_dispatch.h"
#include "doomdef.h"
#include "doomstat.h"
#include "m_argv.h"
#include "i_input.h"
#include "v_video.h"

#include "d_main.h"
#include "d_event.h"
#include "d_gui.h"
#include "c_console.h"
#include "c_cvars.h"
#include "i_system.h"
#include "dikeys.h"
#include "templates.h"
#include "s_sound.h"

static void I_CheckGUICapture ();
static void I_CheckNativeMouse ();

bool GUICapture;

extern int paused;

CVAR (Bool,  use_mouse,				true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,  m_noprescale,			false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,	 m_filter,				false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool,  sdl_nokeyrepeat,		false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

float JoyAxes[6];
//static int JoyActive;
//static BYTE JoyButtons[128];
//static BYTE JoyPOV[4];
static BYTE JoyAxisMap[8];
static float JoyAxisThresholds[8];
char *JoyAxisNames[8];
static const BYTE POVButtons[9] = { 0x01, 0x03, 0x02, 0x06, 0x04, 0x0C, 0x08, 0x09, 0x00 };

TArray<GUIDName> JoystickNames;
CVAR (Bool,  use_joystick,	false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (GUID, joy_guid,		NULL, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOINITCALL)

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

static FBaseCVar * const JoyConfigVars[] =
{
	&joy_xaxis, &joy_yaxis, &joy_zaxis, &joy_xrot, &joy_yrot, &joy_zrot, &joy_slider, &joy_dial,
	&joy_xthreshold, &joy_ythreshold, &joy_zthreshold, &joy_xrotthreshold, &joy_yrotthreshold, &joy_zrotthreshold, &joy_sliderthreshold, &joy_dialthreshold,
	&joy_speedmultiplier, &joy_yawspeed, &joy_pitchspeed, &joy_forwardspeed, &joy_sidespeed,
	&joy_upspeed
};

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
	// Depending on your Linux flavor, you may get SDLK_PRINT or SDLK_SYSREQ
	KeySymToDIK[SDLK_PRINT] = DIK_SYSRQ;
}

static void I_CheckGUICapture ()
{
	bool wantCapt;
	bool repeat;
	int oldrepeat, interval;

	SDL_GetKeyRepeat(&oldrepeat, &interval);

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
		if (wantCapt)
		{
			FlushDIKState ();
			memset (DownState, 0, sizeof(DownState));
			repeat = !sdl_nokeyrepeat;
			SDL_EnableUNICODE (1);
		}
		else
		{
			repeat = false;
			SDL_EnableUNICODE (0);
		}
	}
	if (wantCapt)
	{
		repeat = !sdl_nokeyrepeat;
	}
	else
	{
		repeat = false;
	}
	if (repeat != (oldrepeat != 0))
	{
		if (repeat)
		{
			SDL_EnableKeyRepeat (SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
		}
		else
		{
			SDL_EnableKeyRepeat (0, 0);
		}
	}
}

static void CenterMouse ()
{
	SDL_WarpMouse (screen->GetWidth()/2, screen->GetHeight()/2);
	SDL_PumpEvents ();
	SDL_GetRelativeMouseState (NULL, NULL);
}

static void PostMouseMove (int x, int y)
{
	static int lastx = 0, lasty = 0;
	event_t ev = { 0 };
	
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
		CenterMouse ();
		PostMouseMove (x, -y);
	}
}

static void WheelMoved(event_t *event)
{
	if (GUICapture)
	{
		if (event->type != EV_KeyUp)
		{
			SDLMod mod = SDL_GetModState();
			event->type = EV_GUI_Event;
			event->subtype = event->data1 == KEY_MWHEELUP ? EV_GUI_WheelUp : EV_GUI_WheelDown;
			event->data1 = 0;
			event->data3 = ((mod & KMOD_SHIFT) ? GKM_SHIFT : 0) |
						  ((mod & KMOD_CTRL) ? GKM_CTRL : 0) |
						  ((mod & KMOD_ALT) ? GKM_ALT : 0);
			D_PostEvent(event);
		}
	}
	else
	{
		D_PostEvent(event);
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
	bool focus = (SDL_GetAppState() & (SDL_APPINPUTFOCUS|SDL_APPACTIVE))
			== (SDL_APPINPUTFOCUS|SDL_APPACTIVE);
	bool fs = (SDL_GetVideoSurface ()->flags & SDL_FULLSCREEN) != 0;
	
	bool wantNative = !focus || !use_mouse || (!fs && (GUICapture || paused || demoplayback || !inGame()));

	if (wantNative != NativeMouse)
	{
		NativeMouse = wantNative;
		if (wantNative)
		{
			SDL_ShowCursor (1);
			SDL_WM_GrabInput (SDL_GRAB_OFF);
			FlushDIKState (KEY_MOUSE1, KEY_MOUSE8);
		}
		else
		{
			SDL_ShowCursor (0);
			SDL_WM_GrabInput (SDL_GRAB_ON);
			CenterMouse ();
		}
	}
}

void MessagePump (const SDL_Event &sev)
{
	static int lastx = 0, lasty = 0;
	int x, y;
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
			}
			S_SetSoundPaused(sev.active.gain);
		}
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if (!GUICapture || sev.button.button == 4 || sev.button.button == 5)
		{
			event.type = sev.type == SDL_MOUSEBUTTONDOWN ? EV_KeyDown : EV_KeyUp;
			/* These button mappings work with my Gentoo system using the
			 * evdev driver and a Logitech MX510 mouse. Whether or not they
			 * carry over to other Linux systems, I have no idea, but I sure
			 * hope so. (Though buttons 11 and 12 are kind of useless, since
			 * they also trigger buttons 4 and 5.)
			 */
			switch (sev.button.button)
			{
			case 1:		event.data1 = KEY_MOUSE1;		break;
			case 2:		event.data1 = KEY_MOUSE3;		break;
			case 3:		event.data1 = KEY_MOUSE2;		break;
			case 4:		event.data1 = KEY_MWHEELUP;		break;
			case 5:		event.data1 = KEY_MWHEELDOWN;	break;
			case 6:		event.data1 = KEY_MOUSE4;		break;	/* dunno; not generated by my mouse */
			case 7:		event.data1 = KEY_MOUSE5;		break;	/* ditto */
			case 8:		event.data1 = KEY_MOUSE4;		break;
			case 9:		event.data1 = KEY_MOUSE5;		break;
			case 10:	event.data1 = KEY_MOUSE6;		break;
			case 11:	event.data1 = KEY_MOUSE7;		break;
			case 12:	event.data1 = KEY_MOUSE8;		break;
			default:	printf("SDL mouse button %s %d\n",
				sev.type == SDL_MOUSEBUTTONDOWN ? "down" : "up", sev.button.button);	break;
			}
			if (event.data1 != 0)
			{
				//DIKState[ActiveDIKState][event.data1] = (event.type == EV_KeyDown);
				if (event.data1 == KEY_MWHEELUP || event.data1 == KEY_MWHEELDOWN)
				{
					WheelMoved(&event);
				}
				else
				{
					D_PostEvent(&event);
				}
			}
		}
		else if (sev.button.button >= 1 && sev.button.button <= 3)
		{
			event.type = EV_GUI_Event;
			event.subtype = sev.type == SDL_MOUSEBUTTONDOWN ? EV_GUI_LButtonDown : EV_GUI_LButtonUp;
			event.subtype += (sev.button.button - 1) * 3;
			D_PostEvent(&event);
		}
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
				event.data1 = toupper(event.data1);
				D_PostEvent (&event);
			}
			if (!iscntrl(event.data2) && event.subtype != EV_GUI_KeyUp)
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

void I_StartFrame ()
{
	if (KeySymToDIK[SDLK_BACKSPACE] == 0)
	{
		InitKeySymMap ();
	}
}
