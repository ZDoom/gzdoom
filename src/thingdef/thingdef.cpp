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
#include "p_enemy.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "autosegs.h"
#include "i_system.h"
#include "p_local.h"
#include "p_effect.h"
#include "v_palette.h"
#include "doomerrors.h"
#include "a_doomglobal.h"
#include "a_hexenglobal.h"
#include "a_weaponpiece.h"
#include "p_conversation.h"
#include "v_text.h"
#include "thingdef.h"
#include "a_sharedglobal.h"


const PClass *QuestItemClasses[31];



//==========================================================================
//
// ActorConstDef
//
// Parses a constant definition.
//
//==========================================================================

void ParseConstant (FScanner &sc, PSymbolTable * symt, PClass *cls)
{
	// Read the type and make sure it's int.
	// (Maybe there will be other types later.)
	sc.MustGetToken(TK_Int);
	sc.MustGetToken(TK_Identifier);
	FName symname = sc.String;
	sc.MustGetToken('=');
	int expr = ParseExpression (sc, false, cls);
	sc.MustGetToken(';');

	int val = EvalExpressionI (expr, NULL, cls);
	PSymbolConst *sym = new PSymbolConst;
	sym->SymbolName = symname;
	sym->SymbolType = SYM_Const;
	sym->Value = val;
	if (symt->AddSymbol (sym) == NULL)
	{
		delete sym;
		sc.ScriptError ("'%s' is already defined in class '%s'.",
			symname.GetChars(), cls->TypeName.GetChars());
	}
}

//==========================================================================
//
// ActorEnumDef
//
// Parses an enum definition.
//
//==========================================================================

void ParseEnum (FScanner &sc, PSymbolTable *symt, PClass *cls)
{
	int currvalue = 0;

	sc.MustGetToken('{');
	while (!sc.CheckToken('}'))
	{
		sc.MustGetToken(TK_Identifier);
		FName symname = sc.String;
		if (sc.CheckToken('='))
		{
			int expr = ParseExpression(sc, false, cls);
			currvalue = EvalExpressionI(expr, NULL, cls);
		}
		PSymbolConst *sym = new PSymbolConst;
		sym->SymbolName = symname;
		sym->SymbolType = SYM_Const;
		sym->Value = currvalue;
		if (symt->AddSymbol (sym) == NULL)
		{
			delete sym;
			sc.ScriptError ("'%s' is already defined in class '%s'.",
				symname.GetChars(), cls->TypeName.GetChars());
		}
		// This allows a comma after the last value but doesn't enforce it.
		if (sc.CheckToken('}')) break;
		sc.MustGetToken(',');
		currvalue++;
	}
	sc.MustGetToken(';');
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
#define OPTIONAL		1
#define EVAL			2
#define EVALNOT			4

	AFuncDesc *afd;
	FName funcname;
	FString args;
	
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
				if (sc.CheckToken(TK_Optional))
				{
					flags |= OPTIONAL;
				}
				else if (sc.CheckToken(TK_Eval))
				{
					flags |= EVAL;
				}
				else if (sc.CheckToken(TK_EvalNot))
				{
					flags |= EVALNOT;
				}
				else if (sc.CheckToken(TK_Coerce) || sc.CheckToken(TK_Native))
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
			case TK_Bool:		type = 'i';		break;
			case TK_Int:		type = 'i';		break;
			case TK_Float:		type = 'f';		break;
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
			// If eval or evalnot were a flag, hey the decorate parser doesn't actually care about the type.
			if (flags & EVALNOT)
			{
				type = 'y';
			}
			else if (flags & EVAL)
			{
				type = 'x';
			}
			if (!(flags & OPTIONAL) && type != '+')
			{
				type -= 'a' - 'A';
			}
	#undef OPTIONAL
	#undef EVAL
	#undef EVALNOT
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
static FActorInfo *CreateNewActor(FScanner &sc, FActorInfo **parentc, Baggage *bag)
{
	FName typeName;
	const PClass *replacee = NULL;
	int DoomEdNum = -1;
	PClass *ti = NULL;
	FActorInfo *info = NULL;

	// Get actor name
	sc.MustGetString();
	
	char *colon = strchr(sc.String, ':');
	if (colon != NULL)
	{
		*colon++ = 0;
	}

	/*
	if (PClass::FindClass (sc.String) != NULL)
	{
		sc.ScriptError ("Actor %s is already defined.", sc.String);
	}
	*/

	typeName = sc.String;

	PClass *parent = RUNTIME_CLASS(AActor);
	if (parentc)
	{
		*parentc = NULL;
		
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
			
		if (colon != NULL)
		{
			parent = const_cast<PClass *> (PClass::FindClass (colon));

			if (parent == NULL)
			{
				sc.ScriptError ("Parent type '%s' not found", colon);
			}
			else if (!parent->IsDescendantOf(RUNTIME_CLASS(AActor)))
			{
				sc.ScriptError ("Parent type '%s' is not an actor", colon);
			}
			else if (parent->ActorInfo == NULL)
			{
				sc.ScriptError ("uninitialized parent type '%s'", colon);
			}
			else
			{
				*parentc = parent->ActorInfo;
			}
		}
		else sc.UnGet();
	}

	// Check for "replaces"
	if (sc.CheckString ("replaces"))
	{

		// Get actor name
		sc.MustGetString ();
		replacee = PClass::FindClass (sc.String);

		if (replacee == NULL)
		{
			sc.ScriptError ("Replaced type '%s' not found", sc.String);
		}
		else if (replacee->ActorInfo == NULL)
		{
			sc.ScriptError ("Replaced type '%s' is not an actor", sc.String);
		}
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
		ti = (PClass*)PClass::FindClass(typeName);
		if (ti == NULL)
		{
			sc.ScriptError("Unknown native class '%s'", typeName.GetChars());
		}
		else if (ti != RUNTIME_CLASS(AActor) && ti->ParentClass->NativeClass() != parent->NativeClass())
		{
			sc.ScriptError("Native class '%s' does not inherit from '%s'", 
				typeName.GetChars(),parent->TypeName.GetChars());
		}
		else if (ti->ActorInfo != NULL)
		{
			sc.ScriptMessage("Redefinition of internal class '%s'", typeName.GetChars());
		}
		ti->InitializeActorInfo();
		info = ti->ActorInfo;
	}
	else
	{
		ti = parent->CreateDerivedClass (typeName, parent->Size);
		info = ti->ActorInfo;
	}

	MakeStateDefines(parent->ActorInfo->StateList);

	ResetBaggage (bag);
	bag->Info = info;

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

	info->DoomEdNum = DoomEdNum;
	return info;
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

	try
	{
		FActorInfo * parent;

		info = CreateNewActor(sc, &parent, &bag);
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
	}

	catch(CRecoverableError & e)
	{
		throw e;
	}
	// I think this is better than a crash log.
#ifndef _DEBUG
	catch (...)
	{
		if (info)
			sc.ScriptError("Unexpected error during parsing of actor %s", info->Class->TypeName.GetChars());
		else
			sc.ScriptError("Unexpected error during parsing of actor definitions");
	}
#endif

	sc.SetCMode (false);
}

//==========================================================================
//
// Do some postprocessing after everything has been defined
// This also processes all the internal actors to adjust the type
// fields in the weapons
//
//==========================================================================

void FinishThingdef()
{
	unsigned int i;
	bool isRuntimeActor=false;
	int errorcount=0;

	for (i = 0;i < PClass::m_Types.Size(); i++)
	{
		PClass * ti = PClass::m_Types[i];

		// Skip non-actors
		if (!ti->IsDescendantOf(RUNTIME_CLASS(AActor))) continue;

		// Everything coming after the first runtime actor is also a runtime actor
		// so this check is sufficient
		if (ti == PClass::m_RuntimeActors[0])
		{
			isRuntimeActor=true;
		}

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
			fuglyname v;

			v = defaults->PickupFlash;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->PickupFlash = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->PickupFlash)
					{
						Printf("Unknown pickup flash '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
						errorcount++;
					}
				}
			}
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
			fuglyname v;

			v = defaults->AmmoType1;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->AmmoType1 = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->AmmoType1)
					{
						Printf("Unknown ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
						errorcount++;
					}
					else if (defaults->AmmoType1->ParentClass != RUNTIME_CLASS(AAmmo))
					{
						Printf("Invalid ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
						errorcount++;
					}
				}
			}

			v = defaults->AmmoType2;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->AmmoType2 = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->AmmoType2)
					{
						Printf("Unknown ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
						errorcount++;
					}
					else if (defaults->AmmoType2->ParentClass != RUNTIME_CLASS(AAmmo))
					{
						Printf("Invalid ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
						errorcount++;
					}
				}
			}

			v = defaults->SisterWeaponType;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->SisterWeaponType = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->SisterWeaponType)
					{
						Printf("Unknown sister weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
						errorcount++;
					}
					else if (!defaults->SisterWeaponType->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
					{
						Printf("Invalid sister weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
						errorcount++;
					}
				}
			}

			if (isRuntimeActor)
			{
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

		}
		// same for the weapon type of weapon pieces.
		else if (ti->IsDescendantOf(RUNTIME_CLASS(AWeaponPiece)))
		{
			AWeaponPiece * defaults=(AWeaponPiece *)def;
			fuglyname v;

			v = defaults->WeaponClass;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->WeaponClass = PClass::FindClass(v);
				if (!defaults->WeaponClass)
				{
					Printf("Unknown weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					errorcount++;
				}
				else if (!defaults->WeaponClass->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
				{
					Printf("Invalid weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					errorcount++;
				}
			}
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

