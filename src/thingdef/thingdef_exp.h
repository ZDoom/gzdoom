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

class VMFunctionBuilder;

//==========================================================================
//
//
//
//==========================================================================

struct FCompileContext
{
	PClassActor *cls;

	FCompileContext(PClassActor *_cls = NULL)
	{
		cls = _cls;
	}

	PSymbol *FindInClass(FName identifier)
	{
		return cls ? cls->Symbols.FindSymbol(identifier, true) : NULL;
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

	ExpVal()
	{
		Type = VAL_Int;
		Int = 0;
	}

	~ExpVal()
	{
		if (Type == VAL_String)
		{
			((FString *)&pointer)->~FString();
		}
	}

	ExpVal(const FString &str)
	{
		Type = VAL_String;
		::new(&pointer) FString(str);
	}

	ExpVal(const ExpVal &o)
	{
		Type = o.Type;
		if (o.Type == VAL_String)
		{
			::new(&pointer) FString(*(FString *)&o.pointer);
		}
		else
		{
			memcpy(&Float, &o.Float, 8);
		}
	}

	ExpVal &operator=(const ExpVal &o)
	{
		if (Type == VAL_String)
		{
			((FString *)&pointer)->~FString();
		}
		Type = o.Type;
		if (o.Type == VAL_String)
		{
			::new(&pointer) FString(*(FString *)&o.pointer);
		}
		else
		{
			memcpy(&Float, &o.Float, 8);
		}
		return *this;
	}

	int GetInt() const
	{
		return Type == VAL_Int? Int : Type == VAL_Float? int(Float) : 0;
	}

	double GetFloat() const
	{
		return Type == VAL_Int? double(Int) : Type == VAL_Float? Float : 0;
	}

	const FString GetString() const
	{
		return Type == VAL_String ? *(FString *)&pointer : Type == VAL_Name ? FString(FName(ENamedName(Int)).GetChars()) : "";
	}

	bool GetBool() const
	{
		return (Type == VAL_Int || Type == VAL_Sound) ? !!Int : Type == VAL_Float? Float!=0. : false;
	}
	
	FName GetName() const
	{
		return Type == VAL_Name? ENamedName(Int) : NAME_None;
	}
};

struct ExpEmit
{
	ExpEmit() : RegNum(0), RegType(REGT_NIL), Konst(false), Fixed(false) {}
	ExpEmit(int reg, int type) : RegNum(reg), RegType(type), Konst(false), Fixed(false) {}
	ExpEmit(int reg, int type, bool konst)  : RegNum(reg), RegType(type), Konst(konst), Fixed(false) {}
	ExpEmit(VMFunctionBuilder *build, int type);
	void Free(VMFunctionBuilder *build);
	void Reuse(VMFunctionBuilder *build);

	BYTE RegNum, RegType, Konst:1, Fixed:1;
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
	
	virtual bool isConstant() const;
	virtual void RequestAddress();

	virtual ExpEmit Emit(VMFunctionBuilder *build);

	FScriptPosition ScriptPosition;
	FExpressionType ValueType;

	bool isresolved;
};

class FxParameter : public FxExpression
{
	FxExpression *Operand;

public:
	FxParameter(FxExpression*);
	~FxParameter();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
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

	FxConstant(const FString &str, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = VAL_String;
		value = ExpVal(str);
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

	ExpVal GetValue() const
	{
		return value;
	}
	ExpEmit Emit(VMFunctionBuilder *build);
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

	ExpEmit Emit(VMFunctionBuilder *build);
};

class FxFloatCast : public FxExpression
{
	FxExpression *basex;

public:

	FxFloatCast(FxExpression *x);
	~FxFloatCast();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	void Promote(FCompileContext &ctx);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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

	ExpEmit Emit(VMFunctionBuilder *build);
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

	ExpEmit Emit(VMFunctionBuilder *build);
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

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxRandom : public FxExpression
{
protected:
	FRandom *rng;
	FxExpression *min, *max;

public:

	FxRandom(FRandom *, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos);
	~FxRandom();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxRandomPick : public FxExpression
{
protected:
	FRandom *rng;
	TDeletingArray<FxExpression*> choices;

public:

	FxRandomPick(FRandom *, TArray<FxExpression*> &expr, bool floaty, const FScriptPosition &pos);
	~FxRandomPick();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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

	ExpEmit Emit(VMFunctionBuilder *build);
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
	PField *membervar;
	bool AddressRequested;

	FxClassMember(FxExpression*, PField*, const FScriptPosition&);
	~FxClassMember();
	FxExpression *Resolve(FCompileContext&);
	void RequestAddress();
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxDamage
//
//==========================================================================

class FxDamage : public FxExpression
{
public:
	FxDamage(const FScriptPosition&);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxGlobalFunctionCall
//
//==========================================================================

class FxGlobalFunctionCall : public FxExpression
{
	FName Name;
	FArgumentList *ArgList;

public:

	FxGlobalFunctionCall(FName fname, FArgumentList *args, const FScriptPosition &pos);
	~FxGlobalFunctionCall();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
// FxTailable
//
// An expression that can produce a tail call
//
//==========================================================================

class FxTailable : public FxExpression
{
public:
	FxTailable(const FScriptPosition &pos) : FxExpression(pos) {}
	virtual ExpEmit Emit(VMFunctionBuilder *build, bool tailcall) = 0;
	ExpEmit Emit(VMFunctionBuilder *build);
	virtual VMFunction *GetDirectFunction();
};

//==========================================================================
//
// FxVMFunctionCall
//
//==========================================================================

class FxVMFunctionCall : public FxTailable
{
	PFunction *Function;
	FArgumentList *ArgList;
	PType *ReturnType;

public:
	FxVMFunctionCall(PFunction *func, FArgumentList *args, const FScriptPosition &pos);
	~FxVMFunctionCall();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build, bool tailcall);
	unsigned GetArgCount() { return ArgList == NULL ? 0 : ArgList->Size(); }
	VMFunction *GetVMFunction() { return Function->Variants[0].Implementation; }
	VMFunction *GetDirectFunction();
};

//==========================================================================
//
// FxSequence
//
//==========================================================================

class FxSequence : public FxTailable
{
	TDeletingArray<FxTailable *> Expressions;

public:
	FxSequence(const FScriptPosition &pos) : FxTailable(pos) {}
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build, bool tailcall);
	void Add(FxTailable *expr) { if (expr != NULL) Expressions.Push(expr); }
	VMFunction *GetDirectFunction();
};

//==========================================================================
//
// FxIfStatement
//
//==========================================================================

class FxIfStatement : public FxTailable
{
	FxExpression *Condition;
	FxTailable *WhenTrue;
	FxTailable *WhenFalse;

public:
	FxIfStatement(FxExpression *cond, FxTailable *true_part, FxTailable *false_part, const FScriptPosition &pos);
	~FxIfStatement();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build, bool tailcall);
};

//==========================================================================
//
// FxReturnStatement
//
//==========================================================================

class FxReturnStatement : public FxTailable
{
public:
	FxReturnStatement(const FScriptPosition &pos);
	ExpEmit Emit(VMFunctionBuilder *build, bool tailcall);
};

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
	ExpEmit Emit(VMFunctionBuilder *build);
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
	PClassActor *scope;
	TArray<FName> names;
public:

	FxMultiNameState(const char *statestring, const FScriptPosition &pos);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxDamageValue : public FxExpression
{
	FxExpression *val;
	bool Calculated;
	VMScriptFunction *MyFunction;

public:

	FxDamageValue(FxExpression *v, bool calc);
	~FxDamageValue();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
	VMScriptFunction *GetFunction() const { return MyFunction; }
	void SetFunction(VMScriptFunction *func) { MyFunction = func; }
};


FxExpression *ParseExpression (FScanner &sc, PClassActor *cls);


#endif
