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

void ParseConstant (PSymbolTable * symt, PClass *cls)
{
	// Read the type and make sure it's int.
	// (Maybe there will be other types later.)
	SC_MustGetToken(TK_Int);
	SC_MustGetToken(TK_Identifier);
	FName symname = sc_String;
	SC_MustGetToken('=');
	int expr = ParseExpression (false, cls);
	SC_MustGetToken(';');

	int val = EvalExpressionI (expr, NULL, cls);
	PSymbolConst *sym = new PSymbolConst;
	sym->SymbolName = symname;
	sym->SymbolType = SYM_Const;
	sym->Value = val;
	if (symt->AddSymbol (sym) == NULL)
	{
		delete sym;
		SC_ScriptError ("'%s' is already defined in class '%s'.",
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

void ParseEnum (PSymbolTable * symt, PClass *cls)
{
	int currvalue = 0;

	SC_MustGetToken('{');
	while (!SC_CheckToken('}'))
	{
		SC_MustGetToken(TK_Identifier);
		FName symname = sc_String;
		if (SC_CheckToken('='))
		{
			int expr = ParseExpression(false, cls);
			currvalue = EvalExpressionI (expr, NULL, cls);
		}
		PSymbolConst *sym = new PSymbolConst;
		sym->SymbolName = symname;
		sym->SymbolType = SYM_Const;
		sym->Value = currvalue;
		if (symt->AddSymbol (sym) == NULL)
		{
			delete sym;
			SC_ScriptError ("'%s' is already defined in class '%s'.",
				symname.GetChars(), cls->TypeName.GetChars());
		}
		// This allows a comma after the last value but doesn't enforce it.
		if (SC_CheckToken('}')) break;
		SC_MustGetToken(',');
		currvalue++;
	}
	SC_MustGetToken(';');
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

static void ParseActionDef (PClass *cls)
{
#define OPTIONAL		1
#define EVAL			2
#define EVALNOT			4

	AFuncDesc *afd;
	FName funcname;
	FString args;

	SC_MustGetToken(TK_Native);
	SC_MustGetToken(TK_Identifier);
	funcname = sc_String;
	afd = FindFunction(sc_String);
	if (afd == NULL)
	{
		SC_ScriptError ("The function '%s' has not been exported from the executable.", sc_String);
	}
	SC_MustGetToken('(');
	if (!SC_CheckToken(')'))
	{
		while (sc_TokenType != ')')
		{
			int flags = 0;
			char type = '@';

			// Retrieve flags before type name
			for (;;)
			{
				if (SC_CheckToken(TK_Optional))
				{
					flags |= OPTIONAL;
				}
				else if (SC_CheckToken(TK_Eval))
				{
					flags |= EVAL;
				}
				else if (SC_CheckToken(TK_EvalNot))
				{
					flags |= EVALNOT;
				}
				else if (SC_CheckToken(TK_Coerce) || SC_CheckToken(TK_Native))
				{
				}
				else
				{
					break;
				}
			}
			// Read the variable type
			SC_MustGetAnyToken();
			switch (sc_TokenType)
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
				SC_MustGetToken('<');
				SC_MustGetToken(TK_Identifier);	// Skip class name, since the parser doesn't care
				SC_MustGetToken('>');
				type = 'm';
				break;
			case TK_Ellipsis:
				type = '+';
				SC_MustGetToken(')');
				SC_UnGet();
				break;
			default:
				SC_ScriptError ("Unknown variable type %s", SC_TokenName(sc_TokenType, sc_String).GetChars());
				break;
			}
			// Read the optional variable name
			if (!SC_CheckToken(',') && !SC_CheckToken(')'))
			{
				SC_MustGetToken(TK_Identifier);
			}
			else
			{
				SC_UnGet();
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
			SC_MustGetAnyToken();
			if (sc_TokenType != ',' && sc_TokenType != ')')
			{
				SC_ScriptError ("Expected ',' or ')' but got %s instead", SC_TokenName(sc_TokenType, sc_String).GetChars());
			}
		}
	}
	SC_MustGetToken(';');
	PSymbolActionFunction *sym = new PSymbolActionFunction;
	sym->SymbolName = funcname;
	sym->SymbolType = SYM_ActionFunction;
	sym->Arguments = args;
	sym->Function = afd->Function;
	if (cls->Symbols.AddSymbol (sym) == NULL)
	{
		delete sym;
		SC_ScriptError ("'%s' is already defined in class '%s'.",
			funcname.GetChars(), cls->TypeName.GetChars());
	}
}

//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
static FActorInfo * CreateNewActor(FActorInfo ** parentc, Baggage *bag)
{
	FName typeName;

	// Get actor name
	SC_MustGetString();
	
	char * colon = strchr(sc_String, ':');
	if (colon != NULL)
	{
		*colon++ = 0;
	}

	if (PClass::FindClass (sc_String) != NULL)
	{
		SC_ScriptError ("Actor %s is already defined.", sc_String);
	}

	typeName = sc_String;

	PClass * parent = RUNTIME_CLASS(AActor);
	if (parentc)
	{
		*parentc = NULL;
		
		// Do some tweaking so that a definition like 'Actor:Parent' which is read as a single token is recognized as well
		// without having resort to C-mode (which disallows periods in actor names.)
		if (colon == NULL)
		{
			SC_MustGetString ();
			if (sc_String[0]==':')
			{
				colon = sc_String + 1;
			}
		}
		
		if (colon != NULL)
		{
			if (colon[0] == 0)
			{
				SC_MustGetString ();
				colon = sc_String;
			}
		}
			
		if (colon != NULL)
		{
			parent = const_cast<PClass *> (PClass::FindClass (colon));

			if (parent == NULL)
			{
				SC_ScriptError ("Parent type '%s' not found", colon);
			}
			else if (parent->ActorInfo == NULL)
			{
				SC_ScriptError ("Parent type '%s' is not an actor", colon);
			}
			else
			{
				*parentc = parent->ActorInfo;
			}
		}
		else SC_UnGet();
	}

	PClass * ti = parent->CreateDerivedClass (typeName, parent->Size);
	FActorInfo * info = ti->ActorInfo;

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

	// Check for "replaces"
	SC_MustGetString ();
	if (SC_Compare ("replaces"))
	{
		const PClass *replacee;

		// Get actor name
		SC_MustGetString ();
		replacee = PClass::FindClass (sc_String);

		if (replacee == NULL)
		{
			SC_ScriptError ("Replaced type '%s' not found", sc_String);
		}
		else if (replacee->ActorInfo == NULL)
		{
			SC_ScriptError ("Replaced type '%s' is not an actor", sc_String);
		}
		replacee->ActorInfo->Replacement = ti->ActorInfo;
		ti->ActorInfo->Replacee = replacee->ActorInfo;
	}
	else
	{
		SC_UnGet();
	}

	// Now, after the actor names have been parsed, it is time to switch to C-mode 
	// for the rest of the actor definition.
	SC_SetCMode (true);
	if (SC_CheckNumber()) 
	{
		if (sc_Number>=-1 && sc_Number<32768) info->DoomEdNum = sc_Number;
		else SC_ScriptError ("DoomEdNum must be in the range [-1,32767]");
	}
	if (parent == RUNTIME_CLASS(AWeapon))
	{
		// preinitialize kickback to the default for the game
		((AWeapon*)(info->Class->Defaults))->Kickback=gameinfo.defKickback;
	}

	return info;
}

//==========================================================================
//
// Reads an actor definition
//
//==========================================================================
void ParseActor()
{
	FActorInfo * info=NULL;
	Baggage bag;

	try
	{
		FActorInfo * parent;

		info=CreateNewActor(&parent, &bag);
		SC_MustGetToken('{');
		while (SC_MustGetAnyToken(), sc_TokenType != '}')
		{
			switch (sc_TokenType)
			{
			case TK_Action:
				ParseActionDef (info->Class);
				break;

			case TK_Const:
				ParseConstant (&info->Class->Symbols, info->Class);
				break;

			case TK_Enum:
				ParseEnum (&info->Class->Symbols, info->Class);
				break;

			case TK_Identifier:
				// other identifier related checks here
			case TK_Projectile:	// special case: both keyword and property name
				ParseActorProperty(bag);
				break;

			case '+':
			case '-':
				ParseActorFlag(bag, sc_TokenType);
				break;

			default:
				SC_ScriptError("Unexpected '%s' in definition of '%s'", sc_String, bag.Info->Class->TypeName.GetChars());
				break;
			}

		}

		FinishActor(info, bag);
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
			SC_ScriptError("Unexpected error during parsing of actor %s", info->Class->TypeName.GetChars());
		else
			SC_ScriptError("Unexpected error during parsing of actor definitions");
	}
#endif

	SC_SetCMode (false);
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

		// Friendlies never count as kills!
		if (GetDefaultByType(ti)->flags & MF_FRIENDLY)
		{
			GetDefaultByType(ti)->flags &=~MF_COUNTKILL;
		}

		// the typeinfo properties of weapons have to be fixed here after all actors have been declared
		if (ti->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			AWeapon * defaults=(AWeapon *)ti->Defaults;
			fuglyname v;

			v = defaults->AmmoType1;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->AmmoType1 = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->AmmoType1)
					{
						I_Error("Unknown ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
					else if (defaults->AmmoType1->ParentClass != RUNTIME_CLASS(AAmmo))
					{
						I_Error("Invalid ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
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
						I_Error("Unknown ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
					else if (defaults->AmmoType2->ParentClass != RUNTIME_CLASS(AAmmo))
					{
						I_Error("Invalid ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
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
						I_Error("Unknown sister weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
					else if (!defaults->SisterWeaponType->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
					{
						I_Error("Invalid sister weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
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
					if (!ready) I_Error("Weapon %s doesn't define a ready state.\n", ti->TypeName.GetChars());
					if (!select) I_Error("Weapon %s doesn't define a select state.\n", ti->TypeName.GetChars());
					if (!deselect) I_Error("Weapon %s doesn't define a deselect state.\n", ti->TypeName.GetChars());
					if (!fire) I_Error("Weapon %s doesn't define a fire state.\n", ti->TypeName.GetChars());
				}
			}

		}
		// same for the weapon type of weapon pieces.
		else if (ti->IsDescendantOf(RUNTIME_CLASS(AWeaponPiece)))
		{
			AWeaponPiece * defaults=(AWeaponPiece *)ti->Defaults;
			fuglyname v;

			v = defaults->WeaponClass;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->WeaponClass = PClass::FindClass(v);
				if (!defaults->WeaponClass)
				{
					I_Error("Unknown weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
				}
				else if (!defaults->WeaponClass->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
				{
					I_Error("Invalid weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
				}
			}
		}
	}

	// Since these are defined in DECORATE now the table has to be initialized here.
	for(int i=0;i<31;i++)
	{
		char fmt[20];
		sprintf(fmt, "QuestItem%d", i+1);
		QuestItemClasses[i]=PClass::FindClass(fmt);
	}

}

//==========================================================================
//
// ParseClass
//
// A minimal placeholder so that I can assign properties to some native
// classes. Please, no end users use this until it's finalized.
//
//==========================================================================

void ParseClass()
{
	Baggage bag;
	PClass *cls;
	FName classname;
	FName supername;

	SC_MustGetToken(TK_Identifier);	// class name
	classname = sc_String;
	SC_MustGetToken(TK_Extends);	// because I'm not supporting Object
	SC_MustGetToken(TK_Identifier);	// superclass name
	supername = sc_String;
	SC_MustGetToken(TK_Native);		// use actor definitions for your own stuff
	SC_MustGetToken('{');

	cls = const_cast<PClass*>(PClass::FindClass (classname));
	if (cls == NULL)
	{
		SC_ScriptError ("'%s' is not a native class", classname.GetChars());
	}
	if (cls->ParentClass == NULL || cls->ParentClass->TypeName != supername)
	{
		SC_ScriptError ("'%s' does not extend '%s'", classname.GetChars(), supername.GetChars());
	}
	bag.Info = cls->ActorInfo;

	SC_MustGetAnyToken();
	while (sc_TokenType != '}')
	{
		if (sc_TokenType == TK_Action)
		{
			ParseActionDef(cls);
		}
		else if (sc_TokenType == TK_Const)
		{
			ParseConstant(&cls->Symbols, cls);
		}
		else if (sc_TokenType == TK_Enum)
		{
			ParseEnum(&cls->Symbols, cls);
		}
		else
		{
			FString tokname = SC_TokenName(sc_TokenType, sc_String);
			SC_ScriptError ("Expected 'action', 'const' or 'enum' but got %s", tokname.GetChars());
		}
		SC_MustGetAnyToken();
	}
}
