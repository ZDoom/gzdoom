/*
** c_dispatch.h
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

#ifndef __C_DISPATCH_H__
#define __C_DISPATCH_H__

#include "doomtype.h"

class FConfigFile;
class APlayerPawn;

// Class that can parse command lines
class FCommandLine
{
public:
	FCommandLine (const char *commandline, bool no_escapes = false);
	~FCommandLine ();
	int argc ();
	char *operator[] (int i);
	const char *args () { return cmd; }
	void Shift();

private:
	const char *cmd;
	int _argc;
	char **_argv;
	long argsize;
	bool noescapes;
};

// Contains the contents of an exec'ed file
struct FExecList
{
	TArray<FString> Commands;
	TArray<FString> Pullins;

	void AddCommand(const char *cmd, const char *file = NULL);
	void ExecCommands() const;
	void AddPullins(TArray<FString> &wads) const;
};


extern bool CheckCheatmode (bool printmsg = true);

FExecList *C_ParseCmdLineParams(FExecList *exec);

// Add commands to the console as if they were typed in. Can handle wait
// and semicolon-separated commands. This function may modify the source
// string, but the string will be restored to its original state before
// returning. Therefore, commands passed must not be in read-only memory.
void AddCommandString (char *text, int keynum=0);

// Process a single console command. Does not handle wait.
void C_DoCommand (const char *cmd, int keynum=0);

FExecList *C_ParseExecFile(const char *file, FExecList *source);
void C_SearchForPullins(FExecList *exec, const char *file, class FCommandLine &args);
bool C_ExecFile(const char *file);

// Write out alias commands to a file for all current aliases.
void C_ArchiveAliases (FConfigFile *f);

void C_SetAlias (const char *name, const char *cmd);
void C_ClearAliases ();

// build a single string out of multiple strings
FString BuildString (int argc, FString *argv);

typedef void (*CCmdRun) (FCommandLine &argv, APlayerPawn *instigator, int key);

class FConsoleCommand
{
public:
	FConsoleCommand (const char *name, CCmdRun RunFunc);
	virtual ~FConsoleCommand ();
	virtual bool IsAlias ();
	void PrintCommand () { Printf ("%s\n", m_Name); }

	virtual void Run (FCommandLine &args, APlayerPawn *instigator, int key);
	static FConsoleCommand* FindByName (const char* name);

	FConsoleCommand *m_Next, **m_Prev;
	char *m_Name;

	enum { HASH_SIZE = 251 };	// Is this prime?

protected:
	FConsoleCommand ();
	bool AddToHash (FConsoleCommand **table);

	CCmdRun m_RunFunc;

};

#define CCMD(n) \
	void Cmd_##n (FCommandLine &, APlayerPawn *, int key); \
	FConsoleCommand Cmd_##n##_Ref (#n, Cmd_##n); \
	void Cmd_##n (FCommandLine &argv, APlayerPawn *who, int key)

const int KEY_DBLCLICKED = 0x8000;

class FConsoleAlias : public FConsoleCommand
{
public:
	FConsoleAlias (const char *name, const char *command, bool noSave);
	~FConsoleAlias ();
	void Run (FCommandLine &args, APlayerPawn *Instigator, int key);
	bool IsAlias ();
	void PrintAlias ();
	void Archive (FConfigFile *f);
	void Realias (const char *command, bool noSave);
	void SafeDelete ();
protected:
	FString m_Command[2];	// Slot 0 is saved to the ini, slot 1 is not.
	bool bDoSubstitution;
	bool bRunning;
	bool bKill;
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

	bool PressKey (int keynum);		// Returns true if this key caused the button to be pressed.
	bool ReleaseKey (int keynum);	// Returns true if this key is no longer pressed.
	void ResetTriggers () { bWentDown = bWentUp = false; }
	void Reset () { bDown = bWentDown = bWentUp = false; }
};

extern FButtonStatus Button_Mlook, Button_Klook, Button_Use, Button_AltAttack,
	Button_Attack, Button_Speed, Button_MoveRight, Button_MoveLeft,
	Button_Strafe, Button_LookDown, Button_LookUp, Button_Back,
	Button_Forward, Button_Right, Button_Left, Button_MoveDown,
	Button_MoveUp, Button_Jump, Button_ShowScores, Button_Crouch,
	Button_Zoom, Button_Reload,
	Button_User1, Button_User2, Button_User3, Button_User4,
	Button_AM_PanLeft, Button_AM_PanRight, Button_AM_PanDown, Button_AM_PanUp,
	Button_AM_ZoomIn, Button_AM_ZoomOut;
extern bool ParsingKeyConf;

void ResetButtonTriggers ();	// Call ResetTriggers for all buttons
void ResetButtonStates ();		// Same as above, but also clear bDown

extern unsigned int MakeKey (const char *s);
extern unsigned int MakeKey (const char *s, size_t len);
extern unsigned int SuperFastHash (const char *data, size_t len);

void execLogfile(const char *fn);

#endif //__C_DISPATCH_H__
