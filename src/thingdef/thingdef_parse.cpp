/*
** thingdef-parse.cpp
**
** Actor definitions - all parser related code
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

#include "doomtype.h"
#include "actor.h"
#include "a_pickups.h"
#include "sc_man.h"
#include "thingdef.h"
#include "a_morph.h"
#include "cmdlib.h"
#include "templates.h"
#include "v_palette.h"
#include "doomerrors.h"
#include "autosegs.h"
#include "i_system.h"


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
// Parses a flag name
//
//==========================================================================
void ParseActorFlag (FScanner &sc, Baggage &bag, int mod)
{
	sc.MustGetString ();

	FString part1 = sc.String;
	const char *part2 = NULL;
	if (sc.CheckString ("."))
	{
		sc.MustGetString ();
		part2 = sc.String;
	}
	HandleActorFlag(sc, bag, part1, part2, mod);
}

//==========================================================================
//
// Processes a flag. Also used by olddecorations.cpp
//
//==========================================================================
void HandleActorFlag(FScanner &sc, Baggage &bag, const char *part1, const char *part2, int mod)
{
	FFlagDef *fd;

	if ( (fd = FindFlag (bag.Info->Class, part1, part2)) )
	{
		AActor *defaults = (AActor*)bag.Info->Class->Defaults;
		if (fd->structoffset == -1)	// this is a deprecated flag that has been changed into a real property
		{
			HandleDeprecatedFlags(defaults, bag.Info, mod=='+', fd->flagbit);
		}
		else
		{
			DWORD * flagvar = (DWORD*) ((char*)defaults + fd->structoffset);
			if (mod == '+')
			{
				*flagvar |= fd->flagbit;
			}
			else
			{
				*flagvar &= ~fd->flagbit;
			}
		}
	}
	else
	{
		if (part2 == NULL)
		{
			sc.ScriptError("\"%s\" is an unknown flag\n", part1);
		}
		else
		{
			sc.ScriptError("\"%s.%s\" is an unknown flag\n", part1, part2);
		}
	}
}

//==========================================================================
//
// [MH] parses a morph style expression
//
//==========================================================================

static int ParseMorphStyle (FScanner &sc)
{
 	static const char * morphstyles[]={
		"MRF_ADDSTAMINA", "MRF_FULLHEALTH", "MRF_UNDOBYTOMEOFPOWER", "MRF_UNDOBYCHAOSDEVICE",
		"MRF_FAILNOTELEFRAG", "MRF_FAILNOLAUGH", "MRF_WHENINVULNERABLE", "MRF_LOSEACTUALWEAPON",
		"MRF_NEWTIDBEHAVIOUR", "MRF_UNDOBYDEATH", "MRF_UNDOBYDEATHFORCED", "MRF_UNDOBYDEATHSAVES", NULL};

 	static const int morphstyle_values[]={
		MORPH_ADDSTAMINA, MORPH_FULLHEALTH, MORPH_UNDOBYTOMEOFPOWER, MORPH_UNDOBYCHAOSDEVICE,
		MORPH_FAILNOTELEFRAG, MORPH_FAILNOLAUGH, MORPH_WHENINVULNERABLE, MORPH_LOSEACTUALWEAPON,
		MORPH_NEWTIDBEHAVIOUR, MORPH_UNDOBYDEATH, MORPH_UNDOBYDEATHFORCED, MORPH_UNDOBYDEATHSAVES};

	// May be given flags by number...
	if (sc.CheckNumber())
	{
		sc.MustGetNumber();
		return sc.Number;
	}

	// ... else should be flags by name.
	// NOTE: Later this should be removed and a normal expression used.
	// The current DECORATE parser can't handle this though.
	bool gotparen = sc.CheckString("(");
	int style = 0;
	do
	{
		sc.MustGetString();
		style |= morphstyle_values[sc.MustMatchString(morphstyles)];
	}
	while (sc.CheckString("|"));
	if (gotparen)
	{
		sc.MustGetStringName(")");
	}

	return style;
}

//==========================================================================
//
// For getting a state address from the parent
// No attempts have been made to add new functionality here
// This is strictly for keeping compatibility with old WADs!
//
//==========================================================================

FState *CheckState(FScanner &sc, PClass *type)
{
	int v=0;

	if (sc.GetString() && !sc.Crossed)
	{
		if (sc.Compare("0")) return NULL;
		else if (sc.Compare("PARENT"))
		{
			FState * state = NULL;
			sc.MustGetString();

			FActorInfo * info = type->ParentClass->ActorInfo;

			if (info != NULL)
			{
				state = info->FindState(FName(sc.String));
			}

			if (sc.GetString ())
			{
				if (sc.Compare ("+"))
				{
					sc.MustGetNumber ();
					v = sc.Number;
				}
				else
				{
					sc.UnGet ();
				}
			}

			if (state == NULL && v==0) return NULL;

			if (v!=0 && state==NULL)
			{
				sc.ScriptError("Attempt to get invalid state from actor %s\n", type->ParentClass->TypeName.GetChars());
				return NULL;
			}
			state+=v;
			return state;
		}
		else sc.ScriptError("Invalid state assignment");
	}
	return NULL;
}

//==========================================================================
//
// Parses an actor property's parameters and calls the handler
//
//==========================================================================

bool ParsePropertyParams(FScanner &sc, FPropertyInfo *prop, AActor *defaults, Baggage &bag)
{
	static TArray<FPropParam> params;
	static TArray<FString> strings;

	params.Clear();
	strings.Clear();
	params.Reserve(1);
	params[0].i = 0;
	if (prop->params[0] != '0')
	{
		const char * p = prop->params;
		bool nocomma;
		bool optcomma;
		while (*p)
		{
			FPropParam conv;
			FPropParam pref;

			nocomma = false;
			conv.s = NULL;
			pref.i = -1;
			switch ((*p) & 223)
			{
			case 'X':	// Expression in parentheses or number.
				
				if (sc.CheckString ("("))
				{
					conv.i = 0x40000000 | ParseExpression (sc, false, bag.Info->Class);
					params.Push(conv);
					sc.MustGetStringName(")");
					break;
				}
				// fall through

			case 'I':
				sc.MustGetNumber();
				conv.i = sc.Number;
				break;

			case 'F':
				sc.MustGetFloat();
				conv.f = sc.Float;
				break;

			case 'Z':	// an optional string. Does not allow any numerical value.
				if (sc.CheckFloat())
				{
					nocomma = true;
					sc.UnGet();
					break;
				}
				// fall through

			case 'S':
			case 'T':
				sc.MustGetString();
				conv.s = strings[strings.Reserve(1)] = sc.String;
				break;

			case 'C':
				if (sc.CheckNumber ())
				{
					int R, G, B;
					R = clamp (sc.Number, 0, 255);
					sc.CheckString (",");
					sc.MustGetNumber ();
					G = clamp (sc.Number, 0, 255);
					sc.CheckString (",");
					sc.MustGetNumber ();
					B = clamp (sc.Number, 0, 255);
					conv.i = MAKERGB(R, G, B);
					pref.i = 0;
				}
				else
				{
					sc.MustGetString ();
					conv.s = strings[strings.Reserve(1)] = sc.String;
					pref.i = 1;
				}
				break;

			case 'M':	// special case. An expression-aware parser will not need this.
				conv.i = ParseMorphStyle(sc);
				break;

			case 'L':	// Either a number or a list of strings
				if (sc.CheckNumber())
				{
					pref.i = 0;
					conv.i = sc.Number;
				}
				else
				{
					pref.i = 1;
					params.Push(pref);
					params[0].i++;

					do
					{
						sc.MustGetString ();
						conv.s = strings[strings.Reserve(1)] = sc.String;
						params.Push(conv);
						params[0].i++;
					}
					while (sc.CheckString(","));
					goto endofparm;
				}
				break;

			default:
				assert(false);
				break;

			}
			if (pref.i != -1)
			{
				params.Push(pref);
				params[0].i++;
			}
			params.Push(conv);
			params[0].i++;
		endofparm:
			p++;
			// Hack for some properties that have to allow comma less
			// parameter lists for compatibility.
			if ((optcomma = (*p == '_'))) 
				p++;

			if (nocomma) 
			{
				continue;
			}
			else if (*p == 0) 
			{
				break;
			}
			else if (*p >= 'a')
			{
				if (!sc.CheckString(","))
				{
					if (optcomma)
					{
						if (!sc.CheckFloat()) break;
						else sc.UnGet();
					}
					else break;
				}
			}
			else 
			{
				if (!optcomma) sc.MustGetStringName(",");
				else sc.CheckString(",");
			}
		}
	}
	// call the handler
	try
	{
		prop->Handler(defaults, bag, &params[0]);
	}
	catch (CRecoverableError &error)
	{
		sc.ScriptError("%s", error.GetMessage());
	}

	return true;
}

//==========================================================================
//
// Parses an actor property
//
//==========================================================================

void ParseActorProperty(FScanner &sc, Baggage &bag)
{
	static const char *statenames[] = {
		"Spawn", "See", "Melee", "Missile", "Pain", "Death", "XDeath", "Burn", 
		"Ice", "Raise", "Crash", "Crush", "Wound", "Disintegrate", "Heal", NULL };

	strlwr (sc.String);

	FString propname = sc.String;

	if (sc.CheckString ("."))
	{
		sc.MustGetString ();
		propname += '.';
		strlwr (sc.String);
		propname += sc.String;
	}
	else
	{
		sc.UnGet ();
	}

	FPropertyInfo *prop = FindProperty(propname);

	if (prop != NULL)
	{
		if (bag.Info->Class->IsDescendantOf(prop->cls))
		{
			ParsePropertyParams(sc, prop, (AActor *)bag.Info->Class->Defaults, bag);
		}
		else
		{
			sc.ScriptError("\"%s\" requires an actor of type \"%s\"\n", propname.GetChars(), prop->cls->TypeName.GetChars());
		}
	}
	else if (!propname.CompareNoCase("States"))
	{
		if (!bag.StateSet) ParseStates(sc, bag.Info, (AActor *)bag.Info->Class->Defaults, bag);
		else sc.ScriptError("Multiple state declarations not allowed");
		bag.StateSet=true;
	}
	else if (MatchString(propname, statenames) != -1)
	{
		bag.statedef.AddState(propname, CheckState (sc, bag.Info->Class));
	}
	else
	{
		sc.ScriptError("\"%s\" is an unknown actor property\n", propname.GetChars());
	}
}


//==========================================================================
//
// Finalizes an actor definition
//
//==========================================================================

void FinishActor(FScanner &sc, FActorInfo *info, Baggage &bag)
{
	AActor *defaults = (AActor*)info->Class->Defaults;

	try
	{
		bag.statedef.FinishStates (info, defaults, bag.StateArray);
	}
	catch (CRecoverableError &err)
	{
		sc.ScriptError(err.GetMessage());
	}
	bag.statedef.InstallStates (info, defaults);
	bag.StateArray.Clear ();
	if (bag.DropItemSet)
	{
		if (bag.DropItemList == NULL)
		{
			if (info->Class->Meta.GetMetaInt (ACMETA_DropItems) != 0)
			{
				info->Class->Meta.SetMetaInt (ACMETA_DropItems, 0);
			}
		}
		else
		{
			info->Class->Meta.SetMetaInt (ACMETA_DropItems,
				StoreDropItemChain(bag.DropItemList));
		}
	}
	if (info->Class->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		defaults->flags |= MF_SPECIAL;
	}
}
