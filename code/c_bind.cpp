#include "doomtype.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "c_bind.h"
#include "g_level.h"
#include "hu_stuff.h"
#include "gi.h"
#include "configfile.h"

#include <math.h>
#include <stdlib.h>

struct FBinding
{
	const char *Key;
	const char *Bind;
};

/* Default keybindings for Doom (and all other games)
 */
static const FBinding DefBindings[] =
{
	{ "`", "toggleconsole" },
	{ "1", "slot 1" },
	{ "2", "slot 2" },
	{ "3", "slot 3" },
	{ "4", "slot 4" },
	{ "5", "slot 5" },
	{ "6", "slot 6" },
	{ "7", "slot 7" },
	{ "8", "slot 8" },
	{ "9", "slot 9" },
	{ "0", "slot 0" },
	{ "[", "invprev" },
	{ "]", "invnext" },
	{ "enter", "invuse" },
	{ "-", "sizedown" },
	{ "=", "sizeup" },
	{ "ctrl", "+attack" },
	{ "alt", "+strafe" },
	{ "shift", "+speed" },
	{ "space", "+use" },
	{ "rightarrow", "+right" },
	{ "leftarrow", "+left" },
	{ "uparrow", "+forward" },
	{ "downarrow", "+back" },
	{ ",", "+moveleft" },
	{ ".", "+moveright" },
	{ "mouse1", "+attack" },
	{ "mouse2", "+strafe" },
	{ "mouse3", "+forward" },
	{ "mouse4", "+speed" },
	{ "joy1", "+attack" },
	{ "joy2", "+strafe" },
	{ "joy3", "+speed" },
	{ "joy4", "+use" },
	{ "capslock", "toggle cl_run" },
	{ "f1", "menu_help" },
	{ "f2", "menu_save" },
	{ "f3", "menu_load" },
	{ "f4", "menu_options" },
	{ "f5", "menu_display" },
	{ "f6", "quicksave" },
	{ "f7", "menu_endgame" },
	{ "f8", "togglemessages" },
	{ "f9", "quickload" },
	{ "f11", "bumpgamma" },
	{ "f10", "menu_quit" },
	{ "tab", "togglemap" },
	{ "pause", "pause" },
	{ "sysrq", "screenshot" },
	{ "t", "messagemode" },
	{ "\\", "+showscores" },
	{ "f12", "spynext" },
	{ "mwheeldown", "weapnext" },
	{ "mwheelup", "weapprev" },
	NULL
};

static const FBinding DefRavenBindings[] =
{
	{ "pgup", "+moveup" },
	{ "insert", "+movedown" },
	{ "home", "land" },
	{ "pgdn", "+lookup" },
	{ "del", "+lookdown" },
	{ "end", "centerview" },
	NULL
};

static const FBinding DefHexenBindings[] =
{
	{ "/", "+jump" },
	{ "backspace", "invuseall" },
	// \ arti_health
	// 0 arti_poisonbag
	// 9 arti_blastradius
	// 8 arti_teleport
	// 7 arti_teleportother
	// 6 arti_egg
	// 5 arti_invulnerability
	NULL
};

const char *KeyNames[NUM_KEYS] = {
	// This array is dependant on the particular keyboard input
	// codes generated in i_input.c. If they change there, they
	// also need to change here. In this case, we use the
	// DirectInput codes and assume a qwerty keyboard layout.
	// See <dinput.h> for the DIK_* codes

	NULL,  "escape","1",	"2",	"3",	"4",	"5",		"6",		//00
	"7",	"8",	"9",	"0",	"-",	"=",	"backspace","tab",		//08
	"q",	"w",	"e",	"r",	"t",	"y",	"u",		"i",		//10
	"o",	"p",	"[",	"]",	"enter","ctrl",	"a",		"s",		//18
	"d",	"f",	"g",	"h",	"j",	"k",	"l",		";",		//20
	"'",	"`",	"shift","\\",	"z",	"x",	"c",		"v",		//28
	"b",	"n",	"m",	",",	".",	"/",	NULL,		"kp*",		//30
	"alt",	"space","capslock","f1","f2",	"f3",	"f4",		"f5",		//38
	"f6",	"f7",	"f8",	"f9",	"f10",	"numlock","scroll",	"kp7",		//40
	"kp8",	"kp9",	"kp-",	"kp4",	"kp5",	"kp6",	"kp+",		"kp1",		//48
	"kp2",	"kp3",	"kp0",	"kp.",	NULL,	NULL,	NULL,		"f11",		//50
	"f12",	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//58
	NULL,	NULL,	NULL,	NULL,	"f13",	"f14",	"f15",		NULL,		//60
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//68
	"kana",	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//70
	NULL,	"convert",NULL,	"noconvert",NULL,"yen",	NULL,		NULL,		//78
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//80
	NULL,	NULL,	NULL,	NULL,	NULL,	"kp=",	NULL,		NULL,		//88
	"circumflex","@",":",	"_",	"kanji","stop",	"ax",		"unlabeled",//90
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//98
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//A0
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//A8
	NULL,	NULL,	NULL,	"kp,",	NULL,	"kp/",	NULL,		"sysrq",	//B0
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//B8
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		"home",		//C0
	"uparrow","pgup",NULL,	"leftarrow",NULL,"rightarrow",NULL,	"end",		//C8
	"downarrow","pgdn","ins","del",	NULL,	NULL,	NULL,		NULL,		//D0
	NULL,	NULL,	NULL,	"lwin",	"rwin",	"apps",	NULL,		NULL,		//D8
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//E0
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//E8
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		NULL,		//F0
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,		"pause",	//F8

	// non-keyboard buttons that can be bound
	"mouse1",	"mouse2",	"mouse3",	"mouse4",		// 4 mouse buttons
	"mwheelup",	"mwheeldown",NULL,		NULL,			// the wheel and some extra space
	"joy1",		"joy2",		"joy3",		"joy4",			// 32 joystick buttons
	"joy5",		"joy6",		"joy7",		"joy8",
	"joy9",		"joy10",	"joy11",	"joy12",
	"joy13",	"joy14",	"joy15",	"joy16",
	"joy17",	"joy18",	"joy19",	"joy20",
	"joy21",	"joy22",	"joy23",	"joy24",
	"joy25",	"joy26",	"joy27",	"joy28",
	"joy29",	"joy30",	"joy31",	"joy32"
};

static char *Bindings[NUM_KEYS];
static char *DoubleBindings[NUM_KEYS];
static int DClickTime[NUM_KEYS];
static byte DClicked[(NUM_KEYS+7)/8];

static int GetKeyFromName (const char *name)
{
	int i;

	// Names of the form #xxx are translated to key xxx automatically
	if (name[0] == '#' && name[1] != 0) {
		return atoi (name + 1);
	}

	// Otherwise, we scan the KeyNames[] array for a matching name
	for (i = 0; i < NUM_KEYS; i++) {
		if (KeyNames[i] && !stricmp (KeyNames[i], name))
			return i;
	}
	return 0;
}

static const char *KeyName (int key)
{
	static char name[5];

	if (KeyNames[key])
		return KeyNames[key];

	sprintf (name, "#%d", key);
	return name;
}

CCMD (unbindall)
{
	int i;

	for (i = 0; i < NUM_KEYS; i++)
		if (Bindings[i])
		{
			free (Bindings[i]);
			Bindings[i] = NULL;
		}

	for (i = 0; i < NUM_KEYS; i++)
		if (DoubleBindings[i])
		{
			free (DoubleBindings[i]);
			DoubleBindings[i] = NULL;
		}
}

CCMD (unbind)
{
	int i;

	if (argc > 1)
	{
		if ( (i = GetKeyFromName (argv[1])) )
		{
			if (Bindings[i])
			{
				free (Bindings[i]);
				Bindings[i] = NULL;
			}
		}
		else
		{
			Printf ("Unknown key \"%s\"\n", argv[1]);
			return;
		}

	}
}

CCMD (bind)
{
	int i;

	if (argc > 1) {
		i = GetKeyFromName (argv[1]);
		if (!i) {
			Printf ("Unknown key \"%s\"\n", argv[1]);
			return;
		}
		if (argc == 2) {
			Printf ("\"%s\" = \"%s\"\n", argv[1], (Bindings[i] ? Bindings[i] : ""));
		} else {
			ReplaceString (&Bindings[i], argv[2]);
		}
	} else {
		Printf ("Current key bindings:\n");
		
		for (i = 0; i < NUM_KEYS; i++) {
			if (Bindings[i])
				Printf ("%s \"%s\"\n", KeyName (i), Bindings[i]);
		}
	}
}

CCMD (undoublebind)
{
	int i;

	if (argc > 1)
	{
		if ( (i = GetKeyFromName (argv[1])) )
		{
			if (DoubleBindings[i])
			{
				delete[] DoubleBindings[i];
				DoubleBindings[i] = NULL;
			}
		}
		else
		{
			Printf ("Unknown key \"%s\"\n", argv[1]);
			return;
		}

	}
}

CCMD (doublebind)
{
	int i;

	if (argc > 1)
	{
		i = GetKeyFromName (argv[1]);
		if (!i)
		{
			Printf ("Unknown key \"%s\"\n", argv[1]);
			return;
		}
		if (argc == 2)
		{
			Printf ("\"%s\" = \"%s\"\n", argv[1], (DoubleBindings[i] ? DoubleBindings[i] : ""));
		}
		else
		{
			ReplaceString (&DoubleBindings[i], argv[2]);
		}
	}
	else
	{
		Printf ("Current key doublebindings:\n");
		
		for (i = 0; i < NUM_KEYS; i++)
		{
			if (DoubleBindings[i])
				Printf ("%s \"%s\"\n", KeyName (i), DoubleBindings[i]);
		}
	}
}

static void SetBinds (const FBinding *array)
{
	while (array->Key)
	{
		C_DoBind (array->Key, array->Bind, false);
		array++;
	}
}

CCMD(binddefaults)
{
	SetBinds (DefBindings);

	if (gameinfo.gametype & GAME_Raven)
	{
		SetBinds (DefRavenBindings);
	}

	if (gameinfo.gametype == GAME_Hexen)
	{
		SetBinds (DefHexenBindings);
	}
}

BOOL C_DoKey (event_t *ev)
{
	char *binding = NULL;
	int dclickspot;
	byte dclickmask;

	if (ev->type != EV_KeyDown && ev->type != EV_KeyUp)
		return false;

	dclickspot = ev->data1 >> 3;
	dclickmask = 1 << (ev->data1 & 7);

	if (DClickTime[ev->data1] > level.time && ev->type == EV_KeyDown)
	{
		// Key pressed for a double click
		binding = DoubleBindings[ev->data1];
		DClicked[dclickspot] |= dclickmask;
	}
	else
	{
		if (ev->type == EV_KeyDown)
		{
			// Key pressed for a normal press
			binding = Bindings[ev->data1];
			DClickTime[ev->data1] = level.time + 20;
		}
		else if (DClicked[dclickspot] & dclickmask)
		{
			// Key released from a double click
			binding = DoubleBindings[ev->data1];
			DClicked[dclickspot] &= ~dclickmask;
			DClickTime[ev->data1] = 0;
		}
		else
		{
			// Key released from a normal press
			binding = Bindings[ev->data1];
		}
	}

	if (!binding)
		binding = Bindings[ev->data1];

	if (binding && (chatmodeon == 0 || ev->data1 < 256))
	{
		if (ev->type == EV_KeyDown)
		{
			char copy[1024];
			// Copy the command in case the binding rebinds the key
			strcpy (copy, binding);
			AddCommandString (copy);
		}
		else
		{
			char *achar;
		
			achar = strchr (binding, '+');
			if (!achar)
				return false;

			if ((achar == binding) || (*(achar - 1) <= ' '))
			{
				*achar = '-';
				AddCommandString (binding);
				*achar = '+';
			}
		}
		return true;
	}
	return false;
}

void C_ArchiveBindings (FConfigFile *f, bool dodouble)
{
	char **bindings;
	const char *name;
	int i;

	bindings = dodouble ? DoubleBindings : Bindings;

	for (i = 0; i < NUM_KEYS; i++)
	{
		if (bindings[i])
		{
			name = KeyName (i);
			if (name[1] == 0)	// Make sure given name is config-safe
			{
				if (name[0] == '[')
					name = "LeftBracket";
				else if (name[0] == ']')
					name = "RightBracket";
				else if (name[0] == '=')
					name = "Equals";
			}
			f->SetValueForKey (name, bindings[i]);
		}
	}
}

void C_DoBind (const char *key, const char *bind, bool dodouble)
{
	int keynum = GetKeyFromName (key);
	if (keynum == 0)
	{
		if (stricmp (key, "LeftBracket") == 0)
		{
			keynum = GetKeyFromName ("[");
		}
		else if (stricmp (key, "RightBracket") == 0)
		{
			keynum = GetKeyFromName ("]");
		}
		else if (stricmp (key, "Equals") == 0)
		{
			keynum = GetKeyFromName ("=");
		}
	}
	if (keynum != 0)
	{
		ReplaceString ((dodouble ? DoubleBindings : Bindings) + keynum, bind);
	}
}

int C_GetKeysForCommand (char *cmd, int *first, int *second)
{
	int c, i;

	*first = *second = c = i = 0;

	while (i < NUM_KEYS && c < 2) {
		if (Bindings[i] && !stricmp (cmd, Bindings[i])) {
			if (c++ == 0)
				*first = i;
			else
				*second = i;
		}
		i++;
	}
	return c;
}

void C_NameKeys (char *str, int first, int second)
{
	int c = 0;

	*str = 0;
	if (first) {
		c++;
		strcpy (str, KeyName (first));
		if (second)
			strcat (str, " or ");
	}

	if (second) {
		c++;
		strcat (str, KeyName (second));
	}

	if (!c)
		strcpy (str, "???");
}

void C_UnbindACommand (char *str)
{
	int i;

	for (i = 0; i < NUM_KEYS; i++) {
		if (Bindings[i] && !stricmp (str, Bindings[i])) {
			delete[] Bindings[i];
			Bindings[i] = NULL;
		}
	}
}

void C_ChangeBinding (const char *str, int newone)
{
	if (Bindings[newone])
		delete[] Bindings[newone];

	Bindings[newone] = copystring (str);
}

char *C_GetBinding (int key)
{
	return Bindings[key];
}
