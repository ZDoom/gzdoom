/*
** thingdef_expression.cpp
**
** Expression evaluation
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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
#include "doomstat.h"
#include "thingdef_exp.h"
#include "autosegs.h"

int testglobalvar = 1337;	// just for having one global variable to test with
DEFINE_GLOBAL_VARIABLE(testglobalvar)

// Accessible actor member variables
DEFINE_MEMBER_VARIABLE(alpha, AActor)
DEFINE_MEMBER_VARIABLE(angle, AActor)
DEFINE_MEMBER_VARIABLE(args, AActor)
DEFINE_MEMBER_VARIABLE(ceilingz, AActor)
DEFINE_MEMBER_VARIABLE(floorz, AActor)
DEFINE_MEMBER_VARIABLE(health, AActor)
DEFINE_MEMBER_VARIABLE(pitch, AActor)
DEFINE_MEMBER_VARIABLE(special, AActor)
DEFINE_MEMBER_VARIABLE(tid, AActor)
DEFINE_MEMBER_VARIABLE(TIDtoHate, AActor)
DEFINE_MEMBER_VARIABLE(waterlevel, AActor)
DEFINE_MEMBER_VARIABLE(x, AActor)
DEFINE_MEMBER_VARIABLE(y, AActor)
DEFINE_MEMBER_VARIABLE(z, AActor)
DEFINE_MEMBER_VARIABLE(momx, AActor)
DEFINE_MEMBER_VARIABLE(momy, AActor)
DEFINE_MEMBER_VARIABLE(momz, AActor)

static TDeletingArray<FxExpression *> StateExpressions;

int AddExpression (FxExpression *data)
{
	if (StateExpressions.Size()==0)
	{
		// StateExpressions[0] always is const 0;
		FxExpression *data = new FxConstant(0, FScriptPosition());
		StateExpressions.Push (data);
	}
	return StateExpressions.Push (data);
}

//==========================================================================
//
// EvalExpression
// [GRB] Evaluates previously stored expression
//
//==========================================================================


bool IsExpressionConst(int id)
{
	if (StateExpressions.Size() <= (unsigned int)id) return false;

	return StateExpressions[id]->isConstant();
}

int EvalExpressionI (int id, AActor *self)
{
	if (StateExpressions.Size() <= (unsigned int)id) return 0;

	return StateExpressions[id]->EvalExpression (self).GetInt();
}

double EvalExpressionF (int id, AActor *self)
{
	if (StateExpressions.Size() <= (unsigned int)id) return 0.f;

	return StateExpressions[id]->EvalExpression (self).GetFloat();
}

fixed_t EvalExpressionFix (int id, AActor *self)
{
	if (StateExpressions.Size() <= (unsigned int)id) return 0;

	ExpVal val = StateExpressions[id]->EvalExpression (self);

	switch (val.Type)
	{
	default:
		return 0;
	case VAL_Int:
		return val.Int << FRACBITS;
	case VAL_Float:
		return fixed_t(val.Float*FRACUNIT);
	}
}

//==========================================================================
//
//
//
//==========================================================================

static ExpVal GetVariableValue (void *address, FExpressionType &type)
{
	// NOTE: This cannot access native variables of types
	// char, short and float. These need to be redefined if necessary!
	ExpVal ret;

	switch(type.Type)
	{
	case VAL_Int:
		ret.Type = VAL_Int;
		ret.Int = *(int*)address;
		break;

	case VAL_Bool:
		ret.Type = VAL_Int;
		ret.Int = *(bool*)address;
		break;

	case VAL_Float:
		ret.Type = VAL_Float;
		ret.Float = *(double*)address;
		break;

	case VAL_Fixed:
		ret.Type = VAL_Float;
		ret.Float = (*(fixed_t*)address) / 65536.;
		break;

	case VAL_Angle:
		ret.Type = VAL_Float;
		ret.Float = (*(angle_t*)address) * 90./ANGLE_90;	// intentionally not using ANGLE_1
		break;

	case VAL_Object:
	case VAL_Class:
		ret.Type = ExpValType(type.Type);	// object and class pointers don't retain their specific class information as values
		ret.pointer = *(void**)address;
		break;

	default:
		ret.Type = VAL_Unknown;
		ret.pointer = NULL;
		break;
	}
	return ret;
}

//==========================================================================
//
// FScriptPosition::Message
//
//==========================================================================

void STACK_ARGS FScriptPosition::Message (int severity, const char *message, ...) const
{
	FString composed;

	if ((severity == MSG_DEBUG || severity == MSG_DEBUGLOG) && !developer) return;

	if (message == NULL)
	{
		composed = "Bad syntax.";
	}
	else
	{
		va_list arglist;
		va_start (arglist, message);
		composed.VFormat (message, arglist);
		va_end (arglist);
	}
	const char *type = "";
	int level = PRINT_HIGH;

	switch (severity)
	{
	default:
		return;

	case MSG_WARNING:
		type = "warning";
		break;

	case MSG_ERROR:
		type = "error";
		break;

	case MSG_DEBUG:
		type = "message";
		break;

	case MSG_DEBUGLOG:
	case MSG_LOG:
		type = "message";
		level = PRINT_LOG;
		break;
	}

	Printf (level, "Script %s, \"%s\" line %d:\n%s\n", type,
		FileName.GetChars(), ScriptLine, composed.GetChars());
}


//==========================================================================
//
//
//
//==========================================================================

void FxExpression::RequestAddress()
{
	ScriptPosition.Message(MSG_ERROR, "invalid dereference\n");
}


//==========================================================================
//
//
//
//==========================================================================

ExpVal FxConstant::EvalExpression (AActor *self)
{
	return value;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxConstant::MakeConstant(PSymbol *sym, const FScriptPosition &pos)
{
	FxExpression *x;
	if (sym->SymbolType == SYM_Const)
	{
		PSymbolConst *csym = static_cast<PSymbolConst*>(sym);
		switch(csym->ValueType)
		{
		case VAL_Int:
			x = new FxConstant(csym->Value, pos);
			break;

		case VAL_Float:
			x = new FxConstant(csym->Float, pos);
			break;

		default:
			pos.Message(MSG_ERROR, "Invalid constant '%s'\n", csym->SymbolName.GetChars());
			return NULL;
		}
	}
	else
	{
		pos.Message(MSG_ERROR, "'%s' is not a constant\n", sym->SymbolName.GetChars());
		x = NULL;
	}
	return x;
}



//==========================================================================
//
//
//
//==========================================================================

FxIntCast::FxIntCast(FxExpression *x)
: FxExpression(x->ScriptPosition)
{
	basex=x;
	ValueType = VAL_Int;
}

//==========================================================================
//
//
//
//==========================================================================

FxIntCast::~FxIntCast()
{
	SAFE_DELETE(basex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxIntCast::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(basex, ctx);

	if (basex->ValueType == VAL_Int)
	{
		FxExpression *x = basex;
		basex = NULL;
		delete this;
		return x;
	}
	else if (basex->ValueType == VAL_Float)
	{
		if (basex->isConstant())
		{
			ExpVal constval = basex->EvalExpression(NULL);
			FxExpression *x = new FxConstant(constval.GetInt(), ScriptPosition);
			delete this;
			return x;
		}
		return this;
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxIntCast::EvalExpression (AActor *self)
{
	ExpVal baseval = basex->EvalExpression(self);
	baseval.Int = baseval.GetInt();
	baseval.Type = VAL_Int;
	return baseval;
}


//==========================================================================
//
//
//
//==========================================================================

FxPlusSign::FxPlusSign(FxExpression *operand)
: FxExpression(operand->ScriptPosition)
{
	Operand=operand;
}

//==========================================================================
//
//
//
//==========================================================================

FxPlusSign::~FxPlusSign()
{
	SAFE_DELETE(Operand);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxPlusSign::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Operand, ctx);

	if (Operand->ValueType.isNumeric())
	{
		FxExpression *e = Operand;
		Operand = NULL;
		delete this;
		return e;
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FxMinusSign::FxMinusSign(FxExpression *operand)
: FxExpression(operand->ScriptPosition)
{
	Operand=operand;
}

//==========================================================================
//
//
//
//==========================================================================

FxMinusSign::~FxMinusSign()
{
	SAFE_DELETE(Operand);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxMinusSign::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Operand, ctx);

	if (Operand->ValueType.isNumeric())
	{
		if (Operand->isConstant())
		{
			ExpVal val = Operand->EvalExpression(NULL);
			FxExpression *e = val.Type == VAL_Int?
				new FxConstant(-val.Int, ScriptPosition) :
				new FxConstant(-val.Float, ScriptPosition);
			delete this;
			return e;
		}
		ValueType = Operand->ValueType;
		return this;
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxMinusSign::EvalExpression (AActor *self)
{
	ExpVal ret;

	if (ValueType == VAL_Int)
	{
		ret.Int = -Operand->EvalExpression(self).GetInt();
		ret.Type = VAL_Int;
	}
	else
	{
		ret.Float = -Operand->EvalExpression(self).GetFloat();
		ret.Type = VAL_Float;
	}
	return ret;
}


//==========================================================================
//
//
//
//==========================================================================

FxUnaryNotBitwise::FxUnaryNotBitwise(FxExpression *operand)
: FxExpression(operand->ScriptPosition)
{
	Operand=operand;
}

//==========================================================================
//
//
//
//==========================================================================

FxUnaryNotBitwise::~FxUnaryNotBitwise()
{
	SAFE_DELETE(Operand);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxUnaryNotBitwise::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Operand, ctx);

	if  (Operand->ValueType == VAL_Float && ctx.lax)
	{
		// DECORATE allows floats here so cast them to int.
		Operand = new FxIntCast(Operand);
		Operand = Operand->Resolve(ctx);
		if (Operand == NULL) 
		{
			delete this;
			return NULL;
		}
	}

	if (Operand->ValueType != VAL_Int)
	{
		ScriptPosition.Message(MSG_ERROR, "Integer type expected");
		delete this;
		return NULL;
	}

	if (Operand->isConstant())
	{
		int result = ~Operand->EvalExpression(NULL).GetInt();
		FxExpression *e = new FxConstant(result, ScriptPosition);
		delete this;
		return e;
	}
	ValueType = VAL_Int;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxUnaryNotBitwise::EvalExpression (AActor *self)
{
	ExpVal ret;

	ret.Int = ~Operand->EvalExpression(self).GetInt();
	ret.Type = VAL_Int;
	return ret;
}

//==========================================================================
//
//
//
//==========================================================================

FxUnaryNotBoolean::FxUnaryNotBoolean(FxExpression *operand)
: FxExpression(operand->ScriptPosition)
{
	Operand=operand;
}

//==========================================================================
//
//
//
//==========================================================================

FxUnaryNotBoolean::~FxUnaryNotBoolean()
{
	SAFE_DELETE(Operand);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxUnaryNotBoolean::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	if (Operand)
	{
		Operand = Operand->ResolveAsBoolean(ctx);
	}
	if (!Operand)
	{
		delete this;
		return NULL;
	}

	if (Operand->ValueType.isNumeric() || Operand->ValueType.isPointer())
	{
		if (Operand->isConstant())
		{
			bool result = !Operand->EvalExpression(NULL).GetBool();
			FxExpression *e = new FxConstant(result, ScriptPosition);
			delete this;
			return e;
		}
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
	ValueType = VAL_Int;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxUnaryNotBoolean::EvalExpression (AActor *self)
{
	ExpVal ret;

	ret.Int = !Operand->EvalExpression(self).GetBool();
	ret.Type = VAL_Int;
	return ret;
}

//==========================================================================
//
//
//
//==========================================================================

FxBinary::FxBinary(int o, FxExpression *l, FxExpression *r)
: FxExpression(l->ScriptPosition)
{
	Operator=o;
	left=l;
	right=r;
}

//==========================================================================
//
//
//
//==========================================================================

FxBinary::~FxBinary()
{
	SAFE_DELETE(left);
	SAFE_DELETE(right);
}

//==========================================================================
//
//
//
//==========================================================================

bool FxBinary::ResolveLR(FCompileContext& ctx, bool castnumeric)
{
	RESOLVE(left, ctx);
	RESOLVE(right, ctx);
	if (!left || !right)
	{
		delete this;
		return false;
	}

	if (left->ValueType == VAL_Int && right->ValueType == VAL_Int)
	{
		ValueType = VAL_Int;
	}
	else if (left->ValueType.isNumeric() && right->ValueType.isNumeric())
	{
		ValueType = VAL_Float;
	}
	else if (left->ValueType == VAL_Object && right->ValueType == VAL_Object)
	{
		ValueType = VAL_Object;
	}
	else if (left->ValueType == VAL_Class && right->ValueType == VAL_Class)
	{
		ValueType = VAL_Class;
	}
	else
	{
		ValueType = VAL_Unknown;
	}

	if (castnumeric)
	{
		// later!
	}
	return true;
}


//==========================================================================
//
//
//
//==========================================================================

FxAddSub::FxAddSub(int o, FxExpression *l, FxExpression *r)
: FxBinary(o, l, r)
{
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxAddSub::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	if (!ResolveLR(ctx, true)) return NULL;

	if (!ValueType.isNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
	else if (left->isConstant() && right->isConstant())
	{
		if (ValueType == VAL_Float)
		{
			double v;
			double v1 = left->EvalExpression(NULL).GetFloat();
			double v2 = right->EvalExpression(NULL).GetFloat();

			v =	Operator == '+'? v1 + v2 : 
				Operator == '-'? v1 - v2 : 0;

			FxExpression *e = new FxConstant(v, ScriptPosition);
			delete this;
			return e;
		}
		else
		{
			int v;
			int v1 = left->EvalExpression(NULL).GetInt();
			int v2 = right->EvalExpression(NULL).GetInt();

			v =	Operator == '+'? v1 + v2 : 
				Operator == '-'? v1 - v2 : 0;

			FxExpression *e = new FxConstant(v, ScriptPosition);
			delete this;
			return e;

		}
	}
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxAddSub::EvalExpression (AActor *self)
{
	ExpVal ret;

	if (ValueType == VAL_Float)
	{
		double v1 = left->EvalExpression(self).GetFloat();
		double v2 = right->EvalExpression(self).GetFloat();

		ret.Type = VAL_Float;
		ret.Float = Operator == '+'? v1 + v2 : 
			  		Operator == '-'? v1 - v2 : 0;
	}
	else
	{
		int v1 = left->EvalExpression(self).GetInt();
		int v2 = right->EvalExpression(self).GetInt();

		ret.Type = VAL_Int;
		ret.Int = Operator == '+'? v1 + v2 : 
				  Operator == '-'? v1 - v2 : 0;

	}
	return ret;
}

//==========================================================================
//
//
//
//==========================================================================

FxMulDiv::FxMulDiv(int o, FxExpression *l, FxExpression *r)
: FxBinary(o, l, r)
{
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxMulDiv::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();

	if (!ResolveLR(ctx, true)) return NULL;

	if (!ValueType.isNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
	else if (left->isConstant() && right->isConstant())
	{
		if (ValueType == VAL_Float)
		{
			double v;
			double v1 = left->EvalExpression(NULL).GetFloat();
			double v2 = right->EvalExpression(NULL).GetFloat();

			if (Operator != '*' && v2 == 0)
			{
				ScriptPosition.Message(MSG_ERROR, "Division by 0");
				delete this;
				return NULL;
			}

			v =	Operator == '*'? v1 * v2 : 
				Operator == '/'? v1 / v2 : 
				Operator == '%'? fmod(v1, v2) : 0;

			FxExpression *e = new FxConstant(v, ScriptPosition);
			delete this;
			return e;
		}
		else
		{
			int v;
			int v1 = left->EvalExpression(NULL).GetInt();
			int v2 = right->EvalExpression(NULL).GetInt();

			if (Operator != '*' && v2 == 0)
			{
				ScriptPosition.Message(MSG_ERROR, "Division by 0");
				delete this;
				return NULL;
			}

			v =	Operator == '*'? v1 * v2 : 
				Operator == '/'? v1 / v2 : 
				Operator == '%'? v1 % v2 : 0;

			FxExpression *e = new FxConstant(v, ScriptPosition);
			delete this;
			return e;

		}
	}
	return this;
}


//==========================================================================
//
//
//
//==========================================================================

ExpVal FxMulDiv::EvalExpression (AActor *self)
{
	ExpVal ret;

	if (ValueType == VAL_Float)
	{
		double v1 = left->EvalExpression(self).GetFloat();
		double v2 = right->EvalExpression(self).GetFloat();

		if (Operator != '*' && v2 == 0)
		{
			I_Error("Division by 0");
		}

		ret.Type = VAL_Float;
		ret.Float = Operator == '*'? v1 * v2 : 
			  		Operator == '/'? v1 / v2 : 
					Operator == '%'? fmod(v1, v2) : 0;
	}
	else
	{
		int v1 = left->EvalExpression(self).GetInt();
		int v2 = right->EvalExpression(self).GetInt();

		if (Operator != '*' && v2 == 0)
		{
			I_Error("Division by 0");
		}

		ret.Type = VAL_Int;
		ret.Int = Operator == '*'? v1 * v2 : 
				  Operator == '/'? v1 / v2 : 
				  Operator == '%'? v1 % v2 : 0;

	}
	return ret;
}

//==========================================================================
//
//
//
//==========================================================================

FxCompareRel::FxCompareRel(int o, FxExpression *l, FxExpression *r)
: FxBinary(o, l, r)
{
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxCompareRel::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	if (!ResolveLR(ctx, true)) return false;

	if (!ValueType.isNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
	else if (left->isConstant() && right->isConstant())
	{
		int v;

		if (ValueType == VAL_Float)
		{
			double v1 = left->EvalExpression(NULL).GetFloat();
			double v2 = right->EvalExpression(NULL).GetFloat();
			v =	Operator == '<'? v1 < v2 : 
				Operator == '>'? v1 > v2 : 
				Operator == TK_Geq? v1 >= v2 : 
				Operator == TK_Leq? v1 <= v2 : 0;
		}
		else
		{
			int v1 = left->EvalExpression(NULL).GetInt();
			int v2 = right->EvalExpression(NULL).GetInt();
			v =	Operator == '<'? v1 < v2 : 
				Operator == '>'? v1 > v2 : 
				Operator == TK_Geq? v1 >= v2 : 
				Operator == TK_Leq? v1 <= v2 : 0;
		}
		FxExpression *e = new FxConstant(v, ScriptPosition);
		delete this;
		return e;
	}
	ValueType = VAL_Int;
	return this;
}


//==========================================================================
//
//
//
//==========================================================================

ExpVal FxCompareRel::EvalExpression (AActor *self)
{
	ExpVal ret;

	ret.Type = VAL_Int;

	if (left->ValueType == VAL_Float || right->ValueType == VAL_Float)
	{
		double v1 = left->EvalExpression(self).GetFloat();
		double v2 = right->EvalExpression(self).GetFloat();
		ret.Int = Operator == '<'? v1 < v2 : 
				  Operator == '>'? v1 > v2 : 
				  Operator == TK_Geq? v1 >= v2 : 
				  Operator == TK_Leq? v1 <= v2 : 0;
	}
	else
	{
		int v1 = left->EvalExpression(self).GetInt();
		int v2 = right->EvalExpression(self).GetInt();
		ret.Int = Operator == '<'? v1 < v2 : 
				  Operator == '>'? v1 > v2 : 
				  Operator == TK_Geq? v1 >= v2 : 
				  Operator == TK_Leq? v1 <= v2 : 0;
	}
	return ret;
}


//==========================================================================
//
//
//
//==========================================================================

FxCompareEq::FxCompareEq(int o, FxExpression *l, FxExpression *r)
: FxBinary(o, l, r)
{
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxCompareEq::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();

	if (!ResolveLR(ctx, true)) return false;

	if (!left || !right)
	{
		delete this;
		return NULL;
	}

	if (!ValueType.isNumeric() && !ValueType.isPointer())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
	else if (left->isConstant() && right->isConstant())
	{
		int v;

		if (ValueType == VAL_Float)
		{
			double v1 = left->EvalExpression(NULL).GetFloat();
			double v2 = right->EvalExpression(NULL).GetFloat();
			v = Operator == TK_Eq? v1 == v2 : v1 != v2;
		}
		else
		{
			int v1 = left->EvalExpression(NULL).GetInt();
			int v2 = right->EvalExpression(NULL).GetInt();
			v = Operator == TK_Eq? v1 == v2 : v1 != v2;
		}
		FxExpression *e = new FxConstant(v, ScriptPosition);
		delete this;
		return e;
	}
	ValueType = VAL_Int;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxCompareEq::EvalExpression (AActor *self)
{
	ExpVal ret;

	ret.Type = VAL_Int;

	if (left->ValueType == VAL_Float || right->ValueType == VAL_Float)
	{
		double v1 = left->EvalExpression(self).GetFloat();
		double v2 = right->EvalExpression(self).GetFloat();
		ret.Int = Operator == TK_Eq? v1 == v2 : v1 != v2;
	}
	else if (ValueType == VAL_Int)
	{
		int v1 = left->EvalExpression(self).GetInt();
		int v2 = right->EvalExpression(self).GetInt();
		ret.Int = Operator == TK_Eq? v1 == v2 : v1 != v2;
	}
	else
	{
		// Implement pointer comparison
		ret.Int = 0;
	}
	return ret;
}


//==========================================================================
//
//
//
//==========================================================================

FxBinaryInt::FxBinaryInt(int o, FxExpression *l, FxExpression *r)
: FxBinary(o, l, r)
{
	ValueType = VAL_Int;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxBinaryInt::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	if (!ResolveLR(ctx, false)) return false;

	if (ctx.lax && ValueType == VAL_Float)
	{
		// For DECORATE which allows floats here.
		if (left->ValueType != VAL_Int)
		{
			left = new FxIntCast(left);
			left = left->Resolve(ctx);
		}
		if (right->ValueType != VAL_Int)
		{
			right = new FxIntCast(right);
			right = left->Resolve(ctx);
		}
		if (left == NULL || right == NULL)
		{
			delete this;
			return NULL;
		}
		ValueType = VAL_Int;
	}

	if (ValueType != VAL_Int)
	{
		ScriptPosition.Message(MSG_ERROR, "Integer type expected");
		delete this;
		return NULL;
	}
	else if (left->isConstant() && right->isConstant())
	{
		int v1 = left->EvalExpression(NULL).GetInt();
		int v2 = right->EvalExpression(NULL).GetInt();

		FxExpression *e = new FxConstant(
			Operator == TK_LShift? v1 << v2 : 
			Operator == TK_RShift? v1 >> v2 : 
			Operator == TK_URShift? int((unsigned int)(v1) >> v2) : 
			Operator == '&'? v1 & v2 : 
			Operator == '|'? v1 | v2 : 
			Operator == '^'? v1 ^ v2 : 0, ScriptPosition);

		delete this;
		return e;
	}
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxBinaryInt::EvalExpression (AActor *self)
{
	int v1 = left->EvalExpression(self).GetInt();
	int v2 = right->EvalExpression(self).GetInt();

	ExpVal ret;

	ret.Type = VAL_Int;
	ret.Int =
		Operator == TK_LShift? v1 << v2 : 
		Operator == TK_RShift? v1 >> v2 : 
		Operator == TK_URShift? int((unsigned int)(v1) >> v2) : 
		Operator == '&'? v1 & v2 : 
		Operator == '|'? v1 | v2 : 
		Operator == '^'? v1 ^ v2 : 0;

	return ret;
}

//==========================================================================
//
//
//
//==========================================================================

FxBinaryLogical::FxBinaryLogical(int o, FxExpression *l, FxExpression *r)
: FxExpression(l->ScriptPosition)
{
	Operator=o;
	left=l;
	right=r;
	ValueType = VAL_Int;
}

//==========================================================================
//
//
//
//==========================================================================

FxBinaryLogical::~FxBinaryLogical()
{
	SAFE_DELETE(left);
	SAFE_DELETE(right);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxBinaryLogical::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	if (left) left = left->ResolveAsBoolean(ctx);
	if (right) right = right->ResolveAsBoolean(ctx);
	if (!left || !right)
	{
		delete this;
		return NULL;
	}

	int b_left=-1, b_right=-1;

	if (left->isConstant()) b_left = left->EvalExpression(NULL).GetBool();
	if (right->isConstant()) b_right = right->EvalExpression(NULL).GetBool();

	// Do some optimizations. This will throw out all sub-expressions that are not
	// needed to retrieve the final result.
	if (Operator == TK_AndAnd)
	{
		if (b_left==0 || b_right==0)
		{
			FxExpression *x = new FxConstant(0, ScriptPosition);
			delete this;
			return x;
		}
		else if (b_left==1 && b_right==1)
		{
			FxExpression *x = new FxConstant(1, ScriptPosition);
			delete this;
			return x;
		}
		else if (b_left==1)
		{
			FxExpression *x = right;
			right=NULL;
			delete this;
			return x;
		}
		else if (b_right==1)
		{
			FxExpression *x = left;
			left=NULL;
			delete this;
			return x;
		}
	}
	else if (Operator == TK_OrOr)
	{
		if (b_left==1 || b_right==1)
		{
			FxExpression *x = new FxConstant(1, ScriptPosition);
			delete this;
			return x;
		}
		if (b_left==0 && b_right==0)
		{
			FxExpression *x = new FxConstant(0, ScriptPosition);
			delete this;
			return x;
		}
		else if (b_left==0)
		{
			FxExpression *x = right;
			right=NULL;
			delete this;
			return x;
		}
		else if (b_right==0)
		{
			FxExpression *x = left;
			left=NULL;
			delete this;
			return x;
		}
	}
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxBinaryLogical::EvalExpression (AActor *self)
{
	bool b_left = left->EvalExpression(self).GetBool();
	ExpVal ret;

	ret.Type = VAL_Int;
	ret.Int = false;

	if (Operator == TK_AndAnd)
	{
		ret.Int = (b_left && right->EvalExpression(self).GetBool());
	}
	else if (Operator == TK_OrOr)
	{
		ret.Int = (b_left || right->EvalExpression(self).GetBool());
	}
	return ret;
}


//==========================================================================
//
//
//
//==========================================================================

FxConditional::FxConditional(FxExpression *c, FxExpression *t, FxExpression *f)
: FxExpression(c->ScriptPosition)
{
	condition = c;
	truex=t;
	falsex=f;
}

//==========================================================================
//
//
//
//==========================================================================

FxConditional::~FxConditional()
{
	SAFE_DELETE(condition);
	SAFE_DELETE(truex);
	SAFE_DELETE(falsex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxConditional::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	if (condition) condition = condition->ResolveAsBoolean(ctx);
	RESOLVE(truex, ctx);
	RESOLVE(falsex, ctx);
	ABORT(condition && truex && falsex);

	if (truex->ValueType == VAL_Int && falsex->ValueType == VAL_Int)
		ValueType = VAL_Int;
	else if (truex->ValueType.isNumeric() && falsex->ValueType.isNumeric())
		ValueType = VAL_Float;
	//else if (truex->ValueType != falsex->ValueType)

	if (condition->isConstant())
	{
		ExpVal condval = condition->EvalExpression(NULL);
		bool result = condval.GetBool();

		FxExpression *e = result? truex:falsex;
		delete (result? falsex:truex);
		falsex = truex = NULL;
		delete this;
		return e;
	}

	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxConditional::EvalExpression (AActor *self)
{
	ExpVal condval = condition->EvalExpression(self);
	bool result = condval.GetBool();

	FxExpression *e = result? truex:falsex;
	return e->EvalExpression(self);
}

//==========================================================================
//
//
//
//==========================================================================
FxAbs::FxAbs(FxExpression *v)
: FxExpression(v->ScriptPosition)
{
	val = v;
	ValueType = v->ValueType;
}

//==========================================================================
//
//
//
//==========================================================================

FxAbs::~FxAbs()
{
	SAFE_DELETE(val);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxAbs::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(val, ctx);


	if (!ValueType.isNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
	else if (val->isConstant())
	{
		ExpVal value = val->EvalExpression(NULL);
		switch (value.Type)
		{
		case VAL_Int:
			value.Int = abs(value.Int);
			break;

		case VAL_Float:
			value.Float = fabs(value.Float);

		default:
			// shouldn't happen
			delete this;
			return NULL;
		}
		FxExpression *x = new FxConstant(value, ScriptPosition);
		delete this;
		return x;
	}
	ValueType = val->ValueType;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxAbs::EvalExpression (AActor *self)
{
	ExpVal value = val->EvalExpression(self);
	switch (value.Type)
	{
	default:
	case VAL_Int:
		value.Int = abs(value.Int);
		break;

	case VAL_Float:
		value.Float = fabs(value.Float);
		break;
	}
	return value;
}

//==========================================================================
//
//
//
//==========================================================================
FxRandom::FxRandom(FRandom * r, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos)
: FxExpression(pos)
{
	rng = r;
	min = new FxIntCast(mi);
	max = new FxIntCast(ma);
	ValueType = VAL_Int;
}

//==========================================================================
//
//
//
//==========================================================================

FxRandom::~FxRandom()
{
	SAFE_DELETE(min);
	SAFE_DELETE(max);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxRandom::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	if (min && max)
	{
		RESOLVE(min, ctx);
		RESOLVE(max, ctx);
		ABORT(min && max);
	}
	return this;
};


//==========================================================================
//
//
//
//==========================================================================

ExpVal FxRandom::EvalExpression (AActor *self)
{
	int minval = min->EvalExpression (self).GetInt();
	int maxval = max->EvalExpression (self).GetInt();

	ExpVal val;
	val.Type = VAL_Int;

	if (maxval < minval)
	{
		swap (maxval, minval);
	}

	val.Int = (*rng)(maxval - minval + 1) + minval;
	return val;
}

//==========================================================================
//
//
//
//==========================================================================

FxRandom2::FxRandom2(FRandom *r, FxExpression *m, const FScriptPosition &pos)
: FxExpression(pos)
{
	rng = r;
	if (m) mask = new FxIntCast(m);
	else mask = new FxConstant(-1, pos);
	ValueType = VAL_Int;
}

//==========================================================================
//
//
//
//==========================================================================

FxRandom2::~FxRandom2()
{
	SAFE_DELETE(mask);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxRandom2::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(mask, ctx);
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxRandom2::EvalExpression (AActor *self)
{
	ExpVal maskval = mask->EvalExpression(self);
	int imaskval = maskval.GetInt();

	maskval.Type = VAL_Int;
	maskval.Int = rng->Random2(imaskval);
	return maskval;
}

//==========================================================================
//
//
//
//==========================================================================

FxIdentifier::FxIdentifier(FName name, const FScriptPosition &pos)
: FxExpression(pos)
{
	Identifier = name;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxIdentifier::Resolve(FCompileContext& ctx)
{
	PSymbol * sym;
	FxExpression *newex = NULL;
	//FBaseCVar * cv = NULL;
	//FString s;
	int num;
	//const PClass *Class;
	
	CHECKRESOLVED();
	// see if the current class (if valid) defines something with this name.
	if ((sym = ctx.FindInClass(Identifier)) != NULL)
	{
		if (sym->SymbolType == SYM_Const)
		{
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as class constant\n", Identifier.GetChars());
			newex = FxConstant::MakeConstant(sym, ScriptPosition);
		}
		else if (sym->SymbolType == SYM_Variable)
		{
			PSymbolVariable *vsym = static_cast<PSymbolVariable*>(sym);
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as member variable, index %d\n", Identifier.GetChars(), vsym->offset);
			newex = new FxClassMember((new FxSelf(ScriptPosition))->Resolve(ctx), vsym, ScriptPosition);
		}
		else
		{
			ScriptPosition.Message(MSG_ERROR, "Invalid member identifier '%s'\n", Identifier.GetChars());
		}
	}
	// now check the global identifiers.
	else if ((sym = ctx.FindGlobal(Identifier)) != NULL)
	{
		if (sym->SymbolType == SYM_Const)
		{
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as global constant\n", Identifier.GetChars());
			newex = FxConstant::MakeConstant(sym, ScriptPosition);
		}
		else if (sym->SymbolType == SYM_Variable)	// global variables will always be native
		{
			PSymbolVariable *vsym = static_cast<PSymbolVariable*>(sym);
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as global variable, address %d\n", Identifier.GetChars(), vsym->offset);
			newex = new FxGlobalVariable(vsym, ScriptPosition);
		}
		else
		{
			ScriptPosition.Message(MSG_ERROR, "Invalid global identifier '%s'\n", Identifier.GetChars());
		}
	}
	/*
	else if ((Class = PClass::FindClass(Identifier)))
	{
		pos.Message(MSG_DEBUGLOG, "Resolving name '%s' as class name\n", Identifier.GetChars());
			newex = new FxClassType(Class, ScriptPosition);
		}
	}
	*/

	// also check for CVars
	/*
	else if ((cv = FindCVar(Identifier, NULL)) != NULL)
	{
		CLOG(CL_RESOLVE, LPrintf("Resolving name '%s' as cvar\n", Identifier.GetChars()));
		newex = new FxCVar(cv, ScriptPosition);
	}
	*/
	// amd line specials
	else if ((num = P_FindLineSpecial(Identifier, NULL, NULL)))
	{
		ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as line special %d\n", Identifier.GetChars(), num);
		newex = new FxConstant(num, ScriptPosition);
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Unknown identifier '%s'", Identifier.GetChars());
		newex = new FxConstant(0, ScriptPosition);
	}
	delete this;
	return newex? newex->Resolve(ctx) : NULL;
}


//==========================================================================
//
//
//
//==========================================================================

FxSelf::FxSelf(const FScriptPosition &pos)
: FxExpression(pos)
{
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxSelf::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	if (!ctx.cls)
	{
		// can't really happen with DECORATE's expression evaluator.
		ScriptPosition.Message(MSG_ERROR, "self used outside of a member function");
		delete this;
		return NULL;
	}
	ValueType = ctx.cls;
	ValueType.Type = VAL_Object;
	return this;
}  

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxSelf::EvalExpression (AActor *self)
{
	ExpVal ret;
	
	ret.Type = VAL_Object;
	ret.pointer = self;
	return ret;
}

//==========================================================================
//
//
//
//==========================================================================

FxGlobalVariable::FxGlobalVariable(PSymbolVariable *mem, const FScriptPosition &pos)
: FxExpression(pos)
{
	var = mem;
	AddressRequested = false;
}

//==========================================================================
//
//
//
//==========================================================================

void FxGlobalVariable::RequestAddress()
{
	AddressRequested = true;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxGlobalVariable::Resolve(FCompileContext&)
{
	CHECKRESOLVED();
	switch (var->ValueType.Type)
	{
	case VAL_Int:
	case VAL_Bool:
		ValueType = VAL_Int;
		break;

	case VAL_Float:
	case VAL_Fixed:
	case VAL_Angle:
		ValueType = VAL_Float;

	case VAL_Object:
	case VAL_Class:
		ValueType = var->ValueType;
		break;

	default:
		ScriptPosition.Message(MSG_ERROR, "Invalid type for global variable");
		delete this;
		return NULL;
	}
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxGlobalVariable::EvalExpression (AActor *self)
{
	ExpVal ret;
	
	if (!AddressRequested)
	{
		ret = GetVariableValue((void*)var->offset, var->ValueType);
	}
	else
	{
		ret.pointer = (void*)var->offset;
		ret.Type = VAL_Pointer;
	}
	return ret;
}


//==========================================================================
//
//
//
//==========================================================================

FxClassMember::FxClassMember(FxExpression *x, PSymbolVariable* mem, const FScriptPosition &pos)
: FxExpression(pos)
{
	classx = x;
	membervar = mem;
	AddressRequested = false;
	//if (classx->IsDefaultObject()) Readonly=true;
}

//==========================================================================
//
//
//
//==========================================================================

FxClassMember::~FxClassMember()
{
	SAFE_DELETE(classx);
}

//==========================================================================
//
//
//
//==========================================================================

void FxClassMember::RequestAddress()
{
	AddressRequested = true;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxClassMember::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(classx, ctx);

	if (classx->ValueType != VAL_Object && classx->ValueType != VAL_Class)
	{
		ScriptPosition.Message(MSG_ERROR, "Member variable requires a class or object");
		delete this;
		return NULL;
	}
	switch (membervar->ValueType.Type)
	{
	case VAL_Int:
	case VAL_Bool:
		ValueType = VAL_Int;
		break;

	case VAL_Float:
	case VAL_Fixed:
	case VAL_Angle:
		ValueType = VAL_Float;
		break;

	case VAL_Object:
	case VAL_Class:
	case VAL_Array:
		ValueType = membervar->ValueType;
		break;

	default:
		ScriptPosition.Message(MSG_ERROR, "Invalid type for member variable %s", membervar->SymbolName.GetChars());
		delete this;
		return NULL;
	}
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxClassMember::EvalExpression (AActor *self)
{
	char *object = NULL;
	if (classx->ValueType == VAL_Class)
	{
		// not implemented yet
	}
	else
	{
		object = classx->EvalExpression(self).GetPointer<char>();
	}
	if (object == NULL)
	{
		I_Error("Accessing member variable without valid object");
	}

	ExpVal ret;
	
	if (!AddressRequested)
	{
		ret = GetVariableValue(object + membervar->offset, membervar->ValueType);
	}
	else
	{
		ret.pointer = object + membervar->offset;
		ret.Type = VAL_Pointer;
	}
	return ret;
}



//==========================================================================
//
//
//
//==========================================================================

FxArrayElement::FxArrayElement(FxExpression *base, FxExpression *_index)
:FxExpression(base->ScriptPosition)
{
	Array=base;
	index = _index;
	//AddressRequested = false;
}

//==========================================================================
//
//
//
//==========================================================================

FxArrayElement::~FxArrayElement()
{
	SAFE_DELETE(Array);
	SAFE_DELETE(index);
}

//==========================================================================
//
//
//
//==========================================================================

/*
void FxArrayElement::RequestAddress()
{
	AddressRequested = true;
}
*/

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxArrayElement::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Array,ctx);
	SAFE_RESOLVE(index,ctx);

	if (index->ValueType == VAL_Float && ctx.lax)
	{
		// DECORATE allows floats here so cast them to int.
		index = new FxIntCast(index);
		index = index->Resolve(ctx);
		if (index == NULL) 
		{
			delete this;
			return NULL;
		}
	}
	if (index->ValueType != VAL_Int)
	{
		ScriptPosition.Message(MSG_ERROR, "Array index must be integer");
		delete this;
		return NULL;
	}

	if (Array->ValueType != VAL_Array)
	{
		ScriptPosition.Message(MSG_ERROR, "'[]' can only be used with arrays.");
		delete this;
		return NULL;
	}

	ValueType = Array->ValueType.GetBaseType();
	if (ValueType != VAL_Int)
	{
		// int arrays only for now
		ScriptPosition.Message(MSG_ERROR, "Only integer arrays are supported.");
		delete this;
		return NULL;
	}
	Array->RequestAddress();
	return this;
}

//==========================================================================
//
// in its current state this won't be able to do more than handle the args array.
//
//==========================================================================

ExpVal FxArrayElement::EvalExpression (AActor *self)
{
	int * arraystart = Array->EvalExpression(self).GetPointer<int>();
	int indexval = index->EvalExpression(self).GetInt();

	if (indexval < 0 || indexval >= Array->ValueType.size)
	{
		I_Error("Array index out of bounds");
	}

	ExpVal ret;

	ret.Int = arraystart[indexval];
	ret.Type = VAL_Int;
	return ret;
}



