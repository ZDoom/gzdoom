#ifndef __C_DISPATCH_H__
#define __C_DISPATCH_H__

#define HASH_SIZE	251				// I think this is prime

struct CmdData {
	struct CmdData *next;
	char		   *name;
	union {
		void	  (*func)();		// For an actual command
		char	   *command;		// For an alias
		void	   *generic;
	};
};

// For passing to C_RegisterCommand(s)
struct CmdDispatcher {
	char *CmdName;
	void (*Command)();
};

void C_RegisterCommand (char *name, void (*func)());
void C_RegisterCommands (struct CmdDispatcher *cmd);

void C_DoCommand (char *cmd);

void C_ExecCmdLineParams (int onlyset);

// add commands to the console as if they were typed in
// for map changing, etc
void AddCommandString (char *text);

// parse a command string
char *ParseString (char *data);

// build a single string out of multiple strings
char *BuildString (int argc, char **argv);

// Actions
#define ACTION_MLOOK		0x00001
#define ACTION_KLOOK		0x00002
#define ACTION_USE			0x00004
#define ACTION_ATTACK		0x00008
#define ACTION_SPEED		0x00010
#define ACTION_MOVERIGHT	0x00020
#define ACTION_MOVELEFT		0x00040
#define ACTION_STRAFE		0x00080
#define ACTION_LOOKDOWN		0x00100
#define ACTION_LOOKUP		0x00200
#define ACTION_BACK			0x00400
#define ACTION_FORWARD		0x00800
#define ACTION_RIGHT		0x01000
#define ACTION_LEFT			0x02000
#define ACTION_MOVEDOWN		0x04000
#define ACTION_MOVEUP		0x08000
#define ACTION_JUMP			0x10000
#define NUM_ACTIONS			17

extern int Actions;

#endif //__C_DISPATCH_H__