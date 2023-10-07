/*
** thingdef-parse.cpp
**
** Actor definitions - all parser related code
**
**---------------------------------------------------------------------------
** Copyright 2002-2016 Christoph Oelckers
** Copyright 2004-2016 Randy Heit
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
#include "thingdef.h"
#include "a_morph.h"
#include "codegen.h"
#include "backend/codegen_doom.h"
#include "filesystem.h"
#include "v_text.h"
#include "m_argv.h"
#include "v_video.h"
#ifndef _MSC_VER
#include "i_system.h"  // for strlwr()
#endif // !_MSC_VER

void ParseOldDecoration(FScanner &sc, EDefinitionType def, PNamespace *ns);
EXTERN_CVAR(Bool, strictdecorate);


//==========================================================================
//
// DecoDerivedClass
//
// Create a derived class and performs some additional sanity checks
//
//==========================================================================

PClassActor *DecoDerivedClass(const FScriptPosition &sc, PClassActor *parent, FName typeName)
{
	const PClassType *const parentVMType = parent->VMType;

	if (parentVMType == nullptr)
	{
		// Abort when forward declared class is used as a parent. This edge case cannot be handled gracefully.
		sc.Message(MSG_FATAL, "Tried to define class '%s' without definition of parent class '%s'.", typeName.GetChars(), parent->TypeName.GetChars());
	}
	else if (parentVMType->mVersion > MakeVersion(2, 0))
	{
		sc.Message(MSG_ERROR, "Parent class %s of %s not accessible to DECORATE", parent->TypeName.GetChars(), typeName.GetChars());
	}
	else
	{
		// [Player701] Parent class must not have abstract functions
		for (auto v : parent->Virtuals)
		{
			if (v->VarFlags & VARF_Abstract)
			{
				sc.Message(MSG_ERROR, "Parent class %s of %s cannot have abstract functions.", parent->TypeName.GetChars(), typeName.GetChars());
				break;
			}
		}
	}

	bool newlycreated;
	PClassActor *type = static_cast<PClassActor *>(parent->CreateDerivedClass(typeName, parent->Size, &newlycreated));
	if (type == nullptr)
	{
		FString newname = typeName.GetChars();
		FString sourcefile = sc.FileName.GetChars();

		sourcefile.Substitute(":", "@");
		newname << '@' << sourcefile;
		if (strictdecorate)
		{
			sc.Message(MSG_ERROR, "Tried to define class '%s' more than once.", typeName.GetChars());
		}
		else
		{
			// Due to backwards compatibility issues this cannot be an unconditional error.
			sc.Message(MSG_WARNING, "Tried to define class '%s' more than once. Renaming class to '%s'", typeName.GetChars(), newname.GetChars());
		}
		type = static_cast<PClassActor *>(parent->CreateDerivedClass(newname, parent->Size, &newlycreated));
		if (type == nullptr)
		{
			// This we cannot handle cleanly anymore. Let's just abort and forget about the odd mod out that was this careless.
			sc.Message(MSG_FATAL, "Tried to define class '%s' more than twice in the same file.", typeName.GetChars());
		}
	}
	if (newlycreated) type->InitializeDefaults();

	if (type != nullptr)
	{
		// [ZZ] DECORATE classes are always play
		auto vmtype = type->VMType;
		vmtype->ScopeFlags = FScopeBarrier::ChangeSideInObjectFlags(vmtype->ScopeFlags, FScopeBarrier::Side_Play);
	}

	return type;
}

//==========================================================================
//
// ParseParameter
//
// Parses an argument in a function call.
//
//==========================================================================

FxExpression *ParseParameter(FScanner &sc, PClassActor *cls, PType *type)
{
	FxExpression *x = NULL;
	int v;

	if (type == TypeSound)
	{
		sc.MustGetString();
		x = new FxConstant(S_FindSound(sc.String), sc);
	}
	else if (type == TypeBool || type == TypeSInt32 || type == TypeFloat64)
	{
		x = ParseExpression (sc, cls, nullptr);
		// Do automatic coercion between bools, ints and floats.
		if (type == TypeBool)
		{
			x = new FxBoolCast(x);
		}
		else if (type == TypeSInt32)
		{
			x = new FxIntCast(x, true);
		}
		else
		{
			x = new FxFloatCast(x);
		}
	}
	else if (type == TypeName || type == TypeString)
	{
		sc.SetEscape(true);
		sc.MustGetString();
		sc.SetEscape(false);
		if (type == TypeName)
		{
			x = new FxConstant(sc.String[0] ? FName(sc.String) : NAME_None, sc);
		}
		else
		{
			x = new FxConstant(strbin1(sc.String), sc);
		}
	}
	else if (type == TypeColor)
	{
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
			int c = V_GetColor (sc);
			// 0 needs to be the default so we have to mark the color.
			v = MAKEARGB(1, RPART(c), GPART(c), BPART(c));
		}
		ExpVal val;
		val.Type = TypeColor;
		val.Int = v;
		x = new FxConstant(val, sc);
	}
	else if (type == TypeStateLabel)
	{
		// This forces quotation marks around the state name.
		if (sc.CheckToken(TK_StringConst))
		{
			if (sc.String[0] == 0 || sc.Compare("None"))
			{
				x = new FxConstant(0, sc);
				x->ValueType = TypeStateLabel;
			}
			else if (sc.Compare("*"))
			{
				sc.ScriptError("Invalid state name '*'");
			}
			else
			{
				x = new FxMultiNameState(sc.String, sc);
			}
		}
		else
		{
			x = new FxRuntimeStateIndex(ParseExpression(sc, cls));
		}
	}
	else if (type->isClassPointer())
	{	// Actor name
		sc.SetEscape(true);
		sc.MustGetString();
		sc.SetEscape(false);
		x = new FxClassTypeCast(static_cast<PClassPointer *>(type), new FxConstant(FName(sc.String), sc), false);
	}
	else
	{
		assert(false && "Unknown parameter type");
		x = NULL;
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

static void ParseConstant (FScanner &sc, PSymbolTable *symt, PClassActor *cls, PNamespace *ns)
{
	// Read the type and make sure it's int or float.
	if (sc.CheckToken(TK_Int) || sc.CheckToken(TK_Float))
	{
		int type = sc.TokenType;
		sc.MustGetToken(TK_Identifier);
		FName symname = sc.String;
		sc.MustGetToken('=');
		FxExpression *expr = ParseExpression (sc, cls, ns);
		sc.MustGetToken(';');

		if (expr == nullptr)
		{
			sc.ScriptMessage("Error while resolving constant definition");
			FScriptPosition::ErrorCounter++;
		}
		else if (!expr->isConstant())
		{
			sc.ScriptMessage("Constant definition is not a constant");
			FScriptPosition::ErrorCounter++;
		}
		else
		{
			ExpVal val = static_cast<FxConstant *>(expr)->GetValue();
			delete expr;
			PSymbolConstNumeric *sym;
			if (type == TK_Int)
			{
				sym = Create<PSymbolConstNumeric>(symname, TypeSInt32);
				sym->Value = val.GetInt();
			}
			else
			{
				sym = Create<PSymbolConstNumeric>(symname, TypeFloat64);
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

static void ParseEnum (FScanner &sc, PSymbolTable *symt, PClassActor *cls, PNamespace *ns)
{
	int currvalue = 0;

	sc.MustGetToken('{');
	while (!sc.CheckToken('}'))
	{
		sc.MustGetToken(TK_Identifier);
		FName symname = sc.String;
		if (sc.CheckToken('='))
		{
			FxExpression *expr = ParseExpression (sc, cls, ns);
			if (expr != nullptr)
			{
				if (!expr->isConstant())
				{
					sc.ScriptMessage("'%s' must be constant", symname.GetChars());
					FScriptPosition::ErrorCounter++;
				}
				else
				{
					currvalue = static_cast<FxConstant *>(expr)->GetValue().GetInt();
				}
				delete expr;
			}
			else
			{
				sc.ScriptMessage("Error while resolving expression of '%s'", symname.GetChars());
				FScriptPosition::ErrorCounter++;
			}
		}
		PSymbolConstNumeric *sym = Create<PSymbolConstNumeric>(symname, TypeSInt32);
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
// ParseUserVariable
//
// Parses a user variable declaration.
//
//==========================================================================

static void ParseUserVariable (FScanner &sc, PSymbolTable *symt, PClassActor *cls, PNamespace *ns)
{
	PType *type;
	int maxelems = 1;

	// Only non-native classes may have user variables.
	if (!cls->bRuntimeClass)
	{
		sc.ScriptError("Native classes may not have user variables");
	}

	// Read the type and make sure it's acceptable.
	sc.MustGetAnyToken();
	if (sc.TokenType != TK_Int && sc.TokenType != TK_Float)
	{
		sc.ScriptMessage("User variables must be of type 'int' or 'float'");
		FScriptPosition::ErrorCounter++;
	}
	type = sc.TokenType == TK_Int ? (PType *)TypeSInt32 : (PType *)TypeFloat64;

	sc.MustGetToken(TK_Identifier);
	// For now, restrict user variables to those that begin with "user_" to guarantee
	// no clashes with internal member variable names.
	if (sc.StringLen < 6 || strnicmp("user_", sc.String, 5) != 0)
	{
		sc.ScriptMessage("User variable names must begin with \"user_\"");
		FScriptPosition::ErrorCounter++;
	}

	FName symname = sc.String;

	// We must ensure that we do not define duplicates, even when they come from a parent table.
	if (symt->FindSymbol(symname, true) != NULL)
	{
		sc.ScriptMessage ("'%s' is already defined in '%s' or one of its ancestors.",
			symname.GetChars(), cls ? cls->TypeName.GetChars() : "Global");
		FScriptPosition::ErrorCounter++;
		return;
	}

	if (sc.CheckToken('['))
	{
		FxExpression *expr = ParseExpression(sc, cls, ns);
		if (expr == nullptr)
		{
			sc.ScriptMessage("Error while resolving array size");
			FScriptPosition::ErrorCounter++;
			maxelems = 1;
		}
		else if (!expr->isConstant())
		{
			sc.ScriptMessage("Array size must be a constant");
			FScriptPosition::ErrorCounter++;
			maxelems = 1;
		}
		else
		{
			maxelems = static_cast<FxConstant *>(expr)->GetValue().GetInt();
		}
		sc.MustGetToken(']');
		if (maxelems <= 0)
		{
			sc.ScriptMessage("Array size must be positive");
			FScriptPosition::ErrorCounter++;
			maxelems = 1;
		}
		type = NewArray(type, maxelems);
	}
	sc.MustGetToken(';');

	PField *sym = cls->AddField(symname, type, 0);
	if (sym == NULL)
	{
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
	HandleActorFlag(sc, bag, part1.GetChars(), part2, mod);
}

//==========================================================================
//
// Processes a flag. Also used by olddecorations.cpp
//
//==========================================================================
void HandleActorFlag(FScanner &sc, Baggage &bag, const char *part1, const char *part2, int mod)
{
	FFlagDef *fd;

	if ( (fd = FindFlag (bag.Info, part1, part2)) )
	{
		AActor *defaults = (AActor*)bag.Info->Defaults;
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
		{ "MRF_TRANSFERTRANSLATION", MORPH_TRANSFERTRANSLATION },
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
	int v = 0;

	if (sc.GetString() && !sc.Crossed)
	{
		if (sc.Compare("0")) return NULL;
		else if (sc.Compare("PARENT"))
		{
			FState *state = NULL;
			sc.MustGetString();

			PClassActor *info = ValidateActor(type->ParentClass);

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

			if (state == NULL && v==0)
			{
				return NULL;
			}
			if (v != 0 && state==NULL)
			{
				sc.ScriptMessage("Attempt to get invalid state from actor %s\n", type->ParentClass->TypeName.GetChars());
				FScriptPosition::ErrorCounter++;
				return NULL;
			}
			state += v;
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
			bag.ScriptPosition = sc;
			switch ((*p) & 223)
			{
			case 'X':	// Expression in parentheses or number.
				{
					FxExpression *x = NULL;

					if (sc.CheckString ("("))
					{
						conv.i = -1;
						params.Push(conv);
						x = ParseExpression(sc, bag.Info);
						sc.MustGetStringName(")");
						conv.exp = x;
						params.Push(conv);

					}
					else
					{
						sc.MustGetNumber();
						conv.i = sc.Number;
						params.Push(conv);
						conv.exp = nullptr;
					}
				}
				break;

			case 'I':
				sc.MustGetNumber();
				conv.i = sc.Number;
				break;

			case 'F':
				sc.MustGetFloat();
				conv.d = sc.Float;
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
				conv.s = (strings[strings.Reserve(1)] = sc.String).GetChars();
				break;

			case 'T':
				sc.MustGetString();
				conv.s = (strings[strings.Reserve(1)] = strbin1(sc.String)).GetChars();
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
					conv.s = (strings[strings.Reserve(1)] = sc.String).GetChars();
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
						conv.s = (strings[strings.Reserve(1)] = sc.String).GetChars();
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
// Parses an actor property's parameters and calls the handler
//
//==========================================================================

static void DispatchScriptProperty(FScanner &sc, PProperty *prop, AActor *defaults, Baggage &bag)
{
	for (unsigned i=0; i<prop->Variables.Size();i++)
	{
		auto f = prop->Variables[i];
		void *addr;

		if (i > 0) sc.MustGetStringName(",");
		if (f->Flags & VARF_Meta)
		{
			addr = ((char*)bag.Info->Meta) + f->Offset;
		}
		else
		{
			addr = ((char*)defaults) + f->Offset;
		}

		if (f->Type == TypeBool)
		{
			bool val = sc.CheckNumber() ? !!sc.Number : true;
			static_cast<PBool*>(f->Type)->SetValue(addr, !!val);
		}
		else if (f->Type == TypeName)
		{
			sc.MustGetString();
			*(FName*)addr = sc.String;
		}
		else if (f->Type == TypeSound)
		{
			sc.MustGetString();
			*(FSoundID*)addr = S_FindSound(sc.String);
		}
		else if (f->Type == TypeColor)
		{
			if (sc.CheckNumber()) *(int*)addr = sc.Number;
			else *(PalEntry*)addr = V_GetColor(sc);
		}
		else if (f->Type->isIntCompatible())
		{
			sc.MustGetNumber();
			static_cast<PInt*>(f->Type)->SetValue(addr, sc.Number);
		}
		else if (f->Type->isFloat())
		{
			sc.MustGetFloat();
			static_cast<PFloat*>(f->Type)->SetValue(addr, sc.Float);
		}
		else if (f->Type == TypeString)
		{
			sc.MustGetString();
			*(FString*)addr = strbin1(sc.String);
		}
		else if (f->Type->isClassPointer())
		{
			sc.MustGetString();

			if (*sc.String == 0 || !stricmp(sc.String, "none"))
			{
				*(PClass**)addr = nullptr;
			}
			else
			{
				auto cls = PClass::FindClass(sc.String);
				auto cp = static_cast<PClassPointer*>(f->Type);
				if (cls == nullptr)
				{
					cls = cp->ClassRestriction->FindClassTentative(sc.String);
				}
				else if (!cls->IsDescendantOf(cp->ClassRestriction))
				{
					sc.ScriptMessage("class %s is not compatible with property type %s", sc.String, cp->ClassRestriction->TypeName.GetChars());
					FScriptPosition::ErrorCounter++;
				}
				*(PClass**)addr = cls;
			}
		}
		else
		{
			sc.ScriptMessage("unhandled property type %s", f->Type->DescriptiveName());
			FScriptPosition::ErrorCounter++;
		}
	}
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

	FPropertyInfo *prop = FindProperty(propname.GetChars());

	if (prop != NULL)
	{
		auto pcls = PClass::FindActor(prop->clsname);
		if (bag.Info->IsDescendantOf(pcls))
		{
			ParsePropertyParams(sc, prop, (AActor *)bag.Info->Defaults, bag);
		}
		else
		{
			sc.ScriptMessage("'%s' requires an actor of type '%s'\n", propname.GetChars(), pcls->TypeName.GetChars());
			FScriptPosition::ErrorCounter++;
		}
	}
	else if (MatchString(propname.GetChars(), statenames) != -1)
	{
		bag.statedef.SetStateLabel(propname.GetChars(), CheckState (sc, bag.Info));
	}
	else
	{
		propname.Insert(0, "@property@");
		FName name(propname, true);
		if (name != NAME_None)
		{
			auto propp = dyn_cast<PProperty>(bag.Info->FindSymbol(name, true));
			if (propp != nullptr)
			{
				DispatchScriptProperty(sc, propp, (AActor *)bag.Info->Defaults, bag);
				return;
			}
		}
		sc.ScriptError("'%s' is an unknown actor property\n", propname.GetChars());
	}
}

//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
PClassActor *CreateNewActor(const FScriptPosition &sc, FName typeName, FName parentName)
{
	PClassActor *replacee = NULL;
	PClassActor *ti = NULL;

	PClassActor *parent = RUNTIME_CLASS(AActor);

	if (parentName != NAME_None)
	{
		parent = PClass::FindActor(parentName);

		PClassActor *p = parent;
		while (p != NULL)
		{
			if (p->TypeName == typeName)
			{
				sc.Message(MSG_ERROR, "'%s' inherits from a class with the same name", typeName.GetChars());
				break;
			}
			p = ValidateActor(p->ParentClass);
		}

		if (parent == NULL)
		{
			sc.Message(MSG_ERROR, "Parent type '%s' not found in %s", parentName.GetChars(), typeName.GetChars());
			parent = RUNTIME_CLASS(AActor);
		}
		else if (!parent->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			sc.Message(MSG_ERROR, "Parent type '%s' is not an actor in %s", parentName.GetChars(), typeName.GetChars());
			parent = RUNTIME_CLASS(AActor);
		}
	}
	ti = DecoDerivedClass(sc, parent, typeName);
	ti->bDecorateClass = true;	// we only set this for 'modern' DECORATE. The original stuff  is so limited that it cannot do anything that may require flagging.

	ti->ActorInfo()->DoomEdNum = -1;
	return ti;
}

//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
static PClassActor *ParseActorHeader(FScanner &sc, Baggage *bag)
{
	FName replaceName = NAME_None;
	bool native = false;
	int DoomEdNum = -1;

	// Get actor name
	sc.MustGetString();
	
	char *colon = strchr(sc.String, ':');
	if (colon != NULL)
	{
		*colon++ = 0;
	}

	FName typeName = sc.String;

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

	FName parentName = colon;

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
		sc.ScriptMessage("Cannot define native classes in DECORATE");
		FScriptPosition::ErrorCounter++;
	}

	try
	{
		PClassActor *info = CreateNewActor(sc, typeName, parentName);
		info->ActorInfo()->DoomEdNum = DoomEdNum > 0 ? DoomEdNum : -1;
		info->SourceLumpName = fileSystem.GetFileFullPath(sc.LumpNum).c_str();

		if (!info->SetReplacement(replaceName))
		{
			sc.ScriptMessage("Replaced type '%s' not found for %s", replaceName.GetChars(), info->TypeName.GetChars());
		}

		ResetBaggage (bag, ValidateActor(info->ParentClass));
		bag->Info = info;
		bag->Lumpnum = sc.LumpNum;
#ifdef _DEBUG
		bag->ClassName = typeName.GetChars();
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
static void ParseActor(FScanner &sc, PNamespace *ns)
{
	PClassActor *info = NULL;
	Baggage bag;

	bag.Namespace = ns;
	bag.Version = { 2, 0, 0 };	
	bag.fromDecorate = true;
	bag.ScriptPosition = sc;
	info = ParseActorHeader(sc, &bag);
	sc.MustGetToken('{');
	while (sc.MustGetAnyToken(), sc.TokenType != '}')
	{
		switch (sc.TokenType)
		{
		case TK_Const:
			ParseConstant (sc, &info->VMType->Symbols, info, ns);
			break;

		case TK_Enum:
			ParseEnum (sc, &info->VMType->Symbols, info, ns);
			break;

		case TK_Var:
			ParseUserVariable (sc, &info->VMType->Symbols, info, ns);
			break;

		case TK_Identifier:
			ParseActorProperty(sc, bag);
			break;

		case TK_States:
			ParseStates(sc, bag.Info, (AActor *)bag.Info->Defaults, bag);
			bag.StateSet = true;
			break;

		case '+':
		case '-':
			ParseActorFlag(sc, bag, sc.TokenType);
			break;

		default:
			sc.ScriptError("Unexpected '%s' in definition of '%s'", sc.String, bag.Info->TypeName.GetChars());
			break;
		}
	}
	if (bag.DropItemSet)
	{
		bag.Info->SetDropItems(bag.DropItemList);
	}
	try
	{
		FinalizeClass(info, bag.statedef);
	}
	catch (CRecoverableError &err)
	{
		sc.ScriptError("%s", err.GetMessage());
	}
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
			dtd.DefaultFactor = sc.Float;
			if (dtd.DefaultFactor == 0) dtd.ReplaceFactor = true;
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

void ParseDecorate (FScanner &sc, PNamespace *ns)
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
			if (fileSystem.GetFileContainer(sc.LumpNum) == 0 && !Args->CheckParm("-allowdecoratecrossincludes"))
			{
				int includefile = fileSystem.GetFileContainer(fileSystem.CheckNumForFullName(sc.String, true));
				if (includefile != 0)
				{
					I_FatalError("File %s is overriding core lump %s.",
						fileSystem.GetResourceFileFullName(includefile), sc.String);
				}
			}
			FScanner newscanner;
			newscanner.Open(sc.String);
			ParseDecorate(newscanner, ns);
			break;
		}

		case TK_Const:
			ParseConstant (sc, &ns->Symbols, NULL, ns);
			break;

		case TK_Enum:
			ParseEnum (sc, &ns->Symbols, NULL, ns);
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
				ParseActor (sc, ns);
				break;
			}
			else if (sc.Compare("PICKUP"))
			{
				ParseOldDecoration (sc, DEF_Pickup, ns);
				break;
			}
			else if (sc.Compare("BREAKABLE"))
			{
				ParseOldDecoration (sc, DEF_BreakableDecoration, ns);
				break;
			}
			else if (sc.Compare("PROJECTILE"))
			{
				ParseOldDecoration (sc, DEF_Projectile, ns);
				break;
			}
			else if (sc.Compare("DAMAGETYPE"))
			{
				ParseDamageDefinition(sc);
				break;
			}
			[[fallthrough]];
		default:
			sc.RestorePos(pos);
			ParseOldDecoration(sc, DEF_Decoration, ns);
			break;
		}
	}
}

void ParseAllDecorate()
{
	int lastlump = 0, lump;

	while ((lump = fileSystem.FindLump("DECORATE", &lastlump)) != -1)
	{
		FScanner sc(lump);
		auto ns = Namespaces.NewNamespace(sc.LumpNum);
		ParseDecorate(sc, ns);
	}
}
