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
#include "configfile.h"

static long ParseCommandLine (const char *args, int *argc, char **argv);

CVAR (Bool, lookspring, true, CVAR_ARCHIVE);	// Generate centerview when -mlook encountered?

class DWaitingCommand : public DThinker
{
	DECLARE_CLASS (DWaitingCommand, DThinker)
public:
	DWaitingCommand (const char *cmd, int tics);
	~DWaitingCommand ();
	void Serialize (FArchive &arc);
	void RunThink ();

private:
	DWaitingCommand ();

	char *Command;
	int TicsLeft;
};

class DStoredCommand : public DThinker
{
	DECLARE_CLASS (DStoredCommand, DThinker)
public:
	DStoredCommand (int argc, char **argv, const char *args);
	~DStoredCommand ();
	void RunThink ();

private:
	DStoredCommand ();

	int ArgC;
	char **ArgV;
	char *ArgS;
};

static FConsoleCommand *FindNameInHashTable (FConsoleCommand **table, const char *name);
static FConsoleCommand *ScanChainForName (FConsoleCommand *start, const char *name, FConsoleCommand **prev);

FConsoleCommand *Commands[HASH_SIZE];

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

IMPLEMENT_CLASS (DWaitingCommand)

void DWaitingCommand::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Command << TicsLeft;
}

DWaitingCommand::DWaitingCommand ()
{
	Command = NULL;
	TicsLeft = 1;
}

DWaitingCommand::DWaitingCommand (const char *cmd, int tics)
{
	Command = copystring (cmd);
	TicsLeft = tics;
}

DWaitingCommand::~DWaitingCommand ()
{
	if (Command != NULL)
	{
		delete[] Command;
	}
}

void DWaitingCommand::RunThink ()
{
	if (--TicsLeft == 0)
	{
		AddCommandString (Command);
		Destroy ();
	}
}

IMPLEMENT_CLASS (DStoredCommand)

DStoredCommand::DStoredCommand ()
{
	Destroy ();
}

DStoredCommand::DStoredCommand (int argc, char **argv, const char *args)
{
	ArgC = argc;
	if (argc != 0)
	{
		int len;
		int i;

		for (i = len = 0; i < argc; i++)
		{
			len += strlen (argv[i]) + 1;
		}
		ArgV = new char *[argc];
		ArgV[0] = new char[len];
		ArgV[0][0] = 0;
		for (i = len = 0; i < argc; i++)
		{
			ArgV[i] = ArgV[0] + len;
			strcpy (ArgV[0] + len, argv[i]);
			len += strlen (argv[i]) + 1;
		}
		ArgS = new char[strlen (args) + 1];
		strcpy (ArgS, args);
	}
}		

DStoredCommand::~DStoredCommand ()
{
	if (ArgC != 0)
	{
		delete[] ArgV[0];
		delete[] ArgV;
		delete[] ArgS;
	}
}

void DStoredCommand::RunThink ()
{
	if (ArgC != 0)
	{
		FConsoleCommand *com = FindNameInHashTable (Commands, ArgV[0]);
		if (com != NULL)
		{
			com->Run (ArgC, ArgV, ArgS, players[consoleplayer].mo);
		}
	}
	Destroy ();
}

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
// for a matching key and returns an index or -1 if
// the key could not be found. This uses binary search,
// actionbits[] must be sorted in ascending order.

int GetActionBit (unsigned int key)
{
	int min = 0;
	int max = NUM_ACTIONS - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		unsigned int seekey = actionbits[mid].key;

		if (seekey == key)
			return actionbits[mid].index;
		else if (seekey < key)
			min = mid + 1;
		else
			max = mid - 1;
	}
	
	return -1;
}

void C_DoCommand (char *cmd)
{
	int argc;
	long argsize;
	char **argv;
	FConsoleCommand *com;

	while (*cmd && *cmd <= ' ')
		cmd++;

	// Check if this is an action
	if (*cmd == '+' || *cmd == '-')
	{
		int action;
		char *end = cmd+1;
		char brk;

		while (*end && *end > ' ')
			end++;
		brk = *end;
		*end = 0;
		action = GetActionBit (MakeKey (cmd + 1));
		*end = brk;
		if (action >= 0)
		{
			if (*cmd == '+')
			{
				//if (Actions[check] < 255)
				//	Actions[check]++;
				Actions[action] = 1;
			}
			else
			{
				//if (Actions[check])
				//	Actions[check]--;
				Actions[action] = 0;
				if (action == ACTION_MLOOK && *lookspring)
				{
					AddCommandString ("centerview");
				}
			}
			return;
		}
	}
	
	// Parse it as a normal command
	argsize = ParseCommandLine (cmd, &argc, NULL);
	argv = (char **)Malloc (argc*sizeof(char *) + argsize);
	argv[0] = (char *)argv + argc*sizeof(char *);
	ParseCommandLine (cmd, NULL, argv);

	// Checking for matching commands follows this search order:
	//	1. Check the Commands[] hash table
	//	2. Check the CVars list

	if ( (com = FindNameInHashTable (Commands, argv[0])) )
	{
		if (gamestate != GS_STARTUP ||
			stricmp (argv[0], "set") == 0 ||
			stricmp (argv[0], "logfile") == 0 ||
			stricmp (argv[0], "unbindall") == 0 ||
			stricmp (argv[0], "exec") == 0)
		{
			com->Run (argc, argv, cmd, players[consoleplayer].mo);
		}
		else
		{
			new DStoredCommand (argc, argv, cmd);
		}
	}
	else
	{
		// Check for any console vars that match the command
		FBaseCVar *var;

		if ( (var = FindCVar (argv[0], NULL)) )
		{
			if (argc >= 2)
			{ // Hack
				com = FindNameInHashTable (Commands, "set");
				com->Run (argc + 1, argv - 1, cmd, players[consoleplayer].mo);
			}
			else
			{
				UCVarValue val = var->GetGenericRep (CVAR_String);
				Printf (PRINT_HIGH, "\"%s\" is \"%s\"\n",
					var->GetName(), val);
			}
		}
		else
		{
			// We don't know how to handle this command
			Printf (PRINT_HIGH, "Unknown command \"%s\"\n", argv[0]);
		}
	}
	free (argv);
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
					while (*brkpt != '\0' && (*brkpt != '\"' || *(brkpt-1) == '\\'))
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
			// Intercept wait commands here
			while (*cmd && *cmd <= ' ')
				cmd++;
			if (*cmd)
			{
				if (strnicmp (cmd, "wait", 4) == 0 && (cmd[4] == 0 || cmd[4] == ' '))
				{
					int tics;

					if (cmd[4] == ' ')
					{
						tics = strtol (cmd + 5, NULL, 0);
					}
					else
					{
						tics = 1;
					}
					if (tics > 0)
					{
						if (more)
						{ // The remainder of the command will be executed later
							*brkpt = ';';
							new DWaitingCommand (brkpt + 1, tics);
						}
						return;
					}
				}
				else
				{
					C_DoCommand (cmd);
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
// Special processing: Inside quoted strings, \" becomes just "
// $<cvar> is replaced by the contents of <cvar>

static long ParseCommandLine (const char *args, int *argc, char **argv)
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
				if (stuff == '\\' && *args == '\"')
				{
					stuff = '\"', args++;
				}
				else if (stuff == '\"')
				{
					stuff = 0;
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
			if (*start == '$' && (var = FindCVar (start+1, NULL)))
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
	return (long)buffplace;
}

static FConsoleCommand *ScanChainForName (FConsoleCommand *start, const char *name, FConsoleCommand **prev)
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

static FConsoleCommand *FindNameInHashTable (FConsoleCommand **table, const char *name)
{
	FConsoleCommand *dummy;

	return ScanChainForName (table[MakeKey (name) % HASH_SIZE], name, &dummy);
}

bool FConsoleCommand::AddToHash (FConsoleCommand **table)
{
	unsigned int key;
	FConsoleCommand *insert, **bucket;

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

FConsoleCommand::FConsoleCommand (const char *name, CCmdRun runFunc)
	: m_RunFunc (runFunc)
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
		Printf (PRINT_HIGH, "FConsoleCommand c'tor: %s exists\n", name);
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

void FConsoleCommand::Run (int argc, char **argv, const char *args, AActor *instigator)
{
	m_RunFunc (argc, argv, args, instigator);
}

FConsoleAlias::FConsoleAlias (const char *name, const char *command)
	: FConsoleCommand (name, NULL)
{
	m_Command = copystring (command);
}

FConsoleAlias::~FConsoleAlias ()
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

static int DumpHash (FConsoleCommand **table, BOOL aliases)
{
	int bucket, count;
	FConsoleCommand *cmd;

	for (bucket = count = 0; bucket < HASH_SIZE; bucket++)
	{
		cmd = table[bucket];
		while (cmd)
		{
			count++;
			if (cmd->IsAlias())
			{
				if (aliases)
					static_cast<FConsoleAlias *>(cmd)->PrintAlias ();
			}
			else if (!aliases)
				cmd->PrintCommand ();
			cmd = cmd->m_Next;
		}
	}
	return count;
}

void FConsoleAlias::Archive (FConfigFile *f)
{
	if (f != NULL)
	{
		f->SetValueForKey ("Name", m_Name, true);
		f->SetValueForKey ("Command", m_Command, true);
	}
}

void C_ArchiveAliases (FConfigFile *f)
{
	int bucket;
	FConsoleCommand *alias;

	for (bucket = 0; bucket < HASH_SIZE; bucket++)
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

void C_SetAlias (const char *name, const char *cmd)
{
	FConsoleCommand *prev, *alias, **chain;

	chain = &Commands[MakeKey (name) % HASH_SIZE];
	alias = ScanChainForName (*chain, name, &prev);
	if (alias != NULL)
	{
		delete alias;
	}
	new FConsoleAlias (name, cmd);
}

CCMD (alias)
{
	FConsoleCommand *prev, *alias, **chain;

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
				new FConsoleAlias (argv[1], BuildString (argc - 2, &argv[2]));
			else
				new FConsoleAlias (argv[1], copystring (argv[2]));
		}
	}
}

CCMD (cmdlist)
{
	int count;

	count = ListActionCommands ();
	count += DumpHash (Commands, false);
	Printf (PRINT_HIGH, "%d commands\n", count);
}

CCMD (key)
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

// Execute any console commands specified on the command line.
// These all begin with '+' as opposed to '-'.
// If DispatchSetOnly is true, only "set" commands will be executed,
// otherwise only non-"set" commands are executed.
void C_ExecCmdLineParams ()
{
	for (int currArg = 1; currArg < Args.NumArgs(); )
	{
		if (*Args.GetArg (currArg++) == '+')
		{
			char *cmdString;
			int cmdlen = 1;
			int argstart = currArg - 1;

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

bool FConsoleCommand::IsAlias ()
{
	return false;
}

bool FConsoleAlias::IsAlias ()
{
	return true;
}

void FConsoleAlias::Run (int argc, char **argv, const char *args, AActor *m_Instigator)
{
	AddCommandString (m_Command);
}
