/*
** codegen.cpp
**
** Compiler backend / code generation for ZScript and DECORATE
**
**---------------------------------------------------------------------------
** Copyright 2008-2016 Christoph Oelckers
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
#include "cmdlib.h"
#include "codegen.h"
#include "codegen_doom.h"
#include "v_text.h"
#include "filesystem.h"
#include "v_video.h"
#include "utf8.h"
#include "texturemanager.h"
#include "m_random.h"
#include "v_font.h"
#include "templates.h"
#include "actor.h"
#include "p_lnspec.h"
#include "g_levellocals.h"

PFunction* FindBuiltinFunction(FName funcname);

//==========================================================================
//
//
//
//==========================================================================

bool isActor(PContainerType *type)
{
	auto cls = PType::toClass(type);
	return cls ? cls->Descriptor->IsDescendantOf(RUNTIME_CLASS(AActor)) : false;
}

//==========================================================================
//
//
//
//==========================================================================

static FxExpression *CustomTypeCast(FxTypeCast *func, FCompileContext &ctx)
{
	if (func->ValueType == TypeStateLabel)
	{
		auto& basex = func->basex;
		auto& ScriptPosition = func->ScriptPosition;
		if (basex->ValueType == TypeNullPtr)
		{
			auto x = new FxConstant(0, ScriptPosition);
			x->ValueType = TypeStateLabel;
			delete func;
			return x;
		}
		// Right now this only supports string constants. There should be an option to pass a string variable, too.
		if (basex->isConstant() && (basex->ValueType == TypeString || basex->ValueType == TypeName))
		{
			FString s= static_cast<FxConstant *>(basex)->GetValue().GetString();
			if (s.Len() == 0 && !ctx.FromDecorate)	// DECORATE should never get here at all, but let's better be safe.
			{
				ScriptPosition.Message(MSG_ERROR, "State jump to empty label.");
				delete func;
				return nullptr;
			}
			FxExpression *x = new FxMultiNameState(s, basex->ScriptPosition);
			x = x->Resolve(ctx);
			basex = nullptr;
			delete func;
			return x;
		}
		else if (basex->IsNumeric() && basex->ValueType != TypeSound && basex->ValueType != TypeColor)
		{
			if (ctx.StateIndex < 0)
			{
				ScriptPosition.Message(MSG_ERROR, "State jumps with index can only be used in anonymous state functions.");
				delete func;
				return nullptr;
			}
			if (ctx.StateCount != 1)
			{
				ScriptPosition.Message(MSG_ERROR, "State jumps with index cannot be used on multistate definitions");
				delete func;
				return nullptr;
			}
			if (basex->isConstant())
			{
				int i = static_cast<FxConstant *>(basex)->GetValue().GetInt();
				if (i <= 0)
				{
					ScriptPosition.Message(MSG_ERROR, "State index must be positive");
					delete func;
					return nullptr;
				}
				FxExpression *x = new FxStateByIndex(ctx.StateIndex + i, ScriptPosition);
				x = x->Resolve(ctx);
				basex = nullptr;
				delete func;
				return x;
			}
			else
			{
				FxExpression *x = new FxRuntimeStateIndex(basex);
				x = x->Resolve(ctx);
				basex = nullptr;
				delete func;
				return x;
			}
		}
	}
	return func;
}

//==========================================================================
//
//
//
//==========================================================================

static bool CheckForCustomAddition(FxAddSub *func, FCompileContext &ctx)
{
	if (func->left->ValueType == TypeState && func->right->IsInteger() && func->Operator == '+' && !func->left->isConstant())
	{
		// This is the only special case of pointer addition that will be accepted - because it is used quite often in the existing game code.
		func->ValueType = TypeState;
		func->right = new FxMulDiv('*', func->right, new FxConstant((int)sizeof(FState), func->ScriptPosition));	// multiply by size here, so that constants can be better optimized.
		func->right = func->right->Resolve(ctx);
		return true;
	}
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

static FxExpression* CheckForDefault(FxIdentifier *func, FCompileContext &ctx)
{
	auto& ScriptPosition = func->ScriptPosition;

	if (func->Identifier == NAME_Default)
	{
		if (ctx.Function == nullptr)
		{
			ScriptPosition.Message(MSG_ERROR, "Unable to access class defaults from constant declaration");
			delete func;
			return nullptr;
		}
		if (ctx.Function->Variants[0].SelfClass == nullptr)
		{
			ScriptPosition.Message(MSG_ERROR, "Unable to access class defaults from static function");
			delete func;
			return nullptr;
		}
		if (!isActor(ctx.Function->Variants[0].SelfClass))
		{
			ScriptPosition.Message(MSG_ERROR, "'Default' requires an actor type.");
			delete func;
			return nullptr;
		}

		FxExpression * x = new FxClassDefaults(new FxSelf(ScriptPosition), ScriptPosition);
		delete func;
		return x->Resolve(ctx);
	}
	
	// and line specials
	int num;
	if ((num = P_FindLineSpecial(func->Identifier.GetChars(), nullptr, nullptr)))
	{
		ScriptPosition.Message(MSG_DEBUGLOG, "Resolving name '%s' as line special %d\n", func->Identifier.GetChars(), num);
		auto newex = new FxConstant(num, ScriptPosition);
		delete func;
		return newex? newex->Resolve(ctx) : nullptr;
	}
	return func;
}

//==========================================================================
//
//
//
//==========================================================================

static FxExpression *ResolveForDefault(FxIdentifier *expr, FxExpression*& object, PContainerType* objtype, FCompileContext &ctx)
{
	if (expr->Identifier == NAME_Default)
	{
		if (!isActor(objtype))
		{
			expr->ScriptPosition.Message(MSG_ERROR, "'Default' requires an actor type.");
			delete object;
			object = nullptr;
			return nullptr;
		}

		FxExpression * x = new FxClassDefaults(object, expr->ScriptPosition);
		object = nullptr;
		delete expr;
		return x->Resolve(ctx);
	}
	return expr;
}


//==========================================================================
//
//
//
//==========================================================================

FxExpression* CheckForMemberDefault(FxStructMember *func, FCompileContext &ctx)
{
	auto& membervar = func->membervar;
	auto& classx = func->classx;
	auto& ScriptPosition = func->ScriptPosition;

	if (membervar->SymbolName == NAME_Default)
	{
		if (!classx->ValueType->isObjectPointer()
			|| !static_cast<PObjectPointer *>(classx->ValueType)->PointedClass()->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			ScriptPosition.Message(MSG_ERROR, "'Default' requires an actor type");
			delete func;
			return nullptr;
		}
		FxExpression * x = new FxClassDefaults(classx, ScriptPosition);
		classx = nullptr;
		delete func;
		return x->Resolve(ctx);
	}
	return func;
}

//==========================================================================
//
// FxVMFunctionCall :: UnravelVarArgAJump
//
// Converts A_Jump(chance, a, b, c, d) -> A_Jump(chance, RandomPick[cajump](a, b, c, d))
// so that varargs are restricted to either text formatting or graphics drawing.
//
//==========================================================================
extern FRandom pr_cajump;

static bool UnravelVarArgAJump(FxVMFunctionCall *func, FCompileContext &ctx)
{
	auto& ArgList = func->ArgList;
	FArgumentList rplist;

	for (unsigned i = 1; i < ArgList.Size(); i++)
	{
		// This needs a bit of casting voodoo because RandomPick wants integer parameters.
		auto x = new FxIntCast(new FxTypeCast(ArgList[i], TypeStateLabel, true, true), true, true);
		rplist.Push(x->Resolve(ctx));
		ArgList[i] = nullptr;
		if (rplist[i - 1] == nullptr)
		{
			return false;
		}
	}
	FxExpression *x = new FxRandomPick(&pr_cajump, rplist, false, func->ScriptPosition, true);
	x = x->Resolve(ctx);
	// This cannot be done with a cast because that interprets the value as an index.
	// All we want here is to take the literal value and change its type.
	if (x) x->ValueType = TypeStateLabel;	
	ArgList[1] = x;
	ArgList.Clamp(2);
	return x != nullptr;
}

static bool AJumpProcessing(FxVMFunctionCall *func, FCompileContext &ctx)
{
	// Unfortunately the PrintableName is the only safe thing to catch this special case here.
	if (func->Function->Variants[0].Implementation->PrintableName.CompareNoCase("Actor.A_Jump [Native]") == 0)
	{
		// Unravel the varargs part of this function here so that the VM->native interface does not have to deal with it anymore.
		if (func->ArgList.Size() > 2)
		{
			auto ret = UnravelVarArgAJump(func, ctx);
			if (!ret)
			{
				return false;
			}
		}
	}
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool CheckArgSize(FName fname, FArgumentList &args, int min, int max, FScriptPosition &sc);

static FxExpression *ResolveGlobalCustomFunction(FxFunctionCall *func, FCompileContext &ctx)
{
	auto& ScriptPosition = func->ScriptPosition;
	if (func->MethodName == NAME_GetDefaultByType)
	{
		if (CheckArgSize(NAME_GetDefaultByType, func->ArgList, 1, 1, ScriptPosition))
		{
			auto newfunc = new FxGetDefaultByType(func->ArgList[0]);
			func->ArgList[0] = nullptr;
			delete func;
			return newfunc->Resolve(ctx);
		}
	}
	
	int min, max, special;
	if (func->MethodName == NAME_ACS_NamedExecuteWithResult || func->MethodName == NAME_CallACS)
	{
		special = -ACS_ExecuteWithResult;
		min = 1;
		max = 5;
	}
	else
	{
		// This alias is needed because Actor has a Teleport function.
		if (func->MethodName == NAME_TeleportSpecial) func->MethodName = NAME_Teleport;
		special = P_FindLineSpecial(func->MethodName.GetChars(), &min, &max);
	}
	if (special != 0 && min >= 0)
	{
		int paramcount = func->ArgList.Size();
		if (ctx.Function == nullptr || ctx.Class == nullptr)
		{
			ScriptPosition.Message(MSG_ERROR, "Unable to call action special %s from constant declaration", func->MethodName.GetChars());
			delete func;
			return nullptr;
		}
		else if (paramcount < min)
		{
			ScriptPosition.Message(MSG_ERROR, "Not enough parameters for '%s' (expected %d, got %d)", 
				func->MethodName.GetChars(), min, paramcount);
			delete func;
			return nullptr;
		}
		else if (paramcount > max)
		{
			ScriptPosition.Message(MSG_ERROR, "too many parameters for '%s' (expected %d, got %d)", 
				func->MethodName.GetChars(), max, paramcount);
			delete func;
			return nullptr;
		}
		FxExpression *self = (ctx.Function && (ctx.Function->Variants[0].Flags & VARF_Method) && isActor(ctx.Class)) ? new FxSelf(ScriptPosition) : (FxExpression*)new FxConstant(ScriptPosition);
		FxExpression *x = new FxActionSpecialCall(self, special, func->ArgList, ScriptPosition);
		delete func;
		return x->Resolve(ctx);
	}
	return func;
}



//==========================================================================
//
// FxActionSpecialCall
//
// If special is negative, then the first argument will be treated as a
// name for ACS_NamedExecuteWithResult.
//
//==========================================================================

FxActionSpecialCall::FxActionSpecialCall(FxExpression *self, int special, FArgumentList &args, const FScriptPosition &pos)
: FxExpression(EFX_ActionSpecialCall, pos)
{
	Self = self;
	Special = special;
	ArgList = std::move(args);
	while (ArgList.Size() < 5)
	{
		ArgList.Push(new FxConstant(0, ScriptPosition));
	}
}

//==========================================================================
//
//
//
//==========================================================================

FxActionSpecialCall::~FxActionSpecialCall()
{
	SAFE_DELETE(Self);
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
	for (unsigned i = 0; i < ArgList.Size(); i++)
	{
		ArgList[i] = ArgList[i]->Resolve(ctx);
		if (ArgList[i] == nullptr)
		{
			failed = true;
		}
		else if (Special < 0 && i == 0)
		{
			if (ArgList[i]->ValueType == TypeString)
			{
				ArgList[i] = new FxNameCast(ArgList[i]);
				ArgList[i] = ArgList[i]->Resolve(ctx);
				if (ArgList[i] == nullptr)
				{
					failed = true;
				}
			}
			else if (ArgList[i]->ValueType != TypeName)
			{
				ScriptPosition.Message(MSG_ERROR, "Name expected for parameter %d", i);
				failed = true;
			}
		}
		else if (!ArgList[i]->IsInteger())
		{
			if (ArgList[i]->ValueType->GetRegType() == REGT_FLOAT /* lax */)
			{
				ArgList[i] = new FxIntCast(ArgList[i], ctx.FromDecorate);
				ArgList[i] = ArgList[i]->Resolve(ctx);
				if (ArgList[i] == nullptr)
				{
					delete this;
					return nullptr;
				}
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
	ValueType = TypeSInt32;
	return this;
}


//==========================================================================
//
// 
//
//==========================================================================

int BuiltinCallLineSpecial(int special, AActor *activator, int arg1, int arg2, int arg3, int arg4, int arg5)
{
	return P_ExecuteSpecial(currentVMLevel , special, nullptr, activator, 0, arg1, arg2, arg3, arg4, arg5);
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, BuiltinCallLineSpecial, BuiltinCallLineSpecial)
{
	PARAM_PROLOGUE;
	PARAM_INT(special);
	PARAM_OBJECT(activator, AActor);
	PARAM_INT(arg1);
	PARAM_INT(arg2);
	PARAM_INT(arg3);
	PARAM_INT(arg4);
	PARAM_INT(arg5);

	ACTION_RETURN_INT(P_ExecuteSpecial(currentVMLevel, special, nullptr, activator, 0, arg1, arg2, arg3, arg4, arg5));
}

ExpEmit FxActionSpecialCall::Emit(VMFunctionBuilder *build)
{
	unsigned i = 0;

	// Call the BuiltinCallLineSpecial function to perform the desired special.
	static uint8_t reginfo[] = { REGT_INT, REGT_POINTER, REGT_INT, REGT_INT, REGT_INT, REGT_INT, REGT_INT };
	auto sym = FindBuiltinFunction(NAME_BuiltinCallLineSpecial);

	assert(sym);
	auto callfunc = sym->Variants[0].Implementation;

	FunctionCallEmitter emitters(callfunc);

	emitters.AddParameterIntConst(abs(Special));			// pass special number
	emitters.AddParameter(build, Self);


	for (; i < ArgList.Size(); ++i)
	{
		FxExpression *argex = ArgList[i];
		if (Special < 0 && i == 0)
		{
			assert(argex->ValueType == TypeName);
			assert(argex->isConstant());
			emitters.AddParameterIntConst(-static_cast<FxConstant *>(argex)->GetValue().GetName().GetIndex());
		}
		else
		{
			assert(argex->ValueType->GetRegType() == REGT_INT);
			if (argex->isConstant())
			{
				emitters.AddParameterIntConst(static_cast<FxConstant *>(argex)->GetValue().GetInt());
			}
			else
			{
				emitters.AddParameter(build, argex);
			}
		}
	}
	ArgList.DeleteAndClear();
	ArgList.ShrinkToFit();

	emitters.AddReturn(REGT_INT);
	return emitters.EmitCall(build);
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

FxExpression *FxClassDefaults::Resolve(FCompileContext& ctx)
{
	CHECKRESOLVED();
	SAFE_RESOLVE(obj, ctx);
	assert(obj->ValueType->isRealPointer());
	ValueType = NewPointer(obj->ValueType->toPointer()->PointedType, true);
	return this;
}

//==========================================================================
//
//
//
//==========================================================================

ExpEmit FxClassDefaults::Emit(VMFunctionBuilder *build)
{
	ExpEmit ob = obj->Emit(build);
	ob.Free(build);
	ExpEmit meta(build, REGT_POINTER);
	build->Emit(OP_CLSS, meta.RegNum, ob.RegNum);
	build->Emit(OP_LP, meta.RegNum, meta.RegNum, build->GetConstantInt(myoffsetof(PClass, Defaults)));
	return meta;

}

//==========================================================================
//
//
//==========================================================================

FxGetDefaultByType::FxGetDefaultByType(FxExpression *self)
	:FxExpression(EFX_GetDefaultByType, self->ScriptPosition)
{
	Self = self;
}

FxGetDefaultByType::~FxGetDefaultByType()
{
	SAFE_DELETE(Self);
}

FxExpression *FxGetDefaultByType::Resolve(FCompileContext &ctx)
{
	SAFE_RESOLVE(Self, ctx);
	PClass *cls = nullptr;

	if (Self->ValueType == TypeString || Self->ValueType == TypeName)
	{
		if (Self->isConstant())
		{
			cls = PClass::FindActor(static_cast<FxConstant *>(Self)->GetValue().GetName());
			if (cls == nullptr)
			{
				ScriptPosition.Message(MSG_ERROR, "GetDefaultByType() requires an actor class type, but got %s", static_cast<FxConstant *>(Self)->GetValue().GetString().GetChars());
				delete this;
				return nullptr;
			}
			Self = new FxConstant(cls, NewClassPointer(cls), ScriptPosition);
		}
		else
		{
			// this is the ugly case. We do not know what we have and cannot do proper type casting.
			// For now error out and let this case require explicit handling on the user side.
			ScriptPosition.Message(MSG_ERROR, "GetDefaultByType() requires an actor class type, but got %s", static_cast<FxConstant *>(Self)->GetValue().GetString().GetChars());
			delete this;
			return nullptr;
		}
	}
	else
	{
		auto cp = PType::toClassPointer(Self->ValueType);
		if (cp == nullptr || !cp->ClassRestriction->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			ScriptPosition.Message(MSG_ERROR, "GetDefaultByType() requires an actor class type");
			delete this;
			return nullptr;
		}
		cls = cp->ClassRestriction;
	}
	ValueType = NewPointer(cls, true);
	return this;
}

ExpEmit FxGetDefaultByType::Emit(VMFunctionBuilder *build)
{
	ExpEmit op = Self->Emit(build);
	op.Free(build);
	ExpEmit to(build, REGT_POINTER);
	if (op.Konst)
	{
		build->Emit(OP_LKP, to.RegNum, op.RegNum);
		op = to;
	}
	build->Emit(OP_LP, to.RegNum, op.RegNum, build->GetConstantInt(myoffsetof(PClass, Defaults)));
	return to;
}


//==========================================================================
//
// Symbolic state labels. 
// Conversion will not happen inside the compiler anymore because it causes
// just too many problems.
//
//==========================================================================

FxExpression *FxStateByIndex::Resolve(FCompileContext &ctx)
{
	CHECKRESOLVED();
	ABORT(ctx.Class);
	auto vclass = PType::toClass(ctx.Class);
	assert(vclass != nullptr);
	auto aclass = ValidateActor(vclass->Descriptor);

	// This expression type can only be used from actors, for everything else it has already produced a compile error.
	assert(aclass != nullptr && aclass->GetStateCount() > 0);

	if (aclass->GetStateCount() <= index)
	{
		ScriptPosition.Message(MSG_ERROR, "%s: Attempt to jump to non existing state index %d", 
			ctx.Class->TypeName.GetChars(), index);
		delete this;
		return nullptr;
	}
	int symlabel = StateLabels.AddPointer(aclass->GetStates() + index);
	FxExpression *x = new FxConstant(symlabel, ScriptPosition);
	x->ValueType = TypeStateLabel;
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
	ValueType = TypeStateLabel;
}

FxRuntimeStateIndex::~FxRuntimeStateIndex()
{
	SAFE_DELETE(Index);
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
			int symlabel = StateLabels.AddPointer(nullptr);
			auto x = new FxConstant(symlabel, ScriptPosition);
			delete this;
			x->ValueType = TypeStateLabel;
			return x;
		}
		else
		{
			auto x = new FxStateByIndex(ctx.StateIndex + index, ScriptPosition);
			delete this;
			return x->Resolve(ctx);
		}
	}
	else if (Index->ValueType->GetRegType() != REGT_INT)
	{ // Float.
		Index = new FxIntCast(Index, ctx.FromDecorate);
		SAFE_RESOLVE(Index, ctx);
	}

	auto vclass = PType::toClass(ctx.Class);
	assert(vclass != nullptr);
	auto aclass = ValidateActor(vclass->Descriptor);
	assert(aclass != nullptr && aclass->GetStateCount() > 0);

	symlabel = StateLabels.AddPointer(aclass->GetStates() + ctx.StateIndex);
	ValueType = TypeStateLabel;
	return this;
}

ExpEmit FxRuntimeStateIndex::Emit(VMFunctionBuilder *build)
{
	ExpEmit out = Index->Emit(build);
	// out = (clamp(Index, 0, 32767) << 16) | symlabel | 0x80000000;  0x80000000 is here to make it negative.
	build->Emit(OP_MAX_RK, out.RegNum, out.RegNum, build->GetConstantInt(0));
	build->Emit(OP_MIN_RK, out.RegNum, out.RegNum, build->GetConstantInt(32767));
	build->Emit(OP_SLL_RI, out.RegNum, out.RegNum, 16);
	build->Emit(OP_OR_RK, out.RegNum, out.RegNum, build->GetConstantInt(symlabel|0x80000000));
	return out;
}

//==========================================================================
//
//
//
//==========================================================================

FxMultiNameState::FxMultiNameState(const char *_statestring, const FScriptPosition &pos, PClassActor *checkclass)
	:FxExpression(EFX_MultiNameState, pos)
{
	FName scopename = NAME_None;
	FString statestring = _statestring;
	int scopeindex = statestring.IndexOf("::");

	if (scopeindex >= 0)
	{
		scopename = FName(statestring, scopeindex, false);
		statestring = statestring.Right(statestring.Len() - scopeindex - 2);
	}
	names = MakeStateNameList(statestring);
	names.Insert(0, scopename);
	scope = checkclass;
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
	int symlabel;

	auto vclass = PType::toClass(ctx.Class);
	//assert(vclass != nullptr);
	auto clstype = vclass == nullptr? nullptr : ValidateActor(vclass->Descriptor);

	if (names[0] == NAME_None)
	{
		scope = nullptr;
	}
	else if (clstype == nullptr)
	{
		// not in an actor, so any further checks are pointless.
		ScriptPosition.Message(MSG_ERROR, "'%s' is not an ancestor of '%s'", names[0].GetChars(), ctx.Class->TypeName.GetChars());
		delete this;
		return nullptr;
	}
	else if (names[0] == NAME_Super)
	{
		scope = ValidateActor(clstype->ParentClass);
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
		else if (!scope->IsAncestorOf(clstype))
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
		symlabel = StateLabels.AddPointer(destination);
	}
	else
	{
		names.Delete(0);
		symlabel = StateLabels.AddNames(names);
	}
	FxExpression *x = new FxConstant(symlabel, ScriptPosition);
	x->ValueType = TypeStateLabel;
	delete this;
	return x;
}


//==========================================================================
//
// The CVAR is for finding places where thinkers are created.
// Those will require code changes in ZScript 4.0.
//
//==========================================================================
CVAR(Bool, vm_warnthinkercreation, false, 0)

static DObject *BuiltinNewDoom(PClass *cls, int outerside, int backwardscompatible)
{
	if (cls == nullptr)
	{
		ThrowAbortException(X_OTHER, "New without a class");
		return nullptr;
	}
	if (cls->ConstructNative == nullptr)
	{
		ThrowAbortException(X_OTHER, "Class %s requires native construction", cls->TypeName.GetChars());
		return nullptr;
	}
	if (cls->bAbstract)
	{
		ThrowAbortException(X_OTHER, "Cannot instantiate abstract class %s", cls->TypeName.GetChars());
		return nullptr;
	}
	// Creating actors here must be outright prohibited,
	if (cls->IsDescendantOf(NAME_Actor))
	{
		ThrowAbortException(X_OTHER, "Cannot create actors with 'new'");
		return nullptr;
	}
	if ((vm_warnthinkercreation || !backwardscompatible) && cls->IsDescendantOf(NAME_Thinker))
	{
		// This must output a diagnostic warning
		Printf("Using 'new' to create thinkers is deprecated.");
	}
	// [ZZ] validate readonly and between scope construction
	if (outerside) FScopeBarrier::ValidateNew(cls, outerside - 1);
	DObject *object;
	if (!cls->IsDescendantOf(NAME_Thinker))
	{
		object = cls->CreateNew();
	}
	else
	{
		object = currentVMLevel->CreateThinker(cls);
	}
	return object;
}

DEFINE_ACTION_FUNCTION_NATIVE(DObject, BuiltinNewDoom, BuiltinNewDoom)
{
	PARAM_PROLOGUE;
	PARAM_CLASS(cls, DObject);
	PARAM_INT(outerside);
	PARAM_INT(compatible);
	ACTION_RETURN_OBJECT(BuiltinNewDoom(cls, outerside, compatible));
}


void SetDoomCompileEnvironment()
{
	compileEnvironment.SpecialTypeCast = CustomTypeCast;
	compileEnvironment.CheckForCustomAddition = CheckForCustomAddition;
	compileEnvironment.CheckSpecialIdentifier = CheckForDefault;
	compileEnvironment.ResolveSpecialIdentifier = ResolveForDefault;
	compileEnvironment.CheckSpecialMember = CheckForMemberDefault;
	compileEnvironment.ResolveSpecialFunction = AJumpProcessing;
	compileEnvironment.CheckCustomGlobalFunctions = ResolveGlobalCustomFunction;
	compileEnvironment.CustomBuiltinNew = "BuiltinNewDoom";
}
