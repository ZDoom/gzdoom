
#include "jitintern.h"
#include "v_video.h"
#include "s_soundinternal.h"
#include "texturemanager.h"
#include "palutil.h"

void JitCompiler::EmitMOVE()
{
	cc.mov(regD[A], regD[B]);
}
void JitCompiler::EmitMOVEF()
{
	cc.movsd(regF[A], regF[B]);
}

void JitCompiler::EmitMOVES()
{
	auto call = CreateCall<void, FString*, FString*>(&JitCompiler::CallAssignString);
	call->setArg(0, regS[A]);
	call->setArg(1, regS[B]);
}

void JitCompiler::EmitMOVEA()
{
	cc.mov(regA[A], regA[B]);
}

void JitCompiler::EmitMOVEV2()
{
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
}

void JitCompiler::EmitMOVEV3()
{
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
}

void JitCompiler::EmitMOVEV4()
{
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.movsd(regF[A + 3], regF[B + 3]);
}

static void CastI2S(FString *a, int b) { a->Format("%d", b); }
static void CastU2S(FString *a, int b) { a->Format("%u", b); }
static void CastF2S(FString *a, double b) { a->Format("%.5f", b); }
static void CastV22S(FString *a, double b, double b1) { a->Format("(%.5f, %.5f)", b, b1); }
static void CastV32S(FString *a, double b, double b1, double b2) { a->Format("(%.5f, %.5f, %.5f)", b, b1, b2); }
static void CastV42S(FString *a, double b, double b1, double b2, double b3) { a->Format("(%.5f, %.5f, %.5f, %.5f)", b, b1, b2, b3); }
static void CastP2S(FString *a, void *b) { if (b == nullptr) *a = "null"; else a->Format("%p", b); }
static int CastS2I(FString *b) { return (int)b->ToLong(); }
static double CastS2F(FString *b) { return b->ToDouble(); }
static int CastS2N(FString *b) { return b->Len() == 0 ? NAME_None : FName(*b).GetIndex(); }
static void CastN2S(FString *a, int b) { FName name = FName(ENamedName(b)); *a = name.IsValidName() ? name.GetChars() : ""; }
static int CastS2Co(FString *b) { return V_GetColor(*b); }
static void CastCo2S(FString *a, int b) { PalEntry c(b); a->Format("%02x %02x %02x", c.r, c.g, c.b); }
static int CastS2So(FString *b) { return S_FindSound(*b).index(); }
static void CastSo2S(FString* a, int b) { *a = soundEngine->GetSoundName(FSoundID::fromInt(b)); }
static void CastSID2S(FString* a, unsigned int b) { VM_CastSpriteIDToString(a, b); }
static void CastTID2S(FString *a, int b) { auto tex = TexMan.GetGameTexture(*(FTextureID*)&b); *a = (tex == nullptr) ? "(null)" : tex->GetName().GetChars(); }

void JitCompiler::EmitCAST()
{
	asmjit::X86Gp tmp, resultD;
	asmjit::X86Xmm resultF;
	asmjit::CCFuncCall *call = nullptr;

	switch (C)
	{
	case CAST_I2F:
		cc.cvtsi2sd(regF[A], regD[B]);
		break;
	case CAST_U2F:
		tmp = newTempInt64();
		cc.xor_(tmp, tmp);
		cc.mov(tmp.r32(), regD[B]);
		cc.cvtsi2sd(regF[A], tmp);
		break;
	case CAST_F2I:
		cc.cvttsd2si(regD[A], regF[B]);
		break;
	case CAST_F2U:
		tmp = newTempInt64();
		cc.cvttsd2si(tmp, regF[B]);
		cc.mov(regD[A], tmp.r32());
		break;
	case CAST_I2S:
		call = CreateCall<void, FString*, int>(CastI2S);
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_U2S:
		call = CreateCall<void, FString*, int>(CastU2S);
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_F2S:
		call = CreateCall<void, FString*, double>(CastF2S);
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		break;
	case CAST_V22S:
		call = CreateCall<void, FString*, double, double>(CastV22S);
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		call->setArg(2, regF[B + 1]);
		break;
	case CAST_V32S:
		call = CreateCall<void, FString*, double, double, double>(CastV32S);
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		call->setArg(2, regF[B + 1]);
		call->setArg(3, regF[B + 2]);
		break;
	case CAST_V42S:
		call = CreateCall<void, FString*, double, double, double>(CastV42S);
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		call->setArg(2, regF[B + 1]);
		call->setArg(3, regF[B + 2]);
		call->setArg(4, regF[B + 3]);
		break;
	case CAST_P2S:
		call = CreateCall<void, FString*, void*>(CastP2S);
		call->setArg(0, regS[A]);
		call->setArg(1, regA[B]);
		break;
	case CAST_S2I:
		resultD = newResultInt32();
		call = CreateCall<int, FString*>(CastS2I);
		call->setRet(0, resultD);
		call->setArg(0, regS[B]);
		cc.mov(regD[A], resultD);
		break;
	case CAST_S2F:
		resultF = newResultXmmSd();
		call = CreateCall<double, FString*>(CastS2F);
		call->setRet(0, resultF);
		call->setArg(0, regS[B]);
		cc.movsd(regF[A], resultF);
		break;
	case CAST_S2N:
		resultD = newResultInt32();
		call = CreateCall<int, FString*>(CastS2N);
		call->setRet(0, resultD);
		call->setArg(0, regS[B]);
		cc.mov(regD[A], resultD);
		break;
	case CAST_N2S:
		call = CreateCall<void, FString*, int>(CastN2S);
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_S2Co:
		resultD = newResultInt32();
		call = CreateCall<int, FString*>(CastS2Co);
		call->setRet(0, resultD);
		call->setArg(0, regS[B]);
		cc.mov(regD[A], resultD);
		break;
	case CAST_Co2S:
		call = CreateCall<void, FString*, int>(CastCo2S);
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_S2So:
		resultD = newResultInt32();
		call = CreateCall<int, FString*>(CastS2So);
		call->setRet(0, resultD);
		call->setArg(0, regS[B]);
		cc.mov(regD[A], resultD);
		break;
	case CAST_So2S:
		call = CreateCall<void, FString*, int>(CastSo2S);
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_SID2S:
		call = CreateCall<void, FString*, unsigned int>(CastSID2S);
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_TID2S:
		call = CreateCall<void, FString*, int>(CastTID2S);
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
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
		cc.cmp(regD[B], (int)0);
		cc.setne(regD[A]);
		cc.movzx(regD[A], regD[A].r8Lo()); // not sure if this is needed
	}
	else if (C == CASTB_F)
	{
		auto zero = newTempXmmSd();
		auto one = newTempInt32();
		cc.xorpd(zero, zero);
		cc.mov(one, 1);
		cc.xor_(regD[A], regD[A]);
		cc.ucomisd(regF[B], zero);
		cc.setp(regD[A]);
		cc.cmovne(regD[A], one);
	}
	else if (C == CASTB_A)
	{
		cc.test(regA[B], regA[B]);
		cc.setne(regD[A]);
		cc.movzx(regD[A], regD[A].r8Lo()); // not sure if this is needed
	}
	else
	{
		auto result = newResultInt32();
		auto call = CreateCall<int, FString*>(CastB_S);
		call->setRet(0, result);
		call->setArg(0, regS[B]);
		cc.mov(regD[A], result);
	}
}

static DObject *DynCast(DObject *obj, PClass *cls)
{
	return (obj && obj->IsKindOf(cls)) ? obj : nullptr;
}

void JitCompiler::EmitDYNCAST_R()
{
	auto result = newResultIntPtr();
	auto call = CreateCall<DObject*, DObject*, PClass*>(DynCast);
	call->setRet(0, result);
	call->setArg(0, regA[B]);
	call->setArg(1, regA[C]);
	cc.mov(regA[A], result);
}

void JitCompiler::EmitDYNCAST_K()
{
	auto result = newResultIntPtr();
	auto c = newTempIntPtr();
	cc.mov(c, asmjit::imm_ptr(konsta[C].o));
	auto call = CreateCall<DObject*, DObject*, PClass*>(DynCast);
	call->setRet(0, result);
	call->setArg(0, regA[B]);
	call->setArg(1, c);
	cc.mov(regA[A], result);
}

static PClass *DynCastC(PClass *cls1, PClass *cls2)
{
	return (cls1 && cls1->IsDescendantOf(cls2)) ? cls1 : nullptr;
}

void JitCompiler::EmitDYNCASTC_R()
{
	auto result = newResultIntPtr();
	auto call = CreateCall<PClass*, PClass*, PClass*>(DynCastC);
	call->setRet(0, result);
	call->setArg(0, regA[B]);
	call->setArg(1, regA[C]);
	cc.mov(regA[A], result);
}

void JitCompiler::EmitDYNCASTC_K()
{
	using namespace asmjit;
	auto result = newResultIntPtr();
	auto c = newTempIntPtr();
	cc.mov(c, asmjit::imm_ptr(konsta[C].o));
	typedef PClass*(*FuncPtr)(PClass*, PClass*);
	auto call = CreateCall<PClass*, PClass*, PClass*>(DynCastC);
	call->setRet(0, result);
	call->setArg(0, regA[B]);
	call->setArg(1, c);
	cc.mov(regA[A], result);
}
