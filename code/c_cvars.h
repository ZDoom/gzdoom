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

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s
{
	char		*name;
	char		*string;
	char		*latched_string;	// for CVAR_LATCH vars
	int			flags;
	boolean		modified;	// set each time the cvar is changed
	float		value;
	struct cvar_s *next;
} cvar_t;

#endif		// CVAR

// console variable interaction
cvar_t *cvar (char *var_name, char *value, int flags);
cvar_t *cvar_set (char *var_name, char *value);
cvar_t *cvar_forceset (char *var_name, char *value);

cvar_t *FindCVar (char *var_name, cvar_t **prev);
void SetCVar (cvar_t *var, char *value);
void SetCVarFloat (cvar_t *var, float value);

// initialize all predefined cvars
void C_SetCVars (void);

// archive cvars to FILE f
void C_ArchiveCVars (void *f);

// enable the honoring of CVAR_NOSET
void C_EnableNoSet (void);

#endif //__C_CVARS_H__