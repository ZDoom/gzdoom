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
#include "sc_man.h"
#include "s_soundinternal.h"
#include "vmbuilder.h"
#include "scopebarrier.h"
#include "types.h"
#include "vmintern.h"
#include "c_cvars.h"

struct FState; // needed for FxConstant. Maybe move the state constructor to a subclass later?

#define CHECKRESOLVED() if (isresolved) return this; isresolved=true;
#define SAFE_DELETE(p) if (p!=NULL) { delete p; p=NULL; }
#define RESOLVE(p,c) if (p!=NULL) p = p->Resolve(c)
#define ABORT(p) if (!(p)) { delete this; return NULL; }
#define SAFE_RESOLVE(p,c) RESOLVE(p,c); ABORT(p) 
#define SAFE_RESOLVE_OPT(p,c) if (p!=NULL) { SAFE_RESOLVE(p,c) }

class VMFunctionBuilder;
class FxJumpStatement;

extern FMemArena FxAlloc;

//==========================================================================
//
//
//
//==========================================================================
struct FScriptPosition;
class FxLoopStatement;
class FxCompoundStatement;
class FxLocalVariableDeclaration;
typedef TDeletingArray<FxExpression*> FArgumentList;

struct FCompileContext
{
	FxExpression *ControlStmt = nullptr;
	FxLoopStatement *Loop = nullptr;
	FxCompoundStatement *Block = nullptr;
	PPrototype *ReturnProto;
	PFunction *Function;	// The function that is currently being compiled (or nullptr for constant evaluation.)
	PContainerType *Class;		// The type of the owning class.
	bool FromDecorate;		// DECORATE must silence some warnings and demote some errors.
	int StateIndex;			// index in actor's state table for anonymous functions, otherwise -1 (not used by DECORATE which pre-resolves state indices)
	int StateCount;			// amount of states an anoymous function is being used on (must be 1 for state indices to be allowed.)
	int Lump;
	bool Unsafe = false;
	TDeletingArray<FxLocalVariableDeclaration *> FunctionArgs;
	PNamespace *CurGlobals;
	VersionInfo Version;
	FString VersionString;

	FCompileContext(PNamespace *spc, PFunction *func, PPrototype *ret, bool fromdecorate, int stateindex, int statecount, int lump, const VersionInfo &ver);
	FCompileContext(PNamespace *spc, PContainerType *cls, bool fromdecorate, const VersionInfo& ver);	// only to be used to resolve constants!

	PSymbol *FindInClass(FName identifier, PSymbolTable *&symt);
	PSymbol *FindInSelfClass(FName identifier, PSymbolTable *&symt);
	PSymbol *FindGlobal(FName identifier);

	void HandleJumps(int token, FxExpression *handler);
	void CheckReturn(PPrototype *proto, FScriptPosition &pos);
	bool CheckWritable(int flags);
	FxLocalVariableDeclaration *FindLocalVariable(FName name);
};

//==========================================================================
//
//
//
//==========================================================================

struct ExpVal
{
	PType *Type;
	union
	{
		int Int;
		double Float;
		void *pointer;
	};

	ExpVal()
	{
		Type = TypeSInt32;
		Int = 0;
	}

	~ExpVal()
	{
		if (Type == TypeString)
		{
			((FString *)&pointer)->~FString();
		}
	}

	ExpVal(const FString &str)
	{
		Type = TypeString;
		::new(&pointer) FString(str);
	}

	ExpVal(const ExpVal &o)
	{
		Type = o.Type;
		if (o.Type == TypeString)
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
		if (Type == TypeString)
		{
			((FString *)&pointer)->~FString();
		}
		Type = o.Type;
		if (o.Type == TypeString)
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
		int regtype = Type->GetRegType();
		return regtype == REGT_INT ? Int : regtype == REGT_FLOAT ? int(Float) : 0;
	}

	unsigned GetUInt() const
	{
		int regtype = Type->GetRegType();
		return regtype == REGT_INT ? unsigned(Int) : regtype == REGT_FLOAT ? unsigned(Float) : 0;
	}

	double GetFloat() const
	{
		int regtype = Type->GetRegType();
		return regtype == REGT_INT ? (Type == TypeUInt32? double(unsigned(Int)) : double(Int)) : regtype == REGT_FLOAT ? Float : 0;
	}

	void *GetPointer() const
	{
		int regtype = Type->GetRegType();
		return regtype == REGT_POINTER ? pointer : nullptr;
	}

	const FString GetString() const
	{
		return Type == TypeString ? *(FString *)&pointer : Type == TypeName ? FString(FName(ENamedName(Int)).GetChars()) : FString();
	}

	bool GetBool() const
	{
		int regtype = Type->GetRegType();
		return regtype == REGT_INT ? !!Int : regtype == REGT_FLOAT ? Float!=0. : false;
	}

	FName GetName() const
	{
		if (Type == TypeString)
		{
			if (((FString *)&pointer)->Len() == 0) return NAME_None;
			return FName(*(FString *)&pointer);
		}
		return Type == TypeName ? ENamedName(Int) : NAME_None;
	}
};

enum EFxType
{
	EFX_Expression,
	EFX_Identifier,
	EFX_MemberIdentifier,
	EFX_ClassDefaults,
	EFX_Constant,
	EFX_BoolCast,
	EFX_IntCast,
	EFX_FloatCast,
	EFX_NameCast,
	EFX_StringCast,
	EFX_ColorCast,
	EFX_SoundCast,
	EFX_TypeCast,
	EFX_PlusSign,
	EFX_MinusSign,
	EFX_UnaryNotBitwise,
	EFX_UnaryNotBoolean,
	EFX_SizeAlign,
	EFX_PreIncrDecr,
	EFX_PostIncrDecr,
	EFX_Assign,
	EFX_AssignSelf,
	EFX_Binary,	// one token fits all, the operator is enough to distinguish them.
	EFX_BinaryLogical,
	EFX_DotCross,
	EFX_Conditional,
	EFX_Abs,
	EFX_ATan2,
	EFX_ATan2Vec,
	EFX_New,
	EFX_MinMax,
	EFX_Random,
	EFX_RandomPick,
	EFX_FRandom,
	EFX_Random2,
	EFX_ClassMember,
	EFX_StructMember,
	EFX_LocalVariable,
	EFX_Self,
	EFX_ArrayElement,
	EFX_FunctionCall,
	EFX_MemberFunctionCall,
	EFX_ActionSpecialCall,
	EFX_FlopFunctionCall,
	EFX_VMFunctionCall,
	EFX_Sequence,
	EFX_CompoundStatement,
	EFX_IfStatement,
	EFX_LoopStatement,
	EFX_WhileLoop,
	EFX_DoWhileLoop,
	EFX_ForLoop,
	EFX_ForEachLoop,
	EFX_JumpStatement,
	EFX_ReturnStatement,
	EFX_ClassTypeCast,
	EFX_ClassPtrCast,
	EFX_StateByIndex,
	EFX_RuntimeStateIndex,
	EFX_MultiNameState,
	EFX_DamageValue,
	EFX_Nop,
	EFX_LocalVariableDeclaration,
	EFX_SwitchStatement,
	EFX_CaseStatement,
	EFX_VectorValue,
	EFX_VectorPlusZ,
	EFX_VectorBuiltin,
	EFX_TypeCheck,
	EFX_DynamicCast,
	EFX_GlobalVariable,
	EFX_Super,
	EFX_StackVariable,
	EFX_MultiAssign,
	EFX_StaticArray,
	EFX_StaticArrayVariable,
	EFX_CVar,
	EFX_NamedNode,
	EFX_GetClass,
	EFX_GetParentClass,
	EFX_GetClassName,
	EFX_IsAbstract,
	EFX_StrLen,
	EFX_ColorLiteral,
	EFX_GetDefaultByType,
	EFX_FontCast,
	EFX_LocalArrayDeclaration,
	EFX_OutVarDereference,
	EFX_ToVector,
	EFX_COUNT
};

//==========================================================================
//
//
//
//==========================================================================

class FxExpression
{
protected:
	FxExpression(EFxType type, const FScriptPosition &pos)
	: ScriptPosition(pos), ExprType(type)
	{
	}

public:	
	virtual ~FxExpression() {}
	virtual FxExpression *Resolve(FCompileContext &ctx);

	virtual bool isConstant() const;
	virtual bool RequestAddress(FCompileContext &ctx, bool *writable);
	virtual PPrototype *ReturnProto();
	virtual VMFunction *GetDirectFunction(PFunction *func, const VersionInfo &ver);
	virtual bool CheckReturn() { return false; }
	virtual int GetBitValue() { return -1; }
	bool IsNumeric() const { return ValueType->isNumeric(); }
	bool IsFloat() const { return ValueType->isFloat(); }
	bool IsInteger() const { return ValueType->isNumeric() && ValueType->isIntCompatible(); }
	bool IsPointer() const { return ValueType->isPointer(); }
	bool IsVector() const { return IsVector2() || IsVector3() || IsVector4(); };
	bool IsVector2() const { return ValueType == TypeVector2 || ValueType == TypeFVector2; };
	bool IsVector3() const { return ValueType == TypeVector3 || ValueType == TypeFVector3; };
	bool IsVector4() const { return ValueType == TypeVector4 || ValueType == TypeFVector4; };
	bool IsQuaternion() const { return ValueType == TypeQuaternion || ValueType == TypeFQuaternion || ValueType == TypeQuaternionStruct; };
	bool IsBoolCompat() const { return ValueType->isScalar(); }
	bool IsObject() const { return ValueType->isObjectPointer(); }
	bool IsArray() const { return ValueType->isArray() || (ValueType->isPointer() && ValueType->toPointer()->PointedType->isArray()); }
	bool isStaticArray() const { return (ValueType->isPointer() && ValueType->toPointer()->PointedType->isStaticArray()); } // can only exist in pointer form.
	bool IsDynamicArray() const { return (ValueType->isDynArray()); }
	bool IsMap() const { return ValueType->isMap(); }
	bool IsMapIterator() const { return ValueType->isMapIterator(); }
	bool IsStruct() const { return ValueType->isStruct(); }
	bool IsNativeStruct() const { return (ValueType->isStruct() && static_cast<PStruct*>(ValueType)->isNative); }

	virtual ExpEmit Emit(VMFunctionBuilder *build);
	void EmitStatement(VMFunctionBuilder *build);
	virtual void EmitCompare(VMFunctionBuilder *build, bool invert, TArray<size_t> &patchspots_yes, TArray<size_t> &patchspots_no);

	FScriptPosition ScriptPosition;
	PType *ValueType = nullptr;

	bool isresolved = false;
	bool NeedResult = true;	// should be set to false if not needed and properly handled by all nodes for their subnodes to eliminate redundant code
	EFxType ExprType;

	void *operator new(size_t size)
	{
		return FxAlloc.Alloc(size);
	}

	void operator delete(void *block) {}
	void operator delete[](void *block) {}
};

//==========================================================================
//
//	FxIdentifier
//
//==========================================================================

class FxIdentifier : public FxExpression
{
public:
	FName Identifier = NAME_None;
	bool noglobal = false;

	FxIdentifier(FName i, const FScriptPosition &p);
	FxExpression *Resolve(FCompileContext&);
	FxExpression *ResolveMember(FCompileContext&, PContainerType*, FxExpression*&, PContainerType*);
};


//==========================================================================
//
//	FxMemberIdentifier
//
//==========================================================================

class FxMemberIdentifier : public FxIdentifier
{
	FxExpression *Object;

public:
	FxMemberIdentifier(FxExpression *obj, FName i, const FScriptPosition &p);
	~FxMemberIdentifier();
	FxExpression *Resolve(FCompileContext&);
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
	FxConstant(bool val, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		ValueType = value.Type = TypeBool;
		value.Int = val;
		isresolved = true;
	}

	FxConstant(int val, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		ValueType = value.Type = TypeSInt32;
		value.Int = val;
		isresolved = true;
	}

	FxConstant(unsigned int val, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		ValueType = value.Type = TypeUInt32;
		value.Int = val;
		isresolved = true;
	}

	FxConstant(double val, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		ValueType = value.Type = TypeFloat64;
		value.Float = val;
		isresolved = true;
	}

	FxConstant(FSoundID val, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		ValueType = value.Type = TypeSound;
		value.Int = val.index();
		isresolved = true;
	}

	FxConstant(FName val, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		ValueType = value.Type = TypeName;
		value.Int = val.GetIndex();
		isresolved = true;
	}

	FxConstant(const FString &str, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		ValueType = TypeString;
		value = ExpVal(str);
		isresolved = true;
	}

	FxConstant(ExpVal cv, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		value = cv;
		ValueType = cv.Type;
		isresolved = true;
	}

	FxConstant(PClass *val, PClassPointer *valtype, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		value.pointer = (void*)val;
		value.Type = ValueType = valtype;
		isresolved = true;
	}

	FxConstant(FState *state, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		value.pointer = state;
		ValueType = value.Type = TypeState;
		isresolved = true;
	}

	FxConstant(FFont *state, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		value.pointer = state;
		ValueType = value.Type = TypeFont;
		isresolved = true;
	}

	FxConstant(void *state, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		value.pointer = state;
		ValueType = value.Type = TypeVoidPtr;
		isresolved = true;
	}

	FxConstant(const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		value.pointer = nullptr;
		ValueType = value.Type = TypeNullPtr;
		isresolved = true;
	}

	FxConstant(PType *type, TypedVMValue &vmval, const FScriptPosition &pos) : FxExpression(EFX_Constant, pos)
	{
		isresolved = true;
		switch (vmval.Type)
		{
		default:
		case REGT_INT:
			value.Int = vmval.i;
			break;

		case REGT_FLOAT:
			value.Float = vmval.f;
			break;

		case REGT_STRING:
			new(&value) ExpVal(vmval.s());
			break;

		case REGT_POINTER:
			value.pointer = vmval.a;
			break;
		}
		ValueType = value.Type = type;
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

class FxVectorValue : public FxExpression
{
	constexpr static int maxVectorDimensions = 4;
	bool isConst;	// gets set to true if all element are const (used by function defaults parser)

public:
	FxExpression *xyzw[maxVectorDimensions];

	friend class ZCCCompiler;

	FxVectorValue(FxExpression *x, FxExpression *y, FxExpression *z, FxExpression* w, const FScriptPosition &sc);
	~FxVectorValue();
	FxExpression *Resolve(FCompileContext&);
	bool isConstVector(int dim)
	{
		if (!isConst)
			return false;
		return dim >= 0 && dim <= maxVectorDimensions && xyzw[dim - 1] && (dim == maxVectorDimensions || !xyzw[dim]);
	}

	ExpEmit Emit(VMFunctionBuilder *build);
};


//==========================================================================
//
//
//
//==========================================================================

class FxQuaternionValue : public FxVectorValue
{
public:
	FxQuaternionValue(FxExpression* x, FxExpression* y, FxExpression* z, FxExpression* w, const FScriptPosition& sc);
	FxExpression* Resolve(FCompileContext&);
};


//==========================================================================
//
//
//
//==========================================================================

class FxBoolCast : public FxExpression
{
	FxExpression *basex;
	bool NeedValue;

public:

	FxBoolCast(FxExpression *x, bool needvalue = true);
	~FxBoolCast();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
	void EmitCompare(VMFunctionBuilder *build, bool invert, TArray<size_t> &patchspots_yes, TArray<size_t> &patchspots_no);
};

class FxIntCast : public FxExpression
{
	FxExpression *basex;
	bool NoWarn;
	bool Explicit;

public:

	FxIntCast(FxExpression *x, bool nowarn, bool explicitly = false, bool isunsigned = false);
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

class FxNameCast : public FxExpression
{
	FxExpression *basex;
	bool mExplicit;

public:

	FxNameCast(FxExpression *x, bool explicitly = false);
	~FxNameCast();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

class FxStringCast : public FxExpression
{
	FxExpression *basex;

public:

	FxStringCast(FxExpression *x);
	~FxStringCast();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

class FxColorCast : public FxExpression
{
	FxExpression *basex;

public:

	FxColorCast(FxExpression *x);
	~FxColorCast();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

class FxSoundCast : public FxExpression
{
	FxExpression *basex;

public:

	FxSoundCast(FxExpression *x);
	~FxSoundCast();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

class FxFontCast : public FxExpression
{
	FxExpression *basex;

public:

	FxFontCast(FxExpression *x);
	~FxFontCast();
	FxExpression *Resolve(FCompileContext&);
};


//==========================================================================
//
//	FxTypeCast
//
//==========================================================================

class FxTypeCast : public FxExpression
{
public:
	FxExpression *basex;
	bool NoWarn;
	bool Explicit;

	FxTypeCast(FxExpression *x, PType *type, bool nowarn, bool explicitly = false);
	~FxTypeCast();
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
	void EmitCompare(VMFunctionBuilder *build, bool invert, TArray<size_t> &patchspots_yes, TArray<size_t> &patchspots_no);
};

//==========================================================================
//
//	FxSign
//
//==========================================================================

class FxSizeAlign : public FxExpression
{
	FxExpression *Operand;
	int Which;

public:
	FxSizeAlign(FxExpression*, int which);
	~FxSizeAlign();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxPreIncrDecr
//
//==========================================================================

class FxPreIncrDecr : public FxExpression
{
	int Token;
	FxExpression *Base;
	bool AddressRequested;
	bool AddressWritable;

public:
	FxPreIncrDecr(FxExpression *base, int token);
	~FxPreIncrDecr();
	FxExpression *Resolve(FCompileContext&);
	bool RequestAddress(FCompileContext &ctx, bool *writable);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxPostIncrDecr
//
//==========================================================================

class FxPostIncrDecr : public FxExpression
{
	int Token;
	FxExpression *Base;

public:
	FxPostIncrDecr(FxExpression *base, int token);
	~FxPostIncrDecr();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxAssign
//
//==========================================================================

class FxAssign : public FxExpression
{
	FxExpression *Base;
	FxExpression *Right;
	int IsBitWrite;
	bool AddressRequested;
	bool AddressWritable;
	bool IsModifyAssign;

	friend class FxAssignSelf;

public:
	FxAssign(FxExpression *base, FxExpression *right, bool ismodify = false);
	~FxAssign();
	FxExpression *Resolve(FCompileContext&);
	//bool RequestAddress(FCompileContext &ctx, bool *writable);
	ExpEmit Emit(VMFunctionBuilder *build);

	ExpEmit Address;
};

//==========================================================================
//
//	FxAssign
//
//==========================================================================
class FxCompoundStatement;

class FxMultiAssign : public FxExpression
{
	FxCompoundStatement *LocalVarContainer;	// for handling the temporary variables of the results, which may need type casts.
	FArgumentList Base;
	FxExpression *Right;
	bool AddressRequested = false;
	bool AddressWritable = false;

public:
	FxMultiAssign(FArgumentList &base, FxExpression *right, const FScriptPosition &pos);
	~FxMultiAssign();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

class FxMultiAssignDecl : public FxExpression
{
	FArgumentList Base;
	FxExpression *Right;
public:
	FxMultiAssignDecl(FArgumentList &base, FxExpression *right, const FScriptPosition &pos);
	~FxMultiAssignDecl();
	FxExpression *Resolve(FCompileContext&);
	//ExpEmit Emit(VMFunctionBuilder *build); This node is transformed into Declarations + FxMultiAssign , so it won't ever be emitted itself
};

//==========================================================================
//
//	FxAssignSelf
//
//==========================================================================

class FxAssignSelf : public FxExpression
{
public:
	FxAssign *Assignment;

	FxAssignSelf(const FScriptPosition &pos);
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
	bool Promote(FCompileContext &ctx, bool forceint = false, bool shiftop = false);
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

class FxPow : public FxBinary
{
public:

	FxPow(FxExpression*, FxExpression*);
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
	PType *CompareType;
public:

	FxCompareRel(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit EmitCommon(VMFunctionBuilder *build, bool forcompare, bool invert);
	ExpEmit Emit(VMFunctionBuilder *build);
	void EmitCompare(VMFunctionBuilder *build, bool invert, TArray<size_t> &patchspots_yes, TArray<size_t> &patchspots_no);
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
	ExpEmit EmitCommon(VMFunctionBuilder *build, bool forcompare, bool invert);
	ExpEmit Emit(VMFunctionBuilder *build);
	void EmitCompare(VMFunctionBuilder *build, bool invert, TArray<size_t> &patchspots_yes, TArray<size_t> &patchspots_no);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxBitOp : public FxBinary
{
public:

	FxBitOp(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxBinary
//
//==========================================================================

class FxShift : public FxBinary
{
public:

	FxShift(int, FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxLtGtEq : public FxBinary
{
public:
	FxLtGtEq(FxExpression*, FxExpression*);
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxConcat : public FxBinary
{
public:
	FxConcat(FxExpression*, FxExpression*);
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
	TDeletingArray<FxExpression *> list;

	FxBinaryLogical(int, FxExpression*, FxExpression*);
	~FxBinaryLogical();
	void Flatten();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxBinaryLogical
//
//==========================================================================

class FxDotCross : public FxExpression
{
public:
	int				Operator;
	FxExpression		*left;
	FxExpression		*right;

	FxDotCross(int, FxExpression*, FxExpression*);
	~FxDotCross();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxTypeCheck : public FxExpression
{
public:
	FxExpression		*left;
	FxExpression		*right;
	bool ClassCheck;

	FxTypeCheck(FxExpression*, FxExpression*);
	~FxTypeCheck();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit EmitCommon(VMFunctionBuilder *build);
	ExpEmit Emit(VMFunctionBuilder *build);
	void EmitCompare(VMFunctionBuilder *build, bool invert, TArray<size_t> &patchspots_yes, TArray<size_t> &patchspots_no);
};

//==========================================================================
//
//
//
//==========================================================================

class FxDynamicCast : public FxExpression
{
	PClass *CastType;
public:
	FxExpression *expr;

	FxDynamicCast(PClass*, FxExpression*);
	~FxDynamicCast();
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

class FxATan2 : public FxExpression
{
	FxExpression *yval, *xval;

public:

	FxATan2(FxExpression *y, FxExpression *x, const FScriptPosition &pos);
	~FxATan2();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);

private:
	ExpEmit ToReg(VMFunctionBuilder *build, FxExpression *val);
};

class FxATan2Vec : public FxExpression
{
	FxExpression* vval;

public:

	FxATan2Vec(FxExpression* y, const FScriptPosition& pos);
	~FxATan2Vec();
	FxExpression* Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder* build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxNew : public FxExpression
{
	FxExpression *val;
	PFunction *CallingFunction;

public:

	FxNew(FxExpression *v);
	~FxNew();
	FxExpression *Resolve(FCompileContext&);

	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxMinMax : public FxExpression
{
	TDeletingArray<FxExpression *> choices;
	FName Type;

public:
	FxMinMax(TArray<FxExpression*> &expr, FName type, const FScriptPosition &pos);
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

	FxRandom(EFxType type, FRandom * r, const FScriptPosition &pos);
public:

	FxRandom(FRandom *, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos, bool nowarn);
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

	FxRandomPick(FRandom *, TArray<FxExpression*> &expr, bool floaty, const FScriptPosition &pos, bool nowarn);
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

	FxRandom2(FRandom *, FxExpression *m, const FScriptPosition &pos, bool nowarn);
	~FxRandom2();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};


//==========================================================================
//
//
//
//==========================================================================

class FxRandomSeed : public FxExpression
{
protected:
	FRandom *rng;
	FxExpression *seed;

public:

	FxRandomSeed(FRandom *, FxExpression *mi, const FScriptPosition &pos, bool nowarn);
	~FxRandomSeed();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxMemberBase
//
//==========================================================================

class FxMemberBase : public FxExpression
{
public:
	PField *membervar;
	bool AddressRequested = false;
	bool AddressWritable = true;
	int BarrierSide = -1; // [ZZ] some magic
	FxMemberBase(EFxType type, PField *f, const FScriptPosition &p);
};

//==========================================================================
//
//	FxGlobalVariaböe
//
//==========================================================================

class FxGlobalVariable : public FxMemberBase
{
public:
	FxGlobalVariable(PField*, const FScriptPosition&);
	FxExpression *Resolve(FCompileContext&);
	bool RequestAddress(FCompileContext &ctx, bool *writable);
	ExpEmit Emit(VMFunctionBuilder *build);
	virtual int GetBitValue() { return membervar->BitValue; }
};

class FxCVar : public FxExpression
{
	FBaseCVar *CVar;
public:
	FxCVar(FBaseCVar*, const FScriptPosition&);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};


//==========================================================================
//
//	FxClassMember
//
//==========================================================================

class FxStructMember : public FxMemberBase
{
public:
	FxExpression *classx;

	FxStructMember(FxExpression*, PField*, const FScriptPosition&);
	~FxStructMember();
	FxExpression *Resolve(FCompileContext&);
	bool RequestAddress(FCompileContext &ctx, bool *writable);
	ExpEmit Emit(VMFunctionBuilder *build);
	virtual int GetBitValue() { return membervar->BitValue; }
};

//==========================================================================
//
//	FxClassMember
//
//==========================================================================

class FxClassMember : public FxStructMember
{
public:

	FxClassMember(FxExpression*, PField*, const FScriptPosition&);
};

//==========================================================================
//
//	FxLocalVariable
//
//==========================================================================

class FxLocalVariable : public FxExpression
{
public:
	FxLocalVariableDeclaration *Variable;
	bool AddressRequested;
	int RegOffset;

	FxLocalVariable(FxLocalVariableDeclaration*, const FScriptPosition&);
	FxExpression *Resolve(FCompileContext&);
	bool RequestAddress(FCompileContext &ctx, bool *writable);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxLocalVariable
//
//==========================================================================

class FxStackVariable : public FxMemberBase
{
public:
	FxStackVariable(PType *type, int offset, const FScriptPosition&);
	~FxStackVariable();
	FxExpression *Resolve(FCompileContext&);
	bool RequestAddress(FCompileContext &ctx, bool *writable);
	ExpEmit Emit(VMFunctionBuilder *build);
	virtual int GetBitValue() { return membervar->BitValue; }
};

//==========================================================================
//
//	FxLocalVariable
//
//==========================================================================
class FxStaticArray;

class FxStaticArrayVariable : public FxExpression
{
public:
	FxStaticArray *Variable;
	bool AddressRequested;

	FxStaticArrayVariable(FxLocalVariableDeclaration*, const FScriptPosition&);
	FxExpression *Resolve(FCompileContext&);
	bool RequestAddress(FCompileContext &ctx, bool *writable);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxSelf
//
//==========================================================================

class FxSelf : public FxExpression
{
	bool check;

public:
	FxSelf(const FScriptPosition&, bool deccheck = false);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxSuper
//
//==========================================================================

class FxSuper : public FxSelf
{
public:
	FxSuper(const FScriptPosition&pos)
		: FxSelf(pos)
	{
		ExprType = EFX_Super;
	}
	FxExpression *Resolve(FCompileContext&);
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
	size_t SizeAddr;
	bool AddressRequested;
	bool AddressWritable;
	bool arrayispointer = false;
	bool noboundscheck;

	FxArrayElement(FxExpression*, FxExpression*, bool = false);
	~FxArrayElement();
	FxExpression *Resolve(FCompileContext&);
	bool RequestAddress(FCompileContext &ctx, bool *writable);
	ExpEmit Emit(VMFunctionBuilder *build);
};


//==========================================================================
//
//	FxFunctionCall
//
//==========================================================================

class FxFunctionCall : public FxExpression
{
	FRandom *RNG;

public:
	FName MethodName;
	FArgumentList ArgList;

	FxFunctionCall(FName methodname, FName rngname, FArgumentList &args, const FScriptPosition &pos);
	~FxFunctionCall();
	FxExpression *Resolve(FCompileContext&);
};


//==========================================================================
//
//	FxFunctionCall
//
//==========================================================================

class FxMemberFunctionCall : public FxExpression
{
	FxExpression *Self;
	FName MethodName;
	FArgumentList ArgList;

public:

	FxMemberFunctionCall(FxExpression *self, FName methodname, FArgumentList &args, const FScriptPosition &pos);
	~FxMemberFunctionCall();
	FxExpression *Resolve(FCompileContext&);
};


//==========================================================================
//
//	FxFlopFunctionCall
//
//==========================================================================

class FxFlopFunctionCall : public FxExpression
{
	int Index;
	FArgumentList ArgList;

public:

	FxFlopFunctionCall(size_t index, FArgumentList &args, const FScriptPosition &pos);
	~FxFlopFunctionCall();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxVectorBuiltin
//
//==========================================================================

class FxVectorBuiltin : public FxExpression
{
	FName Function;
	FxExpression *Self;

public:

	FxVectorBuiltin(FxExpression *self, FName name);
	~FxVectorBuiltin();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxPlusZ
//
//==========================================================================

class FxVectorPlusZ : public FxExpression
{
	FName Function;
	FxExpression* Self;
	FxExpression* Z;

public:

	FxVectorPlusZ(FxExpression* self, FName name, FxExpression*);
	~FxVectorPlusZ();
	FxExpression* Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder* build);
};

//==========================================================================
//
//	FxPlusZ
//
//==========================================================================

class FxToVector : public FxExpression
{
	FxExpression* Self;

public:

	FxToVector(FxExpression* self);
	~FxToVector();
	FxExpression* Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder* build);
};

//==========================================================================
//
//	FxVectorBuiltin
//
//==========================================================================

class FxStrLen : public FxExpression
{
	FxExpression *Self;

public:

	FxStrLen(FxExpression *self);
	~FxStrLen();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxGetClass
//
//==========================================================================

class FxGetClass : public FxExpression
{
	FxExpression *Self;

public:

	FxGetClass(FxExpression *self);
	~FxGetClass();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxGetClass
//
//==========================================================================

class FxGetParentClass : public FxExpression
{
	FxExpression *Self;

public:

	FxGetParentClass(FxExpression *self);
	~FxGetParentClass();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxGetClass
//
//==========================================================================

class FxGetClassName : public FxExpression
{
	FxExpression *Self;

public:

	FxGetClassName(FxExpression *self);
	~FxGetClassName();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//	FxIsAbstract
//
//==========================================================================

class FxIsAbstract : public FxExpression
{
	FxExpression* Self;

public:

	FxIsAbstract(FxExpression* self);
	~FxIsAbstract();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder* build);
};

//==========================================================================
//
//	FxColorLiteral
//
//==========================================================================

class FxColorLiteral : public FxExpression
{
	FArgumentList ArgList;
	int constval = 0;

public:

	FxColorLiteral(FArgumentList &args, FScriptPosition &sc);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
// FxVMFunctionCall
//
//==========================================================================

class FxVMFunctionCall : public FxExpression
{
	friend class FxMultiAssign;

	bool NoVirtual;
	bool hasStringArgs = false;
	FxExpression *Self;
	// for multi assignment
	int AssignCount = 0;
	TArray<ExpEmit> ReturnRegs;
	PFunction *CallingFunction;

	bool CheckAccessibility(const VersionInfo &ver);

public:

	FArgumentList ArgList;
	PFunction* Function;

	FxVMFunctionCall(FxExpression *self, PFunction *func, FArgumentList &args, const FScriptPosition &pos, bool novirtual);
	~FxVMFunctionCall();
	FxExpression *Resolve(FCompileContext&);
	PPrototype *ReturnProto();
	VMFunction *GetDirectFunction(PFunction *func, const VersionInfo &ver);
	ExpEmit Emit(VMFunctionBuilder *build);
	bool CheckEmitCast(VMFunctionBuilder *build, bool returnit, ExpEmit &reg);
	TArray<PType*> &GetReturnTypes() const
	{
		return Function->Variants[0].Proto->ReturnTypes;
	}
};

//==========================================================================
//
// FxSequence (a list of statements with no semantics attached - used to return multiple nodes as one)
//
//==========================================================================
class FxLocalVariableDeclaration;

class FxSequence : public FxExpression
{
	TDeletingArray<FxExpression *> Expressions;

public:
	FxSequence(const FScriptPosition &pos) : FxExpression(EFX_Sequence, pos) {}
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
	void Add(FxExpression *expr) { if (expr != NULL) Expressions.Push(expr); expr->NeedResult = false; }
	VMFunction *GetDirectFunction(PFunction *func, const VersionInfo &ver);
	bool CheckReturn();
};

//==========================================================================
//
// FxCompoundStatement (like a list but implements maintenance of local variables)
//
//==========================================================================
class FxLocalVariableDeclaration;

class FxCompoundStatement : public FxSequence
{
	TArray<FxLocalVariableDeclaration *> LocalVars;
	FxCompoundStatement *Outer = nullptr;

	friend class FxLocalVariableDeclaration;
	friend class FxStaticArray;
	friend class FxMultiAssign;
	friend class FxLocalArrayDeclaration;

public:
	FxCompoundStatement(const FScriptPosition &pos) : FxSequence(pos) {}
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
	FxLocalVariableDeclaration *FindLocalVariable(FName name, FCompileContext &ctx);
	bool CheckLocalVariable(FName name);
};

//==========================================================================
//
// FxSwitchStatement
//
//==========================================================================

class FxSwitchStatement : public FxExpression
{
	FxExpression *Condition;
	FArgumentList Content;

	struct CaseAddr
	{
		int casevalue;
		size_t jumpaddress;
	};

	TArray<CaseAddr> CaseAddresses;

public:
	TArray<FxJumpStatement *> Breaks;

	FxSwitchStatement(FxExpression *cond, FArgumentList &content, const FScriptPosition &pos);
	~FxSwitchStatement();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
	bool CheckReturn();
};

//==========================================================================
//
// FxSwitchStatement
//
//==========================================================================

class FxCaseStatement : public FxExpression
{
	FxExpression *Condition;
	int CaseValue;	// copy the value to here for easier access.

	friend class FxSwitchStatement;

public:
	FxCaseStatement(FxExpression *cond, const FScriptPosition &pos);
	~FxCaseStatement();
	FxExpression *Resolve(FCompileContext&);
};

//==========================================================================
//
// FxIfStatement
//
//==========================================================================

class FxIfStatement : public FxExpression
{
	FxExpression *Condition;
	FxExpression *WhenTrue;
	FxExpression *WhenFalse;

public:
	FxIfStatement(FxExpression *cond, FxExpression *true_part, FxExpression *false_part, const FScriptPosition &pos);
	~FxIfStatement();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
	bool CheckReturn();
};

//==========================================================================
//
// Base class for loops
//
//==========================================================================

class FxLoopStatement : public FxExpression
{
protected:
	FxLoopStatement(EFxType etype, const FScriptPosition &pos)
	: FxExpression(etype, pos)
	{
	}

	void Backpatch(VMFunctionBuilder *build, size_t loopstart, size_t loopend);
	FxExpression *Resolve(FCompileContext&) final;
	virtual FxExpression *DoResolve(FCompileContext&) = 0;

public:
	TArray<FxJumpStatement *> Jumps;
};

//==========================================================================
//
// FxWhileLoop
//
//==========================================================================

class FxWhileLoop : public FxLoopStatement
{
	FxExpression *Condition;
	FxExpression *Code;

public:
	FxWhileLoop(FxExpression *condition, FxExpression *code, const FScriptPosition &pos);
	~FxWhileLoop();
	FxExpression *DoResolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
// FxDoWhileLoop
//
//==========================================================================

class FxDoWhileLoop : public FxLoopStatement
{
	FxExpression *Condition;
	FxExpression *Code;

public:
	FxDoWhileLoop(FxExpression *condition, FxExpression *code, const FScriptPosition &pos);
	~FxDoWhileLoop();
	FxExpression *DoResolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
// FxForLoop
//
//==========================================================================

class FxForLoop : public FxLoopStatement
{
	FxExpression *Init;
	FxExpression *Condition;
	FxExpression *Iteration;
	FxExpression *Code;

public:
	FxForLoop(FxExpression *init, FxExpression *condition, FxExpression *iteration, FxExpression *code, const FScriptPosition &pos);
	~FxForLoop();
	FxExpression *DoResolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder* build);
};

//==========================================================================
//
// FxForLoop
//
//==========================================================================

class FxForEachLoop : public FxLoopStatement
{
	FName loopVarName;
	FxExpression* Array;
	FxExpression* Array2;
	FxExpression* Code;

public:
	FxForEachLoop(FName vn, FxExpression* arrayvar, FxExpression* arrayvar2, FxExpression* code, const FScriptPosition& pos);
	~FxForEachLoop();
	FxExpression* DoResolve(FCompileContext&);
};

//==========================================================================
//
// FxJumpStatement
//
//==========================================================================

class FxJumpStatement : public FxExpression
{
public:
	FxJumpStatement(int token, const FScriptPosition &pos);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);

	int Token;
	size_t Address;
};

//==========================================================================
//
// FxReturnStatement
//
//==========================================================================

class FxReturnStatement : public FxExpression
{
	FArgumentList Args;

public:
	FxReturnStatement(FxExpression *value, const FScriptPosition &pos);
	FxReturnStatement(FArgumentList &args, const FScriptPosition &pos);
	~FxReturnStatement();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
	VMFunction *GetDirectFunction(PFunction *func, const VersionInfo &ver);
	bool CheckReturn() { return true; }
};

//==========================================================================
//
//
//
//==========================================================================

class FxClassTypeCast : public FxExpression
{
	PClass *desttype;
	FxExpression *basex;
	bool Explicit;

public:

	FxClassTypeCast(PClassPointer *dtype, FxExpression *x, bool explicitly);
	~FxClassTypeCast();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxClassPtrCast : public FxExpression
{
	PClass *desttype;
	FxExpression *basex;

public:

	FxClassPtrCast(PClass *dtype, FxExpression *x);
	~FxClassPtrCast();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxNop : public FxExpression
{
public:
	FxNop(const FScriptPosition &p)
		: FxExpression(EFX_Nop, p)
	{
		isresolved = true;
		ValueType = TypeVoid;
	}
	ExpEmit Emit(VMFunctionBuilder *build)
	{
		return ExpEmit();
	}
};

//==========================================================================
//
//
//
//==========================================================================

class FxLocalVariableDeclaration : public FxExpression
{
	friend class FxCompoundStatement;
	friend class FxLocalVariable;
	friend class FxStaticArrayVariable;
	friend class FxLocalArrayDeclaration;
	friend class FxStructMember;

	FName Name;
	FxExpression *Init;
	int VarFlags;
	int RegCount;

protected:
	FxExpression *clearExpr;

	void ClearDynamicArray(VMFunctionBuilder *build);

public:
	int StackOffset = -1;
	int RegNum = -1;

	FxLocalVariableDeclaration(PType *type, FName name, FxExpression *initval, int varflags, const FScriptPosition &p);
	~FxLocalVariableDeclaration();
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
	void Release(VMFunctionBuilder *build);
	void SetReg(ExpEmit reginfo);

};

//==========================================================================
//
//
//
//==========================================================================

class FxStaticArray : public FxLocalVariableDeclaration
{
	friend class FxStaticArrayVariable;

	PType *ElementType;
	FArgumentList values;

public:

	FxStaticArray(PType *type, FName name, FArgumentList &args, const FScriptPosition &pos);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

class FxNamedNode : public FxExpression
{
public:
	FName name;
	FxExpression *value;
	FxNamedNode(FName n, FxExpression *x, const FScriptPosition &pos)
		: FxExpression(EFX_NamedNode, pos), name(n), value(x)
	{
	}

	FxExpression *Resolve(FCompileContext&)
	{
		// This should never reach the backend in a supported context, 
		// it's just needed to extend argument lists with the skipped parameters and needs to be resolved by the parent node.
		ScriptPosition.Message(MSG_ERROR, "Named arguments not supported here");
		delete this;
		return nullptr;
	}
};

//==========================================================================
//
//
//
//==========================================================================

class FxLocalArrayDeclaration : public FxLocalVariableDeclaration
{
	PType *ElementType;
	FArgumentList values;

public:

	FxLocalArrayDeclaration(PType *type, FName name, FArgumentList &args, int varflags, const FScriptPosition &pos);
	FxExpression *Resolve(FCompileContext&);
	ExpEmit Emit(VMFunctionBuilder *build);
};

//==========================================================================
//
//
//
//==========================================================================

class FxOutVarDereference : public FxExpression
{
	FxExpression *Self;
	PType *SelfType;
	bool AddressWritable;

public:
	FxOutVarDereference(FxExpression *self, const FScriptPosition &p)
		: FxExpression(EFX_OutVarDereference, p), Self (self)
	{
	}
	~FxOutVarDereference();
	FxExpression *Resolve(FCompileContext &);
	bool RequestAddress(FCompileContext &ctx, bool *writable);
	ExpEmit Emit(VMFunctionBuilder *build);
};


struct CompileEnvironment
{
	FxExpression* (*SpecialTypeCast)(FxTypeCast* func, FCompileContext& ctx);
	bool (*CheckForCustomAddition)(FxAddSub* func, FCompileContext& ctx);
	FxExpression* (*CheckSpecialIdentifier)(FxIdentifier* func, FCompileContext& ctx);
	FxExpression* (*CheckSpecialGlobalIdentifier)(FxIdentifier* func, FCompileContext& ctx);
	FxExpression* (*ResolveSpecialIdentifier)(FxIdentifier* func, FxExpression*& object, PContainerType* objtype, FCompileContext& ctx);
	FxExpression* (*CheckSpecialMember)(FxStructMember* func, FCompileContext& ctx);
	FxExpression* (*CheckCustomGlobalFunctions)(FxFunctionCall* func, FCompileContext& ctx);
	bool (*ResolveSpecialFunction)(FxVMFunctionCall* func, FCompileContext& ctx);
	FName CustomBuiltinNew;	//override the 'new' function if some classes need special treatment.
};

extern CompileEnvironment compileEnvironment;

#endif
