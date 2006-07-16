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
#include "vectors.h"
#include "cmdlib.h"
#include "i_system.h"
#include "m_random.h"
#include "a_pickups.h"

void InitExpressions ();
void ClearExpressions ();

static FRandom pr_exrandom ("EX_Random");

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
	EX_Sin,			// sin (angle)
	EX_Cos,			// cos (angle)
	EX_InvCount,	// invcount (type)
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

static struct FExpVar
{
	const char *name;	// identifier
	int array;			// array size (0 if not an array)
	ExpVarGet get;
} ExpVars[] = {
	{ "alpha",		0, GetAlpha },
	{ "angle",		0, GetAngle },
	{ "args",		5, GetArgs },
	{ "ceilingz",	0, GetCeilingZ },
	{ "floorz",		0, GetFloorZ },
	{ "health",		0, GetHealth },
	{ "pitch",		0, GetPitch },
	{ "special",	0, GetSpecial },
	{ "tid",		0, GetTID },
	{ "tidtohate",	0, GetTIDToHate },
	{ "waterlevel",	0, GetWaterLevel },
	{ "x",			0, GetX },
	{ "y",			0, GetY },
	{ "z",			0, GetZ },
};

struct ExpData;
static ExpVal EvalExpression (ExpData *data, AActor *self);

struct ExpData
{
	ExpData ()
	{
		Type = EX_NOP;
		Value.Type = VAL_Int;
		Value.Int = 0;
		for (int i = 0; i < 3; i++)
			Children[i] = NULL;
	}
	~ExpData ()
	{
		for (int i = 0; i < 3; i++)
		{
			if (Children[i])
			{
				if (Type == EX_InvCount)
					free (Children[i]);
				else
					delete Children[i];
			}
		}
	}
	// Try to evaluate constant expression
	void EvalConst ()
	{
		if (Type == EX_NOP || Type == EX_Const || Type == EX_Var)
		{
			return;
		}
		else if (Type == EX_Compl || Type == EX_Not || /*Type == EX_Plus || */Type == EX_Minus)
		{
			if (Children[0]->Type == EX_Const)
			{
				Value = EvalExpression (this, NULL);
				Type = EX_Const;
			}
		}
		else if (Type == EX_Cond)
		{
			if (Children[0]->Type == EX_Const)
			{
				bool cond = (Children[0]->Value.Type == VAL_Int) ? (Children[0]->Value.Int != 0) : (Children[0]->Value.Float != 0);
				ExpData *data = Children[cond ? 1 : 2];
				delete Children[cond ? 2 : 1];

				Type = data->Type;
				Value = data->Value;
				for (int i = 0; i < 3; i++)
				{
					Children[i] = data->Children[i];
					data->Children[i] = NULL;
				}

				delete data;
			}
		}
		else if (Type != EX_Random && Type != EX_Sin && Type != EX_Cos && Type != EX_InvCount)
		{
			if (Children[0]->Type == EX_Const && Children[1]->Type == EX_Const)
			{
				Value = EvalExpression (this, NULL);
				Type = EX_Const;
				delete Children[0];
				delete Children[1];
			}
		}
	}
	bool Compare (ExpData *other)
	{
		if (!other)
			return false;

		if (Type != other->Type ||
			Value.Type != other->Value.Type ||
			Value.Float != other->Value.Float)
		{
			return false;
		}

		for (int i = 0; i < 3; i++)
		{
			if (Children[i] && !Children[i]->Compare (other->Children[i]))
				return false;
		}

		return true;
	}

	ExpOp Type;
	ExpVal Value;
	ExpData *Children[3];
};

TArray<ExpData *> StateExpressions;

//
// ParseExpression
// [GRB] Parses an expression and stores it into Expression array
//

static ExpData *ParseExpressionM ();
static ExpData *ParseExpressionL ();
static ExpData *ParseExpressionK ();
static ExpData *ParseExpressionJ ();
static ExpData *ParseExpressionI ();
static ExpData *ParseExpressionH ();
static ExpData *ParseExpressionG ();
static ExpData *ParseExpressionF ();
static ExpData *ParseExpressionE ();
static ExpData *ParseExpressionD ();
static ExpData *ParseExpressionC ();
static ExpData *ParseExpressionB ();
static ExpData *ParseExpressionA ();

int ParseExpression (bool _not)
{
	static bool inited=false;
	
	if (!inited)
	{
		InitExpressions ();
		atterm (ClearExpressions);
		inited=true;
	}

	ExpData *data = ParseExpressionM ();

	if (_not)
	{
		ExpData *tmp = data;
		data = new ExpData;
		data->Type = EX_Not;
		data->Children[0] = tmp;
		data->EvalConst ();
	}

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

static ExpData *ParseExpressionM ()
{
	ExpData *tmp = ParseExpressionL ();

	if (SC_CheckString ("?"))
	{
		ExpData *data = new ExpData;
		data->Type = EX_Cond;
		data->Children[0] = tmp;
		data->Children[1] = ParseExpressionM ();
		if (!SC_CheckString (":"))
			SC_ScriptError ("':' expected");
		data->Children[2] = ParseExpressionM ();
		data->EvalConst ();
		return data;
	}
	else
	{
		return tmp;
	}
}

static ExpData *ParseExpressionL ()
{
	ExpData *tmp = ParseExpressionK ();

	if (SC_CheckString ("||"))
	{
		ExpData *data = new ExpData;
		data->Type = EX_LogOr;
		data->Children[0] = tmp;
		data->Children[1] = ParseExpressionL ();
		data->EvalConst ();
		return data;
	}
	else
	{
		return tmp;
	}
}

static ExpData *ParseExpressionK ()
{
	ExpData *tmp = ParseExpressionJ ();

	if (SC_CheckString ("&&"))
	{
		ExpData *data = new ExpData;
		data->Type = EX_LogAnd;
		data->Children[0] = tmp;
		data->Children[1] = ParseExpressionK ();
		data->EvalConst ();
		return data;
	}
	else
	{
		return tmp;
	}
}

static ExpData *ParseExpressionJ ()
{
	ExpData *tmp = ParseExpressionI ();

	if (SC_CheckString ("|"))
	{
		ExpData *data = new ExpData;
		data->Type = EX_Or;
		data->Children[0] = tmp;
		data->Children[1] = ParseExpressionJ ();
		data->EvalConst ();
		return data;
	}
	else
	{
		return tmp;
	}
}

static ExpData *ParseExpressionI ()
{
	ExpData *tmp = ParseExpressionH ();

	if (SC_CheckString ("^"))
	{
		ExpData *data = new ExpData;
		data->Type = EX_Xor;
		data->Children[0] = tmp;
		data->Children[1] = ParseExpressionI ();
		data->EvalConst ();
		return data;
	}
	else
	{
		return tmp;
	}
}

static ExpData *ParseExpressionH ()
{
	ExpData *tmp = ParseExpressionG ();

	if (SC_CheckString ("&"))
	{
		ExpData *data = new ExpData;
		data->Type = EX_And;
		data->Children[0] = tmp;
		data->Children[1] = ParseExpressionH ();
		data->EvalConst ();
		return data;
	}
	else
	{
		return tmp;
	}
}

static ExpData *ParseExpressionG ()
{
	ExpData *tmp = ParseExpressionF ();
	ExpData *data = new ExpData;

	if (SC_CheckString ("=="))
	{
		data->Type = EX_Eq;
	}
	else if (SC_CheckString ("=="))
	{
		data->Type = EX_NE;
	}
	else
	{
		delete data;
		return tmp;
	}

	data->Children[0] = tmp;
	data->Children[1] = ParseExpressionG ();
	data->EvalConst ();

	return data;
}

static ExpData *ParseExpressionF ()
{
	ExpData *tmp = ParseExpressionE ();
	ExpData *data = new ExpData;

	if (SC_CheckString ("<"))
	{
		data->Type = EX_LT;
	}
	else if (SC_CheckString (">"))
	{
		data->Type = EX_GT;
	}
	else if (SC_CheckString ("<="))
	{
		data->Type = EX_LE;
	}
	else if (SC_CheckString (">="))
	{
		data->Type = EX_GE;
	}
	else
	{
		delete data;
		return tmp;
	}

	data->Children[0] = tmp;
	data->Children[1] = ParseExpressionF ();
	data->EvalConst ();

	return data;
}

static ExpData *ParseExpressionE ()
{
	ExpData *tmp = ParseExpressionD ();
	ExpData *data = new ExpData;

	if (SC_CheckString ("<<"))
	{
		data->Type = EX_LShift;
	}
	else if (SC_CheckString (">>"))
	{
		data->Type = EX_RShift;
	}
	else
	{
		delete data;
		return tmp;
	}

	data->Children[0] = tmp;
	data->Children[1] = ParseExpressionE ();
	data->EvalConst ();

	return data;
}

static ExpData *ParseExpressionD ()
{
	ExpData *tmp = ParseExpressionC ();
	ExpData *data = new ExpData;

	if (SC_CheckString ("+"))
	{
		data->Type = EX_Add;
	}
	else if (SC_CheckString ("-"))
	{
		data->Type = EX_Sub;
	}
	else
	{
		delete data;
		return tmp;
	}

	data->Children[0] = tmp;
	data->Children[1] = ParseExpressionD ();
	data->EvalConst ();

	return data;
}

static ExpData *ParseExpressionC ()
{
	ExpData *tmp = ParseExpressionB ();
	ExpData *data = new ExpData;

	if (SC_CheckString ("*"))
	{
		data->Type = EX_Mul;
	}
	else if (SC_CheckString ("/"))
	{
		data->Type = EX_Div;
	}
	else if (SC_CheckString ("%"))
	{
		data->Type = EX_Mod;
	}
	else
	{
		delete data;
		return tmp;
	}

	data->Children[0] = tmp;
	data->Children[1] = ParseExpressionC ();
	data->EvalConst ();

	return data;
}

static ExpData *ParseExpressionB ()
{
	ExpData *data = new ExpData;

	if (SC_CheckString ("~"))
	{
		data->Type = EX_Compl;
	}
	else if (SC_CheckString ("!"))
	{
		data->Type = EX_Not;
	}
	else if (SC_CheckString ("-"))
	{
		data->Type = EX_Minus;
	}
	else
	{
		SC_CheckString ("+");
		delete data;
		return ParseExpressionA ();
	}

	data->Children[0] = ParseExpressionA ();
	data->EvalConst ();

	return data;
}

static ExpData *ParseExpressionA ()
{
	if (SC_CheckString ("("))
	{
		ExpData *data = ParseExpressionM ();

		if (!SC_CheckString (")"))
			SC_ScriptError ("')' expected");

		return data;
	}
	else if (SC_CheckString ("random"))
	{
		if (!SC_CheckString ("("))
			SC_ScriptError ("'(' expected");

		ExpData *data = new ExpData;
		data->Type = EX_Random;

		data->Children[0] = ParseExpressionM ();

		if (!SC_CheckString (","))
			SC_ScriptError ("',' expected");

		data->Children[1] = ParseExpressionM ();

		if (!SC_CheckString (")"))
			SC_ScriptError ("')' expected");

		return data;
	}
	else if (SC_CheckString ("sin"))
	{
		if (!SC_CheckString ("("))
			SC_ScriptError ("'(' expected");

		ExpData *data = new ExpData;
		data->Type = EX_Sin;

		data->Children[0] = ParseExpressionM ();

		if (!SC_CheckString (")"))
			SC_ScriptError ("')' expected");

		return data;
	}
	else if (SC_CheckString ("cos"))
	{
		if (!SC_CheckString ("("))
			SC_ScriptError ("'(' expected");

		ExpData *data = new ExpData;
		data->Type = EX_Cos;

		data->Children[0] = ParseExpressionM ();

		if (!SC_CheckString (")"))
			SC_ScriptError ("')' expected");

		return data;
	}
	else if (SC_CheckString ("invcount"))
	{
		if (!SC_CheckString ("("))
			SC_ScriptError ("'(' expected");

		ExpData *data = new ExpData;
		data->Type = EX_InvCount;

		SC_MustGetString ();
		data->Children[0] = (ExpData *)strdup (sc_String);

		if (!SC_CheckString (")"))
			SC_ScriptError ("')' expected");

		return data;
	}
	else if (SC_CheckNumber ())
	{
		ExpData *data = new ExpData;
		data->Type = EX_Const;
		data->Value.Type = VAL_Int;
		data->Value.Int = sc_Number;

		return data;
	}
	else if (SC_CheckFloat ())
	{
		ExpData *data = new ExpData;
		data->Type = EX_Const;
		data->Value.Type = VAL_Float;
		data->Value.Float = sc_Float;

		return data;
	}
	else
	{
		SC_MustGetString ();

		int varid = -1;
		for (size_t i = 0; i < countof(ExpVars); i++)
		{
			if (!stricmp (sc_String, ExpVars[i].name))
			{
				varid = i;
				break;
			}
		}

		if (varid == -1)
			SC_ScriptError ("Value expected");

		ExpData *data = new ExpData;
		data->Type = EX_Var;
		data->Value.Type = VAL_Int;
		data->Value.Int = varid;

		if (ExpVars[varid].array)
		{
			if (!SC_CheckString ("["))
				SC_ScriptError ("'[' expected");

			data->Children[0] = ParseExpressionM ();

			if (!SC_CheckString ("]"))
				SC_ScriptError ("']' expected");
		}

		return data;
	}
}

//
// EvalExpression
// [GRB] Evaluates previously stored expression
//

int EvalExpressionI (int id, AActor *self)
{
	if (StateExpressions.Size() <= (unsigned int)id) return 0;

	ExpVal val = EvalExpression (StateExpressions[id], self);

	switch (val.Type)
	{
	default:
	case VAL_Int:
		return val.Int;
	case VAL_Float:
		return (int)val.Float;
	}
}

bool EvalExpressionN(int id, AActor * self)
{
	return !EvalExpressionI(id, self);
}

float EvalExpressionF (int id, AActor *self)
{
	if (StateExpressions.Size() <= (unsigned int)id) return 0.f;

	ExpVal val = EvalExpression (StateExpressions[id], self);

	switch (val.Type)
	{
	default:
	case VAL_Int:
		return (float)val.Int;
	case VAL_Float:
		return val.Float;
	}
}

static ExpVal EvalExpression (ExpData *data, AActor *self)
{
	ExpVal val;
	
	switch (data->Type)
	{
	case EX_NOP:
		break;
	case EX_Const:
		val = data->Value;
		break;
	case EX_Var:
		if (!self)
		{
			// Should not happen
			I_FatalError ("Missing actor data");
		}
		else
		{
			int id = 0;
			if (ExpVars[data->Value.Int].array)
			{
				ExpVal idval = EvalExpression (data->Children[0], self);
				id = ((idval.Type == VAL_Int) ? idval.Int : (int)idval.Float) % ExpVars[data->Value.Int].array;
			}

			val = ExpVars[data->Value.Int].get (self, id);
		}
		break;
	case EX_Compl:
		{
			ExpVal a = EvalExpression (data->Children[0], self);

			val.Type = VAL_Int;
			val.Int = ~((a.Type == VAL_Int) ? a.Int : (int)a.Float);
		}
		break;
	case EX_Not:
		{
			ExpVal a = EvalExpression (data->Children[0], self);

			val.Type = VAL_Int;
			val.Int = !((a.Type == VAL_Int) ? a.Int : (int)a.Float);
		}
		break;
	case EX_Minus:
		{
			val = EvalExpression (data->Children[0], self);

			if (val.Type == VAL_Int)
				val.Int = -val.Int;
			else
				val.Float = -val.Float;
		}
		break;
	case EX_Mul:
		{
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

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
			ExpVal a = EvalExpression (data->Children[0], self);

			if (a.Type == VAL_Int)
				val = a.Int ? EvalExpression (data->Children[1], self) : EvalExpression (data->Children[2], self);
			else
				val = a.Float ? EvalExpression (data->Children[1], self) : EvalExpression (data->Children[2], self);
		}
		break;

	case EX_Random:
		{
			ExpVal a = EvalExpression (data->Children[0], self);
			ExpVal b = EvalExpression (data->Children[1], self);

			int min = (a.Type == VAL_Int) ? a.Int : (int)a.Float;
			int max = (b.Type == VAL_Int) ? b.Int : (int)b.Float;

			val.Type = VAL_Int;

			if (max < min)
			{
				swap (max, min);
			}

			if (max - min > 255)
			{
				int num1,num2,num3,num4;

				num1 = pr_exrandom();
				num2 = pr_exrandom();
				num3 = pr_exrandom();
				num4 = pr_exrandom();

				val.Int = ((num1 << 24) | (num2 << 16) | (num3 << 8) | num4);
			}
			else
			{
				val.Int = pr_exrandom();
			}
			val.Int %= (max - min + 1);
			val.Int += min;
		}
		break;

	case EX_Sin:
		{
			ExpVal a = EvalExpression (data->Children[0], self);
			angle_t angle = (a.Type == VAL_Int) ? (a.Int * ANGLE_1) : angle_t(a.Float * ANGLE_1);

			val.Type = VAL_Float;
			val.Float = FIXED2FLOAT (finesine[angle>>ANGLETOFINESHIFT]);
		}
		break;

	case EX_Cos:
		{
			ExpVal a = EvalExpression (data->Children[0], self);
			angle_t angle = (a.Type == VAL_Int) ? (a.Int * ANGLE_1) : angle_t(a.Float * ANGLE_1);

			val.Type = VAL_Float;
			val.Float = FIXED2FLOAT (finecosine[angle>>ANGLETOFINESHIFT]);
		}
		break;

	case EX_InvCount:
		{
			const char *name = (const char *)data->Children[0];
			const PClass *type = PClass::FindClass (name);

			val.Type = VAL_Int;
			val.Int = 0;

			if (!type)
				break;

			AInventory *item = self->FindInventory (type);

			if (item)
				val.Int = item->Amount;
		}
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
