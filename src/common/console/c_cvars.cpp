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
#include <assert.h>

#include "cmdlib.h"
#include "configfile.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "c_cvars.h"
#include "engineerrors.h"
#include "printf.h"
#include "palutil.h"
#include "i_interface.h"
#include "gstrings.h"

#include "dobject.h"
#include "dobjtype.h"
#include "dobjgc.h"

#include "vm.h"



struct FLatchedValue
{
	FBaseCVar *Variable;
	UCVarValue Value;
	ECVarType Type;
	bool UnsafeContext;
};

static TArray<FLatchedValue> LatchedValues;

int cvar_defflags;


static ConsoleCallbacks* callbacks;

// Install game-specific handlers, mainly to deal with serverinfo and userinfo CVARs.
// This is to keep the console independent of game implementation details for easier reusability.
void C_InstallHandlers(ConsoleCallbacks* cb)
{
	callbacks = cb;
}

void C_InitCVars(int which)
{
	AutoSegs::CVarDecl.ForEach([](FCVarDecl* cvInfo)
	{
		FBaseCVar* newcvar;
		switch(cvInfo->type)
		{
		default:
			return;
			
		case CVAR_Int:
		{
			using callbacktype = void (*)(FIntCVar &);
			newcvar = new FIntCVar(cvInfo->name, cvInfo->defaultval.Int, cvInfo->flags, reinterpret_cast<callbacktype>(cvInfo->callbackp), cvInfo->description);
			break;
		}
		case CVAR_Bool:
		{
			using callbacktype = void (*)(FBoolCVar &);
			newcvar = new FBoolCVar(cvInfo->name, cvInfo->defaultval.Bool, cvInfo->flags, reinterpret_cast<callbacktype>(cvInfo->callbackp), cvInfo->description);
			break;
		}
		case CVAR_Float:
		{
			using callbacktype = void (*)(FFloatCVar &);
			newcvar = new FFloatCVar(cvInfo->name, cvInfo->defaultval.Float, cvInfo->flags, reinterpret_cast<callbacktype>(cvInfo->callbackp), cvInfo->description);
			break;
		}
		case CVAR_String:
		{
			using callbacktype = void (*)(FStringCVar &);
			newcvar = new FStringCVar(cvInfo->name, cvInfo->defaultval.String, cvInfo->flags, reinterpret_cast<callbacktype>(cvInfo->callbackp), cvInfo->description);
			break;
		}
		case CVAR_Color:
		{
			using callbacktype = void (*)(FColorCVar &);
			newcvar = new FColorCVar(cvInfo->name, cvInfo->defaultval.Int, cvInfo->flags, reinterpret_cast<callbacktype>(cvInfo->callbackp), cvInfo->description);
			break;
		}

		}
		*(void**)cvInfo->refAddr = newcvar;
	});
	AutoSegs::CVarDecl.ForEach([](FCVarDecl* cvInfo)
		{
			FBaseCVar* newcvar;
			switch (cvInfo->type)
			{
			default:
				return;

			case CVAR_Flag:
			{
				newcvar = new FFlagCVar(cvInfo->name, *cvInfo->defaultval.Pointer->get(), cvInfo->flags, cvInfo->description);
				break;
			}
			case CVAR_Mask:
			{
				newcvar = new FMaskCVar(cvInfo->name, *cvInfo->defaultval.Pointer->get(), cvInfo->flags, cvInfo->description);
				break;
			}

			}
			*(void**)cvInfo->refAddr = newcvar;
		});
}

void C_UninitCVars()
{
	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair* pair;
	while (it.NextPair(pair))
	{
		auto var = pair->Value;
		pair->Value = nullptr;
		delete var;
	}
}

FBaseCVar::FBaseCVar (const char *var_name, uint32_t flags, void (*callback)(FBaseCVar &), const char *descr)
{
	if (var_name != nullptr && (flags & CVAR_SERVERINFO))
	{
		// This limitation is imposed by network protocol which uses only 6 bits 
		// for name's length with terminating null character
		static const size_t NAME_LENGHT_MAX = 63;

		if (strlen(var_name) > NAME_LENGHT_MAX)
		{
			I_FatalError("Name of the server console variable \"%s\" is too long.\n"
				"Its length should not exceed %zu characters.\n", var_name, NAME_LENGHT_MAX);
		}
	}



	m_Callback = callback;
	Flags = 0;
	VarName = "";
	Description = descr;

	FBaseCVar* var = nullptr;
	if (var_name)
	{
		var = FindCVar(var_name, NULL);
		C_AddTabCommand (var_name);
		VarName = var_name;
		cvarMap.Insert(var_name, this);
	}

	if (var)
	{
		ECVarType type;
		UCVarValue value;

		value = var->GetFavoriteRep (&type);
		ForceSet (value, type);
		delete var;

		Flags = flags;
	}
	else
	{
		Flags = flags | CVAR_ISDEFAULT;
	}
}

FBaseCVar::~FBaseCVar ()
{
	if (VarName.IsNotEmpty())
	{
		FBaseCVar *var, *prev;

		var = FindCVar (VarName.GetChars(), &prev);

		if (var == this)
		{
			cvarMap.Remove(var->VarName);
			C_RemoveTabCommand(VarName.GetChars());
		}
	}
}

void FBaseCVar::SetCallback(void (*callback)(FBaseCVar&))
{
	m_Callback = callback;
}

void FBaseCVar::ClearCallback()
{
	m_Callback = nullptr;
}

void FBaseCVar::SetExtraDataPointer(void *pointer)
{
	m_ExtraDataPointer = pointer;
}

void* FBaseCVar::GetExtraDataPointer()
{
	return m_ExtraDataPointer;
}

const char *FBaseCVar::GetHumanString(int precision) const
{
	return GetGenericRep(CVAR_String).String;
}

const char *FBaseCVar::GetHumanStringDefault(int precision) const
{
	return GetGenericRepDefault(CVAR_String).String;
}

void FBaseCVar::ForceSet (UCVarValue value, ECVarType type, bool nouserinfosend)
{
	DoSet (value, type);
	if ((Flags & CVAR_USERINFO) && !nouserinfosend && !(Flags & CVAR_IGNORE))
		if (callbacks && callbacks->UserInfoChanged) callbacks->UserInfoChanged(this);
	if (m_UseCallback)
		Callback ();

	if ((Flags & CVAR_ARCHIVE) && !(Flags & CVAR_UNSAFECONTEXT))
	{
		SafeValue = GetGenericRep(CVAR_String).String;
	}

	Flags &= ~(CVAR_ISDEFAULT | CVAR_UNSAFECONTEXT);
}

void FBaseCVar::SetGenericRep (UCVarValue value, ECVarType type)
{
	if ((Flags & CVAR_NOSET) && m_DoNoSet)
	{
		return;
	}
	if ((Flags & CVAR_LATCH) && callbacks && callbacks->MustLatch())
	{
		FLatchedValue latch;

		latch.Variable = this;
		latch.Type = type;
		if (type != CVAR_String)
			latch.Value = value;
		else
			latch.Value.String = copystring(value.String);
		latch.UnsafeContext = !!(Flags & CVAR_UNSAFECONTEXT);
		LatchedValues.Push (latch);

		Flags &= ~CVAR_UNSAFECONTEXT;
		return;
	}
	if ((Flags & CVAR_SERVERINFO) && callbacks && callbacks->SendServerInfoChange)
	{
		if (callbacks->SendServerInfoChange(this, value, type)) return;
	}
	ForceSet (value, type);
}

bool FBaseCVar::ToBool (UCVarValue value, ECVarType type)
{
	switch (type)
	{
	case CVAR_Bool:
		return value.Bool;

	case CVAR_Color:
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
			return !!strtoll (value.String, NULL, 0);

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
	case CVAR_Color:
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
				res = (int)strtoll (value.String, NULL, 0); 
			break;
		}
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

	case CVAR_Color:
	case CVAR_Int:
		return (float)value.Int;

	case CVAR_Float:
		return value.Float;

	case CVAR_String:
		return (float)strtod (value.String, NULL);

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

	case CVAR_Color:
	case CVAR_Int:
		mysnprintf (cstrbuf, countof(cstrbuf), "%i", value.Int);
		break;

	case CVAR_Float:
		IGNORE_FORMAT_PRE
		mysnprintf (cstrbuf, countof(cstrbuf), "%g", value.Float);
		IGNORE_FORMAT_POST
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
		ret.Int = 0;
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

	default:
		ret.Int = 0;
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
		IGNORE_FORMAT_PRE
		mysnprintf (cstrbuf, countof(cstrbuf), "%g", value);
		IGNORE_FORMAT_POST
		ret.String = cstrbuf;
		break;

	default:
		ret.Int = 0;
		break;
	}

	return ret;
}

static uint8_t HexToByte (const char *hex)
{
	uint8_t v = 0;
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

	switch (type)
	{
	case CVAR_Bool:
		if (stricmp (value, "true") == 0)
			ret.Bool = true;
		else if (stricmp (value, "false") == 0)
			ret.Bool = false;
		else
			ret.Bool = strtoll (value, NULL, 0) != 0;
		break;

	case CVAR_Int:
		if (stricmp (value, "true") == 0)
			ret.Int = 1;
		else if (stricmp (value, "false") == 0)
			ret.Int = 0;
		else
			ret.Int = (int)strtoll (value, NULL, 0);
		break;

	case CVAR_Float:
		ret.Float = (float)strtod (value, NULL);
		break;

	case CVAR_String:
		ret.String = const_cast<char *>(value);
		break;

	default:
		ret.Int = 0;
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
	CVarMap::Iterator it(cvarMap);
	CVarMap::Pair *pair;
	while (it.NextPair(pair))
	{
		auto cvar = pair->Value;
		if (!(cvar->Flags & CVAR_NOINITCALL))
		{
			cvar->Callback ();
		}
	}
}

void FBaseCVar::InitZSCallbacks ()
{
	CVarMap::Iterator it(cvarMap);
	CVarMap::Pair *pair;
	while (it.NextPair(pair))
	{
		auto cvar = pair->Value;
		if (cvar->Flags & CVAR_ZS_CUSTOM)
		{
			cvar->InstantiateZSCVar();
		}
	}
	GC::AddMarkerFunc(FBaseCVar::MarkZSCallbacks);
}

void FBaseCVar::MarkZSCallbacks () {
	CVarMap::Iterator it(cvarMap);
	CVarMap::Pair *pair;
	while (it.NextPair(pair))
	{
		auto cvar = pair->Value;
		if (cvar->Flags & CVAR_ZS_CUSTOM)
		{
			cvar->MarkZSCVar();
		}
	}
}

void FBaseCVar::DisableCallbacks ()
{
	m_UseCallback = false;
}

//
// Boolean cvar implementation
//

FBoolCVar::FBoolCVar (const char *name, bool def, uint32_t flags, void (*callback)(FBoolCVar &), const char* descr)
: FBaseCVar (name, flags, reinterpret_cast<void (*)(FBaseCVar &)>(callback), descr)
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

FIntCVar::FIntCVar (const char *name, int def, uint32_t flags, void (*callback)(FIntCVar &), const char* descr)
: FBaseCVar (name, flags, reinterpret_cast<void (*)(FBaseCVar &)>(callback), descr)
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

FFloatCVar::FFloatCVar (const char *name, float def, uint32_t flags, void (*callback)(FFloatCVar &), const char* descr)
: FBaseCVar (name, flags, reinterpret_cast<void (*)(FBaseCVar &)>(callback), descr)
{
	DefaultValue = def;
	if (Flags & CVAR_ISDEFAULT)
		Value = def;
}

ECVarType FFloatCVar::GetRealType () const
{
	return CVAR_Float;
}

const char *FFloatCVar::GetHumanString(int precision) const
{
	if (precision < 0)
	{
		precision = 6;
	}
	mysnprintf(cstrbuf, countof(cstrbuf), "%.*g", precision, Value);
	return cstrbuf;
}

const char *FFloatCVar::GetHumanStringDefault(int precision) const
{
	if (precision < 0)
	{
		precision = 6;
	}
	mysnprintf(cstrbuf, countof(cstrbuf), "%.*g", precision, DefaultValue);
	return cstrbuf;
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

FStringCVar::FStringCVar (const char *name, const char *def, uint32_t flags, void (*callback)(FStringCVar &), const char* descr)
: FBaseCVar (name, flags, reinterpret_cast<void (*)(FBaseCVar &)>(callback), descr)
{
	mDefaultValue = def;
	if (Flags & CVAR_ISDEFAULT)
		mValue = def;
	else
		mValue = "";
}

FStringCVar::~FStringCVar ()
{
}

ECVarType FStringCVar::GetRealType () const
{
	return CVAR_String;
}

UCVarValue FStringCVar::GetGenericRep (ECVarType type) const
{
	return FromString (mValue.GetChars(), type);
}

UCVarValue FStringCVar::GetFavoriteRep (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_String;
	ret.String = mValue.GetChars();
	return ret;
}

UCVarValue FStringCVar::GetGenericRepDefault (ECVarType type) const
{
	return FromString (mDefaultValue.GetChars(), type);
}

UCVarValue FStringCVar::GetFavoriteRepDefault (ECVarType *type) const
{
	UCVarValue ret;
	*type = CVAR_String;
	ret.String = mDefaultValue.GetChars();
	return ret;
}

void FStringCVar::SetGenericRepDefault (UCVarValue value, ECVarType type)
{
	mDefaultValue = ToString(value, type);
	if (Flags & CVAR_ISDEFAULT)
	{
		SetGenericRep (value, type);
		Flags |= CVAR_ISDEFAULT;
	}
}

void FStringCVar::DoSet (UCVarValue value, ECVarType type)
{
	mValue = ToString (value, type);
}

//
// Color cvar implementation
//

FColorCVar::FColorCVar (const char *name, int def, uint32_t flags, void (*callback)(FColorCVar &), const char* descr)
: FIntCVar (name, def, flags, reinterpret_cast<void (*)(FIntCVar &)>(callback), descr)
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
		FString string = V_GetColorStringByName(value.String);
		// Only allow named colors after the screen exists (i.e. after
		// we've got some lumps loaded, so X11R6RGB can be read). Since
		// the only time this might be called before that is when loading
		// zdoom.ini, this shouldn't be a problem.

		if (string.IsNotEmpty())
		{
			ret = V_GetColorFromString (string.GetChars());
		}
		else
		{
			ret = V_GetColorFromString (value.String);
		}
	}
	else
	{
		ret = ToInt (value, type);
	}
	return ret;
}

//
// More base cvar stuff
//

void FBaseCVar::ResetColors ()
{
	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair *pair;
	while (it.NextPair(pair))
	{
		auto var = pair->Value;

		if (var->GetRealType () == CVAR_Color)
		{
			var->DoSet (var->GetGenericRep (CVAR_Int), CVAR_Int);
		}
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

void FBaseCVar::MarkUnsafe()
{
	if (!(Flags & CVAR_MOD) && UnsafeExecutionContext)
	{
		Flags |= CVAR_UNSAFECONTEXT;
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

FFlagCVar::FFlagCVar (const char *name, FIntCVar &realvar, uint32_t bitval, const char* descr)
: FBaseCVar (name, 0, NULL, descr),
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
	return CVAR_Flag;
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
	if (ValueVar.GetFlags() && callbacks && callbacks->SendServerFlagChange)
	{
		if (callbacks->SendServerFlagChange(&ValueVar, BitNum, newval, false)) return;
	}
	int val = *ValueVar;
	if (newval)
		val |= BitVal;
	else
		val &= ~BitVal;
	ValueVar = val;
}

//
// Mask cvar implementation
//
// Similar to FFlagCVar but can have multiple bits
//

FMaskCVar::FMaskCVar (const char *name, FIntCVar &realvar, uint32_t bitval, const char* descr)
: FBaseCVar (name, 0, NULL, descr),
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
	return CVAR_Mask;
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
	if (ValueVar.GetFlags() && callbacks && callbacks->SendServerFlagChange)
	{
		// The network interface needs to process each bit separately.
		bool silent = false;
		for(int i = 0; i < 32; i++)
		{
			if (BitVal & (1<<i))
			{
				if (!callbacks->SendServerFlagChange(&ValueVar, i, !!(val & (1 << i)), silent)) goto fallback; // the failure case here is either always or never.
				silent = true; // only warn once if SendServerFlagChange needs to.
			}
		}
	}
	else
	{
	fallback:
		int vval = *ValueVar;
		vval &= ~BitVal;
		vval |= val;
		ValueVar = vval;
	}
}


////////////////////////////////////////////////////////////////////////
static int sortcvars (const void *a, const void *b)
{
	return strcmp (((*(FBaseCVar **)a))->GetName(), ((*(FBaseCVar **)b))->GetName());
}

void FilterCompactCVars (TArray<FBaseCVar *> &cvars, uint32_t filter)
{
	// Accumulate all cvars that match the filter flags.
	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair *pair;
	while (it.NextPair(pair))
	{
		auto cvar = pair->Value;
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

void C_WriteCVars (uint8_t **demo_p, uint32_t filter, bool compact)
{
	FString dump = C_GetMassCVarString(filter, compact);
	size_t dumplen = dump.Len() + 1;	// include terminating \0
	memcpy(*demo_p, dump.GetChars(), dumplen);
	*demo_p += dumplen;
}

FString C_GetMassCVarString (uint32_t filter, bool compact)
{
	FBaseCVar* cvar;
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
		decltype(cvarMap)::Iterator it(cvarMap);
		decltype(cvarMap)::Pair *pair;
		while (it.NextPair(pair))
		{
			cvar = pair->Value;
			if ((cvar->Flags & filter) && !(cvar->Flags & (CVAR_NOSAVE|CVAR_IGNORE)))
			{
				UCVarValue val = cvar->GetGenericRep(CVAR_String);
				dump << '\\' << cvar->GetName() << '\\' << val.String;
			}
		}
	}
	return dump;
}

void C_ReadCVars (uint8_t **demo_p)
{
	char *ptr = *((char **)demo_p);
	char *breakpt;

	if (*ptr++ != '\\')
		return;

	if (*ptr == '\\')
	{       // compact mode
		TArray<FBaseCVar *> cvars;
		FBaseCVar *cvar;
		uint32_t filter;

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

	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair *pair;
	while (it.NextPair(pair))
	{
		auto cvar = pair->Value;
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
		cvar_set(CVarBackups[i].Name.GetChars(), CVarBackups[i].String.GetChars());
	}
	C_ForgetCVars();
}

void C_ForgetCVars (void)
{
	CVarBackups.Clear();
}

FBaseCVar *FindCVar (const char *var_name, FBaseCVar **prev)
{
	if (var_name == nullptr)
		return nullptr;

	FName vname(var_name, true);
	if (vname == NAME_None) return nullptr;
	auto find = cvarMap.CheckKey(vname);
	return find? *find : nullptr;
}

FBaseCVar *FindCVarSub (const char *var_name, int namelen)
{
	if (var_name == NULL)
		return NULL;

	FName vname(var_name, namelen, true);
	if (vname == NAME_None) return nullptr;
	auto find = cvarMap.CheckKey(vname);
	return find ? *find : nullptr;
}

FBaseCVar *GetCVar(int playernum, const char *cvarname)
{
	FBaseCVar *cvar = FindCVar(cvarname, nullptr);
	// Either the cvar doesn't exist, or it's for a mod that isn't loaded, so return nullptr.
	if (cvar == nullptr || (cvar->GetFlags() & CVAR_IGNORE))
	{
		return nullptr;
	}
	else
	{
		// For userinfo cvars, redirect to GetUserCVar
		if ((cvar->GetFlags() & CVAR_USERINFO) && callbacks && callbacks->GetUserCVar)
		{
			return callbacks->GetUserCVar(playernum, cvarname);
		}
		return cvar;
	}
}

//===========================================================================
//
// C_CreateCVar
//
// Create a new cvar with the specified name and type. It should not already
// exist.
//
//===========================================================================

FBaseCVar *C_CreateCVar(const char *var_name, ECVarType var_type, uint32_t flags)
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

FBaseCVar * C_CreateZSCustomCVar(const char *var_name, ECVarType var_type, uint32_t flags, FName className)
{
	assert(FindCVar(var_name, NULL) == NULL);
	flags |= CVAR_AUTO | CVAR_ZS_CUSTOM;
	switch (var_type)
	{
	case CVAR_Bool:		return new FZSBoolCVar(var_name, 0, flags, className);
	case CVAR_Int:		return new FZSIntCVar(var_name, 0, flags, className);
	case CVAR_Float:	return new FZSFloatCVar(var_name, 0, flags, className);
	case CVAR_String:	return new FZSStringCVar(var_name, NULL, flags, className);
	case CVAR_Color:	return new FZSColorCVar(var_name, 0, flags, className);
	default:			return NULL;
	}
}

void UnlatchCVars (void)
{
	for (const FLatchedValue& var : LatchedValues)
	{
		uint32_t oldflags = var.Variable->Flags;
		var.Variable->Flags &= ~(CVAR_LATCH | CVAR_SERVERINFO);
		if (var.UnsafeContext)
			var.Variable->Flags |= CVAR_UNSAFECONTEXT;
		var.Variable->SetGenericRep (var.Value, var.Type);
		if (var.Type == CVAR_String)
			delete[] var.Value.String;
		var.Variable->Flags = oldflags;
	}

	LatchedValues.Clear();
}

void DestroyCVarsFlagged (uint32_t flags)
{
	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair *pair;
	while (it.NextPair(pair))
	{
		auto cvar = pair->Value;

		if(cvar->Flags & flags)
			delete cvar;
	}
}

void C_SetCVarsToDefaults (void)
{
	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair *pair;
	while (it.NextPair(pair))
	{
		auto cvar = pair->Value;
		// Only default save-able cvars
		if (cvar->Flags & CVAR_ARCHIVE)
		{
			UCVarValue val;
			ECVarType type;
			val = cvar->GetFavoriteRepDefault (&type);
			cvar->SetGenericRep (val, type);
		}
	}
}

static int cvarcmp(const void* a, const void* b)
{
	FBaseCVar** A = (FBaseCVar**)a;
	FBaseCVar** B = (FBaseCVar**)b;
	return strcmp((*A)->GetName(), (*B)->GetName());
}

void C_ArchiveCVars (FConfigFile *f, uint32_t filter)
{
	TArray<FBaseCVar*> cvarlist;

	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair *pair;
	while (it.NextPair(pair))
	{
		auto cvar = pair->Value;
		if ((cvar->Flags &
			(CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_MOD|CVAR_AUTO|CVAR_USERINFO|CVAR_SERVERINFO|CVAR_NOSAVE|CVAR_CONFIG_ONLY))
			== filter)
		{
			cvarlist.Push(cvar);
		}
	}
	qsort(cvarlist.Data(), cvarlist.Size(), sizeof(FBaseCVar*), cvarcmp);
	for (auto cv : cvarlist)
	{
		const char* const value = (cv->Flags & CVAR_ISDEFAULT)
			? cv->GetGenericRep(CVAR_String).String
			: cv->SafeValue.GetChars();
		f->SetValueForKey(cv->GetName(), value);
	}
}

EXTERN_CVAR(Bool, sv_cheats);

void FBaseCVar::CmdSet (const char *newval)
{
	if ((GetFlags() & CVAR_CHEAT) && sysCallbacks.CheckCheatmode && sysCallbacks.CheckCheatmode(true, false))
		return;

	MarkUnsafe();

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

CCMD (resetcvar)
{
	if (argv.argc() != 2)
	{
		Printf ("usage: resetcvar <variable>\n");
	}
	else
	{
		FBaseCVar *var = FindCVar (argv[1], NULL);
		if (var != NULL)
		{
			var->ResetToDefault();
		}
		else
		{
			Printf ("No such variable: %s\n", argv[1]);
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
			var->MarkUnsafe();

			val = var->GetGenericRep (CVAR_Bool);
			val.Bool = !val.Bool;
			var->SetGenericRep (val, CVAR_Bool);
			auto msg = var->GetToggleMessage(val.Bool);
			if (msg.IsNotEmpty())
			{
				Printf(PRINT_NOTIFY, "%s\n", msg.GetChars());
			}
			else Printf ("\"%s\" = \"%s\"\n", var->GetName(),
				val.Bool ? "true" : "false");
		}
	}
}

void FBaseCVar::ListVars (const char *filter, int listtype)
{
	int count = 0;

	bool plain = listtype == LCT_Plain;
	bool includedesc = listtype == LCT_FullSearch;

	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair *pair;
	while (it.NextPair(pair))
	{
		auto var = pair->Value;

		bool ismatch;

		if (filter && includedesc)
		{
			// search always allow partial matches
			// also allow matching to cvar name, localised description, and description language-id

			FString SearchString = FString("*") + filter + "*";
			ismatch = CheckWildcards (SearchString.GetChars(), var->GetName()) ||
				CheckWildcards (SearchString.GetChars(), var->GetDescription().GetChars()) ||
				CheckWildcards (SearchString.GetChars(), GStrings.localize(var->GetDescription().GetChars()));
		}
		else
		{
			ismatch = CheckWildcards (filter, var->GetName());
		}

		if (ismatch)
		{
			uint32_t flags = var->GetFlags();
			if (plain)
			{ // plain formatting does not include user-defined cvars
				if (!(flags & CVAR_UNSETTABLE))
				{
					++count;
					Printf ("%s : %s\n", var->GetName(), var->GetHumanString());
				}
			}
			else
			{
				++count;

				Printf ("%c%c%c%c%c %s = %s",
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
					var->GetHumanString());

				if (includedesc)
					if (var->GetDescription().Len())
						Printf(" // \"%s\"\n", GStrings.localize(var->GetDescription().GetChars()));
					else
						Printf("\n");
				else
					Printf("\n");
				

			}
		}
	}
	Printf ("%d cvars\n", count);
}

CCMD (cvarlist)
{
	if (argv.argc() == 1)
	{
		FBaseCVar::ListVars (NULL, LCT_Default);
	}
	else
	{
		FBaseCVar::ListVars (argv[1], LCT_Default);
	}
}

CCMD (cvarlistplain)
{
	FBaseCVar::ListVars (NULL, LCT_Plain);
}

CCMD (cvarsearch)
{
	if (argv.argc() == 1)
	{
		FBaseCVar::ListVars (NULL, LCT_FullSearch);
	}
	else
	{
		FBaseCVar::ListVars (argv[1], LCT_FullSearch);
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

void C_ListCVarsWithoutDescription()
{
	decltype(cvarMap)::Iterator it(cvarMap);
	decltype(cvarMap)::Pair *pair;
	while (it.NextPair(pair))
	{
		auto var = pair->Value;
		if (var->GetDescription().IsEmpty())
		{
			Printf("%s\n", var->GetName());
		}
	}
}

CCMD(listcvarswithoutdescription)
{
	C_ListCVarsWithoutDescription();
}


//===========================================================================
//
// FZSIntCVar
//
//===========================================================================


FZSIntCVar::FZSIntCVar(const char *name, int def, uint32_t flags, FName _className, const char* descr)
	: FIntCVar(name,def,flags,nullptr,descr) , cvarName(name) , className(_className)
{ customCVarHandler = nullptr; }

void FZSIntCVar::CallCVarCallback(FZSIntCVar &self)
{
	if (!self.customCVarHandler) {
		I_Error("Handler for CustomIntCVar '%s' of class '%s' was Destroyed", self.cvarName.GetChars(), self.className.GetChars());
	}
	IFVIRTUALPTRNAME(self.customCVarHandler, "CustomIntCVar", ModifyValue)
	{
		VMValue param[] = { self.customCVarHandler.Get() , self.cvarName.GetIndex() , self.Value };
		VMReturn ret(&self.Value);
		VMCall(func, param, 3, &ret, 1);
	}
}

void FZSIntCVar::InstantiateZSCVar()
{
	static PClass * baseClass = PClass::FindClass("CustomIntCVar");
	assert(baseClass);
	PClass * classPtr = PClass::FindClass(className);
	if (!classPtr || !classPtr->IsDescendantOf(baseClass))
	{
		I_Error("Instantiating CVar '%s': Class '%s' %s",cvarName.GetChars(), className.GetChars(), (classPtr ? "is not a descendant of CustomIntCVar" : "does not exist"));
	}
	customCVarHandler = classPtr->CreateNew();
	SetCallback(reinterpret_cast<void (*)(FBaseCVar &)>(CallCVarCallback));
}

void FZSIntCVar::MarkZSCVar()
{
	GC::Mark(customCVarHandler);
}

UCVarValue FZSIntCVar::GenericZSCVarCallback(UCVarValue value, ECVarType type) {
	int val = ToInt(value, type);

	IFVIRTUALPTRNAME(customCVarHandler, "CustomIntCVar", ModifyValue)
	{
		VMValue param[] = { customCVarHandler.Get() , cvarName.GetIndex() , val };
		VMReturn ret(&val);
		VMCall(func, param, 3, &ret, 1);
	}

	UCVarValue v;
	v.Int = val;
	return v;
}


//===========================================================================
//
// FZSFloatCVar
//
//===========================================================================


FZSFloatCVar::FZSFloatCVar(const char *name, float def, uint32_t flags, FName _className, const char* descr)
	: FFloatCVar(name,def,flags,nullptr,descr) , cvarName(name) , className(_className)
{ customCVarHandler = nullptr; }

void FZSFloatCVar::CallCVarCallback(FZSFloatCVar &self)
{
	if (!self.customCVarHandler) {
		I_Error("Handler for CustomFloatCVar '%s' of class '%s' was Destroyed", self.cvarName.GetChars(), self.className.GetChars());
	}
	IFVIRTUALPTRNAME(self.customCVarHandler, "CustomFloatCVar", ModifyValue)
	{
		VMValue param[] = { self.customCVarHandler.Get() , self.cvarName.GetIndex() , (double) self.Value };
		double v;
		VMReturn ret(&v);
		VMCall(func, param, 3, &ret, 1);
		self.Value = (float) v;
	}
}

void FZSFloatCVar::InstantiateZSCVar()
{
	static PClass * baseClass = PClass::FindClass("CustomFloatCVar");
	assert(baseClass);
	PClass * classPtr = PClass::FindClass(className);
	if (!classPtr || !classPtr->IsDescendantOf(baseClass))
	{
		I_Error("Instantiating CVar '%s': Class '%s' %s", cvarName.GetChars(), className.GetChars(), (classPtr ? "is not a descendant of CustomFloatCVar" : "does not exist"));
	}
	customCVarHandler = classPtr->CreateNew();
	SetCallback(reinterpret_cast<void (*)(FBaseCVar &)>(CallCVarCallback));
}

void FZSFloatCVar::MarkZSCVar()
{
	GC::Mark(customCVarHandler);
}

UCVarValue FZSFloatCVar::GenericZSCVarCallback(UCVarValue value, ECVarType type) {
	float val = ToFloat(value, type);

	IFVIRTUALPTRNAME(customCVarHandler, "CustomFloatCVar", ModifyValue)
	{
		VMValue param[] = { customCVarHandler.Get() , cvarName.GetIndex() , (double) val };
		double v;
		VMReturn ret(&v);
		VMCall(func, param, 3, &ret, 1);
		val = (float) v;
	}

	UCVarValue v;
	v.Float = val;
	return v;
}


//===========================================================================
//
// FZSStringCVar
//
//===========================================================================


FZSStringCVar::FZSStringCVar(const char *name, const char * def, uint32_t flags, FName _className, const char* descr)
	: FStringCVar(name,def,flags,nullptr,descr) , cvarName(name) , className(_className)
{ customCVarHandler = nullptr; }

void FZSStringCVar::CallCVarCallback(FZSStringCVar &self)
{
	if (!self.customCVarHandler) {
		I_Error("Handler for CustomStringCVar '%s' of class '%s' was Destroyed", self.cvarName.GetChars(), self.className.GetChars());
	}
	IFVIRTUALPTRNAME(self.customCVarHandler, "CustomStringCVar", ModifyValue)
	{
		VMValue param[] = { self.customCVarHandler.Get() , self.cvarName.GetIndex() , &self.mValue };
		VMReturn ret(&self.mValue);
		VMCall(func, param, 3, &ret, 1);
	}
}

void FZSStringCVar::InstantiateZSCVar()
{
	static PClass * baseClass = PClass::FindClass("CustomStringCVar");
	assert(baseClass);
	PClass * classPtr = PClass::FindClass(className);
	if (!classPtr || !classPtr->IsDescendantOf(baseClass))
	{
		I_Error("Instantiating CVar '%s': Class '%s' %s", cvarName.GetChars(), className.GetChars(), (classPtr ? "is not a descendant of CustomStringCVar" : "does not exist"));
	}
	customCVarHandler = classPtr->CreateNew();
	SetCallback(reinterpret_cast<void (*)(FBaseCVar &)>(CallCVarCallback));
}

void FZSStringCVar::MarkZSCVar()
{
	GC::Mark(customCVarHandler);
}

UCVarValue FZSStringCVar::GenericZSCVarCallback(UCVarValue value, ECVarType type) {
	FString val = ToString(value, type);

	IFVIRTUALPTRNAME(customCVarHandler, "CustomStringCVar", ModifyValue)
	{
		VMValue param[] = { customCVarHandler.Get() , cvarName.GetIndex() , &val };
		VMReturn ret(&val);
		VMCall(func, param, 3, &ret, 1);
	}
	
	char * str = new char[val.Len() + 1];
	memcpy(str, val.GetChars(), val.Len() * sizeof(char));
	str[val.Len()] = '\0';

	UCVarValue v;
	v.String = str;
	return v;
}


//===========================================================================
//
// FZSBoolCVar
//
//===========================================================================


FZSBoolCVar::FZSBoolCVar(const char *name, bool def, uint32_t flags, FName _className, const char* descr)
	: FBoolCVar(name,def,flags,nullptr,descr) , cvarName(name) , className(_className)
{ customCVarHandler = nullptr; }

void FZSBoolCVar::CallCVarCallback(FZSBoolCVar &self)
{
	if (!self.customCVarHandler) {
		I_Error("Handler for CustomBoolCVar '%s' of class '%s' was Destroyed", self.cvarName.GetChars(), self.className.GetChars());
	}
	IFVIRTUALPTRNAME(self.customCVarHandler, "CustomBoolCVar", ModifyValue)
	{
		VMValue param[] = { self.customCVarHandler.Get() , self.cvarName.GetIndex() , self.Value };
		int v;
		VMReturn ret(&v);
		VMCall(func, param, 3, &ret, 1);
		self.Value = v;
	}
}

void FZSBoolCVar::InstantiateZSCVar()
{
	static PClass * baseClass = PClass::FindClass("CustomBoolCVar");
	assert(baseClass);
	PClass * classPtr = PClass::FindClass(className);
	if (!classPtr || !classPtr->IsDescendantOf(baseClass))
	{
		I_Error("Instantiating CVar '%s': Class '%s' %s", cvarName.GetChars(), className.GetChars(), (classPtr ? "is not a descendant of CustomBoolCVar" : "does not exist"));
	}
	customCVarHandler = classPtr->CreateNew();
	SetCallback(reinterpret_cast<void (*)(FBaseCVar &)>(CallCVarCallback));
}

void FZSBoolCVar::MarkZSCVar()
{
	GC::Mark(customCVarHandler);
}

UCVarValue FZSBoolCVar::GenericZSCVarCallback(UCVarValue value, ECVarType type) {
	bool val = ToFloat(value, type);

	IFVIRTUALPTRNAME(customCVarHandler, "CustomBoolCVar", ModifyValue)
	{
		VMValue param[] = { customCVarHandler.Get() , cvarName.GetIndex() , val };
		int v;
		VMReturn ret(&v);
		VMCall(func, param, 3, &ret, 1);
		val = v;
	}

	UCVarValue v;
	v.Bool = val;
	return v;
}


//===========================================================================
//
// FZSColorCVar
//
//===========================================================================


FZSColorCVar::FZSColorCVar(const char *name, int def, uint32_t flags, FName _className, const char* descr)
	: FColorCVar(name,def,flags,nullptr,descr) , cvarName(name) , className(_className)
{ customCVarHandler = nullptr; }

void FZSColorCVar::CallCVarCallback(FZSColorCVar &self)
{
	if (!self.customCVarHandler) {
		I_Error("Handler for CustomColorCVar '%s' of class '%s' was Destroyed", self.cvarName.GetChars(), self.className.GetChars());
	}
	IFVIRTUALPTRNAME(self.customCVarHandler, "CustomColorCVar", ModifyValue)
	{
		VMValue param[] = { self.customCVarHandler.Get() , self.cvarName.GetIndex() , self.Value };
		VMReturn ret(&self.Value);
		VMCall(func, param, 3, &ret, 1);
	}
}

void FZSColorCVar::InstantiateZSCVar()
{
	static PClass * baseClass = PClass::FindClass("CustomColorCVar");
	assert(baseClass);
	PClass * classPtr = PClass::FindClass(className);
	if (!classPtr || !classPtr->IsDescendantOf(baseClass))
	{
		I_Error("Instantiating CVar '%s': Class '%s' %s", cvarName.GetChars(), className.GetChars(), (classPtr ? "is not a descendant of CustomColorCVar" : "does not exist"));
	}
	customCVarHandler = classPtr->CreateNew();
	SetCallback(reinterpret_cast<void (*)(FBaseCVar &)>(CallCVarCallback));
}

void FZSColorCVar::MarkZSCVar()
{
	GC::Mark(customCVarHandler);
}

UCVarValue FZSColorCVar::GenericZSCVarCallback(UCVarValue value, ECVarType type) {
	int val = ToInt(value, type);

	IFVIRTUALPTRNAME(customCVarHandler, "CustomColorCVar", ModifyValue)
	{
		VMValue param[] = { customCVarHandler.Get() , cvarName.GetIndex() , val };
		VMReturn ret(&val);
		VMCall(func, param, 3, &ret, 1);
	}

	UCVarValue v;
	v.Int = val;
	return v;
}