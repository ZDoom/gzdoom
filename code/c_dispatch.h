#ifndef __C_DISPATCH_H__
#define __C_DISPATCH_H__

#include "dobject.h"

class FConfigFile;

#define HASH_SIZE	251				// I think this is prime

void C_ExecCmdLineParams ();

// add commands to the console as if they were typed in
// for map changing, etc
void AddCommandString (char *text);

// Write out alias commands to a file for all current aliases.
void C_ArchiveAliases (FConfigFile *f);

void C_SetAlias (const char *name, const char *cmd);

// build a single string out of multiple strings
char *BuildString (int argc, char **argv);

typedef void (*CCmdRun) (int argc, char **argv, const char *args, AActor *instigator);

class FConsoleCommand
{
public:
	FConsoleCommand (const char *name, CCmdRun RunFunc);
	virtual ~FConsoleCommand ();
	virtual bool IsAlias ();
	void PrintCommand () { Printf ("%s\n", m_Name); }

	virtual void Run (int argc, char **argv, const char *args, AActor *instigator);

	FConsoleCommand *m_Next, **m_Prev;
	char *m_Name;

protected:
	FConsoleCommand ();
	bool AddToHash (FConsoleCommand **table);

	CCmdRun m_RunFunc;

	friend void C_DoCommand (char *cmd);
};

#define CCMD(n) \
	void Cmd_##n (int, char **, const char *, AActor *); \
	FConsoleCommand Cmd_##n##_ (#n, Cmd_##n); \
	void Cmd_##n (int argc, char **argv, const char *args, AActor *m_Instigator)

class FConsoleAlias : public FConsoleCommand
{
public:
	FConsoleAlias (const char *name, const char *command);
	~FConsoleAlias ();
	void Run (int argc, char **argv, const char *args, AActor *m_Instigator);
	bool IsAlias ();
	void PrintAlias () { Printf ("%s : %s\n", m_Name, m_Command); }
	void Archive (FConfigFile *f);
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
