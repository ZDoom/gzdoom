#ifndef __C_DISPATCH_H__
#define __C_DISPATCH_H__

#include "dobject.h"

#define HASH_SIZE	251				// I think this is prime

void C_ExecCmdLineParams (int onlyset);

// add commands to the console as if they were typed in
// for map changing, etc
void AddCommandString (char *text);

// parse a command string
char *ParseString (char *data);

// Write out alias commands to a file for all current aliases.
void C_ArchiveAliases (FILE *f);

// build a single string out of multiple strings
char *BuildString (int argc, char **argv);

class DConsoleCommand : public DObject
{
	DECLARE_CLASS (DConsoleCommand, DObject)
public:
	DConsoleCommand (const char *name);
	virtual ~DConsoleCommand ();
	virtual void Run () = 0;
	virtual bool IsAlias () { return false; }
	void PrintCommand () { Printf (PRINT_HIGH, "%s\n", m_Name); }

	DConsoleCommand *m_Next, **m_Prev;
	char *m_Name;

protected:
	bool AddToHash (DConsoleCommand **table);

	AActor *m_Instigator;
	int argc;
	char **argv;
	char *args;

	friend void C_DoCommand (char *cmd);
};

#define BEGIN_COMMAND(n) \
	static class Cmd_##n : public DConsoleCommand { \
		public: \
			Cmd_##n () : DConsoleCommand (#n) {} \
			Cmd_##n (const char *name) : DConsoleCommand (name) {} \
			void Run ()

#define END_COMMAND(n)		} Istaticcmd##n;

class DConsoleAlias : public DConsoleCommand
{
	DECLARE_CLASS (DConsoleAlias, DConsoleCommand)
public:
	DConsoleAlias (const char *name, const char *command);
	~DConsoleAlias ();
	void Run () { AddCommandString (m_Command); }
	bool IsAlias () { return true; }
	void PrintAlias () { Printf (PRINT_HIGH, "%s : %s\n", m_Name, m_Command); }
	void Archive (FILE *f);
protected:
	char *m_Command;
};

// Actions
#define ACTION_MLOOK		0
#define ACTION_KLOOK		1
#define ACTION_USE			2
#define ACTION_ATTACK		3
#define ACTION_SPEED		4
#define ACTION_MOVERIGHT	5
#define ACTION_MOVELEFT		6
#define ACTION_STRAFE		7
#define ACTION_LOOKDOWN		8
#define ACTION_LOOKUP		9
#define ACTION_BACK			10
#define ACTION_FORWARD		11
#define ACTION_RIGHT		12
#define ACTION_LEFT			13
#define ACTION_MOVEDOWN		14
#define ACTION_MOVEUP		15
#define ACTION_JUMP			16
#define ACTION_SHOWSCORES	17
#define NUM_ACTIONS			18

extern byte Actions[NUM_ACTIONS];

struct ActionBits
{
	unsigned int	key;
	int				index;
	char			name[12];
};

extern unsigned int MakeKey (const char *s);

#endif //__C_DISPATCH_H__