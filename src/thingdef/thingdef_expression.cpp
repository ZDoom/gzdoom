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
#include "m_fixed.h"

int testglobalvar = 1337;	// just for having one global variable to test with
DEFINE_GLOBAL_VARIABLE(testglobalvar)

// Accessible actor member variables
DEFINE_MEMBER_VARIABLE(alpha, AActor)
DEFINE_MEMBER_VARIABLE(angle, AActor)
DEFINE_MEMBER_VARIABLE(args, AActor)
DEFINE_MEMBER_VARIABLE(ceilingz, AActor)
DEFINE_MEMBER_VARIABLE(floorz, AActor)
DEFINE_MEMBER_VARIABLE(health, AActor)
DEFINE_MEMBER_VARIABLE(Mass, AActor)
DEFINE_MEMBER_VARIABLE(pitch, AActor)
DEFINE_MEMBER_VARIABLE(special, AActor)
DEFINE_MEMBER_VARIABLE(special1, AActor)
DEFINE_MEMBER_VARIABLE(special2, AActor)
DEFINE_MEMBER_VARIABLE(tid, AActor)
DEFINE_MEMBER_VARIABLE(TIDtoHate, AActor)
DEFINE_MEMBER_VARIABLE(waterlevel, AActor)
DEFINE_MEMBER_VARIABLE_ALIAS(x, __pos.x, AActor)
DEFINE_MEMBER_VARIABLE_ALIAS(y, __pos.y, AActor)
DEFINE_MEMBER_VARIABLE_ALIAS(z, __pos.z, AActor)
DEFINE_MEMBER_VARIABLE(velx, AActor)
DEFINE_MEMBER_VARIABLE(vely, AActor)
DEFINE_MEMBER_VARIABLE(velz, AActor)
DEFINE_MEMBER_VARIABLE_ALIAS(momx, velx, AActor)
DEFINE_MEMBER_VARIABLE_ALIAS(momy, vely, AActor)
DEFINE_MEMBER_VARIABLE_ALIAS(momz, velz, AActor)
DEFINE_MEMBER_VARIABLE(scaleX, AActor)
DEFINE_MEMBER_VARIABLE(scaleY, AActor)
DEFINE_MEMBER_VARIABLE(Damage, AActor)
DEFINE_MEMBER_VARIABLE(Score, AActor)
DEFINE_MEMBER_VARIABLE(accuracy, AActor)
DEFINE_MEMBER_VARIABLE(stamina, AActor)
DEFINE_MEMBER_VARIABLE(height, AActor)
DEFINE_MEMBER_VARIABLE(radius, AActor)
DEFINE_MEMBER_VARIABLE(reactiontime, AActor)
DEFINE_MEMBER_VARIABLE(meleerange, AActor)
DEFINE_MEMBER_VARIABLE(Speed, AActor)
DEFINE_MEMBER_VARIABLE(roll, AActor)


//==========================================================================
//
// EvalExpression
// [GRB] Evaluates previously stored expression
//
//==========================================================================


int EvalExpressionI (DWORD xi, AActor *self)
{
	FxExpression *x = StateParams.Get(xi);
	if (x == NULL) return 0;

	return x->EvalExpression (self).GetInt();
}

int EvalExpressionCol (DWORD xi, AActor *self)
{
	FxExpression *x = StateParams.Get(xi);
	if (x == NULL) return 0;

	return x->EvalExpression (self).GetColor();
}

FSoundID EvalExpressionSnd (DWORD xi, AActor *self)
{
	FxExpression *x = StateParams.Get(xi);
	if (x == NULL) return 0;

	return x->EvalExpression (self).GetSoundID();
}

double EvalExpressionF (DWORD xi, AActor *self)
{
	FxExpression *x = StateParams.Get(xi);
	if (x == NULL) return 0;

	return x->EvalExpression (self).GetFloat();
}

fixed_t EvalExpressionFix (DWORD xi, AActor *self)
{
	FxExpression *x = StateParams.Get(xi);
	if (x == NULL) return 0;

	ExpVal val = x->EvalExpression (self);

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

FName EvalExpressionName (DWORD xi, AActor *self)
{
	FxExpression *x = StateParams.Get(xi);
	if (x == NULL) return 0;

	return x->EvalExpression (self).GetName();
}

const PClass * EvalExpressionClass (DWORD xi, AActor *self)
{
	FxExpression *x = StateParams.Get(xi);
	if (x == NULL) return 0;

	return x->EvalExpression (self).GetClass();
}

FState *EvalExpressionState (DWORD xi, AActor *self)
{
	FxExpression *x = StateParams.Get(xi);
	if (x == NULL) return 0;

	return x->EvalExpression (self).GetState();
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

	case VAL_Sound:
		ret.Type = VAL_Sound;
		ret.Int = *(FSoundID*)address;
		break;

	case VAL_Name:
		ret.Type = VAL_Name;
		ret.Int = *(FName*)address;
		break;

	case VAL_Color:
		ret.Type = VAL_Color;
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
//
//
//==========================================================================

ExpVal FxExpression::EvalExpression (AActor *self)
{
	ScriptPosition.Message(MSG_ERROR, "Unresolved expression found");
	ExpVal val;

	val.Type = VAL_Int;
	val.Int = 0;
	return val;
}


//==========================================================================
//
//
//
//==========================================================================

bool FxExpression::isConstant() const
{
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxExpression::Resolve(FCompileContext &ctx)
{
	isresolved = true;
	return this;
}


//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxExpression::ResolveAsBoolean(FCompileContext &ctx)
{
	FxExpression *x = Resolve(ctx);
	if (x != NULL)
	{
		switch (x->ValueType.Type)
		{
		case VAL_Sound:
		case VAL_Color:
		case VAL_Name:
			x->ValueType = VAL_Int;
			break;

		default:
			break;
		}
	}
	return x;
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

FxFloatCast::FxFloatCast(FxExpression *x)
: FxExpression(x->ScriptPosition)
{
	basex = x;
	ValueType = VAL_Float;
}

//==========================================================================
//
//
//
//==========================================================================

FxFloatCast::~FxFloatCast()
{
	SAFE_DELETE(basex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxFloatCast::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(basex, ctx);

	if (basex->ValueType == VAL_Float)
	{
		FxExpression *x = basex;
		basex = NULL;
		delete this;
		return x;
	}
	else if (basex->ValueType == VAL_Int)
	{
		if (basex->isConstant())
		{
			ExpVal constval = basex->EvalExpression(NULL);
			FxExpression *x = new FxConstant(constval.GetFloat(), ScriptPosition);
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

ExpVal FxFloatCast::EvalExpression (AActor *self)
{
	ExpVal baseval = basex->EvalExpression(self);
	baseval.Float = baseval.GetFloat();
	baseval.Type = VAL_Float;
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
	if (!ResolveLR(ctx, true)) return NULL;

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

	if (!ResolveLR(ctx, true)) return NULL;

	if (!left || !right)
	{
		delete this;
		return NULL;
	}

	if (!ValueType.isNumeric() && !ValueType.isPointer())
	{
		if (left->ValueType.Type == right->ValueType.Type)
		{
			// compare other types?
			if (left->ValueType == VAL_Sound || left->ValueType == VAL_Color || left->ValueType == VAL_Name)
			{
				left->ValueType = right->ValueType = VAL_Int;
				goto cont;
			}
		}

		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return NULL;
	}
cont:
	if (left->isConstant() && right->isConstant())
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
	if (!ResolveLR(ctx, false)) return NULL;

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
			right = right->Resolve(ctx);
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


	if (!val->ValueType.isNumeric())
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
			break;

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
	if (mi != NULL && ma != NULL)
	{
		min = new FxIntCast(mi);
		max = new FxIntCast(ma);
	}
	else min = max = NULL;
	rng = r;
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
	ExpVal val;
	val.Type = VAL_Int;

	if (min != NULL && max != NULL)
	{
		int minval = min->EvalExpression (self).GetInt();
		int maxval = max->EvalExpression (self).GetInt();

		if (maxval < minval)
		{
			swapvalues (maxval, minval);
		}

		val.Int = (*rng)(maxval - minval + 1) + minval;
	}
	else
	{
		val.Int = (*rng)();
	}
	return val;
}

//==========================================================================
//
//
//
//==========================================================================
FxRandomPick::FxRandomPick(FRandom * r, TArray<FxExpression*> mi, bool floaty, const FScriptPosition &pos)
: FxExpression(pos)
{
	for (unsigned int index = 0; index < mi.Size(); index++)
	{
		FxExpression *casted;
		if (floaty)
		{
			casted = new FxFloatCast(mi[index]);
		}
		else
		{
			casted = new FxIntCast(mi[index]);
		}
		min.Push(casted);
	}
	rng = r;
	ValueType = floaty ? VAL_Float : VAL_Int;
}

//==========================================================================
//
//
//
//==========================================================================

FxRandomPick::~FxRandomPick()
{
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxRandomPick::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	for (unsigned int index = 0; index < min.Size(); index++)
	{
		RESOLVE(min[index], ctx);
		ABORT(min[index]);
	}
	return this;
};


//==========================================================================
//
//
//
//==========================================================================

ExpVal FxRandomPick::EvalExpression(AActor *self)
{
	ExpVal val;
	int max = min.Size();
	if (max > 0)
	{
		int select = (*rng)(max);
		val = min[select]->EvalExpression(self);
	}
	/* Is a default even important when the parser requires at least one
	 * choice? Why do we do this? */
	else if (ValueType == VAL_Int)
	{
		val.Type = VAL_Int;
		val.Int = (*rng)();
	}
	else
	{
		val.Type = VAL_Float;
		val.Float = (*rng)(0x40000000) / double(0x40000000);
	}
	assert(val.Type == ValueType.Type);
	return val;
}

//==========================================================================
//
//
//
//==========================================================================
FxFRandom::FxFRandom(FRandom *r, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos)
: FxRandom(r, NULL, NULL, pos)
{
	if (mi != NULL && ma != NULL)
	{
		min = mi;
		max = ma;
	}
	ValueType = VAL_Float;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxFRandom::EvalExpression (AActor *self)
{
	ExpVal val;
	val.Type = VAL_Float;
	int random = (*rng)(0x40000000);
	double frandom = random / double(0x40000000);

	if (min != NULL && max != NULL)
	{
		double minval = min->EvalExpression (self).GetFloat();
		double maxval = max->EvalExpression (self).GetFloat();

		if (maxval < minval)
		{
			swapvalues (maxval, minval);
		}

		val.Float = frandom * (maxval - minval) + minval;
	}
	else
	{
		val.Float = frandom;
	}
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
	// and line specials
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
		break;

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


//==========================================================================
//
//
//
//==========================================================================

FxFunctionCall::FxFunctionCall(FxExpression *self, FName methodname, FArgumentList *args, const FScriptPosition &pos)
: FxExpression(pos)
{
	Self = self;
	MethodName = methodname;
	ArgList = args;
}

//==========================================================================
//
//
//
//==========================================================================

FxFunctionCall::~FxFunctionCall()
{
	SAFE_DELETE(Self);
	SAFE_DELETE(ArgList);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxFunctionCall::Resolve(FCompileContext& ctx)
{
	int min, max, special;
	if (MethodName == NAME_ACS_NamedExecuteWithResult || MethodName == NAME_CallACS)
	{
		special = -ACS_ExecuteWithResult;
		min = 1;
		max = 5;
	}
	else
	{
		special = P_FindLineSpecial(MethodName.GetChars(), &min, &max);
	}
	if (special != 0 && min >= 0)
	{
		int paramcount = ArgList? ArgList->Size() : 0;
		if (paramcount < min)
		{
			ScriptPosition.Message(MSG_ERROR, "Not enough parameters for '%s' (expected %d, got %d)", 
				MethodName.GetChars(), min, paramcount);
			delete this;
			return NULL;
		}
		else if (paramcount > max)
		{
			ScriptPosition.Message(MSG_ERROR, "too many parameters for '%s' (expected %d, got %d)", 
				MethodName.GetChars(), max, paramcount);
			delete this;
			return NULL;
		}
		FxExpression *x = new FxActionSpecialCall(Self, special, ArgList, ScriptPosition);
		ArgList = NULL;
		delete this;
		return x->Resolve(ctx);
	}
	else
	{
		if (Self != NULL)
		{
			ScriptPosition.Message(MSG_ERROR, "Global variables cannot have a self pointer");
			delete this;
			return NULL;
		}
		FxExpression *x = FxGlobalFunctionCall::StaticCreate(MethodName, ArgList, ScriptPosition);
		ArgList = NULL;
		delete this;
		return x->Resolve(ctx);
	}

	ScriptPosition.Message(MSG_ERROR, "Call to unknown function '%s'", MethodName.GetChars());
	delete this;
	return NULL;
}


//==========================================================================
//
// FxActionSpecialCall
//
// If special is negative, then the first argument will be treated as a
// name for ACS_NamedExecuteWithResult.
//
//==========================================================================

FxActionSpecialCall::FxActionSpecialCall(FxExpression *self, int special, FArgumentList *args, const FScriptPosition &pos)
: FxExpression(pos)
{
	Self = self;
	Special = special;
	ArgList = args;
}

//==========================================================================
//
//
//
//==========================================================================

FxActionSpecialCall::~FxActionSpecialCall()
{
	SAFE_DELETE(Self);
	SAFE_DELETE(ArgList);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxActionSpecialCall::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	bool failed = false;

	if (ArgList != NULL)
	{
		for(unsigned i = 0; i < ArgList->Size(); i++)
		{
			(*ArgList)[i] = (*ArgList)[i]->Resolve(ctx);
			if ((*ArgList)[i] == NULL) failed = true;
			if (Special < 0 && i == 0)
			{
				if ((*ArgList)[i]->ValueType != VAL_Name)
				{
					ScriptPosition.Message(MSG_ERROR, "Name expected for parameter %d", i);
					failed = true;
				}
			}
			else if ((*ArgList)[i]->ValueType != VAL_Int)
			{
				if (ctx.lax && ((*ArgList)[i]->ValueType == VAL_Float))
				{
					(*ArgList)[i] = new FxIntCast((*ArgList)[i]);
				}
				else
				{
					ScriptPosition.Message(MSG_ERROR, "Integer expected for parameter %d", i);
					failed = true;
				}
			}
		}
		if (failed)
		{
			delete this;
			return NULL;
		}
	}
	ValueType = VAL_Int;
	return this;
}


//==========================================================================
//
// 
//
//==========================================================================

ExpVal FxActionSpecialCall::EvalExpression (AActor *self)
{
	int v[5] = {0,0,0,0,0};
	int special = Special;

	if (Self != NULL)
	{
		self = Self->EvalExpression(self).GetPointer<AActor>();
	}

	if (ArgList != NULL)
	{
		for(unsigned i = 0; i < ArgList->Size(); i++)
		{
			if (special < 0)
			{
				special = -special;
				v[i] = -(*ArgList)[i]->EvalExpression(self).GetName();
			}
			else
			{
				v[i] = (*ArgList)[i]->EvalExpression(self).GetInt();
			}
		}
	}
	ExpVal ret;
	ret.Type = VAL_Int;
	ret.Int = P_ExecuteSpecial(special, NULL, self, false, v[0], v[1], v[2], v[3], v[4]);
	return ret;
}

//==========================================================================
//
//
//
//==========================================================================

FxGlobalFunctionCall::FxGlobalFunctionCall(FName fname, FArgumentList *args, const FScriptPosition &pos)
: FxExpression(pos)
{
	Name = fname;
	ArgList = args;
}

//==========================================================================
//
//
//
//==========================================================================

FxGlobalFunctionCall::~FxGlobalFunctionCall()
{
	SAFE_DELETE(ArgList);
}


//==========================================================================
//
//
//
//==========================================================================

FxClassTypeCast::FxClassTypeCast(const PClass *dtype, FxExpression *x)
: FxExpression(x->ScriptPosition)
{
	desttype = dtype;
	basex=x;
}

//==========================================================================
//
//
//
//==========================================================================

FxClassTypeCast::~FxClassTypeCast()
{
	SAFE_DELETE(basex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxClassTypeCast::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(basex, ctx);
	
	if (basex->ValueType != VAL_Name)
	{
		ScriptPosition.Message(MSG_ERROR, "Cannot convert to class type");
		delete this;
		return NULL;
	}

	if (basex->isConstant())
	{
		FName clsname = basex->EvalExpression(NULL).GetName();
		const PClass *cls = NULL;

		if (clsname != NAME_None)
		{
			cls = PClass::FindClass(clsname);
			if (cls == NULL)
			{
				if (!ctx.lax)
				{
					ScriptPosition.Message(MSG_ERROR,"Unknown class name '%s'", clsname.GetChars());
					delete this;
					return NULL;
				}
				// Since this happens in released WADs it must pass without a terminal error... :(
				ScriptPosition.Message(MSG_WARNING,
					"Unknown class name '%s'", 
					clsname.GetChars(), desttype->TypeName.GetChars());
			}
			else 
			{
				if (!cls->IsDescendantOf(desttype))
				{
					ScriptPosition.Message(MSG_ERROR,"class '%s' is not compatible with '%s'", clsname.GetChars(), desttype->TypeName.GetChars());
					delete this;
					return NULL;
				}
			}
			ScriptPosition.Message(MSG_DEBUG,"resolving '%s' as class name", clsname.GetChars());
		}
		FxExpression *x = new FxConstant(cls, ScriptPosition);
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

ExpVal FxClassTypeCast::EvalExpression (AActor *self)
{
	FName clsname = basex->EvalExpression(NULL).GetName();
	const PClass *cls = PClass::FindClass(clsname);

	if (!cls->IsDescendantOf(desttype))
	{
		Printf("class '%s' is not compatible with '%s'", clsname.GetChars(), desttype->TypeName.GetChars());
		cls = NULL;
	}

	ExpVal ret;
	ret.Type = VAL_Class;
	ret.pointer = (void*)cls;
	return ret;
}


//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxStateByIndex::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	if (ctx.cls->ActorInfo == NULL || ctx.cls->ActorInfo->NumOwnedStates == 0)
	{
		// This can't really happen
		assert(false);
	}
	if (ctx.cls->ActorInfo->NumOwnedStates <= index)
	{
		ScriptPosition.Message(MSG_ERROR, "%s: Attempt to jump to non existing state index %d", 
			ctx.cls->TypeName.GetChars(), index);
		delete this;
		return NULL;
	}
	FxExpression *x = new FxConstant(ctx.cls->ActorInfo->OwnedStates + index, ScriptPosition);
	delete this;
	return x;
}
	

//==========================================================================
//
//
//
//==========================================================================

FxMultiNameState::FxMultiNameState(const char *_statestring, const FScriptPosition &pos)
	:FxExpression(pos)
{
	FName scopename;
	FString statestring = _statestring;
	int scopeindex = statestring.IndexOf("::");

	if (scopeindex >= 0)
	{
		scopename = FName(statestring, scopeindex, false);
		statestring = statestring.Right(statestring.Len() - scopeindex - 2);
	}
	else
	{
		scopename = NULL;
	}
	names = MakeStateNameList(statestring);
	names.Insert(0, scopename);
	scope = NULL;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxMultiNameState::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	if (names[0] == NAME_None)
	{
		scope = NULL;
	}
	else if (names[0] == NAME_Super)
	{
		scope = ctx.cls->ParentClass;
	}
	else
	{
		scope = PClass::FindClass(names[0]);
		if (scope == NULL)
		{
			ScriptPosition.Message(MSG_ERROR, "Unknown class '%s' in state label", names[0].GetChars());
			delete this;
			return NULL;
		}
		else if (!scope->IsDescendantOf(ctx.cls))
		{
			ScriptPosition.Message(MSG_ERROR, "'%s' is not an ancestor of '%s'", names[0].GetChars(),ctx.cls->TypeName.GetChars());
			delete this;
			return NULL;
		}
	}
	if (scope != NULL)
	{
		FState *destination = NULL;
		// If the label is class specific we can resolve it right here
		if (names[1] != NAME_None)
		{
			if (scope->ActorInfo == NULL)
			{
				ScriptPosition.Message(MSG_ERROR, "'%s' has no actorinfo", names[0].GetChars());
				delete this;
				return NULL;
			}
			destination = scope->ActorInfo->FindState(names.Size()-1, &names[1], false);
			if (destination == NULL)
			{
				ScriptPosition.Message(ctx.lax? MSG_WARNING:MSG_ERROR, "Unknown state jump destination");
				if (!ctx.lax)
				{
					delete this;
					return NULL;
				}
				return this;
			}
		}
		FxExpression *x = new FxConstant(destination, ScriptPosition);
		delete this;
		return x;
	}
	names.Delete(0);
	names.ShrinkToFit();
	ValueType = VAL_State;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpVal FxMultiNameState::EvalExpression (AActor *self)
{
	ExpVal ret;
	ret.Type = VAL_State;
	ret.pointer = self->GetClass()->ActorInfo->FindState(names.Size(), &names[0]);
	if (ret.pointer == NULL)
	{
		const char *dot="";
		Printf("Jump target '");
		for (unsigned int i=0;i<names.Size();i++)
		{
			Printf("%s%s", dot, names[i].GetChars());
			dot = ".";
		}
		Printf("' not found in %s\n", self->GetClass()->TypeName.GetChars());
	}
	return ret;
}



//==========================================================================
//
// NOTE: I don't expect any of the following to survive Doomscript ;)
//
//==========================================================================

FStateExpressions StateParams;


//==========================================================================
//
//
//
//==========================================================================

void FStateExpressions::Clear()
{
	for(unsigned i=0; i<Size(); i++)
	{
		if (expressions[i].expr != NULL && !expressions[i].cloned)
		{
			delete expressions[i].expr;
		}
	}
	expressions.Clear();
}

//==========================================================================
//
//
//
//==========================================================================

int FStateExpressions::Add(FxExpression *x, const PClass *o, bool c)
{
	int idx = expressions.Reserve(1);
	FStateExpression &exp = expressions[idx];
	exp.expr = x;
	exp.owner = o;
	exp.constant = c;
	exp.cloned = false;
	return idx;
}

//==========================================================================
//
//
//
//==========================================================================

int FStateExpressions::Reserve(int num, const PClass *cls)
{
	int idx = expressions.Reserve(num);
	FStateExpression *exp = &expressions[idx];
	for(int i=0; i<num; i++)
	{
		exp[i].expr = NULL;
		exp[i].owner = cls;
		exp[i].constant = false;
		exp[i].cloned = false;
	}
	return idx;
}

//==========================================================================
//
//
//
//==========================================================================

void FStateExpressions::Set(int num, FxExpression *x, bool cloned)
{
	if (num >= 0 && num < int(Size()))
	{
		assert(expressions[num].expr == NULL || expressions[num].cloned);
		expressions[num].expr = x;
		expressions[num].cloned = cloned;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FStateExpressions::Copy(int dest, int src, int cnt)
{
	for(int i=0; i<cnt; i++)
	{
		// For now set only a reference because these expressions may change when being resolved
		expressions[dest+i].expr = (FxExpression*)intptr_t(src+i);
		expressions[dest+i].cloned = true;
	}
}

//==========================================================================
//
//
//
//==========================================================================

int FStateExpressions::ResolveAll()
{
	int errorcount = 0;

	FCompileContext ctx;
	ctx.lax = true;
	for(unsigned i=0; i<Size(); i++)
	{
		if (expressions[i].cloned)
		{
			// Now that everything coming before has been resolved we may copy the actual pointer.
			unsigned ii = unsigned((intptr_t)expressions[i].expr);
			expressions[i].expr = expressions[ii].expr;
		}
		else if (expressions[i].expr != NULL)
		{
			ctx.cls = expressions[i].owner;
			ctx.isconst = expressions[i].constant;
			expressions[i].expr = expressions[i].expr->Resolve(ctx);
			if (expressions[i].expr == NULL)
			{
				errorcount++;
			}
			else if (expressions[i].constant && !expressions[i].expr->isConstant())
			{
				expressions[i].expr->ScriptPosition.Message(MSG_ERROR, "Constant expression expected");
				errorcount++;
			}
		}
	}

	for(unsigned i=0; i<Size(); i++)
	{
		if (expressions[i].expr != NULL)
		{
			if (!expressions[i].expr->isresolved)
			{
				expressions[i].expr->ScriptPosition.Message(MSG_ERROR, "Expression at index %d not resolved\n", i);
				errorcount++;
			}
		}
	}

	return errorcount;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FStateExpressions::Get(int num)
{
	if (num >= 0 && num < int(Size()))
		return expressions[num].expr;
	return NULL;
}

