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

#include <stdint.h>

#include "cmdlib.h"
#include "keydef.h"
#include "c_commandline.h"
#include "c_bind.h"
#include "c_dispatch.h"
#include "configfile.h"
#include "filesystem.h"
#include "templates.h"
#include "i_time.h"
#include "printf.h"
#include "sc_man.h"
#include "c_cvars.h"

#include "d_event.h"

extern int chatmodeon;

const char *KeyNames[NUM_KEYS] =
{
	// We use the DirectInput codes and assume a qwerty keyboard layout.
	// See <dinput.h> for the DIK_* codes

	nullptr,	"Escape",	"1",		"2",		"3",		"4",		"5",		"6",		//00
	"7",		"8",		"9",		"0",		"-",		"=",		"Backspace","Tab",		//08
	"Q",		"W",		"E",		"R",		"T",		"Y",		"U",		"I",		//10
	"O",		"P",		"[",		"]",		"Enter",	"Ctrl",		"A",		"S",	//18
	"D",		"F",		"G",		"H",		"J",		"K",		"L",		";",		//20
	"'",		"`",		"Shift",	"\\",		"Z",		"X",		"C",		"V",		//28
	"B",		"N",		"M",		",",		".",		"/",		"RShift",	"KP*",		//30
	"Alt",		"Space",	"CapsLock",	"F1",		"F2",		"F3",		"F4",		"F5",		//38
	"F6",		"F7",		"F8",		"F9",		"F10",		"NumLock",	"Scroll",	"KP7",		//40
	"KP8",		"KP9",		"KP-",		"KP4",		"KP5",		"KP6",		"KP+",		"KP1",		//48
	"KP2",		"KP3",		"KP0",		"KP.",		nullptr,	nullptr,	"OEM102",	"F11",		//50
	"F12",		nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	//58
	nullptr,	nullptr,	nullptr,	nullptr,	"F13",		"F14",		"F15",		"F16",		//60
	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	//68
	"Kana",		nullptr,	nullptr,	"Abnt_C1",	nullptr,	nullptr,	nullptr,	nullptr,	//70
	nullptr,	"Convert",	nullptr,	"NoConvert",nullptr,	"Yen",		"Abnt_C2",	nullptr,	//78
	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	//80
	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	"KP=",		nullptr,	nullptr,	//88
	"Circumflex","@",		":",		"_",		"Kanji",	"Stop",		"Ax",		"Unlabeled",//90
	nullptr,	"PrevTrack",nullptr,	nullptr,	"KP-Enter",	"RCtrl",	nullptr,	nullptr,	//98
	"Mute",		"Calculator","Play",	nullptr,	"Stop",		nullptr,	nullptr,	nullptr,	//A0
	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	"VolDown",	nullptr,	//A8
	"VolUp",	nullptr,	"WebHome",	"KP,",		nullptr,	"KP/",		nullptr,	"SysRq",	//B0
	"RAlt",		nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	//B8
	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	"Pause",	nullptr,	"Home",		//C0
	"UpArrow",	"PgUp",		nullptr,	"LeftArrow",nullptr,	"RightArrow",nullptr,	"End",		//C8
	"DownArrow","PgDn",		"Ins",		"Del",		nullptr,	nullptr,	nullptr,	nullptr,	//D0
#ifdef __APPLE__
	nullptr,	nullptr,	nullptr,	"Command",	nullptr,	"Apps",		"Power",	"Sleep",	//D8
#else // !__APPLE__
	nullptr,	nullptr,	nullptr,	"LWin",		"RWin",		"Apps",		"Power",	"Sleep",	//D8
#endif // __APPLE__
	nullptr,	nullptr,	nullptr,	"Wake",		nullptr,	"Search",	"Favorites","Refresh",	//E0
	"WebStop",	"WebForward","WebBack",	"MyComputer","Mail",	"MediaSelect",nullptr,	nullptr,	//E8
	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	//F0
	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	nullptr,	//F8

	// non-keyboard buttons that can be bound
	"Mouse1",	"Mouse2",	"Mouse3",	"Mouse4",		// 8 mouse buttons
	"Mouse5",	"Mouse6",	"Mouse7",	"Mouse8",

	"Joy1",		"Joy2",		"Joy3",		"Joy4",			// 128 joystick buttons!
	"Joy5",		"Joy6",		"Joy7",		"Joy8",
	"Joy9",		"Joy10",	"Joy11",	"Joy12",
	"Joy13",	"Joy14",	"Joy15",	"Joy16",
	"Joy17",	"Joy18",	"Joy19",	"Joy20",
	"Joy21",	"Joy22",	"Joy23",	"Joy24",
	"Joy25",	"Joy26",	"Joy27",	"Joy28",
	"Joy29",	"Joy30",	"Joy31",	"Joy32",
	"Joy33",	"Joy34",	"Joy35",	"Joy36",
	"Joy37",	"Joy38",	"Joy39",	"Joy40",
	"Joy41",	"Joy42",	"Joy43",	"Joy44",
	"Joy45",	"Joy46",	"Joy47",	"Joy48",
	"Joy49",	"Joy50",	"Joy51",	"Joy52",
	"Joy53",	"Joy54",	"Joy55",	"Joy56",
	"Joy57",	"Joy58",	"Joy59",	"Joy60",
	"Joy61",	"Joy62",	"Joy63",	"Joy64",
	"Joy65",	"Joy66",	"Joy67",	"Joy68",
	"Joy69",	"Joy70",	"Joy71",	"Joy72",
	"Joy73",	"Joy74",	"Joy75",	"Joy76",
	"Joy77",	"Joy78",	"Joy79",	"Joy80",
	"Joy81",	"Joy82",	"Joy83",	"Joy84",
	"Joy85",	"Joy86",	"Joy87",	"Joy88",
	"Joy89",	"Joy90",	"Joy91",	"Joy92",
	"Joy93",	"Joy94",	"Joy95",	"Joy96",
	"Joy97",	"Joy98",	"Joy99",	"Joy100",
	"Joy101",	"Joy102",	"Joy103",	"Joy104",
	"Joy105",	"Joy106",	"Joy107",	"Joy108",
	"Joy109",	"Joy110",	"Joy111",	"Joy112",
	"Joy113",	"Joy114",	"Joy115",	"Joy116",
	"Joy117",	"Joy118",	"Joy119",	"Joy120",
	"Joy121",	"Joy122",	"Joy123",	"Joy124",
	"Joy125",	"Joy126",	"Joy127",	"Joy128",

	"POV1Up",	"POV1Right","POV1Down",	"POV1Left",		// First POV hat
	"POV2Up",	"POV2Right","POV2Down",	"POV2Left",		// Second POV hat
	"POV3Up",	"POV3Right","POV3Down",	"POV3Left",		// Third POV hat
	"POV4Up",	"POV4Right","POV4Down",	"POV4Left",		// Fourth POV hat

	"MWheelUp",	"MWheelDown",							// the mouse wheel
	"MWheelRight", "MWheelLeft",

	"Axis1Plus","Axis1Minus","Axis2Plus","Axis2Minus",	// joystick axes as buttons
	"Axis3Plus","Axis3Minus","Axis4Plus","Axis4Minus",
	"Axis5Plus","Axis5Minus","Axis6Plus","Axis6Minus",
	"Axis7Plus","Axis7Minus","Axis8Plus","Axis8Minus",

	"LStickRight","LStickLeft","LStickDown","LStickUp",			// Gamepad axis-based buttons
	"RStickRight","RStickLeft","RStickDown","RStickUp",

	"DPadUp","DPadDown","DPadLeft","DPadRight",	// Gamepad buttons
	"Pad_Start","Pad_Back","LThumb","RThumb",
	"LShoulder","RShoulder","LTrigger","RTrigger",
	"Pad_A", "Pad_B", "Pad_X", "Pad_Y"
};

FKeyBindings Bindings;
FKeyBindings DoubleBindings;
FKeyBindings AutomapBindings;

static unsigned int DClickTime[NUM_KEYS];
static FixedBitArray<NUM_KEYS> DClicked;

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

const char *KeyName (int key)
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
			strcat (str, TEXTCOLOR_BLACK ", " TEXTCOLOR_NORMAL);
	}

	if (second)
	{
		c++;
		strcat (str, KeyName (second));
	}

	if (!c)
		*str = '\0';
}

//=============================================================================
//
//
//
//=============================================================================

FString C_NameKeys (int *keys, int count, bool colors)
{
	FString result;
	for (int i = 0; i < count; i++)
	{
		int key = keys[i];
		if (key == 0) continue;
		for (int j = 0; j < i; j++)
		{
			if (key == keys[j])
			{
				key = 0;
				break;
			}
		}
		if (key == 0) continue;
		if (result.IsNotEmpty()) result += colors? TEXTCOLOR_BLACK ", " TEXTCOLOR_NORMAL : ", ";
		result += KeyName(key);
	}
	return result;
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
// In this case, matchcmd is non-null, and only keys bound to that command
// are stored. If a match is found, its binding is set to "\1".
// After all custom key sections are saved, it is called one more for the
// normal Bindings and DoubleBindings sections for this game. In this case
// matchcmd is null and all keys will be stored. The config section was not
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
			if (matchcmd == nullptr)
			{
				f->ClearKey(ConfigKeyName(i));
			}
		}
		else if (matchcmd == nullptr || stricmp(Binds[i], matchcmd) == 0)
		{
			if (Binds[i][0] == '\1')
			{
				Binds[i] = "";
				continue;
			}
			f->SetValueForKey(ConfigKeyName(i), Binds[i]);
			if (matchcmd != nullptr)
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

//=============================================================================
//
//
//
//=============================================================================

TArray<int> FKeyBindings::GetKeysForCommand (const char *cmd)
{
	int i = 0;
	TArray<int> result;

	while (i < NUM_KEYS)
	{
		if (stricmp (cmd, Binds[i]) == 0)
		{
			result.Push(i);
		}
		i++;
	}
	return result;
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

CCMD(rebind)
{
	FKeyBindings* bindings;

	if (key == 0)
	{
		Printf("Rebind cannot be used from the console\n");
		return;
	}

	if (key & KEY_DBLCLICKED)
	{
		bindings = &DoubleBindings;
		key &= KEY_DBLCLICKED - 1;
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

void ReadBindings(int lump, bool override)
{
	FScanner sc(lump);

	while (sc.GetString())
	{
		FKeyBindings* dest = &Bindings;
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
		dest->SetBind(key, sc.String, override);
	}
}

//=============================================================================
//
//
//
//=============================================================================

void C_SetDefaultKeys(const char* baseconfig)
{
	auto lump = fileSystem.CheckNumForFullName("engine/commonbinds.txt");
	if (lump >= 0) ReadBindings(lump, true);
	int lastlump = 0;

	while ((lump = fileSystem.FindLumpFullName(baseconfig, &lastlump)) != -1)
	{
		// Read this only from the main game resources.
		if (fileSystem.GetFileContainer(lump) <= fileSystem.GetMaxIwadNum())
			ReadBindings(lump, true);
	}

	lastlump = 0;
	while ((lump = fileSystem.FindLump("DEFBINDS", &lastlump)) != -1)
	{
		// [SW] - We need to check to see the origin of the DEFBINDS... if it
		// Comes from an IWAD/IPK3/IPK7 allow it to override the users settings...
		// If it comes from a user mod however, don't.
		if (fileSystem.GetFileContainer(lump) > fileSystem.GetMaxIwadNum())
			ReadBindings(lump, false);
		else
			ReadBindings(lump, true);
	}
}

//=============================================================================
//
//
//
//=============================================================================
CVAR(Int, cl_defaultconfiguration, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)


void C_BindDefaults()
{
	C_SetDefaultKeys(cl_defaultconfiguration == 1 ? "engine/origbinds.txt" : cl_defaultconfiguration == 2 ? "engine/leftbinds.txt" : "engine/defbinds.txt");
}

void C_SetDefaultBindings()
{
	C_UnbindAll();
	C_BindDefaults();
}


CCMD(controlpreset)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: Controlpreset {0,1,2}\n");
		return;
	}
	int v = atoi(argv[1]);
	if (v < 0 || v > 2) return;
	cl_defaultconfiguration = v;
	C_SetDefaultBindings();
}

CCMD(binddefaults)
{
	C_BindDefaults();
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
	unsigned int nowtime;

	if (ev->type != EV_KeyDown && ev->type != EV_KeyUp)
		return false;

	if ((unsigned int)ev->data1 >= NUM_KEYS)
		return false;

	dclick = false;

	nowtime = (unsigned)I_msTime();
	if (doublebinds != nullptr && int(DClickTime[ev->data1] - nowtime) > 0 && ev->type == EV_KeyDown)
	{
		// Key pressed for a double click
		binding = doublebinds->GetBinding(ev->data1);
		DClicked.Set(ev->data1);
		dclick = true;
	}
	else
	{
		if (ev->type == EV_KeyDown)
		{ // Key pressed for a normal press
			binding = binds->GetBinding(ev->data1);
			DClickTime[ev->data1] = nowtime + 571;
		}
		else if (doublebinds != nullptr && DClicked[ev->data1])
		{ // Key released from a double click
			binding = doublebinds->GetBinding(ev->data1);
			DClicked.Clear(ev->data1);
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

	if (ev->type == EV_KeyUp && binding[0] != '+')
	{
		return false;
	}

	if (!binding.IsEmpty() && (chatmodeon == 0 || ev->data1 < 256))
	{
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

