#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "doomtype.h"
#include "cmdlib.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "m_argv.h"
#include "doomstat.h"
#include "m_alloc.h"
#include "d_player.h"

IMPLEMENT_CLASS (DConsoleCommand, DObject)
IMPLEMENT_CLASS (DConsoleAlias, DConsoleCommand)

CVAR (lookspring, "1", CVAR_ARCHIVE);	// Generate centerview when -mlook encountered?

static DConsoleCommand *FindNameInHashTable (DConsoleCommand **table, const char *name);
static DConsoleCommand *ScanChainForName (DConsoleCommand *start, const char *name, DConsoleCommand **prev);

DConsoleCommand *Commands[HASH_SIZE];

struct ActionBits actionbits[NUM_ACTIONS] =
{
	{ 0x00409, ACTION_USE,		  "use" },
	{ 0x0074d, ACTION_BACK,		  "back" },
	{ 0x007e4, ACTION_LEFT,		  "left" },
	{ 0x00816, ACTION_JUMP,		  "jump" },
	{ 0x0106d, ACTION_KLOOK,	  "klook" },
	{ 0x0109d, ACTION_MLOOK,	  "mlook" },
	{ 0x010d8, ACTION_RIGHT,	  "right" },
	{ 0x0110a, ACTION_SPEED,	  "speed" },
	{ 0x01fc5, ACTION_ATTACK,	  "attack" },
	{ 0x021ae, ACTION_LOOKUP,	  "lookup" },
	{ 0x021fe, ACTION_MOVEUP,	  "moveup" },
	{ 0x02315, ACTION_STRAFE,	  "strafe" },
	{ 0x041c4, ACTION_FORWARD,	  "forward" },
	{ 0x08788, ACTION_LOOKDOWN,	  "lookdown" },
	{ 0x088c4, ACTION_MOVELEFT,	  "moveleft" },
	{ 0x088c8, ACTION_MOVEDOWN,	  "movedown" },
	{ 0x11268, ACTION_MOVERIGHT,  "moveright" },
	{ 0x2314d, ACTION_SHOWSCORES, "showscores" }
};
byte Actions[NUM_ACTIONS];

static int ListActionCommands (void)
{
	int i;

	for (i = 0; i < NUM_ACTIONS; i++)
	{
		Printf (PRINT_HIGH, "+%s\n", actionbits[i].name);
		Printf (PRINT_HIGH, "-%s\n", actionbits[i].name);
	}
	return NUM_ACTIONS * 2;
}

unsigned int MakeKey (const char *s)
{
	register unsigned int v = 0;

	if (*s)
		v = tolower(*s++);
	if (*s)
		v = (v*3) + tolower(*s++);
	while (*s)
		v = (v << 1) + tolower(*s++);

	return v;
}

// GetActionBit scans through the actionbits[] array
// for a matching key and returns a bit mask for it
// or 0x0000 if the key could not be found.
// actionbits[] must be sorted in ascending order for
// this function to work properly.
//
// Question: is this worth the trouble?

int GetActionBit (unsigned int key)
{
	int min = 0;
	int max = NUM_ACTIONS - 1;
	int mid = NUM_ACTIONS / 2;
	int smalltimes = 0;

	do
	{
		if (actionbits[mid].key == key)
		{
			return actionbits[mid].index;
		}
		else if (actionbits[mid].key < key)
		{
			min = mid;
		}
		else if (actionbits[mid].key > key)
		{
			max = mid;
		}
		if (max - min > 1)
		{
			mid = (max - min) / 2 + min;
		}
		else if (!smalltimes)
		{
			smalltimes++;
			mid = (max - mid) + min;
		}
		else
		{
			break;
		}
	} while (max - min > 0);

	if (actionbits[min].key == key) 
		return actionbits[mid].index;
	
	return -1;
}

void C_DoCommand (char *cmd)
{
	int argc, argsize;
	char **argv;
	char *args, *arg, *realargs;
	char *data;
	DConsoleCommand *com;
	int check = -1;

	data = ParseString (cmd);
	if (!data)
		return;

	// Check if this is an action
	if (*com_token == '+')
	{
		check = GetActionBit (MakeKey (com_token + 1));
		//if (Actions[check] < 255)
		//	Actions[check]++;
		Actions[check] = 1;
	}
	else if (*com_token == '-')
	{
		check = GetActionBit (MakeKey (com_token + 1));
		//if (Actions[check])
		//	Actions[check]--;
		Actions[check] = 0;
		if (check == ACTION_MLOOK && lookspring.value)
		{
			AddCommandString ("centerview");
		}
	}
	
	// Check if this is a normal command
	if (check == -1)
	{
		argc = 1;
		argsize = strlen (com_token) + 1;

		realargs = new char[strlen (data) + 1];
		strcpy (realargs, data);

		while ( (data = ParseString (data)) )
		{
			argc++;
			argsize += strlen (com_token) + 1;
		}

		args = new char[argsize];
		argv = new char *[argc];

		arg = args;
		data = cmd;
		argsize = 0;
		while ( (data = ParseString (data)) )
		{
			strcpy (arg, com_token);
			argv[argsize] = arg;
			arg += strlen (arg);
			*arg++ = 0;
			argsize++;
		}

		// Checking for matching commands follows this search order:
		//	1. Check the Commands[] hash table
		//	2. Check the CVars list

		if ( (com = FindNameInHashTable (Commands, argv[0])) )
		{
			com->argc = argc;
			com->argv = argv;
			com->args = realargs;
			com->m_Instigator = players[consoleplayer].mo;
			com->Run ();
		}
		else
		{
			// Check for any CVars that match the command
			cvar_t *var, *dummy;

			if ( (var = FindCVar (argv[0], &dummy)) )
			{
				if (argc >= 2)
				{
					com = FindNameInHashTable (Commands, "set");
					com->argc = argc + 1;
					com->argv = argv - 1;	// Hack
					com->m_Instigator = players[consoleplayer].mo;
					com->Run ();
				}
				else
				{
					Printf (PRINT_HIGH, "\"%s\" is \"%s\"\n", var->name, var->string);
				}
			}
			else
			{
				// We don't know how to handle this command
				Printf (PRINT_HIGH, "Unknown command \"%s\"\n", argv[0]);
			}
		}
		delete[] argv;
		delete[] args;
		delete[] realargs;
	}
}

void AddCommandString (char *cmd)
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
					while (*brkpt != '\"' && *brkpt != '\0')
						brkpt++;
				}
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
			C_DoCommand (cmd);
			if (more)
			{
				*brkpt = ';';
			}
			cmd = brkpt + more;
		}
	}
}

// ParseString2 is adapted from COM_Parse
// found in the Quake2 source distribution
char *ParseString2 (char *data)
{
	int c;
	int len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
		{
			return NULL;			// end of string encountered
		}
		data++;
	}
	
// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1) {
			c = *data++;
			if (c == '\"' || c == '\0')
			{
				if (c == '\0')
					data--;
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse a regular word
	do {
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

// ParseString calls ParseString2 to remove the first
// token from an input string. If this token is of
// the form $<cvar>, it will be replaced by the
// contents of <cvar>.
char *ParseString (char *data) 
{
	cvar_t *var, *dummy;

	if ( (data = ParseString2 (data)) )
	{
		if (com_token[0] == '$')
		{
			if ( (var = FindCVar (&com_token[1], &dummy)) )
			{
				strcpy (com_token, var->string);
			}
		}
	}
	return data;
}

static DConsoleCommand *ScanChainForName (DConsoleCommand *start, const char *name, DConsoleCommand **prev)
{
	int comp;

	*prev = NULL;
	while (start)
	{
		comp = stricmp (start->m_Name, name);
		if (comp > 0)
			return NULL;
		else if (comp == 0)
			return start;

		*prev = start;
		start = start->m_Next;
	}
	return NULL;
}

static DConsoleCommand *FindNameInHashTable (DConsoleCommand **table, const char *name)
{
	DConsoleCommand *dummy;

	return ScanChainForName (table[MakeKey (name) % HASH_SIZE], name, &dummy);
}

bool DConsoleCommand::AddToHash (DConsoleCommand **table)
{
	unsigned int key;
	DConsoleCommand *insert, **bucket;

	if (!stricmp (m_Name, "toggle"))
		key = 1;

	key = MakeKey (m_Name);
	bucket = &table[key % HASH_SIZE];

	if (ScanChainForName (*bucket, m_Name, &insert))
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

DConsoleCommand::DConsoleCommand (const char *name)
{
	static bool firstTime = true;

	if (firstTime)
	{
		char tname[16];
		int i;

		firstTime = false;

		// Add all the action commands for tab completion
		for (i = 0; i < NUM_ACTIONS; i++)
		{
			strcpy (&tname[1], actionbits[i].name);
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
		Printf (PRINT_HIGH, "DConsoleCommand c'tor: %s exists\n", name);
	else
		C_AddTabCommand (name);
}

DConsoleCommand::~DConsoleCommand ()
{
	*m_Prev = m_Next;
	if (m_Next)
		m_Next->m_Prev = m_Prev;
	C_RemoveTabCommand (m_Name);
	delete[] m_Name;
}

DConsoleAlias::DConsoleAlias (const char *name, const char *command)
	: DConsoleCommand (name)
{
	m_Command = copystring (command);
}

DConsoleAlias::~DConsoleAlias ()
{
	delete[] m_Command;
}

char *BuildString (int argc, char **argv)
{
	char temp[1024];
	char *cur;
	int arg;

	if (argc == 1)
	{
		return copystring (*argv);
	}
	else
	{
		cur = temp;
		for (arg = 0; arg < argc; arg++)
		{
			if (strchr (argv[arg], ' '))
			{
				cur += sprintf (cur, "\"%s\" ", argv[arg]);
			}
			else
			{
				cur += sprintf (cur, "%s ", argv[arg]);
			}
		}
		temp[strlen (temp) - 1] = 0;
		return copystring (temp);
	}
}

static int DumpHash (DConsoleCommand **table, BOOL aliases)
{
	int bucket, count;
	DConsoleCommand *cmd;

	for (bucket = count = 0; bucket < HASH_SIZE; bucket++)
	{
		cmd = table[bucket];
		while (cmd)
		{
			count++;
			if (cmd->IsAlias())
			{
				if (aliases)
					static_cast<DConsoleAlias *>(cmd)->PrintAlias ();
			}
			else if (!aliases)
				cmd->PrintCommand ();
			cmd = cmd->m_Next;
		}
	}
	return count;
}

void DConsoleAlias::Archive (FILE *f)
{
	fprintf (f, "alias \"%s\" \"%s\"\n", m_Name, m_Command);
}

void C_ArchiveAliases (FILE *f)
{
	int bucket;
	DConsoleCommand *alias;

	for (bucket = 0; bucket < HASH_SIZE; bucket++)
	{
		alias = Commands[bucket];
		while (alias)
		{
			if (alias->IsAlias())
				static_cast<DConsoleAlias *>(alias)->Archive (f);
			alias = alias->m_Next;
		}
	}
}

BEGIN_COMMAND (alias)
{
	DConsoleCommand *prev, *alias, **chain;

	if (argc == 1)
	{
		Printf (PRINT_HIGH, "Current alias commands:\n");
		DumpHash (Commands, true);
	}
	else
	{
		chain = &Commands[MakeKey (argv[1]) % HASH_SIZE];

		if (argc == 2)
		{
			// Remove the alias

			if ( (alias = ScanChainForName (*chain, argv[1], &prev)) )
			{
				delete alias;
			}
		}
		else
		{
			// Add/Change the alias

			alias = ScanChainForName (*chain, argv[1], &prev);
			if (alias)
				delete alias;

			if (argc > 3)
				new DConsoleAlias (argv[1], BuildString (argc - 2, &argv[2]));
			else
				new DConsoleAlias (argv[1], copystring (argv[2]));
		}
	}
}
END_COMMAND (alias)

BEGIN_COMMAND (cmdlist)
{
	int count;

	count = ListActionCommands ();
	count += DumpHash (Commands, false);
	Printf (PRINT_HIGH, "%d commands\n", count);
}
END_COMMAND (cmdlist)

BEGIN_COMMAND (key)
{
	if (argc > 1)
	{
		while (argc > 1)
		{
			Printf (PRINT_HIGH, " %08x", MakeKey (argv[1]));
			argc--;
			argv++;
		}
		Printf (PRINT_HIGH, "\n");
	}
}
END_COMMAND (key)

// Execute any console commands specified on the command line.
// These all begin with '+' as opposed to '-'.
// If onlyset is true, only "set" commands will be executed,
// otherwise only non-"set" commands are executed.
void C_ExecCmdLineParams (int onlyset)
{
	int currArg, setComp, cmdlen, argstart;
	char *cmdString;

	for (currArg = 1; currArg < Args.NumArgs(); )
	{
		if (*Args.GetArg (currArg++) == '+')
		{
			setComp = stricmp (Args.GetArg (currArg - 1) + 1, "set");
			if ((onlyset && setComp) || (!onlyset && !setComp))
			{
				continue;
			}

			cmdlen = 1;
			argstart = currArg - 1;

			while (currArg < Args.NumArgs())
			{
				if (*Args.GetArg (currArg) == '-' || *Args.GetArg (currArg) == '+')
					break;
				currArg++;
				cmdlen++;
			}

			if ( (cmdString = BuildString (cmdlen, Args.GetArgList (argstart))) )
			{
				C_DoCommand (cmdString + 1);
				delete[] cmdString;
			}
		}
	}
}