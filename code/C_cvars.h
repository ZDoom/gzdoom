#ifndef __C_CVARS_H__
#define __C_CVARS_H__

#include "doomtype.h"

/*
==========================================================

CVARS (console variables)

==========================================================
*/

#ifndef CVAR
#define	CVAR

#define	CVAR_ARCHIVE	1	// set to cause it to be saved to vars.rc
#define	CVAR_USERINFO	2	// added to userinfo  when changed
#define	CVAR_SERVERINFO	4	// added to serverinfo when changed
#define	CVAR_NOSET		8	// don't allow change from console at all,
							// but can be set from the command line
#define	CVAR_LATCH		16	// save changes until server restart
#define CVAR_UNSETTABLE	32	// can unset this var from console
#define CVAR_CALLBACK	64	// calls callback() when modified (do not set with CVAR_LATCH!)
#define CVAR_DEMOSAVE	128 // save the value of this cvar in a demo

typedef struct cvar_s
{
	char		*name;
	char		*string;
	union {
		char	*latched_string;					// for CVAR_LATCH vars
		void	(*callback)(struct cvar_s *);		// for CVAR_CALLBACK vars (safe to set)
	} u;
	unsigned	flags;
	BOOL		modified;	// set each time the cvar is changed
	float		value;
	struct cvar_s *next;
} cvar_t;

#endif		// CVAR

// console variable interaction
cvar_t *cvar (char *var_name, char *value, int flags);
cvar_t *cvar_set (char *var_name, char *value);
cvar_t *cvar_forceset (char *var_name, char *value);

// Called from G_InitNew()
void UnlatchCVars (void);

cvar_t *FindCVar (char *var_name, cvar_t **prev);
void SetCVar (cvar_t *var, char *value);
void SetCVarFloat (cvar_t *var, float value);

// initialize all predefined cvars
void C_SetCVars (void);

// initialize all those cvars to default values after they are created
void C_SetCVarsToDefaults (void);

// archive cvars to FILE f
void C_ArchiveCVars (void *f);

// enable the honoring of CVAR_NOSET
void C_EnableNoSet (void);

// Writes all cvars that could effect demo sync to *demo_p. These are
// cvars that have either CVAR_SERVERINFO or CVAR_DEMOSAVE set.
void C_WriteCVars (byte **demo_p, int filter);

// Read all cvars from *demo_p and sets them appropriately.
void C_ReadCVars (byte **demo_p);


// Maximum number of cvars that can be saved across a demo. If you need
// to save more, bump this up.
#define MAX_DEMOCVARS 32

// Backup demo cvars. Called before a demo starts playing to save all
// cvars the demo might change.
void C_BackupCVars (void);

// Restore demo cvars. Called after demo playback to restore all cvars
// that might possibly have been changed during the course of demo playback.
void C_RestoreCVars (void);

#endif //__C_CVARS_H__