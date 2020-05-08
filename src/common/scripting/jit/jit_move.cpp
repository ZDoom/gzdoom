
#include "jitintern.h"
#include "v_video.h"
#include "s_soundinternal.h"
#include "texturemanager.h"

void JitCompiler::EmitMOVE()
{
	StoreD(LoadD(B), A);
}
void JitCompiler::EmitMOVEF()
{
	StoreF(LoadF(B), A);
}

void JitCompiler::EmitMOVES()
{
	cc.CreateCall(GetNativeFunc<void, FString*, FString*>("__CallAssignString", &JitCompiler::CallAssignString), { LoadS(A), LoadS(B) });
}

void JitCompiler::EmitMOVEA()
{
	StoreA(LoadA(B), A);
}

void JitCompiler::EmitMOVEV2()
{
	StoreF(LoadF(B), A);
	StoreF(LoadF(B + 1), A + 1);
}

void JitCompiler::EmitMOVEV3()
{
	StoreF(LoadF(B), A);
	StoreF(LoadF(B + 1), A + 1);
	StoreF(LoadF(B + 2), A + 2);
}

static void CastI2S(FString *a, int b) { a->Format("%d", b); }
static void CastU2S(FString *a, int b) { a->Format("%u", b); }
static void CastF2S(FString *a, double b) { a->Format("%.5f", b); }
static void CastV22S(FString *a, double b, double b1) { a->Format("(%.5f, %.5f)", b, b1); }
static void CastV32S(FString *a, double b, double b1, double b2) { a->Format("(%.5f, %.5f, %.5f)", b, b1, b2); }
static void CastP2S(FString *a, void *b) { if (b == nullptr) *a = "null"; else a->Format("%p", b); }
static int CastS2I(FString *b) { return (int)b->ToLong(); }
static double CastS2F(FString *b) { return b->ToDouble(); }
static int CastS2N(FString *b) { return b->Len() == 0 ? NAME_None : FName(*b).GetIndex(); }
static void CastN2S(FString *a, int b) { FName name = FName(ENamedName(b)); *a = name.IsValidName() ? name.GetChars() : ""; }
static int CastS2Co(FString *b) { return V_GetColor(nullptr, *b); }
static void CastCo2S(FString *a, int b) { PalEntry c(b); a->Format("%02x %02x %02x", c.r, c.g, c.b); }
static int CastS2So(FString *b) { return FSoundID(*b); }
static void CastSo2S(FString* a, int b) { *a = soundEngine->GetSoundName(b); }
static void CastSID2S(FString* a, unsigned int b) { VM_CastSpriteIDToString(a, b); }
static void CastTID2S(FString *a, int b) { auto tex = TexMan.GetTexture(*(FTextureID*)&b); *a = (tex == nullptr) ? "(null)" : tex->GetName().GetChars(); }

void JitCompiler::EmitCAST()
{
	switch (C)
	{
	case CAST_I2F:
		StoreF(cc.CreateSIToFP(LoadD(B), ircontext->getDoubleTy()), A);
		break;
	case CAST_U2F:
		StoreF(cc.CreateUIToFP(LoadD(B), ircontext->getDoubleTy()), A);
		break;
	case CAST_F2I:
		StoreD(cc.CreateFPToSI(LoadF(B), ircontext->getInt32Ty()), A);
		break;
	case CAST_F2U:
		StoreD(cc.CreateFPToUI(LoadF(B), ircontext->getInt32Ty()), A);
		break;
	case CAST_I2S:
		cc.CreateCall(GetNativeFunc<void, FString*, int>("__CastI2S", CastI2S), { LoadS(A), LoadD(B) });
		break;
	case CAST_U2S:
		cc.CreateCall(GetNativeFunc<void, FString*, int>("__CastU2S", CastU2S), { LoadS(A), LoadD(B) });
		break;
	case CAST_F2S:
		cc.CreateCall(GetNativeFunc<void, FString*, double>("__CastF2S", CastF2S), { LoadS(A), LoadF(B) });
		break;
	case CAST_V22S:
		cc.CreateCall(GetNativeFunc<void, FString*, double, double>("__CastV22S", CastV22S), { LoadS(A), LoadF(B), LoadF(B + 1) });
		break;
	case CAST_V32S:
		cc.CreateCall(GetNativeFunc<void, FString*, double, double, double>("__CastV32S", CastV32S), { LoadS(A), LoadF(B), LoadF(B + 1), LoadF(B + 2) });
		break;
	case CAST_P2S:
		cc.CreateCall(GetNativeFunc<void, FString*, void*>("__CastP2S", CastP2S), { LoadS(A), LoadA(B) });
		break;
	case CAST_S2I:
		StoreD(cc.CreateCall(GetNativeFunc<int, FString*>("__CastS2I", CastS2I), { LoadS(B) }), A);
		break;
	case CAST_S2F:
		StoreF(cc.CreateCall(GetNativeFunc<double, FString*>("__CastS2F", CastS2F), { LoadS(B) }), A);
		break;
	case CAST_S2N:
		StoreD(cc.CreateCall(GetNativeFunc<int, FString*>("__CastS2N", CastS2N), { LoadS(B) }), A);
		break;
	case CAST_N2S:
		cc.CreateCall(GetNativeFunc<void, FString*, int>("__CastN2S", CastN2S), { LoadS(A), LoadD(B) });
		break;
	case CAST_S2Co:
		StoreD(cc.CreateCall(GetNativeFunc<int, FString*>("__CastS2Co", CastS2Co), { LoadS(B) }), A);
		break;
	case CAST_Co2S:
		cc.CreateCall(GetNativeFunc<void, FString*, int>("__CastCo2S", CastCo2S), { LoadS(A), LoadD(B) });
		break;
	case CAST_S2So:
		StoreD(cc.CreateCall(GetNativeFunc<int, FString*>("__CastS2So", CastS2So), { LoadS(B) }), A);
		break;
	case CAST_So2S:
		cc.CreateCall(GetNativeFunc<void, FString*, int>("__CastSo2S", CastSo2S), { LoadS(A), LoadD(B) });
		break;
	case CAST_SID2S:
		cc.CreateCall(GetNativeFunc<void, FString*, unsigned int>("__CastSID2S", CastSID2S), { LoadS(A), LoadD(B) });
		break;
	case CAST_TID2S:
		cc.CreateCall(GetNativeFunc<void, FString*, int>("__CastTID2S", CastTID2S), { LoadS(A), LoadD(B) });
		break;
	default:
		I_Error("Unknown OP_CAST type\n");
	}
}

static int CastB_S(FString *s) { return s->Len() > 0; }

void JitCompiler::EmitCASTB()
{
	if (C == CASTB_I)
	{
		StoreD(cc.CreateZExt(cc.CreateICmpNE(LoadD(B), ConstValueD(0)), ircontext->getInt32Ty()), A);
	}
	else if (C == CASTB_F)
	{
		StoreD(cc.CreateZExt(cc.CreateFCmpUNE(LoadF(B), ConstValueF(0.0)), ircontext->getInt32Ty()), A);
	}
	else if (C == CASTB_A)
	{
		StoreD(cc.CreateZExt(cc.CreateICmpNE(LoadA(B), ConstValueA(0)), ircontext->getInt32Ty()), A);
	}
	else
	{
		StoreD(cc.CreateCall(GetNativeFunc<int, FString* >("__CastB_S", CastB_S), { LoadS(B) }), A);
	}
}

static DObject *DynCast(DObject *obj, PClass *cls)
{
	return (obj && obj->IsKindOf(cls)) ? obj : nullptr;
}

void JitCompiler::EmitDYNCAST_R()
{
	StoreA(cc.CreateCall(GetNativeFunc<DObject*, DObject*, PClass*>("__DynCast", DynCast), { LoadA(B), LoadA(C) }), A);
}

void JitCompiler::EmitDYNCAST_K()
{
	StoreA(cc.CreateCall(GetNativeFunc<DObject*, DObject*, PClass*>("__DynCast", DynCast), { LoadA(B), ConstA(C) }), A);
}

static PClass *DynCastC(PClass *cls1, PClass *cls2)
{
	return (cls1 && cls1->IsDescendantOf(cls2)) ? cls1 : nullptr;
}

void JitCompiler::EmitDYNCASTC_R()
{
	StoreA(cc.CreateCall(GetNativeFunc<PClass*, PClass*, PClass*>("__DynCastC", DynCastC), { LoadA(B), LoadA(C) }), A);
}

void JitCompiler::EmitDYNCASTC_K()
{
	StoreA(cc.CreateCall(GetNativeFunc<PClass*, PClass*, PClass*>("__DynCastC", DynCastC), { LoadA(B), ConstA(C) }), A);
}
