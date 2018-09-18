
#include "jitintern.h"
#include "v_video.h"
#include "s_sound.h"
#include "r_state.h"

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

void JitCompiler::EmitCAST()
{
	asmjit::X86Gp tmp;
	asmjit::CCFuncCall *call = nullptr;

	switch (C)
	{
	case CAST_I2F:
		cc.cvtsi2sd(regF[A], regD[B]);
		break;
	case CAST_U2F:
		tmp = cc.newInt64();
		cc.xor_(tmp, tmp);
		cc.mov(tmp.r32(), regD[B]);
		cc.cvtsi2sd(regF[A], tmp);
		break;
	case CAST_F2I:
		cc.cvttsd2si(regD[A], regF[B]);
		break;
	case CAST_F2U:
		tmp = cc.newInt64();
		cc.cvttsd2si(tmp, regF[B]);
		cc.mov(regD[A], tmp.r32());
		break;
	case CAST_I2S:
		call = CreateCall<void, FString*, int>([](FString *a, int b) { a->Format("%d", b); });
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_U2S:
		call = CreateCall<void, FString*, int>([](FString *a, int b) { a->Format("%u", b); });
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_F2S:
		call = CreateCall<void, FString*, double>([](FString *a, double b) { a->Format("%.5f", b); });
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		break;
	case CAST_V22S:
		call = CreateCall<void, FString*, double, double>([](FString *a, double b, double b1) { a->Format("(%.5f, %.5f)", b, b1); });
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		call->setArg(2, regF[B + 1]);
		break;
	case CAST_V32S:
		call = CreateCall<void, FString*, double, double, double>([](FString *a, double b, double b1, double b2) { a->Format("(%.5f, %.5f, %.5f)", b, b1, b2); });
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		call->setArg(2, regF[B + 1]);
		call->setArg(3, regF[B + 2]);
		break;
	case CAST_P2S:
		call = CreateCall<void, FString*, void*>([](FString *a, void *b) { if (b == nullptr) *a = "null"; else a->Format("%p", b); });
		call->setArg(0, regS[A]);
		call->setArg(1, regA[B]);
		break;
	case CAST_S2I:
		call = CreateCall<int, FString*>([](FString *b) -> int { return (VM_SWORD)b->ToLong(); });
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_S2F:
		call = CreateCall<double, FString*>([](FString *b) -> double { return b->ToDouble(); });
		call->setRet(0, regF[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_S2N:
		call = CreateCall<int, FString*>([](FString *b) -> int { return b->Len() == 0 ? FName(NAME_None) : FName(*b); });
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_N2S:
		call = CreateCall<void, FString*, int>([](FString *a, int b) { FName name = FName(ENamedName(b)); *a = name.IsValidName() ? name.GetChars() : ""; });
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_S2Co:
		call = CreateCall<int, FString*>([](FString *b) -> int { return V_GetColor(nullptr, *b); });
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_Co2S:
		call = CreateCall<void, FString*, int>([](FString *a, int b) { PalEntry c(b); a->Format("%02x %02x %02x", c.r, c.g, c.b); });
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_S2So:
		call = CreateCall<int, FString*>([](FString *b) -> int { return FSoundID(*b); });
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_So2S:
		call = CreateCall<void, FString*, int>([](FString *a, int b) { *a = S_sfx[b].name; });
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_SID2S:
		call = CreateCall<void, FString*, unsigned int>([](FString *a, unsigned int b) { *a = (b >= sprites.Size()) ? "TNT1" : sprites[b].name; });
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_TID2S:
		call = CreateCall<void, FString*, int>([](FString *a, int b) { auto tex = TexMan[*(FTextureID*)&b]; *a = (tex == nullptr) ? "(null)" : tex->Name.GetChars(); });
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	default:
		I_FatalError("Unknown OP_CAST type\n");
	}
}

void JitCompiler::EmitCASTB()
{
	if (C == CASTB_I)
	{
		cc.mov(regD[A], regD[B]);
		cc.shr(regD[A], 31);
	}
	else if (C == CASTB_F)
	{
		auto zero = cc.newXmm();
		auto one = cc.newInt32();
		cc.xorpd(zero, zero);
		cc.mov(one, 1);
		cc.ucomisd(regF[A], zero);
		cc.setp(regD[A]);
		cc.cmovne(regD[A], one);
	}
	else if (C == CASTB_A)
	{
		cc.test(regA[A], regA[A]);
		cc.setne(regD[A]);
	}
	else
	{
		auto call = CreateCall<int, FString*>([](FString *s) -> int { return s->Len() > 0; });
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
	}
}

void JitCompiler::EmitDYNCAST_R()
{
	auto call = CreateCall<DObject*, DObject*, PClass*>([](DObject *obj, PClass *cls) -> DObject* {
		return (obj && obj->IsKindOf(cls)) ? obj : nullptr;
	});
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
	call->setArg(1, regA[C]);
}

void JitCompiler::EmitDYNCAST_K()
{
	auto c = cc.newIntPtr();
	cc.mov(c, asmjit::imm_ptr(konsta[C].o));
	auto call = CreateCall<DObject*, DObject*, PClass*>([](DObject *obj, PClass *cls) -> DObject* {
		return (obj && obj->IsKindOf(cls)) ? obj : nullptr;
	});
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
	call->setArg(1, c);
}

void JitCompiler::EmitDYNCASTC_R()
{
	auto call = CreateCall<PClass*, PClass*, PClass*>([](PClass *cls1, PClass *cls2) -> PClass* {
		return (cls1 && cls1->IsDescendantOf(cls2)) ? cls1 : nullptr;
	});
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
	call->setArg(1, regA[C]);
}

void JitCompiler::EmitDYNCASTC_K()
{
	using namespace asmjit;
	auto c = cc.newIntPtr();
	cc.mov(c, asmjit::imm_ptr(konsta[C].o));
	typedef PClass*(*FuncPtr)(PClass*, PClass*);
	auto call = CreateCall<PClass*, PClass*, PClass*>([](PClass *cls1, PClass *cls2) -> PClass* {
		return (cls1 && cls1->IsDescendantOf(cls2)) ? cls1 : nullptr;
	});
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
	call->setArg(1, c);
}
