#define CVAR_IMPLEMENTOR	1

#include <string.h>
#include <stdio.h>

#include "cmdlib.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "m_alloc.h"

#include "doomstat.h"
#include "c_cvars.h"
#include "d_player.h"

#include "d_netinf.h"
#include "dstrings.h"

#include "i_system.h"

bool cvar_t::m_DoNoSet = false;
bool cvar_t::m_UseCallback = false;

cvar_t *CVars = NULL;

int cvar_defflags;

cvar_t::cvar_t (const cvar_t &var)
{
	I_FatalError ("Use of cvar copy constructor");
}

cvar_t &cvar_t::operator= (const cvar_t &var)
{
	I_FatalError ("Use of cvar = operator");
	return *this;
}

cvar_t::cvar_t (const char *var_name, const char *def, DWORD flags)
{
	InitSelf (var_name, def, flags, NULL);
}

cvar_t::cvar_t (const char *var_name, const char *def, DWORD flags, void (*callback)(cvar_t &))
{
	InitSelf (var_name, def, flags, callback);
}

void cvar_t::InitSelf (const char *var_name, const char *def, DWORD var_flags, void (*callback)(cvar_t &))
{
	cvar_t *var, *dummy;

	var = FindCVar (var_name, &dummy);

	m_Callback = callback;
	string = NULL;
	value = 0.0f;
	flags = 0;
	m_LatchedString = NULL;
	if (def)
		m_Default = copystring (def);
	else
		m_Default = NULL;

	if (var_name)
	{
		C_AddTabCommand (var_name);
		name = copystring (var_name);
		m_Next = CVars;
		CVars = this;
	}
	else
		name = NULL;

	if (var)
	{
		ForceSet (var->string);
		if (var->flags & CVAR_AUTO)
			delete var;
		else
			var->~cvar_t();
	}
	else if (def)
		ForceSet (def);

	flags = var_flags | CVAR_ISDEFAULT;
}

cvar_t::~cvar_t ()
{
	if (name)
	{
		cvar_t *var, *dummy;

		var = FindCVar (name, &dummy);

		if (var == this)
		{
			if (dummy)
				dummy->m_Next = m_Next;
			else
				CVars = m_Next;
		}
	}
	if (m_LatchedString)
		delete[] m_LatchedString;
	if (m_Default)
		delete[] m_Default;
	if (string)
		delete[] string;
	if (name)
		delete[] name;
}

void cvar_t::ForceSet (const char *val)
{
	if ((flags & CVAR_LATCH) && gamestate != GS_FULLCONSOLE && gamestate != GS_STARTUP)
	{
		flags |= CVAR_MODIFIED;
		m_LatchedString = copystring (val);
	}
	else if ((flags & CVAR_SERVERINFO) && gamestate != GS_STARTUP && !demoplayback)
	{
		if (netgame && consoleplayer != Net_Arbitrator)
		{
			Printf (PRINT_HIGH, "Only player %d can change %s\n", Net_Arbitrator+1, name);
			return;
		}
		D_SendServerInfoChange (this, val);
	}
	else
	{
		flags |= CVAR_MODIFIED;
		ReplaceString (&string, (char *)val);
		value = atof (val);
		if (flags & CVAR_USERINFO)
			D_UserInfoChanged (this);
		if (m_UseCallback)
			Callback ();
	}
	flags &= ~CVAR_ISDEFAULT;
}

void cvar_t::ForceSet (float val)
{
	char string[32];

	sprintf (string, "%g", val);
	Set (string);
}

void cvar_t::Set (const char *val)
{
	if (!(flags & CVAR_NOSET) || !m_DoNoSet)
		ForceSet (val);
}

void cvar_t::Set (float val)
{
	if (!(flags & CVAR_NOSET) || !m_DoNoSet)
		ForceSet (val);
}

void cvar_t::SetDefault (const char *val)
{
	delete m_Default;
	m_Default = copystring (val);
	if (flags & CVAR_ISDEFAULT)
	{
		Set (val);
		flags |= CVAR_ISDEFAULT;
	}
}

cvar_t *cvar_set (const char *var_name, const char *val)
{
	cvar_t *var, *dummy;

	if ( (var = FindCVar (var_name, &dummy)) )
		var->Set (val);

	return var;
}

cvar_t *cvar_forceset (const char *var_name, const char *val)
{
	cvar_t *var, *dummy;

	if ( (var = FindCVar (var_name, &dummy)) )
		var->ForceSet (val);

	return var;
}

void cvar_t::EnableNoSet ()
{
	m_DoNoSet = true;
}

void cvar_t::EnableCallbacks ()
{
	m_UseCallback = true;
	cvar_t *cvar = CVars;

	while (cvar)
	{
		cvar->Callback ();
		cvar = cvar->m_Next;
	}
}

static int STACK_ARGS sortcvars (const void *a, const void *b)
{
	return strcmp (((*(cvar_t **)a))->name, ((*(cvar_t **)b))->name);
}

void FilterCompactCVars (TArray<cvar_t *> &cvars, DWORD filter)
{
	cvar_t *cvar = CVars;
	while (cvar)
	{
		if (cvar->flags & filter)
			cvars.Push (cvar);
		cvar = cvar->m_Next;
	}
	if (cvars.Size () > 0)
	{
		qsort (&cvars[0], cvars.Size (), sizeof(cvar_t *), sortcvars);
	}
}

void C_WriteCVars (byte **demo_p, DWORD filter, bool compact)
{
	cvar_t *cvar = CVars;
	byte *ptr = *demo_p;

	if (compact)
	{
		TArray<cvar_t *> cvars;
		ptr += sprintf ((char *)ptr, "\\\\%lux", filter);
		FilterCompactCVars (cvars, filter);
		while (cvars.Pop (cvar))
		{
			ptr += sprintf ((char *)ptr, "\\%s", cvar->string);
		}
	}
	else
	{
		cvar = CVars;
		while (cvar)
		{
			if (cvar->flags & filter)
			{
				ptr += sprintf ((char *)ptr, "\\%s\\%s",
								cvar->name, cvar->string);
			}
			cvar = cvar->m_Next;
		}
	}

	*demo_p = ptr + 1;
}

void C_ReadCVars (byte **demo_p)
{
	char *ptr = *((char **)demo_p);
	char *breakpt;

	if (*ptr++ != '\\')
		return;

	if (*ptr == '\\')
	{	// compact mode
		TArray<cvar_t *> cvars;
		cvar_t *cvar;
		DWORD filter;

		ptr++;
		breakpt = strchr (ptr, '\\');
		*breakpt = 0;
		filter = strtoul (ptr, NULL, 16);
		*breakpt = '\\';
		ptr = breakpt + 1;

		FilterCompactCVars (cvars, filter);

		while (cvars.Pop (cvar))
		{
			breakpt = strchr (ptr, '\\');
			if (breakpt)
				*breakpt = 0;
			cvar->Set (ptr);
			if (breakpt)
			{
				*breakpt = '\\';
				ptr = breakpt + 1;
			}
			else
				break;
		}
	}
	else
	{
		char *value;

		while ( (breakpt = strchr (ptr, '\\')) )
		{
			*breakpt = 0;
			value = breakpt + 1;
			if ( (breakpt = strchr (value, '\\')) )
				*breakpt = 0;

			cvar_set (ptr, value);

			*(value - 1) = '\\';
			if (breakpt)
			{
				*breakpt = '\\';
				ptr = breakpt + 1;
			}
			else
			{
				break;
			}
		}
	}
	*demo_p += strlen (*((char **)demo_p)) + 1;
}

static struct backup_s
{
	char *name, *string;
} CVarBackups[MAX_DEMOCVARS];

static int numbackedup = 0;

void C_BackupCVars (void)
{
	struct backup_s *backup = CVarBackups;
	cvar_t *cvar = CVars;

	while (cvar)
	{
		if (((cvar->flags & CVAR_SERVERINFO) || (cvar->flags & CVAR_DEMOSAVE))
			&& !(cvar->flags & CVAR_LATCH))
		{
			if (backup == &CVarBackups[MAX_DEMOCVARS])
				I_Error ("C_BackupDemoCVars: Too many cvars to save (%d)", MAX_DEMOCVARS);
			backup->name = copystring (cvar->name);
			backup->string = copystring (cvar->string);
			backup++;
		}
		cvar = cvar->m_Next;
	}
	numbackedup = backup - CVarBackups;
}

void C_RestoreCVars (void)
{
	struct backup_s *backup = CVarBackups;
	int i;

	for (i = numbackedup; i; i--, backup++)
	{
		cvar_set (backup->name, backup->string);
		delete[] backup->name;
		delete[] backup->string;
		backup->name = backup->string = NULL;
	}
	numbackedup = 0;
}

cvar_t *FindCVar (const char *var_name, cvar_t **prev)
{
	cvar_t *var;

	if (var_name == NULL)
		return NULL;

	var = CVars;
	*prev = NULL;
	while (var)
	{
		if (stricmp (var->name, var_name) == 0)
			break;
		*prev = var;
		var = var->m_Next;
	}
	return var;
}

void UnlatchCVars (void)
{
	cvar_t *var;

	var = CVars;
	while (var)
	{
		if (var->flags & (CVAR_MODIFIED | CVAR_LATCH))
		{
			unsigned oldflags = var->flags & ~CVAR_MODIFIED;
			var->flags &= ~(CVAR_LATCH | CVAR_SERVERINFO);
			if (var->m_LatchedString)
			{
				var->Set (var->m_LatchedString);
				delete[] var->m_LatchedString;
				var->m_LatchedString = NULL;
			}
			var->flags = oldflags;
		}
		var = var->m_Next;
	}
}

void C_SetCVarsToDefaults (void)
{
	cvar_t *cvar = CVars;

	while (cvar)
	{
		// Only default save-able cvars
		if (cvar->flags & CVAR_ARCHIVE)
			cvar->Set (cvar->m_Default);
		cvar = cvar->m_Next;
	}
}

void C_ArchiveCVars (void *f)
{
	cvar_t *cvar = CVars;

	while (cvar)
	{
		if (cvar->flags & CVAR_ARCHIVE)
		{
			fprintf ((FILE *)f, "set %s \"%s\"\n", cvar->name, cvar->string);
		}
		cvar = cvar->m_Next;
	}
}

BEGIN_COMMAND (set)
{
	if (argc != 3)
	{
		Printf (PRINT_HIGH, "usage: set <variable> <value>\n");
	}
	else
	{
		cvar_t *var, *prev;

		var = FindCVar (argv[1], &prev);
		if (!var)
			var = new cvar_t (argv[1], NULL, CVAR_AUTO | CVAR_UNSETTABLE | cvar_defflags);

		var->Set (argv[2]);

		if (var->flags & CVAR_NOSET)
			Printf (PRINT_HIGH, "%s is write protected.\n", argv[1]);
		else if (var->flags & CVAR_LATCH)
			Printf (PRINT_HIGH, "%s will be changed for next game.\n", argv[1]);
	}
}
END_COMMAND (set)

BEGIN_COMMAND (get)
{
	cvar_t *var, *prev;

	if (argc >= 2)
	{
		if ( (var = FindCVar (argv[1], &prev)) )
		{
			Printf (PRINT_HIGH, "\"%s\" is \"%s\"\n", var->name, var->string);
		}
		else
		{
			Printf (PRINT_HIGH, "\"%s\" is unset\n", argv[1]);
		}
	}
	else
	{
		Printf (PRINT_HIGH, "get: need variable name\n");
	}
}
END_COMMAND (get)

BEGIN_COMMAND (toggle)
{
	cvar_t *var, *prev;

	if (argc > 1)
	{
		if ( (var = FindCVar (argv[1], &prev)) )
		{
			var->Set ((float)(!var->value));
			Printf (PRINT_HIGH, "\"%s\" is \"%s\"\n", var->name, var->string);
		}
	}
}
END_COMMAND (toggle)

BEGIN_COMMAND (cvarlist)
{
	cvar_t *var = CVars;
	int count = 0;

	while (var)
	{
		unsigned flags = var->flags;

		count++;
		Printf (PRINT_HIGH, "%c%c%c%c %s \"%s\"\n",
				flags & CVAR_ARCHIVE ? 'A' : ' ',
				flags & CVAR_USERINFO ? 'U' : ' ',
				flags & CVAR_SERVERINFO ? 'S' : ' ',
				flags & CVAR_NOSET ? '-' :
					flags & CVAR_LATCH ? 'L' :
					flags & CVAR_UNSETTABLE ? '*' : ' ',
				var->name,
				var->string);
		var = var->m_Next;
	}
	Printf (PRINT_HIGH, "%d cvars\n", count);
}
END_COMMAND (cvarlist)
