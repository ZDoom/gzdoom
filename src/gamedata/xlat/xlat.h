#ifndef __XLAT_H
#define __XLAT_H

#include "doomtype.h"
#include "tarray.h"

enum ELineTransArgOp
{
	ARGOP_Const,
	ARGOP_Tag,
	ARGOP_Expr,

	TAGOP_NUMBITS = 2,
	TAGOP_MASK = (1 << TAGOP_NUMBITS) - 1
};

enum
{
	LINETRANS_MAXARGS	= 5,
	LINETRANS_TAGSHIFT	= 30 - LINETRANS_MAXARGS * TAGOP_NUMBITS,
};

enum
{
	XEXP_Const,
	XEXP_Tag,
	XEXP_Add,
	XEXP_Sub,
	XEXP_Mul,
	XEXP_Div,
	XEXP_Mod,
	XEXP_And,
	XEXP_Or,
	XEXP_Xor,
	XEXP_Neg,

	XEXP_COUNT
};

struct FXlatExprState;

extern const int* (*XlatExprEval[XEXP_COUNT])(int *dest, const int *xnode, FXlatExprState *state);

struct FLineTrans
{
	int special;
	int flags;
	int args[5];

	FLineTrans()
	{
		special = flags = args[0] = args[1] = args[2] = args[3] = args[4] = 0;
	}
	FLineTrans(int _special, int _flags, int arg1, int arg2, int arg3, int arg4, int arg5)
	{
		special = _special;
		flags = _flags;
		args[0] = arg1;
		args[1] = arg2;
		args[2] = arg3;
		args[3] = arg4;
		args[4] = arg5;
	}
};


#define MAX_BOOMISH			16

struct FBoomArg
{
	bool bOrExisting;
	bool bUseConstant;
	uint8_t ListSize;
	uint8_t ArgNum;
	uint8_t ConstantValue;
	uint16_t AndValue;
	uint16_t ResultFilter[15];
	uint8_t ResultValue[15];
};

struct FBoomTranslator
{
	uint16_t FirstLinetype = 0;
	uint16_t LastLinetype = 0;
	uint8_t NewSpecial = 0;
	TArray<FBoomArg> Args;
} ;

struct FSectorTrans
{
	int newtype;
	bool bitmask_allowed;

	FSectorTrans(int t=0, bool bitmask = false)
	{
		newtype = t;
		bitmask_allowed = bitmask;
	}
};

struct FSectorMask
{
	int mask;
	int op;
	int shift;
};

struct FLineFlagTrans
{
	int newvalue;
	bool ismask;
};

struct FXlatExprState
{
	int linetype;
	int tag;
	bool bIsConstant;
};


struct FTranslator
{
	bool loaded = false;
	TAutoGrowArray<FLineTrans> SimpleLineTranslations;
	TArray<int> XlatExpressions;
	FBoomTranslator Boomish[MAX_BOOMISH];
	int NumBoomish = 0;
	TAutoGrowArray<FSectorTrans> SectorTranslations;
	TArray<FSectorMask> SectorMasks;
	FLineFlagTrans LineFlagTranslations[16] = {};
};

FTranslator *P_LoadTranslator(const char *lumpname);

#endif
