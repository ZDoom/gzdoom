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


//==========================================================================
//
// FScriptPosition::Message
//
//==========================================================================

void STACK_ARGS FScriptPosition::Message (int severity, const char *message, ...)
{
	FString composed;

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
		if (!developer) return;
		type = "message";
		break;

	case MSG_LOG:
		type = "message";
		level = PRINT_LOG;
		break;
		
	case MSG_DEBUGLOG:
		if (!developer) return;
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

ExpVal FxConstant::EvalExpression (AActor *self, const PClass *cls)
{
	return value;
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

	if (basex->isConstant())
	{
		ExpVal constval = basex->EvalExpression(NULL, ctx.cls);
		FxExpression *x = new FxConstant(constval.GetInt(), ScriptPosition);
		delete this;
		return x;
	}
	else if (basex->ValueType == VAL_Int)
	{
		FxExpression *x = basex;
		basex = NULL;
		delete this;
		return x;
	}
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxIntCast::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal baseval = basex->EvalExpression(self, cls);
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

	FxExpression *e = Operand;
	Operand = NULL;
	delete this;
	return e;
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

	if (Operand->isConstant())
	{
		ExpVal val = Operand->EvalExpression(NULL, ctx.cls);
		FxExpression *e = val.Type == VAL_Int?
			new FxConstant(-val.Int, ScriptPosition) :
			new FxConstant(-val.Float, ScriptPosition);
		delete this;
		return e;
	}
	ValueType = Operand->ValueType;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxMinusSign::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal ret;

	if (ValueType == VAL_Int)
	{
		ret.Int = -Operand->EvalExpression(self, cls).GetInt();
		ret.Type = VAL_Int;
	}
	else
	{
		ret.Float = -Operand->EvalExpression(self, cls).GetFloat();
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

	/* DECORATE allows this.
	if (Operand->ValueType != VAL_Int)
	{
		ScriptPosition.Message(MSG_ERROR, "Integer type expected");
		delete this;
		return NULL;
	}
	*/

	if (Operand->isConstant())
	{
		int result = ~Operand->EvalExpression(NULL, ctx.cls).GetInt();
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

ExpVal FxUnaryNotBitwise::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal ret;

	ret.Int = ~Operand->EvalExpression(self, cls).GetInt();
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

	if (Operand->isConstant())
	{
		bool result = !Operand->EvalExpression(NULL, ctx.cls).GetBool();
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

ExpVal FxUnaryNotBoolean::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal ret;

	ret.Int = !Operand->EvalExpression(self, cls).GetBool();
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

	ValueType = left->ValueType;
	if (castnumeric && right->ValueType == VAL_Float)
	{
		ValueType = VAL_Float;
	}
	/* not for DECORATE - will be activated later
	else if (left->ValueType != right->ValueType)
	{
		ScriptPosition.Message(MSG_ERROR, "Type mismatch in expression");
		delete this;
		return false;
	}
	*/
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

	if (left->isConstant() && right->isConstant())
	{
		if (ValueType == VAL_Float)
		{
			double v;
			double v1 = left->EvalExpression(NULL, ctx.cls).GetFloat();
			double v2 = right->EvalExpression(NULL, ctx.cls).GetFloat();

			v =	Operator == '+'? v1 + v2 : 
				Operator == '-'? v1 - v2 : 0;

			FxExpression *e = new FxConstant(v, ScriptPosition);
			delete this;
			return e;
		}
		else
		{
			int v;
			int v1 = left->EvalExpression(NULL, ctx.cls).GetInt();
			int v2 = right->EvalExpression(NULL, ctx.cls).GetInt();

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

ExpVal FxAddSub::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal ret;

	if (left->ValueType == VAL_Float || right->ValueType ==VAL_Float)
	{
		double v1 = left->EvalExpression(self, cls).GetFloat();
		double v2 = right->EvalExpression(self, cls).GetFloat();

		ret.Type = VAL_Float;
		ret.Float = Operator == '+'? v1 + v2 : 
			  		Operator == '-'? v1 - v2 : 0;
	}
	else
	{
		int v1 = left->EvalExpression(self, cls).GetInt();
		int v2 = right->EvalExpression(self, cls).GetInt();

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

	if (left->isConstant() && right->isConstant())
	{
		if (ValueType == VAL_Float)
		{
			double v;
			double v1 = left->EvalExpression(NULL, ctx.cls).GetFloat();
			double v2 = right->EvalExpression(NULL, ctx.cls).GetFloat();

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
			int v1 = left->EvalExpression(NULL, ctx.cls).GetInt();
			int v2 = right->EvalExpression(NULL, ctx.cls).GetInt();

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

ExpVal FxMulDiv::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal ret;

	if (left->ValueType == VAL_Float || right->ValueType ==VAL_Float)
	{
		double v1 = left->EvalExpression(self, cls).GetFloat();
		double v2 = right->EvalExpression(self, cls).GetFloat();

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
		int v1 = left->EvalExpression(self, cls).GetInt();
		int v2 = right->EvalExpression(self, cls).GetInt();

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

	if (left->isConstant() && right->isConstant())
	{
		int v;

		if (ValueType == VAL_Float)
		{
			double v1 = left->EvalExpression(NULL, ctx.cls).GetFloat();
			double v2 = right->EvalExpression(NULL, ctx.cls).GetFloat();
			v =	Operator == '<'? v1 < v2 : 
				Operator == '>'? v1 > v2 : 
				Operator == TK_Geq? v1 >= v2 : 
				Operator == TK_Leq? v1 <= v2 : 0;
		}
		else
		{
			int v1 = left->EvalExpression(NULL, ctx.cls).GetInt();
			int v2 = right->EvalExpression(NULL, ctx.cls).GetInt();
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

ExpVal FxCompareRel::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal ret;

	ret.Type = VAL_Int;

	if (left->ValueType == VAL_Float || right->ValueType ==VAL_Float)
	{
		double v1 = left->EvalExpression(self, cls).GetFloat();
		double v2 = right->EvalExpression(self, cls).GetFloat();
		ret.Int = Operator == '<'? v1 < v2 : 
				  Operator == '>'? v1 > v2 : 
				  Operator == TK_Geq? v1 >= v2 : 
				  Operator == TK_Leq? v1 <= v2 : 0;
	}
	else
	{
		int v1 = left->EvalExpression(self, cls).GetInt();
		int v2 = right->EvalExpression(self, cls).GetInt();
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

	if (left->isConstant() && right->isConstant())
	{
		int v;

		if (ValueType == VAL_Float)
		{
			double v1 = left->EvalExpression(NULL, ctx.cls).GetFloat();
			double v2 = right->EvalExpression(NULL, ctx.cls).GetFloat();
			v = Operator == TK_Eq? v1 == v2 : v1 != v2;
		}
		else
		{
			int v1 = left->EvalExpression(NULL, ctx.cls).GetInt();
			int v2 = right->EvalExpression(NULL, ctx.cls).GetInt();
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

ExpVal FxCompareEq::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal ret;

	ret.Type = VAL_Int;

	if (left->ValueType == VAL_Float || right->ValueType ==VAL_Float)
	{
		double v1 = left->EvalExpression(self, cls).GetFloat();
		double v2 = right->EvalExpression(self, cls).GetFloat();
		ret.Int = Operator == TK_Eq? v1 == v2 : v1 != v2;
	}
	else
	{
		int v1 = left->EvalExpression(self, cls).GetInt();
		int v2 = right->EvalExpression(self, cls).GetInt();
		ret.Int = Operator == TK_Eq? v1 == v2 : v1 != v2;
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

	/* later! DECORATE doesn't do proper type checks
	if (lax)
	{
		if (left->ValueType == VAL_Float) left = new FxIntCast(left);
		if (right->ValueType == VAL_Float) right = new FxIntCast(right);
	}

	if (left->ValueType != VAL_Int || right->->ValueType != VAL_Int)
	{
		ScriptPosition.Message(MSG_ERROR, "Type mismatch in expression");
		delete this;
		return NULL;
	}
	*/

	if (left->isConstant() && right->isConstant())
	{
		int v1 = left->EvalExpression(NULL, ctx.cls).GetInt();
		int v2 = right->EvalExpression(NULL, ctx.cls).GetInt();

		FxExpression *e = new FxConstant(
			Operator == TK_LShift? v1 << v2 : 
			Operator == TK_RShift? v1 >> v2 : 
			Operator == TK_URShift? int(unsigned int(v1) >> v2) : 
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

ExpVal FxBinaryInt::EvalExpression (AActor *self, const PClass *cls)
{
	int v1 = left->EvalExpression(self, cls).GetInt();
	int v2 = right->EvalExpression(self, cls).GetInt();

	ExpVal ret;

	ret.Type = VAL_Int;
	ret.Int =
		Operator == TK_LShift? v1 << v2 : 
		Operator == TK_RShift? v1 >> v2 : 
		Operator == TK_URShift? int(unsigned int(v1) >> v2) : 
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

	if (left->isConstant()) b_left = left->EvalExpression(NULL, ctx.cls).GetBool();
	if (right->isConstant()) b_right = right->EvalExpression(NULL, ctx.cls).GetBool();

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

ExpVal FxBinaryLogical::EvalExpression (AActor *self, const PClass *cls)
{
	bool b_left = left->EvalExpression(self, cls).GetBool();
	ExpVal ret;

	ret.Type = VAL_Int;
	ret.Int = false;

	if (Operator == TK_AndAnd)
	{
		ret.Int = (b_left && right->EvalExpression(self, cls).GetBool());
	}
	else if (Operator == TK_OrOr)
	{
		ret.Int = (b_left || right->EvalExpression(self, cls).GetBool());
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
	else
		ValueType = VAL_Float;

	if (condition->isConstant())
	{
		ExpVal condval = condition->EvalExpression(NULL, ctx.cls);
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

ExpVal FxConditional::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal condval = condition->EvalExpression(self, cls);
	bool result = condval.GetBool();

	FxExpression *e = result? truex:falsex;
	return e->EvalExpression(self, cls);
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
	Type = VAL_Int;
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

	if (val->isConstant())
	{
		ExpVal value = val->EvalExpression(NULL, ctx.cls);
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

ExpVal FxAbs::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal value = val->EvalExpression(self, cls);
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
	if (min && max)
	{
		min = new FxIntCast(mi);
		max = new FxIntCast(ma);
	}
	else
	{
		SAFE_DELETE(mi);
		SAFE_DELETE(ma);
		min = max = NULL;
	}
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

ExpVal FxRandom::EvalExpression (AActor *self, const PClass *cls)
{
	int minval = min->EvalExpression (self, cls).GetInt();
	int maxval = max->EvalExpression (self, cls).GetInt();

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

ExpVal FxRandom2::EvalExpression (AActor *self, const PClass *cls)
{
	ExpVal maskval = mask->EvalExpression(self, cls);
	int imaskval = maskval.GetInt();

	maskval.Type = VAL_Int;
	maskval.Int = rng->Random2(imaskval);
	return maskval;
}
