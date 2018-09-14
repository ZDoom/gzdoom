
#include "jitintern.h"

/////////////////////////////////////////////////////////////////////////////
// Load constants.

void JitCompiler::EmitLI()
{
	cc.mov(regD[A], BCs);
}

void JitCompiler::EmitLK()
{
	cc.mov(regD[A], konstd[BC]);
}

void JitCompiler::EmitLKF()
{
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, ToMemAddress(konstf + BC));
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitLKS()
{
	auto call = cc.call(ToMemAddress(reinterpret_cast<void*>(static_cast<void(*)(FString*, FString*)>(CallAssignString))),
		                asmjit::FuncSignature2<void, FString*, FString*>(asmjit::CallConv::kIdHostCDecl));
	call->setArg(0, regS[A]);
	call->setArg(1, asmjit::imm(ToMemAddress(konsts + BC)));
}

void JitCompiler::EmitLKP()
{
	cc.mov(regA[A], (int64_t)konsta[BC].v);
}

void JitCompiler::EmitLK_R()
{
	cc.mov(regD[A], asmjit::x86::ptr(ToMemAddress(konstd), regD[B], 2, C * sizeof(int32_t)));
}

void JitCompiler::EmitLKF_R()
{
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, ToMemAddress(konstf + BC));
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp, regD[B], 3, C * sizeof(double)));
}

void JitCompiler::EmitLKS_R()
{
	I_FatalError("EmitLKS_R not implemented\n");
}

void JitCompiler::EmitLKP_R()
{
	cc.mov(regA[A], asmjit::x86::ptr(ToMemAddress(konsta), regD[B], 2, C * sizeof(void*)));
}

void JitCompiler::EmitLFP()
{
	I_FatalError("EmitLFP not implemented\n");
}

void JitCompiler::EmitMETA()
{
	typedef void*(*FuncPtr)(void*);

	auto label = cc.newLabel();
	cc.test(regA[B], regA[B]);
	cc.jne(label);
	EmitThrowException(X_READ_NIL);
	cc.bind(label);

	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](void *o) -> void*
	{
		return static_cast<DObject*>(o)->GetClass()->Meta;
	}))), asmjit::FuncSignature1<void*, void*>());
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
}

void JitCompiler::EmitCLSS()
{
	typedef void*(*FuncPtr)(void*);

	auto label = cc.newLabel();
	cc.test(regA[B], regA[B]);
	cc.jne(label);
	EmitThrowException(X_READ_NIL);
	cc.bind(label);

	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](void *o) -> void*
	{
		return static_cast<DObject*>(o)->GetClass();
	}))), asmjit::FuncSignature1<void*, void*>());
	call->setRet(0, regA[A]);
	call->setArg(0, regA[B]);
}

/////////////////////////////////////////////////////////////////////////////
// Load from memory. rA = *(rB + rkC)

void JitCompiler::EmitLB()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movsx(regD[A], asmjit::x86::byte_ptr(regA[B], konstd[C]));
}

void JitCompiler::EmitLB_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movsx(regD[A], asmjit::x86::byte_ptr(regA[B], regD[C]));
}

void JitCompiler::EmitLH()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movsx(regD[A], asmjit::x86::word_ptr(regA[B], konstd[C]));
}

void JitCompiler::EmitLH_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movsx(regD[A], asmjit::x86::word_ptr(regA[B], regD[C]));
}

void JitCompiler::EmitLW()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.mov(regD[A], asmjit::x86::dword_ptr(regA[B], konstd[C]));
}

void JitCompiler::EmitLW_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.mov(regD[A], asmjit::x86::dword_ptr(regA[B], regD[C]));
}

void JitCompiler::EmitLBU()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movzx(regD[A], asmjit::x86::byte_ptr(regA[B], konstd[C]));
}

void JitCompiler::EmitLBU_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movzx(regD[A].r8Lo(), asmjit::x86::byte_ptr(regA[B], regD[C]));
}

void JitCompiler::EmitLHU()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movzx(regD[A].r16(), asmjit::x86::word_ptr(regA[B], konstd[C]));
}

void JitCompiler::EmitLHU_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movzx(regD[A].r16(), asmjit::x86::word_ptr(regA[B], regD[C]));
}

void JitCompiler::EmitLSP()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movss(regF[A], asmjit::x86::dword_ptr(regA[B], konstd[C]));
}

void JitCompiler::EmitLSP_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movss(regF[A], asmjit::x86::dword_ptr(regA[B], regD[C]));
}

void JitCompiler::EmitLDP()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movsd(regF[A], asmjit::x86::qword_ptr(regA[B], konstd[C]));
}

void JitCompiler::EmitLDP_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movsd(regF[A], asmjit::x86::qword_ptr(regA[B], regD[C]));
}

void JitCompiler::EmitLS()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	auto ptr = cc.newIntPtr();
	cc.mov(ptr, regA[B]);
	cc.add(ptr, konstd[C]);
	auto call = cc.call(ToMemAddress(reinterpret_cast<void*>(static_cast<void(*)(FString*, FString*)>(CallAssignString))),
		asmjit::FuncSignature2<void, FString*, FString*>(asmjit::CallConv::kIdHostCDecl));
	call->setArg(0, regS[A]);
	call->setArg(1, ptr);
}

void JitCompiler::EmitLS_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	auto ptr = cc.newIntPtr();
	cc.mov(ptr, regA[B]);
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, regD[C]);
	cc.add(ptr, tmp);
	auto call = cc.call(ToMemAddress(reinterpret_cast<void*>(static_cast<void(*)(FString*, FString*)>(CallAssignString))),
		asmjit::FuncSignature2<void, FString*, FString*>(asmjit::CallConv::kIdHostCDecl));
	call->setArg(0, regS[A]);
	call->setArg(1, ptr);
}

void JitCompiler::EmitLO()
{
	EmitNullPointerThrow(B, X_READ_NIL);

	auto ptr = cc.newIntPtr();
	cc.mov(ptr, asmjit::x86::ptr(regA[B], konstd[C]));

	typedef void*(*FuncPtr)(void*);
	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](void *ptr) -> void*
	{
		DObject *p = static_cast<DObject *>(ptr);
		return GC::ReadBarrier(p);
	}))), asmjit::FuncSignature1<void*, void*>());
	call->setRet(0, regA[A]);
	call->setArg(0, ptr);
}

void JitCompiler::EmitLO_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);

	auto ptr = cc.newIntPtr();
	cc.mov(ptr, asmjit::x86::ptr(regA[B], regD[C]));

	typedef void*(*FuncPtr)(void*);
	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](void *ptr) -> void*
	{
		DObject *p = static_cast<DObject *>(ptr);
		return GC::ReadBarrier(p);
	}))), asmjit::FuncSignature1<void*, void*>());
	call->setRet(0, regA[A]);
	call->setArg(0, ptr);
}

void JitCompiler::EmitLP()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.mov(regA[A], asmjit::x86::ptr(regA[B], konstd[C]));
}

void JitCompiler::EmitLP_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.mov(regA[A], asmjit::x86::ptr(regA[B], regD[C]));
}

void JitCompiler::EmitLV2()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, regA[B]);
	cc.add(tmp, konstd[C]);
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.movsd(regF[A + 1], asmjit::x86::qword_ptr(tmp, 8));
}

void JitCompiler::EmitLV2_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, regA[B]);
	cc.add(tmp, regD[C]);
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.movsd(regF[A + 1], asmjit::x86::qword_ptr(tmp, 8));
}

void JitCompiler::EmitLV3()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, regA[B]);
	cc.add(tmp, konstd[C]);
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.movsd(regF[A + 1], asmjit::x86::qword_ptr(tmp, 8));
	cc.movsd(regF[A + 2], asmjit::x86::qword_ptr(tmp, 16));
}

void JitCompiler::EmitLV3_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, regA[B]);
	cc.add(tmp, regD[C]);
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.movsd(regF[A + 1], asmjit::x86::qword_ptr(tmp, 8));
	cc.movsd(regF[A + 2], asmjit::x86::qword_ptr(tmp, 16));
}

void JitCompiler::EmitLCS()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	auto ptr = cc.newIntPtr();
	cc.mov(ptr, regA[B]);
	cc.add(ptr, konstd[C]);
	auto loadLambda = [](FString* to, char** from) -> void {
		*to = *from;
	};
	auto call = cc.call(ToMemAddress(reinterpret_cast<void*>(static_cast<void(*)(FString*, char**)>(loadLambda))),
		asmjit::FuncSignature2<void, FString*, char**>(asmjit::CallConv::kIdHostCDecl));
	call->setArg(0, regS[A]);
	call->setArg(1, ptr);
}

void JitCompiler::EmitLCS_R()
{
	I_FatalError("EmitLCS_R not implemented\n");
}

void JitCompiler::EmitLBIT()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.movsx(regD[A], asmjit::x86::byte_ptr(regA[B]));
	cc.and_(regD[A], C);
	cc.cmp(regD[A], 0);
	cc.setne(regD[A]);
}
