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

#include <stdlib.h>
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
#include "codegen.h"
#include "m_fixed.h"
#include "vmbuilder.h"
#include "v_text.h"
#include "w_wad.h"
#include "math/cmath.h"

extern FRandom pr_exrandom;

struct FLOP
{
	ENamedName Name;
	int Flop;
	double (*Evaluate)(double);
};

// Decorate operates on degrees, so the evaluate functions need to convert
// degrees to radians for those that work with angles.
static const FLOP FxFlops[] =
{
	{ NAME_Exp,		FLOP_EXP,		[](double v) { return g_exp(v); } },
	{ NAME_Log,		FLOP_LOG,		[](double v) { return g_log(v); } },
	{ NAME_Log10,	FLOP_LOG10,		[](double v) { return g_log10(v); } },
	{ NAME_Sqrt,	FLOP_SQRT,		[](double v) { return g_sqrt(v); } },
	{ NAME_Ceil,	FLOP_CEIL,		[](double v) { return ceil(v); } },
	{ NAME_Floor,	FLOP_FLOOR,		[](double v) { return floor(v); } },

	{ NAME_ACos,	FLOP_ACOS_DEG,	[](double v) { return g_acos(v) * (180.0 / M_PI); } },
	{ NAME_ASin,	FLOP_ASIN_DEG,	[](double v) { return g_asin(v) * (180.0 / M_PI); } },
	{ NAME_ATan,	FLOP_ATAN_DEG,	[](double v) { return g_atan(v) * (180.0 / M_PI); } },
	{ NAME_Cos,		FLOP_COS_DEG,	[](double v) { return g_cosdeg(v); } },
	{ NAME_Sin,		FLOP_SIN_DEG,	[](double v) { return g_sindeg(v); } },
	{ NAME_Tan,		FLOP_TAN_DEG,	[](double v) { return g_tan(v * (M_PI / 180.0)); } },

	{ NAME_CosH,	FLOP_COSH,		[](double v) { return g_cosh(v); } },
	{ NAME_SinH,	FLOP_SINH,		[](double v) { return g_sinh(v); } },
	{ NAME_TanH,	FLOP_TANH,		[](double v) { return g_tanh(v); } },
};

//==========================================================================
//
// FCompileContext
//
//==========================================================================

FCompileContext::FCompileContext(PFunction *fnc, PPrototype *ret, bool fromdecorate, int stateindex, int statecount, int lump) 
	: ReturnProto(ret), Function(fnc), Class(nullptr), FromDecorate(fromdecorate), StateIndex(stateindex), StateCount(statecount), Lump(lump)
{
	if (fnc != nullptr) Class = fnc->OwningClass;
}

FCompileContext::FCompileContext(PClass *cls, bool fromdecorate) 
	: ReturnProto(nullptr), Function(nullptr), Class(cls), FromDecorate(fromdecorate), StateIndex(-1), StateCount(0), Lump(-1)
{
}

PSymbol *FCompileContext::FindInClass(FName identifier, PSymbolTable *&symt)
{
	return Class != nullptr? Class->Symbols.FindSymbolInTable(identifier, symt) : nullptr;
}

PSymbol *FCompileContext::FindInSelfClass(FName identifier, PSymbolTable *&symt)
{
	// If we have no self we cannot retrieve any values from it.
	if (Function == nullptr || Function->Variants[0].SelfClass == nullptr) return nullptr;
	return Function->Variants[0].SelfClass->Symbols.FindSymbolInTable(identifier, symt);
}
PSymbol *FCompileContext::FindGlobal(FName identifier)
{
	return GlobalSymbols.FindSymbol(identifier, true);
}

void FCompileContext::CheckReturn(PPrototype *proto, FScriptPosition &pos)
{
	assert(proto != nullptr);
	bool fail = false;

	if (ReturnProto == nullptr)
	{
		ReturnProto = proto;
		return;
	}

	// A prototype that defines fewer return types can be compatible with
	// one that defines more if the shorter one matches the initial types
	// for the longer one.
	if (ReturnProto->ReturnTypes.Size() < proto->ReturnTypes.Size())
	{ // Make proto the shorter one to avoid code duplication below.
		swapvalues(proto, ReturnProto);
	}
	// If one prototype returns nothing, they both must.
	if (proto->ReturnTypes.Size() == 0)
	{
		if (ReturnProto->ReturnTypes.Size() != 0)
		{
			fail = true;
		}
	}
	else
	{
		for (unsigned i = 0; i < proto->ReturnTypes.Size(); i++)
		{
			if (ReturnProto->ReturnTypes[i] != proto->ReturnTypes[i])
			{ // Incompatible
				fail = true;
				break;
			}
		}
	}

	if (fail)
	{
		pos.Message(MSG_ERROR, "Return type mismatch");
	}
}

bool FCompileContext::CheckReadOnly(int flags)
{
	if (!(flags & VARF_ReadOnly)) return false;
	if (!(flags & VARF_InternalAccess)) return true;
	return Wads.GetLumpFile(Lump) != 0;
}

FxLocalVariableDeclaration *FCompileContext::FindLocalVariable(FName name)
{
	if (Block == nullptr)
	{
		return nullptr;
	}
	else
	{
		return Block->FindLocalVariable(name, *this);
	}
}

//==========================================================================
//
// ExpEmit
//
//==========================================================================

ExpEmit::ExpEmit(VMFunctionBuilder *build, int type, int count)
: RegNum(build->Registers[type].Get(count)), RegType(type), RegCount(count), Konst(false), Fixed(false), Final(false), Target(false)
{
}

void ExpEmit::Free(VMFunctionBuilder *build)
{
	if (!Fixed && !Konst && RegType <= REGT_TYPE)
	{
		build->Registers[RegType].Return(RegNum, RegCount);
	}
}

void ExpEmit::Reuse(VMFunctionBuilder *build)
{
	if (!Fixed && !Konst)
	{
		assert(RegCount == 1);
		bool success = build->Registers[RegType].Reuse(RegNum);
		assert(success && "Attempt to reuse a register that is already in use");
	}
}

//==========================================================================
//
// FindBuiltinFunction
//
// Returns the symbol for a decorate utility function. If not found, create
// it and install it a local symbol table.
//
//==========================================================================

static PSymbol *FindBuiltinFunction(FName funcname, VMNativeFunction::NativeCallType func)
{
	PSymbol *sym = GlobalSymbols.FindSymbol(funcname, false);
	if (sym == nullptr)
	{
		PSymbolVMFunction *symfunc = new PSymbolVMFunction(funcname);
		VMNativeFunction *calldec = new VMNativeFunction(func, funcname);
		symfunc->Function = calldec;
		sym = symfunc;
		GlobalSymbols.AddSymbol(sym);
	}
	return sym;
}

//==========================================================================
//
//
//
//==========================================================================

static bool AreCompatiblePointerTypes(PType *dest, PType *source)
{
	if (dest->IsKindOf(RUNTIME_CLASS(PPointer)) && source->IsKindOf(RUNTIME_CLASS(PPointer)))
	{
		// Pointers to different types are only compatible if both point to an object and the source type is a child of the destination type.
		auto fromtype = static_cast<PPointer *>(source);
		auto totype = static_cast<PPointer *>(dest);
		if (fromtype == nullptr) return true;
		if (totype->IsConst && !fromtype->IsConst) return false;
		if (fromtype == totype) return true;
		if (fromtype->PointedType->IsKindOf(RUNTIME_CLASS(PClass)) && totype->PointedType->IsKindOf(RUNTIME_CLASS(PClass)))
		{
			auto fromcls = static_cast<PClass *>(fromtype->PointedType);
			auto tocls = static_cast<PClass *>(totype->PointedType);
			return (fromcls->IsDescendantOf(tocls));
		}
	}
	return false;
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

VMFunction *FxExpression::GetDirectFunction()
{
	return nullptr;
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
// Returns true if we can write to the address.
//
//==========================================================================

bool FxExpression::RequestAddress(FCompileContext &ctx, bool *writable)
{
	if (writable != nullptr) *writable = false;
	return false;
}

//==========================================================================
//
// Called by return statements.
//
//==========================================================================

PPrototype *FxExpression::ReturnProto()
{
	assert(ValueType != nullptr);

	TArray<PType *> ret(0);
	TArray<PType *> none(0);
	if (ValueType != TypeVoid)
	{
		ret.Push(ValueType);
	}

	return NewPrototype(ret, none);
}

//==========================================================================
//
//
//
//==========================================================================

static int EncodeRegType(ExpEmit reg)
{
	int regtype = reg.RegType;
	if (reg.Konst)
	{
		regtype |= REGT_KONST;
	}
	else if (reg.RegCount == 2)
	{
		regtype |= REGT_MULTIREG2;

	}
	else if (reg.RegCount == 3)
	{
		regtype |= REGT_MULTIREG3;
	}
	return regtype;
}

//==========================================================================
//
//
//
//==========================================================================

static int EmitParameter(VMFunctionBuilder *build, FxExpression *operand, const FScriptPosition &pos)
{
	ExpEmit where = operand->Emit(build);

	if (where.RegType == REGT_NIL)
	{
		pos.Message(MSG_ERROR, "Attempted to pass a non-value");
		build->Emit(OP_PARAM, 0, where.RegType, where.RegNum);
		return 1;
	}
	else
	{
		build->Emit(OP_PARAM, 0, EncodeRegType(where), where.RegNum);
		where.Free(build);
		return where.RegCount;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxConstant::MakeConstant(PSymbol *sym, const FScriptPosition &pos)
{
	FxExpression *x;
	PSymbolConstNumeric *csym = dyn_cast<PSymbolConstNumeric>(sym);
	if (csym != nullptr)
	{
		if (csym->ValueType->IsA(RUNTIME_CLASS(PInt)))
		{
			x = new FxConstant(csym->Value, pos);
		}
		else if (csym->ValueType->IsA(RUNTIME_CLASS(PFloat)))
		{
			x = new FxConstant(csym->Float, pos);
		}
		else
		{
			pos.Message(MSG_ERROR, "Invalid constant '%s'\n", csym->SymbolName.GetChars());
			return nullptr;
		}
	}
	else
	{
		pos.Message(MSG_ERROR, "'%s' is not a constant\n", sym->SymbolName.GetChars());
		x = nullptr;
	}
	return x;
}

ExpEmit FxConstant::Emit(VMFunctionBuilder *build)
{
	ExpEmit out;

	out.Konst = true;
	int regtype = value.Type->GetRegType();
	out.RegType = regtype;
	if (regtype == REGT_INT)
	{
		out.RegNum = build->GetConstantInt(value.Int);
	}
	else if (regtype == REGT_FLOAT)
	{
		out.RegNum = build->GetConstantFloat(value.Float);
	}
	else if (regtype == REGT_POINTER)
	{
		VM_ATAG tag = ATAG_GENERIC;
		if (value.Type == TypeState)
		{
			tag = ATAG_STATE;
		}
		else if (value.Type->GetLoadOp() == OP_LO)
		{
			tag = ATAG_OBJECT;
		}
		out.RegNum = build->GetConstantAddress(value.pointer, tag);
	}
	else if (regtype == REGT_STRING)
	{
		out.RegNum = build->GetConstantString(value.GetString());
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Cannot emit needed constant");
		out.RegNum = 0;
	}
	return out;
}

//==========================================================================
//
//
//
//==========================================================================

FxVectorValue::FxVectorValue(FxExpression *x, FxExpression *y, FxExpression *z, const FScriptPosition &sc)
	:FxExpression(EFX_VectorValue, sc)
{
	xyz[0] = x;
	xyz[1] = y;
	xyz[2] = z;
	isConst = false;
	ValueType = TypeVoid;	// we do not know yet
}

FxVectorValue::~FxVectorValue()
{
	for (auto &a : xyz)
		SAFE_DELETE(a);
}

FxExpression *FxVectorValue::Resolve(FCompileContext&ctx)
{
	bool fails = false;

	for (auto &a : xyz)
	{
		if (a != nullptr)
		{
			a = a->Resolve(ctx);
			if (a == nullptr) fails = true;
			else
			{
				if (a->ValueType != TypeVector2)	// a vec3 may be initialized with (vec2, z)
				{
					a = new FxFloatCast(a);
					a = a->Resolve(ctx);
					fails |= (a == nullptr);
				}
			}
		}
	}
	if (fails)
	{
		delete this;
		return nullptr;
	}
	// at this point there are three legal cases:
	// * two floats = vector2
	// * three floats = vector3
	// * vector2 + float = vector3
	if (xyz[0]->ValueType == TypeVector2)
	{
		if (xyz[1]->ValueType != TypeFloat64 || xyz[2] != nullptr)
		{
			ScriptPosition.Message(MSG_ERROR, "Not a valid vector");
			delete this;
			return nullptr;
		}
		ValueType = TypeVector3;
		if (xyz[0]->ExprType == EFX_VectorValue)
		{
			// If two vector initializers are nested, unnest them now.
			auto vi = static_cast<FxVectorValue*>(xyz[0]);
			xyz[2] = xyz[1];
			xyz[1] = vi->xyz[1];
			xyz[0] = vi->xyz[0];
			vi->xyz[0] = vi->xyz[1] = nullptr; // Don't delete our own expressions.
			delete vi;
		}
	}
	else if (xyz[0]->ValueType == TypeFloat64 && xyz[1]->ValueType == TypeFloat64)
	{
		ValueType = xyz[2] == nullptr ? TypeVector2 : TypeVector3;
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Not a valid vector");
		delete this;
		return nullptr;
	}

	// check if all elements are constant. If so this can be emitted as a constant vector.
	isConst = true;
	for (auto &a : xyz)
	{
		if (a != nullptr && !a->isConstant()) isConst = false;
	}
	return this;
}

static ExpEmit EmitKonst(VMFunctionBuilder *build, ExpEmit &emit)
{
	if (emit.Konst)
	{
		ExpEmit out(build, REGT_FLOAT);
		build->Emit(OP_LKF, out.RegNum, emit.RegNum);
		return out;
	}
	return emit;
}

ExpEmit FxVectorValue::Emit(VMFunctionBuilder *build)
{
	// no const handling here. Ultimstely it's too rarely used (i.e. the only fully constant vector ever allocated in ZDoom is the 0-vector in a very few places)
	// and the negatives (excessive allocation of float constants) outweigh the positives (saved a few instructions)
	assert(xyz[0] != nullptr);
	assert(xyz[1] != nullptr);
	if (ValueType == TypeVector2)
	{
		ExpEmit tempxval = xyz[0]->Emit(build);
		ExpEmit tempyval = xyz[1]->Emit(build);
		ExpEmit xval = EmitKonst(build, tempxval);
		ExpEmit yval = EmitKonst(build, tempyval);
		assert(xval.RegType == REGT_FLOAT && yval.RegType == REGT_FLOAT);
		if (yval.RegNum == xval.RegNum + 1)
		{
			// The results are already in two continuous registers so just return them as-is.
			xval.RegCount++;
			return xval;
		}
		else
		{
			// The values are not in continuous registers so they need to be copied together now.
			ExpEmit out(build, REGT_FLOAT, 2);
			build->Emit(OP_MOVEF, out.RegNum, xval.RegNum);
			build->Emit(OP_MOVEF, out.RegNum + 1, yval.RegNum);
			xval.Free(build);
			yval.Free(build);
			return out;
		}
	}
	else if (xyz[0]->ValueType == TypeVector2)	// vec2+float
	{
		ExpEmit xyval = xyz[0]->Emit(build);
		ExpEmit tempzval = xyz[1]->Emit(build);
		ExpEmit zval = EmitKonst(build, tempzval);
		assert(xyval.RegType == REGT_FLOAT && xyval.RegCount == 2 && zval.RegType == REGT_FLOAT);
		if (zval.RegNum == xyval.RegNum + 2)
		{
			// The results are already in three continuous registers so just return them as-is.
			xyval.RegCount++;
			return xyval;
		}
		else
		{
			// The values are not in continuous registers so they need to be copied together now.
			ExpEmit out(build, REGT_FLOAT, 3);
			build->Emit(OP_MOVEV2, out.RegNum, xyval.RegNum);
			build->Emit(OP_MOVEF, out.RegNum + 2, zval.RegNum);
			xyval.Free(build);
			zval.Free(build);
			return out;
		}
	}
	else // 3*float
	{
		assert(xyz[2] != nullptr);
		ExpEmit tempxval = xyz[0]->Emit(build);
		ExpEmit tempyval = xyz[1]->Emit(build);
		ExpEmit tempzval = xyz[2]->Emit(build);
		ExpEmit xval = EmitKonst(build, tempxval);
		ExpEmit yval = EmitKonst(build, tempyval);
		ExpEmit zval = EmitKonst(build, tempzval);
		assert(xval.RegType == REGT_FLOAT && yval.RegType == REGT_FLOAT && zval.RegType == REGT_FLOAT);
		if (yval.RegNum == xval.RegNum + 1 && zval.RegNum == xval.RegNum + 2)
		{
			// The results are already in three continuous registers so just return them as-is.
			xval.RegCount += 2;
			return xval;
		}
		else
		{
			// The values are not in continuous registers so they need to be copied together now.
			ExpEmit out(build, REGT_FLOAT, 3);
			//Try to optimize a bit...
			if (yval.RegNum == xval.RegNum + 1)
			{
				build->Emit(OP_MOVEV2, out.RegNum, xval.RegNum);
				build->Emit(OP_MOVEF, out.RegNum + 2, zval.RegNum);
			}
			else if (zval.RegNum == yval.RegNum + 1)
			{
				build->Emit(OP_MOVEF, out.RegNum, xval.RegNum);
				build->Emit(OP_MOVEV2, out.RegNum+1, yval.RegNum);
			}
			else
			{
				build->Emit(OP_MOVEF, out.RegNum, xval.RegNum);
				build->Emit(OP_MOVEF, out.RegNum + 1, yval.RegNum);
				build->Emit(OP_MOVEF, out.RegNum + 2, zval.RegNum);
			}
			xval.Free(build);
			yval.Free(build);
			zval.Free(build);
			return out;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

FxBoolCast::FxBoolCast(FxExpression *x, bool needvalue)
	: FxExpression(EFX_BoolCast, x->ScriptPosition)
{
	basex = x;
	ValueType = TypeBool;
	NeedValue = needvalue;
}

//==========================================================================
//
//
//
//==========================================================================

FxBoolCast::~FxBoolCast()
{
	SAFE_DELETE(basex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxBoolCast::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(basex, ctx);

	if (basex->ValueType == TypeBool)
	{
		FxExpression *x = basex;
		basex = nullptr;
		delete this;
		return x;
	}
	else if (basex->IsBoolCompat())
	{
		if (basex->isConstant())
		{
			assert(basex->ValueType != TypeState && "We shouldn't be able to generate a constant state ref");

			ExpVal constval = static_cast<FxConstant *>(basex)->GetValue();
			FxExpression *x = new FxConstant(constval.GetBool(), ScriptPosition);
			delete this;
			return x;
		}
		return this;
	}
	ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
	delete this;
	return nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxBoolCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = basex->Emit(build);
	assert(!from.Konst);
	assert(basex->ValueType->GetRegType() == REGT_INT || basex->ValueType->GetRegType() == REGT_FLOAT || basex->ValueType->GetRegType() == REGT_POINTER);

	if (NeedValue)
	{
		ExpEmit to(build, REGT_INT);
		from.Free(build);
		// Preload result with 0.
		build->Emit(OP_LI, to.RegNum, 0);

		// Check source against 0.
		if (from.RegType == REGT_INT)
		{
			build->Emit(OP_EQ_R, 1, from.RegNum, to.RegNum);
		}
		else if (from.RegType == REGT_FLOAT)
		{
			build->Emit(OP_EQF_K, 1, from.RegNum, build->GetConstantFloat(0.));
		}
		else if (from.RegType == REGT_POINTER)
		{
			build->Emit(OP_EQA_K, 1, from.RegNum, build->GetConstantAddress(nullptr, ATAG_GENERIC));
		}
		build->Emit(OP_JMP, 1);

		// Reload result with 1 if the comparison fell through.
		build->Emit(OP_LI, to.RegNum, 1);
		return to;
	}
	else
	{
		return from;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FxIntCast::FxIntCast(FxExpression *x, bool nowarn, bool explicitly)
: FxExpression(EFX_IntCast, x->ScriptPosition)
{
	basex=x;
	ValueType = TypeSInt32;
	NoWarn = nowarn;
	Explicit = explicitly;
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

	if (basex->ValueType->GetRegType() == REGT_INT)
	{
		if (basex->ValueType != TypeName || Explicit)	// names can be converted to int, but only with an explicit type cast.
		{
			FxExpression *x = basex;
			x->ValueType = ValueType;
			basex = nullptr;
			delete this;
			return x;
		}
		else
		{
			// Ugh. This should abort, but too many mods fell into this logic hole somewhere, so this seroious error needs to be reduced to a warning. :(
			// At least in ZScript, MSG_OPTERROR always means to report an error, not a warning so the problem only exists in DECORATE.
			if (!basex->isConstant())	
				ScriptPosition.Message(MSG_OPTERROR, "Numeric type expected, got a name");
			else ScriptPosition.Message(MSG_OPTERROR, "Numeric type expected, got \"%s\"", static_cast<FxConstant*>(basex)->GetValue().GetName().GetChars());
			FxExpression * x = new FxConstant(0, ScriptPosition);
			delete this;
			return x;
		}
	}
	else if (basex->IsFloat())
	{
		if (basex->isConstant())
		{
			ExpVal constval = static_cast<FxConstant *>(basex)->GetValue();
			FxExpression *x = new FxConstant(constval.GetInt(), ScriptPosition);
			if (!NoWarn && constval.GetInt() != constval.GetFloat())
			{
				ScriptPosition.Message(MSG_WARNING, "Truncation of floating point constant %f", constval.GetFloat());
			}

			delete this;
			return x;
		}
		else if (!NoWarn)
		{
			ScriptPosition.Message(MSG_WARNING, "Truncation of floating point value");
		}

		return this;
	}
	ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
	delete this;
	return nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxIntCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = basex->Emit(build);
	assert(!from.Konst);
	assert(basex->ValueType->GetRegType() == REGT_FLOAT);
	from.Free(build);
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
	: FxExpression(EFX_FloatCast, x->ScriptPosition)
{
	basex = x;
	ValueType = TypeFloat64;
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

	if (basex->IsFloat())
	{
		FxExpression *x = basex;
		basex = nullptr;
		delete this;
		return x;
	}
	else if (basex->ValueType->GetRegType() == REGT_INT)
	{
		if (basex->ValueType != TypeName)
		{
			if (basex->isConstant())
			{
				ExpVal constval = static_cast<FxConstant *>(basex)->GetValue();
				FxExpression *x = new FxConstant(constval.GetFloat(), ScriptPosition);
				delete this;
				return x;
			}
			return this;
		}
		else
		{
			// Ugh. This should abort, but too many mods fell into this logic hole somewhere, so this seroious error needs to be reduced to a warning. :(
			// At least in ZScript, MSG_OPTERROR always means to report an error, not a warning so the problem only exists in DECORATE.
			if (!basex->isConstant()) ScriptPosition.Message(MSG_OPTERROR, "Numeric type expected, got a name");
			else ScriptPosition.Message(MSG_OPTERROR, "Numeric type expected, got \"%s\"", static_cast<FxConstant*>(basex)->GetValue().GetName().GetChars());
			FxExpression *x = new FxConstant(0.0, ScriptPosition);
			delete this;
			return x;
		}
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxFloatCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = basex->Emit(build);
	assert(!from.Konst);
	assert(basex->ValueType->GetRegType() == REGT_INT);
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

FxNameCast::FxNameCast(FxExpression *x)
	: FxExpression(EFX_NameCast, x->ScriptPosition)
{
	basex = x;
	ValueType = TypeName;
}

//==========================================================================
//
//
//
//==========================================================================

FxNameCast::~FxNameCast()
{
	SAFE_DELETE(basex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxNameCast::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(basex, ctx);

	if (basex->ValueType == TypeName)
	{
		FxExpression *x = basex;
		basex = nullptr;
		delete this;
		return x;
	}
	else if (basex->ValueType == TypeString)
	{
		if (basex->isConstant())
		{
			ExpVal constval = static_cast<FxConstant *>(basex)->GetValue();
			FxExpression *x = new FxConstant(constval.GetName(), ScriptPosition);
			delete this;
			return x;
		}
		return this;
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Cannot convert to name");
		delete this;
		return nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxNameCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = basex->Emit(build);
	assert(!from.Konst);
	assert(basex->ValueType == TypeString);
	from.Free(build);
	ExpEmit to(build, REGT_INT);
	build->Emit(OP_CAST, to.RegNum, from.RegNum, CAST_S2N);
	return to;
}

//==========================================================================
//
//
//
//==========================================================================

FxStringCast::FxStringCast(FxExpression *x)
	: FxExpression(EFX_StringCast, x->ScriptPosition)
{
	basex = x;
	ValueType = TypeString;
}

//==========================================================================
//
//
//
//==========================================================================

FxStringCast::~FxStringCast()
{
	SAFE_DELETE(basex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxStringCast::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(basex, ctx);

	if (basex->ValueType == TypeString)
	{
		FxExpression *x = basex;
		basex = nullptr;
		delete this;
		return x;
	}
	else if (basex->ValueType == TypeName)
	{
		if (basex->isConstant())
		{
			ExpVal constval = static_cast<FxConstant *>(basex)->GetValue();
			FxExpression *x = new FxConstant(constval.GetString(), ScriptPosition);
			delete this;
			return x;
		}
		return this;
	}
	else if (basex->ValueType == TypeSound)
	{
		if (basex->isConstant())
		{
			ExpVal constval = static_cast<FxConstant *>(basex)->GetValue();
			FxExpression *x = new FxConstant(S_sfx[constval.GetInt()].name, ScriptPosition);
			delete this;
			return x;
		}
		return this;
	}
	// although it could be done, let's not convert colors back to strings.
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Cannot convert to string");
		delete this;
		return nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxStringCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = basex->Emit(build);
	assert(!from.Konst);

	from.Free(build);
	ExpEmit to(build, REGT_STRING);
	if (basex->ValueType == TypeName)
	{
		build->Emit(OP_CAST, to.RegNum, from.RegNum, CAST_N2S);
	}
	else if (basex->ValueType == TypeSound)
	{
		build->Emit(OP_CAST, to.RegNum, from.RegNum, CAST_So2S);
	}
	return to;
}

//==========================================================================
//
//
//
//==========================================================================

FxColorCast::FxColorCast(FxExpression *x)
	: FxExpression(EFX_ColorCast, x->ScriptPosition)
{
	basex = x;
	ValueType = TypeColor;
}

//==========================================================================
//
//
//
//==========================================================================

FxColorCast::~FxColorCast()
{
	SAFE_DELETE(basex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxColorCast::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(basex, ctx);

	if (basex->ValueType == TypeColor || basex->ValueType->GetClass() == RUNTIME_CLASS(PInt))
	{
		FxExpression *x = basex;
		x->ValueType = TypeColor;
		basex = nullptr;
		delete this;
		return x;
	}
	else if (basex->ValueType == TypeString)
	{
		if (basex->isConstant())
		{
			ExpVal constval = static_cast<FxConstant *>(basex)->GetValue();
			if (constval.GetString().Len() == 0)
			{
				// empty string means 'no state'. This would otherwise just cause endless errors and have the same result anyway.
				FxExpression *x = new FxConstant(-1, ScriptPosition);
				delete this;
				return x;
			}
			else
			{
				FxExpression *x = new FxConstant(V_GetColor(nullptr, constval.GetString()), ScriptPosition);
				delete this;
				return x;
			}
		}
		return this;
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Cannot convert to color");
		delete this;
		return nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxColorCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = basex->Emit(build);
	assert(!from.Konst);
	assert(basex->ValueType == TypeString);
	from.Free(build);
	ExpEmit to(build, REGT_INT);
	build->Emit(OP_CAST, to.RegNum, from.RegNum, CAST_S2Co);
	return to;
}

//==========================================================================
//
//
//
//==========================================================================

FxSoundCast::FxSoundCast(FxExpression *x)
	: FxExpression(EFX_SoundCast, x->ScriptPosition)
{
	basex = x;
	ValueType = TypeSound;
}

//==========================================================================
//
//
//
//==========================================================================

FxSoundCast::~FxSoundCast()
{
	SAFE_DELETE(basex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxSoundCast::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(basex, ctx);

	if (basex->ValueType == TypeSound || basex->ValueType->GetClass() == RUNTIME_CLASS(PInt))
	{
		FxExpression *x = basex;
		x->ValueType = TypeSound;
		basex = nullptr;
		delete this;
		return x;
	}
	else if (basex->ValueType == TypeString)
	{
		if (basex->isConstant())
		{
			ExpVal constval = static_cast<FxConstant *>(basex)->GetValue();
			FxExpression *x = new FxConstant(FSoundID(constval.GetString()), ScriptPosition);
			delete this;
			return x;
		}
		return this;
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Cannot convert to sound");
		delete this;
		return nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxSoundCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit from = basex->Emit(build);
	assert(!from.Konst);
	assert(basex->ValueType == TypeString);
	from.Free(build);
	ExpEmit to(build, REGT_INT);
	build->Emit(OP_CAST, to.RegNum, from.RegNum, CAST_S2So);
	return to;
}

//==========================================================================
//
// generic type cast operator
//
//==========================================================================

FxTypeCast::FxTypeCast(FxExpression *x, PType *type, bool nowarn, bool explicitly)
	: FxExpression(EFX_TypeCast, x->ScriptPosition)
{
	basex = x;
	ValueType = type;
	NoWarn = nowarn;
	Explicit = explicitly;
	assert(ValueType != nullptr);
}

//==========================================================================
//
//
//
//==========================================================================

FxTypeCast::~FxTypeCast()
{
	SAFE_DELETE(basex);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxTypeCast::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(basex, ctx);

	// first deal with the simple types
	if (ValueType == TypeError || basex->ValueType == TypeError)
	{
		ScriptPosition.Message(MSG_ERROR, "Trying to cast to invalid type. This error message means that somewhere in the script compiler an error check is missing.");
		delete this;
		return nullptr;
	}
	else if (ValueType == TypeVoid)	// this should never happen
	{
		goto errormsg;
	}
	else if (basex->ValueType == TypeVoid)
	{
		goto errormsg;
	}
	else if (basex->ValueType == ValueType)
	{
		// don't go through the entire list if the types are the same.
		goto basereturn;
	}
	else if (basex->ValueType == TypeNullPtr && (ValueType == TypeState || ValueType->IsKindOf(RUNTIME_CLASS(PPointer))))
	{
		goto basereturn;
	}
	else if (IsFloat())
	{
		FxExpression *x = new FxFloatCast(basex);
		x = x->Resolve(ctx);
		basex = nullptr;
		delete this;
		return x;
	}
	else if (ValueType->IsA(RUNTIME_CLASS(PInt)))
	{
		// This is only for casting to actual ints. Subtypes representing an int will be handled elsewhere.
		FxExpression *x = new FxIntCast(basex, NoWarn, Explicit);
		x = x->Resolve(ctx);
		basex = nullptr;
		delete this;
		return x;
	}
	else if (ValueType == TypeBool)
	{
		FxExpression *x = new FxBoolCast(basex);
		x = x->Resolve(ctx);
		basex = nullptr;
		delete this;
		return x;
	}
	else if (ValueType == TypeString)
	{
		FxExpression *x = new FxStringCast(basex);
		x = x->Resolve(ctx);
		basex = nullptr;
		delete this;
		return x;
	}
	else if (ValueType == TypeName)
	{
		FxExpression *x = new FxNameCast(basex);
		x = x->Resolve(ctx);
		basex = nullptr;
		delete this;
		return x;
	}
	else if (ValueType == TypeSound)
	{
		FxExpression *x = new FxSoundCast(basex);
		x = x->Resolve(ctx);
		basex = nullptr;
		delete this;
		return x;
	}
	else if (ValueType == TypeColor)
	{
		FxExpression *x = new FxColorCast(basex);
		x = x->Resolve(ctx);
		basex = nullptr;
		delete this;
		return x;
	}
	else if (ValueType == TypeState)
	{
		// Right now this only supports string constants. There should be an option to pass a string variable, too.
		if (basex->isConstant() && (basex->ValueType == TypeString || basex->ValueType == TypeName))
		{
			const char *s = static_cast<FxConstant *>(basex)->GetValue().GetString();
			if (*s == 0 && !ctx.FromDecorate)	// DECORATE should never get here at all, but let's better be safe.
			{
				ScriptPosition.Message(MSG_ERROR, "State jump to empty label.");
				delete this;
				return nullptr;
			}
			FxExpression *x = new FxMultiNameState(s, basex->ScriptPosition);
			x = x->Resolve(ctx);
			basex = nullptr;
			delete this;
			return x;
		}
		else if (basex->IsNumeric() && basex->ValueType != TypeSound && basex->ValueType != TypeColor)
		{
			if (ctx.StateIndex < 0)
			{
				ScriptPosition.Message(MSG_ERROR, "State jumps with index can only be used in anonymous state functions.");
				delete this;
				return nullptr;
			}
			if (ctx.StateCount != 1)
			{
				ScriptPosition.Message(MSG_ERROR, "State jumps with index cannot be used on multistate definitions");
				delete this;
				return nullptr;
			}
			if (basex->isConstant())
			{
				int i = static_cast<FxConstant *>(basex)->GetValue().GetInt();
				if (i <= 0)
				{
					ScriptPosition.Message(MSG_ERROR, "State index must be positive");
					delete this;
					return nullptr;
				}
				FxExpression *x = new FxStateByIndex(ctx.StateIndex + i, ScriptPosition);
				x = x->Resolve(ctx);
				basex = nullptr;
				delete this;
				return x;
			}
			else
			{
				FxExpression *x = new FxRuntimeStateIndex(basex);
				x = x->Resolve(ctx);
				basex = nullptr;
				delete this;
				return x;
			}
		}
	}
	else if (ValueType->IsKindOf(RUNTIME_CLASS(PClassPointer)))
	{
		FxExpression *x = new FxClassTypeCast(static_cast<PClassPointer*>(ValueType), basex);
		x = x->Resolve(ctx);
		basex = nullptr;
		delete this;
		return x;
	}
	/* else if (ValueType->IsKindOf(RUNTIME_CLASS(PEnum)))
	{
	// this is not yet ready and does not get assigned to actual values.
	}
	*/
	else if (ValueType->IsKindOf(RUNTIME_CLASS(PClass)))	// this should never happen because the VM doesn't handle plain class types - just pointers
	{
		if (basex->ValueType->IsKindOf(RUNTIME_CLASS(PClass)))
		{
			// class types are only compatible if the base type is a descendant of the result type.
			auto fromtype = static_cast<PClass *>(basex->ValueType);
			auto totype = static_cast<PClass *>(ValueType);
			if (fromtype->IsDescendantOf(totype)) goto basereturn;
		}
	}
	else if (AreCompatiblePointerTypes(ValueType, basex->ValueType))
	{
		goto basereturn;
	}
	// todo: pointers to class objects. 
	// All other types are only compatible to themselves and have already been handled above by the equality check.
	// Anything that falls through here is not compatible and must print an error.

errormsg:
	ScriptPosition.Message(MSG_ERROR, "Cannot convert %s to %s", basex->ValueType->DescriptiveName(), ValueType->DescriptiveName());
	delete this;
	return nullptr;

basereturn:
	auto x = basex;
	x->ValueType = ValueType;
	basex = nullptr;
	delete this;
	return x;

}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxTypeCast::Emit(VMFunctionBuilder *build)
{
	assert(false);
	// This should never be reached
	return ExpEmit();
}

//==========================================================================
//
//
//
//==========================================================================

FxPlusSign::FxPlusSign(FxExpression *operand)
: FxExpression(EFX_PlusSign, operand->ScriptPosition)
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

	if (Operand->IsNumeric() || Operand->IsVector())
	{
		FxExpression *e = Operand;
		Operand = nullptr;
		delete this;
		return e;
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
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
: FxExpression(EFX_MinusSign, operand->ScriptPosition)
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

	if (Operand->IsNumeric() || Operand->IsVector())
	{
		if (Operand->isConstant())
		{
			ExpVal val = static_cast<FxConstant *>(Operand)->GetValue();
			FxExpression *e = val.Type->GetRegType() == REGT_INT ?
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
		return nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxMinusSign::Emit(VMFunctionBuilder *build)
{
	assert(ValueType == Operand->ValueType);
	ExpEmit from = Operand->Emit(build);
	assert(from.Konst == 0);
	assert(ValueType->GetRegCount() == from.RegCount);
	// Do it in-place, unless a local variable
	if (from.Fixed)
	{
		ExpEmit to = ExpEmit(build, from.RegType, from.RegCount);
		build->Emit(Operand->ValueType->GetMoveOp(), to.RegNum, from.RegNum);
		from = to;
	}

	if (ValueType->GetRegType() == REGT_INT)
	{
		build->Emit(OP_NEG, from.RegNum, from.RegNum, 0);
	}
	else
	{
		assert(ValueType->GetRegType() == REGT_FLOAT);
		switch (from.RegCount)
		{
		case 1:
			build->Emit(OP_FLOP, from.RegNum, from.RegNum, FLOP_NEG);
			break;

		case 2:
			build->Emit(OP_NEGV2, from.RegNum, from.RegNum);
			break;

		case 3:
			build->Emit(OP_NEGV3, from.RegNum, from.RegNum);
			break;

		}
	}
	return from;
}

//==========================================================================
//
//
//
//==========================================================================

FxUnaryNotBitwise::FxUnaryNotBitwise(FxExpression *operand)
: FxExpression(EFX_UnaryNotBitwise, operand->ScriptPosition)
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

	if  (ctx.FromDecorate && Operand->IsFloat() /* lax */)
	{
		// DECORATE allows floats here so cast them to int.
		Operand = new FxIntCast(Operand, true);
		Operand = Operand->Resolve(ctx);
		if (Operand == nullptr) 
		{
			delete this;
			return nullptr;
		}
	}

	// Names were not blocked in DECORATE here after the scripting branch merge. Now they are again.
	if (!Operand->IsInteger())
	{
		ScriptPosition.Message(MSG_ERROR, "Integer type expected");
		delete this;
		return nullptr;
	}

	if (Operand->isConstant())
	{
		int result = ~static_cast<FxConstant *>(Operand)->GetValue().GetInt();
		FxExpression *e = new FxConstant(result, ScriptPosition);
		delete this;
		return e;
	}
	ValueType = TypeSInt32;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxUnaryNotBitwise::Emit(VMFunctionBuilder *build)
{
	assert(Operand->ValueType->GetRegType() == REGT_INT);
	ExpEmit from = Operand->Emit(build);
	assert(!from.Konst);
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
: FxExpression(EFX_UnaryNotBoolean, operand->ScriptPosition)
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
	SAFE_RESOLVE(Operand, ctx);

	if (Operand->ValueType != TypeBool)
	{
		Operand = new FxBoolCast(Operand);
		SAFE_RESOLVE(Operand, ctx);
	}

	if (Operand->isConstant())
	{
		bool result = !static_cast<FxConstant *>(Operand)->GetValue().GetBool();
		FxExpression *e = new FxConstant(result, ScriptPosition);
		delete this;
		return e;
	}

	ValueType = TypeBool;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxUnaryNotBoolean::Emit(VMFunctionBuilder *build)
{
	assert(Operand->ValueType == ValueType);
	assert(ValueType == TypeBool);
	ExpEmit from = Operand->Emit(build);
	assert(!from.Konst);
	// boolean not is the same as XOR-ing the lowest bit
	build->Emit(OP_XOR_RK, from.RegNum, from.RegNum, build->GetConstantInt(1));
	return from;
}

//==========================================================================
//
//
//
//==========================================================================

FxSizeAlign::FxSizeAlign(FxExpression *operand, int which)
	: FxExpression(EFX_SizeAlign, operand->ScriptPosition)
{
	Operand = operand;
	Which = which;
}

//==========================================================================
//
//
//
//==========================================================================

FxSizeAlign::~FxSizeAlign()
{
	SAFE_DELETE(Operand);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxSizeAlign::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Operand, ctx);

	auto type = Operand->ValueType;
	if (Operand->isConstant())
	{
		ScriptPosition.Message(MSG_ERROR, "cannot determine %s of a constant", Which == TK_AlignOf? "alignment" : "size");
		delete this;
		return nullptr;
	}
	else if (!Operand->RequestAddress(ctx, nullptr))
	{
		ScriptPosition.Message(MSG_ERROR, "Operand must be addressable to determine %s", Which == TK_AlignOf ? "alignment" : "size");
		delete this;
		return nullptr;
	}
	else
	{
		FxExpression *x = new FxConstant(Which == TK_AlignOf ? int(type->Align) : int(type->Size), Operand->ScriptPosition);
		delete this;
		return x->Resolve(ctx);
	}
}

ExpEmit FxSizeAlign::Emit(VMFunctionBuilder *build)
{
	return ExpEmit();
}

//==========================================================================
//
// FxPreIncrDecr
//
//==========================================================================

FxPreIncrDecr::FxPreIncrDecr(FxExpression *base, int token)
: FxExpression(EFX_PreIncrDecr, base->ScriptPosition), Token(token), Base(base)
{
	AddressRequested = false;
	AddressWritable = false;
}

FxPreIncrDecr::~FxPreIncrDecr()
{
	SAFE_DELETE(Base);
}

bool FxPreIncrDecr::RequestAddress(FCompileContext &ctx, bool *writable)
{
	AddressRequested = true;
	if (writable != nullptr) *writable = AddressWritable;
	return true;
}

FxExpression *FxPreIncrDecr::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Base, ctx);

	ValueType = Base->ValueType;

	if (!Base->IsNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}
	else if (Base->ValueType == TypeBool)
	{
		ScriptPosition.Message(MSG_ERROR, "%s is not allowed on type bool", FScanner::TokenName(Token).GetChars());
		delete this;
		return nullptr;
	}
	if (!Base->RequestAddress(ctx, &AddressWritable) || !AddressWritable )
	{
		ScriptPosition.Message(MSG_ERROR, "Expression must be a modifiable value");
		delete this;
		return nullptr;
	}

	return this;
}

ExpEmit FxPreIncrDecr::Emit(VMFunctionBuilder *build)
{
	assert(Token == TK_Incr || Token == TK_Decr);
	assert(ValueType == Base->ValueType && IsNumeric());

	int zero = build->GetConstantInt(0);
	int regtype = ValueType->GetRegType();
	ExpEmit pointer = Base->Emit(build);
	ExpEmit value = pointer;

	if (!pointer.Target)
	{
		value = ExpEmit(build, regtype);
		build->Emit(ValueType->GetLoadOp(), value.RegNum, pointer.RegNum, zero);
	}

	if (regtype == REGT_INT)
	{
		build->Emit((Token == TK_Incr) ? OP_ADD_RK : OP_SUB_RK, value.RegNum, value.RegNum, build->GetConstantInt(1));
	}
	else
	{
		build->Emit((Token == TK_Incr) ? OP_ADDF_RK : OP_SUBF_RK, value.RegNum, value.RegNum, build->GetConstantFloat(1.));
	}

	if (!pointer.Target)
	{
		build->Emit(ValueType->GetStoreOp(), pointer.RegNum, value.RegNum, zero);
	}

	if (AddressRequested)
	{
		value.Free(build);
		return pointer;
	}

	pointer.Free(build);
	return value;
}

//==========================================================================
//
// FxPostIncrDecr
//
//==========================================================================

FxPostIncrDecr::FxPostIncrDecr(FxExpression *base, int token)
: FxExpression(EFX_PostIncrDecr, base->ScriptPosition), Token(token), Base(base)
{
}

FxPostIncrDecr::~FxPostIncrDecr()
{
	SAFE_DELETE(Base);
}

FxExpression *FxPostIncrDecr::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Base, ctx);
	bool AddressWritable;

	ValueType = Base->ValueType;

	if (!Base->IsNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}
	else if (Base->ValueType == TypeBool)
	{
		ScriptPosition.Message(MSG_ERROR, "%s is not allowed on type bool", FScanner::TokenName(Token).GetChars());
		delete this;
		return nullptr;
	}
	if (!Base->RequestAddress(ctx, &AddressWritable) || !AddressWritable)
	{
		ScriptPosition.Message(MSG_ERROR, "Expression must be a modifiable value");
		delete this;
		return nullptr;
	}

	return this;
}

ExpEmit FxPostIncrDecr::Emit(VMFunctionBuilder *build)
{
	assert(Token == TK_Incr || Token == TK_Decr);
	assert(ValueType == Base->ValueType && IsNumeric());

	int zero = build->GetConstantInt(0);
	int regtype = ValueType->GetRegType();
	ExpEmit pointer = Base->Emit(build);

	if (!pointer.Target)
	{
		ExpEmit out(build, regtype);
		build->Emit(ValueType->GetLoadOp(), out.RegNum, pointer.RegNum, zero);
		ExpEmit assign(build, regtype);
		if (regtype == REGT_INT)
		{
			build->Emit((Token == TK_Incr) ? OP_ADD_RK : OP_SUB_RK, assign.RegNum, out.RegNum, build->GetConstantInt(1));
		}
		else
		{
			build->Emit((Token == TK_Incr) ? OP_ADDF_RK : OP_SUBF_RK, assign.RegNum, out.RegNum, build->GetConstantFloat(1.));
		}
		build->Emit(ValueType->GetStoreOp(), pointer.RegNum, assign.RegNum, zero);
		pointer.Free(build);
		assign.Free(build);
		return out;
	}
	else if (NeedResult)
	{
		ExpEmit out(build, regtype);
		if (regtype == REGT_INT)
		{
			build->Emit(OP_MOVE, out.RegNum, pointer.RegNum);
			build->Emit((Token == TK_Incr) ? OP_ADD_RK : OP_SUB_RK, pointer.RegNum, pointer.RegNum, build->GetConstantInt(1));
		}
		else
		{
			build->Emit(OP_MOVEF, out.RegNum, pointer.RegNum);
			build->Emit((Token == TK_Incr) ? OP_ADDF_RK : OP_SUBF_RK, pointer.RegNum, pointer.RegNum, build->GetConstantFloat(1.));
		}
		pointer.Free(build);
		return out;
	}
	else
	{
		if (regtype == REGT_INT)
		{
			build->Emit((Token == TK_Incr) ? OP_ADD_RK : OP_SUB_RK, pointer.RegNum, pointer.RegNum, build->GetConstantInt(1));
		}
		else
		{
			build->Emit((Token == TK_Incr) ? OP_ADDF_RK : OP_SUBF_RK, pointer.RegNum, pointer.RegNum, build->GetConstantFloat(1.));
		}
		pointer.Free(build);
		return ExpEmit();
	}
}

//==========================================================================
//
// FxAssign
//
//==========================================================================

FxAssign::FxAssign(FxExpression *base, FxExpression *right, bool ismodify)
: FxExpression(EFX_Assign, base->ScriptPosition), Base(base), Right(right), IsBitWrite(-1), IsModifyAssign(ismodify)
{
	AddressRequested = false;
	AddressWritable = false;
}

FxAssign::~FxAssign()
{
	SAFE_DELETE(Base);
	SAFE_DELETE(Right);
}

/* I don't think we should allow constructs like (a = b) = c;...
bool FxAssign::RequestAddress(FCompileContext &ctx, bool *writable)
{
	AddressRequested = true;
	if (writable != nullptr) *writable = AddressWritable;
	return true;
}
*/

FxExpression *FxAssign::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Base, ctx);

	ValueType = Base->ValueType;

	SAFE_RESOLVE(Right, ctx);

	if (IsModifyAssign && Base->ValueType == TypeBool && Right->ValueType != TypeBool)
	{
		// If the modify operation resulted in a type promotion from bool to int, this must be blocked.
		// (this means, for bool, only &=, ^= and |= are allowed, although DECORATE is more lax.)
		ScriptPosition.Message(MSG_ERROR, "Invalid modify/assign operation with a boolean operand");
		delete this;
		return nullptr;
	}

	// keep the redundant handling for numeric types here to avoid problems with DECORATE.
	// for non-numerics FxTypeCast can be used without issues.
	if (Base->IsNumeric() && Right->IsNumeric())
	{
		if (Right->ValueType != ValueType)
		{
			if (ValueType == TypeBool)
			{
				Right = new FxBoolCast(Right);
			}
			else if (ValueType->GetRegType() == REGT_INT)
			{
				Right = new FxIntCast(Right, ctx.FromDecorate);
			}
			else
			{
				Right = new FxFloatCast(Right);
			}
			SAFE_RESOLVE(Right, ctx);
		}
	}
	else if (Base->ValueType == Right->ValueType)
	{
		if (Base->ValueType->IsKindOf(RUNTIME_CLASS(PArray)))
		{
			ScriptPosition.Message(MSG_ERROR, "Cannot assign arrays");
			delete this;
			return nullptr;
		}
		if (!Base->IsVector() && Base->ValueType->IsKindOf(RUNTIME_CLASS(PStruct)))
		{
			ScriptPosition.Message(MSG_ERROR, "Struct assignment not implemented yet");
			delete this;
			return nullptr;
		}
		// Both types are the same so this is ok.
	}
	else
	{
		// pass it to FxTypeCast for complete handling.
		Right = new FxTypeCast(Right, Base->ValueType, false);
		SAFE_RESOLVE(Right, ctx);
	}

	if (!Base->RequestAddress(ctx, &AddressWritable) || !AddressWritable)
	{
		ScriptPosition.Message(MSG_ERROR, "Expression must be a modifiable value");
		delete this;
		return nullptr;
	}

	// Special case: Assignment to a bitfield.
	if (Base->ExprType == EFX_StructMember || Base->ExprType == EFX_ClassMember)
	{
		auto f = static_cast<FxStructMember *>(Base)->membervar;
		if (f->BitValue != -1 && !ctx.CheckReadOnly(f->Flags))
		{
			IsBitWrite = f->BitValue;
			return this;
		}
	}

	return this;
}

ExpEmit FxAssign::Emit(VMFunctionBuilder *build)
{
	static const BYTE loadops[] = { OP_LK, OP_LKF, OP_LKS, OP_LKP };
	assert(ValueType == Base->ValueType);
	assert(ValueType->GetRegType() == Right->ValueType->GetRegType());

	ExpEmit pointer = Base->Emit(build);
	Address = pointer;

	ExpEmit result = Right->Emit(build);
	assert(result.RegType <= REGT_TYPE);

	if (pointer.Target)
	{
		if (result.Konst)
		{
			build->Emit(loadops[result.RegType], pointer.RegNum, result.RegNum);
		}
		else
		{
			build->Emit(Right->ValueType->GetMoveOp(), pointer.RegNum, result.RegNum);
		}
	}
	else
	{
		if (result.Konst)
		{
			ExpEmit temp(build, result.RegType);
			build->Emit(loadops[result.RegType], temp.RegNum, result.RegNum);
			result.Free(build);
			result = temp;
		}

		if (IsBitWrite == -1)
		{
			build->Emit(ValueType->GetStoreOp(), pointer.RegNum, result.RegNum, build->GetConstantInt(0));
		}
		else
		{
			build->Emit(OP_SBIT, pointer.RegNum, result.RegNum, 1 << IsBitWrite);
		}

	}

	if (AddressRequested)
	{
		result.Free(build);
		return pointer;
	}

	pointer.Free(build);
	return result;
}

//==========================================================================
//
//	FxAssignSelf
//
//==========================================================================

FxAssignSelf::FxAssignSelf(const FScriptPosition &pos)
: FxExpression(EFX_AssignSelf, pos)
{
	Assignment = nullptr;
}

FxExpression *FxAssignSelf::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();

	// This should never happen if FxAssignSelf is used correctly
	assert(Assignment != nullptr);

	ValueType = Assignment->ValueType;

	return this;
}

ExpEmit FxAssignSelf::Emit(VMFunctionBuilder *build)
{
	assert(ValueType == Assignment->ValueType);
	ExpEmit pointer = Assignment->Address; // FxAssign should have already emitted it
	if (!pointer.Target)
	{
		ExpEmit out(build, ValueType->GetRegType());
		if (Assignment->IsBitWrite != -1)
		{
			build->Emit(OP_LBIT, out.RegNum, pointer.RegNum, 1 << Assignment->IsBitWrite);
		}
		else
		{
			build->Emit(ValueType->GetLoadOp(), out.RegNum, pointer.RegNum, build->GetConstantInt(0));
		}
		return out;
	}
	else
	{
		return pointer;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FxBinary::FxBinary(int o, FxExpression *l, FxExpression *r)
: FxExpression(EFX_Binary, l->ScriptPosition)
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

	if (left->ValueType == TypeString || right->ValueType == TypeString)
	{
		switch (Operator)
		{
		case '+':
			// later
			break;

		case '<':
		case '>':
		case TK_Geq:
		case TK_Leq:
		case TK_Eq:
		case TK_Neq:
		case TK_ApproxEq:
			if (left->ValueType != TypeString)
			{
				left = new FxStringCast(left);
				left = left->Resolve(ctx);
				if (left == nullptr)
				{
					delete this;
					return false;
				}
			}
			if (right->ValueType != TypeString)
			{
				right = new FxStringCast(right);
				right = right->Resolve(ctx);
				if (right == nullptr)
				{
					delete this;
					return false;
				}
			}
			ValueType = TypeBool;
			break;

		default:
			ScriptPosition.Message(MSG_ERROR, "Incompatible operands for comparison");
			delete this;
			return false;
		}
	}
	else if (left->IsVector() || right->IsVector())
	{
		switch (Operator)
		{
		case '+':
		case '-':
			// a vector2 can be added to or subtracted from a vector 3 but it needs to be the right operand.
			if (left->ValueType == right->ValueType || (left->ValueType == TypeVector3 && right->ValueType == TypeVector2))
			{
				ValueType = left->ValueType;
				return true;
			}
			else
			{
				ScriptPosition.Message(MSG_ERROR, "Incompatible operands for operator %c", Operator);
				delete this;
				return false;
			}
			break;

		case '/':
			if (right->IsVector())
			{
				// For division, the vector must be the first operand.
				ScriptPosition.Message(MSG_ERROR, "Incompatible operands for division");
				delete this;
				return false;
			}
		case '*':
			if (left->IsVector())
			{
				right = new FxFloatCast(right);
				right = right->Resolve(ctx);
				if (right == nullptr)
				{
					delete this;
					return false;
				}
				ValueType = left->ValueType;
			}
			else
			{
				left = new FxFloatCast(left);
				left = left->Resolve(ctx);
				if (left == nullptr)
				{
					delete this;
					return false;
				}
				ValueType = right->ValueType;
			}
			break;

		case TK_Eq:
		case TK_Neq:
			if (left->ValueType != right->ValueType)
			{
				ScriptPosition.Message(MSG_ERROR, "Incompatible operands for comparison");
				delete this;
				return false;
			}
			ValueType = TypeBool;
			break;

		default:
			ScriptPosition.Message(MSG_ERROR, "Incompatible operation for vector type");
			delete this;
			return false;

		}
	}
	else if (left->ValueType == TypeBool && right->ValueType == TypeBool)
	{
		if (Operator == '&' || Operator == '|' || Operator == '^' || ctx.FromDecorate)
		{
			ValueType = TypeBool;
		}
		else
		{
			ValueType = TypeSInt32;	// math operations on bools result in integers.
		}
	}
	else if (left->ValueType == TypeName && right->ValueType == TypeName)
	{
		// pointers can only be compared for equality.
		if (Operator == TK_Eq || Operator == TK_Neq)
		{
			ValueType = TypeBool;
			return true;
		}
		else
		{
			ScriptPosition.Message(MSG_ERROR, "Invalid operation for names");
			delete this;
			return false;
		}
	}
	else if (left->IsNumeric() && right->IsNumeric())
	{
		if (left->ValueType->GetRegType() == REGT_INT && right->ValueType->GetRegType() == REGT_INT)
		{
			ValueType = TypeSInt32;
		}
		else
		{
			ValueType = TypeFloat64;
		}
	}
	else if (left->ValueType->GetRegType() == REGT_POINTER)
	{
		if (left->ValueType == right->ValueType || right->ValueType == TypeNullPtr || left->ValueType == TypeNullPtr ||
			AreCompatiblePointerTypes(left->ValueType, right->ValueType))
		{
			// pointers can only be compared for equality.
			if (Operator == TK_Eq || Operator == TK_Neq)
			{
				ValueType = TypeBool;
				return true;
			}
			else
			{
				ScriptPosition.Message(MSG_ERROR, "Invalid operation for pointers");
				delete this;
				return false;
			}
		}
	}
	else
	{
		// To check: It may be that this could pass in DECORATE, although setting TypeVoid here would pretty much prevent that.
		ScriptPosition.Message(MSG_ERROR, "Incompatible operator");
		delete this;
		return false;
	}
	assert(ValueType != nullptr && ValueType < (PType*)0xfffffffffffffff);

	if (castnumeric)
	{
		// later!
	}
	return true;
}

void FxBinary::Promote(FCompileContext &ctx)
{
	if (left->ValueType->GetRegType() == REGT_FLOAT && right->ValueType->GetRegType() == REGT_INT)
	{
		right = (new FxFloatCast(right))->Resolve(ctx);
	}
	else if (left->ValueType->GetRegType() == REGT_INT && right->ValueType->GetRegType() == REGT_FLOAT)
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
	if (!ResolveLR(ctx, true)) 
		return nullptr;

	if (!IsNumeric() && !IsVector())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}
	else if (left->isConstant() && right->isConstant())
	{
		if (IsFloat())
		{
			double v;
			double v1 = static_cast<FxConstant *>(left)->GetValue().GetFloat();
			double v2 = static_cast<FxConstant *>(right)->GetValue().GetFloat();

			v =	Operator == '+'? v1 + v2 : 
				Operator == '-'? v1 - v2 : 0;

			FxExpression *e = new FxConstant(v, ScriptPosition);
			delete this;
			return e;
		}
		else
		{
			int v;
			int v1 = static_cast<FxConstant *>(left)->GetValue().GetInt();
			int v2 = static_cast<FxConstant *>(right)->GetValue().GetInt();

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
		ExpEmit to(build, ValueType->GetRegType(), ValueType->GetRegCount());
		if (IsVector())
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			build->Emit(right->ValueType == TypeVector2? OP_ADDV2_RR : OP_ADDV3_RR, to.RegNum, op1.RegNum, op2.RegNum);
			if (left->ValueType == TypeVector3 && right->ValueType == TypeVector2 && to.RegNum != op1.RegNum)
			{
				// must move the z-coordinate
				build->Emit(OP_MOVEF, to.RegNum + 2, op1.RegNum + 2);
			}
			return to;
		}
		else if (ValueType->GetRegType() == REGT_FLOAT)
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			build->Emit(op2.Konst ? OP_ADDF_RK : OP_ADDF_RR, to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
		else
		{
			assert(ValueType->GetRegType() == REGT_INT);
			assert(op1.RegType == REGT_INT && op2.RegType == REGT_INT);
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
		ExpEmit to(build, ValueType->GetRegType(), ValueType->GetRegCount());
		if (IsVector())
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			build->Emit(right->ValueType == TypeVector2 ? OP_SUBV2_RR : OP_SUBV3_RR, to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
		else if (ValueType->GetRegType() == REGT_FLOAT)
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			build->Emit(op1.Konst ? OP_SUBF_KR : op2.Konst ? OP_SUBF_RK : OP_SUBF_RR, to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
		else
		{
			assert(ValueType->GetRegType() == REGT_INT);
			assert(op1.RegType == REGT_INT && op2.RegType == REGT_INT);
			build->Emit(op1.Konst ? OP_SUB_KR : op2.Konst ? OP_SUB_RK : OP_SUB_RR, to.RegNum, op1.RegNum, op2.RegNum);
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

	if (!ResolveLR(ctx, true)) 
		return nullptr;

	if (!IsNumeric() && !IsVector())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}
	else if (left->isConstant() && right->isConstant())
	{
		if (IsFloat())
		{
			double v;
			double v1 = static_cast<FxConstant *>(left)->GetValue().GetFloat();
			double v2 = static_cast<FxConstant *>(right)->GetValue().GetFloat();

			if (Operator != '*' && v2 == 0)
			{
				ScriptPosition.Message(MSG_ERROR, "Division by 0");
				delete this;
				return nullptr;
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
			int v1 = static_cast<FxConstant *>(left)->GetValue().GetInt();
			int v2 = static_cast<FxConstant *>(right)->GetValue().GetInt();

			if (Operator != '*' && v2 == 0)
			{
				ScriptPosition.Message(MSG_ERROR, "Division by 0");
				delete this;
				return nullptr;
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

ExpEmit FxMulDiv::Emit(VMFunctionBuilder *build)
{
	// allocate the result first so that the operation does not leave gaps in the register set.
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);

	if (IsVector())
	{
		assert(Operator != '%');
		if (right->IsVector())
		{
			swapvalues(op1, op2);
		}
		int op;
		if (op2.Konst)
		{
			op = Operator == '*' ? (ValueType == TypeVector2 ? OP_MULVF2_RK : OP_MULVF3_RK) : (ValueType == TypeVector2 ? OP_DIVVF2_RK : OP_DIVVF3_RK);
		}
		else
		{
			op = Operator == '*' ? (ValueType == TypeVector2 ? OP_MULVF2_RR : OP_MULVF3_RR) : (ValueType == TypeVector2 ? OP_DIVVF2_RR : OP_DIVVF3_RR);
		}
		op1.Free(build);
		op2.Free(build);
		ExpEmit to(build, ValueType->GetRegType(), ValueType->GetRegCount());
		build->Emit(op, to.RegNum, op1.RegNum, op2.RegNum);
		return to;
	}

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
		ExpEmit to(build, ValueType->GetRegType());
		if (ValueType->GetRegType() == REGT_FLOAT)
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			build->Emit(op2.Konst ? OP_MULF_RK : OP_MULF_RR, to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
		else
		{
			assert(ValueType->GetRegType() == REGT_INT);
			assert(op1.RegType == REGT_INT && op2.RegType == REGT_INT);
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
		ExpEmit to(build, ValueType->GetRegType());
		if (ValueType->GetRegType() == REGT_FLOAT)
		{
			assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
			build->Emit(Operator == '/' ? (op1.Konst ? OP_DIVF_KR : op2.Konst ? OP_DIVF_RK : OP_DIVF_RR)
				: (op1.Konst ? OP_MODF_KR : op2.Konst ? OP_MODF_RK : OP_MODF_RR),
				to.RegNum, op1.RegNum, op2.RegNum);
			return to;
		}
		else
		{
			assert(ValueType->GetRegType() == REGT_INT);
			assert(op1.RegType == REGT_INT && op2.RegType == REGT_INT);
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

FxPow::FxPow(FxExpression *l, FxExpression *r)
	: FxBinary(TK_MulMul, new FxFloatCast(l), new FxFloatCast(r))
{
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxPow::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();

	if (!ResolveLR(ctx, true)) 
		return nullptr;

	if (!IsNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}
	if (left->isConstant() && right->isConstant())
	{
		double v1 = static_cast<FxConstant *>(left)->GetValue().GetFloat();
		double v2 = static_cast<FxConstant *>(right)->GetValue().GetFloat();
		return new FxConstant(g_pow(v1, v2), left->ScriptPosition);
	}
	return this;
}


//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxPow::Emit(VMFunctionBuilder *build)
{
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);

	// Pow is not commutative, so either side may be constant (but not both).
	assert(!op1.Konst || !op2.Konst);
	op1.Free(build);
	op2.Free(build);
	assert(op1.RegType == REGT_FLOAT && op2.RegType == REGT_FLOAT);
	ExpEmit to(build, REGT_FLOAT);
	build->Emit((op1.Konst ? OP_POWF_KR : op2.Konst ? OP_POWF_RK : OP_POWF_RR),	to.RegNum, op1.RegNum, op2.RegNum);
	return to;
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
	if (!ResolveLR(ctx, true)) 
		return nullptr;

	if (!IsNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}
	else if (left->isConstant() && right->isConstant())
	{
		int v;

		if (ValueType == TypeString)
		{
			FString v1 = static_cast<FxConstant *>(left)->GetValue().GetString();
			FString v2 = static_cast<FxConstant *>(right)->GetValue().GetString();
			int res = v1.Compare(v2);
			v = Operator == '<' ? res < 0 :
				Operator == '>' ? res > 0 :
				Operator == TK_Geq ? res >= 0 :
				Operator == TK_Leq ? res <= 0 : 0;
		}
		else if (IsFloat())
		{
			double v1 = static_cast<FxConstant *>(left)->GetValue().GetFloat();
			double v2 = static_cast<FxConstant *>(right)->GetValue().GetFloat();
			v =	Operator == '<'? v1 < v2 : 
				Operator == '>'? v1 > v2 : 
				Operator == TK_Geq? v1 >= v2 : 
				Operator == TK_Leq? v1 <= v2 : 0;
		}
		else
		{
			int v1 = static_cast<FxConstant *>(left)->GetValue().GetInt();
			int v2 = static_cast<FxConstant *>(right)->GetValue().GetInt();
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
	ValueType = TypeBool;
	return this;
}


//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxCompareRel::Emit(VMFunctionBuilder *build)
{
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);
	assert(op1.RegType == op2.RegType);
	assert(!op1.Konst || !op2.Konst);

	if (op1.RegType == REGT_STRING)
	{
		ExpEmit to(build, REGT_INT);
		int a = Operator == '<' ? CMP_LT :
			Operator == '>' ? CMP_LE | CMP_CHECK :
			Operator == TK_Geq ? CMP_LT | CMP_CHECK : CMP_LE;

		if (op1.Konst)
		{
			a |= CMP_BK;
		}
		else
		{
			op1.Free(build);
		}
		if (op2.Konst)
		{
			a |= CMP_CK;
		}
		else
		{
			op2.Free(build);
		}

		build->Emit(OP_LI, to.RegNum, 0, 0);
		build->Emit(OP_CMPS, a, op1.RegNum, op2.RegNum);
		build->Emit(OP_JMP, 1);
		build->Emit(OP_LI, to.RegNum, 1);
		return to;
	}
	else
	{
		assert(op1.RegType == REGT_INT || op1.RegType == REGT_FLOAT);
		assert(Operator == '<' || Operator == '>' || Operator == TK_Geq || Operator == TK_Leq);
		static const VM_UBYTE InstrMap[][4] =
		{
			{ OP_LT_RR, OP_LTF_RR, 0 },	// <
			{ OP_LE_RR, OP_LEF_RR, 1 },	// >
			{ OP_LT_RR, OP_LTF_RR, 1 },	// >=
			{ OP_LE_RR, OP_LEF_RR, 0 }	// <=
		};
		int instr, check;
		ExpEmit to(build, REGT_INT);
		int index = Operator == '<' ? 0 :
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

		// See FxBoolCast for comments, since it's the same thing.
		build->Emit(OP_LI, to.RegNum, 0, 0);
		build->Emit(instr, check, op1.RegNum, op2.RegNum);
		build->Emit(OP_JMP, 1);
		build->Emit(OP_LI, to.RegNum, 1);
		return to;
	}
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

	if (!ResolveLR(ctx, true)) 
		return nullptr;

	if (!left || !right)
	{
		delete this;
		return nullptr;
	}

	if (!IsNumeric() && !IsPointer() && !IsVector() && ValueType != TypeName)
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}

	if (Operator == TK_ApproxEq && left->ValueType->GetRegType() != REGT_FLOAT && left->ValueType->GetRegType() != REGT_STRING) 
		Operator = TK_Eq;
	if (left->isConstant() && right->isConstant())
	{
		int v;

		if (ValueType == TypeString)
		{
			FString v1 = static_cast<FxConstant *>(left)->GetValue().GetString();
			FString v2 = static_cast<FxConstant *>(right)->GetValue().GetString();
			if (Operator == TK_ApproxEq) v = !v1.CompareNoCase(v2);
			else
			{
				v = !!v1.Compare(v2);
				if (Operator == TK_Eq) v = !v;
			}
		}
		else if (ValueType->GetRegType() == REGT_FLOAT)
		{
			double v1 = static_cast<FxConstant *>(left)->GetValue().GetFloat();
			double v2 = static_cast<FxConstant *>(right)->GetValue().GetFloat();
			v = Operator == TK_Eq? v1 == v2 : Operator == TK_Neq? v1 != v2 : fabs(v1-v2) < VM_EPSILON;
		}
		else
		{
			int v1 = static_cast<FxConstant *>(left)->GetValue().GetInt();
			int v2 = static_cast<FxConstant *>(right)->GetValue().GetInt();
			v = Operator == TK_Eq? v1 == v2 : v1 != v2;
		}
		FxExpression *e = new FxConstant(v, ScriptPosition);
		delete this;
		return e;
	}
	else
	{
		// also simplify comparisons against zero. For these a bool cast/unary not on the other value will do just as well and create better code.
		if (Operator != TK_ApproxEq)
		{
			if (left->isConstant())
			{
				bool leftisnull;
				switch (left->ValueType->GetRegType())
				{
				case REGT_INT:
					leftisnull = static_cast<FxConstant *>(left)->GetValue().GetInt() == 0;
					break;

				case REGT_FLOAT:
					assert(left->ValueType->GetRegCount() == 1);	// vectors should not be able to get here.
					leftisnull = static_cast<FxConstant *>(left)->GetValue().GetFloat() == 0;
					break;

				case REGT_POINTER:
					leftisnull = static_cast<FxConstant *>(left)->GetValue().GetPointer() == nullptr;
					break;

				default:
					leftisnull = false;
				}
				if (leftisnull)
				{
					FxExpression *x;
					if (Operator == TK_Eq) x = new FxUnaryNotBoolean(right);
					else x = new FxBoolCast(right);
					right = nullptr;
					delete this;
					return x->Resolve(ctx);
				}

			}
			if (right->isConstant())
			{
				bool rightisnull;
				switch (right->ValueType->GetRegType())
				{
				case REGT_INT:
					rightisnull = static_cast<FxConstant *>(right)->GetValue().GetInt() == 0;
					break;

				case REGT_FLOAT:
					assert(right->ValueType->GetRegCount() == 1);	// vectors should not be able to get here.
					rightisnull = static_cast<FxConstant *>(right)->GetValue().GetFloat() == 0;
					break;

				case REGT_POINTER:
					rightisnull = static_cast<FxConstant *>(right)->GetValue().GetPointer() == nullptr;
					break;

				default:
					rightisnull = false;
				}
				if (rightisnull)
				{
					FxExpression *x;
					if (Operator == TK_Eq) x = new FxUnaryNotBoolean(left);
					else x = new FxBoolCast(left);
					left = nullptr;
					delete this;
					return x->Resolve(ctx);
				}
			}
		}
	}
	Promote(ctx);
	ValueType = TypeBool;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxCompareEq::Emit(VMFunctionBuilder *build)
{
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);
	assert(op1.RegType == op2.RegType);
	int instr;

	if (op1.RegType == REGT_STRING)
	{
		ExpEmit to(build, REGT_INT);
		assert(Operator == TK_Eq || Operator == TK_Neq || Operator == TK_ApproxEq);
		int a = Operator == TK_Eq ? CMP_EQ :
			Operator == TK_Neq ? CMP_EQ | CMP_CHECK : CMP_EQ | CMP_APPROX;

		build->Emit(OP_LI, to.RegNum, 0, 0);
		build->Emit(OP_CMPS, a, op1.RegNum, op2.RegNum);
		build->Emit(OP_JMP, 1);
		build->Emit(OP_LI, to.RegNum, 1);
		return to;
	}
	else
	{

		// Only the second operand may be constant.
		if (op1.Konst)
		{
			swapvalues(op1, op2);
		}
		assert(!op1.Konst);
		assert(op1.RegCount >= 1 && op1.RegCount <= 3);

		ExpEmit to(build, REGT_INT);

		static int flops[] = { OP_EQF_R, OP_EQV2_R, OP_EQV3_R };
		instr = op1.RegType == REGT_INT ? OP_EQ_R :
			op1.RegType == REGT_FLOAT ? flops[op1.RegCount - 1] :
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

		// See FxUnaryNotBoolean for comments, since it's the same thing.
		build->Emit(OP_LI, to.RegNum, 0, 0);
		build->Emit(instr, Operator == TK_ApproxEq ? CMP_APPROX : ((Operator != TK_Eq) ? CMP_CHECK : 0), op1.RegNum, op2.RegNum);
		build->Emit(OP_JMP, 1);
		build->Emit(OP_LI, to.RegNum, 1);
		return to;
	}
}

//==========================================================================
//
//
//
//==========================================================================

FxBinaryInt::FxBinaryInt(int o, FxExpression *l, FxExpression *r)
: FxBinary(o, l, r)
{
	ValueType = TypeSInt32;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxBinaryInt::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	if (!ResolveLR(ctx, false)) 
		return nullptr;

	if (IsFloat() && ctx.FromDecorate)
	{
		// For DECORATE which allows floats here. ZScript does not.
		if (left->ValueType->GetRegType() != REGT_INT)
		{
			left = new FxIntCast(left, ctx.FromDecorate);
			left = left->Resolve(ctx);
		}
		if (right->ValueType->GetRegType() != REGT_INT)
		{
			right = new FxIntCast(right, ctx.FromDecorate);
			right = right->Resolve(ctx);
		}
		if (left == nullptr || right == nullptr)
		{
			delete this;
			return nullptr;
		}
		ValueType = TypeSInt32;
	}

	if (ValueType->GetRegType() != REGT_INT)
	{
		ScriptPosition.Message(MSG_ERROR, "Integer type expected");
		delete this;
		return nullptr;
	}
	else if (left->isConstant() && right->isConstant())
	{
		int v1 = static_cast<FxConstant *>(left)->GetValue().GetInt();
		int v2 = static_cast<FxConstant *>(right)->GetValue().GetInt();

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

ExpEmit FxBinaryInt::Emit(VMFunctionBuilder *build)
{
	assert(left->ValueType->GetRegType() == REGT_INT);
	assert(right->ValueType->GetRegType() == REGT_INT);
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
			rop = static_cast<FxConstant *>(right)->GetValue().GetInt();
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

FxLtGtEq::FxLtGtEq(FxExpression *l, FxExpression *r)
	: FxBinary(TK_LtGtEq, l, r)
{
	ValueType = TypeSInt32;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxLtGtEq::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	if (!ResolveLR(ctx, true)) 
		return nullptr;

	if (!left->IsNumeric() || !right->IsNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "<>= expects two numeric operands");
		delete this;
		return nullptr;
	}
	if (left->ValueType->GetRegType() != right->ValueType->GetRegType())
	{
		if (left->ValueType->GetRegType() == REGT_INT)
		{
			left = new FxFloatCast(left);
			SAFE_RESOLVE(left, ctx);
		}
		if (right->ValueType->GetRegType() == REGT_INT)
		{
			right = new FxFloatCast(left);
			SAFE_RESOLVE(left, ctx);
		}
	}

	else if (left->isConstant() && right->isConstant())
	{
		// let's cut this short and always compare doubles. For integers the result will be exactly the same as with an integer comparison, either signed or unsigned.
		auto v1 = static_cast<FxConstant *>(left)->GetValue().GetFloat();
		auto v2 = static_cast<FxConstant *>(right)->GetValue().GetFloat();
		auto e = new FxConstant(v1 < v2 ? -1 : v1 > v2 ? 1 : 0, ScriptPosition);
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

ExpEmit FxLtGtEq::Emit(VMFunctionBuilder *build)
{
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);

	assert(op1.RegType == op2.RegType);
	assert(op1.RegType == REGT_INT || op1.RegType == REGT_FLOAT);
	assert(!op1.Konst || !op2.Konst);

	ExpEmit to(build, REGT_INT);

	int instr = op1.RegType == REGT_INT ? (left->ValueType == TypeUInt32? OP_LTU_RR : OP_LT_RR) : OP_LTF_RR;
	if (op1.Konst) instr += 2;
	if (op2.Konst) instr++;


	build->Emit(OP_LI, to.RegNum, 1);										// default to 1
	build->Emit(instr, 0, op1.RegNum, op2.RegNum);							// if (left < right)
	auto j1 = build->Emit(OP_JMP, 1);
	build->Emit(OP_LI, to.RegNum, -1);										// result is -1
	auto j2 = build->Emit(OP_JMP, 1);										// jump to end
	build->BackpatchToHere(j1);
	build->Emit(instr + OP_LE_RR - OP_LT_RR, 0, op1.RegNum, op2.RegNum);	// if (left == right)
	auto j3 = build->Emit(OP_JMP, 1);
	build->Emit(OP_LI, to.RegNum, 0);										// result is 0
	build->BackpatchToHere(j2);
	build->BackpatchToHere(j3);

	return to;
}

//==========================================================================
//
//
//
//==========================================================================

FxBinaryLogical::FxBinaryLogical(int o, FxExpression *l, FxExpression *r)
: FxExpression(EFX_BinaryLogical, l->ScriptPosition)
{
	Operator=o;
	left=l;
	right=r;
	ValueType = TypeBool;
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
	RESOLVE(left, ctx);
	RESOLVE(right, ctx);
	ABORT(right && left);

	if (left->ValueType != TypeBool)
	{
		left = new FxBoolCast(left);
		SAFE_RESOLVE(left, ctx);
	}
	if (right->ValueType != TypeBool)
	{
		right = new FxBoolCast(right);
		SAFE_RESOLVE(right, ctx);
	}

	int b_left=-1, b_right=-1;
	if (left->isConstant()) b_left = static_cast<FxConstant *>(left)->GetValue().GetBool();
	if (right->isConstant()) b_right = static_cast<FxConstant *>(right)->GetValue().GetBool();

	// Do some optimizations. This will throw out all sub-expressions that are not
	// needed to retrieve the final result.
	if (Operator == TK_AndAnd)
	{
		if (b_left==0 || b_right==0)
		{
			FxExpression *x = new FxConstant(true, ScriptPosition);
			delete this;
			return x;
		}
		else if (b_left==1 && b_right==1)
		{
			FxExpression *x = new FxConstant(false, ScriptPosition);
			delete this;
			return x;
		}
		else if (b_left==1)
		{
			FxExpression *x = right;
			right=nullptr;
			delete this;
			return x;
		}
		else if (b_right==1)
		{
			FxExpression *x = left;
			left=nullptr;
			delete this;
			return x;
		}
	}
	else if (Operator == TK_OrOr)
	{
		if (b_left==1 || b_right==1)
		{
			FxExpression *x = new FxConstant(true, ScriptPosition);
			delete this;
			return x;
		}
		if (b_left==0 && b_right==0)
		{
			FxExpression *x = new FxConstant(false, ScriptPosition);
			delete this;
			return x;
		}
		else if (b_left==0)
		{
			FxExpression *x = right;
			right=nullptr;
			delete this;
			return x;
		}
		else if (b_right==0)
		{
			FxExpression *x = left;
			left=nullptr;
			delete this;
			return x;
		}
	}
	Flatten();
	return this;
}

//==========================================================================
//
// flatten a list of the same operator into a single node.
//
//==========================================================================

void FxBinaryLogical::Flatten()
{
	if (left->ExprType == EFX_BinaryLogical && static_cast<FxBinaryLogical *>(left)->Operator == Operator)
	{
		list = std::move(static_cast<FxBinaryLogical *>(left)->list);
		delete left;
	}
	else
	{
		list.Push(left);
	}

	if (right->ExprType == EFX_BinaryLogical && static_cast<FxBinaryLogical *>(right)->Operator == Operator)
	{
		auto &rlist = static_cast<FxBinaryLogical *>(right)->list;
		auto cnt = rlist.Size();
		auto v = list.Reserve(cnt);
		for (unsigned i = 0; i < cnt; i++)
		{
			list[v + i] = rlist[i];
			rlist[i] = nullptr;
		}
		delete right;
	}
	else
	{
		list.Push(right);
	}
	left = right = nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxBinaryLogical::Emit(VMFunctionBuilder *build)
{
	TArray<size_t> patchspots;

	int zero = build->GetConstantInt(0);
	for (unsigned i = 0; i < list.Size(); i++)
	{
		assert(list[i]->ValueType->GetRegType() == REGT_INT);
		ExpEmit op1 = list[i]->Emit(build);
		assert(!op1.Konst);
		op1.Free(build);
		build->Emit(OP_EQ_K, (Operator == TK_AndAnd) ? 1 : 0, op1.RegNum, zero);
		patchspots.Push(build->Emit(OP_JMP, 0, 0, 0));
	}
	ExpEmit to(build, REGT_INT);
	build->Emit(OP_LI, to.RegNum, (Operator == TK_AndAnd) ? 1 : 0);
	build->Emit(OP_JMP, 1);
	auto ctarget = build->Emit(OP_LI, to.RegNum, (Operator == TK_AndAnd) ? 0 : 1);
	for (auto addr : patchspots) build->Backpatch(addr, ctarget);
	return to;
}

//==========================================================================
//
//
//
//==========================================================================

FxDotCross::FxDotCross(int o, FxExpression *l, FxExpression *r)
	: FxExpression(EFX_DotCross, l->ScriptPosition)
{
	Operator = o;
	left = l;
	right = r;
}

//==========================================================================
//
//
//
//==========================================================================

FxDotCross::~FxDotCross()
{
	SAFE_DELETE(left);
	SAFE_DELETE(right);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxDotCross::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	RESOLVE(left, ctx);
	RESOLVE(right, ctx);
	ABORT(right && left);

	if (!left->IsVector() || left->ValueType != right->ValueType || (Operator == TK_Cross && left->ValueType != TypeVector3))
	{
		ScriptPosition.Message(MSG_ERROR, "Incompatible operants for %sproduct", Operator == TK_Cross ? "cross-" : "dot-");
		delete this;
		return nullptr;
	}
	ValueType = Operator == TK_Cross ? (PType*)TypeVector3 : TypeFloat64;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxDotCross::Emit(VMFunctionBuilder *build)
{
	ExpEmit to(build, ValueType->GetRegType(), ValueType->GetRegCount());
	ExpEmit op1 = left->Emit(build);
	ExpEmit op2 = right->Emit(build);
	int op = Operator == TK_Cross ? OP_CROSSV_RR : left->ValueType == TypeVector3 ? OP_DOTV3_RR : OP_DOTV2_RR;
	build->Emit(op, to.RegNum, op1.RegNum, op2.RegNum);
	op1.Free(build);
	op2.Free(build);
	return to;
}

//==========================================================================
//
//
//
//==========================================================================

FxTypeCheck::FxTypeCheck(FxExpression *l, FxExpression *r)
	: FxExpression(EFX_TypeCheck, l->ScriptPosition)
{
	left = new FxTypeCast(l, NewPointer(RUNTIME_CLASS(DObject)), false);
	right = new FxClassTypeCast(NewClassPointer(RUNTIME_CLASS(DObject)), r);
	EmitTail = false;
	ValueType = TypeBool;
}

//==========================================================================
//
//
//
//==========================================================================

FxTypeCheck::~FxTypeCheck()
{
	SAFE_DELETE(left);
	SAFE_DELETE(right);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxTypeCheck::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	RESOLVE(left, ctx);
	RESOLVE(right, ctx);
	ABORT(right && left);
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

PPrototype *FxTypeCheck::ReturnProto()
{
	EmitTail = true;
	return FxExpression::ReturnProto();
}


//==========================================================================
//
//
//
//==========================================================================

int BuiltinTypeCheck(VMFrameStack *stack, VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret)
{
	assert(numparam == 2);
	PARAM_POINTER_AT(0, obj, DObject);
	PARAM_CLASS_AT(1, cls, DObject);
	ACTION_RETURN_BOOL(obj && obj->IsKindOf(cls));
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxTypeCheck::Emit(VMFunctionBuilder *build)
{
	ExpEmit out(build, REGT_INT);
	EmitParameter(build, left, ScriptPosition);
	EmitParameter(build, right, ScriptPosition);


	PSymbol *sym = FindBuiltinFunction(NAME_BuiltinTypeCheck, BuiltinTypeCheck);

	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	auto callfunc = ((PSymbolVMFunction *)sym)->Function;

	int opcode = (EmitTail ? OP_TAIL_K : OP_CALL_K);
	build->Emit(opcode, build->GetConstantAddress(callfunc, ATAG_OBJECT), 2, 1);

	if (EmitTail)
	{
		ExpEmit call;
		call.Final = true;
		return call;
	}

	build->Emit(OP_RESULT, 0, REGT_INT, out.RegNum);
	return out;
}

//==========================================================================
//
//
//
//==========================================================================

FxDynamicCast::FxDynamicCast(PClass * cls, FxExpression *r)
	: FxExpression(EFX_DynamicCast, r->ScriptPosition)
{
	expr = r;
	CastType = cls;
}

//==========================================================================
//
//
//
//==========================================================================

FxDynamicCast::~FxDynamicCast()
{
	SAFE_DELETE(expr);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxDynamicCast::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(expr, ctx);
	bool constflag = expr->ValueType->IsKindOf(RUNTIME_CLASS(PPointer)) && static_cast<PPointer *>(expr->ValueType)->IsConst;
	expr = new FxTypeCast(expr, NewPointer(RUNTIME_CLASS(DObject), constflag), true, true);
	expr = expr->Resolve(ctx);
	if (expr == nullptr)
	{
		delete this;
		return nullptr;
	}
	ValueType = NewPointer(CastType, constflag);
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxDynamicCast::Emit(VMFunctionBuilder *build)
{
	ExpEmit out = expr->Emit(build);
	ExpEmit check(build, REGT_INT);
	assert(out.RegType == REGT_POINTER);

	build->Emit(OP_PARAM, 0, REGT_POINTER, out.RegNum);
	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(CastType, ATAG_OBJECT));

	PSymbol *sym = FindBuiltinFunction(NAME_BuiltinTypeCheck, BuiltinTypeCheck);
	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	auto callfunc = ((PSymbolVMFunction *)sym)->Function;

	build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 2, 1);
	build->Emit(OP_RESULT, 0, REGT_INT, check.RegNum);
	build->Emit(OP_EQ_K, 0, check.RegNum, build->GetConstantInt(0));
	auto patch = build->Emit(OP_JMP, 0);
	build->Emit(OP_LKP, out.RegNum, build->GetConstantAddress(nullptr, ATAG_OBJECT));
	build->BackpatchToHere(patch);
	return out;
}

//==========================================================================
//
//
//
//==========================================================================

FxConditional::FxConditional(FxExpression *c, FxExpression *t, FxExpression *f)
: FxExpression(EFX_Conditional, c->ScriptPosition)
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
	RESOLVE(condition, ctx);
	RESOLVE(truex, ctx);
	RESOLVE(falsex, ctx);
	ABORT(condition && truex && falsex);

	if (truex->ValueType == falsex->ValueType)
		ValueType = truex->ValueType;
	else if (truex->ValueType == TypeBool && falsex->ValueType == TypeBool)
		ValueType = TypeBool;
	else if (truex->IsInteger() && falsex->IsInteger())
		ValueType = TypeSInt32;
	else if (truex->IsNumeric() && falsex->IsNumeric())
		ValueType = TypeFloat64;
	else
		ValueType = TypeVoid;
	//else if (truex->ValueType != falsex->ValueType)

	if (ValueType->GetRegType() == REGT_NIL)
	{
		ScriptPosition.Message(MSG_ERROR, "Incompatible types for ?: operator");
		delete this;
		return nullptr;
	}

	if (condition->ValueType != TypeBool)
	{
		condition = new FxBoolCast(condition);
		SAFE_RESOLVE(condition, ctx);
	}

	if (condition->isConstant())
	{
		ExpVal condval = static_cast<FxConstant *>(condition)->GetValue();
		bool result = condval.GetBool();

		FxExpression *e = result? truex:falsex;
		delete (result? falsex:truex);
		falsex = truex = nullptr;
		delete this;
		return e;
	}

	if (IsFloat())
	{
		if (truex->ValueType->GetRegType() != REGT_FLOAT)
		{
			truex = new FxFloatCast(truex);
			RESOLVE(truex, ctx);
		}
		if (falsex->ValueType->GetRegType() != REGT_FLOAT)
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

ExpEmit FxConditional::Emit(VMFunctionBuilder *build)
{
	size_t truejump, falsejump;
	ExpEmit out;

	// The true and false expressions ought to be assigned to the
	// same temporary instead of being copied to it. Oh well; good enough
	// for now.
	ExpEmit cond = condition->Emit(build);
	assert(cond.RegType == REGT_INT && !cond.Konst);

	// Test condition.
	build->Emit(OP_EQ_K, 1, cond.RegNum, build->GetConstantInt(0));
	falsejump = build->Emit(OP_JMP, 0);
	cond.Free(build);

	// Evaluate true expression.
	if (truex->isConstant() && truex->ValueType->GetRegType() == REGT_INT)
	{
		out = ExpEmit(build, REGT_INT);
		build->EmitLoadInt(out.RegNum, static_cast<FxConstant *>(truex)->GetValue().GetInt());
	}
	else
	{
		ExpEmit trueop = truex->Emit(build);
		if (trueop.Konst)
		{
			trueop.Free(build);
			if (trueop.RegType == REGT_FLOAT)
			{
				out = ExpEmit(build, REGT_FLOAT);
				build->Emit(OP_LKF, out.RegNum, trueop.RegNum);
			}
			else if (trueop.RegType == REGT_POINTER)
			{
				out = ExpEmit(build, REGT_POINTER);
				build->Emit(OP_LKP, out.RegNum, trueop.RegNum);
			}
			else
			{
				assert(trueop.RegType == REGT_STRING);
				out = ExpEmit(build, REGT_STRING);
				build->Emit(OP_LKS, out.RegNum, trueop.RegNum);
			}
		}
		else
		{
			// Use the register returned by the true condition as the
			// target for the false condition.
			out = trueop;
		}
	}
	// Make sure to skip the false path.
	truejump = build->Emit(OP_JMP, 0);

	// Evaluate false expression.
	build->BackpatchToHere(falsejump);
	if (falsex->isConstant() && falsex->ValueType->GetRegType() == REGT_INT)
	{
		build->EmitLoadInt(out.RegNum, static_cast<FxConstant *>(falsex)->GetValue().GetInt());
	}
	else
	{
		ExpEmit falseop = falsex->Emit(build);
		if (falseop.Konst)
		{
			if (falseop.RegType == REGT_FLOAT)
			{
				build->Emit(OP_LKF, out.RegNum, falseop.RegNum);
			}
			else if (falseop.RegType == REGT_POINTER)
			{
				build->Emit(OP_LKP, out.RegNum, falseop.RegNum);
			}
			else
			{
				assert(falseop.RegType == REGT_STRING);
				build->Emit(OP_LKS, out.RegNum, falseop.RegNum);
			}
			falseop.Free(build);
		}
		else
		{
			// Move result from the register returned by "false" to the one
			// returned by "true" so that only one register is returned by
			// this tree.
			falseop.Free(build);
			build->Emit(falsex->ValueType->GetMoveOp(), out.RegNum, falseop.RegNum, 0);
		}
	}
	build->BackpatchToHere(truejump);

	return out;
}

//==========================================================================
//
//
//
//==========================================================================
FxAbs::FxAbs(FxExpression *v)
: FxExpression(EFX_Abs, v->ScriptPosition)
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


	if (!val->IsNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}
	else if (val->isConstant())
	{
		ExpVal value = static_cast<FxConstant *>(val)->GetValue();
		switch (value.Type->GetRegType())
		{
		case REGT_INT:
			value.Int = abs(value.Int);
			break;

		case REGT_FLOAT:
			value.Float = fabs(value.Float);
			break;

		default:
			// shouldn't happen
			delete this;
			return nullptr;
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
FxATan2::FxATan2(FxExpression *y, FxExpression *x, const FScriptPosition &pos)
: FxExpression(EFX_ATan2, pos)
{
	yval = y;
	xval = x;
}

//==========================================================================
//
//
//
//==========================================================================
FxATan2::~FxATan2()
{
	SAFE_DELETE(yval);
	SAFE_DELETE(xval);
}

//==========================================================================
//
//
//
//==========================================================================
FxExpression *FxATan2::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(yval, ctx);
	SAFE_RESOLVE(xval, ctx);

	if (!yval->IsNumeric() || !xval->IsNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "numeric value expected for parameter");
		delete this;
		return nullptr;
	}
	if (yval->isConstant() && xval->isConstant())
	{
		double y = static_cast<FxConstant *>(yval)->GetValue().GetFloat();
		double x = static_cast<FxConstant *>(xval)->GetValue().GetFloat();
		FxExpression *z = new FxConstant(g_atan2(y, x) * (180 / M_PI), ScriptPosition);
		delete this;
		return z;
	}
	if (yval->ValueType->GetRegType() != REGT_FLOAT && !yval->isConstant())
	{
		yval = new FxFloatCast(yval);
	}
	if (xval->ValueType->GetRegType() != REGT_FLOAT && !xval->isConstant())
	{
		xval = new FxFloatCast(xval);
	}
	ValueType = TypeFloat64;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================
ExpEmit FxATan2::Emit(VMFunctionBuilder *build)
{
	ExpEmit yreg = ToReg(build, yval);
	ExpEmit xreg = ToReg(build, xval);
	yreg.Free(build);
	xreg.Free(build);
	ExpEmit out(build, REGT_FLOAT);
	build->Emit(OP_ATAN2, out.RegNum, yreg.RegNum, xreg.RegNum);
	return out;
}

//==========================================================================
//
// The atan2 opcode only takes registers as parameters, so any constants
// must be loaded into registers first.
//
//==========================================================================
ExpEmit FxATan2::ToReg(VMFunctionBuilder *build, FxExpression *val)
{
	if (val->isConstant())
	{
		ExpEmit reg(build, REGT_FLOAT);
		build->Emit(OP_LKF, reg.RegNum, build->GetConstantFloat(static_cast<FxConstant*>(val)->GetValue().GetFloat()));
		return reg;
	}
	return val->Emit(build);
}

//==========================================================================
//
//
//
//==========================================================================
FxMinMax::FxMinMax(TArray<FxExpression*> &expr, FName type, const FScriptPosition &pos)
: FxExpression(EFX_MinMax, pos), Type(type)
{
	assert(expr.Size() > 0);
	assert(type == NAME_Min || type == NAME_Max);

	choices.Resize(expr.Size());
	for (unsigned i = 0; i < expr.Size(); ++i)
	{
		choices[i] = expr[i];
	}
}

//==========================================================================
//
//
//
//==========================================================================
FxExpression *FxMinMax::Resolve(FCompileContext &ctx)
{
	unsigned int i;
	int intcount, floatcount;

	CHECKRESOLVED();

	// Determine if float or int
	intcount = floatcount = 0;
	for (i = 0; i < choices.Size(); ++i)
	{
		RESOLVE(choices[i], ctx);
		ABORT(choices[i]);

		if (choices[i]->IsFloat())
		{
			floatcount++;
		}
		else if (choices[i]->IsInteger())
		{
			intcount++;
		}
		else
		{
			ScriptPosition.Message(MSG_ERROR, "Arguments must be of type int or float");
			delete this;
			return nullptr;
		}
	}
	if (floatcount != 0)
	{
		ValueType = TypeFloat64;
		if (intcount != 0)
		{ // There are some ints that need to be cast to floats
			for (i = 0; i < choices.Size(); ++i)
			{
				if (choices[i]->ValueType->GetRegType() == REGT_INT)
				{
					choices[i] = new FxFloatCast(choices[i]);
					RESOLVE(choices[i], ctx);
					ABORT(choices[i]);
				}
			}
		}
	}
	else
	{
		ValueType = TypeSInt32;
	}

	// If at least two arguments are constants, they can be solved now.

	// Look for first constant
	for (i = 0; i < choices.Size(); ++i)
	{
		if (choices[i]->isConstant())
		{
			ExpVal best = static_cast<FxConstant *>(choices[i])->GetValue();
			// Compare against remaining constants, which are removed.
			// The best value gets stored in this one.
			for (unsigned j = i + 1; j < choices.Size(); )
			{
				if (!choices[j]->isConstant())
				{
					j++;
				}
				else
				{
					ExpVal value = static_cast<FxConstant *>(choices[j])->GetValue();
					assert(value.Type == ValueType);
					if (Type == NAME_Min)
					{
						if (value.Type->GetRegType() == REGT_FLOAT)
						{
							if (value.Float < best.Float)
							{
								best.Float = value.Float;
							}
						}
						else
						{
							if (value.Int < best.Int)
							{
								best.Int = value.Int;
							}
						}
					}
					else
					{
						if (value.Type->GetRegType() == REGT_FLOAT)
						{
							if (value.Float > best.Float)
							{
								best.Float = value.Float;
							}
						}
						else
						{
							if (value.Int > best.Int)
							{
								best.Int = value.Int;
							}
						}
					}
					delete choices[j];
					choices[j] = nullptr;
					choices.Delete(j);
				}
			}
			FxExpression *x = new FxConstant(best, ScriptPosition);
			if (i == 0 && choices.Size() == 1)
			{ // Every choice was constant
				delete this;
				return x;
			}
			delete choices[i];
			choices[i] = x;
			break;
		}
	}
	return this;
}

//==========================================================================
//
//
//
//==========================================================================
static void EmitLoad(VMFunctionBuilder *build, const ExpEmit resultreg, const ExpVal &value)
{
	if (resultreg.RegType == REGT_FLOAT)
	{
		build->Emit(OP_LKF, resultreg.RegNum, build->GetConstantFloat(value.GetFloat()));
	}
	else
	{
		build->EmitLoadInt(resultreg.RegNum, value.GetInt());
	}
}

ExpEmit FxMinMax::Emit(VMFunctionBuilder *build)
{
	unsigned i;
	int opcode, opA;

	assert(choices.Size() > 0);
	assert(OP_LTF_RK == OP_LTF_RR+1);
	assert(OP_LT_RK == OP_LT_RR+1);
	assert(OP_LEF_RK == OP_LEF_RR+1);
	assert(OP_LE_RK == OP_LE_RR+1);

	if (Type == NAME_Min)
	{
		opcode = ValueType->GetRegType() == REGT_FLOAT ? OP_LEF_RR : OP_LE_RR;
		opA = 1;
	}
	else
	{
		opcode = ValueType->GetRegType() == REGT_FLOAT ? OP_LTF_RR : OP_LT_RR;
		opA = 0;
	}

	ExpEmit bestreg;

	// Get first value into a register. This will also be the result register.
	if (choices[0]->isConstant())
	{
		bestreg = ExpEmit(build, ValueType->GetRegType());
		EmitLoad(build, bestreg, static_cast<FxConstant *>(choices[0])->GetValue());
	}
	else
	{
		bestreg = choices[0]->Emit(build);
	}

	// Compare every choice. Better matches get copied to the bestreg.
	for (i = 1; i < choices.Size(); ++i)
	{
		ExpEmit checkreg = choices[i]->Emit(build);
		assert(checkreg.RegType == bestreg.RegType);
		build->Emit(opcode + checkreg.Konst, opA, bestreg.RegNum, checkreg.RegNum);
		build->Emit(OP_JMP, 1);
		if (checkreg.Konst)
		{
			build->Emit(bestreg.RegType == REGT_FLOAT ? OP_LKF : OP_LK, bestreg.RegNum, checkreg.RegNum);
		}
		else
		{
			build->Emit(bestreg.RegType == REGT_FLOAT ? OP_MOVEF : OP_MOVE, bestreg.RegNum, checkreg.RegNum, 0);
			checkreg.Free(build);
		}
	}
	return bestreg;
}

//==========================================================================
//
//
//
//==========================================================================
FxRandom::FxRandom(FRandom * r, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos, bool nowarn)
: FxExpression(EFX_Random, pos)
{
	EmitTail = false;
	if (mi != nullptr && ma != nullptr)
	{
		min = new FxIntCast(mi, nowarn);
		max = new FxIntCast(ma, nowarn);
	}
	else min = max = nullptr;
	rng = r;
	ValueType = TypeSInt32;
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

PPrototype *FxRandom::ReturnProto()
{
	EmitTail = true;
	return FxExpression::ReturnProto();
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
		assert(min->ValueType == ValueType);
		assert(max->ValueType == ValueType);
	}
	return this;
};


//==========================================================================
//
//
//
//==========================================================================

int BuiltinRandom(VMFrameStack *stack, VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret)
{
	assert(numparam >= 1 && numparam <= 3);
	FRandom *rng = reinterpret_cast<FRandom *>(param[0].a);
	if (numparam == 1)
	{
		ACTION_RETURN_INT((*rng)());
	}
	else if (numparam == 2)
	{
		int maskval = param[1].i;
		ACTION_RETURN_INT(rng->Random2(maskval));
	}
	else if (numparam == 3)
	{
		int min = param[1].i, max = param[2].i;
		if (max < min)
		{
			swapvalues(max, min);
		}
		ACTION_RETURN_INT((*rng)(max - min + 1) + min);
	}

	// Shouldn't happen
	return 0;
}

ExpEmit FxRandom::Emit(VMFunctionBuilder *build)
{
	// Call DecoRandom to generate a random number.
	VMFunction *callfunc;
	PSymbol *sym = FindBuiltinFunction(NAME_BuiltinRandom, BuiltinRandom);

	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	int opcode = (EmitTail ? OP_TAIL_K : OP_CALL_K);

	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(rng, ATAG_RNG));
	if (min != nullptr && max != nullptr)
	{
		EmitParameter(build, min, ScriptPosition);
		EmitParameter(build, max, ScriptPosition);
		build->Emit(opcode, build->GetConstantAddress(callfunc, ATAG_OBJECT), 3, 1);
	}
	else
	{
		build->Emit(opcode, build->GetConstantAddress(callfunc, ATAG_OBJECT), 1, 1);
	}

	if (EmitTail)
	{
		ExpEmit call;
		call.Final = true;
		return call;
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
FxRandomPick::FxRandomPick(FRandom *r, TArray<FxExpression*> &expr, bool floaty, const FScriptPosition &pos, bool nowarn)
: FxExpression(EFX_RandomPick, pos)
{
	assert(expr.Size() > 0);
	choices.Resize(expr.Size());
	for (unsigned int index = 0; index < expr.Size(); index++)
	{
		if (floaty)
		{
			choices[index] = new FxFloatCast(expr[index]);
		}
		else
		{
			choices[index] = new FxIntCast(expr[index], nowarn);
		}

	}
	rng = r;
	if (floaty)
	{
		ValueType = TypeFloat64;
	}
	else
	{
		ValueType = TypeSInt32;
	}
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
	for (unsigned int index = 0; index < choices.Size(); index++)
	{
		RESOLVE(choices[index], ctx);
		ABORT(choices[index]);
		assert(choices[index]->ValueType == ValueType);
	}
	return this;
};


//==========================================================================
//
// FxPick :: Emit
//
// The expression:
//   a = pick[rng](i_0, i_1, i_2, ..., i_n)
//   [where i_x is a complete expression and not just a value]
// is syntactic sugar for:
//
//   switch(random[rng](0, n)) {
//     case 0: a = i_0;
//     case 1: a = i_1;
//     case 2: a = i_2;
//     ...
//     case n: a = i_n;
//   }
//
//==========================================================================

ExpEmit FxRandomPick::Emit(VMFunctionBuilder *build)
{
	unsigned i;

	assert(choices.Size() > 0);

	// Call BuiltinRandom to generate a random number.
	VMFunction *callfunc;
	PSymbol *sym = FindBuiltinFunction(NAME_BuiltinRandom, BuiltinRandom);

	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(rng, ATAG_RNG));
	build->EmitParamInt(0);
	build->EmitParamInt(choices.Size() - 1);
	build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 3, 1);

	ExpEmit resultreg(build, REGT_INT);
	build->Emit(OP_RESULT, 0, REGT_INT, resultreg.RegNum);
	build->Emit(OP_IJMP, resultreg.RegNum, 0);

	// Free the result register now. The simple code generation algorithm should
	// automatically pick it as the destination register for each case.
	resultreg.Free(build);

	// For floating point results, we need to get a new register, since we can't
	// reuse the integer one used to store the random result.
	if (ValueType->GetRegType() == REGT_FLOAT)
	{
		resultreg = ExpEmit(build, REGT_FLOAT);
		resultreg.Free(build);
	}

	// Allocate space for the jump table.
	size_t jumptable = build->Emit(OP_JMP, 0);
	for (i = 1; i < choices.Size(); ++i)
	{
		build->Emit(OP_JMP, 0);
	}

	// Emit each case
	TArray<size_t> finishes(choices.Size() - 1);
	for (unsigned i = 0; i < choices.Size(); ++i)
	{
		build->BackpatchToHere(jumptable + i);
		if (choices[i]->isConstant())
		{
			EmitLoad(build, resultreg, static_cast<FxConstant *>(choices[i])->GetValue());
		}
		else
		{
			ExpEmit casereg = choices[i]->Emit(build);
			if (casereg.RegNum != resultreg.RegNum)
			{ // The result of the case is in a different register from what
			  // was expected. Copy it to the one we wanted.

				resultreg.Reuse(build);	// This is really just for the assert in Reuse()
				build->Emit(ValueType->GetRegType() == REGT_INT ? OP_MOVE : OP_MOVEF, resultreg.RegNum, casereg.RegNum, 0);
				resultreg.Free(build);
			}
			// Free this register so the remaining cases can use it.
			casereg.Free(build);
		}
		// All but the final case needs a jump to the end of the expression's code.
		if (i + 1 < choices.Size())
		{
			size_t loc = build->Emit(OP_JMP, 0);
			finishes.Push(loc);
		}
	}
	// Backpatch each case (except the last, since it ends here) to jump to here.
	for (i = 0; i < choices.Size() - 1; ++i)
	{
		build->BackpatchToHere(finishes[i]);
	}
	// The result register needs to be in-use when we return.
	// It should have been freed earlier, so restore its in-use flag.
	resultreg.Reuse(build);
	return resultreg;
}

//==========================================================================
//
//
//
//==========================================================================
FxFRandom::FxFRandom(FRandom *r, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos)
: FxRandom(r, nullptr, nullptr, pos, true)
{
	if (mi != nullptr && ma != nullptr)
	{
		min = new FxFloatCast(mi);
		max = new FxFloatCast(ma);
	}
	ValueType = TypeFloat64;
	ExprType = EFX_FRandom;
}

//==========================================================================
//
//
//
//==========================================================================

int BuiltinFRandom(VMFrameStack *stack, VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret)
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
		ACTION_RETURN_FLOAT(frandom * (max - min) + min);
	}
	else
	{
		ACTION_RETURN_FLOAT(frandom);
	}
}

ExpEmit FxFRandom::Emit(VMFunctionBuilder *build)
{
	// Call the BuiltinFRandom function to generate a floating point random number..
	VMFunction *callfunc;
	PSymbol *sym = FindBuiltinFunction(NAME_BuiltinFRandom, BuiltinFRandom);

	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	int opcode = (EmitTail ? OP_TAIL_K : OP_CALL_K);

	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(rng, ATAG_RNG));
	if (min != nullptr && max != nullptr)
	{
		EmitParameter(build, min, ScriptPosition);
		EmitParameter(build, max, ScriptPosition);
		build->Emit(opcode, build->GetConstantAddress(callfunc, ATAG_OBJECT), 3, 1);
	}
	else
	{
		build->Emit(opcode, build->GetConstantAddress(callfunc, ATAG_OBJECT), 1, 1);
	}

	if (EmitTail)
	{
		ExpEmit call;
		call.Final = true;
		return call;
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

FxRandom2::FxRandom2(FRandom *r, FxExpression *m, const FScriptPosition &pos, bool nowarn)
: FxExpression(EFX_Random2, pos)
{
	EmitTail = false;
	rng = r;
	if (m) mask = new FxIntCast(m, nowarn);
	else mask = new FxConstant(-1, pos);
	ValueType = TypeSInt32;
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

PPrototype *FxRandom2::ReturnProto()
{
	EmitTail = true;
	return FxExpression::ReturnProto();
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

ExpEmit FxRandom2::Emit(VMFunctionBuilder *build)
{
	// Call the BuiltinRandom function to generate the random number.
	VMFunction *callfunc;
	PSymbol *sym = FindBuiltinFunction(NAME_BuiltinRandom, BuiltinRandom);

	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	int opcode = (EmitTail ? OP_TAIL_K : OP_CALL_K);

	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(rng, ATAG_RNG));
	EmitParameter(build, mask, ScriptPosition);
	build->Emit(opcode, build->GetConstantAddress(callfunc, ATAG_OBJECT), 2, 1);

	if (EmitTail)
	{
		ExpEmit call;
		call.Final = true;
		return call;
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

FxIdentifier::FxIdentifier(FName name, const FScriptPosition &pos)
: FxExpression(EFX_Identifier, pos)
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
	FxExpression *newex = nullptr;
	int num;
	
	CHECKRESOLVED();

	// Local variables have highest priority.
	FxLocalVariableDeclaration *local = ctx.FindLocalVariable(Identifier);
	if (local != nullptr)
	{
		auto x = new FxLocalVariable(local, ScriptPosition);
		delete this;
		return x->Resolve(ctx);
	}

	if (Identifier == NAME_Default)
	{
		if (ctx.Function->Variants[0].SelfClass == nullptr)
		{
			ScriptPosition.Message(MSG_ERROR, "Unable to access class defaults from static function");
			delete this;
			return nullptr;
		}
		if (!ctx.Function->Variants[0].SelfClass->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			ScriptPosition.Message(MSG_ERROR, "'Default' requires an actor type.");
			delete this;
			return nullptr;
		}

		FxExpression * x = new FxClassDefaults(new FxSelf(ScriptPosition), ScriptPosition);
		delete this;
		return x->Resolve(ctx);
	}

	// Ugh, the horror. Constants need to be taken from the owning class, but members from the self class to catch invalid accesses here...
	// see if the current class (if valid) defines something with this name.
	PSymbolTable *symtbl;
	if ((sym = ctx.FindInClass(Identifier, symtbl)) != nullptr)
	{
		if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConst)))
		{
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as class constant\n", Identifier.GetChars());
			newex = FxConstant::MakeConstant(sym, ScriptPosition);
		}
		else if (sym->IsKindOf(RUNTIME_CLASS(PField)))
		{
			if (!ctx.Function)
			{
				ScriptPosition.Message(MSG_ERROR, "Cannot resolve class member outside a function", sym->SymbolName.GetChars());
				delete this;
				return nullptr;
			}
			PField *vsym = static_cast<PField*>(sym);

			// We have 4 cases to consider here:
			// 1. The symbol is a static/meta member (not implemented yet) which is always accessible.
			// 2. This is a static function 
			// 3. This is an action function with a restricted self pointer
			// 4. This is a normal member or unrestricted action function.
			if ((vsym->Flags & VARF_Deprecated) && !ctx.FromDecorate)
			{
				ScriptPosition.Message(MSG_WARNING, "Accessing deprecated member variable %s", sym->SymbolName.GetChars());
			}
			if ((vsym->Flags & VARF_Private) && symtbl != &ctx.Class->Symbols)
			{
				ScriptPosition.Message(MSG_ERROR, "Private member %s not accessible", sym->SymbolName.GetChars());
				delete this;
				return nullptr;
			}

			if (vsym->Flags & VARF_Static)
			{
				// todo. For now these cannot be defined so let's just exit.
				ScriptPosition.Message(MSG_ERROR, "Static members not implemented yet.");
				delete this;
				return nullptr;
			}

			if (ctx.Function->Variants[0].SelfClass == nullptr)
			{
				ScriptPosition.Message(MSG_ERROR, "Unable to access class member from static function");
				delete this;
				return nullptr;
			}

			if (ctx.Function->Variants[0].SelfClass != ctx.Class)
			{
				// Check if the restricted class can access it.
				PSymbol *sym2;
				if ((sym2 = ctx.FindInSelfClass(Identifier, symtbl)) != nullptr)
				{
					if (sym != sym2)
					{
						ScriptPosition.Message(MSG_ERROR, "Member variable of %s not accessible through restricted self pointer", ctx.Class->TypeName.GetChars());
						delete this;
						return nullptr;
					}
				}
			}
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as member variable, index %d\n", Identifier.GetChars(), vsym->Offset);
			newex = new FxClassMember((new FxSelf(ScriptPosition))->Resolve(ctx), vsym, ScriptPosition);
		}
		else
		{
			if (sym->IsKindOf(RUNTIME_CLASS(PFunction)))
			{
				ScriptPosition.Message(MSG_ERROR, "Function '%s' used without ().\n", Identifier.GetChars());
			}
			else
			{
				ScriptPosition.Message(MSG_ERROR, "Invalid member identifier '%s'.\n", Identifier.GetChars());
			}
			delete this;
			return nullptr;
		}
	}

	// now check the global identifiers.
	else if ((sym = ctx.FindGlobal(Identifier)) != nullptr)
	{
		if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConst)))
		{
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as global constant\n", Identifier.GetChars());
			newex = FxConstant::MakeConstant(sym, ScriptPosition);
		}
		else if (sym->IsKindOf(RUNTIME_CLASS(PField)))
		{
			// internally defined global variable
			ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as global variable\n", Identifier.GetChars());
			newex = new FxGlobalVariable(static_cast<PField *>(sym), ScriptPosition);
		}
		else
		{
			ScriptPosition.Message(MSG_ERROR, "Invalid global identifier '%s'\n", Identifier.GetChars());
		}
	}

	// and line specials
	else if ((num = P_FindLineSpecial(Identifier, nullptr, nullptr)))
	{
		ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as line special %d\n", Identifier.GetChars(), num);
		newex = new FxConstant(num, ScriptPosition);
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Unknown identifier '%s'", Identifier.GetChars());
		delete this;
		return nullptr;
	}
	delete this;
	return newex? newex->Resolve(ctx) : nullptr;
}


//==========================================================================
//
//
//
//==========================================================================

FxMemberIdentifier::FxMemberIdentifier(FxExpression *left, FName name, const FScriptPosition &pos)
	: FxIdentifier(name, pos)
{
	Object = left;
	ExprType = EFX_MemberIdentifier;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxMemberIdentifier::Resolve(FCompileContext& ctx)
{
	PSymbol * sym;
	FxExpression *newex = nullptr;

	CHECKRESOLVED();

	SAFE_RESOLVE(Object, ctx);

	if (Object->ValueType->IsKindOf(RUNTIME_CLASS(PPointer)))
	{
		PSymbolTable *symtbl;
		auto ptype = static_cast<PPointer *>(Object->ValueType)->PointedType;

		if (ptype->IsKindOf(RUNTIME_CLASS(PStruct)))	// PClass is a child class of PStruct so this covers both.
		{
			PStruct *cls = static_cast<PStruct *>(ptype);
			bool isclass = cls->IsKindOf(RUNTIME_CLASS(PClass));
			if ((sym = cls->Symbols.FindSymbolInTable(Identifier, symtbl)) != nullptr)
			{
				if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConst)))
				{
					ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as %s constant\n", Identifier.GetChars(), isclass? "class" : "struct");
					newex = FxConstant::MakeConstant(sym, ScriptPosition);
				}
				else if (sym->IsKindOf(RUNTIME_CLASS(PField)))
				{
					PField *vsym = static_cast<PField*>(sym);

					// We have 4 cases to consider here:
					// 1. The symbol is a static/meta member (not implemented yet) which is always accessible.
					// 2. This is a static function 
					// 3. This is an action function with a restricted self pointer
					// 4. This is a normal member or unrestricted action function.
					if (vsym->Flags & VARF_Deprecated)
					{
						ScriptPosition.Message(MSG_WARNING, "Accessing deprecated member variable %s", vsym->SymbolName.GetChars());
					}
					if ((vsym->Flags & VARF_Private) && symtbl != &ctx.Class->Symbols)
					{
						ScriptPosition.Message(MSG_ERROR, "Private member %s not accessible", vsym->SymbolName.GetChars());
						delete this;
						return nullptr;
					}

					if (vsym->Flags & VARF_Static)
					{
						// todo. For now these cannot be defined so let's just exit.
						ScriptPosition.Message(MSG_ERROR, "Static members not implemented yet.");
						delete this;
						return nullptr;
					}
					auto x = isclass? new FxClassMember(Object, vsym, ScriptPosition) : new FxStructMember(Object, vsym, ScriptPosition);
					delete this;
					return x->Resolve(ctx);
				}
				else
				{
					ScriptPosition.Message(MSG_ERROR, "Invalid member identifier '%s'\n", Identifier.GetChars());
					delete this;
					return nullptr;
				}
			}
			else
			{
				ScriptPosition.Message(MSG_ERROR, "Unknown identifier '%s'", Identifier.GetChars());
				delete this;
				return nullptr;
			}
		}
	}
	else if (Object->ValueType->IsA(RUNTIME_CLASS(PStruct)))
	{
		if ((sym = Object->ValueType->Symbols.FindSymbol(Identifier, false)) != nullptr)
		{
			if (sym->IsKindOf(RUNTIME_CLASS(PSymbolConst)))
			{
				ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as struct constant\n", Identifier.GetChars());
				newex = FxConstant::MakeConstant(sym, ScriptPosition);
			}
			else if (sym->IsKindOf(RUNTIME_CLASS(PField)))
			{
				PField *vsym = static_cast<PField*>(sym);

				if (vsym->Flags & VARF_Deprecated)
				{
					ScriptPosition.Message(MSG_WARNING, "Accessing deprecated member variable %s", vsym->SymbolName.GetChars());
				}
				auto x = new FxStructMember(Object, vsym, ScriptPosition);
				delete this;
				return x->Resolve(ctx);
			}
			else
			{
				ScriptPosition.Message(MSG_ERROR, "Invalid member identifier '%s'\n", Identifier.GetChars());
				delete this;
				return nullptr;
			}
		}
		else
		{
			ScriptPosition.Message(MSG_ERROR, "Unknown identifier '%s'", Identifier.GetChars());
			delete this;
			return nullptr;
		}
	}

	ScriptPosition.Message(MSG_ERROR, "Left side of %s is not a struct or class", Identifier.GetChars());
	delete this;
	return nullptr;
}


//==========================================================================
//
//
//
//==========================================================================

FxLocalVariable::FxLocalVariable(FxLocalVariableDeclaration *var, const FScriptPosition &sc)
	: FxExpression(EFX_LocalVariable, sc)
{
	Variable = var;
	ValueType = var->ValueType;
	AddressRequested = false;
	RegOffset = 0;
}

FxExpression *FxLocalVariable::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	return this;
}

bool FxLocalVariable::RequestAddress(FCompileContext &ctx, bool *writable)
{
	AddressRequested = true;
	if (writable != nullptr) *writable = !ctx.CheckReadOnly(Variable->VarFlags);
	return true;
}
	
ExpEmit FxLocalVariable::Emit(VMFunctionBuilder *build)
{
	ExpEmit ret(Variable->RegNum + RegOffset, Variable->ValueType->GetRegType(), false, true);
	ret.RegCount = ValueType->GetRegCount();
	if (AddressRequested) ret.Target = true;
	return ret;
}



//==========================================================================
//
//
//
//==========================================================================

FxSelf::FxSelf(const FScriptPosition &pos)
: FxExpression(EFX_Self, pos)
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
	if (ctx.Function == nullptr || ctx.Function->Variants[0].SelfClass == nullptr)
	{
		ScriptPosition.Message(MSG_ERROR, "self used outside of a member function");
		delete this;
		return nullptr;
	}
	ValueType = NewPointer(ctx.Function->Variants[0].SelfClass);
	return this;
}  

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxSelf::Emit(VMFunctionBuilder *build)
{
	// self is always the first pointer passed to the function
	return ExpEmit(0, REGT_POINTER, false, true);
}


//==========================================================================
//
//
//
//==========================================================================

FxClassDefaults::FxClassDefaults(FxExpression *X, const FScriptPosition &pos)
	: FxExpression(EFX_ClassDefaults, pos)
{
	obj = X;
	EmitTail = false;
}

FxClassDefaults::~FxClassDefaults()
{
	SAFE_DELETE(obj);
}


//==========================================================================
//
//
//
//==========================================================================

PPrototype *FxClassDefaults::ReturnProto()
{
	EmitTail = true;
	return FxExpression::ReturnProto();
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxClassDefaults::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(obj, ctx);
	assert(obj->ValueType->IsKindOf(RUNTIME_CLASS(PPointer)));
	ValueType = NewPointer(static_cast<PPointer*>(obj->ValueType)->PointedType, true);
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

int BuiltinGetDefault(VMFrameStack *stack, VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret)
{
	assert(numparam == 1);
	PARAM_POINTER_AT(0, obj, DObject);
	ACTION_RETURN_OBJECT(obj->GetClass()->Defaults);
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxClassDefaults::Emit(VMFunctionBuilder *build)
{
	EmitParameter(build, obj, ScriptPosition);
	PSymbol *sym = FindBuiltinFunction(NAME_BuiltinGetDefault, BuiltinGetDefault);

	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	auto callfunc = ((PSymbolVMFunction *)sym)->Function;
	int opcode = (EmitTail ? OP_TAIL_K : OP_CALL_K);
	build->Emit(opcode, build->GetConstantAddress(callfunc, ATAG_OBJECT), 1, 1);

	if (EmitTail)
	{
		ExpEmit call;
		call.Final = true;
		return call;
	}

	ExpEmit out(build, REGT_POINTER);
	build->Emit(OP_RESULT, 0, REGT_POINTER, out.RegNum);
	return out;

}

//==========================================================================
//
//
//
//==========================================================================

FxGlobalVariable::FxGlobalVariable(PField* mem, const FScriptPosition &pos)
	: FxExpression(EFX_GlobalVariable, pos)
{
	membervar = mem;
	AddressRequested = false;
	AddressWritable = true;	// must be true unless classx tells us otherwise if requested.
}

//==========================================================================
//
//
//
//==========================================================================

bool FxGlobalVariable::RequestAddress(FCompileContext &ctx, bool *writable)
{
	AddressRequested = true;
	if (writable != nullptr) *writable = AddressWritable && !ctx.CheckReadOnly(membervar->Flags);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxGlobalVariable::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	ValueType = membervar->Type;
	return this;
}

ExpEmit FxGlobalVariable::Emit(VMFunctionBuilder *build)
{
	ExpEmit obj(build, REGT_POINTER);

	build->Emit(OP_LKP, obj.RegNum, build->GetConstantAddress((void*)(intptr_t)membervar->Offset, ATAG_GENERIC));
	if (AddressRequested)
	{
		return obj;
	}

	ExpEmit loc(build, membervar->Type->GetRegType(), membervar->Type->GetRegCount());

	if (membervar->BitValue == -1)
	{
		int offsetreg = build->GetConstantInt(0);
		build->Emit(membervar->Type->GetLoadOp(), loc.RegNum, obj.RegNum, offsetreg);
	}
	else
	{
		build->Emit(OP_LBIT, loc.RegNum, obj.RegNum, 1 << membervar->BitValue);
	}
	obj.Free(build);
	return loc;
}


//==========================================================================
//
//
//
//==========================================================================

FxStructMember::FxStructMember(FxExpression *x, PField* mem, const FScriptPosition &pos)
	: FxExpression(EFX_StructMember, pos)
{
	classx = x;
	membervar = mem;
	AddressRequested = false;
	AddressWritable = true;	// must be true unless classx tells us otherwise if requested.
}

//==========================================================================
//
//
//
//==========================================================================

FxStructMember::~FxStructMember()
{
	SAFE_DELETE(classx);
}

//==========================================================================
//
//
//
//==========================================================================

bool FxStructMember::RequestAddress(FCompileContext &ctx, bool *writable)
{
	AddressRequested = true;
	if (writable != nullptr) *writable = (AddressWritable && !ctx.CheckReadOnly(membervar->Flags) &&
											(!classx->ValueType->IsKindOf(RUNTIME_CLASS(PPointer)) || !static_cast<PPointer*>(classx->ValueType)->IsConst));
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxStructMember::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(classx, ctx);

	if (membervar->SymbolName == NAME_Default)
	{
		if (!classx->ValueType->IsKindOf(RUNTIME_CLASS(PPointer))
			|| !static_cast<PPointer *>(classx->ValueType)->PointedType->IsKindOf(RUNTIME_CLASS(AActor)))
		{
			ScriptPosition.Message(MSG_ERROR, "'Default' requires an actor type.");
			delete this;
			return nullptr;
		}
		FxExpression * x = new FxClassDefaults(classx, ScriptPosition);
		classx = nullptr;
		delete this;
		return x->Resolve(ctx);
	}

	if (classx->ValueType->IsKindOf(RUNTIME_CLASS(PPointer)))
	{
		PPointer *ptrtype = dyn_cast<PPointer>(classx->ValueType);
		if (ptrtype == nullptr || !ptrtype->PointedType->IsKindOf(RUNTIME_CLASS(PStruct)))
		{
			ScriptPosition.Message(MSG_ERROR, "Member variable requires a struct or class object.");
			delete this;
			return nullptr;
		}
	}
	else if (classx->ValueType->IsA(RUNTIME_CLASS(PStruct)))	// Classes can never be used as value types so we do not have to consider that case.
	{
		// if this is a struct within a class or another struct we can simplify the expression by creating a new PField with a cumulative offset.
		if (classx->ExprType == EFX_ClassMember || classx->ExprType == EFX_StructMember)
		{
			auto parentfield = static_cast<FxStructMember *>(classx)->membervar;
			// PFields are garbage collected so this will be automatically taken care of later.
			auto newfield = new PField(membervar->SymbolName, membervar->Type, membervar->Flags | parentfield->Flags, membervar->Offset + parentfield->Offset, membervar->BitValue);
			static_cast<FxStructMember *>(classx)->membervar = newfield;
			classx->isresolved = false;	// re-resolve the parent so it can also check if it can be optimized away.
			auto x = classx->Resolve(ctx);
			classx = nullptr;
			return x;
		}
		else if (classx->ExprType == EFX_GlobalVariable)
		{
			auto parentfield = static_cast<FxGlobalVariable *>(classx)->membervar;
			auto newfield = new PField(membervar->SymbolName, membervar->Type, membervar->Flags | parentfield->Flags, membervar->Offset + parentfield->Offset, membervar->BitValue);
			static_cast<FxGlobalVariable *>(classx)->membervar = newfield;
			classx->isresolved = false;	// re-resolve the parent so it can also check if it can be optimized away.
			auto x = classx->Resolve(ctx);
			classx = nullptr;
			return x;
		}
		else if (classx->ExprType == EFX_LocalVariable && classx->IsVector())	// vectors are a special case because they are held in registers
		{
			// since this is a vector, all potential things that may get here are single float or an xy-vector.
			auto locvar = static_cast<FxLocalVariable *>(classx);
			locvar->RegOffset = int(membervar->Offset / 8);
			locvar->ValueType = membervar->Type;
			classx = nullptr;
			delete this;
			return locvar;
		}
		else
		{
			if (!(classx->RequestAddress(ctx, &AddressWritable)))
			{
				ScriptPosition.Message(MSG_ERROR, "unable to dereference left side of %s", membervar->SymbolName.GetChars());
				delete this;
				return nullptr;
			}
		}
	}
	ValueType = membervar->Type;
	return this;
}

ExpEmit FxStructMember::Emit(VMFunctionBuilder *build)
{
	ExpEmit obj = classx->Emit(build);
	assert(obj.RegType == REGT_POINTER);

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

	if (AddressRequested)
	{
		if (membervar->Offset == 0)
		{
			return obj;
		}
		obj.Free(build);
		ExpEmit out(build, REGT_POINTER);
		build->Emit(OP_ADDA_RK, out.RegNum, obj.RegNum, build->GetConstantInt((int)membervar->Offset));
		return out;
	}

	int offsetreg = build->GetConstantInt((int)membervar->Offset);
	ExpEmit loc(build, membervar->Type->GetRegType(), membervar->Type->GetRegCount());

	if (membervar->BitValue == -1)
	{
		build->Emit(membervar->Type->GetLoadOp(), loc.RegNum, obj.RegNum, offsetreg);
	}
	else
	{
		ExpEmit out(build, REGT_POINTER);
		build->Emit(OP_ADDA_RK, out.RegNum, obj.RegNum, offsetreg);
		build->Emit(OP_LBIT, loc.RegNum, out.RegNum, 1 << membervar->BitValue);
		out.Free(build);
	}
	obj.Free(build);
	return loc;
}


//==========================================================================
//
// not really needed at the moment but may become useful with meta properties
// and some other class-specific extensions.
//
//==========================================================================

FxClassMember::FxClassMember(FxExpression *x, PField* mem, const FScriptPosition &pos)
: FxStructMember(x, mem, pos)
{
	ExprType = EFX_ClassMember;
}

//==========================================================================
//
//
//
//==========================================================================

FxArrayElement::FxArrayElement(FxExpression *base, FxExpression *_index)
:FxExpression(EFX_ArrayElement, base->ScriptPosition)
{
	Array=base;
	index = _index;
	AddressRequested = false;
	AddressWritable = false;
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

bool FxArrayElement::RequestAddress(FCompileContext &ctx, bool *writable)
{
	AddressRequested = true;
	if (writable != nullptr) *writable = AddressWritable;
	return true;
}

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

	if (index->ValueType->GetRegType() == REGT_FLOAT /* lax */)
	{
		// DECORATE allows floats here so cast them to int.
		index = new FxIntCast(index, ctx.FromDecorate);
		index = index->Resolve(ctx);
		if (index == nullptr) 
		{
			delete this;
			return nullptr;
		}
	}
	if (index->ValueType->GetRegType() != REGT_INT && index->ValueType != TypeName)
	{
		ScriptPosition.Message(MSG_ERROR, "Array index must be integer");
		delete this;
		return nullptr;
	}

	PArray *arraytype = dyn_cast<PArray>(Array->ValueType);
	if (arraytype == nullptr)
	{
		ScriptPosition.Message(MSG_ERROR, "'[]' can only be used with arrays.");
		delete this;
		return nullptr;
	}
	if (index->isConstant())
	{
		unsigned indexval = static_cast<FxConstant *>(index)->GetValue().GetInt();
		if (indexval >= arraytype->ElementCount)
		{
			ScriptPosition.Message(MSG_ERROR, "Array index out of bounds");
			delete this;
			return nullptr;
		}
	}

	ValueType = arraytype->ElementType;
	if (ValueType->GetRegType() != REGT_INT && ValueType->GetRegType() != REGT_FLOAT)
	{
		// int arrays only for now
		ScriptPosition.Message(MSG_ERROR, "Only numeric arrays are supported.");
		delete this;
		return nullptr;
	}
	if (!Array->RequestAddress(ctx, &AddressWritable))
	{
		ScriptPosition.Message(MSG_ERROR, "Unable to dereference array.");
		delete this;
		return nullptr;
	}
	return this;
}

//==========================================================================
//
// in its current state this won't be able to do more than handle the args array.
//
//==========================================================================

ExpEmit FxArrayElement::Emit(VMFunctionBuilder *build)
{
	ExpEmit start = Array->Emit(build);
	PArray *const arraytype = static_cast<PArray*>(Array->ValueType);
	ExpEmit dest(build, arraytype->ElementType->GetRegType());

	if (start.Konst)
	{
		ExpEmit tmpstart(build, REGT_POINTER);
		build->Emit(OP_LKP, tmpstart.RegNum, start.RegNum);
		start.Free(build);
		start = tmpstart;
	}
	if (index->isConstant())
	{
		unsigned indexval = static_cast<FxConstant *>(index)->GetValue().GetInt();
		assert(indexval < arraytype->ElementCount && "Array index out of bounds");
		indexval *= arraytype->ElementSize;

		if (AddressRequested)
		{
			if (indexval != 0)
			{
				build->Emit(OP_ADDA_RK, start.RegNum, start.RegNum, build->GetConstantInt(indexval));
			}
		}
		else
		{
			build->Emit(arraytype->ElementType->GetLoadOp(), dest.RegNum,
				start.RegNum, build->GetConstantInt(indexval));
		}
	}
	else
	{
		ExpEmit indexv(index->Emit(build));
		int shiftbits = 0;
		while (1u << shiftbits < arraytype->ElementSize)
		{ 
			shiftbits++;
		}
		assert(1u << shiftbits == arraytype->ElementSize && "Element sizes other than power of 2 are not implemented");
		build->Emit(OP_BOUND, indexv.RegNum, arraytype->ElementCount);
		if (shiftbits > 0)
		{
			build->Emit(OP_SLL_RI, indexv.RegNum, indexv.RegNum, shiftbits);
		}

		if (AddressRequested)
		{
			build->Emit(OP_ADDA_RR, start.RegNum, start.RegNum, indexv.RegNum);
		}
		else
		{
			build->Emit(arraytype->ElementType->GetLoadOp() + 1,	// added 1 to use the *_R version that
				dest.RegNum, start.RegNum, indexv.RegNum);			// takes the offset from a register
		}
		indexv.Free(build);
	}
	if (AddressRequested)
	{
		dest.Free(build);
		return start;
	}
	
	start.Free(build);
	return dest;
}

//==========================================================================
//
//
//
//==========================================================================

FxFunctionCall::FxFunctionCall(FName methodname, FName rngname, FArgumentList *args, const FScriptPosition &pos)
: FxExpression(EFX_FunctionCall, pos)
{
	MethodName = methodname;
	RNG = &pr_exrandom;
	ArgList = args;
	if (rngname != NAME_None)
	{
		switch (MethodName)
		{
		case NAME_Random:
		case NAME_FRandom:
		case NAME_RandomPick:
		case NAME_FRandomPick:
		case NAME_Random2:
			RNG = FRandom::StaticFindRNG(rngname.GetChars());
			break;

		default:
			pos.Message(MSG_ERROR, "Cannot use named RNGs with %s", MethodName.GetChars());
			break;

		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

FxFunctionCall::~FxFunctionCall()
{
	SAFE_DELETE(ArgList);
}

//==========================================================================
//
// Check function that gets called
//
//==========================================================================

static bool CheckArgSize(FName fname, FArgumentList *args, int min, int max, FScriptPosition &sc)
{
	int s = args ? args->Size() : 0;
	if (s < min)
	{
		sc.Message(MSG_ERROR, "Insufficient arguments in call to %s, expected %d, got %d", fname.GetChars(), min, s);
		return false;
	}
	else if (s > max && max >= 0)
	{
		sc.Message(MSG_ERROR, "Too many arguments in call to %s, expected %d, got %d", fname.GetChars(), min, s);
		return false;
	}
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxFunctionCall::Resolve(FCompileContext& ctx)
{
	ABORT(ctx.Class);
	bool error = false;

	PFunction *afd = FindClassMemberFunction(ctx.Class, ctx.Class, MethodName, ScriptPosition, &error);

	if (error)
	{
		delete this;
		return nullptr;
	}

	if (afd != nullptr)
	{
		if (ctx.Function->Variants[0].Flags & VARF_Static && !(afd->Variants[0].Flags & VARF_Static))
		{
			ScriptPosition.Message(MSG_ERROR, "Call to non-static function %s from a static context", MethodName.GetChars());
			delete this;
			return nullptr;
		}
		auto self = !(afd->Variants[0].Flags & VARF_Static)? new FxSelf(ScriptPosition) : nullptr;
		auto x = new FxVMFunctionCall(self, afd, ArgList, ScriptPosition, false);
		ArgList = nullptr;
		delete this;
		return x->Resolve(ctx);
	}

	for (size_t i = 0; i < countof(FxFlops); ++i)
	{
		if (MethodName == FxFlops[i].Name)
		{
			FxExpression *x = new FxFlopFunctionCall(i, ArgList, ScriptPosition);
			ArgList = nullptr;
			delete this;
			return x->Resolve(ctx);
		}
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
			return nullptr;
		}
		else if (paramcount > max)
		{
			ScriptPosition.Message(MSG_ERROR, "too many parameters for '%s' (expected %d, got %d)", 
				MethodName.GetChars(), max, paramcount);
			delete this;
			return nullptr;
		}
		FxExpression *self = (ctx.Function && ctx.Function->Variants[0].Flags & VARF_Method) ? new FxSelf(ScriptPosition) : nullptr;
		FxExpression *x = new FxActionSpecialCall(self, special, ArgList, ScriptPosition);
		ArgList = nullptr;
		delete this;
		return x->Resolve(ctx);
	}

	PClass *cls = PClass::FindClass(MethodName);
	if (cls != nullptr && cls->bExported)
	{
		if (CheckArgSize(MethodName, ArgList, 1, 1, ScriptPosition))
		{
			FxExpression *x = new FxDynamicCast(cls, (*ArgList)[0]);
			(*ArgList)[0] = nullptr;
			delete this;
			return x->Resolve(ctx);
		}
		else
		{
			delete this;
			return nullptr;
		}
	}


	// Last but not least: Check builtins and type casts. The random functions can take a named RNG if specified.
	// Note that for all builtins the used arguments have to be nulled in the ArgList so that they won't get deleted before they get used.
	FxExpression *func = nullptr;

	switch (MethodName)
	{
	case NAME_Bool:
	case NAME_Int:
	case NAME_uInt:
	case NAME_Float:
	case NAME_Double:
	case NAME_Name:
	case NAME_Color:
	case NAME_Sound:
	case NAME_State:
		if (CheckArgSize(MethodName, ArgList, 1, 1, ScriptPosition))
		{
			PType *type = 
				MethodName == NAME_Bool ? TypeBool :
				MethodName == NAME_Int ? TypeSInt32 :
				MethodName == NAME_uInt ? TypeUInt32 :
				MethodName == NAME_Float ? TypeFloat64 :
				MethodName == NAME_Double ? TypeFloat64 :
				MethodName == NAME_Name ? TypeName :
				MethodName == NAME_Color ? TypeColor :
				MethodName == NAME_State? TypeState :(PType*)TypeSound;

			func = new FxTypeCast((*ArgList)[0], type, true, true);
			(*ArgList)[0] = nullptr;
		}
		break;

	case NAME_Random:
		// allow calling Random without arguments to default to (0, 255)
		if (ArgList->Size() == 0)
		{
			func = new FxRandom(RNG, new FxConstant(0, ScriptPosition), new FxConstant(255, ScriptPosition), ScriptPosition, ctx.FromDecorate);
		}
		else if (CheckArgSize(NAME_Random, ArgList, 2, 2, ScriptPosition))
		{
			func = new FxRandom(RNG, (*ArgList)[0], (*ArgList)[1], ScriptPosition, ctx.FromDecorate);
			(*ArgList)[0] = (*ArgList)[1] = nullptr;
		}
		break;

	case NAME_FRandom:
		if (CheckArgSize(NAME_FRandom, ArgList, 2, 2, ScriptPosition))
		{
			func = new FxFRandom(RNG, (*ArgList)[0], (*ArgList)[1], ScriptPosition);
			(*ArgList)[0] = (*ArgList)[1] = nullptr;
		}
		break;

	case NAME_RandomPick:
	case NAME_FRandomPick:
		if (CheckArgSize(MethodName, ArgList, 1, -1, ScriptPosition))
		{
			func = new FxRandomPick(RNG, *ArgList, MethodName == NAME_FRandomPick, ScriptPosition, ctx.FromDecorate);
			for (auto &i : *ArgList) i = nullptr;
		}
		break;

	case NAME_Random2:
		if (CheckArgSize(NAME_Random2, ArgList, 0, 1, ScriptPosition))
		{
			func = new FxRandom2(RNG, ArgList->Size() == 0? nullptr : (*ArgList)[0], ScriptPosition, ctx.FromDecorate);
			if (ArgList->Size() > 0) (*ArgList)[0] = nullptr;
		}
		break;

	case NAME_Min:
	case NAME_Max:
		if (CheckArgSize(MethodName, ArgList, 2, -1, ScriptPosition))
		{
			func = new FxMinMax(*ArgList, MethodName, ScriptPosition);
			for (auto &i : *ArgList) i = nullptr;
		}
		break;

	case NAME_Clamp:
		if (CheckArgSize(MethodName, ArgList, 3, 3, ScriptPosition))
		{
			TArray<FxExpression *> pass;
			pass.Resize(2);
			pass[0] = (*ArgList)[0];
			pass[1] = (*ArgList)[1];
			pass[0] = new FxMinMax(pass, NAME_Max, ScriptPosition);
			pass[1] = (*ArgList)[2];
			func = new FxMinMax(pass, NAME_Min, ScriptPosition);
			(*ArgList)[0] = (*ArgList)[1] = (*ArgList)[2] = nullptr;
		}
		break;

	case NAME_Abs:
		if (CheckArgSize(MethodName, ArgList, 1, 1, ScriptPosition))
		{
			func = new FxAbs((*ArgList)[0]);
			(*ArgList)[0] = nullptr;
		}
		break;

	case NAME_ATan2:
	case NAME_VectorAngle:
		if (CheckArgSize(MethodName, ArgList, 2, 2, ScriptPosition))
		{
			func = MethodName == NAME_ATan2 ? new FxATan2((*ArgList)[0], (*ArgList)[1], ScriptPosition) : new FxATan2((*ArgList)[1], (*ArgList)[0], ScriptPosition);
			(*ArgList)[0] = (*ArgList)[1] = nullptr;
		}
		break;

	default:
		ScriptPosition.Message(MSG_ERROR, "Call to unknown function '%s'", MethodName.GetChars());
		break;
	}
	if (func != nullptr)
	{
		delete this;
		return func->Resolve(ctx);
	}
	delete this;
	return nullptr;
}


//==========================================================================
//
//
//
//==========================================================================

FxMemberFunctionCall::FxMemberFunctionCall(FxExpression *self, FName methodname, FArgumentList *args, const FScriptPosition &pos)
	: FxExpression(EFX_MemberFunctionCall, pos)
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

FxMemberFunctionCall::~FxMemberFunctionCall()
{
	SAFE_DELETE(Self);
	SAFE_DELETE(ArgList);
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxMemberFunctionCall::Resolve(FCompileContext& ctx)
{
	ABORT(ctx.Class);
	PClass *cls;
	bool staticonly = false;

	if (Self->ExprType == EFX_Identifier)
	{
		// If the left side is a class name for a static member function call it needs to be resolved manually 
		// because the resulting value type would cause problems in nearly every other place where identifiers are being used.
		cls = PClass::FindClass(static_cast<FxIdentifier *>(Self)->Identifier);
		if (cls != nullptr && cls->bExported)
		{
			staticonly = true;
			goto isresolved;
		}
	}
	SAFE_RESOLVE(Self, ctx);

	if (Self->IsVector())
	{
		// handle builtins: Vectors got 2: Length and Unit.
		if (MethodName == NAME_Length || MethodName == NAME_Unit)
		{
			auto x = new FxVectorBuiltin(Self, MethodName);
			Self = nullptr;
			delete this;
			return x->Resolve(ctx);
		}
	}

	if (Self->ValueType->IsKindOf(RUNTIME_CLASS(PPointer)))
	{
		auto ptype = static_cast<PPointer *>(Self->ValueType)->PointedType;
		if (ptype->IsKindOf(RUNTIME_CLASS(PClass)))
		{
			cls = static_cast<PClass *>(ptype);
		}
		else
		{
			ScriptPosition.Message(MSG_ERROR, "Left hand side of %s must point to a class object\n", MethodName.GetChars());
			delete this;
			return nullptr;
		}
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "Invalid expression on left hand side of %s\n", MethodName.GetChars());
		delete this;
		return nullptr;
	}

isresolved:
	bool error = false;
	PFunction *afd = FindClassMemberFunction(cls, cls, MethodName, ScriptPosition, &error);
	if (error)
	{
		delete this;
		return nullptr;
	}

	if (afd == nullptr)
	{
		ScriptPosition.Message(MSG_ERROR, "Unknown function %s\n", MethodName.GetChars());
		delete this;
		return nullptr;
	}
	if (staticonly && (afd->Variants[0].Flags & VARF_Method))
	{
		if (!ctx.Class->IsDescendantOf(cls))
		{
			ScriptPosition.Message(MSG_ERROR, "Cannot call non-static function %s::%s from here\n", cls->TypeName.GetChars(), MethodName.GetChars());
			delete this;
			return nullptr;
		}
		else
		{
			// Todo: If this is a qualified call to a parent class function, let it through (but this needs to disable virtual calls later.)
			ScriptPosition.Message(MSG_ERROR, "Qualified member call to parent class not yet implemented\n", cls->TypeName.GetChars(), MethodName.GetChars());
			delete this;
			return nullptr;
		}
	}

	// do not pass the self pointer to static functions.
	auto self = (afd->Variants[0].Flags & VARF_Method) ? Self : nullptr;
	auto x = new FxVMFunctionCall(self, afd, ArgList, ScriptPosition, staticonly);
	ArgList = nullptr;
	if (Self == self) Self = nullptr;
	delete this;
	return x->Resolve(ctx);
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
: FxExpression(EFX_ActionSpecialCall, pos)
{
	Self = self;
	Special = special;
	ArgList = args;
	EmitTail = false;
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

PPrototype *FxActionSpecialCall::ReturnProto()
{
	EmitTail = true;
	return FxExpression::ReturnProto();
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

	SAFE_RESOLVE_OPT(Self, ctx);
	if (ArgList != nullptr)
	{
		for (unsigned i = 0; i < ArgList->Size(); i++)
		{
			(*ArgList)[i] = (*ArgList)[i]->Resolve(ctx);
			if ((*ArgList)[i] == nullptr)
			{
				failed = true;
			}
			else if (Special < 0 && i == 0)
			{
				if ((*ArgList)[i]->ValueType == TypeString)
				{
					(*ArgList)[i] = new FxNameCast((*ArgList)[i]);
					(*ArgList)[i] = (*ArgList)[i]->Resolve(ctx);
					if ((*ArgList)[i] == nullptr)
					{
						failed = true;
					}
				}
				else if ((*ArgList)[i]->ValueType != TypeName)
				{
					ScriptPosition.Message(MSG_ERROR, "Name expected for parameter %d", i);
					failed = true;
				}
			}
			else if (!(*ArgList)[i]->IsInteger())
			{
				if ((*ArgList)[i]->ValueType->GetRegType() == REGT_FLOAT /* lax */)
				{
					(*ArgList)[i] = new FxIntCast((*ArgList)[i], ctx.FromDecorate);
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
			return nullptr;
		}
	}
	ValueType = TypeSInt32;
	return this;
}


//==========================================================================
//
// 
//
//==========================================================================

int BuiltinCallLineSpecial(VMFrameStack *stack, VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret)
{
	assert(numparam > 2 && numparam < 8);
	assert(param[0].Type == REGT_INT);
	assert(param[1].Type == REGT_POINTER);
	int v[5] = { 0 };

	for (int i = 2; i < numparam; ++i)
	{
		v[i - 2] = param[i].i;
	}
	ACTION_RETURN_INT(P_ExecuteSpecial(param[0].i, nullptr, reinterpret_cast<AActor*>(param[1].a), false, v[0], v[1], v[2], v[3], v[4]));
}

ExpEmit FxActionSpecialCall::Emit(VMFunctionBuilder *build)
{
	unsigned i = 0;

	build->Emit(OP_PARAMI, abs(Special));			// pass special number
	// fixme: This really should use the Self pointer that got passed to this class instead of just using the first argument from the function. 
	// Once static functions are possible, or specials can be called through a member access operator this won't work anymore.
	build->Emit(OP_PARAM, 0, REGT_POINTER, 0);		// pass self 
	if (ArgList != nullptr)
	{
		for (; i < ArgList->Size(); ++i)
		{
			FxExpression *argex = (*ArgList)[i];
			if (Special < 0 && i == 0)
			{
				assert(argex->ValueType == TypeName);
				assert(argex->isConstant());
				build->EmitParamInt(-static_cast<FxConstant *>(argex)->GetValue().GetName());
			}
			else
			{
				assert(argex->ValueType->GetRegType() == REGT_INT);
				if (argex->isConstant())
				{
					build->EmitParamInt(static_cast<FxConstant *>(argex)->GetValue().GetInt());
				}
				else
				{
					ExpEmit arg(argex->Emit(build));
					build->Emit(OP_PARAM, 0, arg.RegType, arg.RegNum);
					arg.Free(build);
				}
			}
		}
	}
	// Call the BuiltinCallLineSpecial function to perform the desired special.
	VMFunction *callfunc;
	PSymbol *sym = FindBuiltinFunction(NAME_BuiltinCallLineSpecial, BuiltinCallLineSpecial);

	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	if (EmitTail)
	{
		build->Emit(OP_TAIL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 2 + i, 0);
		ExpEmit call;
		call.Final = true;
		return call;
	}

	ExpEmit dest(build, REGT_INT);
	build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 2 + i, 1);
	build->Emit(OP_RESULT, 0, REGT_INT, dest.RegNum);
	return dest;
}

//==========================================================================
//
// FxVMFunctionCall
//
//==========================================================================

FxVMFunctionCall::FxVMFunctionCall(FxExpression *self, PFunction *func, FArgumentList *args, const FScriptPosition &pos, bool novirtual)
: FxExpression(EFX_VMFunctionCall, pos)
{
	Self = self;
	Function = func;
	ArgList = args;
	EmitTail = false;
	NoVirtual = novirtual;
}

//==========================================================================
//
//
//
//==========================================================================

FxVMFunctionCall::~FxVMFunctionCall()
{
	SAFE_DELETE(ArgList);
}

//==========================================================================
//
//
//
//==========================================================================

PPrototype *FxVMFunctionCall::ReturnProto()
{
	EmitTail = true;
	return Function->Variants[0].Proto;
}

//==========================================================================
//
//
//
//==========================================================================

VMFunction *FxVMFunctionCall::GetDirectFunction()
{
	// If this return statement calls a non-virtual function with no arguments,
	// then it can be a "direct" function. That is, the DECORATE
	// definition can call that function directly without wrapping
	// it inside VM code.
	if ((ArgList ? ArgList->Size() : 0) == 0 && !(Function->Variants[0].Flags & VARF_Virtual))
	{
		return Function->Variants[0].Implementation;
	}
	
	return nullptr;
}

//==========================================================================
//
// FxVMFunctionCall :: Resolve
//
//==========================================================================

FxExpression *FxVMFunctionCall::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE_OPT(Self, ctx);
	bool failed = false;
	auto proto = Function->Variants[0].Proto;
	auto argtypes = proto->ArgumentTypes;

	int implicit = Function->GetImplicitArgs();

	// This should never happen.
	if (Self == nullptr && (Function->Variants[0].Flags & VARF_Method))
	{
		ScriptPosition.Message(MSG_ERROR, "Call to non-static function without a self pointer");
		delete this;
		return nullptr;
	}

	if (ArgList != nullptr)
	{
		bool foundvarargs = false;
		PType * type = nullptr;
		if (argtypes.Last() != nullptr && ArgList->Size() + implicit > argtypes.Size())
		{
			ScriptPosition.Message(MSG_ERROR, "Too many arguments in call to %s", Function->SymbolName.GetChars());
			delete this;
			return nullptr;
		}

		for (unsigned i = 0; i < ArgList->Size(); i++)
		{
			// Varargs must all have the same type as the last typed argument. A_Jump is the only function using it.
			if (!foundvarargs)
			{
				if (argtypes[i + implicit] == nullptr) foundvarargs = true;
				else type = argtypes[i + implicit];
			}
			assert(type != nullptr);

			FxExpression *x = new FxTypeCast((*ArgList)[i], type, false);
			x = x->Resolve(ctx);
			failed |= (x == nullptr);
			(*ArgList)[i] = x;
		}
		int numargs = ArgList->Size() + implicit;
		if ((unsigned)numargs < argtypes.Size() && argtypes[numargs] != nullptr)
		{
			auto flags = Function->Variants[0].ArgFlags[numargs];
			if (!(flags & VARF_Optional))
			{
				ScriptPosition.Message(MSG_ERROR, "Insufficient arguments in call to %s", Function->SymbolName.GetChars());
				delete this;
				return nullptr;
			}
		}
		
	}
	if (failed)
	{
		delete this;
		return nullptr;
	}
	TArray<PType *> &rets = proto->ReturnTypes;
	if (rets.Size() > 0)
	{
		ValueType = rets[0];
	}
	else
	{
		ValueType = TypeVoid;
	}

	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxVMFunctionCall::Emit(VMFunctionBuilder *build)
{
	assert(build->Registers[REGT_POINTER].GetMostUsed() >= build->NumImplicits);
	int count = 0; (ArgList ? ArgList->Size() : 0);

	if (count == 1)
	{
		ExpEmit reg;
		if (CheckEmitCast(build, EmitTail, reg))
		{
			return reg;
		}
	}

	count = 0;
	// Emit code to pass implied parameters
	if (Function->Variants[0].Flags & VARF_Method)
	{
		assert(Self != nullptr);
		ExpEmit selfemit = Self->Emit(build);
		assert(selfemit.RegType == REGT_POINTER);
		build->Emit(OP_PARAM, 0, selfemit.RegType, selfemit.RegNum);
		count += 1;
		if (Function->Variants[0].Flags & VARF_Action)
		{
			static_assert(NAP == 3, "This code needs to be updated if NAP changes");
			if (build->NumImplicits == NAP && selfemit.RegNum == 0)	// only pass this function's stateowner and stateinfo if the subfunction is run in self's context.
			{
				build->Emit(OP_PARAM, 0, REGT_POINTER, 1);
				build->Emit(OP_PARAM, 0, REGT_POINTER, 2);
			}
			else
			{
				// pass self as stateowner, otherwise all attempts of the subfunction to retrieve a state from a name would fail.
				build->Emit(OP_PARAM, 0, selfemit.RegType, selfemit.RegNum);
				build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(nullptr, ATAG_GENERIC));
			}
			count += 2;
		}
		selfemit.Free(build);
	}
	// Emit code to pass explicit parameters
	if (ArgList != nullptr)
	{
		for (unsigned i = 0; i < ArgList->Size(); ++i)
		{
			count += EmitParameter(build, (*ArgList)[i], ScriptPosition);
		}
	}
	// Get a constant register for this function
	VMFunction *vmfunc = Function->Variants[0].Implementation;
	int funcaddr = build->GetConstantAddress(vmfunc, ATAG_OBJECT);
	// Emit the call
	if (EmitTail)
	{ // Tail call
		build->Emit(OP_TAIL_K, funcaddr, count, 0);
		ExpEmit call;
		call.Final = true;
		return call;
	}
	else if (vmfunc->Proto->ReturnTypes.Size() > 0)
	{ // Call, expecting one result
		ExpEmit reg(build, vmfunc->Proto->ReturnTypes[0]->GetRegType(), vmfunc->Proto->ReturnTypes[0]->GetRegCount());
		build->Emit(OP_CALL_K, funcaddr, count, 1);
		build->Emit(OP_RESULT, 0, EncodeRegType(reg), reg.RegNum);
		return reg;
	}
	else
	{ // Call, expecting no results
		build->Emit(OP_CALL_K, funcaddr, count, 0);
		return ExpEmit();
	}
}

//==========================================================================
//
// If calling one of the casting kludge functions, don't bother calling the
// function; just use the parameter directly. Returns true if this was a
// kludge function, false otherwise.
//
//==========================================================================

bool FxVMFunctionCall::CheckEmitCast(VMFunctionBuilder *build, bool returnit, ExpEmit &reg)
{
	FName funcname = Function->SymbolName;
	if (funcname == NAME___decorate_internal_int__ ||
		funcname == NAME___decorate_internal_bool__ ||
		funcname == NAME___decorate_internal_state__ ||
		funcname == NAME___decorate_internal_float__)
	{
		FxExpression *arg = (*ArgList)[0];
		if (returnit)
		{
			if (arg->isConstant() &&
				(funcname == NAME___decorate_internal_int__ ||
				 funcname == NAME___decorate_internal_bool__))
			{ // Use immediate version for integers in range
				build->EmitRetInt(0, true, static_cast<FxConstant *>(arg)->GetValue().Int);
			}
			else
			{
				ExpEmit where = arg->Emit(build);
				build->Emit(OP_RET, RET_FINAL, EncodeRegType(where), where.RegNum);
				where.Free(build);
			}
			reg = ExpEmit();
			reg.Final = true;
		}
		else
		{
			reg = arg->Emit(build);
		}
		return true;
	}
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

FxFlopFunctionCall::FxFlopFunctionCall(size_t index, FArgumentList *args, const FScriptPosition &pos)
: FxExpression(EFX_FlopFunctionCall, pos)
{
	assert(index < countof(FxFlops) && "FLOP index out of range");
	Index = (int)index;
	ArgList = args;
}

//==========================================================================
//
//
//
//==========================================================================

FxFlopFunctionCall::~FxFlopFunctionCall()
{
	SAFE_DELETE(ArgList);
}

FxExpression *FxFlopFunctionCall::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();

	if (ArgList == nullptr || ArgList->Size() != 1)
	{
		ScriptPosition.Message(MSG_ERROR, "%s only has one parameter", FName(FxFlops[Index].Name).GetChars());
		delete this;
		return nullptr;
	}

	(*ArgList)[0] = (*ArgList)[0]->Resolve(ctx);
	if ((*ArgList)[0] == nullptr)
	{
		delete this;
		return nullptr;
	}

	if (!(*ArgList)[0]->IsNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "numeric value expected for parameter");
		delete this;
		return nullptr;
	}
	if ((*ArgList)[0]->isConstant())
	{
		double v = static_cast<FxConstant *>((*ArgList)[0])->GetValue().GetFloat();
		v = FxFlops[Index].Evaluate(v);
		FxExpression *x = new FxConstant(v, ScriptPosition);
		delete this;
		return x;
	}
	if ((*ArgList)[0]->ValueType->GetRegType() == REGT_INT)
	{
		(*ArgList)[0] = new FxFloatCast((*ArgList)[0]);
	}
	ValueType = TypeFloat64;
	return this;
}

//==========================================================================
//
//
//==========================================================================

ExpEmit FxFlopFunctionCall::Emit(VMFunctionBuilder *build)
{
	ExpEmit v = (*ArgList)[0]->Emit(build);
	assert(!v.Konst && v.RegType == REGT_FLOAT);

	build->Emit(OP_FLOP, v.RegNum, v.RegNum, FxFlops[Index].Flop);
	return v;
}


//==========================================================================
//
//
//==========================================================================

FxVectorBuiltin::FxVectorBuiltin(FxExpression *self, FName name)
	:FxExpression(EFX_VectorBuiltin, self->ScriptPosition)
{
	Self = self;
	Function = name;
}

FxVectorBuiltin::~FxVectorBuiltin()
{
	SAFE_DELETE(Self);
}

FxExpression *FxVectorBuiltin::Resolve(FCompileContext &ctx)
{
	SAFE_RESOLVE(Self, ctx);
	assert(Self->IsVector());	// should never be created for anything else.
	ValueType = Function == NAME_Length ? TypeFloat64 : Self->ValueType;
	return this;
}

ExpEmit FxVectorBuiltin::Emit(VMFunctionBuilder *build)
{
	ExpEmit to(build, ValueType->GetRegType(), ValueType->GetRegCount());
	ExpEmit op = Self->Emit(build);
	if (Function == NAME_Length)
	{
		build->Emit(Self->ValueType == TypeVector2 ? OP_LENV2 : OP_LENV3, to.RegNum, op.RegNum);
	}
	else
	{
		ExpEmit len(build, REGT_FLOAT);
		build->Emit(Self->ValueType == TypeVector2 ? OP_LENV2 : OP_LENV3, len.RegNum, op.RegNum);
		build->Emit(Self->ValueType == TypeVector2 ? OP_DIVVF2_RR : OP_DIVVF3_RR, to.RegNum, op.RegNum, len.RegNum);
		len.Free(build);
	}
	op.Free(build);
	return to;
}

//==========================================================================
//
// FxSequence :: Resolve
//
//==========================================================================

FxExpression *FxSequence::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	bool fail = false;
	for (unsigned i = 0; i < Expressions.Size(); ++i)
	{
		if (nullptr == (Expressions[i] = Expressions[i]->Resolve(ctx)))
		{
			fail = true;
		}
	}
	if (fail)
	{
		delete this;
		return nullptr;
	}
	return this;
}

//==========================================================================
//
// FxSequence :: CheckReturn
//
//==========================================================================

bool FxSequence::CheckReturn()
{
	// a sequence always returns when its last element returns.
	return Expressions.Size() > 0 && Expressions.Last()->CheckReturn();
}

//==========================================================================
//
// FxSequence :: Emit
//
//==========================================================================

ExpEmit FxSequence::Emit(VMFunctionBuilder *build)
{
	for (unsigned i = 0; i < Expressions.Size(); ++i)
	{
		ExpEmit v = Expressions[i]->Emit(build);
		// Throw away any result. We don't care about it.
		v.Free(build);
	}
	return ExpEmit();
}

//==========================================================================
//
// FxSequence :: GetDirectFunction
//
//==========================================================================

VMFunction *FxSequence::GetDirectFunction()
{
	if (Expressions.Size() == 1)
	{
		return Expressions[0]->GetDirectFunction();
	}
	return nullptr;
}

//==========================================================================
//
// FxCompoundStatement :: Resolve
//
//==========================================================================

FxExpression *FxCompoundStatement::Resolve(FCompileContext &ctx)
{
	auto outer = ctx.Block;
	Outer = ctx.Block;
	ctx.Block = this;
	auto x = FxSequence::Resolve(ctx);
	ctx.Block = outer;
	return x;
}

//==========================================================================
//
// FxCompoundStatement :: Emit
//
//==========================================================================

ExpEmit FxCompoundStatement::Emit(VMFunctionBuilder *build)
{
	auto e = FxSequence::Emit(build);
	// Release all local variables in this block.
	for (auto l : LocalVars)
	{
		l->Release(build);
	}
	return e;
}

//==========================================================================
//
// FxCompoundStatement :: FindLocalVariable
//
// Looks for a variable name in any of the containing compound statements
// This does a simple linear search on each block's variables. 
// The lists here normally don't get large enough to justify something more complex.
//
//==========================================================================

FxLocalVariableDeclaration *FxCompoundStatement::FindLocalVariable(FName name, FCompileContext &ctx)
{
	auto block = this;
	while (block != nullptr)
	{
		for (auto l : block->LocalVars)
		{
			if (l->Name == name)
			{
				return l;
			}
		}
		block = block->Outer;
	}
	// finally check the context for function arguments
	for (auto arg : ctx.FunctionArgs)
	{
		if (arg->Name == name)
		{
			return arg;
		}
	}
	return nullptr;
}

//==========================================================================
//
// FxCompoundStatement :: CheckLocalVariable
//
// Checks if the current block already contains a local variable 
// of the given name.
//
//==========================================================================

bool FxCompoundStatement::CheckLocalVariable(FName name)
{
	for (auto l : LocalVars)
	{
		if (l->Name == name)
		{
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// FxSwitchStatement
//
//==========================================================================

FxSwitchStatement::FxSwitchStatement(FxExpression *cond, FArgumentList *content, const FScriptPosition &pos)
	: FxExpression(EFX_SwitchStatement, pos)
{
	Condition = new FxIntCast(cond, false);
	Content = content;
}

FxSwitchStatement::~FxSwitchStatement()
{
	SAFE_DELETE(Condition);
	SAFE_DELETE(Content);
}

FxExpression *FxSwitchStatement::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Condition, ctx);

	if (Content == nullptr || Content->Size() == 0)
	{
		ScriptPosition.Message(MSG_WARNING, "Empty switch statement");
		if (Condition->isConstant())
		{
			return new FxNop(ScriptPosition);
		}
		else
		{
			// The condition may have a side effect so it should be processed (possible to-do: Analyze all nodes in there and delete if not.)
			auto x = Condition;
			Condition = nullptr;
			delete this;
			x->NeedResult = false;
			return x;
		}
	}

	for (auto &line : *Content)
	{
		// Do not resolve breaks, they need special treatment inside switch blocks.
		if (line->ExprType != EFX_JumpStatement || static_cast<FxJumpStatement *>(line)->Token != TK_Break)
		{
			SAFE_RESOLVE(line, ctx);
			line->NeedResult = false;
		}
	}

	if (Condition->isConstant())
	{
		ScriptPosition.Message(MSG_WARNING, "Case expression is constant");
		auto &content = *Content;
		int defaultindex = -1;
		int defaultbreak = -1;
		int caseindex = -1;
		int casebreak = -1;
		// look for a case label with a matching value
		for (unsigned i = 0; i < content.Size(); i++)
		{
			if (content[i] != nullptr)
			{
				if (content[i]->ExprType == EFX_CaseStatement)
				{
					auto casestmt = static_cast<FxCaseStatement *>(content[i]);
					if (casestmt->Condition == nullptr) defaultindex = i;
					else if (casestmt->CaseValue == static_cast<FxConstant *>(Condition)->GetValue().GetInt()) caseindex = i;
				}
				if (content[i]->ExprType == EFX_JumpStatement && static_cast<FxJumpStatement *>(content[i])->Token == TK_Break)
				{
					if (defaultindex >= 0 && defaultbreak < 0) defaultbreak = i;
					if (caseindex >= 0 && casebreak < 0)
					{
						casebreak = i;
						break;	// when we find this we do not need to look any further.
					}
				}
			}
		}
		if (caseindex < 0)
		{
			caseindex = defaultindex;
			casebreak = defaultbreak;
		}
		if (caseindex > 0 && casebreak - caseindex > 1)
		{
			auto seq = new FxSequence(ScriptPosition);
			for (int i = caseindex + 1; i < casebreak; i++)
			{
				if (content[i] != nullptr && content[i]->ExprType != EFX_CaseStatement)
				{
					seq->Add(content[i]);
					content[i] = nullptr;
				}
			}
			delete this;
			return seq->Resolve(ctx);
		}
		delete this;
		return new FxNop(ScriptPosition);
	}

	int mincase = INT_MAX;
	int maxcase = INT_MIN;
	for (auto line : *Content)
	{
		if (line->ExprType == EFX_CaseStatement)
		{
			auto casestmt = static_cast<FxCaseStatement *>(line);
			if (casestmt->Condition != nullptr)
			{
				CaseAddr ca = { casestmt->CaseValue, 0 };
				CaseAddresses.Push(ca);
				if (ca.casevalue < mincase) mincase = ca.casevalue;
				if (ca.casevalue > maxcase) maxcase = ca.casevalue;
			}
		}
	}
	return this;
}

ExpEmit FxSwitchStatement::Emit(VMFunctionBuilder *build)
{
	assert(Condition != nullptr);
	ExpEmit emit = Condition->Emit(build);
	assert(emit.RegType == REGT_INT);
	// todo: 
	// - sort jump table by value.
	// - optimize the switch dispatcher to run in native code instead of executing each single branch instruction on its own.
	// e.g.: build->Emit(OP_SWITCH, emit.RegNum, build->GetConstantInt(CaseAddresses.Size());
	for (auto &ca : CaseAddresses)
	{
		if (ca.casevalue >= 0 && ca.casevalue <= 0xffff)
		{
			build->Emit(OP_TEST, emit.RegNum, (VM_SHALF)ca.casevalue);
		}
		else if (ca.casevalue < 0 && ca.casevalue >= -0xffff)
		{
			build->Emit(OP_TESTN, emit.RegNum, (VM_SHALF)-ca.casevalue);
		}
		else
		{
			build->Emit(OP_EQ_K, 1, emit.RegNum, build->GetConstantInt(ca.casevalue));
		}
		ca.jumpaddress = build->Emit(OP_JMP, 0);
	}
	size_t DefaultAddress = build->Emit(OP_JMP, 0);
	TArray<size_t> BreakAddresses;

	for (auto line : *Content)
	{
		switch (line->ExprType)
		{
		case EFX_CaseStatement:
			if (static_cast<FxCaseStatement *>(line)->Condition != nullptr)
			{
				for (auto &ca : CaseAddresses)
				{
					if (ca.casevalue == static_cast<FxCaseStatement *>(line)->CaseValue)
					{
						build->BackpatchToHere(ca.jumpaddress);
						break;
					}
				}
			}
			else
			{
				build->BackpatchToHere(DefaultAddress);
			}
			break;

		case EFX_JumpStatement:
			if (static_cast<FxJumpStatement *>(line)->Token == TK_Break)
			{
				BreakAddresses.Push(build->Emit(OP_JMP, 0));
				break;
			}
			// fall through for continue.

		default:
			line->Emit(build);
			break;
		}
	}
	for (auto addr : BreakAddresses)
	{
		build->BackpatchToHere(addr);
	}
	return ExpEmit();
}

//==========================================================================
//
// FxSequence :: CheckReturn
//
//==========================================================================

bool FxSwitchStatement::CheckReturn()
{
	//A switch statement returns when it contains no breaks and ends with a return
	for (auto line : *Content)
	{
		if (line->ExprType == EFX_JumpStatement)
		{
			return false;	// Break means that the end of the statement will be reached, Continue cannot happen in the last statement of the last block.
		}
	}
	return Content->Size() > 0 && Content->Last()->CheckReturn();
}

//==========================================================================
//
// FxCaseStatement
//
//==========================================================================

FxCaseStatement::FxCaseStatement(FxExpression *cond, const FScriptPosition &pos)
	: FxExpression(EFX_CaseStatement, pos)
{
	Condition = cond? new FxIntCast(cond, false) : nullptr;
}

FxCaseStatement::~FxCaseStatement()
{
	SAFE_DELETE(Condition);
}

FxExpression *FxCaseStatement::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE_OPT(Condition, ctx);

	if (Condition != nullptr)
	{
		if (!Condition->isConstant())
		{
			ScriptPosition.Message(MSG_ERROR, "Case label must be a constant value");
			delete this;
			return nullptr;
		}
		CaseValue = static_cast<FxConstant *>(Condition)->GetValue().GetInt();
	}
	return this;
}

//==========================================================================
//
// FxIfStatement
//
//==========================================================================

FxIfStatement::FxIfStatement(FxExpression *cond, FxExpression *true_part,
	FxExpression *false_part, const FScriptPosition &pos)
: FxExpression(EFX_IfStatement, pos)
{
	Condition = cond;
	WhenTrue = true_part;
	WhenFalse = false_part;
	if (WhenTrue != nullptr) WhenTrue->NeedResult = false;
	if (WhenFalse != nullptr) WhenFalse->NeedResult = false;
	assert(cond != nullptr);
}

FxIfStatement::~FxIfStatement()
{
	SAFE_DELETE(Condition);
	SAFE_DELETE(WhenTrue);
	SAFE_DELETE(WhenFalse);
}

FxExpression *FxIfStatement::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();

	if (WhenTrue == nullptr && WhenFalse == nullptr)
	{ // We don't do anything either way, so disappear
		delete this;
		return nullptr;
	}

	SAFE_RESOLVE(Condition, ctx);

	if (Condition->ValueType != TypeBool)
	{
		Condition = new FxBoolCast(Condition, false);
		SAFE_RESOLVE(Condition, ctx);
	}

	if (WhenTrue != nullptr)
	{
		WhenTrue = WhenTrue->Resolve(ctx);
		ABORT(WhenTrue);
	}
	if (WhenFalse != nullptr)
	{
		WhenFalse = WhenFalse->Resolve(ctx);
		ABORT(WhenFalse);
	}

	ValueType = TypeVoid;

	if (Condition->isConstant())
	{
		ExpVal condval = static_cast<FxConstant *>(Condition)->GetValue();
		bool result = condval.GetBool();

		FxExpression *e = result ? WhenTrue : WhenFalse;
		delete (result ? WhenFalse : WhenTrue);
		WhenTrue = WhenFalse = nullptr;
		if (e == nullptr) e = new FxNop(ScriptPosition);	// create a dummy if this statement gets completely removed by optimizing out the constant parts.
		delete this;
		return e;
	}

	return this;
}

ExpEmit FxIfStatement::Emit(VMFunctionBuilder *build)
{
	ExpEmit v;
	size_t jumpspot;
	FxExpression *path1, *path2;
	int condcheck;

	// This is pretty much copied from FxConditional, except we don't
	// keep any results.
	ExpEmit cond = Condition->Emit(build);
	assert(cond.RegType != REGT_STRING && !cond.Konst);

	if (WhenTrue != nullptr)
	{
		path1 = WhenTrue;
		path2 = WhenFalse;
		condcheck = 1;
	}
	else
	{
		// When there is only a false path, reverse the condition so we can
		// treat it as a true path.
		assert(WhenFalse != nullptr);
		path1 = WhenFalse;
		path2 = nullptr;
		condcheck = 0;
	}

	// Test condition.

	switch (cond.RegType)
	{
	default:
	case REGT_INT:
		build->Emit(OP_EQ_K, condcheck, cond.RegNum, build->GetConstantInt(0));
		break;

	case REGT_FLOAT:
		build->Emit(OP_EQF_K, condcheck, cond.RegNum, build->GetConstantFloat(0));
		break;

	case REGT_POINTER:
		build->Emit(OP_EQA_K, condcheck, cond.RegNum, build->GetConstantAddress(nullptr, ATAG_GENERIC));
		break;
	}
	jumpspot = build->Emit(OP_JMP, 0);
	cond.Free(build);

	// Evaluate first path
	v = path1->Emit(build);
	v.Free(build);
	if (path2 != nullptr)
	{
		size_t path1jump;
		
		// if the branch ends with a return we do not need a terminating jmp.
		if (!path1->CheckReturn()) path1jump = build->Emit(OP_JMP, 0);
		else path1jump = 0xffffffff;
		// Evaluate second path
		build->BackpatchToHere(jumpspot);
		v = path2->Emit(build);
		v.Free(build);
		jumpspot = path1jump;
	}
	if (jumpspot != 0xffffffff) build->BackpatchToHere(jumpspot);
	return ExpEmit();
}


//==========================================================================
//
// FxIfStatement :: CheckReturn
//
//==========================================================================

bool FxIfStatement::CheckReturn()
{
	//An if statement returns if both branches return. Both branches must be present.
	return WhenTrue != nullptr && WhenTrue->CheckReturn() &&
			WhenFalse != nullptr && WhenFalse->CheckReturn();
}

//==========================================================================
//
// FxLoopStatement :: Resolve
//
// saves the loop pointer in the context and sets this object as the current loop
// so that continues and breaks always resolve to the innermost loop.
//
//==========================================================================

FxExpression *FxLoopStatement::Resolve(FCompileContext &ctx)
{
	auto outer = ctx.Loop;
	ctx.Loop = this;
	auto x = DoResolve(ctx);
	ctx.Loop = outer;
	return x;
}

void FxLoopStatement::Backpatch(VMFunctionBuilder *build, size_t loopstart, size_t loopend)
{
	// Give a proper address to any break/continue statement within this loop.
	for (unsigned int i = 0; i < Jumps.Size(); i++)
	{
		if (Jumps[i]->Token == TK_Break)
		{
			build->Backpatch(Jumps[i]->Address, loopend);
		}
		else
		{ // Continue statement.
			build->Backpatch(Jumps[i]->Address, loopstart);
		}
	}
}

//==========================================================================
//
// FxWhileLoop
//
//==========================================================================

FxWhileLoop::FxWhileLoop(FxExpression *condition, FxExpression *code, const FScriptPosition &pos)
: FxLoopStatement(EFX_WhileLoop, pos), Condition(condition), Code(code)
{
	ValueType = TypeVoid;
}

FxWhileLoop::~FxWhileLoop()
{
	SAFE_DELETE(Condition);
	SAFE_DELETE(Code);
}

FxExpression *FxWhileLoop::DoResolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Condition, ctx);
	SAFE_RESOLVE_OPT(Code, ctx);

	if (Condition->ValueType != TypeBool)
	{
		Condition = new FxBoolCast(Condition);
		SAFE_RESOLVE(Condition, ctx);
	}

	if (Condition->isConstant())
	{
		if (static_cast<FxConstant *>(Condition)->GetValue().GetBool() == false)
		{ // Nothing happens
			FxExpression *nop = new FxNop(ScriptPosition);
			delete this;
			return nop;
		}
		else if (Code == nullptr)
		{ // "while (true) { }"
		  // Someone could be using this for testing.
			ScriptPosition.Message(MSG_WARNING, "Infinite empty loop");
		}
	}

	return this;
}

ExpEmit FxWhileLoop::Emit(VMFunctionBuilder *build)
{
	assert(Condition->ValueType == TypeBool);

	size_t loopstart, loopend;
	size_t jumpspot;

	// Evaluate the condition and execute/break out of the loop.
	loopstart = build->GetAddress();
	if (!Condition->isConstant())
	{
		ExpEmit cond = Condition->Emit(build);
		build->Emit(OP_TEST, cond.RegNum, 0);
		jumpspot = build->Emit(OP_JMP, 0);
		cond.Free(build);
	}
	else assert(static_cast<FxConstant *>(Condition)->GetValue().GetBool() == true);

	// Execute the loop's content.
	if (Code != nullptr)
	{
		ExpEmit code = Code->Emit(build);
		code.Free(build);
	}

	// Loop back.
	build->Backpatch(build->Emit(OP_JMP, 0), loopstart);
	loopend = build->GetAddress();

	if (!Condition->isConstant()) 
	{
		build->Backpatch(jumpspot, loopend);
	}

	Backpatch(build, loopstart, loopend);
	return ExpEmit();
}

//==========================================================================
//
// FxDoWhileLoop
//
//==========================================================================

FxDoWhileLoop::FxDoWhileLoop(FxExpression *condition, FxExpression *code, const FScriptPosition &pos)
: FxLoopStatement(EFX_DoWhileLoop, pos), Condition(condition), Code(code)
{
	ValueType = TypeVoid;
}

FxDoWhileLoop::~FxDoWhileLoop()
{
	SAFE_DELETE(Condition);
	SAFE_DELETE(Code);
}

FxExpression *FxDoWhileLoop::DoResolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Condition, ctx);
	SAFE_RESOLVE_OPT(Code, ctx);

	if (Condition->ValueType != TypeBool)
	{
		Condition = new FxBoolCast(Condition);
		SAFE_RESOLVE(Condition, ctx);
	}

	if (Condition->isConstant())
	{
		if (static_cast<FxConstant *>(Condition)->GetValue().GetBool() == false)
		{ // The code executes once, if any.
			if (Jumps.Size() == 0)
			{ // We would still have to handle the jumps however.
				FxExpression *e = Code;
				if (e == nullptr) e = new FxNop(ScriptPosition);
				Code = nullptr;
				delete this;
				return e;
			}
		}
		else if (Code == nullptr)
		{ // "do { } while (true);"
		  // Someone could be using this for testing.
			ScriptPosition.Message(MSG_WARNING, "Infinite empty loop");
		}
	}

	return this;
}

ExpEmit FxDoWhileLoop::Emit(VMFunctionBuilder *build)
{
	assert(Condition->ValueType == TypeBool);

	size_t loopstart, loopend;
	size_t codestart;

	// Execute the loop's content.
	codestart = build->GetAddress();
	if (Code != nullptr)
	{
		ExpEmit code = Code->Emit(build);
		code.Free(build);
	}

	// Evaluate the condition and execute/break out of the loop.
	loopstart = build->GetAddress();
	if (!Condition->isConstant())
	{
		ExpEmit cond = Condition->Emit(build);
		build->Emit(OP_TEST, cond.RegNum, 1);
		cond.Free(build);
		build->Backpatch(build->Emit(OP_JMP, 0), codestart);
	}
	else if (static_cast<FxConstant *>(Condition)->GetValue().GetBool() == true)
	{ // Always looping
		build->Backpatch(build->Emit(OP_JMP, 0), codestart);
	}
	loopend = build->GetAddress();

	Backpatch(build, loopstart, loopend);

	return ExpEmit();
}

//==========================================================================
//
// FxForLoop
//
//==========================================================================

FxForLoop::FxForLoop(FxExpression *init, FxExpression *condition, FxExpression *iteration, FxExpression *code, const FScriptPosition &pos)
: FxLoopStatement(EFX_ForLoop, pos), Init(init), Condition(condition), Iteration(iteration), Code(code)
{
	ValueType = TypeVoid;
	if (Iteration != nullptr) Iteration->NeedResult = false;
	if (Code != nullptr) Code->NeedResult = false;
}

FxForLoop::~FxForLoop()
{
	SAFE_DELETE(Init);
	SAFE_DELETE(Condition);
	SAFE_DELETE(Iteration);
	SAFE_DELETE(Code);
}

FxExpression *FxForLoop::DoResolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE_OPT(Init, ctx);
	SAFE_RESOLVE_OPT(Condition, ctx);
	SAFE_RESOLVE_OPT(Iteration, ctx);
	SAFE_RESOLVE_OPT(Code, ctx);

	if (Condition != nullptr)
	{
		if (Condition->ValueType != TypeBool)
		{
			Condition = new FxBoolCast(Condition);
			SAFE_RESOLVE(Condition, ctx);
		}

		if (Condition->isConstant())
		{
			if (static_cast<FxConstant *>(Condition)->GetValue().GetBool() == false)
			{ // Nothing happens
				FxExpression *nop = new FxNop(ScriptPosition);
				delete this;
				return nop;
			}
			else
			{ // "for (..; true; ..)"
				delete Condition;
				Condition = nullptr;
			}
		}
	}
	if (Condition == nullptr && Code == nullptr)
	{ // "for (..; ; ..) { }"
	  // Someone could be using this for testing.
		ScriptPosition.Message(MSG_WARNING, "Infinite empty loop");
	}

	return this;
}

ExpEmit FxForLoop::Emit(VMFunctionBuilder *build)
{
	assert((Condition && Condition->ValueType == TypeBool && !Condition->isConstant()) || Condition == nullptr);

	size_t loopstart, loopend;
	size_t codestart;
	size_t jumpspot;

	// Init statement (only used by DECORATE. ZScript is pulling it before the loop statement and enclosing the entire loop in a compound statement so that Init can have local variables.)
	if (Init != nullptr)
	{
		ExpEmit init = Init->Emit(build);
		init.Free(build);
	}

	// Evaluate the condition and execute/break out of the loop.
	codestart = build->GetAddress();
	if (Condition != nullptr)
	{
		ExpEmit cond = Condition->Emit(build);
		build->Emit(OP_TEST, cond.RegNum, 0);
		cond.Free(build);
		jumpspot = build->Emit(OP_JMP, 0);
	}

	// Execute the loop's content.
	if (Code != nullptr)
	{
		ExpEmit code = Code->Emit(build);
		code.Free(build);
	}

	// Iteration statement.
	loopstart = build->GetAddress();
	if (Iteration != nullptr)
	{
		ExpEmit iter = Iteration->Emit(build);
		iter.Free(build);
	}
	build->Backpatch(build->Emit(OP_JMP, 0), codestart);

	// End of loop.
	loopend = build->GetAddress();
	if (Condition != nullptr)
	{
		build->Backpatch(jumpspot, loopend);
	}

	Backpatch(build, loopstart, loopend);
	return ExpEmit();
}

//==========================================================================
//
// FxJumpStatement
//
//==========================================================================

FxJumpStatement::FxJumpStatement(int token, const FScriptPosition &pos)
: FxExpression(EFX_JumpStatement, pos), Token(token)
{
	ValueType = TypeVoid;
}

FxExpression *FxJumpStatement::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();

	if (ctx.Loop != nullptr)
	{
		ctx.Loop->Jumps.Push(this);
		return this;
	}
	else
	{
		ScriptPosition.Message(MSG_ERROR, "'%s' outside of a loop", Token == TK_Break ? "break" : "continue");
		delete this;
		return nullptr;
	}
}

ExpEmit FxJumpStatement::Emit(VMFunctionBuilder *build)
{
	Address = build->Emit(OP_JMP, 0);

	return ExpEmit();
}

//==========================================================================
//
//==========================================================================

FxReturnStatement::FxReturnStatement(FxExpression *value, const FScriptPosition &pos)
: FxExpression(EFX_ReturnStatement, pos), Value(value)
{
	ValueType = TypeVoid;
}

FxReturnStatement::~FxReturnStatement()
{
	SAFE_DELETE(Value);
}

FxExpression *FxReturnStatement::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE_OPT(Value, ctx);

	PPrototype *retproto;
	if (Value == nullptr)
	{
		TArray<PType *> none(0);
		retproto = NewPrototype(none, none);
	}
	else
	{
		retproto = Value->ReturnProto();
	}

	ctx.CheckReturn(retproto, ScriptPosition);

	return this;
}

ExpEmit FxReturnStatement::Emit(VMFunctionBuilder *build)
{
	ExpEmit out(0, REGT_NIL);

	// If we return nothing, use a regular RET opcode.
	// Otherwise just return the value we're given.
	if (Value == nullptr)
	{
		build->Emit(OP_RET, RET_FINAL, REGT_NIL, 0);
	}
	else
	{
		out = Value->Emit(build);

		// Check if it is a function call that simplified itself
		// into a tail call in which case we don't emit anything.
		if (!out.Final)
		{
			if (Value->ValueType == TypeVoid)
			{ // Nothing is returned.
				build->Emit(OP_RET, RET_FINAL, REGT_NIL, 0);
			}
			else
			{
				build->Emit(OP_RET, RET_FINAL, EncodeRegType(out), out.RegNum);
			}
		}
	}

	out.Final = true;
	return out;
}

VMFunction *FxReturnStatement::GetDirectFunction()
{
	if (Value != nullptr)
	{
		return Value->GetDirectFunction();
	}
	return nullptr;
}

//==========================================================================
//
//==========================================================================

FxClassTypeCast::FxClassTypeCast(PClassPointer *dtype, FxExpression *x)
: FxExpression(EFX_ClassTypeCast, x->ScriptPosition)
{
	ValueType = dtype;
	desttype = dtype->ClassRestriction;
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

	if (basex->ValueType == TypeNullPtr)
	{
		basex->ValueType = ValueType;
		auto x = basex;
		basex = nullptr;
		delete this;
		return x;
	}
	auto to = static_cast<PClassPointer *>(ValueType);
	if (basex->ValueType->GetClass() == RUNTIME_CLASS(PClassPointer))
	{
		auto from = static_cast<PClassPointer *>(basex->ValueType);
		if (from->ClassRestriction->IsDescendantOf(to->ClassRestriction))
		{
			basex->ValueType = to;
			auto x = basex;
			basex = nullptr;
			delete this;
			return x;
		}
		ScriptPosition.Message(MSG_ERROR, "Cannot convert from %s to %s: Incompatible class types", from->ClassRestriction->TypeName.GetChars(), to->ClassRestriction->TypeName.GetChars());
		delete this;
		return nullptr;
	}
	
	if (basex->ValueType != TypeName && basex->ValueType != TypeString)
	{
		ScriptPosition.Message(MSG_ERROR, "Cannot convert %s to class type", basex->ValueType->DescriptiveName());
		delete this;
		return nullptr;
	}

	if (basex->isConstant())
	{
		FName clsname = static_cast<FxConstant *>(basex)->GetValue().GetName();
		PClass *cls = nullptr;

		if (clsname != NAME_None)
		{
			cls = PClass::FindClass(clsname);
			if (cls == nullptr)
			{
				/* lax */
				// Since this happens in released WADs it must pass without a terminal error... :(
				ScriptPosition.Message(MSG_OPTERROR,
					"Unknown class name '%s'",
					clsname.GetChars(), desttype->TypeName.GetChars());
			}
			else
			{
				if (!cls->IsDescendantOf(desttype))
				{
					ScriptPosition.Message(MSG_ERROR, "class '%s' is not compatible with '%s'", clsname.GetChars(), desttype->TypeName.GetChars());
					delete this;
					return nullptr;
				}
				ScriptPosition.Message(MSG_DEBUG, "resolving '%s' as class name", clsname.GetChars());
			}
		}
		FxExpression *x = new FxConstant(cls, to, ScriptPosition);
		delete this;
		return x;
	}
	if (basex->ValueType == TypeString)
	{
		basex = new FxNameCast(basex);
	}
	return this;
}

//==========================================================================
//
// 
//
//==========================================================================

int BuiltinNameToClass(VMFrameStack *stack, VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret)
{
	assert(numparam == 2);
	assert(numret == 1);
	assert(param[0].Type == REGT_INT);
	assert(param[1].Type == REGT_POINTER);
	assert(ret->RegType == REGT_POINTER);

	FName clsname = ENamedName(param[0].i);
	const PClass *cls = PClass::FindClass(clsname);
	const PClass *desttype = reinterpret_cast<PClass *>(param[1].a);

	if (!cls->IsDescendantOf(desttype))
	{
		Printf("class '%s' is not compatible with '%s'", clsname.GetChars(), desttype->TypeName.GetChars());
		cls = nullptr;
	}
	ret->SetPointer(const_cast<PClass *>(cls), ATAG_OBJECT);
	return 1;
}

ExpEmit FxClassTypeCast::Emit(VMFunctionBuilder *build)
{
	if (basex->ValueType != TypeName)
	{
		return ExpEmit(build->GetConstantAddress(nullptr, ATAG_OBJECT), REGT_POINTER, true);
	}
	ExpEmit clsname = basex->Emit(build);
	assert(!clsname.Konst);
	ExpEmit dest(build, REGT_POINTER);
	build->Emit(OP_PARAM, 0, clsname.RegType, clsname.RegNum);
	build->Emit(OP_PARAM, 0, REGT_POINTER | REGT_KONST, build->GetConstantAddress(const_cast<PClass *>(desttype), ATAG_OBJECT));

	// Call the BuiltinNameToClass function to convert from 'name' to class.
	VMFunction *callfunc;
	PSymbol *sym = FindBuiltinFunction(NAME_BuiltinNameToClass, BuiltinNameToClass);

	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
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
	ABORT(ctx.Class);
	auto aclass = dyn_cast<PClassActor>(ctx.Class);

	// This expression type can only be used from actors, for everything else it has already produced a compile error.
	assert(aclass != nullptr && aclass->NumOwnedStates > 0);

	if (aclass->NumOwnedStates <= index)
	{
		ScriptPosition.Message(MSG_ERROR, "%s: Attempt to jump to non existing state index %d", 
			ctx.Class->TypeName.GetChars(), index);
		delete this;
		return nullptr;
	}
	FxExpression *x = new FxConstant(aclass->OwnedStates + index, ScriptPosition);
	delete this;
	return x;
}

//==========================================================================
//
//
//
//==========================================================================

FxRuntimeStateIndex::FxRuntimeStateIndex(FxExpression *index)
: FxExpression(EFX_RuntimeStateIndex, index->ScriptPosition), Index(index)
{
	EmitTail = false;
	ValueType = TypeState;
}

FxRuntimeStateIndex::~FxRuntimeStateIndex()
{
	SAFE_DELETE(Index);
}

PPrototype *FxRuntimeStateIndex::ReturnProto()
{
	EmitTail = true;
	return FxExpression::ReturnProto();
}

FxExpression *FxRuntimeStateIndex::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(Index, ctx);

	if (!Index->IsNumeric())
	{
		ScriptPosition.Message(MSG_ERROR, "Numeric type expected");
		delete this;
		return nullptr;
	}
	else if (Index->isConstant())
	{
		int index = static_cast<FxConstant *>(Index)->GetValue().GetInt();
		if (index < 0 || (index == 0 && !ctx.FromDecorate))
		{
			ScriptPosition.Message(MSG_ERROR, "State index must be positive");
			delete this;
			return nullptr;
		}
		else if (index == 0)
		{
			auto x = new FxConstant((FState*)nullptr, ScriptPosition);
			delete this;
			return x->Resolve(ctx);
		}
		else
		{
			auto x = new FxStateByIndex(index, ScriptPosition);
			delete this;
			return x->Resolve(ctx);
		}
	}
	else if (Index->ValueType->GetRegType() != REGT_INT)
	{ // Float.
		Index = new FxIntCast(Index, ctx.FromDecorate);
		SAFE_RESOLVE(Index, ctx);
	}

	return this;
}

static bool VerifyJumpTarget(AActor *stateowner, FStateParamInfo *stateinfo, int index)
{
	PClassActor *cls = stateowner->GetClass();

	if (stateinfo->mCallingState != nullptr)
	{
	while (cls != RUNTIME_CLASS(AActor))
	{
		// both calling and target state need to belong to the same class.
		if (cls->OwnsState(stateinfo->mCallingState))
		{
			return cls->OwnsState(stateinfo->mCallingState + index);
		}

		// We can safely assume the ParentClass is of type PClassActor
		// since we stop when we see the Actor base class.
		cls = static_cast<PClassActor *>(cls->ParentClass);
	}
	}
	return false;
}

static int BuiltinHandleRuntimeState(VMFrameStack *stack, VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(stateowner, AActor);
	PARAM_POINTER(stateinfo, FStateParamInfo);
	PARAM_INT(index);

	if (index == 0 || !VerifyJumpTarget(stateowner, stateinfo, index))
	{
		// Null is returned if the location was invalid which means that no jump will be performed
		// if used as return value
		// 0 always meant the same thing so we handle it here for compatibility
		ACTION_RETURN_STATE(nullptr);
	}
	else
	{
		ACTION_RETURN_STATE(stateinfo->mCallingState + index);
	}
}

ExpEmit FxRuntimeStateIndex::Emit(VMFunctionBuilder *build)
{
	// This can only be called from inline state functions which must be VARF_Action.
	assert(build->NumImplicits >= NAP && build->Registers[REGT_POINTER].GetMostUsed() >= build->NumImplicits &&
		"FxRuntimeStateIndex is only valid inside action functions");

	ExpEmit out(build, REGT_POINTER);

	build->Emit(OP_PARAM, 0, REGT_POINTER, 1); // stateowner
	build->Emit(OP_PARAM, 0, REGT_POINTER, 2); // stateinfo
	ExpEmit id = Index->Emit(build);
	build->Emit(OP_PARAM, 0, REGT_INT | (id.Konst ? REGT_KONST : 0), id.RegNum); // index

	VMFunction *callfunc;
	PSymbol *sym;
	
	sym = FindBuiltinFunction(NAME_BuiltinHandleRuntimeState, BuiltinHandleRuntimeState);
	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	if (EmitTail)
	{
		build->Emit(OP_TAIL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 3, 1);
		out.Final = true;
	}
	else
	{
		build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), 3, 1);
		build->Emit(OP_RESULT, 0, REGT_POINTER, out.RegNum);
	}

	return out;
}

//==========================================================================
//
//
//
//==========================================================================

FxMultiNameState::FxMultiNameState(const char *_statestring, const FScriptPosition &pos)
	:FxExpression(EFX_MultiNameState, pos)
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
		scopename = NAME_None;
	}
	names = MakeStateNameList(statestring);
	names.Insert(0, scopename);
	scope = nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

FxExpression *FxMultiNameState::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	ABORT(ctx.Class);

	if (names[0] == NAME_None)
	{
		scope = nullptr;
	}
	else if (names[0] == NAME_Super)
	{
		scope = dyn_cast<PClassActor>(ctx.Class->ParentClass);
	}
	else
	{
		scope = PClass::FindActor(names[0]);
		if (scope == nullptr)
		{
			ScriptPosition.Message(MSG_ERROR, "Unknown class '%s' in state label", names[0].GetChars());
			delete this;
			return nullptr;
		}
		else if (!scope->IsAncestorOf(ctx.Class) && ctx.Class != RUNTIME_CLASS(AActor)) // AActor needs access to subclasses in a few places. TBD: Relax this for non-action functions?
		{
			ScriptPosition.Message(MSG_ERROR, "'%s' is not an ancestor of '%s'", names[0].GetChars(), ctx.Class->TypeName.GetChars());
			delete this;
			return nullptr;
		}
	}
	if (scope != nullptr)
	{
		FState *destination = nullptr;
		// If the label is class specific we can resolve it right here
		if (names[1] != NAME_None)
		{
			destination = scope->FindState(names.Size()-1, &names[1], false);
			if (destination == nullptr)
			{
				ScriptPosition.Message(MSG_OPTERROR, "Unknown state jump destination");
				/* lax */
				return this;
			}
		}
		FxExpression *x = new FxConstant(destination, ScriptPosition);
		delete this;
		return x;
	}
	names.Delete(0);
	names.ShrinkToFit();
	ValueType = TypeState;
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

static int DoFindState(VMFrameStack *stack, VMValue *param, int numparam, VMReturn *ret, FName *names, int numnames)
{
	PARAM_OBJECT_AT(0, self, AActor);
	FState *state = self->GetClass()->FindState(numparam - 1, names);
	if (state == nullptr)
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

// Find a state with any number of dots in its name.
int BuiltinFindMultiNameState(VMFrameStack *stack, VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret)
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
	return DoFindState(stack, param, numparam, ret, names, numparam - 1);
}

// Find a state without any dots in its name.
int BuiltinFindSingleNameState(VMFrameStack *stack, VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret)
{
	assert(numparam == 2);
	assert(numret == 1);
	assert(ret->RegType == REGT_POINTER);

	PARAM_NAME_AT(1, zaname);
	return DoFindState(stack, param, numparam, ret, &zaname, 1);
}

ExpEmit FxMultiNameState::Emit(VMFunctionBuilder *build)
{
	ExpEmit dest(build, REGT_POINTER);
	if (build->NumImplicits == NAP)
	{
		build->Emit(OP_PARAM, 0, REGT_POINTER, 1);		// pass stateowner
	}
	else
	{
		build->Emit(OP_PARAM, 0, REGT_POINTER, 0);		// pass self
	}
	for (unsigned i = 0; i < names.Size(); ++i)
	{
		build->EmitParamInt(names[i]);
	}

	// For one name, use the BuiltinFindSingleNameState function. For more than
	// one name, use the BuiltinFindMultiNameState function.
	VMFunction *callfunc;
	PSymbol *sym;
	
	if (names.Size() == 1)
	{
		sym = FindBuiltinFunction(NAME_BuiltinFindSingleNameState, BuiltinFindSingleNameState);
	}
	else
	{
		sym = FindBuiltinFunction(NAME_BuiltinFindMultiNameState, BuiltinFindMultiNameState);
	}

	assert(sym->IsKindOf(RUNTIME_CLASS(PSymbolVMFunction)));
	assert(((PSymbolVMFunction *)sym)->Function != nullptr);
	callfunc = ((PSymbolVMFunction *)sym)->Function;

	build->Emit(OP_CALL_K, build->GetConstantAddress(callfunc, ATAG_OBJECT), names.Size() + 1, 1);
	build->Emit(OP_RESULT, 0, REGT_POINTER, dest.RegNum);
	return dest;
}

//==========================================================================
//
// declares a single local variable (no arrays)
//
//==========================================================================

FxLocalVariableDeclaration::FxLocalVariableDeclaration(PType *type, FName name, FxExpression *initval, int varflags, const FScriptPosition &p)
	:FxExpression(EFX_LocalVariableDeclaration, p)
{
	ValueType = type;
	VarFlags = varflags;
	Name = name;
	RegCount = type == TypeVector2 ? 2 : type == TypeVector3 ? 3 : 1;
	Init = initval == nullptr? nullptr : new FxTypeCast(initval, type, false);
}

FxExpression *FxLocalVariableDeclaration::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE_OPT(Init, ctx);
	if (ctx.Block == nullptr)
	{
		ScriptPosition.Message(MSG_ERROR, "Variable declaration outside compound statement");
		delete this;
		return nullptr;
	}
	ctx.Block->LocalVars.Push(this);
	return this;
}

ExpEmit FxLocalVariableDeclaration::Emit(VMFunctionBuilder *build)
{
	if (Init == nullptr)
	{
		RegNum = build->Registers[ValueType->GetRegType()].Get(RegCount);
	}
	else
	{
		ExpEmit emitval = Init->Emit(build);

		int regtype = emitval.RegType;
		if (regtype < REGT_INT || regtype > REGT_TYPE)
		{
			ScriptPosition.Message(MSG_ERROR, "Attempted to assign a non-value");
			return ExpEmit();
		}
		if (emitval.Konst)
		{
			auto constval = static_cast<FxConstant *>(Init);
			RegNum = build->Registers[regtype].Get(1);
			switch (regtype)
			{
			default:
			case REGT_INT:
				build->Emit(OP_LK, RegNum, build->GetConstantInt(constval->GetValue().GetInt()));
				break;

			case REGT_FLOAT:
				build->Emit(OP_LKF, RegNum, build->GetConstantFloat(constval->GetValue().GetFloat()));
				break;

			case REGT_POINTER:
				build->Emit(OP_LKP, RegNum, build->GetConstantAddress(constval->GetValue().GetPointer(), ATAG_GENERIC));
				break;

			case REGT_STRING:
				build->Emit(OP_LKS, RegNum, build->GetConstantString(constval->GetValue().GetString()));
			}
			emitval.Free(build);
		}
		else if (Init->ExprType != EFX_LocalVariable)
		{
			// take over the register that got allocated while emitting the Init expression.
			RegNum = emitval.RegNum;
		}
		else
		{
			ExpEmit out(build, emitval.RegType, emitval.RegCount);
			build->Emit(ValueType->GetMoveOp(), out.RegNum, emitval.RegNum);
			RegNum = out.RegNum;
		}
	}
	return ExpEmit();
}

void FxLocalVariableDeclaration::Release(VMFunctionBuilder *build)
{
	// Release the register after the containing block gets closed
	assert(RegNum != -1);
	build->Registers[ValueType->GetRegType()].Return(RegNum, RegCount);
}
