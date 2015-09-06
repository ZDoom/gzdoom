/*
** c_cvars.cpp
** Defines all the different console variable types
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

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cmdlib.h"
#include "configfile.h"
#include "c_console.h"
#include "c_dispatch.h"

#include "doomstat.h"
#include "c_cvars.h"
#include "d_player.h"

#include "d_netinf.h"

#include "i_system.h"
#include "v_palette.h"
#include "v_video.h"
#include "colormatcher.h"

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
		C_RemoveTabCommand(Name);
		delete[] Name;
	}
}

void FBaseCVar::ForceSet (UCVarValue value, ECVarType type, bool nouserinfosend)
{
	DoSet (value, type);
	if ((Flags & CVAR_USERINFO) && !nouserinfosend && !(Flags & CVAR_IGNORE))
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
		if (netgame && !players[consoleplayer].settings_controller)
		{
			Printf ("Only setting controllers can change %s\n", Name);
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

	case CVAR_GUID:
		return false;

	default:
		return false;
	}
}

int FBaseCVar::ToInt (UCVarValue value, ECVarType type)
{
	int res;
#if __GNUC__ <= 2
	float tmp;
#endif

	switch (type)
	{
	case CVAR_Bool:			res = (int)value.Bool; break;
	case CVAR_Int:			res = value.Int; break;
#if __GNUC__ <= 2
	case CVAR_Float:		tmp = value.Float; res = (int)tmp; break;
#else
	case CVAR_Float:		res = (int)value.Float; break;
#endif
	case CVAR_String:		
		{
			if (stricmp (value.String, "true") == 0)
				res = 1;
			else if (stricmp (value.String, "false") == 0)
				res = 0;
			else
				res = strtol (value.String, NULL, 0); 
			break;
		}
	case CVAR_GUID:			res = 0; break;
	default:				res = 0; break;
	}
	return res;
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
		return (float)strtod (value.String, NULL);

	case CVAR_GUID:
		return 0.f;

	default:
		return 0.f;
	}
}

static char cstrbuf[40];
static GUID cGUID;
static char truestr[] = "true";
static char falsestr[] = "false";

const char *FBaseCVar::ToString (UCVarValue value, ECVarType type)
{
	switch (type)
	{
	case CVAR_Bool:
		return value.Bool ? truestr : falsestr;

	case CVAR_String:
		return value.String;

	case CVAR_Int:
		mysnprintf (cstrbuf, countof(cstrbuf), "%i", value.Int);
		break;

	case CVAR_Float:
		mysnprintf (cstrbuf, countof(cstrbuf), "%g", value.Float);
		break;

	case CVAR_GUID:
		FormatGUID (cstrbuf, countof(cstrbuf), *value.pGUID);
		break;

	default:
		strcpy (cstrbuf, "<huh?>");
		break;
	}
	return cstrbuf;
}

const GUID *FBaseCVar::ToGUID (UCVarValue value, ECVarType type)
{
	UCVarValue trans;

	switch (type)
	{
	case CVAR_String:
		trans = FromString (value.String, CVAR_GUID);
		return trans.pGUID;

	case CVAR_GUID:
		return value.pGUID;

	default:
		return NULL;
	}
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

	case CVAR_GUID:
		ret.pGUID = NULL;
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
		mysnprintf (cstrbuf, countof(cstrbuf), "%i", value);
		ret.String = cstrbuf;
		break;

	case CVAR_GUID:
		ret.pGUID = NULL;
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
		mysnprintf (cstrbuf, countof(cstrbuf), "%g", value);
		ret.String = cstrbuf;
		break;

	case CVAR_GUID:
		ret.pGUID = NULL;
		break;

	default:
		break;
	}

	return ret;
}

static BYTE HexToByte (const char *hex)
{
	BYTE v = 0;
	for (int i = 0; i < 2; ++i)
	{
		v <<= 4;
		if (hex[i] >= '0' && hex[i] <= '9')
		{
			v += hex[i] - '0';
		}
		else if (hex[i] >= 'A' && hex[i] <= 'F')
		{
			v += hex[i] - 'A';
		}
		else // The string is already verified to contain valid hexits
		{
			v += hex[i] - 'a';
		}
	}
	return v;
}

UCVarValue FBaseCVar::FromString (const char *value, ECVarType type)
{
	UCVarValue ret;
	int i;

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
		if (stricmp (value, "true") == 0)
			ret.Int = 1;
		else if (stricmp (value, "false") == 0)
			ret.Int = 0;
		else
			ret.Int = strtol (value, NULL, 0);
		break;

	case CVAR_Float:
		ret.Float = (float)strtod (value, NULL);
		break;

	case CVAR_String:
		ret.String = const_cast<char *>(value);
		break;

	case CVAR_GUID:
		// {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
		// 01234567890123456789012345678901234567
		// 0         1         2         3

		ret.pGUID = NULL;
		if (value == NULL)
		{
			break;
		}
		for (i = 0; value[i] != 0 && i < 38; i++)
		{
			switch (i)
			{
			case 0:
				if (value[i] != '{')
					break;
			case 9:
			case 14:
			case 19:
			case 24:
				if (value[i] != '-')
					break;
			case 37:
				if (value[i] != '}')
					break;
			default:
				if (value[i] < '0' || 
					(value[i] > '9' && value[i] < 'A') || 
					(value[i] > 'F' && value[i] < 'a') || 
					value[i] > 'f')
					break;
			}
		}
		if (i == 38 && value[i] == 0)
		{
			cGUID.Data1 = strtoul (value + 1, NULL, 16);
			cGUID.Data2 = (WORD)strtoul (value + 10, NULL, 16);
			cGUID.Data3 = (WORD)strtoul (value + 15, NULL, 16);
			cGUID.Data4[0] = HexToByte (value + 20);
			cGUID.Data4[1] = HexToByte (value + 22);
			cGUID.Data4[2] = HexToByte (value + 25);
			cGUID.Data4[3] = HexToByte (value + 27);
			cGUID.Data4[4] = HexToByte (value + 29);
			cGUID.Data4[5] = HexToByte (value + 31);
			cGUID.Data4[6] = HexToByte (value + 33);
			cGUID.Data4[7] = HexToByte (value + 35);
			ret.pGUID = &cGUID;
		}
		break;

	default:
		break;
	}

	return ret;
}

UCVarValue FBaseCVar::FromGUID (const GUID &guid, ECVarType type)
{
	UCVarValue ret;

	switch (type)
	{
	case CVAR_Bool:
		ret.Bool = false;
		break;

	case CVAR_Int:
		ret.Int = 0;
		break;

	case CVAR_Float:
		ret.Float = 0.f;
		break;

	case CVAR_String:
		ret.pGUID = &guid;
		ret.String = ToString (ret, CVAR_GUID);
		break;

	case CVAR_GUID:
		ret.pGUID = &guid;
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

void FBaseCVar::DisableCallbacks ()
{
	m_UseCallback = false;
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
	else
		Value = NULL;
}

FStringCVar::~FStringCVar ()
{
	if (Value != NULL)
	{
		delete[] Value;
	}
	delete[] DefaultValue;
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
	ret.String = Value;
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
	ret.String = DefaultValue;
	return ret;
}

void FStringCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	ReplaceString(&DefaultValue, ToString(value, type));
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
		mysnprintf (cstrbuf, countof(cstrbuf), "%02x %02x %02x",
			RPART(value), GPART(value), BPART(value));
		ret.String = cstrbuf;
		return ret;
	}
	return FromInt (value, type);
}

int FColorCVar::ToInt2 (UCVarValue value, ECVarType type)
{
	int ret;

	if (type == CVAR_String)
	{
		FString string;
		// Only allow named colors after the screen exists (i.e. after
		// we've got some lumps loaded, so X11R6RGB can be read). Since
		// the only time this might be called before that is when loading
		// zdoom.ini, this shouldn't be a problem.
		if (screen && !(string = V_GetColorStringByName (value.String)).IsEmpty() )
		{
			ret = V_GetColorFromString (NULL, string);
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

//
// GUID cvar implementation
//

FGUIDCVar::FGUIDCVar (const char *name, const GUID *def, DWORD flags, void (*callback)(FGUIDCVar &))
: FBaseCVar (name, flags, reinterpret_cast<void (*)(FBaseCVar &)>(callback))
{
	if (def != NULL)
	{
		DefaultValue = *def;
		if (Flags & CVAR_ISDEFAULT)
			Value = *def;
	}
	else
	{
		memset (&Value, 0, sizeof(DefaultValue));
		memset (&DefaultValue, 0, sizeof(DefaultValue));
	}
}

ECVarType FGUIDCVar::GetRealType () const
{
	return CVAR_GUID;
}

UCVarValue FGUIDCVar::GetGenericRep (ECVarType type) const
{
	return FromGUID (Value, type);
}

UCVarValue FGUIDCVar::GetFavoriteRep (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_GUID;
	ret.pGUID = &Value;
	return ret;
}

UCVarValue FGUIDCVar::GetGenericRepDefault (ECVarType type) const
{
	return FromGUID (DefaultValue, type);
}

UCVarValue FGUIDCVar::GetFavoriteRepDefault (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_GUID;
	ret.pGUID = &DefaultValue;
	return ret;
}

void FGUIDCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	const GUID *guid = ToGUID (value, type);
	if (guid != NULL)
	{
		Value = *guid;
		if (Flags & CVAR_ISDEFAULT)
		{
			SetGenericRep (value, type);
			Flags |= CVAR_ISDEFAULT;
		}
	}
}

void FGUIDCVar::DoSet (UCVarValue value, ECVarType type)
{
	const GUID *guid = ToGUID (value, type);
	if (guid != NULL)
	{
		Value = *guid;
	}
}

//
// More base cvar stuff
//

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
	int bit;

	Flags &= ~CVAR_ISDEFAULT;

	assert (bitval != 0);

	bit = 0;
	while ((bitval >>= 1) != 0)
	{
		++bit;
	}
	BitNum = bit;

	assert ((1u << BitNum) == BitVal);
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

	// Server cvars that get changed by this need to use a special message, because
	// changes are not processed until the next net update. This is a problem with
	// exec scripts because all flags will base their changes off of the value of
	// the "master" cvar at the time the script was run, overriding any changes
	// another flag might have made to the same cvar earlier in the script.
	if ((ValueVar.GetFlags() & CVAR_SERVERINFO) && gamestate != GS_STARTUP && !demoplayback)
	{
		if (netgame && !players[consoleplayer].settings_controller)
		{
			Printf ("Only setting controllers can change %s\n", Name);
			return;
		}
		D_SendServerFlagChange (&ValueVar, BitNum, newval);
	}
	else
	{
		int val = *ValueVar;
		if (newval)
			val |= BitVal;
		else
			val &= ~BitVal;
		ValueVar = val;
	}
}

//
// Mask cvar implementation
//
// Similar to FFlagCVar but can have multiple bits
//

FMaskCVar::FMaskCVar (const char *name, FIntCVar &realvar, DWORD bitval)
: FBaseCVar (name, 0, NULL),
ValueVar (realvar),
BitVal (bitval)
{
	int bit;

	Flags &= ~CVAR_ISDEFAULT;

	assert (bitval != 0);

	bit = 0;
	while ((bitval & 1) == 0)
	{
		++bit;
		bitval >>= 1;
	}
	BitNum = bit;
}

ECVarType FMaskCVar::GetRealType () const
{
	return CVAR_Dummy;
}

UCVarValue FMaskCVar::GetGenericRep (ECVarType type) const
{
	return FromInt ((ValueVar & BitVal) >> BitNum, type);
}

UCVarValue FMaskCVar::GetFavoriteRep (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_Int;
	ret.Int = (ValueVar & BitVal) >> BitNum;
	return ret;
}

UCVarValue FMaskCVar::GetGenericRepDefault (ECVarType type) const
{
	ECVarType dummy;
	UCVarValue def;
	def = ValueVar.GetFavoriteRepDefault (&dummy);
	return FromInt ((def.Int & BitVal) >> BitNum, type);
}

UCVarValue FMaskCVar::GetFavoriteRepDefault (ECVarType *type) const
{
	ECVarType dummy;
	UCVarValue def;
	def = ValueVar.GetFavoriteRepDefault (&dummy);
	def.Int = (def.Int & BitVal) >> BitNum;
	*type = CVAR_Int;
	return def;
}

void FMaskCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	int val = ToInt(value, type) << BitNum;
	ECVarType dummy;
	UCVarValue def;
	def = ValueVar.GetFavoriteRepDefault (&dummy);
	def.Int &= ~BitVal;
	def.Int |= val;
	ValueVar.SetGenericRepDefault (def, CVAR_Int);
}

void FMaskCVar::DoSet (UCVarValue value, ECVarType type)
{
	int val = ToInt(value, type) << BitNum;

	// Server cvars that get changed by this need to use a special message, because
	// changes are not processed until the next net update. This is a problem with
	// exec scripts because all flags will base their changes off of the value of
	// the "master" cvar at the time the script was run, overriding any changes
	// another flag might have made to the same cvar earlier in the script.
	if ((ValueVar.GetFlags() & CVAR_SERVERINFO) && gamestate != GS_STARTUP && !demoplayback)
	{
		if (netgame && !players[consoleplayer].settings_controller)
		{
			Printf ("Only setting controllers can change %s\n", Name);
			return;
		}
		// Ugh...
		for(int i = 0; i < 32; i++)
		{
			if (BitVal & (1<<i))
			{
				D_SendServerFlagChange (&ValueVar, i, !!(val & (1<<i)));
			}
		}
	}
	else
	{
		int vval = *ValueVar;
		vval &= ~BitVal;
		vval |= val;
		ValueVar = vval;
	}
}


////////////////////////////////////////////////////////////////////////
static int STACK_ARGS sortcvars (const void *a, const void *b)
{
	return strcmp (((*(FBaseCVar **)a))->GetName(), ((*(FBaseCVar **)b))->GetName());
}

void FilterCompactCVars (TArray<FBaseCVar *> &cvars, DWORD filter)
{
	// Accumulate all cvars that match the filter flags.
	for (FBaseCVar *cvar = CVars; cvar != NULL; cvar = cvar->m_Next)
	{
		if ((cvar->Flags & filter) && !(cvar->Flags & CVAR_IGNORE))
			cvars.Push(cvar);
	}
	// Now sort them, so they're in a deterministic order and not whatever
	// order the linker put them in.
	if (cvars.Size() > 0)
	{
		qsort(&cvars[0], cvars.Size(), sizeof(FBaseCVar *), sortcvars);
	}
}

void C_WriteCVars (BYTE **demo_p, DWORD filter, bool compact)
{
	FString dump = C_GetMassCVarString(filter, compact);
	size_t dumplen = dump.Len() + 1;	// include terminating \0
	memcpy(*demo_p, dump.GetChars(), dumplen);
	*demo_p += dumplen;
}

FString C_GetMassCVarString (DWORD filter, bool compact)
{
	FBaseCVar *cvar;
	FString dump;

	if (compact)
	{
		TArray<FBaseCVar *> cvars;
		dump.AppendFormat("\\\\%ux", filter);
		FilterCompactCVars(cvars, filter);
		while (cvars.Pop (cvar))
		{
			UCVarValue val = cvar->GetGenericRep(CVAR_String);
			dump << '\\' << val.String;
		}
	}
	else
	{
		for (cvar = CVars; cvar != NULL; cvar = cvar->m_Next)
		{
			if ((cvar->Flags & filter) && !(cvar->Flags & (CVAR_NOSAVE|CVAR_IGNORE)))
			{
				UCVarValue val = cvar->GetGenericRep(CVAR_String);
				dump << '\\' << cvar->GetName() << '\\' << val.String;
			}
		}
	}
	return dump;
}

void C_ReadCVars (BYTE **demo_p)
{
	char *ptr = *((char **)demo_p);
	char *breakpt;

	if (*ptr++ != '\\')
		return;

	if (*ptr == '\\')
	{       // compact mode
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

struct FCVarBackup
{
	FString Name, String;
};
static TArray<FCVarBackup> CVarBackups;

void C_BackupCVars (void)
{
	assert(CVarBackups.Size() == 0);
	CVarBackups.Clear();

	FCVarBackup backup;

	for (FBaseCVar *cvar = CVars; cvar != NULL; cvar = cvar->m_Next)
	{
		if ((cvar->Flags & (CVAR_SERVERINFO|CVAR_DEMOSAVE)) && !(cvar->Flags & CVAR_LATCH))
		{
			backup.Name = cvar->GetName();
			backup.String = cvar->GetGenericRep(CVAR_String).String;
			CVarBackups.Push(backup);
		}
	}
}

void C_RestoreCVars (void)
{
	for (unsigned int i = 0; i < CVarBackups.Size(); ++i)
	{
		cvar_set(CVarBackups[i].Name, CVarBackups[i].String);
	}
	C_ForgetCVars();
}

void C_ForgetCVars (void)
{
	CVarBackups.Clear();
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

//===========================================================================
//
// C_CreateCVar
//
// Create a new cvar with the specified name and type. It should not already
// exist.
//
//===========================================================================

FBaseCVar *C_CreateCVar(const char *var_name, ECVarType var_type, DWORD flags)
{
	assert(FindCVar(var_name, NULL) == NULL);
	flags |= CVAR_AUTO;
	switch (var_type)
	{
	case CVAR_Bool:		return new FBoolCVar(var_name, 0, flags);
	case CVAR_Int:		return new FIntCVar(var_name, 0, flags);
	case CVAR_Float:	return new FFloatCVar(var_name, 0, flags);
	case CVAR_String:	return new FStringCVar(var_name, NULL, flags);
	case CVAR_Color:	return new FColorCVar(var_name, 0, flags);
	default:			return NULL;
	}
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

void DestroyCVarsFlagged (DWORD flags)
{
	FBaseCVar *cvar = CVars;
	FBaseCVar *next = cvar;

	while(cvar)
	{
		next = cvar->m_Next;

		if(cvar->Flags & flags)
			delete cvar;

		cvar = next;
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

void C_ArchiveCVars (FConfigFile *f, uint32 filter)
{
	FBaseCVar *cvar = CVars;

	while (cvar)
	{
		if ((cvar->Flags &
			(CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_MOD|CVAR_AUTO|CVAR_USERINFO|CVAR_SERVERINFO|CVAR_NOSAVE))
			== filter)
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

void FBaseCVar::ListVars (const char *filter, bool plain)
{
	FBaseCVar *var = CVars;
	int count = 0;

	while (var)
	{
		if (CheckWildcards (filter, var->GetName()))
		{
			DWORD flags = var->GetFlags();
			if (plain)
			{ // plain formatting does not include user-defined cvars
				if (!(flags & CVAR_UNSETTABLE))
				{
					++count;
					Printf ("%s : %s\n", var->GetName(), var->GetGenericRep(CVAR_String).String);
				}
			}
			else
			{
				++count;
				Printf ("%c%c%c%c%c %s = %s\n",
					flags & CVAR_ARCHIVE ? 'A' : ' ',
					flags & CVAR_USERINFO ? 'U' :
						flags & CVAR_SERVERINFO ? 'S' :
						flags & CVAR_AUTO ? 'C' : ' ',
					flags & CVAR_NOSET ? '-' :
						flags & CVAR_LATCH ? 'L' :
						flags & CVAR_UNSETTABLE ? '*' : ' ',
					flags & CVAR_MOD ? 'M' : ' ',
					flags & CVAR_IGNORE ? 'X' : ' ',
					var->GetName(),
					var->GetGenericRep(CVAR_String).String);
			}
		}
		var = var->m_Next;
	}
	Printf ("%d cvars\n", count);
}

CCMD (cvarlist)
{
	if (argv.argc() == 1)
	{
		FBaseCVar::ListVars (NULL, false);
	}
	else
	{
		FBaseCVar::ListVars (argv[1], false);
	}
}

CCMD (cvarlistplain)
{
	FBaseCVar::ListVars (NULL, true);
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
