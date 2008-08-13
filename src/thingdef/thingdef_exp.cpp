/*
** thingdef_exp.cpp
**
** Expression parsing / runtime evaluating support
**
**---------------------------------------------------------------------------
** Copyright 2005 Jan Cholasta
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

#include "actor.h"
#include "sc_man.h"
#include "tarray.h"
#include "templates.h"
#include "cmdlib.h"
#include "i_system.h"
#include "m_random.h"
#include "a_pickups.h"
#include "thingdef.h"
#include "p_lnspec.h"

void InitExpressions ();
void ClearExpressions ();

FRandom pr_exrandom ("EX_Random");

enum ExpOp
{
	EX_NOP,

	EX_Const,
	EX_Var,

	EX_Compl,		// ~exp
	EX_Not,			// !exp
//	EX_Plus,		// +exp
	EX_Minus,		// -exp
	EX_Mul,			// exp * exp
	EX_Div,			// exp / exp
	EX_Mod,			// exp % exp
	EX_Add,			// exp + exp
	EX_Sub,			// exp - exp
	EX_LShift,		// exp << exp
	EX_RShift,		// exp >> exp
	EX_LT,			// exp < exp
	EX_GT,			// exp > exp
	EX_LE,			// exp <= exp
	EX_GE,			// exp >= exp
	EX_Eq,			// exp == exp
	EX_NE,			// exp != exp
	EX_And,			// exp & exp
	EX_Xor,			// exp ^ exp
	EX_Or,			// exp | exp
	EX_LogAnd,		// exp && exp
	EX_LogOr,		// exp || exp
	EX_Cond,		// exp ? exp : exp

	EX_Random,		// random (min, max)
	EX_Random2,		// random2 ([mask])
	EX_Sin,			// sin (angle)
	EX_Cos,			// cos (angle)
	EX_ActionSpecial,
	EX_Right,
};

enum ExpValType
{
	VAL_Int,
	VAL_Float,
};

struct ExpVal
{
	ExpValType Type;
	union
	{
		int Int;
		float Float;
	};
};

typedef ExpVal (*ExpVarGet) (AActor *, int);

ExpVal GetAlpha (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = FIXED2FLOAT (actor->alpha);
	return val;
}

ExpVal GetAngle (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = (float)actor->angle / ANGLE_1;
	return val;
}

ExpVal GetArgs (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Int;
	val.Int = actor->args[id];
	return val;
}

ExpVal GetCeilingZ (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = FIXED2FLOAT (actor->ceilingz);
	return val;
}

ExpVal GetFloorZ (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = FIXED2FLOAT (actor->floorz);
	return val;
}

ExpVal GetHealth (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Int;
	val.Int = actor->health;
	return val;
}

ExpVal GetPitch (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = (float)actor->pitch / ANGLE_1;
	return val;
}

ExpVal GetSpecial (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Int;
	val.Int = actor->special;
	return val;
}

ExpVal GetTID (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Int;
	val.Int = actor->tid;
	return val;
}

ExpVal GetTIDToHate (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Int;
	val.Int = actor->TIDtoHate;
	return val;
}

ExpVal GetWaterLevel (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Int;
	val.Int = actor->waterlevel;
	return val;
}

ExpVal GetX (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = FIXED2FLOAT (actor->x);
	return val;
}

ExpVal GetY (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = FIXED2FLOAT (actor->y);
	return val;
}

ExpVal GetZ (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = FIXED2FLOAT (actor->z);
	return val;
}

ExpVal GetMomX (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = FIXED2FLOAT (actor->momx);
	return val;
}

ExpVal GetMomY (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = FIXED2FLOAT (actor->momy);
	return val;
}

ExpVal GetMomZ (AActor *actor, int id)
{
	ExpVal val;
	val.Type = VAL_Float;
	val.Float = FIXED2FLOAT (actor->momz);
	return val;
}

static struct FExpVar
{
	ENamedName name;	// identifier
	int array;			// array size (0 if not an array)
	ExpVarGet get;
} ExpVars[] = {
	{ NAME_Alpha,		0, GetAlpha },
	{ NAME_Angle,		0, GetAngle },
	{ NAME_Args,		5, GetArgs },
	{ NAME_CeilingZ,	0, GetCeilingZ },
	{ NAME_FloorZ,		0, GetFloorZ },
	{ NAME_Health,		0, GetHealth },
	{ NAME_Pitch,		0, GetPitch },
	{ NAME_Special,		0, GetSpecial },
	{ NAME_TID,			0, GetTID },
	{ NAME_TIDtoHate,	0, GetTIDToHate },
	{ NAME_WaterLevel,	0, GetWaterLevel },
	{ NAME_X,			0, GetX },
	{ NAME_Y,			0, GetY },
	{ NAME_Z,			0, GetZ },
	{ NAME_MomX,		0, GetMomX },
	{ NAME_MomY,		0, GetMomY },
	{ NAME_MomZ,		0, GetMomZ },
};

struct ExpData;
static ExpVal EvalExpression (ExpData *data, AActor *self, const PClass *cls);

struct ExpData
{
	ExpData ()
	{
		Type = EX_NOP;
		Value.Type = VAL_Int;
		Value.Int = 0;
		RNG = NULL;
		for (int i = 0; i < 2; i++)
			Children[i] = NULL;
	}
	~ExpData ()
	{
		for (int i = 0; i < 2; i++)
		{
			if (Children[i])
			{
				delete Children[i];
			}
		}
	}
	// Try to evaluate constant expression
	void EvalConst (const PClass *cls)
	{
		if (Type == EX_NOP || Type == EX_Const || Type == EX_Var)
		{
			return;
		}
		else if (Type == EX_Compl || Type == EX_Not || /*Type == EX_Plus || */Type == EX_Minus)
		{
			if (Children[0]->Type == EX_Const)
			{
				Value = EvalExpression (this, NULL, cls);
				Type = EX_Const;
			}
		}
		else if (Type == EX_Cond)
		{
			if (Children[0]->Type == EX_Const)
			{
				bool cond = (Children[0]->Value.Type == VAL_Int) ? (Children[0]->Value.Int != 0) : (Children[0]->Value.Float != 0);
				ExpData *data = Children[1]->Children[cond];
				delete Children[1]->Children[!cond];
				delete Children[1];
				delete Children[0];

				Type = data->Type;
				Value = data->Value;
				for (int i = 0; i < 2; i++)
				{
					Children[i] = data->Children[i];
					data->Children[i] = NULL;
				}

				delete data;
			}
		}
		else if (Type != EX_Random && Type != EX_Sin && Type != EX_Cos && Type != EX_ActionSpecial)
		{
			if (Children[0]->Type == EX_Const && Children[1]->Type == EX_Const)
			{
				Value = EvalExpression (this, NULL, NULL);
				Type = EX_Const;
				delete Children[0]; Children[0] = NULL;
				delete Children[1]; Children[1] = NULL;
			}
		}
	}
	bool Compare (ExpData *other)
	{
		if (!other)
			return false;

		if (Type != other->Type ||
			Value.Type != other->Value.Type ||
			Value.Float != other->Value.Float ||
			RNG != other->RNG)
		{
			return false;
		}

		for (int i = 0; i < 2; i++)
		{
			if (Children[i] && !Children[i]->Compare (other->Children[i]))
				return false;
		}

		return true;
	}

	ExpOp Type;
	ExpVal Value;
	ExpData *Children[2];
	FRandom *RNG;	// only used by random and random2
};

TArray<ExpData *> StateExpressions;

//
// ParseExpression
// [GRB] Parses an expression and stores it into Expression array
//

static ExpData *ParseExpressionM (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionL (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionK (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionJ (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionI (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionH (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionG (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionF (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionE (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionD (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionC (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionB (FScanner &sc, const PClass *cls);
static ExpData *ParseExpressionA (FScanner &sc, const PClass *cls);

int ParseExpression (FScanner &sc, bool _not, PClass *cls)
{
	static bool inited=false;
	
	if (!inited)
	{
		InitExpressions ();
		atterm (ClearExpressions);
		inited=true;
	}

	ExpData *data = ParseExpressionM (sc, cls);

	for (unsigned int i = 0; i < StateExpressions.Size (); i++)
	{
		if (StateExpressions[i]->Compare (data))
		{
			delete data;
			return i;
		}
	}

	return StateExpressions.Push (data);
}

static ExpData *ParseExpressionM (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionL (sc, cls);

	if (sc.CheckToken('?'))
	{
		ExpData *data = new ExpData;
		data->Type = EX_Cond;
		data->Children[0] = tmp;
		ExpData *choices = new ExpData;
		data->Children[1] = choices;
		choices->Type = EX_Right;
		choices->Children[0] = ParseExpressionM (sc, cls);
		sc.MustGetToken(':');
		choices->Children[1] = ParseExpressionM (sc, cls);
		data->EvalConst (cls);
		return data;
	}
	else
	{
		return tmp;
	}
}

static ExpData *ParseExpressionL (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionK (sc, cls);

	while (sc.CheckToken(TK_OrOr))
	{
		ExpData *right = ParseExpressionK (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = EX_LogOr;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	return tmp;
}

static ExpData *ParseExpressionK (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionJ (sc, cls);

	while (sc.CheckToken(TK_AndAnd))
	{
		ExpData *right = ParseExpressionJ (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = EX_LogAnd;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	return tmp;
}

static ExpData *ParseExpressionJ (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionI (sc, cls);

	while (sc.CheckToken('|'))
	{
		ExpData *right = ParseExpressionI (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = EX_Or;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	return tmp;
}

static ExpData *ParseExpressionI (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionH (sc, cls);

	while (sc.CheckToken('^'))
	{
		ExpData *right = ParseExpressionH (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = EX_Xor;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	return tmp;
}

static ExpData *ParseExpressionH (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionG (sc, cls);

	while (sc.CheckToken('&'))
	{
		ExpData *right = ParseExpressionG (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = EX_And;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	return tmp;
}

static ExpData *ParseExpressionG (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionF (sc, cls);

	while (sc.GetToken() && (sc.TokenType == TK_Eq || sc.TokenType == TK_Neq))
	{
		int token = sc.TokenType;
		ExpData *right = ParseExpressionF (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = token == TK_Eq? EX_Eq : EX_NE;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static ExpData *ParseExpressionF (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionE (sc, cls);

	while (sc.GetToken() && (sc.TokenType == '<' || sc.TokenType == '>' || sc.TokenType == TK_Leq || sc.TokenType == TK_Geq))
	{
		int token = sc.TokenType;
		ExpData *right = ParseExpressionE (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = token == '<' ? EX_LT : token == '>' ? EX_GT : token == TK_Leq? EX_LE : EX_GE;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static ExpData *ParseExpressionE (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionD (sc, cls);

	while (sc.GetToken() && (sc.TokenType == TK_LShift || sc.TokenType == TK_RShift))
	{
		int token = sc.TokenType;
		ExpData *right = ParseExpressionD (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = token == TK_LShift? EX_LShift : EX_RShift;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static ExpData *ParseExpressionD (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionC (sc, cls);

	while (sc.GetToken() && (sc.TokenType == '+' || sc.TokenType == '-'))
	{
		int token = sc.TokenType;
		ExpData *right = ParseExpressionC (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = token == '+'? EX_Add : EX_Sub;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static ExpData *ParseExpressionC (FScanner &sc, const PClass *cls)
{
	ExpData *tmp = ParseExpressionB (sc, cls);

	while (sc.GetToken() && (sc.TokenType == '*' || sc.TokenType == '/' || sc.TokenType == '%'))
	{
		int token = sc.TokenType;
		ExpData *right = ParseExpressionB (sc, cls);
		ExpData *data =  new ExpData;
		data->Type = token == '*'? EX_Mul : token == '/'? EX_Div : EX_Mod;
		data->Children[0] = tmp;
		data->Children[1] = right;
		data->EvalConst (cls);
		tmp = data;
	}
	if (!sc.End) sc.UnGet();
	return tmp;
}

static ExpData *ParseExpressionB (FScanner &sc, const PClass *cls)
{
	ExpData *data = new ExpData;

	if (sc.CheckToken('~'))
	{
		data->Type = EX_Compl;
	}
	else if (sc.CheckToken('!'))
	{
		data->Type = EX_Not;
	}
	else if (sc.CheckToken('-'))
	{
		data->Type = EX_Minus;
	}
	else
	{
		sc.CheckToken('+');
		delete data;
		return ParseExpressionA (sc, cls);
	}

	data->Children[0] = ParseExpressionA (sc, cls);
	data->EvalConst (cls);

	return data;
}

static ExpData *ParseExpressionA (FScanner &sc, const PClass *cls)
{
	if (sc.CheckToken('('))
	{
		ExpData *data = ParseExpressionM (sc, cls);
		sc.MustGetToken(')');
		return data;
	}
	else if (sc.CheckToken(TK_True))
	{
		ExpData *data = new ExpData;
		data->Type = EX_Const;
		data->Value.Type = VAL_Int;
		data->Value.Int = 1;

		return data;
	}
	else if (sc.CheckToken(TK_False))
	{
		ExpData *data = new ExpData;
		data->Type = EX_Const;
		data->Value.Type = VAL_Int;
		data->Value.Int = 0;

		return data;
	}
	else if (sc.CheckToken(TK_IntConst))
	{
		ExpData *data = new ExpData;
		data->Type = EX_Const;
		data->Value.Type = VAL_Int;
		data->Value.Int = sc.Number;

		return data;
	}
	else if (sc.CheckToken(TK_FloatConst))
	{
		ExpData *data = new ExpData;
		data->Type = EX_Const;
		data->Value.Type = VAL_Float;
		data->Value.Float = sc.Float;

		return data;
	}
	else if (sc.CheckToken(TK_Class))
	{
		// Accept class'SomeClassName'.SomeConstant
		sc.MustGetToken(TK_NameConst);
		cls = PClass::FindClass (sc.Name);
		if (cls == NULL)
		{
			sc.ScriptError ("Unknown class '%s'", sc.String);
		}
		sc.MustGetToken('.');
		sc.MustGetToken(TK_Identifier);
		PSymbol *sym = cls->Symbols.FindSymbol (sc.String, true);
		if (sym != NULL && sym->SymbolType == SYM_Const)
		{
			ExpData *data = new ExpData;
			data->Type = EX_Const;
			data->Value.Type = VAL_Int;
			data->Value.Int = static_cast<PSymbolConst *>(sym)->Value;
			return data;
		}
		else
		{
			sc.ScriptError ("'%s' is not a constant value in class '%s'", sc.String, cls->TypeName.GetChars());
			return NULL;
		}
	}
	else if (sc.CheckToken(TK_Identifier))
	{
		switch (FName(sc.String))
		{
		case NAME_Random:
		{
			FRandom *rng;

			if (sc.CheckToken('['))
			{
				sc.MustGetToken(TK_Identifier);
				rng = FRandom::StaticFindRNG(sc.String);
				sc.MustGetToken(']');
			}
			else
			{
				rng = &pr_exrandom;
			}
			sc.MustGetToken('(');

			ExpData *data = new ExpData;
			data->Type = EX_Random;
			data->RNG = rng;

			data->Children[0] = ParseExpressionM (sc, cls);
			sc.MustGetToken(',');
			data->Children[1] = ParseExpressionM (sc, cls);
			sc.MustGetToken(')');
			return data;
		}
		break;

		case NAME_Random2:
		{
			FRandom *rng;

			if (sc.CheckToken('['))
			{
				sc.MustGetToken(TK_Identifier);
				rng = FRandom::StaticFindRNG(sc.String);
				sc.MustGetToken(']');
			}
			else
			{
				rng = &pr_exrandom;
			}

			sc.MustGetToken('(');

			ExpData *data = new ExpData;
			data->Type = EX_Random2;
			data->RNG = rng;

			if (!sc.CheckToken(')'))
			{
				data->Children[0] = ParseExpressionM(sc, cls);
				sc.MustGetToken(')');
			}
			return data;
		}
		break;

		case NAME_Sin:
		{
			sc.MustGetToken('(');

			ExpData *data = new ExpData;
			data->Type = EX_Sin;

			data->Children[0] = ParseExpressionM (sc, cls);

			sc.MustGetToken(')');
			return data;
		}
		break;

		case NAME_Cos:
		{
			sc.MustGetToken('(');

			ExpData *data = new ExpData;
			data->Type = EX_Cos;

			data->Children[0] = ParseExpressionM (sc, cls);

			sc.MustGetToken(')');
			return data;
		}
		break;

		default:
		{
			int specnum, min_args, max_args;

			// Check if this is an action special
			specnum = P_FindLineSpecial (sc.String, &min_args, &max_args);
			if (specnum != 0 && min_args >= 0)
			{
				int i;

				sc.MustGetToken('(');

				ExpData *data = new ExpData, **left;
				data->Type = EX_ActionSpecial;
				data->Value.Int = specnum;

				data->Children[0] = ParseExpressionM (sc, cls);
				left = &data->Children[1];

				for (i = 1; i < 5 && sc.CheckToken(','); ++i)
				{
					ExpData *right = new ExpData;
					right->Type = EX_Right;
					right->Children[0] = ParseExpressionM (sc, cls);
					*left = right;
					left = &right->Children[1];
				}
				*left = NULL;
				sc.MustGetToken(')');
				if (i < min_args)
					sc.ScriptError ("Not enough arguments to action special");
				if (i > max_args)
					sc.ScriptError ("Too many arguments to action special");

				return data;
			}

			// Check if this is a constant
			if (cls != NULL)
			{
				PSymbol *sym = cls->Symbols.FindSymbol (sc.String, true);
				if (sym != NULL && sym->SymbolType == SYM_Const)
				{
					ExpData *data = new ExpData;
					data->Type = EX_Const;
					data->Value.Type = VAL_Int;
					data->Value.Int = static_cast<PSymbolConst *>(sym)->Value;
					return data;
				}
			}

			// Check if it's a variable we understand
			int varid = -1;
			FName vname = sc.String;
			for (size_t i = 0; i < countof(ExpVars); i++)
			{
				if (vname == ExpVars[i].name)
				{
					varid = (int)i;
					break;
				}
			}

			if (varid == -1)
				sc.ScriptError ("Unknown value '%s'", sc.String);

			ExpData *data = new ExpData;
			data->Type = EX_Var;
			data->Value.Type = VAL_Int;
			data->Value.Int = varid;

			if (ExpVars[varid].array)
			{
				sc.MustGetToken('[');
				data->Children[0] = ParseExpressionM (sc, cls);
				sc.MustGetToken(']');
			}
			return data;
		}
		break;
		}
	}
	else
	{
		FString tokname = sc.TokenName(sc.TokenType, sc.String);
		sc.ScriptError ("Unexpected token %s", tokname.GetChars());
		return NULL;
	}
}

//
// EvalExpression
// [GRB] Evaluates previously stored expression
//

bool IsExpressionConst(int id)
{
	if (StateExpressions.Size() <= (unsigned int)id) return false;

	return StateExpressions[id]->Type == EX_Const;
}

int EvalExpressionI (int id, AActor *self, const PClass *cls)
{
	if (StateExpressions.Size() <= (unsigned int)id) return 0;

	if (cls == NULL && self != NULL)
	{
		cls = self->GetClass();
	}

	ExpVal val = EvalExpression (StateExpressions[id], self, cls);

	switch (val.Type)
	{
	default:
	case VAL_Int:
		return val.Int;
	case VAL_Float:
		return (int)val.Float;
	}
}

float EvalExpressionF (int id, AActor *self, const PClass *cls)
{
	if (StateExpressions.Size() <= (unsigned int)id) return 0.f;

	if (cls == NULL && self != NULL)
	{
		cls = self->GetClass();
	}

	ExpVal val = EvalExpression (StateExpressions[id], self, cls);

	switch (val.Type)
	{
	default:
	case VAL_Int:
		return (float)val.Int;
	case VAL_Float:
		return val.Float;
	}
}

fixed_t EvalExpressionFix (int id, AActor *self, const PClass *cls)
{
	if (StateExpressions.Size() <= (unsigned int)id) return 0;

	if (cls == NULL && self != NULL)
	{
		cls = self->GetClass();
	}

	ExpVal val = EvalExpression (StateExpressions[id], self, cls);

	switch (val.Type)
	{
	default:
	case VAL_Int:
		return val.Int << FRACBITS;
	case VAL_Float:
		return fixed_t(val.Float*FRACUNIT);
	}
}

static ExpVal EvalExpression (ExpData *data, AActor *self, const PClass *cls)
{
	ExpVal val;

	val.Type = VAL_Int;		// Placate GCC

	switch (data->Type)
	{
	case EX_NOP:
		assert (data->Type != EX_NOP);
		val = data->Value;
		break;
	case EX_Const:
		val = data->Value;
		break;
	case EX_Var:
		if (!self)
		{
			I_FatalError ("Missing actor data");
		}
		else
		{
			int id = 0;
			if (ExpVars[data->Value.Int].array)
			{
				ExpVal idval = EvalExpression (data->Children[0], self, cls);
				id = ((idval.Type == VAL_Int) ? idval.Int : (int)idval.Float) % ExpVars[data->Value.Int].array;
			}

			val = ExpVars[data->Value.Int].get (self, id);
		}
		break;
	case EX_Compl:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);

			val.Type = VAL_Int;
			val.Int = ~((a.Type == VAL_Int) ? a.Int : (int)a.Float);
		}
		break;
	case EX_Not:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);

			val.Type = VAL_Int;
			val.Int = !((a.Type == VAL_Int) ? a.Int : (int)a.Float);
		}
		break;
	case EX_Minus:
		{
			val = EvalExpression (data->Children[0], self, cls);

			if (val.Type == VAL_Int)
				val.Int = -val.Int;
			else
				val.Float = -val.Float;
		}
		break;
	case EX_Mul:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
				{
					val.Type = VAL_Int;
					val.Int = a.Int * b.Int;
				}
				else
				{
					val.Type = VAL_Float;
					val.Float = a.Int * b.Float;
				}
			}
			else
			{
				val.Type = VAL_Float;
				if (b.Type == VAL_Int)
					val.Float = a.Float * b.Int;
				else
					val.Float = a.Float * b.Float;
			}
		}
		break;
	case EX_Div:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			if (b.Int == 0)
			{
				I_FatalError ("Division by zero");
			}

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
				{
					val.Type = VAL_Int;
					val.Int = a.Int / b.Int;
				}
				else
				{
					val.Type = VAL_Float;
					val.Float = a.Int / b.Float;
				}
			}
			else
			{
				val.Type = VAL_Float;
				if (b.Type == VAL_Int)
					val.Float = a.Float / b.Int;
				else
					val.Float = a.Float / b.Float;
			}
		}
		break;
	case EX_Mod:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			if (b.Int == 0)
			{
				I_FatalError ("Division by zero");
			}

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
				{
					val.Type = VAL_Int;
					val.Int = a.Int % b.Int;
				}
				else
				{
					val.Type = VAL_Float;
					val.Float = fmodf (a.Int, b.Float);
				}
			}
			else
			{
				val.Type = VAL_Float;
				if (b.Type == VAL_Int)
					val.Float = fmodf (a.Float, b.Int);
				else
					val.Float = fmodf (a.Float, b.Float);
			}
		}
		break;
	case EX_Add:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
				{
					val.Type = VAL_Int;
					val.Int = a.Int + b.Int;
				}
				else
				{
					val.Type = VAL_Float;
					val.Float = a.Int + b.Float;
				}
			}
			else
			{
				val.Type = VAL_Float;
				if (b.Type == VAL_Int)
					val.Float = a.Float + b.Int;
				else
					val.Float = a.Float + b.Float;
			}
		}
		break;
	case EX_Sub:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
				{
					val.Type = VAL_Int;
					val.Int = a.Int - b.Int;
				}
				else
				{
					val.Type = VAL_Float;
					val.Float = a.Int - b.Float;
				}
			}
			else
			{
				val.Type = VAL_Float;
				if (b.Type == VAL_Int)
					val.Float = a.Float - b.Int;
				else
					val.Float = a.Float - b.Float;
			}
		}
		break;
	case EX_LShift:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int << b.Int;
				else
					val.Int = a.Int << (int)b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = (int)a.Float << b.Int;
				else
					val.Int = (int)a.Float << (int)b.Float;
			}
		}
		break;
	case EX_RShift:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int >> b.Int;
				else
					val.Int = a.Int >> (int)b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = (int)a.Float >> b.Int;
				else
					val.Int = (int)a.Float >> (int)b.Float;
			}
		}
		break;
	case EX_LT:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int < b.Int;
				else
					val.Int = a.Int < b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = a.Float < b.Int;
				else
					val.Int = a.Float < b.Float;
			}
		}
		break;
	case EX_GT:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int > b.Int;
				else
					val.Int = a.Int > b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = a.Float > b.Int;
				else
					val.Int = a.Float > b.Float;
			}
		}
		break;
	case EX_LE:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int <= b.Int;
				else
					val.Int = a.Int <= b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = a.Float <= b.Int;
				else
					val.Int = a.Float <= b.Float;
			}
		}
		break;
	case EX_GE:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int >= b.Int;
				else
					val.Int = a.Int >= b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = a.Float >= b.Int;
				else
					val.Int = a.Float >= b.Float;
			}
		}
		break;
	case EX_Eq:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int == b.Int;
				else
					val.Int = a.Int == b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = a.Float == b.Int;
				else
					val.Int = a.Float == b.Float;
			}
		}
		break;
	case EX_NE:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int != b.Int;
				else
					val.Int = a.Int != b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = a.Float != b.Int;
				else
					val.Int = a.Float != b.Float;
			}
		}
		break;
	case EX_And:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int & b.Int;
				else
					val.Int = a.Int & (int)b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = (int)a.Float & b.Int;
				else
					val.Int = (int)a.Float & (int)b.Float;
			}
		}
		break;
	case EX_Xor:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int ^ b.Int;
				else
					val.Int = a.Int ^ (int)b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = (int)a.Float ^ b.Int;
				else
					val.Int = (int)a.Float ^ (int)b.Float;
			}
		}
		break;
	case EX_Or:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int | b.Int;
				else
					val.Int = a.Int | (int)b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = (int)a.Float | b.Int;
				else
					val.Int = (int)a.Float | (int)b.Float;
			}
		}
		break;
	case EX_LogAnd:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int && b.Int;
				else
					val.Int = a.Int && b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = a.Float && b.Int;
				else
					val.Int = a.Float && b.Float;
			}
		}
		break;
	case EX_LogOr:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			val.Type = VAL_Int;

			if (a.Type == VAL_Int)
			{
				if (b.Type == VAL_Int)
					val.Int = a.Int || b.Int;
				else
					val.Int = a.Int || b.Float;
			}
			else
			{
				if (b.Type == VAL_Int)
					val.Int = a.Float || b.Int;
				else
					val.Int = a.Float || b.Float;
			}
		}
		break;
	case EX_Cond:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);

			if (a.Type == VAL_Float)
				a.Int = (int)a.Float;

			val = EvalExpression (data->Children[1]->Children[!!a.Int], self, cls);
		}
		break;

	case EX_Random:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			ExpVal b = EvalExpression (data->Children[1], self, cls);

			int min = (a.Type == VAL_Int) ? a.Int : (int)a.Float;
			int max = (b.Type == VAL_Int) ? b.Int : (int)b.Float;

			val.Type = VAL_Int;

			if (max < min)
			{
				swap (max, min);
			}

			val.Int = (*data->RNG)(max - min + 1) + min;
		}
		break;

	case EX_Random2:
		{
			if (data->Children[0] == NULL)
			{
				val.Type = VAL_Int;
				val.Int = data->RNG->Random2();
			}
			else
			{
				ExpVal a = EvalExpression (data->Children[0], self, cls);
				val.Type = VAL_Int;
				val.Int = data->RNG->Random2((a.Type == VAL_Int) ? a.Int : (int)a.Float);
			}
		}
		break;

	case EX_Sin:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			angle_t angle = (a.Type == VAL_Int) ? (a.Int * ANGLE_1) : angle_t(a.Float * ANGLE_1);

			val.Type = VAL_Float;
			val.Float = FIXED2FLOAT (finesine[angle>>ANGLETOFINESHIFT]);
		}
		break;

	case EX_Cos:
		{
			ExpVal a = EvalExpression (data->Children[0], self, cls);
			angle_t angle = (a.Type == VAL_Int) ? (a.Int * ANGLE_1) : angle_t(a.Float * ANGLE_1);

			val.Type = VAL_Float;
			val.Float = FIXED2FLOAT (finecosine[angle>>ANGLETOFINESHIFT]);
		}
		break;

	case EX_ActionSpecial:
		{
			int parms[5] = { 0, 0, 0, 0 };
			int i = 0;
			ExpData *parm = data;
			
			while (parm != NULL && i < 5)
			{
				ExpVal val = EvalExpression (parm->Children[0], self, cls);
				if (val.Type == VAL_Int)
				{
					parms[i] = val.Int;
				}
				else
				{
					parms[i] = (int)val.Float;
				}
				i++;
				parm = parm->Children[1];
			}

			val.Type = VAL_Int;
			val.Int = LineSpecials[data->Value.Int] (NULL, self, false,
				parms[0], parms[1], parms[2], parms[3], parms[4]);
		}
		break;

	case EX_Right:
		// This should never be a top-level expression.
		assert (data->Type != EX_Right);
		break;
	}

	return val;
}


//
// InitExpressions
// [GRB] Set up expression data
//

void InitExpressions ()
{
	// StateExpressions[0] always is const 0;
	ExpData *data = new ExpData;
	data->Type = EX_Const;
	data->Value.Type = VAL_Int;
	data->Value.Int = 0;

	StateExpressions.Push (data);
}


//
// ClearExpressions
// [GRB] Free all expression data
//

void ClearExpressions ()
{
	ExpData *data;

	while (StateExpressions.Pop (data))
		delete data;
}
