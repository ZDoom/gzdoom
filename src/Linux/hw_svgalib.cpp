#define HARDWARELIB
#include "hardware.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <vga.h>
#include <vgakeyboard.h>
#include <vgamouse.h>
extern "C" {
#include <vgajoystick.h>
}

#ifdef LINUX
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <errno.h>
#include <sys/ioctl.h>
#endif

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "c_cvars.h"
#include "c_console.h"
#include "d_main.h"

#include "i_system.h"

static bool InitedSVGA = false;
static int globalres;

// KEYBOARD -----------------------------------------------------------------

class SVGAKeyboard : public IKeyboard
{
public:
    SVGAKeyboard ();
    ~SVGAKeyboard ();
    void ProcessInput (bool consoleOpen);
	void SetKeypadRemapping (bool remap);

private:
	static bool m_RemapKeypad;
    static bool m_AltDown;
    static bool m_ShiftDown;
    static bool m_CtrlDown;
    static bool m_Inited;
	static bool m_ConsoleOpen;

    static const byte m_ToQWERTY[128];
    static const byte m_ToShiftedQWERTY[128];

#ifdef LINUX
	static void InitTranslations ();
	static void BuildKeymap (int fd, int map, int linuxmap);
	static void QWERTYTranslation ();
	static byte m_Translations[8][128];
#endif

    static void MunchKeys (int scancode, int press);

	enum
	{
		SHIFT_MASK = 1,
		CTRL_MASK = 2,
		ALT_MASK = 4
	};
};

bool SVGAKeyboard::m_RemapKeypad;
bool SVGAKeyboard::m_AltDown;
bool SVGAKeyboard::m_ShiftDown;
bool SVGAKeyboard::m_CtrlDown;
bool SVGAKeyboard::m_Inited;
bool SVGAKeyboard::m_ConsoleOpen;

const byte SVGAKeyboard::m_ToQWERTY[128] =
{
//  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',   8,   9,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',  13,   0, 'a', 's',
  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';','\'', '`',   0,'\\', 'z', 'x', 'c', 'v',
  'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
  '2', '3', '0', '.',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   13,   0, '/',
};

const byte SVGAKeyboard::m_ToShiftedQWERTY[128] =
{
//  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',   8,   9,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',  13,   0, 'A', 'S',
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ':','\"', '~',   0, '|', 'Z', 'X', 'C', 'V',
  'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' ',   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
  '2', '3', '0', '.',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   13,   0, '/',
};

#ifdef LINUX
#define L_SHIFT	(1<<KG_SHIFT)
#define L_ALT	(1<<KG_ALT)
#define L_CTRL	(1<<KG_CTRL)

byte SVGAKeyboard::m_Translations[8][128];

void SVGAKeyboard::InitTranslations ()
{
	int fd = get_console_fd (NULL);

	if (fd == -1)
		goto DefaultMap;
	memset (m_Translations, 0, sizeof(m_Translations));
	BuildKeymap (fd, 0, 0);
	BuildKeymap (fd, SHIFT_MASK, L_SHIFT);
	BuildKeymap (fd, CTRL_MASK, L_CTRL);
	BuildKeymap (fd, ALT_MASK, L_ALT);
	BuildKeymap (fd, SHIFT_MASK|CTRL_MASK, L_SHIFT|L_CTRL);
	BuildKeymap (fd, SHIFT_MASK|ALT_MASK, L_SHIFT|L_ALT);
	BuildKeymap (fd, CTRL_MASK|ALT_MASK, L_CTRL|L_ALT);
	BuildKeymap (fd, SHIFT_MASK|CTRL_MASK|ALT_MASK, L_SHIFT|L_CTRL|L_ALT);

	// Make sure we actually have something in the standard keymap (this
	// should always happen). If not, use a QWERTY map.
	if (m_Translations[0][1])
		return;

 DefaultMap:
	QWERTYTranslation ();
}

void SVGAKeyboard::BuildKeymap (int fd, int map, int linuxmap)
{
	struct kbentry ke;
	int i, j;
	byte *table = &m_Translations[map][0];

	ke.kb_index = 0;
	ke.kb_table = linuxmap;
	j = ioctl (fd, KDGKBENT, (unsigned long)&ke);
	if (j && errno != EINVAL)
		return;		// This keymap isn't defined

	for (i = 1; i < NR_KEYS; i++)
	{
		ke.kb_index = i;
		if (ioctl (fd, KDGKBENT, (unsigned long)&ke))
			continue;
		
		switch (KTYP(ke.kb_value))
		{
		case KT_LATIN:
		case KT_LETTER:
		case KT_ASCII:
			table[i] = KVAL(ke.kb_value);
			break;

		case KT_SPEC:
			switch (KVAL(ke.kb_value))
			{
			case 1:
				table[i] = '\r';
				break;
			default:
				table[i] = 0;
			}
			break;

		case KT_PAD:
			switch (KVAL(ke.kb_value))
			{
			case 0: case 1: case 2: case 3: case 4:
			case 5: case 6: case 7: case 8: case 9:
				table[i] = KVAL(ke.kb_value) + '0';
				break;
			case 10:
				table[i] = '+';
				break;
			case 11:
				table[i] = '-';
				break;
			case 12:
				table[i] = '*';
				break;
			case 13:
				table[i] = '/';
				break;
			case 14:
				table[i] = '\r';
				break;
			case 16:
				table[i] = '.';
				break;
			default:
				table[i] = 0;
			}
		default:
			table[i] = 0;
		}
	}
}

void SVGAKeyboard::QWERTYTranslation ()
{
	memset (m_Translations, 0, sizeof(m_Translations));
	memcpy (m_Translations[0], m_ToQWERTY, 128);
	memcpy (m_Translations[SHIFT_MASK], m_ToShiftedQWERTY, 128);
}
#endif

SVGAKeyboard::SVGAKeyboard ()
{
    if (keyboard_init () < 0)
		I_FatalError ("Failed to initialize keyboard\n");

#ifdef LINUX
	InitTranslations ();
#endif

    keyboard_seteventhandler (MunchKeys);
    m_Inited = true;
}

SVGAKeyboard::~SVGAKeyboard ()
{
    if (m_Inited)
    {
		keyboard_close ();
		m_Inited = false;
    }
}

void SVGAKeyboard::SetKeypadRemapping (bool remap)
{
	m_RemapKeypad = remap;
}

void SVGAKeyboard::ProcessInput (bool consoleOpen)
{
	m_ConsoleOpen = consoleOpen;
    keyboard_update ();
}

void SVGAKeyboard::MunchKeys (int scancode, int press)
{
    static int lastcode;
    event_t ev;

    if (press && lastcode == scancode)
		return;		// Ignore repeating keys

	lastcode = press ? scancode : 0;
    ev.type = press ? ev_keydown : ev_keyup;

    switch (scancode)
    {
		// Convert right-side keys to their left-side equivalents
    case SCANCODE_RIGHTSHIFT:			scancode = KEY_LSHIFT;	  	 	break;
    case SCANCODE_RIGHTCONTROL:			scancode = KEY_LCTRL;	   		break;
    case SCANCODE_RIGHTALT:				scancode = KEY_LALT;	   		break;
    case SCANCODE_KEYPADENTER:			scancode = KEY_ENTER;	   		break;

		// Convert keycodes to their DirectInput equivalents
    case SCANCODE_PRINTSCREEN:
    case 84:							scancode = 183;					break;
    case SCANCODE_BREAK_ALTERNATIVE:
    case SCANCODE_BREAK:				scancode = KEY_PAUSE;			break;
    case SCANCODE_HOME:					scancode = KEY_HOME;			break;
    case SCANCODE_CURSORBLOCKUP:		scancode = KEY_UPARROW;			break;
    case SCANCODE_PAGEUP:				scancode = KEY_PGUP;			break;
    case SCANCODE_CURSORBLOCKLEFT:		scancode = KEY_LEFTARROW;		break;
    case SCANCODE_CURSORBLOCKRIGHT:		scancode = KEY_RIGHTARROW;		break;
    case SCANCODE_END:					scancode = KEY_END;				break;
    case SCANCODE_CURSORBLOCKDOWN:		scancode = KEY_DOWNARROW;		break;
    case SCANCODE_PAGEDOWN:				scancode = KEY_PGDN;			break;
    case SCANCODE_INSERT:				scancode = KEY_INS;				break;
    case SCANCODE_REMOVE:				scancode = KEY_DEL;				break;
    case 0x7d:							scancode = 0xdb; /* DIK_LWIN */	break;
    case 0x7e:							scancode = 0xdb; /* DIK_LWIN */	break;
    case 0x7f:							scancode = 0xdd; /* DIK_APPS */	break;

    // Remap the keypad, as appropriate 
    default:
		if (m_ConsoleOpen)
		{
			switch (scancode)
			{
			case SCANCODE_KEYPAD7:		scancode = SCANCODE_7;		break;
			case SCANCODE_KEYPAD8:		scancode = SCANCODE_8;		break;
			case SCANCODE_KEYPAD9:		scancode = SCANCODE_9; 		break;
			case SCANCODE_KEYPAD4:		scancode = SCANCODE_4;		break;
			case SCANCODE_KEYPAD5:		scancode = SCANCODE_5;		break;
			case SCANCODE_KEYPAD6:		scancode = SCANCODE_6;		break;
			case SCANCODE_KEYPAD1:		scancode = SCANCODE_1;		break;
			case SCANCODE_KEYPAD2:		scancode = SCANCODE_2;		break;
			case SCANCODE_KEYPAD3:		scancode = SCANCODE_3;		break;
			case SCANCODE_KEYPAD0:		scancode = SCANCODE_0;		break;
			case SCANCODE_KEYPADPERIOD:	scancode = SCANCODE_PERIOD;	break;
			}
		}
		else if (m_RemapKeypad)
		{
			switch (scancode)
			{
			case SCANCODE_KEYPAD7:		scancode = KEY_HOME;		break;
			case SCANCODE_KEYPAD8:		scancode = KEY_UPARROW;		break;
			case SCANCODE_KEYPAD9:		scancode = KEY_PGUP;		break;
			case SCANCODE_KEYPAD4:		scancode = KEY_LEFTARROW;	break;
			case SCANCODE_KEYPAD6:		scancode = KEY_RIGHTARROW;	break;
			case SCANCODE_KEYPAD1:		scancode = KEY_END;			break;
			case SCANCODE_KEYPAD2:		scancode = KEY_DOWNARROW;	break;
			case SCANCODE_KEYPAD3:		scancode = KEY_PGDN;		break;
			case SCANCODE_KEYPAD0:		scancode = KEY_INS;			break;
			case SCANCODE_KEYPADPERIOD:	scancode = KEY_DEL;			break;
			}
		}
    }
    if (scancode == KEY_LALT)
		m_AltDown = press;
    if (scancode == KEY_LSHIFT)
		m_ShiftDown = press;
    if (scancode == KEY_LCTRL)
		m_CtrlDown = press;

    if (scancode < 128)
    {
		ev.data2 = m_ToQWERTY[scancode];
#ifndef LINUX
		ev.data3 = m_ShiftDown ? m_ToShiftedQWERTY[scancode]
			: m_ToQWERTY[scancode];
#else
		ev.data3 = m_Translations
			[(m_ShiftDown ? SHIFT_MASK : 0) |
			 (m_AltDown ? ALT_MASK : 0) |
			 (m_CtrlDown ? CTRL_MASK : 0)][scancode];
#endif
    }
    else
    {
		ev.data2 = ev.data3 = 0;
    }
    
    ev.data1 = scancode == SCANCODE_KEYPADDIVIDE ? 0xb5 : scancode;

    ge->PostEvent (&ev);
}

// MOUSE ----------------------------------------------------------------------

#define WHEEL_DELTA		20

class SVGAMouse : public IMouse
{
public:
	SVGAMouse ();
	~SVGAMouse ();
	void ProcessInput (bool active);
	void SetGrabbed (bool grabbed) {}
	static void MouseHook ();

private:
	static int m_Buttons;

	static void MunchMouse (int button, int dx, int dy, int dz,
							int drx, int dry, int drz);

	static bool m_Active;
	static bool m_GenEvents;
};

bool SVGAMouse::m_Active = false;
bool SVGAMouse::m_GenEvents = true;

SVGAMouse::SVGAMouse ()
{
	if (!InitedSVGA)
		if (vga_init () == 0)
			InitedSVGA = true;
		else
		{
			globalres = 1;
			return;
		}

	vga_setmousesupport (1);
	m_Active = true;
	mouse_seteventhandler (MunchMouse);
}

SVGAMouse::~SVGAMouse ()
{
	m_Active = false;
	vga_setmousesupport (0);
}

void SVGAMouse::MouseHook ()
{
	if (m_Active)
		mouse_seteventhandler (MunchMouse);
}

void SVGAMouse::ProcessInput (bool active)
{
	m_GenEvents = active;
	mouse_update ();
}

int SVGAMouse::m_Buttons = 0;
void SVGAMouse::MunchMouse (int buttons, int dx, int dy, int dz,
							int drx, int dry, int drz)
{
	event_t ev;

	if (!m_GenEvents)
		return;

	if (dx | dy)
	{
		ev.type = ev_mouse;
		ev.data1 = 0;
		ev.data2 = dx * 8;
		ev.data3 = -dy * 4;
		ge->PostEvent (&ev);
	}

	ev.data2 = ev.data3 = 0;

	int prevbuttons = m_Buttons;
	if (prevbuttons != buttons)
	{
		int i;

		m_Buttons = buttons;
		for (i = 0; i < 4; i++)
		{
			if ((prevbuttons & 1) ^ (buttons & 1))
			{
				ev.type = (buttons & 1) ? ev_keydown : ev_keyup;
				ev.data1 = i == 0 ? KEY_MOUSE2 :
						   i == 1 ? KEY_MOUSE3 :
						   i == 2 ? KEY_MOUSE1 : KEY_MOUSE4;
				ge->PostEvent (&ev);
			}
			prevbuttons >>= 1;
			buttons >>= 1;
		}
	}

	if (drx)
	{
		int dir;

		if (drx < 0)
			ev.data1 = KEY_MWHEELUP, dir = WHEEL_DELTA;
		else
			ev.data1 = KEY_MWHEELDOWN, dir = -WHEEL_DELTA;

		while (abs (drx) >= WHEEL_DELTA)
		{
			ev.type = ev_keydown;
			ge->PostEvent (&ev);
			ev.type = ev_keyup;
			ge->PostEvent (&ev);
			drx += dir;
		}
	}
}

// JOYSTICK -------------------------------------------------------------------

class SVGAJoystick : public IJoystick
{
public:
	SVGAJoystick (int whichjoy);
	~SVGAJoystick ();
	void ProcessInput (bool active);
	void SetProperty (EJoyProp prop, float val);

private:
	bool m_Inited;
	int m_JoyDev;
	char m_xAxis, m_yAxis;

	static SVGAJoystick *m_Handle;

	static float m_SpeedMultiplier;
	static float m_xSensitivity;
	static float m_ySensitivity;
	static float m_xThreshold;
	static float m_yThreshold;

    static void JoyOut (const char *msg);
	static void MunchJoy (int event, int number, char value, int joydev);
};

SVGAJoystick *SVGAJoystick::m_Handle = NULL;
float SVGAJoystick::m_SpeedMultiplier;
float SVGAJoystick::m_xSensitivity;
float SVGAJoystick::m_ySensitivity;
float SVGAJoystick::m_xThreshold;
float SVGAJoystick::m_yThreshold;

void SVGAJoystick::SetProperty (EJoyProp prop, float val)
{
	switch (prop)
	{
	case JOYPROP_SpeedMultiplier:	m_SpeedMultiplier = val;	break;
	case JOYPROP_XSensitivity:		m_xSensitivity = val;		break;
	case JOYPROP_YSensitivity:		m_ySensitivity = val;		break;
	case JOYPROP_XThreshold:		m_xThreshold = val;			break;
	case JOYPROP_YThreshold:		m_yThreshold = val;			break;
	}
}

SVGAJoystick::SVGAJoystick (int whichjoy)
{
	m_Inited = false;
	if (whichjoy < 0 || whichjoy > 3)
	{
		globalres = 1;
		return;
	}
	m_JoyDev = whichjoy;
	if (joystick_init (whichjoy, JoyOut) < 0)
	{
		Printf (PRINT_HIGH, "Failed to initialize joystick %d\n", whichjoy);
		globalres = 1;
		return;
	}
	m_Inited = true;
	m_xAxis = m_yAxis = 0;

	joystick_sethandler (whichjoy, MunchJoy);
}

SVGAJoystick::~SVGAJoystick ()
{
	if (m_Inited)
	{
		m_Inited = false;
		joystick_close (m_JoyDev);
	}
}

void SVGAJoystick::JoyOut (const char *msg)
{
	Printf (PRINT_HIGH, "%s", msg);
}

void SVGAJoystick::ProcessInput (bool active)
{
	if (active)
	{
		m_Handle = this;
		joystick_update ();

		event_t ev;

		if (m_xAxis | m_yAxis)
		{
			ev.type = ev_joystick;
			ev.data1 = 0;
			ev.data2 = m_xAxis;
			ev.data3 = m_yAxis;
			ge->PostEvent (&ev);
		}
	}
}

void SVGAJoystick::MunchJoy (int event, int number, char value, int joydev)
{
	event_t ev;

	switch (event)
	{
	case JOY_EVENTBUTTONDOWN:
		ev.type = ev_keydown;
		ev.data1 = KEY_JOY1 + number;
		ev.data2 = ev.data3 = 0;
		ge->PostEvent (&ev);
		break;

	case JOY_EVENTBUTTONUP:
		ev.type = ev_keyup;
		ev.data1 = KEY_JOY1 + number;
		ev.data2 = ev.data3 = 0;
		ge->PostEvent (&ev);
		break;

	case JOY_EVENTAXIS:
		if (number == 0)
			m_Handle->m_xAxis = value;
		else if (number == 1)
			m_Handle->m_yAxis = value;
		break;
	}
}

// VIDEO ----------------------------------------------------------------------

class SVGAVideo : public IVideo
{
public:
	SVGAVideo ();
	~SVGAVideo ();

	EDisplayType GetDisplayType () { return DISPLAY_FullscreenOnly; }
	bool FullscreenChanged (bool fs) { return false; }
	void SetWindowedScale (float scale) {}
	bool CanBlit () { return false; }

	void SetPalette (DWORD *palette);
	void UpdateScreen (DCanvas *canvas);
	void ReadScreen (byte *block);

	bool CheckResolution (int width, int height, int bits);
	int GetModeCount ();
	void StartModeIterator (int bits);
	bool NextMode (int *width, int *height);
	bool SetMode (int width, int height, int bits, bool fs);

	bool AllocateSurface (DCanvas *scrn, int width, int height, int bits, bool primary);
	void ReleaseSurface (DCanvas *scrn);
	void LockSurface (DCanvas *scrn);
	void UnlockSurface (DCanvas *scrn);
	bool Blit (DCanvas *src, int sx, int sy, int sw, int sh,
			   DCanvas *dst, int dx, int dy, int dw, int dh);

private:
	static int SortModes (const void *a, const void *b);

	bool MakeModesList ();
	void FreeModes ();

	int m_NumModes;
	int m_IteratorMode;
	int m_IteratorBits;

	int m_DisplayWidth;
	int m_DisplayHeight;
	int m_DisplayBits;
	int m_DisplayPitch;

	int m_NeedPalChange;
	int m_PalEntries[256*3];
	bool m_Linear;

	struct Chain
	{
		DCanvas *canvas;
		Chain *next;
	} *m_Chain;

	struct ModeInfo
	{
		int width, height, bits, modenum;
	} *m_Modes;
};

SVGAVideo::SVGAVideo ()
{
	m_IteratorMode = m_IteratorBits = 0;
	m_DisplayWidth = m_DisplayHeight = m_DisplayBits = 0;
	m_NeedPalChange = false;

	if (!InitedSVGA)
	{
		seteuid (0);
		if (vga_init () != 0)
		{
			seteuid (getuid ());
			globalres = 1;
			return;
		}
	}

	InitedSVGA = true;
	if (!MakeModesList ())
	{
		seteuid (getuid ());
		globalres = 1;
		return;
	}
	seteuid (getuid ());
}

SVGAVideo::~SVGAVideo ()
{
	FreeModes ();
	while (m_Chain)
		ReleaseSurface (m_Chain->canvas);
	vga_setmode (TEXT);
	printf ("\n");
	system ("stty sane");
}

bool SVGAVideo::SetMode (int width, int height, int bits, bool fs)
{
	vga_modeinfo *info;
    int i;

    m_DisplayWidth = width;
    m_DisplayHeight = height;
    m_DisplayBits = bits;

    for (i = 0; i < m_NumModes; i++)
    {
		if (m_Modes[i].width == width &&
			m_Modes[i].height == height &&
			m_Modes[i].bits == bits)
		{
			break;
		}
    }

	seteuid (0);
    if (i == m_NumModes ||
		(info = vga_getmodeinfo (m_Modes[i].modenum)) == NULL ||
		vga_setmode (m_Modes[i].modenum) != 0)
	{
		seteuid (getuid ());
		return false;
	}

	SVGAMouse::MouseHook ();
	seteuid (getuid ());

	m_DisplayPitch = info->linewidth;

	if (info->flags & IS_LINEAR)
	{
		m_Linear = true;
	}
	else if (info->flags & CAPABLE_LINEAR && vga_setlinearaddressing() != -1)
	{
		m_Linear = true;
	}
	else
	{
		m_Linear = false;
	}
	return true;
}

void SVGAVideo::SetPalette (DWORD *pal)
{
    if (!pal)
		return;

    int i, j;
    for (i = j = 0; i < 256; i++, j+=3)
    {
		m_PalEntries[j]   = RPART(pal[i]) >> 2;
		m_PalEntries[j+1] = GPART(pal[i]) >> 2;
		m_PalEntries[j+2] = BPART(pal[i]) >> 2;
    }
    m_NeedPalChange++;
}

void SVGAVideo::UpdateScreen (DCanvas *canvas)
{
	int y;

	vga_lockvc ();
	vga_setdisplaystart (0);
	byte *from = canvas->buffer;

	if (m_Linear)
	{
		byte *to = vga_getgraphmem ();
		if (m_DisplayPitch == canvas->width && canvas->width == canvas->pitch)
		{
			memcpy (to, from, canvas->width * canvas->height);
		}
		else
		{
			for (y = canvas->height; y; y--)
			{
				memcpy (to, from, canvas->width);
				to += m_DisplayPitch;
				from += canvas->pitch;
			}
		}
	}
	else
	{
		for (y = 0; y < canvas->height; y++)
		{
			vga_drawscansegment (from, 0, y, canvas->width);
			from += canvas->pitch;
		}
	}

    if (m_NeedPalChange > 0)
    {
		m_NeedPalChange--;
		vga_setpalvec (0, 256, m_PalEntries);
    }

	vga_unlockvc ();
}

void SVGAVideo::ReadScreen (byte *block)
{
	int y;

	vga_lockvc ();
	for (y = 0; y < m_DisplayHeight; y++, block += m_DisplayWidth)
	{
		vga_getscansegment (block, 0, y, m_DisplayWidth);
	}
	vga_unlockvc ();
}

bool SVGAVideo::MakeModesList ()
{
    vga_modeinfo *info;
    int i, mode;
	bool success = false;
	int highestmode = vga_lastmodenumber ();

	m_NumModes = 0;
	for (i = 1; i < highestmode; i++)
		if (vga_hasmode (i))
			m_NumModes++;

	if (m_NumModes == 0)
		return false;

    m_Modes = new ModeInfo[m_NumModes];

    // Filter out modes taller than 1024 pixels because some
    // of the assembly routines will choke on them.
    for (mode = 1, i = 0; mode < highestmode; mode++)
    {
		if (vga_hasmode (mode) && (info = vga_getmodeinfo (mode)))
		{
			if (info->bytesperpixel == 1
				&& info->height <= 1024
				&& info->colors >= 256)
			{
				success = true;
				m_Modes[i].width = info->width;
				m_Modes[i].height = info->height;
				m_Modes[i].bits = info->bytesperpixel << 3;
				m_Modes[i].modenum = mode;
				i++;
			}
		}
    }
	m_NumModes = i;
	qsort (m_Modes, m_NumModes, sizeof(ModeInfo), SortModes);
	return success;
}

int SVGAVideo::SortModes (const void *a, const void *b)
{
	ModeInfo *mode1 = (ModeInfo *)a;
	ModeInfo *mode2 = (ModeInfo *)b;
	if (mode1->width < mode2->width)
		return -1;
	if (mode1->width > mode2->width)
		return 1;
	if (mode1->height < mode2->height)
		return -1;
	return 1;
}

void SVGAVideo::FreeModes ()
{
	if (m_Modes)
	{
		delete[] m_Modes;
		m_Modes = NULL;
		m_NumModes = 0;
	}
}

int SVGAVideo::GetModeCount ()
{
	return m_NumModes;
}

void SVGAVideo::StartModeIterator (int bits)
{
	m_IteratorMode = 0;
	m_IteratorBits = bits;
}

bool SVGAVideo::NextMode (int *width, int *height)
{
    if (m_IteratorMode < m_NumModes)
    {
		while (m_IteratorMode < m_NumModes &&
			   m_Modes[m_IteratorMode].bits != m_IteratorBits)
		{
			m_IteratorMode++;
		}

		if (m_IteratorMode < m_NumModes)
		{
			*width = m_Modes[m_IteratorMode].width;
			*height = m_Modes[m_IteratorMode].height;
			m_IteratorMode++;
			return true;
		}
    }
    return false;
}

bool SVGAVideo::AllocateSurface (DCanvas *scrn, int width, int height,
								 int bits, bool primary)
{
    if (scrn->m_Private)
		ReleaseSurface (scrn);

    scrn->width = width;
    scrn->height = height;
    scrn->is8bit = (bits == 8) ? true : false;
    scrn->bits = m_DisplayBits;
    scrn->m_LockCount = 0;
    scrn->m_Palette = NULL;

    if (!scrn->is8bit)
		return false;

	scrn->m_Private = new byte[width*height*(bits>>3)];

    Chain *tracer = new Chain;
    tracer->canvas = scrn;
    tracer->next = m_Chain;
    m_Chain = tracer;

    return true;
}

void SVGAVideo::ReleaseSurface (DCanvas *scrn)
{
    struct Chain *thisone, **prev;

    if (scrn->m_Private)
    {
		delete[] (byte *)scrn->m_Private;
		scrn->m_Private = NULL;
    }

    scrn->DetachPalette ();

    thisone = m_Chain;
    prev = &m_Chain;
    while (thisone && thisone->canvas != scrn)
    {
		prev = &thisone->next;
		thisone = thisone->next;
	}
	if (thisone)
	{
		*prev = thisone->next;
		delete thisone;
	}
}

void SVGAVideo::LockSurface (DCanvas *scrn)
{
    if (scrn->m_Private)
    {
		scrn->buffer = (byte *)scrn->m_Private;
		scrn->pitch = scrn->width;
    }
    else
    {
		scrn->m_LockCount--;
    }
}

void SVGAVideo::UnlockSurface (DCanvas *scrn)
{
	if (scrn->m_Private)
	{
		scrn->buffer = NULL;
	}
}

bool SVGAVideo::Blit (DCanvas *src, int sx, int sy, int sw, int sh,
					  DCanvas *dst, int dx, int dy, int dw, int dh)
{
	return false;
}

// ----------------------------------------------------------------------------

static void *QueryInterface (EInterfaceType type, int parm)
{
	void *res;

	globalres = 0;
	switch (type)
	{
	case INTERFACE_Video:		res = new SVGAVideo;			break;
	case INTERFACE_Keyboard:	res = new SVGAKeyboard;			break;
	case INTERFACE_Mouse:		res = new SVGAMouse;			break;
	case INTERFACE_Joystick:	res = new SVGAJoystick (parm);	break;
	default:					return NULL;
	}
	if (res == NULL)
		return NULL;
	if (globalres == 0)
		return res;
	switch (type)
	{
	case INTERFACE_Video:		delete (SVGAVideo *)res;		break;
	case INTERFACE_Keyboard:	delete (SVGAKeyboard *)res;		break;
	case INTERFACE_Mouse:		delete (SVGAMouse *)res;		break;
	case INTERFACE_Joystick:	delete (SVGAJoystick *)res;		break;
	}
	return NULL;
}

extern "C" queryinterface_t PrepLibrary (IEngineGlobals *globs)
{
	ge = globs;
	return QueryInterface;
}
