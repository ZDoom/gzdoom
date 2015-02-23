#ifndef THINGDEF_EXP_H
#define THINGDEF_EXP_H

/*
** thingdef_exp.h
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

#include "m_random.h"

#define CHECKRESOLVED() if (isresolved) return this; isresolved=true;
#define SAFE_DELETE(p) if (p!=NULL) { delete p; p=NULL; }
#define RESOLVE(p,c) if (p!=NULL) p = p->Resolve(c)
#define ABORT(p) if (!(p)) { delete this; return NULL; }
#define SAFE_RESOLVE(p,c) RESOLVE(p,c); ABORT(p) 

extern PSymbolTable		 GlobalSymbols;

//==========================================================================
//
//
//
//==========================================================================

struct FCompileContext
{
	const PClass *cls;
	bool lax;
	bool isconst;

	FCompileContext(const PClass *_cls = NULL, bool _lax = false, bool _isconst = false)
	{
		cls = _cls;
		lax = _lax;
		isconst = _isconst;
	}

	PSymbol *FindInClass(FName identifier)
	{
		return cls? cls->Symbols.FindSymbol(identifier, true) : NULL;
	}
	PSymbol *FindGlobal(FName identifier)
	{
		return GlobalSymbols.FindSymbol(identifier, true);
	}
};

//==========================================================================
//
//
//
//==========================================================================

struct ExpVal
{
	ExpValType Type;
	union
	{
		int Int;
		double Float;
		void *pointer;
	};

	int GetInt() const
	{
		return Type == VAL_Int? Int : Type == VAL_Float? int(Float) : 0;
	}

	double GetFloat() const
	{
		return Type == VAL_Int? double(Int) : Type == VAL_Float? Float : 0;
	}

	bool GetBool() const
	{
		return (Type == VAL_Int || Type == VAL_Sound) ? !!Int : Type == VAL_Float? Float!=0. : false;
	}
	
	template<class T> T *GetPointer() const
	{
		return Type == VAL_Object || Type == VAL_Pointer? (T*)pointer : NULL;
	}

	FSoundID GetSoundID() const
	{
		return Type == VAL_Sound? Int : 0;
	}

	int GetColor() const
	{
		return Type == VAL_Color? Int : 0;
	}

	FName GetName() const
	{
		return Type == VAL_Name? ENamedName(Int) : NAME_None;
	}
	
	FState *GetState() const
	{
		return Type == VAL_State? (FState*)pointer : NULL;
	}

	const PClass *GetClass() const
	{
		return Type == VAL_Class? (const PClass *)pointer : NULL;
	}

};


//==========================================================================
//
//
//
//==========================================================================

class FxExpression
{
protected:
	FxExpression(const FScriptPosition &pos)
	: ScriptPosition(pos)
	{
		isresolved = false;
		ScriptPosition = pos;
	}
public:
	virtual ~FxExpression() {}
	virtual FxExpression *Resolve(FCompileContext &ctx);
	FxExpression *ResolveAsBoolean(FCompileContext &ctx);
	
	virtual ExpVal EvalExpression (AActor *self);
	virtual bool isConstant() const;
	virtual void RequestAddress();

	FScriptPosition ScriptPosition;
	FExpressionType ValueType;

	bool isresolved;
};

//==========================================================================
//
//	FxIdentifier
//
//==========================================================================

class FxIdentifier : public FxExpression
{
	FName Identifier;

public:
	FxIdentifier(FName i, const FScriptPosition &p);
	FxExpression *Resolve(FCompileContext&);
};


//==========================================================================
//
//	FxDotIdentifier
//
//==========================================================================

class FxDotIdentifier : public FxExpression
{
	FxExpression *container;
	FName Identifier;

public:
	FxDotIdentifier(FxExpression*, FName, const FScriptPosition &);
	~FxDotIdentifier();
	FxExpression *Resolve(FCompileContext&);
};

//==========================================================================
//
//	FxClassDefaults
//
//==========================================================================

class FxClassDefaults : public FxExpression
{
	FxExpression *obj;

public:
	FxClassDefaults(FxExpression*, const FScriptPosition &);
	~FxClassDefaults();
	FxExpression *Resolve(FCompileContext&);
	bool IsDefaultObject() const;
};

//==========================================================================
//
//	FxConstant
//
//==========================================================================

class FxConstant : public FxExpression
{
	ExpVal value;

public:
	FxConstant(int val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = VAL_Int;
		value.Int = val;
		isresolved = true;
	}

	FxConstant(double val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = VAL_Float;
		value.Float = val;
		isresolved = true;
	}

	FxConstant(FSoundID val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = VAL_Sound;
		value.Int = val;
		isresolved = true;
	}

	FxConstant(FName val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = VAL_Name;
		value.Int = val;
		isresolved = true;
	}

	FxConstant(ExpVal cv, const FScriptPosition &pos) : FxExpression(pos)
	{
		value = cv;
		ValueType = cv.Type;
		isresolved = true;
	}
	
	FxConstant(const PClass *val, const FScriptPosition &pos) : FxExpression(pos)
	{
		value.pointer = (void*)val;
		ValueType = val;
		value.Type = VAL_Class;
		isresolved = true;
	}

	FxConstant(FState *state, const FScriptPosition &pos) : FxExpression(pos)
	{
		value.pointer = state;
		ValueType = value.Type = VAL_State;
		isresolved = true;
	}
	
	static FxExpression *MakeConstant(PSymbol *sym, const FScriptPosition &pos);

	bool isConstant() const
	{
		return true;
	}
	ExpVal EvalExpression (AActor *self);
};


//==========================================================================
//
//
//
//==========================================================================

class FxIntCast : public FxExpression
{
	FxExpression *basex;

public:

	FxIntCast(FxExpression *x);
	~FxIntCast();
	FxExpression *Resolve(FCompileContext&);

	ExpVal EvalExpression (AActor *self);
};


//==========================================================================
//
//
//
//==========================================================================

class FxFloatCast : public FxExpression
{
	FxExpression *basex;

public:

	FxFloatCast(FxExpression *x);
	~FxFloatCast();
	FxExpression *Resolve(FCompileContext&);

	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxSign
//
//==========================================================================

class FxPlusSign : public FxExpression
{
	FxExpression *Operand;

public:
	FxPlusSign(FxExpression*);
	~FxPlusSign();
	FxExpression *Resolve(FCompileContext&);
};

//==========================================================================
//
//	FxSign
//
//==========================================================================

class FxMinusSign : public FxExpression
{
	FxExpression *Operand;

public:
	FxMinusSign(FxExpression*);
	~FxMinusSign();
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxUnaryNot
//
//==========================================================================

class FxUnaryNotBitwise : public FxExpression
{
	FxExpression *Operand;

public:
	FxUnaryNotBitwise(FxExpression*);
	~FxUnaryNotBitwise();
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxUnaryNot
//
//==========================================================================

class FxUnaryNotBoolean : public FxExpression
{
	FxExpression *Operand;

public:
	FxUnaryNotBoolean(FxExpression*);
	~FxUnaryNotBoolean();
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxBinary : public FxExpression
{
public:
	int				Operator;
	FxExpression		*left;
	FxExpression		*right;

	FxBinary(int, FxExpression*, FxExpression*);
	~FxBinary();
	bool ResolveLR(FCompileContext& ctx, bool castnumeric);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxAddSub : public FxBinary
{
public:

	FxAddSub(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxMulDiv : public FxBinary
{
public:

	FxMulDiv(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxCompareRel : public FxBinary
{
public:

	FxCompareRel(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxCompareEq : public FxBinary
{
public:

	FxCompareEq(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxBinaryInt : public FxBinary
{
public:

	FxBinaryInt(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxBinaryLogical
//
//==========================================================================

class FxBinaryLogical : public FxExpression
{
public:
	int				Operator;
	FxExpression		*left;
	FxExpression		*right;

	FxBinaryLogical(int, FxExpression*, FxExpression*);
	~FxBinaryLogical();
	FxExpression *Resolve(FCompileContext&);

	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxConditional
//
//==========================================================================

class FxConditional : public FxExpression
{
public:
	FxExpression *condition;
	FxExpression *truex;
	FxExpression *falsex;

	FxConditional(FxExpression*, FxExpression*, FxExpression*);
	~FxConditional();
	FxExpression *Resolve(FCompileContext&);

	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//
//
//==========================================================================

class FxAbs : public FxExpression
{
	FxExpression *val;

public:

	FxAbs(FxExpression *v);
	~FxAbs();
	FxExpression *Resolve(FCompileContext&);

	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//
//
//==========================================================================

class FxRandom : public FxExpression
{
protected:
	FRandom * rng;
	FxExpression *min, *max;

public:

	FxRandom(FRandom *, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos);
	~FxRandom();
	FxExpression *Resolve(FCompileContext&);

	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//
//
//==========================================================================

class FxRandomPick : public FxExpression
{
protected:
	FRandom * rng;
	TDeletingArray<FxExpression*> min;

public:

	FxRandomPick(FRandom *, TArray<FxExpression*> mi, bool floaty, const FScriptPosition &pos);
	~FxRandomPick();
	FxExpression *Resolve(FCompileContext&);

	ExpVal EvalExpression(AActor *self);
};

//==========================================================================
//
//
//
//==========================================================================

class FxFRandom : public FxRandom
{
public:
	FxFRandom(FRandom *, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//
//
//==========================================================================

class FxRandom2 : public FxExpression
{
	FRandom * rng;
	FxExpression *mask;

public:

	FxRandom2(FRandom *, FxExpression *m, const FScriptPosition &pos);
	~FxRandom2();
	FxExpression *Resolve(FCompileContext&);

	ExpVal EvalExpression (AActor *self);
};


//==========================================================================
//
//	FxGlobalVariable
//
//==========================================================================

class FxGlobalVariable : public FxExpression
{
public:
	PSymbolVariable *var;
	bool AddressRequested;

	FxGlobalVariable(PSymbolVariable*, const FScriptPosition&);
	FxExpression *Resolve(FCompileContext&);
	void RequestAddress();
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxClassMember
//
//==========================================================================

class FxClassMember : public FxExpression
{
public:
	FxExpression *classx;
	PSymbolVariable *membervar;
	bool AddressRequested;

	FxClassMember(FxExpression*, PSymbolVariable*, const FScriptPosition&);
	~FxClassMember();
	FxExpression *Resolve(FCompileContext&);
	void RequestAddress();
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxSelf
//
//==========================================================================

class FxSelf : public FxExpression
{
public:
	FxSelf(const FScriptPosition&);
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxArrayElement
//
//==========================================================================

class FxArrayElement : public FxExpression
{
public:
	FxExpression *Array;
	FxExpression *index;
	//bool AddressRequested;

	FxArrayElement(FxExpression*, FxExpression*);
	~FxArrayElement();
	FxExpression *Resolve(FCompileContext&);
	//void RequestAddress();
	ExpVal EvalExpression (AActor *self);
};


//==========================================================================
//
//	FxFunctionCall
//
//==========================================================================

typedef TDeletingArray<FxExpression*> FArgumentList;

class FxFunctionCall : public FxExpression
{
	FxExpression *Self;
	FName MethodName;
	FArgumentList *ArgList;

public:

	FxFunctionCall(FxExpression *self, FName methodname, FArgumentList *args, const FScriptPosition &pos);
	~FxFunctionCall();
	FxExpression *Resolve(FCompileContext&);
};


//==========================================================================
//
//	FxActionSpecialCall
//
//==========================================================================

class FxActionSpecialCall : public FxExpression
{
	FxExpression *Self;
	int Special;
	FArgumentList *ArgList;

public:

	FxActionSpecialCall(FxExpression *self, int special, FArgumentList *args, const FScriptPosition &pos);
	~FxActionSpecialCall();
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
//	FxGlobalFunctionCall
//
//==========================================================================

class FxGlobalFunctionCall : public FxExpression
{
public:
	typedef FxExpression *(*Creator)(FName, FArgumentList *, const FScriptPosition &);
	struct CreatorAdder
	{
		CreatorAdder(FName methodname, Creator creator)
		{
			FxGlobalFunctionCall::AddCreator(methodname, creator);
		}
	};

	static void AddCreator(FName methodname, Creator creator);
	static FxExpression *StaticCreate(FName methodname, FArgumentList *args, const FScriptPosition &pos);

protected:
	FName Name;
	FArgumentList *ArgList;

	FxGlobalFunctionCall(FName fname, FArgumentList *args, const FScriptPosition &pos);
	~FxGlobalFunctionCall();

	FxExpression *ResolveArgs(FCompileContext &, unsigned min, unsigned max, bool numeric);
};

#define GLOBALFUNCTION_DEFINE(CLASS) \
FxGlobalFunctionCall_##CLASS(FName methodname, FArgumentList *args, const FScriptPosition &pos) \
: FxGlobalFunctionCall(methodname, args, pos) {} \
static FxExpression *StaticCreate(FName methodname, FArgumentList *args, const FScriptPosition &pos) \
	{return new FxGlobalFunctionCall_##CLASS(methodname, args, pos);}

#define GLOBALFUNCTION_ADDER(CLASS) GLOBALFUNCTION_ADDER_NAMED(CLASS, CLASS)

#define GLOBALFUNCTION_ADDER_NAMED(CLASS,NAME) \
static FxGlobalFunctionCall::CreatorAdder FxGlobalFunctionCall_##NAME##Adder \
(NAME_##NAME, FxGlobalFunctionCall_##CLASS::StaticCreate)


//==========================================================================
//
//
//
//==========================================================================

class FxClassTypeCast : public FxExpression
{
	const PClass *desttype;
	FxExpression *basex;

public:

	FxClassTypeCast(const PClass *dtype, FxExpression *x);
	~FxClassTypeCast();
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};

//==========================================================================
//
// Only used to resolve the old jump by index feature of DECORATE
//
//==========================================================================

class FxStateByIndex : public FxExpression
{
	int index;

public:

	FxStateByIndex(int i, const FScriptPosition &pos) : FxExpression(pos)
	{
		index = i;
	}
	FxExpression *Resolve(FCompileContext&);
};

//==========================================================================
//
//
//
//==========================================================================

class FxMultiNameState : public FxExpression
{
	const PClass *scope;
	TArray<FName> names;
public:

	FxMultiNameState(const char *statestring, const FScriptPosition &pos);
	FxExpression *Resolve(FCompileContext&);
	ExpVal EvalExpression (AActor *self);
};



FxExpression *ParseExpression (FScanner &sc, PClass *cls);


#endif
