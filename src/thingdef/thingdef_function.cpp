/*
** thingdef_function.cpp
**
** Expression function evaluation.
**
**---------------------------------------------------------------------------
** Copyright 2012 David Hill
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
**    the Free Software Foundation; either version 3 of the License, or (at
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

#include <math.h>

#include "tables.h"
#include "tarray.h"
#include "thingdef.h"
#include "thingdef_exp.h"
#include "actor.h"
#include "actorptrselect.h"

static TMap<FName, FxGlobalFunctionCall::Creator> CreatorMap;

//==========================================================================
//
// FxGlobalFunctionCall::AddCreator
//
//==========================================================================

void FxGlobalFunctionCall::AddCreator(FName methodname, Creator creator)
{
	CreatorMap.Insert(methodname, creator);
}


//==========================================================================
//
// FxGlobalFunctionCall::ResolveArgs
//
// Handles common Resolve processing of args.
//
//==========================================================================

FxExpression *FxGlobalFunctionCall::ResolveArgs(FCompileContext &ctx, unsigned min, unsigned max, bool numeric)
{
	unsigned i = ArgList ? ArgList->Size() : 0;

	if (i < min || i > max)
	{
		ScriptPosition.Message(MSG_ERROR, "%s expects %u to %u parameters", Name.GetChars(), min, max);
		delete this;
		return NULL;
	}

	while (i--)
	{
		if (!((*ArgList)[i] = (*ArgList)[i]->Resolve(ctx)))
		{
			delete this;
			return NULL;
		}

		if (numeric && !(*ArgList)[i]->ValueType.isNumeric())
		{
			ScriptPosition.Message(MSG_ERROR, "numeric value expected for parameter");
			delete this;
			return NULL;
		}
	}

	return this;
}

//==========================================================================
//
// FxGlobalFunctionCall::StaticCreate
//
//==========================================================================

FxExpression *FxGlobalFunctionCall::StaticCreate
(FName methodname, FArgumentList *args, const FScriptPosition &pos)
{
	Creator *creator = CreatorMap.CheckKey(methodname);

	if (!creator)
	{
		pos.Message(MSG_ERROR, "Call to unknown function '%s'", methodname.GetChars());
		return NULL;
	}

	return (*creator)(methodname, args, pos);
}

//==========================================================================
//
// Function: cos/sin
//
//==========================================================================

class FxGlobalFunctionCall_Cos : public FxGlobalFunctionCall
{
public:
	GLOBALFUNCTION_DEFINE(Cos);

	FxExpression *Resolve(FCompileContext& ctx)
	{
		CHECKRESOLVED();

		ValueType = VAL_Float;
		return ResolveArgs(ctx, 1, 1, true);
	}

	ExpVal EvalExpression(AActor *self)
	{
		double v = (*ArgList)[0]->EvalExpression(self).GetFloat();
		ExpVal ret;
		ret.Type = VAL_Float;

		// shall we use the CRT's sin and cos functions?
		angle_t angle = angle_t(v * ANGLE_90/90.);
		if (Name == NAME_Sin) ret.Float = FIXED2DBL (finesine[angle>>ANGLETOFINESHIFT]);
		else ret.Float = FIXED2DBL (finecosine[angle>>ANGLETOFINESHIFT]);
		return ret;
	}
};

GLOBALFUNCTION_ADDER(Cos);
GLOBALFUNCTION_ADDER_NAMED(Cos, Sin);

//==========================================================================
//
// Function: sqrt
//
//==========================================================================

class FxGlobalFunctionCall_Sqrt : public FxGlobalFunctionCall
{
public:
	GLOBALFUNCTION_DEFINE(Sqrt);

	FxExpression *Resolve(FCompileContext& ctx)
	{
		CHECKRESOLVED();

		if (!ResolveArgs(ctx, 1, 1, true))
			return NULL;

		ValueType = VAL_Float;
		return this;
	}

	ExpVal EvalExpression(AActor *self)
	{
		ExpVal ret;
		ret.Type = VAL_Float;
		ret.Float = sqrt((*ArgList)[0]->EvalExpression(self).GetFloat());
		return ret;
	}
};

GLOBALFUNCTION_ADDER(Sqrt);

//==========================================================================
//
// Function: checkclass
//
//==========================================================================

class FxGlobalFunctionCall_CheckClass : public FxGlobalFunctionCall
{
	public:
		GLOBALFUNCTION_DEFINE(CheckClass);
		
		FxExpression *Resolve(FCompileContext& ctx)
		{
			CHECKRESOLVED();
			
			if (!ResolveArgs(ctx, 1, 3, false))
			return NULL;
			
			for (int i = ArgList->Size(); i > 1;)
			{
				if (!(*ArgList)[--i]->ValueType.isNumeric())
				{
					ScriptPosition.Message(MSG_ERROR, "numeric value expected for parameter");
					delete this;
					return NULL;
				}
			}
			
			switch ((*ArgList)[0]->ValueType.Type)
			{
				case VAL_Class: case VAL_Name:break;
				default:
					ScriptPosition.Message(MSG_ERROR, "actor class expected for parameter");
				delete this;
				return NULL;
			}
			
			ValueType = VAL_Float;
			return this;
		}
		
		ExpVal EvalExpression(AActor *self)
		{
			ExpVal ret;
			ret.Type = VAL_Int;
			
			const PClass  * checkclass;
			{
				ExpVal v = (*ArgList)[0]->EvalExpression(self);
				checkclass = v.GetClass();
				if (!checkclass)
				{
					checkclass = PClass::FindClass(v.GetName());
					if (!checkclass) { ret.Int = 0; return ret; }
				}
			}
			
			bool match_superclass = false;
			int pick_pointer = AAPTR_DEFAULT;
			
			switch (ArgList->Size())
			{
				case 3: match_superclass = (*ArgList)[2]->EvalExpression(self).GetBool();
				case 2: pick_pointer = (*ArgList)[1]->EvalExpression(self).GetInt();
			}
			
			self = COPY_AAPTR(self, pick_pointer);
			if (!self){ ret.Int = 0; return ret; }
				ret.Int = match_superclass ? checkclass->IsAncestorOf(self->GetClass()) : checkclass == self->GetClass();
			return ret;
		}
	};

GLOBALFUNCTION_ADDER(CheckClass);

//==========================================================================
//
// Function: ispointerequal
//
//==========================================================================

class FxGlobalFunctionCall_IsPointerEqual : public FxGlobalFunctionCall
{
	public:
		GLOBALFUNCTION_DEFINE(IsPointerEqual);
		
		FxExpression *Resolve(FCompileContext& ctx)
		{
			CHECKRESOLVED();
			
			if (!ResolveArgs(ctx, 2, 2, true))
				return NULL;
			
			ValueType = VAL_Int;
			return this;
		}
		
		ExpVal EvalExpression(AActor *self)
		{
			ExpVal ret;
			ret.Type = VAL_Int;
			ret.Int = COPY_AAPTR(self, (*ArgList)[0]->EvalExpression(self).GetInt()) == COPY_AAPTR(self, (*ArgList)[1]->EvalExpression(self).GetInt());
			return ret;
		}
};

GLOBALFUNCTION_ADDER(IsPointerEqual);