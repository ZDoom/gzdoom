#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "doomtype.h"
#include "cmdlib.h"
#include "c_consol.h"
#include "c_cmds.h"
#include "c_dispch.h"
#include "m_argv.h"
#include "doomstat.h"
#include "m_alloc.h"

cvar_t *lookspring;			// Generate centerview when -mlook encountered?

static struct CmdData *FindNameInHashTable (struct CmdData **table, char *name);
static struct CmdData *ScanChainForName (struct CmdData *start, char *name, struct CmdData **prev);

struct CmdData *Commands[HASH_SIZE];
struct CmdData *Aliases[HASH_SIZE];

struct ActionBits actionbits[NUM_ACTIONS] = {
	{ 0x116716a2, ACTION_SPEED,		 "speed" },
	{ 0x1e60dad4, ACTION_RIGHT,		 "right" },
	{ 0x206c10aa, ACTION_KLOOK,		 "klook" },
	{ 0x226a20ae, ACTION_MLOOK,		 "mlook" },
	{ 0x4d63cc88, ACTION_USE,		 "use" },
	{ 0x50183a64, ACTION_SHOWSCORES, "showscores" },
	{ 0x620a3c5e, ACTION_MOVELEFT,	 "moveleft" },
	{ 0x6d157234, ACTION_LOOKDOWN,	 "lookdown" },
	{ 0x6f03745a, ACTION_MOVEDOWN,	 "movedown" },
	{ 0x78086cf0, ACTION_ATTACK,	 "attack" },
	{ 0x8517869e, ACTION_STRAFE,	 "strafe" },
	{ 0x910b0cac, ACTION_BACK,		 "back" },
	{ 0x9a02b460, ACTION_LOOKUP,	 "lookup" },
	{ 0x9c14ae86, ACTION_MOVEUP,	 "moveup" },
	{ 0xab1b201e, ACTION_LEFT,		 "left" },
	{ 0xbc02544e, ACTION_JUMP,		 "jump" },
	{ 0xd571f880, ACTION_MOVERIGHT,	 "moveright" },
	{ 0xf57be8e2, ACTION_FORWARD,	 "forward" }
};
int Actions;

static int ListActionCommands (void)
{
	int i;

	for (i = 0; i < NUM_ACTIONS; i++) {
		Printf ("+%s\n", actionbits[i].name);
		Printf ("-%s\n", actionbits[i].name);
	}
	return NUM_ACTIONS * 2;
}

unsigned int MakeKey (char *s)
{
	byte a,b,c,d,e;

	a = b = c = d = 0;

	while (*s) {
		e = tolower (*s++);
		a += e;
		b ^= e;
		c += a - b;
		d += a + b;
	}

	return (a << 24) | (b << 16) | (c << 8) | d;
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

	do {
		if (actionbits[mid].key == key) {
			return actionbits[mid].bit;
		} else if (actionbits[mid].key < key) {
			min = mid;
		} else if (actionbits[mid].key > key) {
			max = mid;
		}
		if (max - min > 1) {
			mid = (max - min) / 2 + min;
		} else if (!smalltimes) {
			smalltimes++;
			mid = (max - mid) + min;
		} else {
			break;
		}
	} while (max - min > 0);

	if (actionbits[min].key == key) 
		return actionbits[mid].bit;
	
	return 0;
}

void C_DoCommand (char *cmd)
{
	int argc, argsize;
	char **argv, *args, *arg;
	char *data;
	struct CmdData *com;
	int check = 0;

	data = ParseString (cmd);
	if (!data)
		return;

	// Check if this is an action
	if (*com_token == '+') {
		check = GetActionBit (MakeKey (com_token + 1));
		Actions |= check;
	} else if (*com_token == '-') {
		check = GetActionBit (MakeKey (com_token + 1));
		Actions &= ~check;
		if (check == ACTION_MLOOK) {
			if (lookspring->value) {
				Cmd_CenterView (&players[consoleplayer], 0, NULL);
			}
		}
	}
	
	// Check if this is a normal command
	if (check == 0) {
		argc = 1;
		argsize = strlen (com_token) + 1;

		while ( (data = ParseString (data)) ) {
			argc++;
			argsize += strlen (com_token) + 1;
		}

		args = Malloc (argsize);
		argv = Malloc (sizeof (char *) * argc);

		arg = args;
		data = cmd;
		argsize = 0;
		while ( (data = ParseString (data)) ) {
			strcpy (arg, com_token);
			argv[argsize] = arg;
			arg += strlen (arg);
			*arg++ = 0;
			argsize++;
		}

		// Checking for matching commands follows this search order:
		//	1. Check the Commands[] hash table
		//	2. Check the Aliases[] hash table
		//	3. Check the CVars list

		if ( (com = FindNameInHashTable (Commands, argv[0])) ) {
			com->call.func (&players[consoleplayer], argc, argv);
		} else if ( (com = FindNameInHashTable (Aliases, argv[0])) ) {
			AddCommandString (com->call.command);
		} else {
			// Check for any CVars that match the command
			cvar_t *var, *dummy;

			if ( (var = FindCVar (argv[0], &dummy)) ) {
				if (argc >= 2) {
					// Hack
					Cmd_Set (&players[consoleplayer], argc + 1, argv - 1);
				} else {
					Printf ("\"%s\" is \"%s\"\n", var->name, var->string);
				}
			} else {
				// We don't know how to handle this command
				Printf ("Unknown command \"%s\"\n", argv[0]);
			}
		}
		free (argv);
		free (args);
	}
}

void AddCommandString (char *cmd)
{
	char *brkpt;
	int more;

	while (*cmd) {
		brkpt = cmd;
		while (*brkpt != ';' && *brkpt != '\0') {
			if (*brkpt == '\"') {
				brkpt++;
				while (*brkpt != '\"' && *brkpt != '\0')
					brkpt++;
			}
			brkpt++;
		}
		if (*brkpt == ';') {
			*brkpt = '\0';
			more = 1;
		} else {
			more = 0;
		}
		C_DoCommand (cmd);
		if (more) {
			*brkpt = ';';
		}
		cmd = brkpt + more;
	}
}

// ParseString2 is adapted from COM_Parse
// found in the Quake2 source distribution
char *ParseString2 (char *data)
{
	int		c;
	int		len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
	while ( (c = *data) <= ' ')	{
		if (c == 0) {
			return NULL;			// end of string encountered
		}
		data++;
	}
	
// handle quoted strings specially
	if (c == '\"') {
		data++;
		while (1) {
			c = *data++;
			if (c == '\"' || c == '\0') {
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

	if ( (data = ParseString2 (data)) ) {
		if (com_token[0] == '$') {
			if ( (var = FindCVar (&com_token[1], &dummy)) ) {
				strcpy (com_token, var->string);
			}
		}
	}
	return data;
}

static struct CmdData *ScanChainForName (struct CmdData *start, char *name, struct CmdData **prev)
{
	int comp;

	*prev = NULL;
	while (start) {
		comp = stricmp (start->name, name);
		if (comp > 0)
			return NULL;
		else if (comp == 0)
			return start;

		*prev = start;
		start = start->next;
	}
	return NULL;
}

static struct CmdData *FindNameInHashTable (struct CmdData **table, char *name)
{
	struct CmdData *dummy;

	return ScanChainForName (table[MakeKey (name) % HASH_SIZE], name, &dummy);
}

static BOOL AddToHash (struct CmdData **table, char *name, void *data)
{
	unsigned int key;
	struct CmdData *insert;

	if (!stricmp (name, "toggle"))
		key = 1;

	key = MakeKey (name);

	if (ScanChainForName (table[key % HASH_SIZE], name, &insert)) {
		return false;
	} else {
		struct CmdData *newcmd = Malloc (sizeof(struct CmdData));
	
		newcmd->name = name;
		newcmd->call.generic = data;
		if (insert) {
			newcmd->next = insert->next;
			insert->next = newcmd;
		} else {
			newcmd->next = table[key % HASH_SIZE];
			table[key % HASH_SIZE] = newcmd;
		}
	}
	return true;
}

void C_RegisterCommand (char *name, void (*func)())
{
	static BOOL firstTime = true;

	if (firstTime) {
		char name[16];
		int i;

		firstTime = false;

		// Add all the action commands for tab completion
		for (i = 0; i < NUM_ACTIONS; i++) {
			strcpy (&name[1], actionbits[i].name);
			name[0] = '+';
			C_AddTabCommand (name);
			name[0] = '-';
			C_AddTabCommand (name);
		}
	}


	if (!AddToHash (Commands, name, func))
		Printf ("C_RegisterCommand: %s exists\n", name);
	else
		C_AddTabCommand (name);
}

void C_RegisterCommands (struct CmdDispatcher *cmd)
{
	if (cmd) {
		while (cmd->CmdName) {
			C_RegisterCommand (cmd->CmdName, cmd->Command);
			cmd++;
		}
	}
}

char *BuildString (int argc, char **argv)
{
	char temp[1024];
	char *cur;
	int arg;

	if (argc == 1) {
		return copystring (*argv);
	} else {
		cur = temp;
		for (arg = 0; arg < argc; arg++) {
			if (strchr (argv[arg], ' ')) {
				cur += sprintf (cur, "\"%s\" ", argv[arg]);
			} else {
				cur += sprintf (cur, "%s ", argv[arg]);
			}
		}
		temp[strlen (temp) - 1] = 0;
		return copystring (temp);
	}
}

static int DumpHash (struct CmdData **table, BOOL showcommand)
{
	int bucket, count;
	struct CmdData *cmd;

	for (bucket = count = 0; bucket < HASH_SIZE; bucket++) {
		cmd = table[bucket];
		while (cmd) {
			count++;
			if (showcommand)
				Printf ("%s : %s\n", cmd->name, cmd->call.command);
			else
				Printf ("%s\n", cmd->name);
			cmd = cmd->next;
		}
	}
	return count;
}

void C_ArchiveAliases (FILE *f)
{
	int bucket;
	struct CmdData *alias;

	for (bucket = 0; bucket < HASH_SIZE; bucket++) {
		alias = Aliases[bucket];
		while (alias) {
			fprintf (f, "alias \"%s\" \"%s\"\n", alias->name, alias->call.command);
			alias = alias->next;
		}
	}
}

void Cmd_Alias (player_t *player, int argc, char **argv)
{
	struct CmdData *prev, *alias, **chain;

	if (argc == 1) {
		Printf ("Current alias commands:\n");
		DumpHash (Aliases, true);
	} else {
		chain = &Aliases[MakeKey (argv[1]) % HASH_SIZE];

		if (argc == 2) {
			// Remove the alias

			if ( (alias = ScanChainForName (*chain, argv[1], &prev)) ) {
				if (prev) {
					prev->next = alias->next;
				} else {
					*chain = alias->next;
				}
				C_RemoveTabCommand (alias->name);
				free (alias->name);
				free (alias->call.command);
				free (alias);
			}
		} else {
			// Add/Change the alias

			alias = ScanChainForName (*chain, argv[1], &prev);
			if (alias) {
				free (alias->name);
				free (alias->call.command);
			} else {
				alias = Malloc (sizeof(struct CmdData));
				if (prev) {
					alias->next = prev->next;
					prev->next = alias;
				} else {
					alias->next = NULL;
					*chain = alias;
				}
			}
			alias->name = copystring (argv[1]);
			if (argc > 3)
				alias->call.command = BuildString (argc - 2, &argv[2]);
			else
				alias->call.command = copystring (argv[2]);
			C_AddTabCommand (alias->name);
		}
	}
}


void Cmd_Cmdlist (player_t *plyr, int argc, char **argv)
{
	int count;

	count = ListActionCommands ();
	count += DumpHash (Commands, false);
	Printf ("%d commands\n", count);
}

void Cmd_Key (player_t *plyr, int argc, char **argv)
{
	if (argc > 1) {
		while (argc > 1) {
			Printf (" %08x", MakeKey (argv[1]));
			argc--;
			argv++;
		}
		Printf ("\n");
	}
}

// Execute any console commands specified on the command line.
// These all begin with '+' as opposed to '-'.
// If onlyset is true, only "set" commands will be executed,
// otherwise only non-"set" commands are executed.
void C_ExecCmdLineParams (int onlyset)
{
	int currArg, setComp, cmdlen, argstart;
	char *cmdString;

	for (currArg = 1; currArg < myargc; ) {
		if (*myargv[currArg++] == '+') {
			setComp = stricmp (myargv[currArg - 1] + 1, "set");
			if ((onlyset && setComp) || (!onlyset && !setComp)) {
				continue;
			}

			cmdlen = 1;
			argstart = currArg - 1;

			while (currArg < myargc) {
				if (*myargv[currArg] == '-' || *myargv[currArg] == '+')
					break;
				currArg++;
				cmdlen++;
			}

			if ( (cmdString = BuildString (cmdlen, &myargv[argstart])) ) {
				C_DoCommand (cmdString + 1);
				free (cmdString);
			}
		}
	}
}