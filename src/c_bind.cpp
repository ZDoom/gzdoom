/*
** c_bind.cpp
** Functions for using and maintaining key bindings
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "doomtype.h"
#include "doomdef.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "c_bind.h"
#include "g_level.h"
#include "hu_stuff.h"
#include "gi.h"
#include "configfile.h"
#include "i_system.h"
#include "d_event.h"
#include "w_wad.h"
#include "templates.h"
#include "dobject.h"
#include "vm.h"
#include "i_time.h"

#include <math.h>
#include <stdlib.h>

const char *KeyNames[NUM_KEYS] =
{
	// This array is dependant on the particular keyboard input
	// codes generated in i_input.c. If they change there, they
	// also need to change here. In this case, we use the
	// DirectInput codes and assume a qwerty keyboard layout.
	// See <dinput.h> for the DIK_* codes

	NULL,		"escape",	"1",		"2",		"3",		"4",		"5",		"6",		//00
	"7",		"8",		"9",		"0",		"-",		"=",		"backspace","tab",		//08
	"q",		"w",		"e",		"r",		"t",		"y",		"u",		"i",		//10
	"o",		"p",		"[",		"]",		"enter",	"ctrl",		"a",		"s",		//18
	"d",		"f",		"g",		"h",		"j",		"k",		"l",		";",		//20
	"'",		"`",		"shift",	"\\",		"z",		"x",		"c",		"v",		//28
	"b",		"n",		"m",		",",		".",		"/",		"rshift",	"kp*",		//30
	"alt",		"space",	"capslock",	"f1",		"f2",		"f3",		"f4",		"f5",		//38
	"f6",		"f7",		"f8",		"f9",		"f10",		"numlock",	"scroll",	"kp7",		//40
	"kp8",		"kp9",		"kp-",		"kp4",		"kp5",		"kp6",		"kp+",		"kp1",		//48
	"kp2",		"kp3",		"kp0",		"kp.",		NULL,		NULL,		"oem102",	"f11",		//50
	"f12",		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//58
	NULL,		NULL,		NULL,		NULL,		"f13",		"f14",		"f15",		"f16",		//60
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//68
	"kana",		NULL,		NULL,		"abnt_c1",	NULL,		NULL,		NULL,		NULL,		//70
	NULL,		"convert",	NULL,		"noconvert",NULL,		"yen",		"abnt_c2",	NULL,		//78
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//80
	NULL,		NULL,		NULL,		NULL,		NULL,		"kp=",		NULL,		NULL,		//88
	"circumflex","@",		":",		"_",		"kanji",	"stop",		"ax",		"unlabeled",//90
	NULL,		"prevtrack",NULL,		NULL,		"kp-enter",	"rctrl",	NULL,		NULL,		//98
	"mute",		"calculator","play",	NULL,		"stop",		NULL,		NULL,		NULL,		//A0
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		"voldown",	NULL,		//A8
	"volup",	NULL,		"webhome",	"kp,",		NULL,		"kp/",		NULL,		"sysrq",	//B0
	"ralt",		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//B8
	NULL,		NULL,		NULL,		NULL,		NULL,		"pause",	NULL,		"home",		//C0
	"uparrow",	"pgup",		NULL,		"leftarrow",NULL,		"rightarrow",NULL,		"end",		//C8
	"downarrow","pgdn",		"ins",		"del",		NULL,		NULL,		NULL,		NULL,		//D0
#ifdef __APPLE__
	NULL,		NULL,		NULL,		"command",	NULL,		"apps",		"power",	"sleep",	//D8
#else // !__APPLE__
	NULL,		NULL,		NULL,		"lwin",		"rwin",		"apps",		"power",	"sleep",	//D8
#endif // __APPLE__
	NULL,		NULL,		NULL,		"wake",		NULL,		"search",	"favorites","refresh",	//E0
	"webstop",	"webforward","webback",	"mycomputer","mail",	"mediaselect",NULL,		NULL,		//E8
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//F0
	NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		NULL,		//F8

	// non-keyboard buttons that can be bound
	"mouse1",	"mouse2",	"mouse3",	"mouse4",		// 8 mouse buttons
	"mouse5",	"mouse6",	"mouse7",	"mouse8",

	"joy1",		"joy2",		"joy3",		"joy4",			// 128 joystick buttons!
	"joy5",		"joy6",		"joy7",		"joy8",
	"joy9",		"joy10",	"joy11",	"joy12",
	"joy13",	"joy14",	"joy15",	"joy16",
	"joy17",	"joy18",	"joy19",	"joy20",
	"joy21",	"joy22",	"joy23",	"joy24",
	"joy25",	"joy26",	"joy27",	"joy28",
	"joy29",	"joy30",	"joy31",	"joy32",
	"joy33",	"joy34",	"joy35",	"joy36",
	"joy37",	"joy38",	"joy39",	"joy40",
	"joy41",	"joy42",	"joy43",	"joy44",
	"joy45",	"joy46",	"joy47",	"joy48",
	"joy49",	"joy50",	"joy51",	"joy52",
	"joy53",	"joy54",	"joy55",	"joy56",
	"joy57",	"joy58",	"joy59",	"joy60",
	"joy61",	"joy62",	"joy63",	"joy64",
	"joy65",	"joy66",	"joy67",	"joy68",
	"joy69",	"joy70",	"joy71",	"joy72",
	"joy73",	"joy74",	"joy75",	"joy76",
	"joy77",	"joy78",	"joy79",	"joy80",
	"joy81",	"joy82",	"joy83",	"joy84",
	"joy85",	"joy86",	"joy87",	"joy88",
	"joy89",	"joy90",	"joy91",	"joy92",
	"joy93",	"joy94",	"joy95",	"joy96",
	"joy97",	"joy98",	"joy99",	"joy100",
	"joy101",	"joy102",	"joy103",	"joy104",
	"joy105",	"joy106",	"joy107",	"joy108",
	"joy109",	"joy110",	"joy111",	"joy112",
	"joy113",	"joy114",	"joy115",	"joy116",
	"joy117",	"joy118",	"joy119",	"joy120",
	"joy121",	"joy122",	"joy123",	"joy124",
	"joy125",	"joy126",	"joy127",	"joy128",

	"pov1up",	"pov1right","pov1down",	"pov1left",		// First POV hat
	"pov2up",	"pov2right","pov2down",	"pov2left",		// Second POV hat
	"pov3up",	"pov3right","pov3down",	"pov3left",		// Third POV hat
	"pov4up",	"pov4right","pov4down",	"pov4left",		// Fourth POV hat

	"mwheelup",	"mwheeldown",							// the mouse wheel
	"mwheelright", "mwheelleft",

	"axis1plus","axis1minus","axis2plus","axis2minus",	// joystick axes as buttons
	"axis3plus","axis3minus","axis4plus","axis4minus",
	"axis5plus","axis5minus","axis6plus","axis6minus",
	"axis7plus","axis7minus","axis8plus","axis8minus",

	"lstickright","lstickleft","lstickdown","lstickup",			// Gamepad axis-based buttons
	"rstickright","rstickleft","rstickdown","rstickup",

	"dpadup","dpaddown","dpadleft","dpadright",	// Gamepad buttons
	"pad_start","pad_back","lthumb","rthumb",
	"lshoulder","rshoulder","ltrigger","rtrigger",
	"pad_a", "pad_b", "pad_x", "pad_y"
};

FKeyBindings Bindings;
FKeyBindings DoubleBindings;
FKeyBindings AutomapBindings;

DEFINE_GLOBAL(Bindings)
DEFINE_GLOBAL(AutomapBindings)

static unsigned int DClickTime[NUM_KEYS];
static uint8_t DClicked[(NUM_KEYS+7)/8];

//=============================================================================
//
//
//
//=============================================================================

static int GetKeyFromName (const char *name)
{
	int i;

	// Names of the form #xxx are translated to key xxx automatically
	if (name[0] == '#' && name[1] != 0)
	{
		return atoi (name + 1);
	}

	// Otherwise, we scan the KeyNames[] array for a matching name
	for (i = 0; i < NUM_KEYS; i++)
	{
		if (KeyNames[i] && !stricmp (KeyNames[i], name))
			return i;
	}
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

static int GetConfigKeyFromName (const char *key)
{
	int keynum = GetKeyFromName(key);
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
		else if (stricmp (key, "KP-Equals") == 0)
		{
			keynum = GetKeyFromName ("kp=");
		}
	}
	return keynum;
}

//=============================================================================
//
//
//
//=============================================================================

static const char *KeyName (int key)
{
	static char name[5];

	if (KeyNames[key])
		return KeyNames[key];

	mysnprintf (name, countof(name), "Key_%d", key);
	return name;
}

//=============================================================================
//
//
//
//=============================================================================

static const char *ConfigKeyName(int keynum)
{
	const char *name = KeyName(keynum);
	if (name[1] == 0)	// Make sure given name is config-safe
	{
		if (name[0] == '[')
			return "LeftBracket";
		else if (name[0] == ']')
			return "RightBracket";
		else if (name[0] == '=')
			return "Equals";
		else if (strcmp (name, "kp=") == 0)
			return "KP-Equals";
	}
	return name;
}

//=============================================================================
//
//
//
//=============================================================================

DEFINE_ACTION_FUNCTION(FKeyBindings, SetBind)
{
	PARAM_SELF_STRUCT_PROLOGUE(FKeyBindings);
	PARAM_INT(k);
	PARAM_STRING(cmd);
	self->SetBind(k, cmd);
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

void C_NameKeys (char *str, int first, int second)
{
	int c = 0;

	*str = 0;
	if (second == first) second = 0;
	if (first)
	{
		c++;
		strcpy (str, KeyName (first));
		if (second)
			strcat (str, " or ");
	}

	if (second)
	{
		c++;
		strcat (str, KeyName (second));
	}

	if (!c)
		*str = '\0';
}

DEFINE_ACTION_FUNCTION(FKeyBindings, NameKeys)
{
	PARAM_PROLOGUE;
	PARAM_INT(k1);
	PARAM_INT(k2);
	char buffer[120];
	C_NameKeys(buffer, k1, k2);
	ACTION_RETURN_STRING(buffer);
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::DoBind (const char *key, const char *bind)
{
	int keynum = GetConfigKeyFromName (key);
	if (keynum != 0)
	{
		Binds[keynum] = bind;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::UnbindAll ()
{
	for (int i = 0; i < NUM_KEYS; ++i)
	{
		Binds[i] = "";
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::UnbindKey(const char *key)
{
	int i;

	if ( (i = GetKeyFromName (key)) )
	{
		Binds[i] = "";
	}
	else
	{
		Printf ("Unknown key \"%s\"\n", key);
		return;
	}
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::PerformBind(FCommandLine &argv, const char *msg)
{
	int i;

	if (argv.argc() > 1)
	{
		i = GetKeyFromName (argv[1]);
		if (!i)
		{
			Printf ("Unknown key \"%s\"\n", argv[1]);
			return;
		}
		if (argv.argc() == 2)
		{
			Printf ("\"%s\" = \"%s\"\n", argv[1], Binds[i].GetChars());
		}
		else
		{
			Binds[i] = argv[2];
		}
	}
	else
	{
		Printf ("%s:\n", msg);
		
		for (i = 0; i < NUM_KEYS; i++)
		{
			if (!Binds[i].IsEmpty())
				Printf ("%s \"%s\"\n", KeyName (i), Binds[i].GetChars());
		}
	}
}


//=============================================================================
//
// This function is first called for functions in custom key sections.
// In this case, matchcmd is non-NULL, and only keys bound to that command
// are stored. If a match is found, its binding is set to "\1".
// After all custom key sections are saved, it is called one more for the
// normal Bindings and DoubleBindings sections for this game. In this case
// matchcmd is NULL and all keys will be stored. The config section was not
// previously cleared, so all old bindings are still in place. If the binding
// for a key is empty, the corresponding key in the config is removed as well.
// If a binding is "\1", then the binding itself is cleared, but nothing
// happens to the entry in the config.
//
//=============================================================================

void FKeyBindings::ArchiveBindings(FConfigFile *f, const char *matchcmd)
{
	int i;

	for (i = 0; i < NUM_KEYS; i++)
	{
		if (Binds[i].IsEmpty())
		{
			if (matchcmd == NULL)
			{
				f->ClearKey(ConfigKeyName(i));
			}
		}
		else if (matchcmd == NULL || stricmp(Binds[i], matchcmd) == 0)
		{
			if (Binds[i][0] == '\1')
			{
				Binds[i] = "";
				continue;
			}
			f->SetValueForKey(ConfigKeyName(i), Binds[i]);
			if (matchcmd != NULL)
			{ // If saving a specific command, set a marker so that
			  // it does not get saved in the general binding list.
				Binds[i] = "\1";
			}
		}
	}
}

//=============================================================================
//
//
//
//=============================================================================

int FKeyBindings::GetKeysForCommand (const char *cmd, int *first, int *second)
{
	int c, i;

	*first = *second = c = i = 0;

	while (i < NUM_KEYS && c < 2)
	{
		if (stricmp (cmd, Binds[i]) == 0)
		{
			if (c++ == 0)
				*first = i;
			else
				*second = i;
		}
		i++;
	}
	return c;
}

DEFINE_ACTION_FUNCTION(FKeyBindings, GetKeysForCommand)
{
	PARAM_SELF_STRUCT_PROLOGUE(FKeyBindings);
	PARAM_STRING(cmd);
	int k1, k2;
	self->GetKeysForCommand(cmd.GetChars(), &k1, &k2);
	if (numret > 0) ret[0].SetInt(k1);
	if (numret > 1) ret[1].SetInt(k2);
	return MIN(numret, 2);
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::UnbindACommand (const char *str)
{
	int i;

	for (i = 0; i < NUM_KEYS; i++)
	{
		if (!stricmp (str, Binds[i]))
		{
			Binds[i] = "";
		}
	}
}

DEFINE_ACTION_FUNCTION(FKeyBindings, UnbindACommand)
{
	PARAM_SELF_STRUCT_PROLOGUE(FKeyBindings);
	PARAM_STRING(cmd);
	self->UnbindACommand(cmd);
	return 0;
}

//=============================================================================
//
//
//
//=============================================================================

void FKeyBindings::DefaultBind(const char *keyname, const char *cmd)
{
	int key = GetKeyFromName (keyname);
	if (key == 0)
	{
		Printf ("Unknown key \"%s\"\n", keyname);
		return;
	}
	if (!Binds[key].IsEmpty())
	{ // This key is already bound.
		return;
	}
	for (int i = 0; i < NUM_KEYS; ++i)
	{
		if (!Binds[i].IsEmpty() && stricmp (Binds[i], cmd) == 0)
		{ // This command is already bound to a key.
			return;
		}
	}
	// It is safe to do the bind, so do it.
	Binds[key] = cmd;
}

//=============================================================================
//
//
//
//=============================================================================

void C_UnbindAll ()
{
	Bindings.UnbindAll();
	DoubleBindings.UnbindAll();
	AutomapBindings.UnbindAll();
}

UNSAFE_CCMD (unbindall)
{
	C_UnbindAll ();
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (unbind)
{
	if (argv.argc() > 1)
	{
		Bindings.UnbindKey(argv[1]);
	}
}

CCMD (undoublebind)
{
	if (argv.argc() > 1)
	{
		DoubleBindings.UnbindKey(argv[1]);
	}
}

CCMD (unmapbind)
{
	if (argv.argc() > 1)
	{
		AutomapBindings.UnbindKey(argv[1]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (bind)
{
	Bindings.PerformBind(argv, "Current key bindings");
}

CCMD (doublebind)
{
	DoubleBindings.PerformBind(argv, "Current key doublebindings");
}

CCMD (mapbind)
{
	AutomapBindings.PerformBind(argv, "Current automap key bindings");
}

//==========================================================================
//
// CCMD defaultbind
//
// Binds a command to a key if that key is not already bound and if
// that command is not already bound to another key.
//
//==========================================================================

CCMD (defaultbind)
{
	if (argv.argc() < 3)
	{
		Printf ("Usage: defaultbind <key> <command>\n");
	}
	else
	{
		Bindings.DefaultBind(argv[1], argv[2]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

CCMD (rebind)
{
	FKeyBindings *bindings;

	if (key == 0)
	{
		Printf ("Rebind cannot be used from the console\n");
		return;
	}

	if (key & KEY_DBLCLICKED)
	{
		bindings = &DoubleBindings;
		key &= KEY_DBLCLICKED-1;
	}
	else
	{
		bindings = &Bindings;
	}

	if (argv.argc() > 1)
	{
		bindings->SetBind(key, argv[1]);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void C_BindDefaults ()
{
	int lump, lastlump = 0;

	while ((lump = Wads.FindLump("DEFBINDS", &lastlump)) != -1)
	{
		FScanner sc(lump);

		while (sc.GetString())
		{
			FKeyBindings *dest = &Bindings;
			int key;

			// bind destination is optional and is the same as the console command
			if (sc.Compare("bind"))
			{
				sc.MustGetString();
			}
			else if (sc.Compare("doublebind"))
			{
				dest = &DoubleBindings;
				sc.MustGetString();
			}
			else if (sc.Compare("mapbind"))
			{
				dest = &AutomapBindings;
				sc.MustGetString();
			}
			key = GetConfigKeyFromName(sc.String);
			sc.MustGetString();
			dest->SetBind(key, sc.String);
		}
	}
}

CCMD(binddefaults)
{
	C_BindDefaults ();
}

void C_SetDefaultBindings ()
{
	C_UnbindAll ();
	C_BindDefaults ();
}

//=============================================================================
//
//
//
//=============================================================================

bool C_DoKey (event_t *ev, FKeyBindings *binds, FKeyBindings *doublebinds)
{
	FString binding;
	bool dclick;
	int dclickspot;
	uint8_t dclickmask;
	unsigned int nowtime;

	if (ev->type != EV_KeyDown && ev->type != EV_KeyUp)
		return false;

	if ((unsigned int)ev->data1 >= NUM_KEYS)
		return false;

	dclickspot = ev->data1 >> 3;
	dclickmask = 1 << (ev->data1 & 7);
	dclick = false;

	// This used level.time which didn't work outside a level.
	nowtime = (unsigned)I_msTime();
	if (doublebinds != NULL && int(DClickTime[ev->data1] - nowtime) > 0 && ev->type == EV_KeyDown)
	{
		// Key pressed for a double click
		binding = doublebinds->GetBinding(ev->data1);
		DClicked[dclickspot] |= dclickmask;
		dclick = true;
	}
	else
	{
		if (ev->type == EV_KeyDown)
		{ // Key pressed for a normal press
			binding = binds->GetBinding(ev->data1);
			DClickTime[ev->data1] = nowtime + 571;
		}
		else if (doublebinds != NULL && DClicked[dclickspot] & dclickmask)
		{ // Key released from a double click
			binding = doublebinds->GetBinding(ev->data1);
			DClicked[dclickspot] &= ~dclickmask;
			DClickTime[ev->data1] = 0;
			dclick = true;
		}
		else
		{ // Key released from a normal press
			binding = binds->GetBinding(ev->data1);
		}
	}


	if (binding.IsEmpty())
	{
		binding = binds->GetBinding(ev->data1);
		dclick = false;
	}

	if (!binding.IsEmpty() && (chatmodeon == 0 || ev->data1 < 256))
	{
		if (ev->type == EV_KeyUp && binding[0] != '+')
		{
			return false;
		}

		char *copy = binding.LockBuffer();

		if (ev->type == EV_KeyUp)
		{
			copy[0] = '-';
		}

		AddCommandString (copy, dclick ? ev->data1 | KEY_DBLCLICKED : ev->data1);
		return true;
	}
	return false;
}

