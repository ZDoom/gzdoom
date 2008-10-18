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


//==========================================================================
//
//
//
//==========================================================================
/*
class FxType
{
	EBaseType type;
public:
	FxType(EBaseType t) { type = t; }
	FxType(const FxType & t) { type = t.type; }
	FxType &operator =(const FxType &t) { type = t.type; return *this; }

	EBaseType GetBaseType() const { return type; }
};
*/

//==========================================================================
//
//
//
//==========================================================================

enum
{
	MSG_WARNING,
	MSG_ERROR,
	MSG_DEBUG,
	MSG_LOG,
	MSG_DEBUGLOG
};

//==========================================================================
//
//
//
//==========================================================================

struct FScriptPosition
{
	FString FileName;
	int ScriptLine;

	FScriptPosition()
	{
		ScriptLine=0;
	}
	FScriptPosition(const FScriptPosition &other)
	{
		FileName = other.FileName;
		ScriptLine = other.ScriptLine;
	}
	FScriptPosition(FString fname, int line)
	{
		FileName = fname;
		ScriptLine = line;
	}
	FScriptPosition(FScanner &sc)
	{
		FileName = sc.ScriptName;
		ScriptLine = sc.GetMessageLine();
	}
	FScriptPosition &operator=(const FScriptPosition &other)
	{
		FileName = other.FileName;
		ScriptLine = other.ScriptLine;
		return *this;
	}

	void Message(int severity, const char *message,...);
};

//==========================================================================
//
//
//
//==========================================================================

struct FCompileContext
{
	const PClass *cls;
};

//==========================================================================
//
//
//
//==========================================================================

enum ExpOp
{
	EX_NOP,

	EX_Var,

	EX_Sin,			// sin (angle)
	EX_Cos,			// cos (angle)
	EX_ActionSpecial,
	EX_Right,
};

//==========================================================================
//
//
//
//==========================================================================

enum ExpValType
{
	VAL_Int,
	VAL_Float,
	VAL_Unknown,

	// only used for accessing class member fields to ensure proper conversion
	VAL_Fixed,
	VAL_Angle,
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
	};

	int GetInt()
	{
		return Type == VAL_Int? Int : Type == VAL_Float? int(Float) : 0;
	}

	double GetFloat()
	{
		return Type == VAL_Int? double(Int) : Type == VAL_Float? Float : 0;
	}

	bool GetBool()
	{
		return Type == VAL_Int? !!Int : Type == VAL_Float? Float!=0. : false;
	}

};


//==========================================================================
//
//
//
//==========================================================================

struct FxExpression
{
	FxExpression ()
	{
		isresolved = false;
		ValueType = VAL_Unknown;
		Type = EX_NOP;
		Value.Type = VAL_Int;
		Value.Int = 0;
		for (int i = 0; i < 2; i++)
			Children[i] = NULL;
	}
	virtual ~FxExpression ()
	{
		for (int i = 0; i < 2; i++)
		{
			if (Children[i])
			{
				delete Children[i];
			}
		}
	}

protected:
	FxExpression(const FScriptPosition &pos)
	{
		isresolved = false;
		ScriptPosition = pos;
		for (int i = 0; i < 2; i++)
			Children[i] = NULL;
	}
public:
	virtual FxExpression *Resolve(FCompileContext &ctx);
	FxExpression *ResolveAsBoolean(FCompileContext &ctx)
	{
		// This will need more handling if other types than Int and Float are added
		return Resolve(ctx);
	}
	
	virtual ExpVal EvalExpression (AActor *self, const PClass *cls);
	virtual bool isConstant() const;

	int Type;
	ExpVal Value;
	FxExpression *Children[2];

	FScriptPosition ScriptPosition;
	int ValueType;
protected:
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
//	FxArrayElement
//
//==========================================================================

class FxArrayElement : public FxExpression
{
public:
	FxExpression *Array;
	FxExpression *index;

	FxArrayElement(FxExpression*, FxExpression*, const FScriptPosition&);
	~FxArrayElement();
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
	FxConstant(int val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = VAL_Int;
		value.Int = val;
	}

	FxConstant(double val, const FScriptPosition &pos) : FxExpression(pos)
	{
		ValueType = value.Type = VAL_Float;
		value.Float = val;
	}

	FxConstant(ExpVal cv, const FScriptPosition &pos) : FxExpression(pos)
	{
		value = cv;
		ValueType = cv.Type;
	}

	bool isConstant() const
	{
		return true;
	}
	ExpVal EvalExpression (AActor *self, const PClass *cls);
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

	ExpVal EvalExpression (AActor *self, const PClass *cls);
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
	ExpVal EvalExpression (AActor *self, const PClass *cls);
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
	ExpVal EvalExpression (AActor *self, const PClass *cls);
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
	ExpVal EvalExpression (AActor *self, const PClass *cls);
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
	ExpVal EvalExpression (AActor *self, const PClass *cls);
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
	ExpVal EvalExpression (AActor *self, const PClass *cls);
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
	ExpVal EvalExpression (AActor *self, const PClass *cls);
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
	ExpVal EvalExpression (AActor *self, const PClass *cls);
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
	ExpVal EvalExpression (AActor *self, const PClass *cls);
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

	ExpVal EvalExpression (AActor *self, const PClass *cls);
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

	ExpVal EvalExpression (AActor *self, const PClass *cls);
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

	ExpVal EvalExpression (AActor *self, const PClass *cls);
};

//==========================================================================
//
//
//
//==========================================================================

class FxRandom : public FxExpression
{
	FRandom * rng;
	FxExpression *min, *max;

public:

	FxRandom(FRandom *, FxExpression *mi, FxExpression *ma, const FScriptPosition &pos);
	~FxRandom();
	FxExpression *Resolve(FCompileContext&);

	ExpVal EvalExpression (AActor *self, const PClass *cls);
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

	ExpVal EvalExpression (AActor *self, const PClass *cls);
};


FxExpression *ParseExpression (FScanner &sc, PClass *cls);


#endif
