
#include "jitintern.h"

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
	switch (C)
	{
	case CAST_I2F:
		cc.cvtsi2sd(regF[A], regD[B]);
		break;
	case CAST_U2F:
	{
		auto tmp = cc.newInt64();
		cc.xor_(tmp, tmp);
		cc.mov(tmp.r32(), regD[B]);
		cc.cvtsi2sd(regF[A], tmp);
		break;
	}
	case CAST_F2I:
		cc.cvttsd2si(regD[A], regF[B]);
		break;
	case CAST_F2U:
	{
		auto tmp = cc.newInt64();
		cc.cvttsd2si(tmp, regF[B]);
		cc.mov(regD[A], tmp.r32());
		break;
	}
	/*case CAST_I2S:
		reg.s[A].Format("%d", reg.d[B]);
		break;
	case CAST_U2S:
		reg.s[A].Format("%u", reg.d[B]);
		break;
	case CAST_F2S:
		reg.s[A].Format("%.5f", reg.f[B]);	// keep this small. For more precise conversion there should be a conversion function.
		break;
	case CAST_V22S:
		reg.s[A].Format("(%.5f, %.5f)", reg.f[B], reg.f[b + 1]);
		break;
	case CAST_V32S:
		reg.s[A].Format("(%.5f, %.5f, %.5f)", reg.f[B], reg.f[b + 1], reg.f[b + 2]);
		break;
	case CAST_P2S:
	{
		if (reg.a[B] == nullptr) reg.s[A] = "null";
		else reg.s[A].Format("%p", reg.a[B]);
		break;
	}
	case CAST_S2I:
		reg.d[A] = (VM_SWORD)reg.s[B].ToLong();
		break;
	case CAST_S2F:
		reg.f[A] = reg.s[B].ToDouble();
		break;
	case CAST_S2N:
		reg.d[A] = reg.s[B].Len() == 0 ? FName(NAME_None) : FName(reg.s[B]);
		break;
	case CAST_N2S:
	{
		FName name = FName(ENamedName(reg.d[B]));
		reg.s[A] = name.IsValidName() ? name.GetChars() : "";
		break;
	}
	case CAST_S2Co:
		reg.d[A] = V_GetColor(NULL, reg.s[B]);
		break;
	case CAST_Co2S:
		reg.s[A].Format("%02x %02x %02x", PalEntry(reg.d[B]).r, PalEntry(reg.d[B]).g, PalEntry(reg.d[B]).b);
		break;
	case CAST_S2So:
		reg.d[A] = FSoundID(reg.s[B]);
		break;
	case CAST_So2S:
		reg.s[A] = S_sfx[reg.d[B]].name;
		break;
	case CAST_SID2S:
		reg.s[A] = unsigned(reg.d[B]) >= sprites.Size() ? "TNT1" : sprites[reg.d[B]].name;
		break;
	case CAST_TID2S:
	{
		auto tex = TexMan[*(FTextureID*)&(reg.d[B])];
		reg.s[A] = tex == nullptr ? "(null)" : tex->Name.GetChars();
		break;
	}*/
	default:
		assert(0);
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
		//reg.d[A] = reg.s[B].Len() > 0;
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
