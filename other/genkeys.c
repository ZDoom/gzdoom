#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

static const char qwerty[] = "qwerty";

static const struct { KeySym sym; const char *to; } Mappings[] =
{
	{ XK_BackSpace,		"backspace" },
	{ XK_Tab,			"tab" },
	{ XK_Return,		"enter" },
	{ XK_Pause,			"pause" },
	{ XK_Scroll_Lock,	"scroll" },
	{ XK_Sys_Req,		"sysrq" },
	{ XK_Escape,		"escape" },
	{ XK_Delete,		"del" },
	{ XK_Kanji,			"kanji" },
	{ XK_Home,			"home" },
	{ XK_Left,			"leftarrow" },
	{ XK_Up,			"uparrow" },
	{ XK_Right,			"rightarrow" },
	{ XK_Down,			"downarrow" },
	{ XK_Prior,			"pgup" },
	{ XK_Next,			"pgdn" },
	{ XK_End,			"end" },
	{ XK_Select,		"lwin" },
	{ XK_Print,			"sysrq" },
	{ XK_Insert,		"ins" },
	{ XK_Find,			"pause" },
	{ XK_Break,			"pause" },
	{ XK_Mode_switch,	"alt" },
	{ XK_Num_Lock,		"numlock" },
	{ XK_KP_Enter,		"enter" },
	{ XK_KP_Tab,		"tab" },
	{ XK_KP_Space,		"space" },
	{ XK_KP_F1,			"f1" },
	{ XK_KP_F2,			"f2" },
	{ XK_KP_F3,			"f3" },
	{ XK_KP_F4,			"f4" },
	{ XK_KP_Home,		"kp7" },
	{ XK_KP_Left,		"kp4" },
	{ XK_KP_Up,			"kp8" },
	{ XK_KP_Right,		"kp6" },
	{ XK_KP_Down,		"kp2" },
	{ XK_KP_Prior,		"kp9" },
	{ XK_KP_Next,		"kp3" },
	{ XK_KP_End,		"kp1" },
	{ XK_KP_Begin,		"kp7" },
	{ XK_KP_Insert,		"kp0" },
	{ XK_KP_Delete,		"kp." },
	{ XK_KP_Equal,		"kp=" },
	{ XK_KP_Multiply,	"kp*" },
	{ XK_KP_Add,		"kp+" },
	{ XK_KP_Separator,	"kp," },
	{ XK_KP_Subtract,	"kp-" },
	{ XK_KP_Decimal,	"kp." },
	{ XK_KP_Divide,		"kp/" },
	{ XK_KP_0,			"kp0" },
	{ XK_KP_1,			"kp1" },
	{ XK_KP_2,			"kp2" },
	{ XK_KP_3,			"kp3" },
	{ XK_KP_4,			"kp4" },
	{ XK_KP_5,			"kp5" },
	{ XK_KP_6,			"kp6" },
	{ XK_KP_7,			"kp7" },
	{ XK_KP_8,			"kp8" },
	{ XK_KP_9,			"kp9" },
	{ XK_F1,			"f1" },
	{ XK_F2,			"f2" },
	{ XK_F3,			"f3" },
	{ XK_F4,			"f4" },
	{ XK_F5,			"f5" },
	{ XK_F6,			"f6" },
	{ XK_F7,			"f7" },
	{ XK_F8,			"f8" },
	{ XK_F9,			"f9" },
	{ XK_F10,			"f10" },
	{ XK_F11,			"f11" },
	{ XK_F12,			"f12" },
	{ XK_F13,			"f13" },
	{ XK_F14,			"f14" },
	{ XK_F15,			"f15" },
	{ XK_Shift_L,		"shift" },
	{ XK_Shift_R,		"shift" },
	{ XK_Control_L,		"ctrl" },
	{ XK_Control_R,		"ctrl" },
	{ XK_Caps_Lock,		"capslock" },
	{ XK_Shift_Lock,	"capslock" },
	{ XK_Meta_L,		"alt" },
	{ XK_Meta_R,		"alt" },
	{ XK_Alt_L,			"alt" },
	{ XK_Alt_R,			"alt" },
	{ XK_space,			"space" },
	{ XK_exclam,		"!" },
	{ XK_quotedbl,		"\"" },
	{ XK_numbersign,	"#" },
	{ XK_dollar,		"$" },
	{ XK_percent,		"%" },
	{ XK_ampersand,		"&" },
	{ XK_apostrophe,	"'" },
	{ XK_parenleft,		"(" },
	{ XK_parenright,	")" },
	{ XK_asterisk,		"*" },
	{ XK_plus,			"=" },
	{ XK_comma,			"," },
	{ XK_minus,			"-" },
	{ XK_period,		"." },
	{ XK_slash,			"/" },
	{ XK_colon,			":" },
	{ XK_semicolon,		";" },
	{ XK_less,			"," },
	{ XK_equal,			"=" },
	{ XK_greater,		"." },
	{ XK_question,		"/" },
	{ XK_at,			"@" },
	{ XK_bracketleft,	"[" },
	{ XK_backslash,		"\\" },
	{ XK_bracketright,	"]" },
	{ XK_underscore,	"_" },
	{ XK_grave,			"`" },
	{ XK_braceleft,		"[" },
	{ XK_bar,			"\\" },
	{ XK_braceright,	"]" },
	{ XK_asciitilde,	"~" },
	{ XK_yen,			"yen" },
	{ XK_Henkan_Mode,	"convert" },
	{ XK_Muhenkan,		"noconvert" },
	{ XK_Cancel,		"stop" },
	{ XK_Multi_key,		"apps" },
#ifdef XK_ColonSign
	{ XK_ColonSign,		":" },
#endif
#ifdef XK_dead_circumflex
	{ XK_dead_circumflex, "circumflex" },
#endif
	{ 0, NULL }
};

int main (void)
{
	int i, j;
	const char *sigchar = qwerty;
	Display *disp = XOpenDisplay (NULL);

	if (!disp)
	{
		fprintf (stderr, "You need to be connected to an X server\n");
		return 1;
	}
	
	for (i = 0; i < 256; i++)
	{
		KeySym sym = XKeycodeToKeysym (disp, i, 0);
		if (sym >= XK_0 && sym <= XK_9)
			printf ("%d %c\n", i, sym - XK_0 + '0');
		else if (sym >= XK_A && sym <= XK_Z)
			printf ("%d %c\n", i, sym - XK_A + 'a');
		else if (sym >= XK_a && sym <= XK_z)
		{
			printf ("%d %c\n", i, sym - XK_a + 'a');
			if (sigchar && *sigchar)
			{
				if (sym - XK_a + 'a' != *sigchar)
				{
					fprintf (stderr,
		   ">>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<\n"
		   ">>> Are you sure this is a QWERTY keyboard layout? <<<\n"
		   ">>>>>>>>>>>>>>>>>>>>>>>>>>>><<<<<<<<<<<<<<<<<<<<<<<<<<\n");
					sigchar = NULL;
				}
				else
					sigchar++;
			}
		}
		else
		{
			for (j = 0; Mappings[j].to; j++)
				if (Mappings[j].sym == sym)
				{
					printf ("%d %s\n", i, Mappings[j].to);
					break;
				}
		}
	}

	XCloseDisplay (disp);
	return 0;
}
