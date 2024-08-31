// ZDoom bases its keycodes on DirectInput's scan codes
// Why? Because it was Win32-only before porting to anything else,
// so this made sense. AFAIK, it's primarily used under Win32 now,
// so it still makes sense.
//
// Actually, these key codes may only be used for key bindings now,
// in which case they're not really necessary--if we tweaked c_bind.cpp.

enum
{
	DIK_ESCAPE = 1,
	DIK_1,
	DIK_2,
	DIK_3,
	DIK_4,
	DIK_5,
	DIK_6,
	DIK_7,
	DIK_8,
	DIK_9,
	DIK_0,
	DIK_MINUS,				/* - on main keyboard */
	DIK_EQUALS,
	DIK_BACK,				/* backspace */
	DIK_TAB,
	DIK_Q,
	DIK_W,
	DIK_E,
	DIK_R,
	DIK_T,
	DIK_Y,
	DIK_U,
	DIK_I,
	DIK_O,
	DIK_P,
	DIK_LBRACKET,
	DIK_RBRACKET,
	DIK_RETURN,				/* Enter on main keyboard */
	DIK_LCONTROL,
	DIK_A,
	DIK_S,
	DIK_D,
	DIK_F,
	DIK_G,
	DIK_H,
	DIK_J,
	DIK_K,
	DIK_L,
	DIK_SEMICOLON,
	DIK_APOSTROPHE,
	DIK_GRAVE,				/* accent grave */
	DIK_LSHIFT,
	DIK_BACKSLASH,
	DIK_Z,
	DIK_X,
	DIK_C,
	DIK_V,
	DIK_B,
	DIK_N,
	DIK_M,
	DIK_COMMA,
	DIK_PERIOD,				/* . on main keyboard */
	DIK_SLASH,				/* / on main keyboard */
	DIK_RSHIFT,
	DIK_MULTIPLY,			/* * on numeric keypad */
	DIK_LMENU,				/* left Alt */
	DIK_SPACE,
	DIK_CAPITAL,
	DIK_F1,
	DIK_F2,
	DIK_F3,
	DIK_F4,
	DIK_F5,
	DIK_F6,
	DIK_F7,
	DIK_F8,
	DIK_F9,
	DIK_F10,
	DIK_NUMLOCK,
	DIK_SCROLL,				/* Scroll Lock */
	DIK_NUMPAD7,
	DIK_NUMPAD8,
	DIK_NUMPAD9,
	DIK_SUBTRACT,			/* - on numeric keypad */
	DIK_NUMPAD4,
	DIK_NUMPAD5,
	DIK_NUMPAD6,
	DIK_ADD,				/* + on numeric keypad */
	DIK_NUMPAD1,
	DIK_NUMPAD2,
	DIK_NUMPAD3,
	DIK_NUMPAD0,
	DIK_DECIMAL,			/* . on numeric keypad */
	DIK_OEM_102 = 0x56,		/* < > | on UK/Germany keyboards */
	DIK_F11,
	DIK_F12,
	DIK_F13 = 0x64,		    /*                     (NEC PC98) */
	DIK_F14,				/*                     (NEC PC98) */
	DIK_F15,				/*                     (NEC PC98) */
	DIK_KANA = 0x70,		/* (Japanese keyboard)            */
	DIK_ABNT_C1 = 0x73,		/* / ? on Portugese (Brazilian) keyboards */
	DIK_CONVERT = 0x79,    	/* (Japanese keyboard)            */
	DIK_NOCONVERT = 0x7B,	/* (Japanese keyboard)            */
	DIK_YEN = 0x7D,			/* (Japanese keyboard)            */
	DIK_ABNT_C2 = 0x7E,		/* Numpad . on Portugese (Brazilian) keyboards */
	DIK_NUMPAD_EQUALS = 0x8D, /* = on numeric keypad (NEC PC98) */
	DIK_PREVTRACK = 0x90,	/* Previous Track (DIK_CIRCUMFLEX on Japanese keyboard) */
	DIK_AT,					/*                     (NEC PC98) */
	DIK_COLON,				/*                     (NEC PC98) */
	DIK_UNDERLINE,			/*                     (NEC PC98) */
	DIK_KANJI,				/* (Japanese keyboard)            */
	DIK_STOP,				/*                     (NEC PC98) */
	DIK_AX,					/*                     (Japan AX) */
	DIK_UNLABELED,			/*                        (J3100) */
	DIK_NEXTTRACK = 0x99,	/* Next Track */
	DIK_NUMPADENTER = 0x9C,	/* Enter on numeric keypad */
	DIK_RCONTROL = 0x9D,
	DIK_MUTE = 0xA0,		/* Mute */
	DIK_CALCULATOR = 0xA1,	/* Calculator */
	DIK_PLAYPAUSE = 0xA2,	/* Play / Pause */
	DIK_MEDIASTOP = 0xA4,	/* Media Stop */
	DIK_VOLUMEDOWN = 0xAE,	/* Volume - */
	DIK_VOLUMEUP = 0xB0,   	/* Volume + */
	DIK_WEBHOME = 0xB2,    	/* Web home */
	DIK_NUMPADCOMMA = 0xB3,	/* , on numeric keypad (NEC PC98) */
	DIK_DIVIDE = 0xB5,    	/* / on numeric keypad */
	DIK_SYSRQ = 0xB7,
	DIK_RMENU = 0xB8,		/* right Alt */
	DIK_PAUSE = 0xC5,		/* Pause */
	DIK_HOME = 0xC7,		/* Home on arrow keypad */
	DIK_UP = 0xC8,			/* UpArrow on arrow keypad */
	DIK_PRIOR = 0xC9,		/* PgUp on arrow keypad */
	DIK_LEFT = 0xCB,		/* LeftArrow on arrow keypad */
	DIK_RIGHT = 0xCD,		/* RightArrow on arrow keypad */
	DIK_END = 0xCF,			/* End on arrow keypad */
	DIK_DOWN = 0xD0,		/* DownArrow on arrow keypad */
	DIK_NEXT = 0xD1,		/* PgDn on arrow keypad */
	DIK_INSERT = 0xD2,		/* Insert on arrow keypad */
	DIK_DELETE = 0xD3,		/* Delete on arrow keypad */
	DIK_LWIN = 0xDB,		/* Left Windows key */
	DIK_RWIN = 0xDC,		/* Right Windows key */
	DIK_APPS = 0xDD,		/* AppMenu key */
	DIK_POWER = 0xDE,		/* System Power */
	DIK_SLEEP = 0xDF,		/* System Sleep */
	DIK_WAKE = 0xE3,		/* System Wake */
	DIK_WEBSEARCH = 0xE5,	/* Web Search */
	DIK_WEBFAVORITES = 0xE6, /* Web Favorites */
	DIK_WEBREFRESH = 0xE7,	/* Web Refresh */
	DIK_WEBSTOP = 0xE8,		/* Web Stop */
	DIK_WEBFORWARD = 0xE9,	/* Web Forward */
	DIK_WEBBACK = 0xEA,		/* Web Back */
	DIK_MYCOMPUTER = 0xEB,	/* My Computer */
	DIK_MAIL = 0xEC,		/* Mail */
	DIK_MEDIASELECT = 0xED	/* Media Select */
};
