/*
** c_dispatch.cpp
** Functions for executing console commands and aliases
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "templates.h"
#include "doomtype.h"
#include "cmdlib.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "m_argv.h"
#include "doomstat.h"
#include "d_player.h"
#include "configfile.h"
#include "m_crc32.h"
#include "v_text.h"
#include "d_net.h"
#include "d_main.h"
#include "serializer.h"
#include "menu/menu.h"
#include "vm.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class DWaitingCommand : public DThinker
{
	DECLARE_CLASS (DWaitingCommand, DThinker)
public:
	DWaitingCommand (const char *cmd, int tics);
	~DWaitingCommand ();
	void Serialize(FSerializer &arc);
	void Tick ();

private:
	DWaitingCommand ();

	char *Command;
	int TicsLeft;
};

class DStoredCommand : public DThinker
{
	DECLARE_CLASS (DStoredCommand, DThinker)
public:
	DStoredCommand (FConsoleCommand *com, const char *cmd);
	~DStoredCommand ();
	void Tick ();

private:
	DStoredCommand ();

	FConsoleCommand *Command;
	char *Text;
};

struct FActionMap
{
	FButtonStatus	*Button;
	unsigned int	Key;	// value from passing Name to MakeKey()
	char			Name[12];
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static long ParseCommandLine (const char *args, int *argc, char **argv, bool no_escapes);
static FConsoleCommand *FindNameInHashTable (FConsoleCommand **table, const char *name, size_t namelen);
static FConsoleCommand *ScanChainForName (FConsoleCommand *start, const char *name, size_t namelen, FConsoleCommand **prev);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool, lookspring, true, CVAR_ARCHIVE);	// Generate centerview when -mlook encountered?

FConsoleCommand *Commands[FConsoleCommand::HASH_SIZE];
FButtonStatus Button_Mlook, Button_Klook, Button_Use, Button_AltAttack,
	Button_Attack, Button_Speed, Button_MoveRight, Button_MoveLeft,
	Button_Strafe, Button_LookDown, Button_LookUp, Button_Back,
	Button_Forward, Button_Right, Button_Left, Button_MoveDown,
	Button_MoveUp, Button_Jump, Button_ShowScores, Button_Crouch,
	Button_Zoom, Button_Reload,
	Button_User1, Button_User2, Button_User3, Button_User4,
	Button_AM_PanLeft, Button_AM_PanRight, Button_AM_PanDown, Button_AM_PanUp,
	Button_AM_ZoomIn, Button_AM_ZoomOut;

bool ParsingKeyConf, ParsingMenuDef = false;

// To add new actions, go to the console and type "key <action name>".
// This will give you the key value to use in the first column. Then
// insert your new action into this list so that the keys remain sorted
// in ascending order. No two keys can be identical. If yours matches
// an existing key, change the name of your action.

FActionMap ActionMaps[] =
{
	{ &Button_AM_PanLeft,	0x0d52d67b, "am_panleft"},
	{ &Button_User2,		0x125f5226, "user2" },
	{ &Button_Jump,			0x1eefa611, "jump" },
	{ &Button_Right,		0x201f1c55, "right" },
	{ &Button_Zoom,			0x20ccc4d5, "zoom" },
	{ &Button_Back,			0x23a99cd7, "back" },
	{ &Button_AM_ZoomIn,	0x41df90c2, "am_zoomin"},
	{ &Button_Reload,		0x426b69e7, "reload" },
	{ &Button_LookDown,		0x4463f43a, "lookdown" },
	{ &Button_AM_ZoomOut,	0x51f7a334, "am_zoomout"},
	{ &Button_User4,		0x534c30ee, "user4" },
	{ &Button_Attack,		0x5622bf42, "attack" },
	{ &Button_User1,		0x577712d0, "user1" },
	{ &Button_Klook,		0x57c25cb2, "klook" },
	{ &Button_Forward,		0x59f3e907, "forward" },
	{ &Button_MoveDown,		0x6167ce99, "movedown" },
	{ &Button_AltAttack,	0x676885b8, "altattack" },
	{ &Button_MoveLeft,		0x6fa41b84, "moveleft" },
	{ &Button_MoveRight,	0x818f08e6, "moveright" },
	{ &Button_AM_PanRight,	0x8197097b, "am_panright"},
	{ &Button_AM_PanUp,		0x8d89955e, "am_panup"} ,
	{ &Button_Mlook,		0xa2b62d8b, "mlook" },
	{ &Button_Crouch,		0xab2c3e71, "crouch" },
	{ &Button_Left,			0xb000b483, "left" },
	{ &Button_LookUp,		0xb62b1e49, "lookup" },
	{ &Button_User3,		0xb6f8fe92, "user3" },
	{ &Button_Strafe,		0xb7e6a54b, "strafe" },
	{ &Button_AM_PanDown,	0xce301c81, "am_pandown"},
	{ &Button_ShowScores,	0xd5897c73, "showscores" },
	{ &Button_Speed,		0xe0ccb317, "speed" },
	{ &Button_Use,			0xe0cfc260, "use" },
	{ &Button_MoveUp,		0xfdd701c7, "moveup" },
};
#define NUM_ACTIONS countof(ActionMaps)


// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *KeyConfCommands[] =
{
	"alias",
	"defaultbind",
	"addkeysection",
	"addmenukey",
	"addslotdefault",
	"weaponsection",
	"setslot",
	"addplayerclass",
	"clearplayerclasses"
};

static const char *MenuDefCommands[] =
{
	"snd_reset",
	"reset2defaults",
	"reset2saved",
	"menuconsole",
	"clearnodecache",
	"am_restorecolors",
	"undocolorpic",
	"special",
	"puke",
	"fpuke",
	"pukename",
	"event",
	"netevent",
	"openmenu"
};

// CODE --------------------------------------------------------------------

IMPLEMENT_CLASS(DWaitingCommand, false, false)

void DWaitingCommand::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("command", Command)
		("ticsleft", TicsLeft);
}

DWaitingCommand::DWaitingCommand ()
{
	Command = NULL;
	TicsLeft = 1;
}

DWaitingCommand::DWaitingCommand (const char *cmd, int tics)
{
	Command = copystring (cmd);
	TicsLeft = tics+1;
}

DWaitingCommand::~DWaitingCommand ()
{
	if (Command != NULL)
	{
		delete[] Command;
	}
}

void DWaitingCommand::Tick ()
{
	if (--TicsLeft == 0)
	{
		AddCommandString (Command);
		Destroy ();
	}
}

IMPLEMENT_CLASS(DStoredCommand, false, false)

DStoredCommand::DStoredCommand ()
{
	Text = NULL;
	Destroy ();
}

DStoredCommand::DStoredCommand (FConsoleCommand *command, const char *args)
{
	Command = command;
	Text = copystring (args);
}		

DStoredCommand::~DStoredCommand ()
{
	if (Text != NULL)
	{
		delete[] Text;
	}
}

void DStoredCommand::Tick ()
{
	if (Text != NULL && Command != NULL)
	{
		FCommandLine args (Text);
		Command->Run (args, players[consoleplayer].mo, 0);
	}
	Destroy ();
}

static int ListActionCommands (const char *pattern)
{
	char matcher[16];
	unsigned int i;
	int count = 0;

	for (i = 0; i < NUM_ACTIONS; ++i)
	{
		if (pattern == NULL || CheckWildcards (pattern,
			(mysnprintf (matcher, countof(matcher), "+%s", ActionMaps[i].Name), matcher)))
		{
			Printf ("+%s\n", ActionMaps[i].Name);
			count++;
		}
		if (pattern == NULL || CheckWildcards (pattern,
			(mysnprintf (matcher, countof(matcher), "-%s", ActionMaps[i].Name), matcher)))
		{
			Printf ("-%s\n", ActionMaps[i].Name);
			count++;
		}
	}
	return count;
}

/* ======================================================================== */

/* By Paul Hsieh (C) 2004, 2005.  Covered under the Paul Hsieh derivative 
   license. See: 
   http://www.azillionmonkeys.com/qed/weblicense.html for license details.

   http://www.azillionmonkeys.com/qed/hash.html */

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

uint32_t SuperFastHash (const char *data, size_t len)
{
	uint32_t hash = 0, tmp;
	size_t rem;

	if (len == 0 || data == NULL) return 0;

	rem = len & 3;
	len >>= 2;

	/* Main loop */
	for (;len > 0; len--)
	{
		hash  += get16bits (data);
		tmp    = (get16bits (data+2) << 11) ^ hash;
		hash   = (hash << 16) ^ tmp;
		data  += 2*sizeof (uint16_t);
		hash  += hash >> 11;
	}

	/* Handle end cases */
	switch (rem)
	{
		case 3:	hash += get16bits (data);
				hash ^= hash << 16;
				hash ^= data[sizeof (uint16_t)] << 18;
				hash += hash >> 11;
				break;
		case 2:	hash += get16bits (data);
				hash ^= hash << 11;
				hash += hash >> 17;
				break;
		case 1: hash += *data;
				hash ^= hash << 10;
				hash += hash >> 1;
	}

	/* Force "avalanching" of final 127 bits */
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;

	return hash;
}

/* A modified version to do a case-insensitive hash */

#undef get16bits
#define get16bits(d) ((((uint32_t)tolower(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)tolower(((const uint8_t *)(d))[0]) )

uint32_t SuperFastHashI (const char *data, size_t len)
{
	uint32_t hash = 0, tmp;
	size_t rem;

	if (len <= 0 || data == NULL) return 0;

	rem = len & 3;
	len >>= 2;

	/* Main loop */
	for (;len > 0; len--)
	{
		hash  += get16bits (data);
		tmp    = (get16bits (data+2) << 11) ^ hash;
		hash   = (hash << 16) ^ tmp;
		data  += 2*sizeof (uint16_t);
		hash  += hash >> 11;
	}

	/* Handle end cases */
	switch (rem)
	{
		case 3:	hash += get16bits (data);
				hash ^= hash << 16;
				hash ^= tolower(data[sizeof (uint16_t)]) << 18;
				hash += hash >> 11;
				break;
		case 2:	hash += get16bits (data);
				hash ^= hash << 11;
				hash += hash >> 17;
				break;
		case 1: hash += tolower(*data);
				hash ^= hash << 10;
				hash += hash >> 1;
	}

	/* Force "avalanching" of final 127 bits */
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;

	return hash;
}

/* ======================================================================== */

unsigned int MakeKey (const char *s)
{
	if (s == NULL)
	{
		return 0;
	}
	return SuperFastHashI (s, strlen (s));
}

unsigned int MakeKey (const char *s, size_t len)
{
	return SuperFastHashI (s, len);
}

// FindButton scans through the actionbits[] array
// for a matching key and returns an index or -1 if
// the key could not be found. This uses binary search,
// so actionbits[] must be sorted in ascending order.

FButtonStatus *FindButton (unsigned int key)
{
	const FActionMap *bit;

	bit = BinarySearch<FActionMap, unsigned int>
			(ActionMaps, NUM_ACTIONS, &FActionMap::Key, key);
	return bit ? bit->Button : NULL;
}

bool FButtonStatus::PressKey (int keynum)
{
	int i, open;

	keynum &= KEY_DBLCLICKED-1;

	if (keynum == 0)
	{ // Issued from console instead of a key, so force on
		Keys[0] = 0xffff;
		for (i = MAX_KEYS-1; i > 0; --i)
		{
			Keys[i] = 0;
		}
	}
	else
	{
		for (i = MAX_KEYS-1, open = -1; i >= 0; --i)
		{
			if (Keys[i] == 0)
			{
				open = i;
			}
			else if (Keys[i] == keynum)
			{ // Key is already down; do nothing
				return false;
			}
		}
		if (open < 0)
		{ // No free key slots, so do nothing
			Printf ("More than %u keys pressed for a single action!\n", MAX_KEYS);
			return false;
		}
		Keys[open] = keynum;
	}
	uint8_t wasdown = bDown;
	bDown = bWentDown = true;
	// Returns true if this key caused the button to go down.
	return !wasdown;
}

bool FButtonStatus::ReleaseKey (int keynum)
{
	int i, numdown, match;
	uint8_t wasdown = bDown;

	keynum &= KEY_DBLCLICKED-1;

	if (keynum == 0)
	{ // Issued from console instead of a key, so force off
		for (i = MAX_KEYS-1; i >= 0; --i)
		{
			Keys[i] = 0;
		}
		bWentUp = true;
		bDown = false;
	}
	else
	{
		for (i = MAX_KEYS-1, numdown = 0, match = -1; i >= 0; --i)
		{
			if (Keys[i] != 0)
			{
				++numdown;
				if (Keys[i] == keynum)
				{
					match = i;
				}
			}
		}
		if (match < 0)
		{ // Key was not down; do nothing
			return false;
		}
		Keys[match] = 0;
		bWentUp = true;
		if (--numdown == 0)
		{
			bDown = false;
		}
	}
	// Returns true if releasing this key caused the button to go up.
	return wasdown && !bDown;
}

void ResetButtonTriggers ()
{
	for (int i = NUM_ACTIONS-1; i >= 0; --i)
	{
		ActionMaps[i].Button->ResetTriggers ();
	}
}

void ResetButtonStates ()
{
	for (int i = NUM_ACTIONS-1; i >= 0; --i)
	{
		FButtonStatus *button = ActionMaps[i].Button;

		if (button != &Button_Mlook && button != &Button_Klook)
		{
			button->ReleaseKey (0);
		}
		button->ResetTriggers ();
	}
}

void C_DoCommand (const char *cmd, int keynum)
{
	FConsoleCommand *com;
	const char *end;
	const char *beg;

	// Skip any beginning whitespace
	while (*cmd && *cmd <= ' ')
		cmd++;

	// Find end of the command name
	if (*cmd == '\"')
	{
		for (end = beg = cmd+1; *end && *end != '\"'; ++end)
			;
	}
	else
	{
		beg = cmd;
		for (end = cmd+1; *end > ' '; ++end)
			;
	}

	const size_t len = end - beg;

	if (ParsingKeyConf)
	{
		int i;

		for (i = countof(KeyConfCommands)-1; i >= 0; --i)
		{
			if (strnicmp (beg, KeyConfCommands[i], len) == 0 &&
				KeyConfCommands[i][len] == 0)
			{
				break;
			}
		}
		if (i < 0)
		{
			Printf ("Invalid command for KEYCONF: %s\n", beg);
			return;
		}
	}

	if (ParsingMenuDef)
	{
		int i;

		for (i = countof(MenuDefCommands)-1; i >= 0; --i)
		{
			if (strnicmp (beg, MenuDefCommands[i], len) == 0 &&
				MenuDefCommands[i][len] == 0)
			{
				break;
			}
		}
		if (i < 0)
		{
			Printf ("Invalid command for MENUDEF/ZScript: %s\n", beg);
			return;
		}
	}

	// Check if this is an action
	if (*beg == '+' || *beg == '-')
	{
		FButtonStatus *button;

		button = FindButton (MakeKey (beg + 1, end - beg - 1));
		if (button != NULL)
		{
			if (*beg == '+')
			{
				button->PressKey (keynum);
			}
			else
			{
				button->ReleaseKey (keynum);
				if (button == &Button_Mlook && lookspring)
				{
					Net_WriteByte (DEM_CENTERVIEW);
				}
			}
			return;
		}
	}
	
	// Parse it as a normal command
	// Checking for matching commands follows this search order:
	//	1. Check the Commands[] hash table
	//	2. Check the CVars list

	if ( (com = FindNameInHashTable (Commands, beg, len)) )
	{
		if (gamestate != GS_STARTUP || ParsingKeyConf ||
			(len == 3 && strnicmp (beg, "set", 3) == 0) ||
			(len == 7 && strnicmp (beg, "logfile", 7) == 0) ||
			(len == 9 && strnicmp (beg, "unbindall", 9) == 0) ||
			(len == 4 && strnicmp (beg, "bind", 4) == 0) ||
			(len == 4 && strnicmp (beg, "exec", 4) == 0) ||
			(len ==10 && strnicmp (beg, "doublebind", 10) == 0) ||
			(len == 6 && strnicmp (beg, "pullin", 6) == 0)
			)
		{
			FCommandLine args (beg);
			com->Run (args, players[consoleplayer].mo, keynum);
		}
		else
		{
			if (len == 4 && strnicmp(beg, "warp", 4) == 0)
			{
				StoredWarp = beg;
			}
			else
			{
				Create<DStoredCommand> (com, beg);
			}
		}
	}
	else
	{ // Check for any console vars that match the command
		FBaseCVar *var = FindCVarSub (beg, int(len));

		if (var != NULL)
		{
			FCommandLine args (beg);

			if (args.argc() >= 2)
			{ // Set the variable
				var->CmdSet (args[1]);
			}
			else
			{ // Get the variable's value
				Printf ("\"%s\" is \"%s\"\n", var->GetName(), var->GetHumanString());
			}
		}
		else
		{ // We don't know how to handle this command
			Printf ("Unknown command \"%.*s\"\n", (int)len, beg);
		}
	}
}

// This is only accessible to the special menu item to run CCMDs.
DEFINE_ACTION_FUNCTION(DOptionMenuItemCommand, DoCommand)
{
	if (CurrentMenu == nullptr) return 0;
	PARAM_PROLOGUE;
	PARAM_STRING(cmd);
	ParsingMenuDef = true;
	C_DoCommand(cmd);
	ParsingMenuDef = false;
	return 0;
}

void AddCommandString (char *cmd, int keynum)
{
	char *brkpt;
	int more;

	if (cmd)
	{
		while (*cmd)
		{
			brkpt = cmd;
			while (*brkpt != ';' && *brkpt != '\0')
			{
				if (*brkpt == '\"')
				{
					brkpt++;
					while (*brkpt != '\0' && (*brkpt != '\"' || *(brkpt-1) == '\\'))
						brkpt++;
				}
				if (*brkpt != '\0')
					brkpt++;
			}
			if (*brkpt == ';')
			{
				*brkpt = '\0';
				more = 1;
			}
			else
			{
				more = 0;
			}
			// Intercept wait commands here. Note: wait must be lowercase
			while (*cmd && *cmd <= ' ')
				cmd++;
			if (*cmd)
			{
				if (!ParsingKeyConf &&
					cmd[0] == 'w' && cmd[1] == 'a' && cmd[2] == 'i' && cmd[3] == 't' &&
					(cmd[4] == 0 || cmd[4] == ' '))
				{
					int tics;

					if (cmd[4] == ' ')
					{
						tics = (int)strtoll (cmd + 5, NULL, 0);
					}
					else
					{
						tics = 1;
					}
					if (tics > 0)
					{
						if (more)
						{ // The remainder of the command will be executed later
						  // Note that deferred commands lose track of which key
						  // (if any) they were pressed from.
							*brkpt = ';';
							Create<DWaitingCommand> (brkpt, tics);
						}
						return;
					}
				}
				else
				{
					C_DoCommand (cmd, keynum);
				}
			}
			if (more)
			{
				*brkpt = ';';
			}
			cmd = brkpt + more;
		}
	}
}

// ParseCommandLine
//
// Parse a command line (passed in args). If argc is non-NULL, it will
// be set to the number of arguments. If argv is non-NULL, it will be
// filled with pointers to each argument; argv[0] should be initialized
// to point to a buffer large enough to hold all the arguments. The
// return value is the necessary size of this buffer.
//
// Special processing:
//   Inside quoted strings, \" becomes just "
//                          \\ becomes just a single backslash          
//							\c becomes just TEXTCOLOR_ESCAPE
//   $<cvar> is replaced by the contents of <cvar>

static long ParseCommandLine (const char *args, int *argc, char **argv, bool no_escapes)
{
	int count;
	char *buffplace;

	count = 0;
	buffplace = NULL;
	if (argv != NULL)
	{
		buffplace = argv[0];
	}

	for (;;)
	{
		while (*args <= ' ' && *args)
		{ // skip white space
			args++;
		}
		if (*args == 0)
		{
			break;
		}
		else if (*args == '\"')
		{ // read quoted string
			char stuff;
			if (argv != NULL)
			{
				argv[count] = buffplace;
			}
			count++;
			args++;
			do
			{
				stuff = *args++;
				if (!no_escapes && stuff == '\\' && *args == '\"')
				{
					stuff = '\"', args++;
				}
				else if (!no_escapes && stuff == '\\' && *args == '\\')
				{
					args++;
				}
				else if (!no_escapes && stuff == '\\' && *args == 'c')
				{
					stuff = TEXTCOLOR_ESCAPE, args++;
				}
				else if (stuff == '\"')
				{
					stuff = 0;
				}
				else if (stuff == 0)
				{
					args--;
				}
				if (argv != NULL)
				{
					*buffplace = stuff;
				}
				buffplace++;
			} while (stuff);
		}
		else
		{ // read unquoted string
			const char *start = args++, *end;
			FBaseCVar *var;
			UCVarValue val;

			while (*args && *args > ' ' && *args != '\"')
				args++;
			if (*start == '$' && (var = FindCVarSub (start+1, int(args-start-1))))
			{
				val = var->GetGenericRep (CVAR_String);
				start = val.String;
				end = start + strlen (start);
			}
			else
			{
				end = args;
			}
			if (argv != NULL)
			{
				argv[count] = buffplace;
				while (start < end)
					*buffplace++ = *start++;
				*buffplace++ = 0;
			}
			else
			{
				buffplace += end - start + 1;
			}
			count++;
		}
	}
	if (argc != NULL)
	{
		*argc = count;
	}
	return (long)(buffplace - (char *)0);
}

FCommandLine::FCommandLine (const char *commandline, bool no_escapes)
{
	cmd = commandline;
	_argc = -1;
	_argv = NULL;
	noescapes = no_escapes;
}

FCommandLine::~FCommandLine ()
{
	if (_argv != NULL)
	{
		delete[] _argv;
	}
}

void FCommandLine::Shift()
{
	// Only valid after _argv has been filled.
	for (int i = 1; i < _argc; ++i)
	{
		_argv[i - 1] = _argv[i];
	}
}

int FCommandLine::argc ()
{
	if (_argc == -1)
	{
		argsize = ParseCommandLine (cmd, &_argc, NULL, noescapes);
	}
	return _argc;
}

char *FCommandLine::operator[] (int i)
{
	if (_argv == NULL)
	{
		int count = argc();
		_argv = new char *[count + (argsize+sizeof(char*)-1)/sizeof(char*)];
		_argv[0] = (char *)_argv + count*sizeof(char *);
		ParseCommandLine (cmd, NULL, _argv, noescapes);
	}
	return _argv[i];
}

static FConsoleCommand *ScanChainForName (FConsoleCommand *start, const char *name, size_t namelen, FConsoleCommand **prev)
{
	int comp;

	*prev = NULL;
	while (start)
	{
		comp = strnicmp (start->m_Name, name, namelen);
		if (comp > 0)
			return NULL;
		else if (comp == 0 && start->m_Name[namelen] == 0)
			return start;

		*prev = start;
		start = start->m_Next;
	}
	return NULL;
}

static FConsoleCommand *FindNameInHashTable (FConsoleCommand **table, const char *name, size_t namelen)
{
	FConsoleCommand *dummy;

	return ScanChainForName (table[MakeKey (name, namelen) % FConsoleCommand::HASH_SIZE], name, namelen, &dummy);
}

bool FConsoleCommand::AddToHash (FConsoleCommand **table)
{
	unsigned int key;
	FConsoleCommand *insert, **bucket;

	key = MakeKey (m_Name);
	bucket = &table[key % HASH_SIZE];

	if (ScanChainForName (*bucket, m_Name, strlen (m_Name), &insert))
	{
		return false;
	}
	else
	{
		if (insert)
		{
			m_Next = insert->m_Next;
			if (m_Next)
				m_Next->m_Prev = &m_Next;
			insert->m_Next = this;
			m_Prev = &insert->m_Next;
		}
		else
		{
			m_Next = *bucket;
			*bucket = this;
			m_Prev = bucket;
			if (m_Next)
				m_Next->m_Prev = &m_Next;
		}
	}
	return true;
}

FConsoleCommand* FConsoleCommand::FindByName (const char* name)
{
	return FindNameInHashTable (Commands, name, strlen (name));
}

FConsoleCommand::FConsoleCommand (const char *name, CCmdRun runFunc)
	: m_RunFunc (runFunc)
{
	static bool firstTime = true;

	if (firstTime)
	{
		char tname[16];
		unsigned int i;

		firstTime = false;

		// Add all the action commands for tab completion
		for (i = 0; i < NUM_ACTIONS; i++)
		{
			strcpy (&tname[1], ActionMaps[i].Name);
			tname[0] = '+';
			C_AddTabCommand (tname);
			tname[0] = '-';
			C_AddTabCommand (tname);
		}
	}

	int ag = strcmp (name, "kill");
	if (ag == 0)
		ag=0;
	m_Name = copystring (name);

	if (!AddToHash (Commands))
		Printf ("FConsoleCommand c'tor: %s exists\n", name);
	else
		C_AddTabCommand (name);
}

FConsoleCommand::~FConsoleCommand ()
{
	*m_Prev = m_Next;
	if (m_Next)
		m_Next->m_Prev = m_Prev;
	C_RemoveTabCommand (m_Name);
	delete[] m_Name;
}

void FConsoleCommand::Run (FCommandLine &argv, APlayerPawn *who, int key)
{
	m_RunFunc (argv, who, key);
}

FConsoleAlias::FConsoleAlias (const char *name, const char *command, bool noSave)
	: FConsoleCommand (name, NULL),
	  bRunning(false), bKill(false)
{
	m_Command[noSave] = command;
	m_Command[!noSave] = FString();
	// If the command contains % characters, assume they are parameter markers
	// for substitution when the command is executed.
	bDoSubstitution = (strchr (command, '%') != NULL);
}

FConsoleAlias::~FConsoleAlias ()
{
	m_Command[1] = m_Command[0] = FString();
}

// Given an argument vector, reconstitute the command line it could have been produced from.
FString BuildString (int argc, FString *argv)
{
	if (argc == 1)
	{
		return *argv;
	}
	else
	{
		FString buf;
		int arg;

		for (arg = 0; arg < argc; arg++)
		{
			if (argv[arg][0] == '\0')
			{ // It's an empty argument, we need to convert it to '""'
				buf << "\"\" ";
			}
			else if (strchr(argv[arg], '"'))
			{ // If it contains one or more quotes, we need to escape them.
				buf << '"';
				long substr_start = 0, quotepos;
				while ((quotepos = argv[arg].IndexOf('"', substr_start)) >= 0)
				{
					if (substr_start < quotepos)
					{
						buf << argv[arg].Mid(substr_start, quotepos - substr_start);
					}
					buf << "\\\"";
					substr_start = quotepos + 1;
				}
				buf << argv[arg].Mid(substr_start) << "\" ";
			}
			else if (strchr(argv[arg], ' '))
			{ // If it contains a space, it needs to be quoted.
				buf << '"' << argv[arg] << "\" ";
			}
			else
			{
				buf << argv[arg] << ' ';
			}
		}
		return buf;
	}
}

//===========================================================================
//
// SubstituteAliasParams
//
// Given an command line and a set of arguments, replace instances of
// %x or %{x} in the command line with argument x. If argument x does not
// exist, then the empty string is substituted in its place.
//
// Substitution is not done inside of quoted strings, unless that string is
// prepended with a % character.
//
// To avoid a substitution, use %%. The %% will be replaced by a single %.
//
//===========================================================================

FString SubstituteAliasParams (FString &command, FCommandLine &args)
{
	// Do substitution by replacing %x with the argument x.
	// If there is no argument x, then %x is simply removed.

	// For some reason, strtoul's stop parameter is non-const.
	char *p = command.LockBuffer(), *start = p;
	unsigned long argnum;
	FString buf;
	bool inquote = false;

	while (*p != '\0')
	{
		if (p[0] == '%' && ((p[1] >= '0' && p[1] <= '9') || p[1] == '{'))
		{
			// Do a substitution. Output what came before this.
			buf.AppendCStrPart (start, p - start);

			// Extract the argument number and substitute the corresponding argument.
			argnum = strtoul (p + 1 + (p[1] == '{'), &start, 10);
			if ((p[1] != '{' || *start == '}') && argnum < (unsigned long)args.argc())
			{
				buf += args[argnum];
			}
			p = (start += (p[1] == '{' && *start == '}'));
		}
		else if (p[0] == '%' && p[1] == '%')
		{
			// Do not substitute. Just collapse to a single %.
			buf.AppendCStrPart (start, p - start + 1);
			start = p = p + 2;
			continue;
		}
		else if (p[0] == '%' && p[1] == '"')
		{
			// Collapse %" to " and remember that we're in a quote so when we
			// see a " character again, we don't start skipping below.
			if (!inquote)
			{
				inquote = true;
				buf.AppendCStrPart(start, p - start);
				start = p + 1;
			}
			else
			{
				inquote = false;
			}
			p += 2;
		}
		else if (p[0] == '\\' && p[1] == '"')
		{
			p += 2;
		}
		else if (p[0] == '"')
		{
			// Don't substitute inside quoted strings if it didn't start
			// with a %"
			if (!inquote)
			{
				p++;
				while (*p != '\0' && (*p != '"' || *(p-1) == '\\'))
					p++;
				if (*p != '\0')
					p++;
			}
			else
			{
				inquote = false;
				p++;
			}
		}
		else
		{
			p++;
		}
	}
	// Return whatever was after the final substitution.
	if (p > start)
	{
		buf.AppendCStrPart (start, p - start);
	}
	command.UnlockBuffer();

	return buf;
}

static int DumpHash (FConsoleCommand **table, bool aliases, const char *pattern=NULL)
{
	int bucket, count;
	FConsoleCommand *cmd;

	for (bucket = count = 0; bucket < FConsoleCommand::HASH_SIZE; bucket++)
	{
		cmd = table[bucket];
		while (cmd)
		{
			if (CheckWildcards (pattern, cmd->m_Name))
			{
				if (cmd->IsAlias())
				{
					if (aliases)
					{
						++count;
						static_cast<FConsoleAlias *>(cmd)->PrintAlias ();
					}
				}
				else if (!aliases)
				{
					++count;
					cmd->PrintCommand ();
				}
			}
			cmd = cmd->m_Next;
		}
	}
	return count;
}

void FConsoleAlias::PrintAlias ()
{
	if (m_Command[0].IsNotEmpty())
	{
		Printf (TEXTCOLOR_YELLOW "%s : %s\n", m_Name, m_Command[0].GetChars());
	}
	if (m_Command[1].IsNotEmpty())
	{
		Printf (TEXTCOLOR_ORANGE "%s : %s\n", m_Name, m_Command[1].GetChars());
	}
}

void FConsoleAlias::Archive (FConfigFile *f)
{
	if (f != NULL && !m_Command[0].IsEmpty())
	{
		f->SetValueForKey ("Name", m_Name, true);
		f->SetValueForKey ("Command", m_Command[0], true);
	}
}

void C_ArchiveAliases (FConfigFile *f)
{
	int bucket;
	FConsoleCommand *alias;

	for (bucket = 0; bucket < FConsoleCommand::HASH_SIZE; bucket++)
	{
		alias = Commands[bucket];
		while (alias)
		{
			if (alias->IsAlias())
				static_cast<FConsoleAlias *>(alias)->Archive (f);
			alias = alias->m_Next;
		}
	}
}

void C_ClearAliases ()
{
	int bucket;
	FConsoleCommand *alias;

	for (bucket = 0; bucket < FConsoleCommand::HASH_SIZE; bucket++)
	{
		alias = Commands[bucket];
		while (alias)
		{
			FConsoleCommand *next = alias->m_Next;
			if (alias->IsAlias())
				static_cast<FConsoleAlias *>(alias)->SafeDelete();
			alias = next;
		}
	}
}

CCMD(clearaliases)
{
	C_ClearAliases();
}


// This is called only by the ini parser.
void C_SetAlias (const char *name, const char *cmd)
{
	FConsoleCommand *prev, *alias, **chain;

	chain = &Commands[MakeKey (name) % FConsoleCommand::HASH_SIZE];
	alias = ScanChainForName (*chain, name, strlen (name), &prev);
	if (alias != NULL)
	{
		if (!alias->IsAlias ())
		{
			//Printf (PRINT_BOLD, "%s is a command and cannot be an alias.\n", name);
			return;
		}
		delete alias;
	}
	new FConsoleAlias (name, cmd, false);
}

CCMD (alias)
{
	FConsoleCommand *prev, *alias, **chain;

	if (argv.argc() == 1)
	{
		Printf ("Current alias commands:\n");
		DumpHash (Commands, true);
	}
	else
	{
		chain = &Commands[MakeKey (argv[1]) % FConsoleCommand::HASH_SIZE];

		if (argv.argc() == 2)
		{ // Remove the alias

			if ( (alias = ScanChainForName (*chain, argv[1], strlen (argv[1]), &prev)))
			{
				if (alias->IsAlias ())
				{
					static_cast<FConsoleAlias *> (alias)->SafeDelete ();
				}
				else
				{
					Printf ("%s is a normal command\n", alias->m_Name);
				}
			}
		}
		else
		{ // Add/change the alias

			alias = ScanChainForName (*chain, argv[1], strlen (argv[1]), &prev);
			if (alias != NULL)
			{
				if (alias->IsAlias ())
				{
					static_cast<FConsoleAlias *> (alias)->Realias (argv[2], ParsingKeyConf);
				}
				else
				{
					Printf ("%s is a normal command\n", alias->m_Name);
					alias = NULL;
				}
			}
			else
			{
				new FConsoleAlias (argv[1], argv[2], ParsingKeyConf);
			}
		}
	}
}

CCMD (cmdlist)
{
	int count;
	const char *filter = (argv.argc() == 1 ? NULL : argv[1]);

	count = ListActionCommands (filter);
	count += DumpHash (Commands, false, filter);
	Printf ("%d commands\n", count);
}

CCMD (key)
{
	if (argv.argc() > 1)
	{
		int i;

		for (i = 1; i < argv.argc(); ++i)
		{
			unsigned int key = MakeKey (argv[i]);
			Printf (" 0x%08x\n", key);
		}
	}
}

// Execute any console commands specified on the command line.
// These all begin with '+' as opposed to '-'.
FExecList *C_ParseCmdLineParams(FExecList *exec)
{
	for (int currArg = 1; currArg < Args->NumArgs(); )
	{
		if (*Args->GetArg (currArg++) == '+')
		{
			FString cmdString;
			int cmdlen = 1;
			int argstart = currArg - 1;

			while (currArg < Args->NumArgs())
			{
				if (*Args->GetArg (currArg) == '-' || *Args->GetArg (currArg) == '+')
					break;
				currArg++;
				cmdlen++;
			}

			cmdString = BuildString (cmdlen, Args->GetArgList (argstart));
			if (!cmdString.IsEmpty())
			{
				if (exec == NULL)
				{
					exec = new FExecList;
				}
				exec->AddCommand(&cmdString[1]);
			}
		}
	}
	return exec;
}

bool FConsoleCommand::IsAlias ()
{
	return false;
}

bool FConsoleAlias::IsAlias ()
{
	return true;
}

void FConsoleAlias::Run (FCommandLine &args, APlayerPawn *who, int key)
{
	if (bRunning)
	{
		Printf ("Alias %s tried to recurse.\n", m_Name);
		return;
	}

	int index = !m_Command[1].IsEmpty();
	FString savedcommand = m_Command[index], mycommand;
	m_Command[index] = FString();

	if (bDoSubstitution)
	{
		mycommand = SubstituteAliasParams (savedcommand, args);
	}
	else
	{
		mycommand = savedcommand;
	}

	bRunning = true;
	AddCommandString (mycommand.LockBuffer(), key);
	mycommand.UnlockBuffer();
	bRunning = false;
	if (m_Command[index].IsEmpty())
	{ // The alias is unchanged, so put the command back so it can be used again.
	  // If the command had been non-empty, then that means that executing this
	  // alias caused it to realias itself, so the old command will be forgotten
	  // once this function returns.
		m_Command[index] = savedcommand;
	}
	if (bKill)
	{ // The alias wants to remove itself
		delete this;
	}
}

void FConsoleAlias::Realias (const char *command, bool noSave)
{
	if (!noSave && !m_Command[1].IsEmpty())
	{
		noSave = true;
	}
	m_Command[noSave] = command;

	// If the command contains % characters, assume they are parameter markers
	// for substitution when the command is executed.
	bDoSubstitution = (strchr (command, '%') != NULL);
	bKill = false;
}

void FConsoleAlias::SafeDelete ()
{
	if (!bRunning)
	{
		delete this;
	}
	else
	{
		bKill = true;
	}
}

void FExecList::AddCommand(const char *cmd, const char *file)
{
	// Pullins are special and need to be separated from general commands.
	// They also turned out to be a really bad idea, since they make things
	// more complicated. :(
	if (file != NULL && strnicmp(cmd, "pullin", 6) == 0 && isspace(cmd[6]))
	{
		FCommandLine line(cmd);
		C_SearchForPullins(this, file, line);
	}
	// Recursive exec: Parse this file now.
	else if (strnicmp(cmd, "exec", 4) == 0 && isspace(cmd[4]))
	{
		FCommandLine argv(cmd);
		for (int i = 1; i < argv.argc(); ++i)
		{
			C_ParseExecFile(argv[i], this);
		}
	}
	else
	{
		Commands.Push(cmd);
	}
}

void FExecList::ExecCommands() const
{
	for (unsigned i = 0; i < Commands.Size(); ++i)
	{
		AddCommandString(Commands[i].LockBuffer());
		Commands[i].UnlockBuffer();
	}
}

void FExecList::AddPullins(TArray<FString> &wads) const
{
	for (unsigned i = 0; i < Pullins.Size(); ++i)
	{
		D_AddFile(wads, Pullins[i]);
	}
}

FExecList *C_ParseExecFile(const char *file, FExecList *exec)
{
	FILE *f;
	char cmd[4096];
	int retval = 0;

	if ( (f = fopen (file, "r")) )
	{
		while (fgets(cmd, countof(cmd)-1, f))
		{
			// Comments begin with //
			char *stop = cmd + strlen(cmd) - 1;
			char *comment = cmd;
			int inQuote = 0;

			if (*stop == '\n')
				*stop-- = 0;

			while (comment < stop)
			{
				if (*comment == '\"')
				{
					inQuote ^= 1;
				}
				else if (!inQuote && *comment == '/' && *(comment + 1) == '/')
				{
					break;
				}
				comment++;
			}
			if (comment == cmd)
			{ // Comment at line beginning
				continue;
			}
			else if (comment < stop)
			{ // Comment in middle of line
				*comment = 0;
			}
			if (exec == NULL)
			{
				exec = new FExecList;
			}
			exec->AddCommand(cmd, file);
		}
		if (!feof(f))
		{
			Printf("Error parsing \"%s\"\n", file);
		}
		fclose(f);
	}
	else
	{
		Printf ("Could not open \"%s\"\n", file);
	}
	return exec;
}

bool C_ExecFile (const char *file)
{
	FExecList *exec = C_ParseExecFile(file, NULL);
	if (exec != NULL)
	{
		exec->ExecCommands();
		if (exec->Pullins.Size() > 0)
		{
			Printf(TEXTCOLOR_BOLD "Notice: Pullin files were ignored.\n");
		}
		delete exec;
	}
	return exec != NULL;
}

void C_SearchForPullins(FExecList *exec, const char *file, FCommandLine &argv)
{
	const char *lastSlash;

	assert(exec != NULL);
	assert(file != NULL);
#ifdef __unix__
	lastSlash = strrchr(file, '/');
#else
	const char *lastSlash1, *lastSlash2;

	lastSlash1 = strrchr(file, '/');
	lastSlash2 = strrchr(file, '\\');
	lastSlash = MAX(lastSlash1, lastSlash2);
#endif

	for (int i = 1; i < argv.argc(); ++i)
	{
		// Try looking for the wad in the same directory as the .cfg
		// before looking for it in the current directory.
		if (lastSlash != NULL)
		{
			FString path(file, (lastSlash - file) + 1);
			path += argv[i];
			if (FileExists(path))
			{
				exec->Pullins.Push(path);
				continue;
			}
		}
		exec->Pullins.Push(argv[i]);
	}
}

CCMD (pullin)
{
	// Actual handling for pullin is now completely special-cased above
	Printf (TEXTCOLOR_BOLD "Pullin" TEXTCOLOR_NORMAL " is only valid from .cfg\n"
			"files and only when used at startup.\n");
}

