
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
	auto call = cc.call(ToMemAddress(reinterpret_cast<void*>(static_cast<void(*)(FString*, FString*)>(CallAssignString))),
		asmjit::FuncSignature2<void, FString*, FString*>(asmjit::CallConv::kIdHostCDecl));
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
	static void (*i2s)(FString *a, int b) = [](FString *a, int b) { a->Format("%d", b); };
	static void (*u2s)(FString *a, int b) = [](FString *a, int b) { a->Format("%u", b); };
	static void (*f2s)(FString *a, double b) = [](FString *a, double b) { a->Format("%.5f", b); };
	static void (*v22s)(FString *a, double b, double b1) = [](FString *a, double b, double b1) { a->Format("(%.5f, %.5f)", b, b1); };
	static void (*v32s)(FString *a, double b, double b1, double b2) = [](FString *a, double b, double b1, double b2) { a->Format("(%.5f, %.5f, %.5f)", b, b1, b2); };
	static void (*p2s)(FString *a, void *b) = [](FString *a, void *b) { if (b == nullptr) *a = "null"; else a->Format("%p", b); };
	static int (*s2i)(FString *b) = [](FString *b) -> int { return (VM_SWORD)b->ToLong(); };
	static double (*s2f)(FString *b) = [](FString *b) -> double { return b->ToDouble(); };
	static int (*s2n)(FString *b) = [](FString *b) -> int { return b->Len() == 0 ? FName(NAME_None) : FName(*b); };
	static void (*n2s)(FString *a, int b) = [](FString *a, int b) { FName name = FName(ENamedName(b)); *a = name.IsValidName() ? name.GetChars() : ""; };
	static int (*s2co)(FString *b) = [](FString *b) -> int { return V_GetColor(nullptr, *b); };
	static void (*co2s)(FString *a, int b) = [](FString *a, int b) { PalEntry c(b); a->Format("%02x %02x %02x", c.r, c.g, c.b); };
	static int (*s2so)(FString *b) = [](FString *b) -> int { return FSoundID(*b); };
	static void (*so2s)(FString *a, int b) = [](FString *a, int b) { *a = S_sfx[b].name; };
	static void (*sid2s)(FString *a, unsigned int b) = [](FString *a, unsigned int b) { *a = (b >= sprites.Size()) ? "TNT1" : sprites[b].name; };
	static void (*tid2s)(FString *a, int b) = [](FString *a, int b) { auto tex = TexMan[*(FTextureID*)&b]; *a = (tex == nullptr) ? "(null)" : tex->Name.GetChars(); };

	typedef asmjit::FuncSignature2<void, FString*, int> FuncI2S, FuncU2S, FuncN2S, FuncCo2S, FuncSo2S, FuncSid2S, FuncTid2S;
	typedef asmjit::FuncSignature2<void, FString*, double> FuncF2S;
	typedef asmjit::FuncSignature3<void, FString*, double, double> FuncV22S;
	typedef asmjit::FuncSignature4<void, FString*, double, double, double> FuncV32S;
	typedef asmjit::FuncSignature2<void, FString*, void*> FuncP2S;
	typedef asmjit::FuncSignature1<int, FString*> FuncS2I, FuncS2N, FuncS2Co, FuncS2So;
	typedef asmjit::FuncSignature1<double, FString*> FuncS2F;

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
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(i2s)), FuncI2S());
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_U2S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(u2s)), FuncU2S());
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_F2S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(f2s)), FuncF2S());
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		break;
	case CAST_V22S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(v22s)), FuncV22S());
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		call->setArg(2, regF[B + 1]);
		break;
	case CAST_V32S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(v32s)), FuncV32S());
		call->setArg(0, regS[A]);
		call->setArg(1, regF[B]);
		call->setArg(2, regF[B + 1]);
		call->setArg(3, regF[B + 2]);
		break;
	case CAST_P2S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(p2s)), FuncP2S());
		call->setArg(0, regS[A]);
		call->setArg(1, regA[B]);
		break;
	case CAST_S2I:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(s2i)), FuncS2I());
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_S2F:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(s2f)), FuncS2F());
		call->setRet(0, regF[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_S2N:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(s2n)), FuncS2N());
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_N2S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(n2s)), FuncN2S());
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_S2Co:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(s2co)), FuncS2Co());
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_Co2S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(co2s)), FuncCo2S());
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_S2So:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(s2so)), FuncS2So());
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
		break;
	case CAST_So2S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(so2s)), FuncSo2S());
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_SID2S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(sid2s)), FuncSid2S());
		call->setArg(0, regS[A]);
		call->setArg(1, regD[B]);
		break;
	case CAST_TID2S:
		call = cc.call(ToMemAddress(reinterpret_cast<const void*>(tid2s)), FuncTid2S());
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
		using namespace asmjit;
		typedef int(*FuncPtr)(FString*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](FString *s) -> int {
			return s->Len() > 0;
		}))), FuncSignature1<int, void*>());
		call->setRet(0, regD[A]);
		call->setArg(0, regS[B]);
	}
}

void JitCompiler::EmitDYNCAST_R()
{
	using namespace asmjit;
	typedef DObject*(*FuncPtr)(DObject*, PClass*);
	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](DObject *obj, PClass *cls) -> DObject* {
		return (obj && obj->IsKindOf(cls)) ? obj : nullptr;
	}))), FuncSignature2<void*, void*, void*>());
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
	call->setArg(1, regA[C]);
}

void JitCompiler::EmitDYNCAST_K()
{
	using namespace asmjit;
	auto c = cc.newIntPtr();
	cc.mov(c, ToMemAddress(konsta[C].o));
	typedef DObject*(*FuncPtr)(DObject*, PClass*);
	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](DObject *obj, PClass *cls) -> DObject* {
		return (obj && obj->IsKindOf(cls)) ? obj : nullptr;
	}))), FuncSignature2<void*, void*, void*>());
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
	call->setArg(1, c);
}

void JitCompiler::EmitDYNCASTC_R()
{
	using namespace asmjit;
	typedef PClass*(*FuncPtr)(PClass*, PClass*);
	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](PClass *cls1, PClass *cls2) -> PClass* {
		return (cls1 && cls1->IsDescendantOf(cls2)) ? cls1 : nullptr;
	}))), FuncSignature2<void*, void*, void*>());
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
	call->setArg(1, regA[C]);
}

void JitCompiler::EmitDYNCASTC_K()
{
	using namespace asmjit;
	auto c = cc.newIntPtr();
	cc.mov(c, ToMemAddress(konsta[C].o));
	typedef PClass*(*FuncPtr)(PClass*, PClass*);
	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](PClass *cls1, PClass *cls2) -> PClass* {
		return (cls1 && cls1->IsDescendantOf(cls2)) ? cls1 : nullptr;
	}))), FuncSignature2<void*, void*, void*>());
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
	call->setArg(1, c);
}
