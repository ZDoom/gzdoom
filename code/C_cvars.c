#include <string.h>
#include <stdio.h>

#include "cmdlib.h"
#include "c_consol.h"
#include "m_alloc.h"

#include "doomstat.h"
#include "c_cvars.h"
#include "d_player.h"

#include "d_netinf.h"
#include "dstrings.h"

#include "i_system.h"

static BOOL donoset = false;
cvar_t *CVars = NULL;

cvar_t *FindCVar (char *var_name, cvar_t **prev)
{
	cvar_t *var;

	var = CVars;
	*prev = NULL;
	while (var) {
		if (stricmp (var->name, var_name) == 0)
			break;
		*prev = var;
		var = var->next;
	}
	return var;
}

void UnlatchCVars (void)
{
	cvar_t *var;

	var = CVars;
	while (var) {
		if ((var->modified) && (var->flags & CVAR_LATCH)) {
			unsigned oldflags = var->flags;
			var->flags &= ~(CVAR_LATCH|CVAR_SERVERINFO);
			var->modified = false;
			if (var->u.latched_string) {
				SetCVar (var, var->u.latched_string);
				free (var->u.latched_string);
			}
			var->u.latched_string = NULL;
			var->flags = oldflags;
		}
		var = var->next;
	}
}

void SetCVar (cvar_t *var, char *value)
{
	if (var->flags & CVAR_LATCH && gamestate != GS_FULLCONSOLE && gamestate != GS_STARTUP) {
		var->modified = true;
		var->u.latched_string = copystring (value);
	} else if (var->flags & CVAR_SERVERINFO && gamestate != GS_STARTUP && !demoplayback) {
		if (netgame && consoleplayer != 0) {
			Printf (PRINT_HIGH, "Only player 1 can change %s\n", var->name);
			return;
		}
		D_SendServerInfoChange (var, value);
	} else {
		var->modified = true;
		ReplaceString (&var->string, value);
		var->value = (float)atof (value);
		if (var->flags & CVAR_USERINFO)
			D_UserInfoChanged (var);
		if (var->flags & CVAR_CALLBACK && var->u.callback)
			var->u.callback (var);
	}
}

void SetCVarFloat (cvar_t *var, float value)
{
	char string[32];

	sprintf (string, "%g", value);
	SetCVar (var, string);
}

int cvar_defflags;

cvar_t *cvar (char *var_name, char *value, int flags)
{
	cvar_t *var, *dummy;

	var = FindCVar (var_name, &dummy);

	if (!var) {
		var = Malloc (sizeof(cvar_t));
		var->name = Malloc (strlen (var_name) + 1);
		strcpy (var->name, var_name);
		var->string = NULL;
		var->u.latched_string = NULL;
		var->flags = 0;
		var->modified = false;
		SetCVar (var, value);
		var->next = CVars;
		CVars = var;
		C_AddTabCommand (var_name);
	}
	var->flags = flags | cvar_defflags;;
	return var;
}

cvar_t *cvar_set (char *var_name, char *value)
{
	cvar_t *var, *dummy;

	if ( (var = FindCVar (var_name, &dummy)) )
		if (!(var->flags & CVAR_NOSET) || !donoset)
			SetCVar (var, value);

	return var;
}

cvar_t *cvar_forceset (char *var_name, char *value)
{
	cvar_t *var, *dummy;

	if ( (var = FindCVar (var_name, &dummy)) )
		SetCVar (var, value);

	return var;
}


void Cmd_Set (player_t *plyr, int argc, char **argv)
{
	cvar_t *var, *prev;

	if (argc != 3) {
		Printf (PRINT_HIGH, "usage: set <variable> <value>\n");
	} else {
		if (!cvar_set (argv[1], argv[2]))
			cvar (argv[1], argv[2], CVAR_UNSETTABLE);

		var = FindCVar (argv[1], &prev);
		if (var->flags & CVAR_NOSET)
			Printf (PRINT_HIGH, "%s is write protected.\n", argv[1]);
		else if (var->flags & CVAR_LATCH)
			Printf (PRINT_HIGH, "%s will be changed for next game.\n", argv[1]);
	}
}

void Cmd_Get (player_t *plyr, int argc, char **argv)
{
	cvar_t *var, *prev;

	if (argc >= 2) {
		if ( (var = FindCVar (argv[1], &prev)) ) {
			Printf (PRINT_HIGH, "\"%s\" is \"%s\"\n", var->name, var->string);
		} else {
			Printf (PRINT_HIGH, "\"%s\" is unset\n", argv[1]);
		}
	} else {
		Printf (PRINT_HIGH, "get: need variable name\n");
	}
}

void Cmd_Toggle (player_t *plyr, int argc, char **argv)
{
	cvar_t *var, *prev;

	if (argc > 1) {
		if ( (var = FindCVar (argv[1], &prev)) ) {
			SetCVarFloat (var, (float)(!var->value));
			Printf (PRINT_HIGH, "\"%s\" is \"%s\"\n", var->name, var->string);
		}
	}
}

void Cmd_CvarList (player_t *plyr, int argc, char **argv)
{
	cvar_t *var = CVars;
	int count = 0;

	while (var) {
		unsigned flags = var->flags;

		count++;
		Printf (PRINT_HIGH, "%c%c%c%c %s \"%s\"\n",
				flags & CVAR_ARCHIVE ? 'A' : ' ',
				flags & CVAR_USERINFO ? 'U' : ' ',
				flags & CVAR_SERVERINFO ? 'S' : ' ',
				flags & CVAR_NOSET ? '-' :
					flags & CVAR_LATCH ? 'L' :
					flags & CVAR_CALLBACK ? 'C' :
					flags & CVAR_UNSETTABLE ? '*' : ' ',
				var->name,
				var->string);
		var = var->next;
	}
	Printf (PRINT_HIGH, "%d cvars\n", count);
}

void C_EnableNoSet (void)
{
	donoset = true;
}

void C_WriteCVars (byte **demo_p, int filter)
{
	cvar_t *cvar = CVars;
	byte *ptr = *demo_p;

	while (cvar) {
		if (cvar->flags & filter) {
			sprintf (ptr, "\\%s\\%s", cvar->name, cvar->string);
			ptr += strlen (ptr);
		}
		cvar = cvar->next;
	}

	*demo_p = ptr + 1;
}

void C_ReadCVars (byte **demo_p)
{
	byte *ptr = *demo_p;
	byte *breakpt;
	byte *value;

	if (*ptr++ != '\\')
		return;

	while ( (breakpt = strchr (ptr, '\\')) ) {
		*breakpt = 0;
		value = breakpt + 1;
		if ( (breakpt = strchr (value, '\\')) )
			*breakpt = 0;

		cvar_set (ptr, value);

		*(value - 1) = '\\';
		if (breakpt) {
			*breakpt = '\\';
			ptr = breakpt + 1;
		} else {
			break;
		}
	}

	*demo_p += strlen (*demo_p) + 1;
}

static struct backup_s {
	char *name, *string;
} CVarBackups[MAX_DEMOCVARS];

static int numbackedup = 0;

void C_BackupCVars (void)
{
	struct backup_s *backup = CVarBackups;
	cvar_t *cvar = CVars;

	while (cvar) {
		if (((cvar->flags & CVAR_SERVERINFO) || (cvar->flags & CVAR_DEMOSAVE))
			&& !(cvar->flags & CVAR_LATCH)) {
			if (backup == &CVarBackups[MAX_DEMOCVARS])
				I_Error ("C_BackupDemoCVars: Too many cvars to save (%d)", MAX_DEMOCVARS);
			backup->name = copystring (cvar->name);
			backup->string = copystring (cvar->string);
			backup++;
		}
		cvar = cvar->next;
	}
	numbackedup = backup - CVarBackups;
}

void C_RestoreCVars (void)
{
	struct backup_s *backup = CVarBackups;
	int i;

	for (i = numbackedup; i; i--, backup++) {
		cvar_set (backup->name, backup->string);
		free (backup->name);
		free (backup->string);
		backup->name = backup->string = NULL;
	}
	numbackedup = 0;
}