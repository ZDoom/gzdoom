/*
** c_cvars.cpp
** Defines all the different console variable types
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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

#include <string.h>
#include <stdio.h>

#include "cmdlib.h"
#include "configfile.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "m_alloc.h"

#include "doomstat.h"
#include "c_cvars.h"
#include "d_player.h"

#include "d_netinf.h"

#include "i_system.h"
#include "v_palette.h"
#include "v_video.h"

struct FLatchedValue
{
	FBaseCVar *Variable;
	UCVarValue Value;
	ECVarType Type;
};

static TArray<FLatchedValue> LatchedValues;

bool FBaseCVar::m_DoNoSet = false;
bool FBaseCVar::m_UseCallback = false;

FBaseCVar *CVars = NULL;

int cvar_defflags;

FBaseCVar::FBaseCVar (const FBaseCVar &var)
{
	I_FatalError ("Use of cvar copy constructor");
}

FBaseCVar::FBaseCVar (const char *var_name, DWORD flags, void (*callback)(FBaseCVar &))
{
	FBaseCVar *var;

	var = FindCVar (var_name, NULL);

	m_Callback = callback;
	Flags = 0;
	Name = NULL;

	if (var_name)
	{
		C_AddTabCommand (var_name);
		Name = copystring (var_name);
		m_Next = CVars;
		CVars = this;
	}

	if (var)
	{
		ECVarType type;
		UCVarValue value;

		value = var->GetFavoriteRep (&type);
		ForceSet (value, type);

		if (var->Flags & CVAR_AUTO)
			delete var;
		else
			var->~FBaseCVar();

		Flags = flags;
	}
	else
	{
		Flags = flags | CVAR_ISDEFAULT;
	}
}

FBaseCVar::~FBaseCVar ()
{
	if (Name)
	{
		FBaseCVar *var, *prev;

		var = FindCVar (Name, &prev);

		if (var == this)
		{
			if (prev)
				prev->m_Next = m_Next;
			else
				CVars = m_Next;
		}
		delete[] Name;
	}
}

void FBaseCVar::ForceSet (UCVarValue value, ECVarType type)
{
	DoSet (value, type);
	if (Flags & CVAR_USERINFO)
		D_UserInfoChanged (this);
	if (m_UseCallback)
		Callback ();

	Flags &= ~CVAR_ISDEFAULT;
}

void FBaseCVar::SetGenericRep (UCVarValue value, ECVarType type)
{
	if ((Flags & CVAR_NOSET) && m_DoNoSet)
	{
		return;
	}
	else if ((Flags & CVAR_LATCH) && gamestate != GS_FULLCONSOLE && gamestate != GS_STARTUP)
	{
		FLatchedValue latch;

		latch.Variable = this;
		latch.Type = type;
		if (type != CVAR_String)
			latch.Value = value;
		else
			latch.Value.String = copystring(value.String);
		LatchedValues.Push (latch);
	}
	else if ((Flags & CVAR_SERVERINFO) && gamestate != GS_STARTUP && !demoplayback)
	{
		if (netgame && consoleplayer != Net_Arbitrator)
		{
			Printf ("Only player %d can change %s\n", Net_Arbitrator+1, Name);
			return;
		}
		D_SendServerInfoChange (this, value, type);
	}
	else
	{
		ForceSet (value, type);
	}
}

bool FBaseCVar::ToBool (UCVarValue value, ECVarType type)
{
	switch (type)
	{
	case CVAR_Bool:
		return value.Bool;

	case CVAR_Int:
		return !!value.Int;

	case CVAR_Float:
		return value.Float != 0.f;

	case CVAR_String:
		if (stricmp (value.String, "true") == 0)
			return true;
		else if (stricmp (value.String, "false") == 0)
			return false;
		else
			return !!strtol (value.String, NULL, 0);

	default:
		return false;
	}
}

int FBaseCVar::ToInt (UCVarValue value, ECVarType type)
{
	switch (type)
	{
	case CVAR_Bool:
		return (int)value.Bool;

	case CVAR_Int:
		return value.Int;

	case CVAR_Float:
		return (int)value.Float;

	case CVAR_String:
		return strtol (value.String, NULL, 0);

	default:
		return 0;
	}
}

float FBaseCVar::ToFloat (UCVarValue value, ECVarType type)
{
	switch (type)
	{
	case CVAR_Bool:
		return (float)value.Bool;

	case CVAR_Int:
		return (float)value.Int;

	case CVAR_Float:
		return value.Float;

	case CVAR_String:
		return strtod (value.String, NULL);

	default:
		return 0.f;
	}
}

static char cstrbuf[32];
static char truestr[] = "true";
static char falsestr[] = "false";

char *FBaseCVar::ToString (UCVarValue value, ECVarType type)
{
	switch (type)
	{
	case CVAR_Bool:
		return value.Bool ? truestr : falsestr;

	case CVAR_String:
		return value.String;

	case CVAR_Int:
		sprintf (cstrbuf, "%i", value.Int);
		break;

	case CVAR_Float:
		sprintf (cstrbuf, "%g", value.Float);
		break;

	default:
		strcpy (cstrbuf, "<huh?>");
		break;
	}
	return cstrbuf;
}

UCVarValue FBaseCVar::FromBool (bool value, ECVarType type)
{
	UCVarValue ret;

	switch (type)
	{
	case CVAR_Bool:
		ret.Bool = value;
		break;

	case CVAR_Int:
		ret.Int = value;
		break;

	case CVAR_Float:
		ret.Float = value;
		break;

	case CVAR_String:
		ret.String = value ? truestr : falsestr;
		break;

	default:
		break;
	}

	return ret;
}

UCVarValue FBaseCVar::FromInt (int value, ECVarType type)
{
	UCVarValue ret;

	switch (type)
	{
	case CVAR_Bool:
		ret.Bool = value != 0;
		break;

	case CVAR_Int:
		ret.Int = value;
		break;

	case CVAR_Float:
		ret.Float = (float)value;
		break;

	case CVAR_String:
		sprintf (cstrbuf, "%i", value);
		ret.String = cstrbuf;
		break;

	default:
		break;
	}

	return ret;
}

UCVarValue FBaseCVar::FromFloat (float value, ECVarType type)
{
	UCVarValue ret;

	switch (type)
	{
	case CVAR_Bool:
		ret.Bool = value != 0.f;
		break;

	case CVAR_Int:
		ret.Int = (int)value;
		break;

	case CVAR_Float:
		ret.Float = value;
		break;

	case CVAR_String:
		sprintf (cstrbuf, "%g", value);
		ret.String = cstrbuf;
		break;

	default:
		break;
	}

	return ret;
}

UCVarValue FBaseCVar::FromString (const char *value, ECVarType type)
{
	UCVarValue ret;

	switch (type)
	{
	case CVAR_Bool:
		if (stricmp (value, "true") == 0)
			ret.Bool = true;
		else if (stricmp (value, "false") == 0)
			ret.Bool = false;
		else
			ret.Bool = strtol (value, NULL, 0) != 0;
		break;

	case CVAR_Int:
		ret.Int = strtol (value, NULL, 0);
		break;

	case CVAR_Float:
		ret.Float = (float)strtod (value, NULL);
		break;

	case CVAR_String:
		ret.String = const_cast<char *>(value);
		break;

	default:
		break;
	}

	return ret;
}

FBaseCVar *cvar_set (const char *var_name, const char *val)
{
	FBaseCVar *var;

	if ( (var = FindCVar (var_name, NULL)) )
	{
		UCVarValue value;
		value.String = const_cast<char *>(val);
		var->SetGenericRep (value, CVAR_String);
	}

	return var;
}

FBaseCVar *cvar_forceset (const char *var_name, const char *val)
{
	FBaseCVar *var;
	UCVarValue vval;

	if ( (var = FindCVar (var_name, NULL)) )
	{
		vval.String = const_cast<char *>(val);
		var->ForceSet (vval, CVAR_String);
	}

	return var;
}

void FBaseCVar::EnableNoSet ()
{
	m_DoNoSet = true;
}

void FBaseCVar::EnableCallbacks ()
{
	m_UseCallback = true;
	FBaseCVar *cvar = CVars;

	while (cvar)
	{
		if (!(cvar->Flags & CVAR_NOINITCALL))
		{
			cvar->Callback ();
		}
		cvar = cvar->m_Next;
	}
}

//
// Boolean cvar implementation
//

FBoolCVar::FBoolCVar (const char *name, bool def, DWORD flags, void (*callback)(FBoolCVar &))
	: FBaseCVar (name, flags, reinterpret_cast<void (*)(FBaseCVar &)>(callback))
{
	DefaultValue = def;
	if (Flags & CVAR_ISDEFAULT)
		Value = def;
}

ECVarType FBoolCVar::GetRealType () const
{
	return CVAR_Bool;
}

UCVarValue FBoolCVar::GetGenericRep (ECVarType type) const
{
	return FromBool (Value, type);
}

UCVarValue FBoolCVar::GetFavoriteRep (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_Bool;
	ret.Bool = Value;
	return ret;
}

UCVarValue FBoolCVar::GetGenericRepDefault (ECVarType type) const
{
	return FromBool (DefaultValue, type);
}

UCVarValue FBoolCVar::GetFavoriteRepDefault (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_Bool;
	ret.Bool = DefaultValue;
	return ret;
}

void FBoolCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	DefaultValue = ToBool (value, type);
	if (Flags & CVAR_ISDEFAULT)
	{
		SetGenericRep (value, type);
		Flags |= CVAR_ISDEFAULT;
	}
}

void FBoolCVar::DoSet (UCVarValue value, ECVarType type)
{
	Value = ToBool (value, type);
}

//
// Integer cvar implementation
//

FIntCVar::FIntCVar (const char *name, int def, DWORD flags, void (*callback)(FIntCVar &))
	: FBaseCVar (name, flags, reinterpret_cast<void (*)(FBaseCVar &)>(callback))
{
	DefaultValue = def;
	if (Flags & CVAR_ISDEFAULT)
		Value = def;
}

ECVarType FIntCVar::GetRealType () const
{
	return CVAR_Int;
}

UCVarValue FIntCVar::GetGenericRep (ECVarType type) const
{
	return FromInt (Value, type);
}

UCVarValue FIntCVar::GetFavoriteRep (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_Int;
	ret.Int = Value;
	return ret;
}

UCVarValue FIntCVar::GetGenericRepDefault (ECVarType type) const
{
	return FromInt (DefaultValue, type);
}

UCVarValue FIntCVar::GetFavoriteRepDefault (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_Int;
	ret.Int = DefaultValue;
	return ret;
}

void FIntCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	DefaultValue = ToInt (value, type);
	if (Flags & CVAR_ISDEFAULT)
	{
		SetGenericRep (value, type);
		Flags |= CVAR_ISDEFAULT;
	}
}

void FIntCVar::DoSet (UCVarValue value, ECVarType type)
{
	Value = ToInt (value, type);
}

//
// Floating point cvar implementation
//

FFloatCVar::FFloatCVar (const char *name, float def, DWORD flags, void (*callback)(FFloatCVar &))
	: FBaseCVar (name, flags, reinterpret_cast<void (*)(FBaseCVar &)>(callback))
{
	DefaultValue = def;
	if (Flags & CVAR_ISDEFAULT)
		Value = def;
}

ECVarType FFloatCVar::GetRealType () const
{
	return CVAR_Float;
}

UCVarValue FFloatCVar::GetGenericRep (ECVarType type) const
{
	return FromFloat (Value, type);
}

UCVarValue FFloatCVar::GetFavoriteRep (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_Float;
	ret.Float = Value;
	return ret;
}

UCVarValue FFloatCVar::GetGenericRepDefault (ECVarType type) const
{
	return FromFloat (DefaultValue, type);
}

UCVarValue FFloatCVar::GetFavoriteRepDefault (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_Float;
	ret.Float = DefaultValue;
	return ret;
}

void FFloatCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	DefaultValue = ToFloat (value, type);
	if (Flags & CVAR_ISDEFAULT)
	{
		SetGenericRep (value, type);
		Flags |= CVAR_ISDEFAULT;
	}
}

void FFloatCVar::DoSet (UCVarValue value, ECVarType type)
{
	Value = ToFloat (value, type);
}

//
// String cvar implementation
//

FStringCVar::FStringCVar (const char *name, const char *def, DWORD flags, void (*callback)(FStringCVar &))
	: FBaseCVar (name, flags, reinterpret_cast<void (*)(FBaseCVar &)>(callback))
{
	DefaultValue = copystring (def);
	if (Flags & CVAR_ISDEFAULT)
		Value = copystring (def);
}

ECVarType FStringCVar::GetRealType () const
{
	return CVAR_String;
}

UCVarValue FStringCVar::GetGenericRep (ECVarType type) const
{
	return FromString (Value, type);
}

UCVarValue FStringCVar::GetFavoriteRep (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_String;
	ret.String = copystring (Value);
	return ret;
}

UCVarValue FStringCVar::GetGenericRepDefault (ECVarType type) const
{
	return FromString (DefaultValue, type);
}

UCVarValue FStringCVar::GetFavoriteRepDefault (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_String;
	ret.String = copystring (DefaultValue);
	return ret;
}

void FStringCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	if (DefaultValue)
		delete[] DefaultValue;
	DefaultValue = ToString (value, type);
	if (Flags & CVAR_ISDEFAULT)
	{
		SetGenericRep (value, type);
		Flags |= CVAR_ISDEFAULT;
	}
}

void FStringCVar::DoSet (UCVarValue value, ECVarType type)
{
	ReplaceString (&Value, ToString (value, type));
}

//
// Color cvar implementation
//

FColorCVar::FColorCVar (const char *name, int def, DWORD flags, void (*callback)(FColorCVar &))
	: FIntCVar (name, def, flags, reinterpret_cast<void (*)(FIntCVar &)>(callback))
{
}

ECVarType FColorCVar::GetRealType () const
{
	return CVAR_Color;
}

UCVarValue FColorCVar::GetGenericRep (ECVarType type) const
{
	return FromInt2 (Value, type);
}

UCVarValue FColorCVar::GetGenericRepDefault (ECVarType type) const
{
	return FromInt2 (DefaultValue, type);
}

void FColorCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	DefaultValue = ToInt2 (value, type);
	if (Flags & CVAR_ISDEFAULT)
	{
		SetGenericRep (value, type);
		Flags |= CVAR_ISDEFAULT;
	}
}

void FColorCVar::DoSet (UCVarValue value, ECVarType type)
{
	Value = ToInt2 (value, type);
	if (screen)
		Index = ColorMatcher.Pick (RPART(Value), GPART(Value), BPART(Value));
}

UCVarValue FColorCVar::FromInt2 (int value, ECVarType type)
{
	if (type == CVAR_String)
	{
		UCVarValue ret;
		char work[9];

		sprintf (work, "%02x %02x %02x",
			RPART(value), GPART(value), BPART(value));
		ret.String = copystring (work);
		return ret;
	}
	return FromInt (value, type);
}

int FColorCVar::ToInt2 (UCVarValue value, ECVarType type)
{
	int ret;

	if (type == CVAR_String)
	{
		char *string;
		// Only allow named colors after the screen exists (i.e. after
		// we've got some lumps loaded, so X11R6RGB can be read). Since
		// the only time this might be called before that is when loading
		// zdoom.cfg, this shouldn't be a problem.
		if (screen && (string = V_GetColorStringByName (value.String)) )
		{
			ret = V_GetColorFromString (NULL, string);
			delete[] string;
		}
		else
		{
			ret = V_GetColorFromString (NULL, value.String);
		}
	}
	else
	{
		ret = ToInt (value, type);
	}
	return ret;
}

void FBaseCVar::ResetColors ()
{
	FBaseCVar *var = CVars;

	while (var)
	{
		if (var->GetRealType () == CVAR_Color)
		{
			var->DoSet (var->GetGenericRep (CVAR_Int), CVAR_Int);
		}
		var = var->m_Next;
	}
}

void FBaseCVar::ResetToDefault ()
{
	if (!(Flags & CVAR_ISDEFAULT))
	{
		UCVarValue val;
		ECVarType type;

		val = GetFavoriteRepDefault (&type);
		SetGenericRep (val, type);
		Flags |= CVAR_ISDEFAULT;
	}
}

//
// Flag cvar implementation
//
// This type of cvar is not a "real" cvar. Instead, it gets and sets
// the value of a FIntCVar, modifying it bit-by-bit. As such, it has
// no default, and is not written to the .cfg or transferred around
// the network. The "host" cvar is responsible for that.
//

FFlagCVar::FFlagCVar (const char *name, FIntCVar &realvar, DWORD bitval)
	: FBaseCVar (name, 0, NULL),
	ValueVar (realvar),
	BitVal (bitval)
{
	Flags &= ~CVAR_ISDEFAULT;
}

ECVarType FFlagCVar::GetRealType () const
{
	return CVAR_Dummy;
}

UCVarValue FFlagCVar::GetGenericRep (ECVarType type) const
{
	return FromBool ((ValueVar & BitVal) != 0, type);
}

UCVarValue FFlagCVar::GetFavoriteRep (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_Bool;
	ret.Bool = (ValueVar & BitVal) != 0;
	return ret;
}

UCVarValue FFlagCVar::GetGenericRepDefault (ECVarType type) const
{
	ECVarType dummy;
	UCVarValue def;
	def = ValueVar.GetFavoriteRepDefault (&dummy);
	return FromBool ((def.Int & BitVal) != 0, type);
}

UCVarValue FFlagCVar::GetFavoriteRepDefault (ECVarType *type) const
{
	ECVarType dummy;
	UCVarValue def;
	def = ValueVar.GetFavoriteRepDefault (&dummy);
	def.Bool = (def.Int & BitVal) != 0;
	*type = CVAR_Bool;
	return def;
}

void FFlagCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	bool newdef = ToBool (value, type);
	ECVarType dummy;
	UCVarValue def;
	def = ValueVar.GetFavoriteRepDefault (&dummy);
	if (newdef)
		def.Int |= BitVal;
	else
		def.Int &= ~BitVal;
	ValueVar.SetGenericRepDefault (def, CVAR_Int);
}

void FFlagCVar::DoSet (UCVarValue value, ECVarType type)
{
	bool newval = ToBool (value, type);
	ECVarType dummy;
	UCVarValue val;
	val = ValueVar.GetFavoriteRep (&dummy);
	if (newval)
		val.Int |= BitVal;
	else
		val.Int &= ~BitVal;
	ValueVar = val.Int;
}

////////////////////////////////////////////////////////////////////////
static int STACK_ARGS sortcvars (const void *a, const void *b)
{
	return strcmp (((*(FBaseCVar **)a))->GetName(), ((*(FBaseCVar **)b))->GetName());
}

void FilterCompactCVars (TArray<FBaseCVar *> &cvars, DWORD filter)
{
	FBaseCVar *cvar = CVars;
	while (cvar)
	{
		if (cvar->Flags & filter)
			cvars.Push (cvar);
		cvar = cvar->m_Next;
	}
	if (cvars.Size () > 0)
	{
		cvars.ShrinkToFit ();
		qsort (&cvars[0], cvars.Size (), sizeof(FBaseCVar *), sortcvars);
	}
}

void C_WriteCVars (byte **demo_p, DWORD filter, bool compact)
{
	FBaseCVar *cvar = CVars;
	byte *ptr = *demo_p;

	if (compact)
	{
		TArray<FBaseCVar *> cvars;
		ptr += sprintf ((char *)ptr, "\\\\%lux", filter);
		FilterCompactCVars (cvars, filter);
		while (cvars.Pop (cvar))
		{
			UCVarValue val = cvar->GetGenericRep (CVAR_String);
			ptr += sprintf ((char *)ptr, "\\%s", val.String);
		}
	}
	else
	{
		cvar = CVars;
		while (cvar)
		{
			if (cvar->Flags & filter)
			{
				UCVarValue val = cvar->GetGenericRep (CVAR_String);
				ptr += sprintf ((char *)ptr, "\\%s\\%s",
					cvar->GetName (), val.String);
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
		TArray<FBaseCVar *> cvars;
		FBaseCVar *cvar;
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
			UCVarValue val;
			breakpt = strchr (ptr, '\\');
			if (breakpt)
				*breakpt = 0;
			val.String = ptr;
			cvar->ForceSet (val, CVAR_String);
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
	FBaseCVar *cvar = CVars;

	while (cvar)
	{
		if ((cvar->Flags & (CVAR_SERVERINFO|CVAR_DEMOSAVE))
			&& !(cvar->Flags & CVAR_LATCH))
		{
			if (backup == &CVarBackups[MAX_DEMOCVARS])
				I_Error ("C_BackupDemoCVars: Too many cvars to save (%d)", MAX_DEMOCVARS);
			backup->name = copystring (cvar->GetName());
			backup->string = copystring (cvar->GetGenericRep (CVAR_String).String);
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

FBaseCVar *FindCVar (const char *var_name, FBaseCVar **prev)
{
	FBaseCVar *var;
	FBaseCVar *dummy;

	if (var_name == NULL)
		return NULL;

	if (prev == NULL)
		prev = &dummy;

	var = CVars;
	*prev = NULL;
	while (var)
	{
		if (stricmp (var->GetName (), var_name) == 0)
			break;
		*prev = var;
		var = var->m_Next;
	}
	return var;
}

FBaseCVar *FindCVarSub (const char *var_name, int namelen)
{
	FBaseCVar *var;

	if (var_name == NULL)
		return NULL;

	var = CVars;
	while (var)
	{
		const char *probename = var->GetName ();

		if (strnicmp (probename, var_name, namelen) == 0 &&
			probename[namelen] == 0)
		{
			break;
		}
		var = var->m_Next;
	}
	return var;
}

void UnlatchCVars (void)
{
	FLatchedValue var;

	while (LatchedValues.Pop (var))
	{
		DWORD oldflags = var.Variable->Flags;
		var.Variable->Flags &= ~(CVAR_LATCH | CVAR_SERVERINFO);
		var.Variable->SetGenericRep (var.Value, var.Type);
		if (var.Type == CVAR_String)
			delete[] var.Value.String;
		var.Variable->Flags = oldflags;
	}
}

void C_SetCVarsToDefaults (void)
{
	FBaseCVar *cvar = CVars;

	while (cvar)
	{
		// Only default save-able cvars
		if (cvar->Flags & CVAR_ARCHIVE)
		{
			UCVarValue val;
			ECVarType type;
			val = cvar->GetFavoriteRepDefault (&type);
			cvar->SetGenericRep (val, type);
		}
		cvar = cvar->m_Next;
	}
}

void C_ArchiveCVars (FConfigFile *f, int type)
{
	// type 0: Game-specific cvars
	// type 1: Global cvars
	// type 2: Unknown cvars
	// type 3: Unknown global cvars
	// type 4: User info cvars
	static const DWORD filters[5] =
	{
		CVAR_ARCHIVE,
		CVAR_ARCHIVE|CVAR_GLOBALCONFIG,
		CVAR_ARCHIVE|CVAR_AUTO,
		CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_AUTO,
		CVAR_ARCHIVE|CVAR_USERINFO
	};

	FBaseCVar *cvar = CVars;
	DWORD filter;
	
	filter = filters[type];

	while (cvar)
	{
		if ((cvar->Flags & (CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_AUTO|CVAR_USERINFO)) == filter)
		{
			UCVarValue val;
			val = cvar->GetGenericRep (CVAR_String);
			f->SetValueForKey (cvar->GetName (), val.String);
		}
		cvar = cvar->m_Next;
	}
}

void FBaseCVar::CmdSet (const char *newval)
{
	UCVarValue val;

	// Casting away the const is safe in this case.
	val.String = const_cast<char *>(newval);	
	SetGenericRep (val, CVAR_String);

	if (GetFlags() & CVAR_NOSET)
		Printf ("%s is write protected.\n", GetName());
	else if (GetFlags() & CVAR_LATCH)
		Printf ("%s will be changed for next game.\n", GetName());
}

CCMD (set)
{
	if (argv.argc() != 3)
	{
		Printf ("usage: set <variable> <value>\n");
	}
	else
	{
		FBaseCVar *var;

		var = FindCVar (argv[1], NULL);
		if (var == NULL)
			var = new FStringCVar (argv[1], NULL, CVAR_AUTO | CVAR_UNSETTABLE | cvar_defflags);

		var->CmdSet (argv[2]);
	}
}

CCMD (unset)
{
	if (argv.argc() != 2)
	{
		Printf ("usage: unset <variable>\n");
	}
	else
	{
		FBaseCVar *var = FindCVar (argv[1], NULL);
		if (var != NULL)
		{
			if (var->GetFlags() & CVAR_UNSETTABLE)
			{
				delete var;
			}
			else
			{
				Printf ("Cannot unset %s\n", argv[1]);
			}
		}
	}
}

CCMD (get)
{
	FBaseCVar *var, *prev;

	if (argv.argc() >= 2)
	{
		if ( (var = FindCVar (argv[1], &prev)) )
		{
			UCVarValue val;
			val = var->GetGenericRep (CVAR_String);
			Printf ("\"%s\" is \"%s\"\n", var->GetName(), val.String);
		}
		else
		{
			Printf ("\"%s\" is unset\n", argv[1]);
		}
	}
	else
	{
		Printf ("get: need variable name\n");
	}
}

CCMD (toggle)
{
	FBaseCVar *var, *prev;
	UCVarValue val;

	if (argv.argc() > 1)
	{
		if ( (var = FindCVar (argv[1], &prev)) )
		{
			val = var->GetGenericRep (CVAR_Bool);
			val.Bool = !val.Bool;
			var->SetGenericRep (val, CVAR_Bool);
			Printf ("\"%s\" is \"%s\"\n", var->GetName(),
				val.Bool ? "true" : "false");
		}
	}
}

void FBaseCVar::ListVars (const char *filter)
{
	FBaseCVar *var = CVars;
	int count = 0;

	while (var)
	{
		DWORD flags = var->GetFlags();
		UCVarValue val;

		count++;
		val = var->GetGenericRep (CVAR_String);
		Printf ("%c%c%c %s \"%s\"\n",
				flags & CVAR_ARCHIVE ? 'A' : ' ',
				flags & CVAR_USERINFO ? 'U' :
					flags & CVAR_SERVERINFO ? 'S' :
					flags & CVAR_AUTO ? 'C' : ' ',
				flags & CVAR_NOSET ? '-' :
					flags & CVAR_LATCH ? 'L' :
					flags & CVAR_UNSETTABLE ? '*' : ' ',
				var->GetName(),
				var->GetGenericRep (CVAR_String).String);
		var = var->m_Next;
	}
	Printf ("%d cvars\n", count);
}

CCMD (cvarlist)
{
	if (argv.argc() == 1)
	{
		FBaseCVar::ListVars (NULL);
	}
	else
	{
		FBaseCVar::ListVars (argv[1]);
	}
}

CCMD (archivecvar)
{

	if (argv.argc() == 1)
	{
		Printf ("Usage: archivecvar <cvar>\n");
	}
	else
	{
		FBaseCVar *var = FindCVar (argv[1], NULL);

		if (var != NULL && (var->GetFlags() & CVAR_AUTO))
		{
			var->SetArchiveBit ();
		}
	}
}
