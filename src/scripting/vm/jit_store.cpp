
#include "jitintern.h"

void JitCompiler::EmitSB()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::byte_ptr(regA[A], konstd[C]), regD[B].r8Lo());
}

void JitCompiler::EmitSB_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::byte_ptr(regA[A], regD[C]), regD[B].r8Lo());
}

void JitCompiler::EmitSH()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::word_ptr(regA[A], konstd[C]), regD[B].r16());
}

void JitCompiler::EmitSH_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::word_ptr(regA[A], regD[C]), regD[B].r16());
}

void JitCompiler::EmitSW()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::dword_ptr(regA[A], konstd[C]), regD[B]);
}

void JitCompiler::EmitSW_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::dword_ptr(regA[A], regD[C]), regD[B]);
}

void JitCompiler::EmitSSP()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.movss(asmjit::x86::dword_ptr(regA[A], konstd[C]), regF[B]);
}

void JitCompiler::EmitSSP_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.movss(asmjit::x86::dword_ptr(regA[A], regD[C]), regF[B]);
}

void JitCompiler::EmitSDP()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.movsd(asmjit::x86::qword_ptr(regA[A], konstd[C]), regF[B]);
}

void JitCompiler::EmitSDP_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.movsd(asmjit::x86::qword_ptr(regA[A], regD[C]), regF[B]);
}

void JitCompiler::EmitSS()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto ptr = cc.newIntPtr();
	cc.lea(ptr, asmjit::x86::ptr(regA[A], konstd[C]));
	auto call = cc.call(ToMemAddress(reinterpret_cast<void*>(static_cast<void(*)(FString*, FString*)>(CallAssignString))),
		asmjit::FuncSignature2<void, FString*, FString*>(asmjit::CallConv::kIdHostCDecl));
	call->setArg(0, ptr);
	call->setArg(1, regS[B]);
}

void JitCompiler::EmitSS_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto ptr = cc.newIntPtr();
	cc.lea(ptr, asmjit::x86::ptr(regA[A], regD[C]));
	auto call = cc.call(ToMemAddress(reinterpret_cast<void*>(static_cast<void(*)(FString*, FString*)>(CallAssignString))),
		asmjit::FuncSignature2<void, FString*, FString*>(asmjit::CallConv::kIdHostCDecl));
	call->setArg(0, ptr);
	call->setArg(1, regS[B]);
}

void JitCompiler::EmitSO()
{
	cc.mov(asmjit::x86::ptr(regA[A], konstd[C]), regA[B]);

	typedef void(*FuncPtr)(DObject*);
	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(GC::WriteBarrier))), asmjit::FuncSignature1<void, void*>());
	call->setArg(0, regA[B]);
}

void JitCompiler::EmitSO_R()
{
	cc.mov(asmjit::x86::ptr(regA[A], regD[C]), regA[B]);

	typedef void(*FuncPtr)(DObject*);
	auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(GC::WriteBarrier))), asmjit::FuncSignature1<void, void*>());
	call->setArg(0, regA[B]);
}

void JitCompiler::EmitSP()
{
	cc.mov(asmjit::x86::ptr(regA[A], konstd[C]), regA[B]);
}

void JitCompiler::EmitSP_R()
{
	cc.mov(asmjit::x86::ptr(regA[A], regD[C]), regA[B]);
}

void JitCompiler::EmitSV2()
{
	EmitNullPointerThrow(B, X_WRITE_NIL);
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, regA[B]);
	cc.add(tmp, konstd[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
}

void JitCompiler::EmitSV2_R()
{
	EmitNullPointerThrow(B, X_WRITE_NIL);
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, regA[B]);
	cc.add(tmp, regD[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
}

void JitCompiler::EmitSV3()
{
	EmitNullPointerThrow(B, X_WRITE_NIL);
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, regA[B]);
	cc.add(tmp, konstd[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
}

void JitCompiler::EmitSV3_R()
{
	EmitNullPointerThrow(B, X_WRITE_NIL);
	auto tmp = cc.newIntPtr();
	cc.mov(tmp, regA[B]);
	cc.add(tmp, regD[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
}

void JitCompiler::EmitSBIT()
{
	EmitNullPointerThrow(B, X_WRITE_NIL);
	auto tmp1 = cc.newInt32();
	auto tmp2 = cc.newInt32();
	cc.mov(tmp1, asmjit::x86::byte_ptr(regA[A]));
	cc.mov(tmp2, tmp1);
	cc.or_(tmp1, (int)C);
	cc.and_(tmp2, ~(int)C);
	cc.test(regD[B], regD[B]);
	cc.cmove(tmp1, tmp2);
	cc.mov(asmjit::x86::byte_ptr(regA[A]), tmp1);
}
