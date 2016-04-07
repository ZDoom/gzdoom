#include <math.h>
#include "dobject.h"
#include "sc_man.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "m_alloc.h"
#include "zcc_parser.h"
#include "templates.h"

#define luai_nummod(a,b)        ((a) - floor((a)/(b))*(b))

static void FtoD(ZCC_ExprConstant *expr, FSharedStringArena &str_arena);

ZCC_OpInfoType ZCC_OpInfo[PEX_COUNT_OF] =
{
#define xx(a,z)	{ #a, NULL },
#include "zcc_exprlist.h"
};

// Structures used for initializing operator overloads
struct OpProto1
{
	EZCCExprType Op;
	PType **Type;
	EvalConst1op EvalConst;
};

struct OpProto2
{
	EZCCExprType Op;
	PType **Res, **Ltype, **Rtype;
	EvalConst2op EvalConst;
};

static struct FreeOpInfoProtos
{
	~FreeOpInfoProtos()
	{
		for (size_t i = 0; i < countof(ZCC_OpInfo); ++i)
		{
			ZCC_OpInfo[i].FreeAllProtos();
		}
	}
} ProtoFreeer;

void ZCC_OpInfoType::FreeAllProtos()
{
	for (ZCC_OpProto *proto = Protos, *next = NULL; proto != NULL; proto = next)
	{
		next = proto->Next;
		delete proto;
	}
	Protos = NULL;
}

void ZCC_OpInfoType::AddProto(PType *res, PType *optype, EvalConst1op evalconst)
{
	ZCC_OpProto *proto = new ZCC_OpProto(res, optype, NULL);
	proto->EvalConst1 = evalconst;
	proto->Next = Protos;
	Protos = proto;
}

void ZCC_OpInfoType::AddProto(PType *res, PType *ltype, PType *rtype, EvalConst2op evalconst)
{
	assert(ltype != NULL);
	ZCC_OpProto *proto = new ZCC_OpProto(res, ltype, rtype);
	proto->EvalConst2 = evalconst;
	proto->Next = Protos;
	Protos = proto;
}

//==========================================================================
//
// ZCC_OpInfoType :: FindBestProto (Unary)
//
// Finds the "best" prototype for this operand type. Best is defined as the
// one that requires the fewest conversions. Also returns the conversion
// route necessary to get from the input type to the desired type.
//
//==========================================================================

ZCC_OpProto *ZCC_OpInfoType::FindBestProto(PType *optype, const PType::Conversion **route, int &numslots)
{
	assert(optype != NULL);

	const PType::Conversion *routes[2][CONVERSION_ROUTE_SIZE];
	const PType::Conversion **best_route = NULL;
	int cur_route = 0;
	ZCC_OpProto *best_proto = NULL;
	int best_dist = INT_MAX;

	// Find the best prototype.
	for (ZCC_OpProto *proto = Protos; best_dist != 0 && proto != NULL; proto = proto->Next)
	{
		if (proto->Type2 != NULL)
		{ // Not a unary prototype.
			continue;
		}
		int dist = optype->FindConversion(proto->Type1, routes[cur_route], CONVERSION_ROUTE_SIZE);
		if (dist >= 0 && dist < best_dist)
		{
			best_dist = dist;
			best_proto = proto;
			best_route = routes[cur_route];
			cur_route ^= 1;
		}
	}
	// Copy best conversion route to the caller's array.
	if (best_route != NULL && route != NULL && numslots > 0)
	{
		numslots = MIN(numslots, best_dist);
		if (numslots > 0)
		{
			memcpy(route, best_route, sizeof(*route) * numslots);
		}
	}
	return best_proto;
}

//==========================================================================
//
// ZCC_OpInfoType :: FindBestProto (Binary)
//
// Finds the "best" prototype for the given operand types. Here, best is
// defined as the one that requires the fewest conversions for *one* of the
// operands. For prototypes with matching distances, the first one found
// is used. ZCC_InitOperators() initializes the prototypes in order such
// that this will result in the precedences: double > uint > int
//
//==========================================================================

ZCC_OpProto *ZCC_OpInfoType::FindBestProto(
	PType *left, const PType::Conversion **route1, int &numslots1,
	PType *right, const PType::Conversion **route2, int &numslots2)
{
	assert(left != NULL && right != NULL);

	const PType::Conversion *routes[2][2][CONVERSION_ROUTE_SIZE];
	const PType::Conversion **best_route1 = NULL, **best_route2 = NULL;
	int cur_route1 = 0, cur_route2 = 0;
	int best_dist1 = INT_MAX, best_dist2 = INT_MAX;

	ZCC_OpProto *best_proto = NULL;
	int best_low_dist = INT_MAX;

	for (ZCC_OpProto *proto = Protos; best_low_dist != 0 && proto != NULL; proto = proto->Next)
	{
		if (proto->Type2 == NULL)
		{ // Not a binary prototype
			continue;
		}
		int dist1 = left->FindConversion(proto->Type1, routes[0][cur_route1], CONVERSION_ROUTE_SIZE);
		int dist2 = right->FindConversion(proto->Type2, routes[1][cur_route2], CONVERSION_ROUTE_SIZE);
		if (dist1 < 0 || dist2 < 0)
		{ // one or both operator types are unreachable
			continue;
		}
		// Do not count F32->F64 conversions in the distance comparisons. If we do, then
		// [[float32 (op) int]] will choose the integer version instead of the floating point
		// version, which we do not want.
		int test_dist1 = dist1, test_dist2 = dist2;
		if (routes[0][cur_route1][0]->ConvertConstant == FtoD)
		{
			test_dist1--;
		}
		if (routes[1][cur_route2][0]->ConvertConstant == FtoD)
		{
			test_dist2--;
		}
		int dist = MIN(test_dist1, test_dist2);
		if (dist < best_low_dist)
		{
			best_low_dist = dist;
			best_proto = proto;
			best_dist1 = dist1;
			best_dist2 = dist2;
			best_route1 = routes[0][cur_route1];
			best_route2 = routes[1][cur_route2];
			cur_route1 ^= 1;
			cur_route2 ^= 1;
		}
	}
	// Copy best conversion route to the caller's arrays.
	if (best_route1 != NULL && route1 != NULL && numslots1 > 0)
	{
		numslots1 = MIN(numslots1, best_dist1);
		if (numslots1 > 0)
		{
			memcpy(route1, best_route1, sizeof(*route1) * numslots1);
		}
	}
	if (best_route2 != NULL && route2 != NULL && numslots2 > 0)
	{
		numslots2 = MIN(numslots2, best_dist2);
		if (numslots2 > 0)
		{
			memcpy(route2, best_route2, sizeof(*route2) * numslots2);
		}
	}
	return best_proto;
}

static ZCC_ExprConstant *EvalIdentity(ZCC_ExprConstant *val)
{
	return val;
}


static ZCC_ExprConstant *EvalConcat(ZCC_ExprConstant *l, ZCC_ExprConstant *r, FSharedStringArena &strings)
{
	FString str = *l->StringVal + *r->StringVal;
	l->StringVal = strings.Alloc(str);
	return l;
}

static ZCC_ExprConstant *EvalLTGTEQSInt32(ZCC_ExprConstant *l, ZCC_ExprConstant *r, FSharedStringArena &)
{
	l->IntVal = l->IntVal < r->IntVal ? -1 : l->IntVal == r->IntVal ? 0 : 1;
	return l;
}

static ZCC_ExprConstant *EvalLTGTEQUInt32(ZCC_ExprConstant *l, ZCC_ExprConstant *r, FSharedStringArena &)
{
	l->IntVal = l->UIntVal < r->UIntVal ? -1 : l->UIntVal == r->UIntVal ? 0 : 1;
	l->Type = TypeSInt32;
	return l;
}

static ZCC_ExprConstant *EvalLTGTEQFloat64(ZCC_ExprConstant *l, ZCC_ExprConstant *r, FSharedStringArena &)
{
	l->IntVal = l->DoubleVal < r->DoubleVal ? -1 : l->DoubleVal == r->DoubleVal ? 0 : 1;
	l->Type = TypeSInt32;
	return l;
}

void ZCC_InitOperators()
{
	// Prototypes are added from lowest to highest conversion precedence.

	// Unary operators
	static const OpProto1 UnaryOpInit[] =
	{
		{ PEX_PostInc	 , (PType **)&TypeSInt32,  EvalIdentity },
		{ PEX_PostInc	 , (PType **)&TypeUInt32,  EvalIdentity },
		{ PEX_PostInc	 , (PType **)&TypeFloat64, EvalIdentity },

		{ PEX_PostDec	 , (PType **)&TypeSInt32,  EvalIdentity },
		{ PEX_PostDec	 , (PType **)&TypeUInt32,  EvalIdentity },
		{ PEX_PostDec	 , (PType **)&TypeFloat64, EvalIdentity },

		{ PEX_PreInc	 , (PType **)&TypeSInt32,  [](auto *val) { val->IntVal += 1; return val; } },
		{ PEX_PreInc	 , (PType **)&TypeUInt32,  [](auto *val) { val->UIntVal += 1; return val; } },
		{ PEX_PreInc	 , (PType **)&TypeFloat64, [](auto *val) { val->DoubleVal += 1; return val; } },

		{ PEX_PreDec	 , (PType **)&TypeSInt32,  [](auto *val) { val->IntVal -= 1; return val; } },
		{ PEX_PreDec	 , (PType **)&TypeUInt32,  [](auto *val) { val->UIntVal -= 1; return val; } },
		{ PEX_PreDec	 , (PType **)&TypeFloat64, [](auto *val) { val->DoubleVal -= 1; return val; } },

		{ PEX_Negate	 , (PType **)&TypeSInt32,  [](auto *val) { val->IntVal = -val->IntVal; return val; } },
		{ PEX_Negate	 , (PType **)&TypeFloat64, [](auto *val) { val->DoubleVal = -val->DoubleVal; return val; } },

		{ PEX_AntiNegate , (PType **)&TypeSInt32,  EvalIdentity },
		{ PEX_AntiNegate , (PType **)&TypeUInt32,  EvalIdentity },
		{ PEX_AntiNegate , (PType **)&TypeFloat64, EvalIdentity },

		{ PEX_BitNot	 , (PType **)&TypeSInt32,  [](auto *val) { val->IntVal = ~val->IntVal; return val; } },
		{ PEX_BitNot	 , (PType **)&TypeUInt32,  [](auto *val) { val->UIntVal = ~val->UIntVal; return val; } },

		{ PEX_BoolNot	 , (PType **)&TypeBool,    [](auto *val) { val->IntVal = !val->IntVal; return val; } },
	};
	for (size_t i = 0; i < countof(UnaryOpInit); ++i)
	{
		ZCC_OpInfo[UnaryOpInit[i].Op].AddProto(*UnaryOpInit[i].Type, *UnaryOpInit[i].Type, UnaryOpInit[i].EvalConst);
	}

	// Binary operators
	static const OpProto2 BinaryOpInit[] =
	{
		{ PEX_Add		 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal += r->IntVal; return l; } },
		{ PEX_Add		 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal += r->UIntVal; return l; } },
		{ PEX_Add		 , (PType **)&TypeFloat64, (PType **)&TypeFloat64, (PType **)&TypeFloat64, [](auto *l, auto *r, auto &) { l->DoubleVal += r->DoubleVal; return l; } },

		{ PEX_Sub		 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal -= r->IntVal; return l; } },
		{ PEX_Sub		 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal -= r->UIntVal; return l; } },
		{ PEX_Sub		 , (PType **)&TypeFloat64, (PType **)&TypeFloat64, (PType **)&TypeFloat64, [](auto *l, auto *r, auto &) { l->DoubleVal -= r->DoubleVal; return l; } },

		{ PEX_Mul		 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal *= r->IntVal; return l; } },
		{ PEX_Mul		 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal *= r->UIntVal; return l; } },
		{ PEX_Mul		 , (PType **)&TypeFloat64, (PType **)&TypeFloat64, (PType **)&TypeFloat64, [](auto *l, auto *r, auto &) { l->DoubleVal *= r->DoubleVal; return l; } },

		{ PEX_Div		 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal /= r->IntVal; return l; } },
		{ PEX_Div		 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal /= r->UIntVal; return l; } },
		{ PEX_Div		 , (PType **)&TypeFloat64, (PType **)&TypeFloat64, (PType **)&TypeFloat64, [](auto *l, auto *r, auto &) { l->DoubleVal /= r->DoubleVal; return l; } },

		{ PEX_Mod		 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal %= r->IntVal; return l; } },
		{ PEX_Mod		 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal %= r->UIntVal; return l; } },
		{ PEX_Mod		 , (PType **)&TypeFloat64, (PType **)&TypeFloat64, (PType **)&TypeFloat64, [](auto *l, auto *r, auto &) { l->DoubleVal = luai_nummod(l->DoubleVal, r->DoubleVal); return l; } },

		{ PEX_Pow		 , (PType **)&TypeFloat64, (PType **)&TypeFloat64, (PType **)&TypeFloat64, [](auto *l, auto *r, auto &) { l->DoubleVal = pow(l->DoubleVal, r->DoubleVal); return l; } },

		{ PEX_Concat	 , (PType **)&TypeString,  (PType **)&TypeString,  (PType **)&TypeString,  EvalConcat },

		{ PEX_BitAnd	 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal &= r->IntVal; return l; } },
		{ PEX_BitAnd	 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal &= r->UIntVal; return l; } },

		{ PEX_BitOr		 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal |= r->IntVal; return l; } },
		{ PEX_BitOr		 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal |= r->UIntVal; return l; } },

		{ PEX_BitXor	 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal ^= r->IntVal; return l; } },
		{ PEX_BitXor	 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal ^= r->UIntVal; return l; } },

		{ PEX_BoolAnd	 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->IntVal && r->IntVal; l->Type = TypeBool; return l; } },
		{ PEX_BoolAnd	 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->UIntVal && r->UIntVal; l->Type = TypeBool; return l;  } },

		{ PEX_BoolOr	 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->IntVal || r->IntVal; l->Type = TypeBool; return l; } },
		{ PEX_BoolOr	 , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->UIntVal || r->UIntVal; l->Type = TypeBool; return l; } },

		{ PEX_LeftShift  , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->IntVal <<= r->UIntVal; return l; } },
		{ PEX_LeftShift  , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal <<= r->UIntVal; return l; } },

		{ PEX_RightShift , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->IntVal >>= r->UIntVal; return l; } },
		{ PEX_RightShift , (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->UIntVal >>= r->UIntVal; return l; } },

		{ PEX_LT		 , (PType **)&TypeBool,    (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->IntVal < r->IntVal; l->Type = TypeBool; return l; } },
		{ PEX_LT		 , (PType **)&TypeBool,    (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->UIntVal < r->UIntVal; l->Type = TypeBool; return l; } },
		{ PEX_LT		 , (PType **)&TypeBool,    (PType **)&TypeFloat64, (PType **)&TypeFloat64, [](auto *l, auto *r, auto &) { l->IntVal = l->DoubleVal < r->DoubleVal; l->Type = TypeBool; return l; } },

		{ PEX_LTEQ		 , (PType **)&TypeBool,    (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->IntVal <= r->IntVal; l->Type = TypeBool; return l; } },
		{ PEX_LTEQ		 , (PType **)&TypeBool,    (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->UIntVal <= r->UIntVal; l->Type = TypeBool; return l; } },
		{ PEX_LTEQ		 , (PType **)&TypeBool,    (PType **)&TypeFloat64, (PType **)&TypeFloat64, [](auto *l, auto *r, auto &) { l->IntVal = l->DoubleVal <= r->DoubleVal; l->Type = TypeBool; return l; } },

		{ PEX_EQEQ		 , (PType **)&TypeBool,    (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->IntVal == r->IntVal; l->Type = TypeBool; return l; } },
		{ PEX_EQEQ		 , (PType **)&TypeBool,    (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  [](auto *l, auto *r, auto &) { l->IntVal = l->UIntVal == r->UIntVal; l->Type = TypeBool; return l; } },
		{ PEX_EQEQ		 , (PType **)&TypeBool,    (PType **)&TypeFloat64, (PType **)&TypeFloat64, [](auto *l, auto *r, auto &) { l->IntVal = l->DoubleVal == r->DoubleVal; l->Type = TypeBool; return l; } },

		{ PEX_LTGTEQ	 , (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  (PType **)&TypeSInt32,  EvalLTGTEQSInt32 },
		{ PEX_LTGTEQ	 , (PType **)&TypeSInt32,  (PType **)&TypeUInt32,  (PType **)&TypeUInt32,  EvalLTGTEQUInt32 },
		{ PEX_LTGTEQ	 , (PType **)&TypeSInt32,  (PType **)&TypeFloat64, (PType **)&TypeFloat64, EvalLTGTEQFloat64 },
	};
	for (size_t i = 0; i < countof(BinaryOpInit); ++i)
	{
		ZCC_OpInfo[BinaryOpInit[i].Op].AddProto(*BinaryOpInit[i].Res, *BinaryOpInit[i].Ltype, *BinaryOpInit[i].Rtype, BinaryOpInit[i].EvalConst);
	}
}

static void IntToS32(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	// Integers always fill out the full sized 32-bit field, so converting
	// from a smaller sized integer to a 32-bit one is as simple as changing
	// the type field.
	expr->Type = TypeSInt32;
}

static void S32toS8(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	expr->IntVal = ((expr->IntVal << 24) >> 24);
	expr->Type = TypeSInt8;
}

static void S32toS16(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	expr->IntVal = ((expr->IntVal << 16) >> 16);
	expr->Type = TypeSInt16;
}

static void S32toU8(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	expr->IntVal &= 0xFF;
	expr->Type = TypeUInt8;
}

static void S32toU16(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	expr->IntVal &= 0xFFFF;
	expr->Type = TypeUInt16;
}

static void S32toU32(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	expr->Type = TypeUInt32;
}

static void S32toD(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	expr->DoubleVal = expr->IntVal;
	expr->Type = TypeFloat64;
}

static void DtoS32(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	expr->IntVal = (int)expr->DoubleVal;
	expr->Type = TypeSInt32;
}

static void U32toD(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	expr->DoubleVal = expr->UIntVal;
	expr->Type = TypeFloat64;
}

static void DtoU32(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	expr->UIntVal = (unsigned int)expr->DoubleVal;
	expr->Type = TypeUInt32;
}

static void FtoD(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	// Constant single precision numbers are stored as doubles.
	assert(expr->Type == TypeFloat32);
	expr->Type = TypeFloat64;
}

static void DtoF(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	// Truncate double precision to single precision.
	float poop = (float)expr->DoubleVal;
	expr->DoubleVal = poop;
	expr->Type = TypeFloat32;
}

static void S32toS(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	char str[16];
	int len = mysnprintf(str, countof(str), "%i", expr->IntVal);
	expr->StringVal = str_arena.Alloc(str, len);
	expr->Type = TypeString;
}

static void U32toS(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	char str[16];
	int len = mysnprintf(str, countof(str), "%u", expr->UIntVal);
	expr->StringVal = str_arena.Alloc(str, len);
	expr->Type = TypeString;
}

static void DtoS(ZCC_ExprConstant *expr, FSharedStringArena &str_arena)
{
	// Convert to a string with enough precision such that converting
	// back to a double will not lose any data.
	char str[64];
	IGNORE_FORMAT_PRE
	int len = mysnprintf(str, countof(str), "%H", expr->DoubleVal);
	IGNORE_FORMAT_POST
	expr->StringVal = str_arena.Alloc(str, len);
	expr->Type = TypeString;
}

//==========================================================================
//
// ZCC_InitConversions
//
//==========================================================================

void ZCC_InitConversions()
{
	TypeUInt8->AddConversion(TypeSInt32, IntToS32);
	TypeSInt8->AddConversion(TypeSInt32, IntToS32);
	TypeUInt16->AddConversion(TypeSInt32, IntToS32);
	TypeSInt16->AddConversion(TypeSInt32, IntToS32);

	TypeUInt32->AddConversion(TypeSInt32, IntToS32);
	TypeUInt32->AddConversion(TypeFloat64, U32toD);
	TypeUInt32->AddConversion(TypeString, U32toS);

	TypeSInt32->AddConversion(TypeUInt8, S32toU8);
	TypeSInt32->AddConversion(TypeSInt8, S32toS8);
	TypeSInt32->AddConversion(TypeSInt16, S32toS16);
	TypeSInt32->AddConversion(TypeUInt16, S32toU16);
	TypeSInt32->AddConversion(TypeUInt32, S32toU32);
	TypeSInt32->AddConversion(TypeFloat64, S32toD);
	TypeSInt32->AddConversion(TypeString, S32toS);

	TypeFloat32->AddConversion(TypeFloat64, FtoD);

	TypeFloat64->AddConversion(TypeUInt32, DtoU32);
	TypeFloat64->AddConversion(TypeSInt32, DtoS32);
	TypeFloat64->AddConversion(TypeFloat32, DtoF);
	TypeFloat64->AddConversion(TypeString, DtoS);
}
