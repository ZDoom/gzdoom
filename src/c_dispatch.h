#ifndef __C_DISPATCH_H__
#define __C_DISPATCH_H__

#include "dobject.h"

class FConfigFile;

#define HASH_SIZE	251				// I think this is prime

void C_ExecCmdLineParams ();

// Add commands to the console as if they were typed in. Can handle wait
// and semicolon-separated commands. This function may modify the source
// string, but the string will be restored to its original state before
// returning. Therefore, commands passed must not be in read-only memory.
void AddCommandString (char *text, int keynum=0);

// Process a single console command. Does not handle wait.
void C_DoCommand (const char *cmd, int keynum=0);

// Write out alias commands to a file for all current aliases.
void C_ArchiveAliases (FConfigFile *f);

void C_SetAlias (const char *name, const char *cmd);

// build a single string out of multiple strings
char *BuildString (int argc, char **argv);

// Class that can parse command lines
class FCommandLine
{
public:
	FCommandLine (const char *commandline);
	~FCommandLine ();
	int argc ();
	char *operator[] (int i);
	const char *args () { return cmd; }
	const char *AllButFirstArg (int numToSkip=1);	// like args(), but can skip first n args

private:
	const char *cmd;
	int _argc;
	char **_argv;
	long argsize;
};

typedef void (*CCmdRun) (FCommandLine &argv, AActor *instigator, int key);

class FConsoleCommand
{
public:
	FConsoleCommand (const char *name, CCmdRun RunFunc);
	virtual ~FConsoleCommand ();
	virtual bool IsAlias ();
	void PrintCommand () { Printf ("%s\n", m_Name); }

	virtual void Run (FCommandLine &args, AActor *instigator, int key);

	FConsoleCommand *m_Next, **m_Prev;
	char *m_Name;

protected:
	FConsoleCommand ();
	bool AddToHash (FConsoleCommand **table);

	CCmdRun m_RunFunc;

};

#define CCMD(n) \
	void Cmd_##n (FCommandLine &, AActor *, int key); \
	FConsoleCommand Cmd_##n##_ (#n, Cmd_##n); \
	void Cmd_##n (FCommandLine &argv, AActor *m_Instigator, int key)

const int KEY_DBLCLICKED = 0x8000;

class FConsoleAlias : public FConsoleCommand
{
public:
	FConsoleAlias (const char *name, const char *command);
	~FConsoleAlias ();
	void Run (FCommandLine &args, AActor *Instigator, int key);
	bool IsAlias ();
	void PrintAlias () { Printf ("%s : %s\n", m_Name, m_Command); }
	void Archive (FConfigFile *f);
protected:
	char *m_Command;
};

// Actions
struct FButtonStatus
{
	enum { MAX_KEYS = 6 };	// Maximum number of keys that can press this button

	WORD Keys[MAX_KEYS];
	BYTE bDown;				// Button is down right now
	BYTE bWentDown;			// Button went down this tic
	BYTE bWentUp;			// Button went up this tic
	BYTE padTo16Bytes;

	void PressKey (int keynum);
	void ReleaseKey (int keynum);
	void ResetTriggers () { bWentDown = bWentUp = false; }
};

extern FButtonStatus Button_Mlook, Button_Klook, Button_Use,
	Button_Attack, Button_Speed, Button_MoveRight, Button_MoveLeft,
	Button_Strafe, Button_LookDown, Button_LookUp, Button_Back,
	Button_Forward, Button_Right, Button_Left, Button_MoveDown,
	Button_MoveUp, Button_Jump, Button_ShowScores;

void ResetButtonTriggers ();	// Call ResetTriggers for all buttons
void ResetButtonStates ();		// Same as above, but also clear bDown

extern unsigned int MakeKey (const char *s);

#endif //__C_DISPATCH_H__
