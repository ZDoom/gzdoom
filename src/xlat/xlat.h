#ifndef __XLAT_H
#define __XLAT_H

#include "doomtype.h"
#include "tarray.h"

enum
{
	LINETRANS_HASTAGAT1	= (1<<5),		// (tag, x, x, x, x)
	LINETRANS_HASTAGAT2	= (2<<5),		// (x, tag, x, x, x)
	LINETRANS_HASTAGAT3	= (3<<5),		// (x, x, tag, x, x)
	LINETRANS_HASTAGAT4	= (4<<5),		// (x, x, x, tag, x)
	LINETRANS_HASTAGAT5	= (5<<5),		// (x, x, x, x, tag)

	LINETRANS_HAS2TAGS	= (7<<5),		// (tag, tag, x, x, x)
};

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
	BYTE ListSize;
	BYTE ArgNum;
	BYTE ConstantValue;
	WORD AndValue;
	WORD ResultFilter[15];
	BYTE ResultValue[15];
};

struct FBoomTranslator
{
	WORD FirstLinetype;
	WORD LastLinetype;
	BYTE NewSpecial;
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

extern TAutoGrowArray<FLineTrans> SimpleLineTranslations;
extern FBoomTranslator Boomish[MAX_BOOMISH];
extern int NumBoomish;
extern TAutoGrowArray<FSectorTrans> SectorTranslations;
extern TArray<FSectorMask> SectorMasks;

#endif
