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

#include <malloc.h>
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
#include "vmbuilder.h"

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
DEFINE_MEMBER_VARIABLE(x, AActor)
DEFINE_MEMBER_VARIABLE(y, AActor)
DEFINE_MEMBER_VARIABLE(z, AActor)
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

ExpEmit::ExpEmit(VMFunctionBuilder *build, int type)
: RegNum(build->Registers[type].Get(1)), RegType(type), Konst(false), Fixed(false)
{
}

void ExpEmit::Free(VMFunctionBuilder *build)
{
	if (!Fixed && !Konst)
	{
		build->Registers[RegType].Return(RegNum, 1);
	}
}

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

ExpEmit FxExpression::Emit (VMFunctionBuilder *build)
{
	ScriptPosition.Message(MSG_ERROR, "Unemitted expression found");
	return ExpEmit();
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

FxParameter::FxParameter(FxExpression *operand)
: FxExpression(operand->ScriptPosition)
{
	Operand = operand;
	ValueType = operand->ValueType;
}

//==========================================================================
//
//
//
//==========================================================================

FxParameter::~FxParameter()
{
	SAFE_DELETE(Operand);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxParameter::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Operand, ctx);
	ValueType = Operand->ValueType;
	return this;
}

static void EmitConstantInt(VMFunctionBuilder *build, int val)
{
	// If it fits in 24 bits, use PARAMI instead of PARAM.
	if (((val << 8) >> 8) == val)
	{
		build->Emit(OP_PARAMI, val);
	}
	else
	{
		build->Emit(OP_PARAM, 0, REGT_INT | REGT_KONST, build->GetConstantInt(val));
	}
}

ExpEmit FxParameter::Emit(VMFunctionBuilder *build)
{
	if (Operand->isConstant())
	{
		ExpVal val = Operand->EvalExpression(NULL);
		if (val.Type == VAL_Int || val.Type == VAL_Sound || val.Type == VAL_Name || val.Type == VAL_Color)
		{
			EmitConstantInt(build, val.Int);
		}
		else if (val.Type == VAL_Float)
		{
			build->Emit(OP_PARAM, 0, REGT_FLOAT | REGT_KONST, build->GetConstantFloat(val.Float));
		}
		else if (val.Type == VAL_Class || val.Type == VAL_Object)
		{
			build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(val.pointer, ATAG_OBJECT));
		}
		else if (val.Type == VAL_State)
		{
			build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(val.pointer, ATAG_STATE));
		}
		else if (val.Type == VAL_String)
		{
			build->Emit(OP_PARAM, 0, REGT_STRING | REGT_KONST, build->GetConstantString(val.GetString()));
		}
		else
		{
			build->Emit(OP_PARAM, 0, REGT_NIL, 0);
			ScriptPosition.Message(MSG_ERROR, "Cannot emit needed constant");
		}
	}
	else
	{
		ExpEmit where = Operand->Emit(build);

		if (where.RegType == REGT_NIL)
		{
			ScriptPosition.Message(MSG_ERROR, "Attempted to pass a non-value");
			build->Emit(OP_PARAM, 0, where.RegType, where.RegNum);
		}
		else
		{
			build->Emit(OP_PARAM, 0, where.RegType, where.RegNum);
			where.Free(build);
		}
	}
	return ExpEmit();
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
	PSymbolConst *csym = dyn_cast<PSymbolConst>(sym);
	if (csym != NULL)
	{
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

ExpEmit FxConstant::Emit(VMFunctionBuilder *build)
{
	ExpEmit out;

	out.Konst = true;
	if (value.Type == VAL_Int || value.Type == VAL_Sound || value.Type == VAL_Name || value.Type == VAL_Color)
	{
		out.RegType = REGT_INT;
		out.RegNum = build->GetConstantInt(value.Int);
	}
	else if (value.Type == VAL_Float)
	{
		out.RegType = REGT_FLOAT;
		out.RegNum = build->GetConstantFloat(value.Float);
	}
	else if (value.Type == VAL_Class || value.Type == VAL_Object)
	{
		out.RegType = REGT_POINTER;
		out.RegNum = build->GetConstantAddress(value.pointer, ATAG_OBJECT);
	}
	else if (value.Type == VAL_State)
	{
		out.RegType = REGT_POINTER;
		out.RegNum = build->GetConstantAddress(value.pointer, ATAG_STATE);
	}
	else if (value.Type == VAL_String)
	{
		out.RegType = REGT_STRING;
		out.RegNum = build->GetConstantString(value.GetString());
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Cannot emit needed constant");
		out.RegType = REGT_NIL;
		out.RegNum = 0;
	}
	return out;
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

ExpEmit FxIntCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = basex->Emit(build);
	assert(!from.Konst);
	assert(basex->ValueType == VAL_Float);
	ExpEmit to(build, REGT_INT);
	build->Emit(OP_CAST, to.RegNum, from.RegNum, CAST_F2I);
	return to;
}

//==========================================================================
//
//
//
//==========================================================================

FxFloatCast::FxFloatCast(FxExpression *x)
: FxExpression(x->ScriptPosition)
{
	basex=x;
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

ExpEmit FxFloatCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = basex->Emit(build);
	assert(!from.Konst);
	assert(basex->ValueType == VAL_Int);
	from.Free(build);
	ExpEmit to(build, REGT_FLOAT);
	build->Emit(OP_CAST, to.RegNum, from.RegNum, CAST_I2F);
	return to;
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

ExpEmit FxPlusSign::Emit(VMFunctionBuilder *build)
{
	return Operand->Emit(build);
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

ExpEmit FxMinusSign::Emit(VMFunctionBuilder *build)
{
	assert(ValueType.Type == Operand->ValueType.Type);
	ExpEmit from = Operand->Emit(build);
	assert(from.Konst != 0);
	// Do it in-place.
	if (ValueType == VAL_Int)
	{
		build->Emit(OP_NEG, from.RegNum, from.RegNum, 0);
	}
	else
	{
		assert(ValueType == VAL_Float);
		build->Emit(OP_NEG, from.RegNum, from.RegNum, 0);
	}
	return from;
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

ExpEmit FxUnaryNotBitwise::Emit(VMFunctionBuilder *build)
{
	assert(ValueType.Type == Operand->ValueType.Type);
	assert(ValueType == VAL_Int);
	ExpEmit from = Operand->Emit(build);
	assert(from.Konst != 0);
	// Do it in-place.
	build->Emit(OP_NOT, from.RegNum, from.RegNum, 0);
	return from;
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

ExpEmit FxUnaryNotBoolean::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = Operand->Emit(build);
	assert(!from.Konst);
	ExpEmit to(build, REGT_INT);
	from.Free(build);

	// Preload result with 0.
	build->Emit(OP_LI, to.RegNum, 0, 0);

	// Check source against 0.
	if (from.RegType == REGT_INT)
	{
		build->Emit(OP_EQ_R, 0, from.RegNum, to.RegNum);
	}
	else if (from.RegType == REGT_FLOAT)
	{
		build->Emit(OP_EQF_K, 0, from.RegNum, build->GetConstantFloat(0));
	}
	else if (from.RegNum == REGT_POINTER)
	{
		build->Emit(OP_EQA_K, 0, from.RegNum, build->GetConstantAddress(NULL, ATAG_GENERIC));
	}
	build->Emit(OP_JMP, 1);

	// Reload result with 1 if the comparison fell through.
	build->Emit(OP_LI, to.RegNum, 1);
	return to;
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

void FxBinary::Promote(FCompileContext &ctx)
{
	if (left->ValueType == VAL_Float && right->ValueType == VAL_Int)
	{
		right = (new FxFloatCast(right))->Resolve(ctx);
	}
	else if (left->ValueType == VAL_Int && right->ValueType == VAL_Float)
	{
		left = (new FxFloatCast(left))->Resolve(ctx);
	}
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
	Promote(ctx);
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

ExpEmit FxAddSub::Emit(VMFunctionBuilder *build)
{
	assert(Operator == '+' || Operator == '-');
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);
	if (Operator == '+')
	{
		// Since addition is commutative, only the second operand may be a constant.
		if (op1.Konst)
		{
			swapvalues(op1, op2);
		}
		assert(!op1.Konst);
		op1.Free(build);
		op2.Free(build);
		if (ValueType == VAL_Float)
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			ExpEmit to(build, REGT_FLOAT);
			build->Emit(op2.Konst ? OP_ADDF_RK : OP_ADDF_RR, to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
		else
		{
			assert(ValueType == VAL_Int);
			assert(op1.RegType == REGT_INT && op2.RegType == REGT_INT);
			ExpEmit to(build, REGT_INT);
			build->Emit(op2.Konst ? OP_ADD_RK : OP_ADD_RR, to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
	}
	else
	{
		// Subtraction is not commutative, so either side may be constant (but not both).
		assert(!op1.Konst || !op2.Konst);
		op1.Free(build);
		op2.Free(build);
		if (ValueType == VAL_Float)
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			ExpEmit to(build, REGT_FLOAT);
			build->Emit(op1.Konst ? OP_SUBF_KR : op2.Konst ? OP_SUBF_RK : OP_SUBF_RR,
				to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
		else
		{
			assert(ValueType == VAL_Int);
			assert(op1.RegType == REGT_INT && op2.RegType == REGT_INT);
			ExpEmit to(build, REGT_INT);
			build->Emit(op1.Konst ? OP_SUB_KR : op2.Konst ? OP_SUB_RK : OP_SUB_RR,
				to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
	}
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
	Promote(ctx);
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

ExpEmit FxMulDiv::Emit(VMFunctionBuilder *build)
{
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);

	if (Operator == '*')
	{
		// Multiplication is commutative, so only the second operand may be constant.
		if (op1.Konst)
		{
			swapvalues(op1, op2);
		}
		assert(!op1.Konst);
		op1.Free(build);
		op2.Free(build);
		if (ValueType == VAL_Float)
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			ExpEmit to(build, REGT_FLOAT);
			build->Emit(op2.Konst ? OP_MULF_RK : OP_MULF_RR, to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
		else
		{
			assert(ValueType == VAL_Int);
			assert(op1.RegType == REGT_INT && op2.RegType == REGT_INT);
			ExpEmit to(build, REGT_INT);
			build->Emit(op2.Konst ? OP_MUL_RK : OP_MUL_RR, to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
	}
	else
	{
		// Division is not commutative, so either side may be constant (but not both).
		assert(!op1.Konst || !op2.Konst);
		assert(Operator == '%' || Operator == '/');
		op1.Free(build);
		op2.Free(build);
		if (ValueType == VAL_Float)
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			ExpEmit to(build, REGT_FLOAT);
			build->Emit(Operator == '/' ? (op1.Konst ? OP_DIVF_KR : op2.Konst ? OP_DIVF_RK : OP_DIVF_RR)
				: (op1.Konst ? OP_MODF_KR : op2.Konst ? OP_MODF_RK : OP_MODF_RR),
				to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
		else
		{
			assert(ValueType == VAL_Int);
			assert(op1.RegType == REGT_INT && op2.RegType == REGT_INT);
			ExpEmit to(build, REGT_INT);
			build->Emit(Operator == '/' ? (op1.Konst ? OP_DIV_KR : op2.Konst ? OP_DIV_RK : OP_DIV_RR)
				: (op1.Konst ? OP_MOD_KR : op2.Konst ? OP_MOD_RK : OP_MOD_RR),
				to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
	}
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
	Promote(ctx);
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

ExpEmit FxCompareRel::Emit(VMFunctionBuilder *build)
{
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);
	assert(op1.RegType == op2.RegType);
	assert(op1.RegType == REGT_INT || op1.RegType == REGT_FLOAT);
	assert(!op1.Konst || !op2.Konst);
	assert(Operator == '<' || Operator == '>' || Operator == TK_Geq || Operator == TK_Leq);
	static const VM_UBYTE InstrMap[][4] =
	{
		{ OP_LT_RR, OP_LTF_RR, 0 },	// <
		{ OP_LE_RR, OP_LEF_RR, 1 },	// >
		{ OP_LT_RR, OP_LTF_RR, 1 },	// >=
		{ OP_LE_RR, OP_LE_RR, 0 }	// <=
	};
	int instr, check, index;

	index = Operator == '<' ? 0 :
			Operator == '>' ? 1 :
			Operator == TK_Geq ? 2 : 3;
	instr = InstrMap[index][op1.RegType == REGT_INT ? 0 : 1];
	check = InstrMap[index][2];
	if (op2.Konst)
	{
		instr += 1;
	}
	else
	{
		op2.Free(build);
	}
	if (op1.Konst)
	{
		instr += 2;
	}
	else
	{
		op1.Free(build);
	}
	ExpEmit to(build, op1.RegType);

	// See FxUnaryNotBoolean for comments, since it's the same thing.
	build->Emit(OP_LI, to.RegNum, 0, 0);
	build->Emit(instr, check, op1.RegNum, op2.RegNum);
	build->Emit(OP_JMP, 1);
	build->Emit(OP_LI, to.RegNum, 1);
	return to;
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
	Promote(ctx);
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
	else if (left->ValueType == VAL_Int)
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

ExpEmit FxCompareEq::Emit(VMFunctionBuilder *build)
{
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);
	assert(op1.RegType == op2.RegType);
	assert(op1.RegType == REGT_INT || op1.RegType == REGT_FLOAT || op1.RegType == REGT_POINTER);
	int instr;

	// Only the second operand may be constant.
	if (op1.Konst)
	{
		swapvalues(op1, op2);
	}
	assert(!op1.Konst);

	instr = op1.RegType == REGT_INT ? OP_EQ_R :
			op1.RegType == REGT_FLOAT ? OP_EQF_R :
			OP_EQA_R;
	op1.Free(build);
	if (!op2.Konst)
	{
		op2.Free(build);
	}
	else
	{
		instr += 1;
	}
	ExpEmit to(build, op1.RegType);

	// See FxUnaryNotBoolean for comments, since it's the same thing.
	build->Emit(OP_LI, to.RegNum, 0, 0);
	build->Emit(instr, Operator != TK_Eq, op1.RegNum, op2.RegNum);
	build->Emit(OP_JMP, 1);
	build->Emit(OP_LI, to.RegNum, 1);
	return to;
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

ExpEmit FxBinaryInt::Emit(VMFunctionBuilder *build)
{
	assert(left->ValueType == VAL_Int);
	assert(right->ValueType == VAL_Int);
	static const VM_UBYTE InstrMap[][4] =
	{
		{ OP_SLL_RR, OP_SLL_KR, OP_SLL_RI },	// TK_LShift
		{ OP_SRA_RR, OP_SRA_KR, OP_SRA_RI },	// TK_RShift
		{ OP_SRL_RR, OP_SRL_KR, OP_SRL_RI },	// TK_URShift
		{ OP_AND_RR, 0,         OP_AND_RK },	// '&'
		{ OP_OR_RR,  0,			OP_OR_RK  },	// '|'
		{ OP_XOR_RR, 0,         OP_XOR_RK },	// '^'
	};
	int index, instr, rop;
	ExpEmit op1, op2;

	index = Operator == TK_LShift ? 0 :
			Operator == TK_RShift ? 1 :
			Operator == TK_URShift ? 2 :
			Operator == '&' ? 3 :
			Operator == '|' ? 4 :
			Operator == '^' ? 5 : -1;
	assert(index >= 0);
	op1 = left->Emit(build);
	if (index < 3)
	{ // Shift instructions use right-hand immediates instead of constant registers.
		if (right->isConstant())
		{
			rop = right->EvalExpression(NULL).GetInt();
			op2.Konst = true;
		}
		else
		{
			op2 = right->Emit(build);
			assert(!op2.Konst);
			op2.Free(build);
			rop = op2.RegNum;
		}
	}
	else
	{ // The other operators only take a constant on the right-hand side.
		op2 = right->Emit(build);
		if (op1.Konst)
		{
			swapvalues(op1, op2);
		}
		assert(!op1.Konst);
		rop = op2.RegNum;
		op2.Free(build);
	}
	if (!op1.Konst)
	{
		op1.Free(build);
		if (!op2.Konst)
		{
			instr = InstrMap[index][0];
		}
		else
		{
			instr = InstrMap[index][2];
		}
	}
	else
	{
		assert(!op2.Konst);
		instr = InstrMap[index][1];
	}
	assert(instr != 0);
	ExpEmit to(build, REGT_INT);
	build->Emit(instr, to.RegNum, op1.RegNum, rop);
	return to;
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
	if (left->ValueType != VAL_Int && left->ValueType != VAL_Sound)
	{
		left = new FxIntCast(left);
	}
	if (right->ValueType != VAL_Int && right->ValueType != VAL_Sound)
	{
		right = new FxIntCast(right);
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

ExpEmit FxBinaryLogical::Emit(VMFunctionBuilder *build)
{
	// This is not the "right" way to do these, but it works for now.
	// (Problem: No information sharing is done between nodes to reduce the
	// code size if you have something like a1 && a2 && a3 && ... && an.)
	assert(left->ValueType == VAL_Int && right->ValueType == VAL_Int);
	ExpEmit op1 = left->Emit(build);
	assert(!op1.Konst);
	int zero = build->GetConstantInt(0);
	op1.Free(build);

	if (Operator == TK_AndAnd)
	{
		build->Emit(OP_EQ_K, 1, op1.RegNum, zero);
		// If op1 is 0, skip evaluation of op2.
		size_t patchspot = build->Emit(OP_JMP, 0, 0, 0);

		// Evaluate op2.
		ExpEmit op2 = right->Emit(build);
		assert(!op2.Konst);
		op2.Free(build);

		ExpEmit to(build, REGT_INT);
		build->Emit(OP_EQ_K, 0, op2.RegNum, zero);
		build->Emit(OP_JMP, 2);
		build->Emit(OP_LI, to.RegNum, 1);
		build->Emit(OP_JMP, 1);
		size_t target = build->Emit(OP_LI, to.RegNum, 0);
		build->Backpatch(patchspot, target);
		return to;
	}
	else
	{
		assert(Operator == TK_OrOr);
		build->Emit(OP_EQ_K, 0, op1.RegNum, zero);
		// If op1 is not 0, skip evaluation of op2.
		size_t patchspot = build->Emit(OP_JMP, 0, 0, 0);

		// Evaluate op2.
		ExpEmit op2 = right->Emit(build);
		assert(!op2.Konst);
		op2.Free(build);

		ExpEmit to(build, REGT_INT);
		build->Emit(OP_EQ_K, 1, op2.RegNum, zero);
		build->Emit(OP_JMP, 2);
		build->Emit(OP_LI, to.RegNum, 0);
		build->Emit(OP_JMP, 1);
		size_t target = build->Emit(OP_LI, to.RegNum, 1);
		build->Backpatch(patchspot, target);
		return to;
	}
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

	if (ValueType == VAL_Float)
	{
		if (truex->ValueType != VAL_Float)
		{
			truex = new FxFloatCast(truex);
			RESOLVE(truex, ctx);
		}
		if (falsex->ValueType != VAL_Float)
		{
			falsex = new FxFloatCast(falsex);
			RESOLVE(falsex, ctx);
		}
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

ExpEmit FxConditional::Emit(VMFunctionBuilder *build)
{
	ExpEmit out;

	// The true and false expressions ought to be assigned to the
	// same temporary instead of being copied to it. Oh well; good enough
	// for now.
	ExpEmit cond = condition->Emit(build);
	assert(cond.RegType == REGT_INT && !cond.Konst);

	// Test condition.
	build->Emit(OP_EQ_K, 1, cond.RegNum, build->GetConstantInt(0));
	size_t patchspot = build->Emit(OP_JMP, 0);

	// Evaluate true expression.
	if (truex->isConstant() && truex->ValueType == VAL_Int)
	{
		out = ExpEmit(build, REGT_INT);
		build->EmitLoadInt(out.RegNum, truex->EvalExpression(NULL).GetInt());
	}
	else
	{
		ExpEmit trueop = truex->Emit(build);
		if (trueop.Konst)
		{
			assert(trueop.RegType == REGT_FLOAT);
			out = ExpEmit(build, REGT_FLOAT);
			build->Emit(OP_LKF, out.RegNum, trueop.RegNum);
		}
		else
		{
			// Use the register returned by the true condition as the
			// target for the false condition.
			out = trueop;
		}
	}

	// Evaluate false expression.
	build->BackpatchToHere(patchspot);
	if (falsex->isConstant() && falsex->ValueType == VAL_Int)
	{
		build->EmitLoadInt(out.RegNum, falsex->EvalExpression(NULL).GetInt());
	}
	else
	{
		ExpEmit falseop = falsex->Emit(build);
		if (falseop.Konst)
		{
			assert(falseop.RegType == REGT_FLOAT);
			build->Emit(OP_LKF, out.RegNum, falseop.RegNum);
		}
		else
		{
			// Move result from the register returned by "false" to the one
			// returned by "true" so that only one register is returned by
			// this tree.
			falseop.Free(build);
			if (falseop.RegType == REGT_INT)
			{
				build->Emit(OP_MOVE, out.RegNum, falseop.RegNum, 0);
			}
			else
			{
				assert(falseop.RegType == REGT_FLOAT);
				build->Emit(OP_MOVEF, out.RegNum, falseop.RegNum, 0);
			}
		}
	}

	return out;
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

ExpEmit FxAbs::Emit(VMFunctionBuilder *build)
{
	ExpEmit absofsteal = val->Emit(build);
	assert(!absofsteal.Konst);
	ExpEmit out(build, absofsteal.RegType);
	if (absofsteal.RegType == REGT_INT)
	{
		build->Emit(OP_ABS, out.RegNum, absofsteal.RegNum, 0);
	}
	else
	{
		assert(absofsteal.RegType == REGT_FLOAT);
		build->Emit(OP_FLOP, out.RegNum, absofsteal.RegNum, FLOP_ABS);
	}
	return out;
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
		min = new FxParameter(new FxIntCast(mi));
		max = new FxParameter(new FxIntCast(ma));
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
		assert(min->ValueType == ValueType.Type);
		assert(max->ValueType == ValueType.Type);
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

int DecoRandom(VMFrameStack *stack, VMValue *param, int numparam, VMReturn *ret, int numret)
{
	assert(numparam >= 1 && numparam <= 3);
	FRandom *rng = reinterpret_cast<FRandom *>(param[0].a);
	if (numparam == 1)
	{
		ret->SetInt((*rng)());
	}
	else if (numparam == 2)
	{
		int maskval = param[1].i;
		ret->SetInt(rng->Random2(maskval));
	}
	else if (numparam == 3)
	{
		int min = param[1].i, max = param[2].i;
		if (max < min)
		{
			swapvalues(max, min);
		}
		ret->SetInt((*rng)(max - min + 1) + min);
	}
	return 1;
}

ExpEmit FxRandom::Emit(VMFunctionBuilder *build)
{
	// Find the DecoRandom function. If not found, create it and install it
	// in Actor.
	VMFunction *callfunc;
	PSymbol *sym = RUNTIME_CLASS(AActor)->Symbols.FindSymbol("DecoRandom", false);
	if (sym == NULL)
	{
		PSymbolVMFunction *symfunc = new PSymbolVMFunction("DecoRandom");
		VMNativeFunction *calldec = new VMNativeFunction(DecoRandom);
		symfunc->Function = calldec;
		sym = symfunc;
		RUNTIME_CLASS(AActor)->Symbols.AddSymbol(sym);
	}
	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != NULL);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(rng, ATAG_RNG));
	if (min != NULL && max != NULL)
	{
		min->Emit(build);
		max->Emit(build);
		build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 3, 1);
	}
	else
	{
		build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 1, 1);
	}
	ExpEmit out(build, REGT_INT);
	build->Emit(OP_RESULT, 0, REGT_INT, out.RegNum);
	return out;
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
		min = new FxParameter(new FxFloatCast(mi));
		max = new FxParameter(new FxFloatCast(ma));
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

int DecoFRandom(VMFrameStack *stack, VMValue *param, int numparam, VMReturn *ret, int numret)
{
	assert(numparam == 1 || numparam == 3);
	FRandom *rng = reinterpret_cast<FRandom *>(param[0].a);

	int random = (*rng)(0x40000000);
	double frandom = random / double(0x40000000);

	if (numparam == 3)
	{
		double min = param[1].f, max = param[2].f;
		if (max < min)
		{
			swapvalues(max, min);
		}
		ret->SetFloat(frandom * (max - min) + min);
	}
	else
	{
		ret->SetFloat(frandom);
	}
	return 1;
}

ExpEmit FxFRandom::Emit(VMFunctionBuilder *build)
{
	// Find the DecoFRandom function. If not found, create it and install it
	// in Actor.
	VMFunction *callfunc;
	PSymbol *sym = RUNTIME_CLASS(AActor)->Symbols.FindSymbol("DecoFRandom", false);
	if (sym == NULL)
	{
		PSymbolVMFunction *symfunc = new PSymbolVMFunction("DecoFRandom");
		VMNativeFunction *calldec = new VMNativeFunction(DecoFRandom);
		symfunc->Function = calldec;
		sym = symfunc;
		RUNTIME_CLASS(AActor)->Symbols.AddSymbol(sym);
	}
	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != NULL);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(rng, ATAG_RNG));
	if (min != NULL && max != NULL)
	{
		min->Emit(build);
		max->Emit(build);
		build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 3, 1);
	}
	else
	{
		build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 1, 1);
	}
	ExpEmit out(build, REGT_FLOAT);
	build->Emit(OP_RESULT, 0, REGT_FLOAT, out.RegNum);
	return out;
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
	mask = new FxParameter(mask);
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

ExpEmit FxRandom2::Emit(VMFunctionBuilder *build)
{
	// Find the DecoRandom function. If not found, create it and install it
	// in Actor.
	VMFunction *callfunc;
	PSymbol *sym = RUNTIME_CLASS(AActor)->Symbols.FindSymbol("DecoRandom", false);
	if (sym == NULL)
	{
		PSymbolVMFunction *symfunc = new PSymbolVMFunction("DecoRandom");
		VMNativeFunction *calldec = new VMNativeFunction(DecoRandom);
		symfunc->Function = calldec;
		sym = symfunc;
		RUNTIME_CLASS(AActor)->Symbols.AddSymbol(sym);
	}
	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != NULL);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(rng, ATAG_RNG));
	mask->Emit(build);
	build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 2, 1);
	ExpEmit out(build, REGT_INT);
	build->Emit(OP_RESULT, 0, REGT_INT, out.RegNum);
	return out;
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
		if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConst)))
		{
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as class constant\n", Identifier.GetChars());
			newex = FxConstant::MakeConstant(sym, ScriptPosition);
		}
		else if (sym->IsKindOf(RUNTIME_CLASS(PSymbolVariable)))
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
		if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConst)))
		{
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as global constant\n", Identifier.GetChars());
			newex = FxConstant::MakeConstant(sym, ScriptPosition);
		}
		else if (sym->IsKindOf(RUNTIME_CLASS(PSymbolVariable)))	// global variables will always be native
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

ExpEmit FxSelf::Emit(VMFunctionBuilder *build)
{
	// self is always the first pointer passed to the function;
	ExpEmit me(0, REGT_POINTER);
	me.Fixed = true;
	return me;
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

ExpEmit FxClassMember::Emit(VMFunctionBuilder *build)
{
	ExpEmit obj = classx->Emit(build);
	assert(obj.RegType == REGT_POINTER);

	if (AddressRequested)
	{
		if (membervar->offset == 0)
		{
			return obj;
		}
		obj.Free(build);
		ExpEmit out(build, REGT_POINTER);
		build->Emit(OP_ADDA_RK, out.RegNum, obj.RegNum, build->GetConstantInt((int)membervar->offset));
		return out;
	}

	int offsetreg = build->GetConstantInt((int)membervar->offset);
	ExpEmit loc, tmp;

	if (obj.Konst)
	{
		// If the situation where we are dereferencing a constant
		// pointer is common, then it would probably be worthwhile
		// to add new opcodes for those. But as of right now, I
		// don't expect it to be a particularly common case.
		ExpEmit newobj(build, REGT_POINTER);
		build->Emit(OP_LKP, newobj.RegNum, obj.RegNum);
		obj = newobj;
	}

	switch (membervar->ValueType.Type)
	{
	case VAL_Int:
	case VAL_Sound:
	case VAL_Name:
	case VAL_Color:
		loc = ExpEmit(build, REGT_INT);
		build->Emit(OP_LW, loc.RegNum, obj.RegNum, offsetreg);
		break;

	case VAL_Bool:
		loc = ExpEmit(build, REGT_INT);
		// Some implementations have 1 byte bools, and others have
		// 4 byte bools. For all I know, there might be some with
		// 2 byte bools, too.
		build->Emit((sizeof(bool) == 1 ? OP_LBU : sizeof(bool) == 2 ? OP_LHU : OP_LW),
			loc.RegNum, obj.RegNum, offsetreg);
		break;

	case VAL_Float:
		loc = ExpEmit(build, REGT_FLOAT);
		build->Emit(OP_LDP, loc.RegNum, obj.RegNum, offsetreg);
		break;

	case VAL_Fixed:
		loc = ExpEmit(build, REGT_FLOAT);
		build->Emit(OP_LX, loc.RegNum, obj.RegNum, offsetreg);
		break;

	case VAL_Angle:
		loc = ExpEmit(build, REGT_FLOAT);
		tmp = ExpEmit(build, REGT_INT);
		build->Emit(OP_LW, tmp.RegNum, obj.RegNum, offsetreg);
		build->Emit(OP_CAST, loc.RegNum, tmp.RegNum, CAST_I2F);
		build->Emit(OP_MULF_RK, loc.RegNum, loc.RegNum, build->GetConstantFloat(90.0 / ANGLE_90));
		tmp.Free(build);
		break;

	case VAL_Object:
	case VAL_Class:
		loc = ExpEmit(build, REGT_POINTER);
		build->Emit(OP_LO, loc.RegNum, obj.RegNum, offsetreg);
		break;

	default:
		assert(0);
	}
	obj.Free(build);
	return loc;
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

ExpEmit FxArrayElement::Emit(VMFunctionBuilder *build)
{
	ExpEmit start = Array->Emit(build);
	ExpEmit dest(build, REGT_INT);
	if (start.Konst)
	{
		ExpEmit tmpstart(build, REGT_POINTER);
		build->Emit(OP_LKP, tmpstart.RegNum, start.RegNum);
		start = tmpstart;
	}
	if (index->isConstant())
	{
		int indexval = index->EvalExpression(NULL).GetInt();
		if (indexval < 0 || indexval >= Array->ValueType.size)
		{
			I_Error("Array index out of bounds");
		}
		indexval <<= 2;
		build->Emit(OP_LW, dest.RegNum, start.RegNum, build->GetConstantInt(indexval));
	}
	else
	{
		ExpEmit indexv(index->Emit(build));
		build->Emit(OP_SLL_RI, indexv.RegNum, indexv.RegNum, 2);
		build->Emit(OP_BOUND, indexv.RegNum, Array->ValueType.size);
		build->Emit(OP_LW_R, dest.RegNum, start.RegNum, indexv.RegNum);
		indexv.Free(build);
	}
	start.Free(build);
	return dest;
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
	// There's currently only 3 global functions.
	// If this changes later, it won't be here!
	if (MethodName == NAME_Sin || MethodName == NAME_Cos || MethodName == NAME_Sqrt)
	{
		if (Self != NULL)
		{
			ScriptPosition.Message(MSG_ERROR, "Global functions cannot have a self pointer");
			delete this;
			return NULL;
		}
		FxExpression *x = new FxGlobalFunctionCall(MethodName, ArgList, ScriptPosition);
		ArgList = NULL;
		delete this;
		return x->Resolve(ctx);
	}

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

int DecoCallLineSpecial(VMFrameStack *stack, VMValue *param, int numparam, VMReturn *ret, int numret)
{
	assert(numparam > 2 && numparam < 7);
	assert(numret == 1);
	assert(param[0].Type == REGT_INT);
	assert(param[1].Type == REGT_POINTER);
	int v[5] = { 0 };

	for (int i = 2; i < numparam; ++i)
	{
		v[i - 2] = param[i].i;
	}
	ret->SetInt(LineSpecials[param[0].i](NULL, reinterpret_cast<AActor*>(param[1].a), false, v[0], v[1], v[2], v[3], v[4]));
	return 1;
}

ExpEmit FxActionSpecialCall::Emit(VMFunctionBuilder *build)
{
	assert(Self == NULL);
	unsigned i = 0;

	build->Emit(OP_PARAMI, Special);				// pass special number
	build->Emit(OP_PARAM, 0, REGT_POINTER, 0);		// pass self
	if (ArgList != NULL)
	{
		for (; i < ArgList->Size(); ++i)
		{
			FxExpression *argex = (*ArgList)[i];
			assert(argex->ValueType == VAL_Int);
			if (argex->isConstant())
			{
				EmitConstantInt(build, argex->EvalExpression(NULL).GetInt());
			}
			else
			{
				ExpEmit arg(argex->Emit(build));
				build->Emit(OP_PARAM, 0, arg.RegType, arg.RegNum);
				arg.Free(build);
			}
		}
	}
	// Find the DecoCallLineSpecial function. If not found, create it and install it
	// in Actor.
	VMFunction *callfunc;
	PSymbol *sym = RUNTIME_CLASS(AActor)->Symbols.FindSymbol("DecoCallLineSpecial", false);
	if (sym == NULL)
	{
		PSymbolVMFunction *symfunc = new PSymbolVMFunction("DecoCallLineSpecial");
		VMNativeFunction *calldec = new VMNativeFunction(DecoCallLineSpecial);
		symfunc->Function = calldec;
		sym = symfunc;
		RUNTIME_CLASS(AActor)->Symbols.AddSymbol(sym);
	}
	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != NULL);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	ExpEmit dest(build, REGT_INT);
	build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 2 + i, 1);
	build->Emit(OP_RESULT, 0, REGT_INT, dest.RegNum);
	return dest;
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

FxExpression *FxGlobalFunctionCall::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();

	if (ArgList == NULL || ArgList->Size() != 1)
	{
		ScriptPosition.Message(MSG_ERROR, "%s only has one parameter", Name.GetChars());
		delete this;
		return NULL;
	}

	(*ArgList)[0] = (*ArgList)[0]->Resolve(ctx);
	if ((*ArgList)[0] == NULL)
	{
		delete this;
		return NULL;
	}

	if (!(*ArgList)[0]->ValueType.isNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "numeric value expected for parameter");
		delete this;
		return NULL;
	}
	if ((*ArgList)[0]->isConstant())
	{
		double v = (*ArgList)[0]->EvalExpression(NULL).GetFloat();
		if (Name == NAME_Sqrt)
		{
			v = sqrt(v);
		}
		else
		{
			v *= M_PI / 180.0;		// convert from degrees to radians
			v = (Name == NAME_Sin) ? sin(v) : cos(v);
		}
		FxExpression *x = new FxConstant(v, ScriptPosition);
		delete this;
		return x;
	}
	if ((*ArgList)[0]->ValueType == VAL_Int)
	{
		(*ArgList)[0] = new FxFloatCast((*ArgList)[0]);
	}
	ValueType = VAL_Float;
	return this;
}

//==========================================================================
//
//
//==========================================================================

ExpVal FxGlobalFunctionCall::EvalExpression (AActor *self)
{
	double v = (*ArgList)[0]->EvalExpression(self).GetFloat();
	ExpVal ret;
	ret.Type = VAL_Float;

	if (Name == NAME_Sqrt)
	{
		ret.Float = sqrt(v);
	}
	else
	{
		v *= M_PI / 180.0;		// convert from degrees to radians
		ret.Float = (Name == NAME_Sin) ? sin(v) : cos(v);
	}
	return ret;
}

ExpEmit FxGlobalFunctionCall::Emit(VMFunctionBuilder *build)
{
	ExpEmit v = (*ArgList)[0]->Emit(build);
	assert(!v.Konst && v.RegType == REGT_FLOAT);

	build->Emit(OP_MULF_RK, v.RegNum, v.RegNum, build->GetConstantFloat(M_PI / 180.0));
	build->Emit(OP_FLOP, v.RegNum, v.RegNum,
		(Name == NAME_Sqrt) ?	FLOP_SQRT :
		(Name == NAME_Sin) ?	FLOP_SIN :
								FLOP_COS);
	return v;
}

//==========================================================================
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

		if (clsname != NAME_None || !ctx.isconst)
		{
			cls= PClass::FindClass(clsname);
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

int DecoNameToClass(VMFrameStack *stack, VMValue *param, int numparam, VMReturn *ret, int numret)
{
	assert(numparam == 2);
	assert(numret == 1);
	assert(param[0].Type == REGT_INT);
	assert(param[1].Type == REGT_POINTER);
	assert(ret->RegType == REGT_POINTER);

	FName clsname = ENamedName(param[0].i);
	const PClass *cls = PClass::FindClass(clsname);
	const PClass *desttype = reinterpret_cast<PClass *>(param[0].a);

	if (!cls->IsDescendantOf(desttype))
	{
		Printf("class '%s' is not compatible with '%s'", clsname.GetChars(), desttype->TypeName.GetChars());
		cls = NULL;
	}
	ret->SetPointer(const_cast<PClass *>(cls), ATAG_OBJECT);
	return 1;
}

ExpEmit FxClassTypeCast::Emit(VMFunctionBuilder *build)
{
	if (basex->ValueType != VAL_Name)
	{
		return ExpEmit(build->GetConstantAddress(NULL, ATAG_OBJECT), REGT_POINTER, true);
	}
	ExpEmit clsname = basex->Emit(build);
	assert(!clsname.Konst);
	ExpEmit dest(build, REGT_POINTER);
	build->Emit(OP_PARAM, 0, clsname.RegType, clsname.RegNum);
	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(const_cast<PClass *>(desttype), ATAG_OBJECT));

	// Find the DecoNameToClass function. If not found, create it and install it
	// in Actor.
	VMFunction *callfunc;
	PSymbol *sym = RUNTIME_CLASS(AActor)->Symbols.FindSymbol("DecoNameToClass", false);
	if (sym == NULL)
	{
		PSymbolVMFunction *symfunc = new PSymbolVMFunction("DecoNameToClass");
		VMNativeFunction *calldec = new VMNativeFunction(DecoNameToClass);
		symfunc->Function = calldec;
		sym = symfunc;
		RUNTIME_CLASS(AActor)->Symbols.AddSymbol(sym);
	}
	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != NULL);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 2, 1);
	build->Emit(OP_RESULT, 0, REGT_POINTER, dest.RegNum);
	clsname.Free(build);
	return dest;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxStateByIndex::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	if (ctx.cls->NumOwnedStates == 0)
	{
		// This can't really happen
		assert(false);
	}
	if (ctx.cls->NumOwnedStates <= index)
	{
		ScriptPosition.Message(MSG_ERROR, "%s: Attempt to jump to non existing state index %d", 
			ctx.cls->TypeName.GetChars(), index);
		delete this;
		return NULL;
	}
	FxExpression *x = new FxConstant(ctx.cls->OwnedStates + index, ScriptPosition);
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
		scope = dyn_cast<PClassActor>(ctx.cls->ParentClass);
	}
	else
	{
		scope = PClass::FindActor(names[0]);
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
			destination = scope->FindState(names.Size()-1, &names[1], false);
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
	ret.pointer = self->GetClass()->FindState(names.Size(), &names[0]);
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

int DecoFindMultiNameState(VMFrameStack *stack, VMValue *param, int numparam, VMReturn *ret, int numret)
{
	assert(numparam > 1);
	assert(numret == 1);
	assert(ret->RegType == REGT_POINTER);

	FName *names = (FName *)alloca((numparam - 1) * sizeof(FName));
	for (int i = 1; i < numparam; ++i)
	{
		PARAM_NAME_AT(i, zaname);
		names[i - 1] = zaname;
	}
	PARAM_OBJECT_AT(0, self, AActor);
	FState *state = self->GetClass()->FindState(numparam - 1, names);
	if (state == NULL)
	{
		const char *dot = "";
		Printf("Jump target '");
 		for (int i = 0; i < numparam - 1; i++)
		{
			Printf("%s%s", dot, names[i].GetChars());
			dot = ".";
		}
		Printf("' not found in %s\n", self->GetClass()->TypeName.GetChars());
	}
	ret->SetPointer(state, ATAG_STATE);
	return 1;
}

ExpEmit FxMultiNameState::Emit(VMFunctionBuilder *build)
{
	ExpEmit dest(build, REGT_POINTER);
	build->Emit(OP_PARAM, 0, REGT_POINTER, 1);		// pass stateowner
	for (unsigned i = 0; i < names.Size(); ++i)
	{
		EmitConstantInt(build, names[i]);
	}

	// Find the DecoFindMultiNameState function. If not found, create it and install it
	// in Actor.
	VMFunction *callfunc;
	PSymbol *sym = RUNTIME_CLASS(AActor)->Symbols.FindSymbol("DecoFindMultiNameState", false);
	if (sym == NULL)
	{
		PSymbolVMFunction *symfunc = new PSymbolVMFunction("DecoFindMultiNameState");
		VMNativeFunction *calldec = new VMNativeFunction(DecoFindMultiNameState);
		symfunc->Function = calldec;
		sym = symfunc;
		RUNTIME_CLASS(AActor)->Symbols.AddSymbol(sym);
	}
	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != NULL);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), names.Size() + 1, 1);
	build->Emit(OP_RESULT, 0, REGT_POINTER, dest.RegNum);
	return dest;
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

int FStateExpressions::Add(FxExpression *x, PClassActor *o, bool c)
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

int FStateExpressions::Reserve(int num, PClassActor *cls)
{
	int idx = expressions.Reserve(num);
	FStateExpression *exp = &expressions[idx];
	for(int i = 0; i < num; i++)
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
