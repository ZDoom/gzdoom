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
#include "i_system.h"
#include "thingdef_exp.h"
#include "w_wad.h"
#include "v_video.h"
#include "version.h"
#include "v_text.h"
#include "m_argv.h"

void ParseOldDecoration(FScanner &sc, EDefinitionType def);

//==========================================================================
//
// ParseParameter
//
// Parses a parameter - either a default in a function declaration 
// or an argument in a function call.
//
//==========================================================================

FxExpression *ParseParameter(FScanner &sc, PClass *cls, char type, bool constant)
{
	FxExpression *x = NULL;
	int v;

	switch(type)
	{
	case 'S':
	case 's':		// Sound name
		sc.MustGetString();
		x = new FxConstant(FSoundID(sc.String), sc);
		break;

	case 'M':
	case 'm':		// Actor name
		sc.SetEscape(true);
		sc.MustGetString();
		sc.SetEscape(false);
		x = new FxClassTypeCast(RUNTIME_CLASS(AActor), new FxConstant(FName(sc.String), sc));
		break;

	case 'T':
	case 't':		// String
		sc.SetEscape(true);
		sc.MustGetString();
		sc.SetEscape(false);
		x = new FxConstant(sc.String[0]? FName(sc.String) : NAME_None, sc);
		break;

	case 'C':
	case 'c':		// Color
		sc.MustGetString ();
		if (sc.Compare("none"))
		{
			v = -1;
		}
		else if (sc.Compare(""))
		{
			v = 0;
		}
		else
		{
			int c = V_GetColor (NULL, sc.String);
			// 0 needs to be the default so we have to mark the color.
			v = MAKEARGB(1, RPART(c), GPART(c), BPART(c));
		}
		{
			ExpVal val;
			val.Type = VAL_Color;
			val.Int = v;
			x = new FxConstant(val, sc);
			break;
		}

	case 'L':
	case 'l':
	{
		// This forces quotation marks around the state name.
		sc.MustGetToken(TK_StringConst);
		if (sc.String[0] == 0 || sc.Compare("None"))
		{
			x = new FxConstant((FState*)NULL, sc);
		}
		else if (sc.Compare("*"))
		{
			if (constant) 
			{
				x = new FxConstant((FState*)(intptr_t)-1, sc);
			}
			else sc.ScriptError("Invalid state name '*'");
		}
		else
		{
			x = new FxMultiNameState(sc.String, sc);
		}
		break;
	}

	case 'X':
	case 'x':
	case 'Y':
	case 'y':
		x = ParseExpression (sc, cls);
		if (constant && !x->isConstant())
		{
			sc.ScriptMessage("Default parameter must be constant.");
			FScriptPosition::ErrorCounter++;
		}
		break;

	default:
		assert(false);
		return NULL;
	}
	return x;
}

//==========================================================================
//
// ActorConstDef
//
// Parses a constant definition.
//
//==========================================================================

static void ParseConstant (FScanner &sc, PSymbolTable * symt, PClass *cls)
{
	// Read the type and make sure it's int or float.
	if (sc.CheckToken(TK_Int) || sc.CheckToken(TK_Float))
	{
		int type = sc.TokenType;
		sc.MustGetToken(TK_Identifier);
		FName symname = sc.String;
		sc.MustGetToken('=');
		FxExpression *expr = ParseExpression (sc, cls);
		sc.MustGetToken(';');

		ExpVal val = expr->EvalExpression(NULL);
		delete expr;
		PSymbolConst *sym = new PSymbolConst(symname);
		if (type == TK_Int)
		{
			sym->ValueType = VAL_Int;
			sym->Value = val.GetInt();
		}
		else
		{
			sym->ValueType = VAL_Float;
			sym->Float = val.GetFloat();
		}
		if (symt->AddSymbol (sym) == NULL)
		{
			delete sym;
			sc.ScriptMessage ("'%s' is already defined in '%s'.",
				symname.GetChars(), cls? cls->TypeName.GetChars() : "Global");
			FScriptPosition::ErrorCounter++;
		}
	}
	else
	{
		sc.ScriptMessage("Numeric type required for constant");
		FScriptPosition::ErrorCounter++;
	}
}

//==========================================================================
//
// ActorEnumDef
//
// Parses an enum definition.
//
//==========================================================================

static void ParseEnum (FScanner &sc, PSymbolTable *symt, PClass *cls)
{
	int currvalue = 0;

	sc.MustGetToken('{');
	while (!sc.CheckToken('}'))
	{
		sc.MustGetToken(TK_Identifier);
		FName symname = sc.String;
		if (sc.CheckToken('='))
		{
			FxExpression *expr = ParseExpression (sc, cls);
			currvalue = expr->EvalExpression(NULL).GetInt();
			delete expr;
		}
		PSymbolConst *sym = new PSymbolConst(symname);
		sym->ValueType = VAL_Int;
		sym->Value = currvalue;
		if (symt->AddSymbol (sym) == NULL)
		{
			delete sym;
			sc.ScriptMessage ("'%s' is already defined in '%s'.",
				symname.GetChars(), cls? cls->TypeName.GetChars() : "Global");
			FScriptPosition::ErrorCounter++;
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
// ParseNativeVariable
//
// Parses a native variable declaration.
//
//==========================================================================

static void ParseNativeVariable (FScanner &sc, PSymbolTable * symt, PClass *cls)
{
	FExpressionType valuetype;

	if (sc.LumpNum == -1 || Wads.GetLumpFile(sc.LumpNum) > 0)
	{
		sc.ScriptMessage ("variables can only be imported by internal class and actor definitions!");
		FScriptPosition::ErrorCounter++;
	}

	// Read the type and make sure it's int or float.
	sc.MustGetAnyToken();
	switch (sc.TokenType)
	{
	case TK_Int:
		valuetype = VAL_Int;
		break;

	case TK_Float:
		valuetype = VAL_Float;
		break;

	case TK_Angle_t:
		valuetype = VAL_Angle;
		break;

	case TK_Fixed_t:
		valuetype = VAL_Fixed;
		break;

	case TK_Bool:
		valuetype = VAL_Bool;
		break;

	case TK_Identifier:
		valuetype = VAL_Object;
		// Todo: Object type
		sc.ScriptError("Object type variables not implemented yet!");
		break;

	default:
		sc.ScriptError("Invalid variable type %s", sc.String);
		return;
	}

	sc.MustGetToken(TK_Identifier);
	FName symname = sc.String;
	if (sc.CheckToken('['))
	{
		FxExpression *expr = ParseExpression (sc, cls);
		int maxelems = expr->EvalExpression(NULL).GetInt();
		delete expr;
		sc.MustGetToken(']');
		valuetype.MakeArray(maxelems);
	}
	sc.MustGetToken(';');

	const FVariableInfo *vi = FindVariable(symname, cls);
	if (vi == NULL)
	{
		sc.ScriptError("Unknown native variable '%s'", symname.GetChars());
	}

	PSymbolVariable *sym = new PSymbolVariable(symname);
	sym->offset = vi->address;	// todo
	sym->ValueType = valuetype;
	sym->bUserVar = false;

	if (symt->AddSymbol (sym) == NULL)
	{
		delete sym;
		sc.ScriptMessage ("'%s' is already defined in '%s'.",
			symname.GetChars(), cls? cls->TypeName.GetChars() : "Global");
		FScriptPosition::ErrorCounter++;
	}
}

//==========================================================================
//
// ParseUserVariable
//
// Parses a user variable declaration.
//
//==========================================================================

static void ParseUserVariable (FScanner &sc, PSymbolTable *symt, PClass *cls)
{
	FExpressionType valuetype;

	// Only non-native classes may have user variables.
	if (!cls->bRuntimeClass)
	{
		sc.ScriptError("Native classes may not have user variables");
	}

	// Read the type and make sure it's int.
	sc.MustGetAnyToken();
	if (sc.TokenType != TK_Int)
	{
		sc.ScriptMessage("User variables must be of type int");
		FScriptPosition::ErrorCounter++;
	}
	valuetype = VAL_Int;

	sc.MustGetToken(TK_Identifier);
	// For now, restrict user variables to those that begin with "user_" to guarantee
	// no clashes with internal member variable names.
	if (sc.StringLen < 6 || strnicmp("user_", sc.String, 5) != 0)
	{
		sc.ScriptMessage("User variable names must begin with \"user_\"");
		FScriptPosition::ErrorCounter++;
	}



	FName symname = sc.String;
	if (sc.CheckToken('['))
	{
		FxExpression *expr = ParseExpression(sc, cls);
		int maxelems = expr->EvalExpression(NULL).GetInt();
		delete expr;
		sc.MustGetToken(']');
		if (maxelems <= 0)
		{
			sc.ScriptMessage("Array size must be positive");
			FScriptPosition::ErrorCounter++;
			maxelems = 1;
		}
		valuetype.MakeArray(maxelems);
	}
	sc.MustGetToken(';');

	// We must ensure that we do not define duplicates, even when they come from a parent table.
	if (symt->FindSymbol(symname, true) != NULL)
	{
		sc.ScriptMessage ("'%s' is already defined in '%s' or one of its ancestors.",
			symname.GetChars(), cls ? cls->TypeName.GetChars() : "Global");
		FScriptPosition::ErrorCounter++;
		return;
	}

	PSymbolVariable *sym = new PSymbolVariable(symname);
	sym->offset = cls->Extend(sizeof(int) * (valuetype.Type == VAL_Array ? valuetype.size : 1));
	sym->ValueType = valuetype;
	sym->bUserVar = true;
	if (symt->AddSymbol(sym) == NULL)
	{
		delete sym;
		sc.ScriptMessage ("'%s' is already defined in '%s'.",
			symname.GetChars(), cls ? cls->TypeName.GetChars() : "Global");
		FScriptPosition::ErrorCounter++;
	}
}

//==========================================================================
//
// Parses a flag name
//
//==========================================================================
static void ParseActorFlag (FScanner &sc, Baggage &bag, int mod)
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
			ModActorFlag(defaults, fd, mod == '+');
		}
	}
	else
	{
		if (part2 == NULL)
		{
			sc.ScriptMessage("\"%s\" is an unknown flag\n", part1);
		}
		else
		{
			sc.ScriptMessage("\"%s.%s\" is an unknown flag\n", part1, part2);
		}
		FScriptPosition::ErrorCounter++;
	}
}

//==========================================================================
//
// [MH] parses a morph style expression
//
//==========================================================================

struct FParseValue
{
	const char *Name;
	int Flag;
};

int ParseFlagExpressionString(FScanner &sc, const FParseValue *vals)
{
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
		style |= vals[sc.MustMatchString(&vals->Name, sizeof (*vals))].Flag;
	}
	while (sc.CheckString("|"));
	if (gotparen)
	{
		sc.MustGetStringName(")");
	}

	return style;
}



static int ParseMorphStyle (FScanner &sc)
{
 	static const FParseValue morphstyles[]={
		{ "MRF_ADDSTAMINA",			MORPH_ADDSTAMINA},
		{ "MRF_FULLHEALTH",			MORPH_FULLHEALTH}, 
		{ "MRF_UNDOBYTOMEOFPOWER",	MORPH_UNDOBYTOMEOFPOWER},  
		{ "MRF_UNDOBYCHAOSDEVICE",	MORPH_UNDOBYCHAOSDEVICE},
		{ "MRF_FAILNOTELEFRAG",		MORPH_FAILNOTELEFRAG}, 
		{ "MRF_FAILNOLAUGH",		MORPH_FAILNOLAUGH}, 
		{ "MRF_WHENINVULNERABLE",	MORPH_WHENINVULNERABLE}, 
		{ "MRF_LOSEACTUALWEAPON",	MORPH_LOSEACTUALWEAPON},
		{ "MRF_NEWTIDBEHAVIOUR",	MORPH_NEWTIDBEHAVIOUR}, 
		{ "MRF_UNDOBYDEATH",		MORPH_UNDOBYDEATH}, 
		{ "MRF_UNDOBYDEATHFORCED",	MORPH_UNDOBYDEATHFORCED},  
		{ "MRF_UNDOBYDEATHSAVES",	MORPH_UNDOBYDEATHSAVES},
		{ "MRF_UNDOALWAYS",			MORPH_UNDOALWAYS },
		{ NULL, 0 }
	};

	return ParseFlagExpressionString(sc, morphstyles);
}

static int ParseThingActivation (FScanner &sc)
{
 	static const FParseValue activationstyles[]={

		{ "THINGSPEC_Default",				THINGSPEC_Default},
		{ "THINGSPEC_ThingActs",			THINGSPEC_ThingActs},
		{ "THINGSPEC_ThingTargets",			THINGSPEC_ThingTargets},
		{ "THINGSPEC_TriggerTargets",		THINGSPEC_TriggerTargets},
		{ "THINGSPEC_MonsterTrigger",		THINGSPEC_MonsterTrigger},
		{ "THINGSPEC_MissileTrigger",		THINGSPEC_MissileTrigger},
		{ "THINGSPEC_ClearSpecial",			THINGSPEC_ClearSpecial},
		{ "THINGSPEC_NoDeathSpecial",		THINGSPEC_NoDeathSpecial},
		{ "THINGSPEC_TriggerActs",			THINGSPEC_TriggerActs},
		{ "THINGSPEC_Activate",				THINGSPEC_Activate},
		{ "THINGSPEC_Deactivate",			THINGSPEC_Deactivate},
		{ "THINGSPEC_Switch",				THINGSPEC_Switch},
		{ NULL, 0 }
	};

	return ParseFlagExpressionString(sc, activationstyles);
}

//==========================================================================
//
// For getting a state address from the parent
// No attempts have been made to add new functionality here
// This is strictly for keeping compatibility with old WADs!
//
//==========================================================================

static FState *CheckState(FScanner &sc, PClass *type)
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
				sc.ScriptMessage("Attempt to get invalid state from actor %s\n", type->ParentClass->TypeName.GetChars());
				FScriptPosition::ErrorCounter++;
				return NULL;
			}
			state+=v;
			return state;
		}
		else 
		{
			sc.ScriptMessage("Invalid state assignment");
			FScriptPosition::ErrorCounter++;
		}
	}
	return NULL;
}

//==========================================================================
//
// Parses an actor property's parameters and calls the handler
//
//==========================================================================

static bool ParsePropertyParams(FScanner &sc, FPropertyInfo *prop, AActor *defaults, Baggage &bag)
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
			pref.s = NULL;
			pref.i = -1;
			switch ((*p) & 223)
			{
			case 'X':	// Expression in parentheses or number.
				
				if (sc.CheckString ("("))
				{
					FxExpression *x = ParseExpression(sc, bag.Info->Class);
					conv.i = 0x40000000 | StateParams.Add(x, bag.Info->Class, false);
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
				conv.f = float(sc.Float);
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
				sc.MustGetString();
				conv.s = strings[strings.Reserve(1)] = sc.String;
				break;

			case 'T':
				sc.MustGetString();
				conv.s = strings[strings.Reserve(1)] = strbin1(sc.String);
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
				
			case 'N':	// special case. An expression-aware parser will not need this.
				conv.i = ParseThingActivation(sc);
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
		prop->Handler(defaults, bag.Info, bag, &params[0]);
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

static void ParseActorProperty(FScanner &sc, Baggage &bag)
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
			sc.ScriptMessage("\"%s\" requires an actor of type \"%s\"\n", propname.GetChars(), prop->cls->TypeName.GetChars());
			FScriptPosition::ErrorCounter++;
		}
	}
	else if (!propname.CompareNoCase("States"))
	{
		if (bag.StateSet) 
		{
			sc.ScriptMessage("'%s' contains multiple state declarations", bag.Info->Class->TypeName.GetChars());
			FScriptPosition::ErrorCounter++;
		}
		ParseStates(sc, bag.Info, (AActor *)bag.Info->Class->Defaults, bag);
		bag.StateSet=true;
	}
	else if (MatchString(propname, statenames) != -1)
	{
		bag.statedef.SetStateLabel(propname, CheckState (sc, bag.Info->Class));
	}
	else
	{
		sc.ScriptError("\"%s\" is an unknown actor property\n", propname.GetChars());
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

	unsigned int error = 0;
	const AFuncDesc *afd;
	FName funcname;
	FString args;
	TArray<FxExpression *> DefaultParams;
	bool hasdefaults = false;
	
	if (sc.LumpNum == -1 || Wads.GetLumpFile(sc.LumpNum) > 0)
	{
		sc.ScriptMessage ("Action functions can only be imported by internal class and actor definitions!");
		++error;
	}

	sc.MustGetToken(TK_Native);
	sc.MustGetToken(TK_Identifier);
	funcname = sc.String;
	afd = FindFunction(sc.String);
	if (afd == NULL)
	{
		sc.ScriptMessage ("The function '%s' has not been exported from the executable.", sc.String);
		++error;
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
				type = 'x';
				break;

			case TK_Float:
				type = 'y';
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
				sc.ScriptMessage ("Unknown variable type %s", sc.TokenName(sc.TokenType, sc.String).GetChars());
				type = 'x';
				FScriptPosition::ErrorCounter++;
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

			FxExpression *def;
			if (sc.CheckToken('='))
			{
				hasdefaults = true;
				flags |= OPTIONAL;
				def = ParseParameter(sc, cls, type, true);
			}
			else
			{
				def = NULL;
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
	if (afd != NULL)
	{
		PSymbolActionFunction *sym = new PSymbolActionFunction(funcname);
		sym->Arguments = args;
		sym->Function = afd->Function;
		if (hasdefaults)
		{
			sym->defaultparameterindex = StateParams.Size();
			for(unsigned int i = 0; i < DefaultParams.Size(); i++)
			{
				StateParams.Add(DefaultParams[i], cls, true);
			}
		}
		else
		{
			sym->defaultparameterindex = -1;
		}
		if (error)
		{
			FScriptPosition::ErrorCounter += error;
		}
		else if (cls->Symbols.AddSymbol (sym) == NULL)
		{
			delete sym;
			sc.ScriptMessage ("'%s' is already defined in class '%s'.",
				funcname.GetChars(), cls->TypeName.GetChars());
			FScriptPosition::ErrorCounter++;
		}
	}
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

		if (replaceName == typeName)
		{
			sc.ScriptMessage ("Cannot replace class %s with itself", typeName.GetChars());
			FScriptPosition::ErrorCounter++;
		}
	}

	// Now, after the actor names have been parsed, it is time to switch to C-mode 
	// for the rest of the actor definition.
	sc.SetCMode (true);
	if (sc.CheckNumber()) 
	{
		if (sc.Number>=-1 && sc.Number<32768) DoomEdNum = sc.Number;
		else 
		{
			// does not need to be fatal.
			sc.ScriptMessage ("DoomEdNum must be in the range [-1,32767]");
			FScriptPosition::ErrorCounter++;
		}
	}

	if (sc.CheckString("native"))
	{
		native = true;
	}

	try
	{
		FActorInfo *info =  CreateNewActor(sc, typeName, parentName, native);
		info->DoomEdNum = DoomEdNum > 0? DoomEdNum : -1;
		info->Class->Meta.SetMetaString (ACMETA_Lump, Wads.GetLumpFullPath(sc.LumpNum));

		SetReplacement(sc, info, replaceName);

		ResetBaggage (bag, info->Class->ParentClass);
		bag->Info = info;
		bag->Lumpnum = sc.LumpNum;
#ifdef _DEBUG
		bag->ClassName = typeName;
#endif
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
static void ParseActor(FScanner &sc)
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

		case TK_Native:
			ParseNativeVariable (sc, &info->Class->Symbols, info->Class);
			break;

		case TK_Var:
			ParseUserVariable (sc, &info->Class->Symbols, info->Class);
			break;

		case TK_Identifier:
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
// Reads a damage definition
//
//==========================================================================

static void ParseDamageDefinition(FScanner &sc)
{
	sc.SetCMode (true); // This may be 100% irrelevant for such a simple syntax, but I don't know

	// Get DamageType

	sc.MustGetString();
	FName damageType = sc.String;

	DamageTypeDefinition dtd;

	sc.MustGetToken('{');
	while (sc.MustGetAnyToken(), sc.TokenType != '}')
	{
		if (sc.Compare("FACTOR"))
		{
			sc.MustGetFloat();
			dtd.DefaultFactor = FLOAT2FIXED(sc.Float);
			if (!dtd.DefaultFactor) dtd.ReplaceFactor = true; // Multiply by 0 yields 0: FixedMul(damage, FixedMul(factor, 0)) is more wasteful than FixedMul(factor, 0)
		}
		else if (sc.Compare("REPLACEFACTOR"))
		{
			dtd.ReplaceFactor = true;
		}
		else if (sc.Compare("NOARMOR"))
		{
			dtd.NoArmor = true;
		}
		else
		{
			sc.ScriptError("Unexpected data (%s) in damagetype definition.", sc.String);
		}
	}

	dtd.Apply(damageType);

	sc.SetCMode (false); // (set to true earlier in function)
}

//==========================================================================
//
// ParseDecorate
//
// Parses a single DECORATE lump
//
//==========================================================================

void ParseDecorate (FScanner &sc)
{
	// Get actor class name.
	for(;;)
	{
		FScanner::SavedPos pos = sc.SavePos();
		if (!sc.GetToken ())
		{
			return;
		}
		switch (sc.TokenType)
		{
		case TK_Include:
		{
			sc.MustGetString();
			// This check needs to remain overridable for testing purposes.
			if (Wads.GetLumpFile(sc.LumpNum) == 0 && !Args->CheckParm("-allowdecoratecrossincludes"))
			{
				int includefile = Wads.GetLumpFile(Wads.CheckNumForFullName(sc.String, true));
				if (includefile != 0)
				{
					I_FatalError("File %s is overriding core lump %s.",
						Wads.GetWadFullName(includefile), sc.String);
				}
			}
			FScanner newscanner;
			newscanner.Open(sc.String);
			ParseDecorate(newscanner);
			break;
		}

		case TK_Const:
			ParseConstant (sc, &GlobalSymbols, NULL);
			break;

		case TK_Enum:
			ParseEnum (sc, &GlobalSymbols, NULL);
			break;

		case TK_Native:
			ParseNativeVariable(sc, &GlobalSymbols, NULL);
			break;

		case ';':
			// ';' is the start of a comment in the non-cmode parser which
			// is used to parse parts of the DECORATE lump. If we don't add 
			// a check here the user will only get weird non-informative
			// error messages if a semicolon is found.
			sc.ScriptError("Unexpected ';'");
			break;

		case TK_Identifier:
			// 'ACTOR' cannot be a keyword because it is also needed as a class identifier
			// so let's do a special case for this.
			if (sc.Compare("ACTOR"))
			{
				ParseActor (sc);
				break;
			}
			else if (sc.Compare("PICKUP"))
			{
				ParseOldDecoration (sc, DEF_Pickup);
				break;
			}
			else if (sc.Compare("BREAKABLE"))
			{
				ParseOldDecoration (sc, DEF_BreakableDecoration);
				break;
			}
			else if (sc.Compare("PROJECTILE"))
			{
				ParseOldDecoration (sc, DEF_Projectile);
				break;
			}
			else if (sc.Compare("DAMAGETYPE"))
			{
				ParseDamageDefinition(sc);
				break;
			}
		default:
			sc.RestorePos(pos);
			ParseOldDecoration(sc, DEF_Decoration);
			break;
		}
	}
}
