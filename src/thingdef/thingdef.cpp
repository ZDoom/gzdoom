/*
** thingdef.cpp
**
** Actor definitions
**
**---------------------------------------------------------------------------
** Copyright 2002-2007 Christoph Oelckers
** Copyright 2004-2007 Randy Heit
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
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
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

#include "gi.h"
#include "actor.h"
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "w_wad.h"
#include "templates.h"
#include "r_defs.h"
#include "r_draw.h"
#include "a_pickups.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "i_system.h"
#include "p_local.h"
#include "v_palette.h"
#include "doomerrors.h"
#include "a_hexenglobal.h"
#include "a_weaponpiece.h"
#include "p_conversation.h"
#include "v_text.h"
#include "thingdef.h"
#include "a_sharedglobal.h"


const PClass *QuestItemClasses[31];

//==========================================================================
//
// ParseParameter
//
// Parses aparameter - either a default in a function declaration 
// or an argument in a function call.
//
//==========================================================================

int ParseParameter(FScanner &sc, PClass *cls, char type, bool constant)
{
	int v;

	switch(type)
	{
	case 'S':
	case 's':		// Sound name
		sc.MustGetString();
		return S_FindSound(sc.String);

	case 'M':
	case 'm':		// Actor name
	case 'T':
	case 't':		// String
		sc.SetEscape(true);
		sc.MustGetString();
		sc.SetEscape(false);
		return (int)(sc.String[0] ? FName(sc.String) : NAME_None);

	case 'C':
	case 'c':		// Color
		sc.MustGetString ();
		if (sc.Compare("none"))
		{
			return -1;
		}
		else if (sc.Compare(""))
		{
			return 0;
		}
		else
		{
			int c = V_GetColor (NULL, sc.String);
			// 0 needs to be the default so we have to mark the color.
			return MAKEARGB(1, RPART(c), GPART(c), BPART(c));
		}

	case 'L':
	case 'l':
	{
		if (JumpParameters.Size()==0) JumpParameters.Push(NAME_None);

		v = -(int)JumpParameters.Size();
		// This forces quotation marks around the state name.
		sc.MustGetToken(TK_StringConst);
		if (sc.String[0] == 0 || sc.Compare("None"))
		{
			return 0;
		}
		if (sc.Compare("*"))
		{
			if (constant) return INT_MIN;
			else sc.ScriptError("Invalid state name '*'");
		}
		FString statestring = sc.String; // ParseStateString(sc);
		const PClass *stype=NULL;
		int scope = statestring.IndexOf("::");
		if (scope >= 0)
		{
			FName scopename = FName(statestring, scope, false);
			if (scopename == NAME_Super)
			{
				// Super refers to the direct superclass
				scopename = cls->ParentClass->TypeName;
			}
			JumpParameters.Push(scopename);
			statestring = statestring.Right(statestring.Len()-scope-2);

			stype = PClass::FindClass (scopename);
			if (stype == NULL)
			{
				sc.ScriptError ("%s is an unknown class.", scopename.GetChars());
			}
			if (!stype->IsDescendantOf (RUNTIME_CLASS(AActor)))
			{
				sc.ScriptError ("%s is not an actor class, so it has no states.", stype->TypeName.GetChars());
			}
			if (!stype->IsAncestorOf (cls))
			{
				sc.ScriptError ("%s is not derived from %s so cannot access its states.",
					cls->TypeName.GetChars(), stype->TypeName.GetChars());
			}
		}
		else
		{
			// No class name is stored. This allows 'virtual' jumps to
			// labels in subclasses.
			// It also means that the validity of the given state cannot
			// be checked here.
			JumpParameters.Push(NAME_None);
		}

		TArray<FName> &names = MakeStateNameList(statestring);

		if (stype != NULL)
		{
			if (!stype->ActorInfo->FindState(names.Size(), &names[0]))
			{
				sc.ScriptError("Jump to unknown state '%s' in class '%s'",
					statestring.GetChars(), stype->TypeName.GetChars());
			}
		}
		JumpParameters.Push((ENamedName)names.Size());
		for(unsigned i=0;i<names.Size();i++)
		{
			JumpParameters.Push(names[i]);
		}
		// No offsets here. The point of jumping to labels is to avoid such things!
		return v;
	}

	case 'X':
	case 'x':
		v = ParseExpression (sc, false, cls);
		if (constant && !IsExpressionConst(v))
		{
			sc.ScriptError("Default parameter must be constant.");
		}
		return v;

	default:
		assert(false);
		return -1;
	}
}

//==========================================================================
//
// ActorActionDef
//
// Parses an action function definition. A lot of this is essentially
// documentation in the declaration for when I have a proper language
// ready.
//
//==========================================================================

static void ParseActionDef (FScanner &sc, PClass *cls)
{
	enum
	{
		OPTIONAL = 1
	};

	AFuncDesc *afd;
	FName funcname;
	FString args;
	TArray<int> DefaultParams;
	bool hasdefaults = false;
	
	if (sc.LumpNum == -1 || Wads.GetLumpFile(sc.LumpNum) > 0)
	{
		sc.ScriptError ("action functions can only be imported by internal class and actor definitions!");
	}

	sc.MustGetToken(TK_Native);
	sc.MustGetToken(TK_Identifier);
	funcname = sc.String;
	afd = FindFunction(sc.String);
	if (afd == NULL)
	{
		sc.ScriptError ("The function '%s' has not been exported from the executable.", sc.String);
	}
	sc.MustGetToken('(');
	if (!sc.CheckToken(')'))
	{
		while (sc.TokenType != ')')
		{
			int flags = 0;
			char type = '@';

			// Retrieve flags before type name
			for (;;)
			{
				if (sc.CheckToken(TK_Coerce) || sc.CheckToken(TK_Native))
				{
				}
				else
				{
					break;
				}
			}
			// Read the variable type
			sc.MustGetAnyToken();
			switch (sc.TokenType)
			{
			case TK_Bool:
			case TK_Int:
			case TK_Float:
				type = 'x';
				break;

			case TK_Sound:		type = 's';		break;
			case TK_String:		type = 't';		break;
			case TK_Name:		type = 't';		break;
			case TK_State:		type = 'l';		break;
			case TK_Color:		type = 'c';		break;
			case TK_Class:
				sc.MustGetToken('<');
				sc.MustGetToken(TK_Identifier);	// Skip class name, since the parser doesn't care
				sc.MustGetToken('>');
				type = 'm';
				break;
			case TK_Ellipsis:
				type = '+';
				sc.MustGetToken(')');
				sc.UnGet();
				break;
			default:
				sc.ScriptError ("Unknown variable type %s", sc.TokenName(sc.TokenType, sc.String).GetChars());
				break;
			}
			// Read the optional variable name
			if (!sc.CheckToken(',') && !sc.CheckToken(')'))
			{
				sc.MustGetToken(TK_Identifier);
			}
			else
			{
				sc.UnGet();
			}

			int def;
			if (sc.CheckToken('='))
			{
				hasdefaults = true;
				flags|=OPTIONAL;
				def = ParseParameter(sc, cls, type, true);
			}
			else
			{
				def = 0;
			}
			DefaultParams.Push(def);

			if (!(flags & OPTIONAL) && type != '+')
			{
				type -= 'a' - 'A';
			}
			args += type;
			sc.MustGetAnyToken();
			if (sc.TokenType != ',' && sc.TokenType != ')')
			{
				sc.ScriptError ("Expected ',' or ')' but got %s instead", sc.TokenName(sc.TokenType, sc.String).GetChars());
			}
		}
	}
	sc.MustGetToken(';');
	PSymbolActionFunction *sym = new PSymbolActionFunction;
	sym->SymbolName = funcname;
	sym->SymbolType = SYM_ActionFunction;
	sym->Arguments = args;
	sym->Function = afd->Function;
	if (hasdefaults)
	{
		sym->defaultparameterindex = StateParameters.Size();
		for(unsigned int i = 0; i < DefaultParams.Size(); i++)
			StateParameters.Push(DefaultParams[i]);
	}
	else
	{
		sym->defaultparameterindex = -1;
	}
	if (cls->Symbols.AddSymbol (sym) == NULL)
	{
		delete sym;
		sc.ScriptError ("'%s' is already defined in class '%s'.",
			funcname.GetChars(), cls->TypeName.GetChars());
	}
}

//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
static FActorInfo *CreateNewActor(FName typeName, FName parentName, FName replaceName, 
								  int DoomEdNum, bool native)
{
	const PClass *replacee = NULL;
	PClass *ti = NULL;
	FActorInfo *info = NULL;

	PClass *parent = RUNTIME_CLASS(AActor);

	if (parentName != NAME_None)
	{
		parent = const_cast<PClass *> (PClass::FindClass (parentName));

		if (parent == NULL)
		{
			I_Error ("Parent type '%s' not found in %s", parentName.GetChars(), typeName.GetChars());
		}
		else if (!parent->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			I_Error ("Parent type '%s' is not an actor in %s", parentName.GetChars(), typeName.GetChars());
		}
		else if (parent->ActorInfo == NULL)
		{
			I_Error ("uninitialized parent type '%s' in %s", parentName.GetChars(), typeName.GetChars());
		}
	}

	// Check for "replaces"
	if (replaceName != NAME_None)
	{
		// Get actor name
		replacee = PClass::FindClass (replaceName);

		if (replacee == NULL)
		{
			I_Error ("Replaced type '%s' not found in %s", replaceName.GetChars(), typeName.GetChars());
		}
		else if (replacee->ActorInfo == NULL)
		{
			I_Error ("Replaced type '%s' is not an actor in %s", replaceName.GetChars(), typeName.GetChars());
		}
	}

	if (native)
	{
		ti = (PClass*)PClass::FindClass(typeName);
		if (ti == NULL)
		{
			I_Error("Unknown native class '%s'", typeName.GetChars());
		}
		else if (ti != RUNTIME_CLASS(AActor) && ti->ParentClass->NativeClass() != parent->NativeClass())
		{
			I_Error("Native class '%s' does not inherit from '%s'", typeName.GetChars(), parentName.GetChars());
		}
		else if (ti->ActorInfo != NULL)
		{
			I_Error("Redefinition of internal class '%s'", typeName.GetChars());
		}
		ti->InitializeActorInfo();
		info = ti->ActorInfo;
	}
	else
	{
		ti = parent->CreateDerivedClass (typeName, parent->Size);
		info = ti->ActorInfo;
	}

	info->DoomEdNum = -1;
	if (parent->ActorInfo->DamageFactors != NULL)
	{
		// copy damage factors from parent
		info->DamageFactors = new DmgFactors;
		*info->DamageFactors = *parent->ActorInfo->DamageFactors;
	}
	if (parent->ActorInfo->PainChances != NULL)
	{
		// copy pain chances from parent
		info->PainChances = new PainChanceList;
		*info->PainChances = *parent->ActorInfo->PainChances;
	}

	if (replacee != NULL)
	{
		replacee->ActorInfo->Replacement = ti->ActorInfo;
		ti->ActorInfo->Replacee = replacee->ActorInfo;
	}

	if (DoomEdNum > 0) info->DoomEdNum = DoomEdNum;
	return info;
}

//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
static FActorInfo *ParseActorHeader(FScanner &sc, Baggage *bag)
{
	FName typeName;
	FName parentName;
	FName replaceName;
	bool native = false;
	int DoomEdNum = -1;

	// Get actor name
	sc.MustGetString();
	
	char *colon = strchr(sc.String, ':');
	if (colon != NULL)
	{
		*colon++ = 0;
	}

	typeName = sc.String;

	// Do some tweaking so that a definition like 'Actor:Parent' which is read as a single token is recognized as well
	// without having resort to C-mode (which disallows periods in actor names.)
	if (colon == NULL)
	{
		sc.MustGetString ();
		if (sc.String[0]==':')
		{
			colon = sc.String + 1;
		}
	}
		
	if (colon != NULL)
	{
		if (colon[0] == 0)
		{
			sc.MustGetString ();
			colon = sc.String;
		}
	}

	if (colon == NULL)
	{
		sc.UnGet();
	}

	parentName = colon;

	// Check for "replaces"
	if (sc.CheckString ("replaces"))
	{
		// Get actor name
		sc.MustGetString ();
		replaceName = sc.String;
	}

	// Now, after the actor names have been parsed, it is time to switch to C-mode 
	// for the rest of the actor definition.
	sc.SetCMode (true);
	if (sc.CheckNumber()) 
	{
		if (sc.Number>=-1 && sc.Number<32768) DoomEdNum = sc.Number;
		else sc.ScriptError ("DoomEdNum must be in the range [-1,32767]");
	}

	if (sc.CheckString("native"))
	{
		native = true;
	}

	try
	{
		FActorInfo *info =  CreateNewActor(typeName, parentName, replaceName, DoomEdNum, native);
		ResetBaggage (bag, info->Class->ParentClass);
		bag->Info = info;
		bag->Lumpnum = sc.LumpNum;
		return info;
	}
	catch (CRecoverableError &err)
	{
		sc.ScriptError("%s", err.GetMessage());
		return NULL;
	}
}

//==========================================================================
//
// Reads an actor definition
//
//==========================================================================
void ParseActor(FScanner &sc)
{
	FActorInfo * info=NULL;
	Baggage bag;

	info = ParseActorHeader(sc, &bag);
	sc.MustGetToken('{');
	while (sc.MustGetAnyToken(), sc.TokenType != '}')
	{
		switch (sc.TokenType)
		{
		case TK_Action:
			ParseActionDef (sc, info->Class);
			break;

		case TK_Const:
			ParseConstant (sc, &info->Class->Symbols, info->Class);
			break;

		case TK_Enum:
			ParseEnum (sc, &info->Class->Symbols, info->Class);
			break;

		case TK_Identifier:
			// other identifier related checks here
		case TK_Projectile:	// special case: both keyword and property name
			ParseActorProperty(sc, bag);
			break;

		case '+':
		case '-':
			ParseActorFlag(sc, bag, sc.TokenType);
			break;

		default:
			sc.ScriptError("Unexpected '%s' in definition of '%s'", sc.String, bag.Info->Class->TypeName.GetChars());
			break;
		}
	}
	FinishActor(sc, info, bag);
	sc.SetCMode (false);
}

//==========================================================================
//
// Do some postprocessing after everything has been defined
// This also processes all the internal actors to adjust the type
// fields in the weapons
//
//==========================================================================

static int ResolvePointer(const PClass **pPtr, const PClass *owner, const PClass *destclass, const char *description)
{
	fuglyname v;

	v = *pPtr;
	if (v != NAME_None && v.IsValidName())
	{
		*pPtr = PClass::FindClass(v);
		if (!*pPtr)
		{
			Printf("Unknown %s '%s' in '%s'\n", description, v.GetChars(), owner->TypeName.GetChars());
			return 1;
		}
		else if (!(*pPtr)->IsDescendantOf(destclass))
		{
			*pPtr = NULL;
			Printf("Invalid %s '%s' in '%s'\n", description, v.GetChars(), owner->TypeName.GetChars());
			return 1;
		}
	}
	return 0;
}


void FinishThingdef()
{
	unsigned int i;
	int errorcount=0;

	for (i = 0;i < PClass::m_Types.Size(); i++)
	{
		PClass * ti = PClass::m_Types[i];

		// Skip non-actors
		if (!ti->IsDescendantOf(RUNTIME_CLASS(AActor))) continue;

		AActor *def = GetDefaultByType(ti);

		if (!def)
		{
			Printf("No ActorInfo defined for class '%s'\n", ti->TypeName.GetChars());
			errorcount++;
			continue;
		}

		// Friendlies never count as kills!
		if (def->flags & MF_FRIENDLY)
		{
			def->flags &=~MF_COUNTKILL;
		}

		if (ti->IsDescendantOf(RUNTIME_CLASS(AInventory)))
		{
			AInventory * defaults=(AInventory *)def;
			errorcount += ResolvePointer(&defaults->PickupFlash, ti, RUNTIME_CLASS(AActor), "pickup flash");
		}

		if (ti->IsDescendantOf(RUNTIME_CLASS(APowerupGiver)) && ti != RUNTIME_CLASS(APowerupGiver))
		{
			FString typestr;
			APowerupGiver * defaults=(APowerupGiver *)def;
			fuglyname v;

			v = defaults->PowerupType;
			if (v != NAME_None && v.IsValidName())
			{
				typestr.Format ("Power%s", v.GetChars());
				const PClass * powertype=PClass::FindClass(typestr);
				if (!powertype) powertype=PClass::FindClass(v.GetChars());

				if (!powertype)
				{
					Printf("Unknown powerup type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					errorcount++;
				}
				else if (!powertype->IsDescendantOf(RUNTIME_CLASS(APowerup)))
				{
					Printf("Invalid powerup type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					errorcount++;
				}
				else
				{
					defaults->PowerupType=powertype;
				}
			}
			else if (v == NAME_None)
			{
				Printf("No powerup type specified in '%s'\n", ti->TypeName.GetChars());
				errorcount++;
			}
		}

		// the typeinfo properties of weapons have to be fixed here after all actors have been declared
		if (ti->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			AWeapon * defaults=(AWeapon *)def;
			errorcount += ResolvePointer(&defaults->AmmoType1, ti, RUNTIME_CLASS(AAmmo), "ammo type");
			errorcount += ResolvePointer(&defaults->AmmoType2, ti, RUNTIME_CLASS(AAmmo), "ammo type");
			errorcount += ResolvePointer(&defaults->SisterWeaponType, ti, RUNTIME_CLASS(AWeapon), "sister weapon type");

			FState * ready = ti->ActorInfo->FindState(NAME_Ready);
			FState * select = ti->ActorInfo->FindState(NAME_Select);
			FState * deselect = ti->ActorInfo->FindState(NAME_Deselect);
			FState * fire = ti->ActorInfo->FindState(NAME_Fire);

			// Consider any weapon without any valid state abstract and don't output a warning
			// This is for creating base classes for weapon groups that only set up some properties.
			if (ready || select || deselect || fire)
			{
				// Do some consistency checks. If these states are undefined the weapon cannot work!
				if (!ready)
				{
					Printf("Weapon %s doesn't define a ready state.\n", ti->TypeName.GetChars());
					errorcount++;
				}
				if (!select) 
				{
					Printf("Weapon %s doesn't define a select state.\n", ti->TypeName.GetChars());
					errorcount++;
				}
				if (!deselect) 
				{
					Printf("Weapon %s doesn't define a deselect state.\n", ti->TypeName.GetChars());
					errorcount++;
				}
				if (!fire) 
				{
					Printf("Weapon %s doesn't define a fire state.\n", ti->TypeName.GetChars());
					errorcount++;
				}
			}
		}
		// same for the weapon type of weapon pieces.
		else if (ti->IsDescendantOf(RUNTIME_CLASS(AWeaponPiece)))
		{
			AWeaponPiece * defaults=(AWeaponPiece *)def;
			errorcount += ResolvePointer(&defaults->WeaponClass, ti, RUNTIME_CLASS(AWeapon), "weapon type");
		}
	}
	if (errorcount > 0)
	{
		I_Error("%d errors during actor postprocessing", errorcount);
	}

	// Since these are defined in DECORATE now the table has to be initialized here.
	for(int i=0;i<31;i++)
	{
		char fmt[20];
		mysnprintf(fmt, countof(fmt), "QuestItem%d", i+1);
		QuestItemClasses[i] = PClass::FindClass(fmt);
	}

}

