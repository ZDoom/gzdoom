#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>

#include "c_cvars.h"
#include "d_player.h"

void *mymalloc (size_t size);

static boolean donoset = false;
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

void SetCVar (cvar_t *var, char *value)
{
	if (var->flags & CVAR_LATCH && !var->modified) {
		var->modified = true;
		var->latched_string = var->string;
		var->string = NULL;
	}
	if (var->string)
		free (var->string);
	var->string = mymalloc (strlen (value) + 1);
	strcpy (var->string, value);
	var->value = (float)atof (value);
}

void SetCVarFloat (cvar_t *var, float value)
{
	char string[32];

	sprintf (string, "%g", value);
	SetCVar (var, string);
}

cvar_t *cvar (char *var_name, char *value, int flags)
{
	cvar_t *var, *dummy;

	var = FindCVar (var_name, &dummy);

	if (!var) {
		var = mymalloc (sizeof(cvar_t));
		var->name = mymalloc (strlen (var_name) + 1);
		strcpy (var->name, var_name);
		var->string = NULL;
		var->latched_string = NULL;
		var->flags = 0;
		var->modified = false;
		SetCVar (var, value);
		var->flags = flags;
		var->next = CVars;
		CVars = var;
	}
	return var;
}

cvar_t *cvar_set (char *var_name, char *value)
{
	cvar_t *var, *dummy;

	if (var = FindCVar (var_name, &dummy))
		if (!(var->flags & CVAR_NOSET) || !donoset)
			SetCVar (var, value);

	return var;
}

cvar_t *cvar_forceset (char *var_name, char *value)
{
	cvar_t *var, *dummy;

	if (var = FindCVar (var_name, &dummy))
		SetCVar (var, value);

	return var;
}


void Cmd_Set (player_t *plyr, int argc, char **argv)
{
	cvar_t *var, *prev;

	if (argc == 2) {
		if (var = FindCVar (argv[1], &prev)) {
			if (var->flags & CVAR_UNSETTABLE) {
				if (prev)
					prev->next = var->next;
				else
					CVars = var->next;
				free (var);
			} else {
				Printf ("set: could not unset \"%s\"\n", argv[1]);
			}
		}
	} else if (argc >= 3) {
		if (!cvar_set (argv[1], argv[2]))
			cvar (argv[1], argv[2], CVAR_UNSETTABLE);
	} else {
		var = CVars;
		while (var) {
			Printf ("\"%s\" is \"%s\"\n", var->name, var->string);
			var = var->next;
		}
	}
}

void Cmd_Get (player_t *plyr, int argc, char **argv)
{
	cvar_t *var, *prev;

	if (argc >= 2) {
		if (var = FindCVar (argv[1], &prev)) {
			Printf ("\"%s\" is \"%s\"\n", var->name, var->string);
		} else {
			Printf ("\"%s\" is unset\n", argv[1]);
		}
	} else {
		Printf ("get: need variable name\n");
	}
}

void Cmd_Toggle (player_t *plyr, int argc, char **argv)
{
	cvar_t *var, *prev;

	if (argc > 1) {
		if (var = FindCVar (argv[1], &prev)) {
			SetCVarFloat (var, (float)(!var->value));
			Printf ("\"%s\" is \"%s\"\n", var->name, var->string);
		}
	}
}

void C_EnableNoSet (void)
{
	donoset = true;
}
