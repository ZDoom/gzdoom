
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
	auto tmp = newTempXmmSd();
	cc.xorpd(tmp, tmp);
	cc.cvtsd2ss(tmp, regF[B]);
	cc.movss(asmjit::x86::dword_ptr(regA[A], konstd[C]), tmp);
}

void JitCompiler::EmitSSP_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempXmmSd();
	cc.xorpd(tmp, tmp);
	cc.cvtsd2ss(tmp, regF[B]);
	cc.movss(asmjit::x86::dword_ptr(regA[A], regD[C]), tmp);
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
	auto ptr = newTempIntPtr();
	cc.lea(ptr, asmjit::x86::ptr(regA[A], konstd[C]));
	auto call = CreateCall<void, FString*, FString*>(&JitCompiler::CallAssignString);
	call->setArg(0, ptr);
	call->setArg(1, regS[B]);
}

void JitCompiler::EmitSS_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto ptr = newTempIntPtr();
	cc.lea(ptr, asmjit::x86::ptr(regA[A], regD[C]));
	auto call = CreateCall<void, FString*, FString*>(&JitCompiler::CallAssignString);
	call->setArg(0, ptr);
	call->setArg(1, regS[B]);
}

void JitCompiler::EmitSO()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::ptr(regA[A], konstd[C]), regA[B]);

	typedef void(*FuncPtr)(DObject*);
	auto call = CreateCall<void, DObject*>(static_cast<FuncPtr>(GC::WriteBarrier));
	call->setArg(0, regA[B]);
}

void JitCompiler::EmitSO_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::ptr(regA[A], regD[C]), regA[B]);

	typedef void(*FuncPtr)(DObject*);
	auto call = CreateCall<void, DObject*>(static_cast<FuncPtr>(GC::WriteBarrier));
	call->setArg(0, regA[B]);
}

void JitCompiler::EmitSP()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::ptr(regA[A], konstd[C]), regA[B]);
}

void JitCompiler::EmitSP_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	cc.mov(asmjit::x86::ptr(regA[A], regD[C]), regA[B]);
}

void JitCompiler::EmitSV2()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, konstd[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
}

void JitCompiler::EmitSV2_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, regD[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
}

void JitCompiler::EmitSV3()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, konstd[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
}

void JitCompiler::EmitSV3_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, regD[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
}

void JitCompiler::EmitSV4()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, konstd[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 24), regF[B + 3]);
}

void JitCompiler::EmitSV4_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, regD[C]);
	cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
	cc.movsd(asmjit::x86::qword_ptr(tmp, 24), regF[B + 3]);
}

void JitCompiler::EmitSFV2()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, konstd[C]);

	auto tmpF = newTempXmmSs();
	cc.cvtsd2ss(tmpF, regF[B]);
	cc.movss(asmjit::x86::qword_ptr(tmp), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 1]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 4), tmpF);
}

void JitCompiler::EmitSFV2_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, regD[C]);

	auto tmpF = newTempXmmSs();
	cc.cvtsd2ss(tmpF, regF[B]);
	cc.movss(asmjit::x86::qword_ptr(tmp), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 1]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 4), tmpF);
}

void JitCompiler::EmitSFV3()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, konstd[C]);
	auto tmpF = newTempXmmSs();
	cc.cvtsd2ss(tmpF, regF[B]);
	cc.movss(asmjit::x86::qword_ptr(tmp), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 1]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 4), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 2]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 8), tmpF);
}

void JitCompiler::EmitSFV3_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, regD[C]);
	auto tmpF = newTempXmmSs();
	cc.cvtsd2ss(tmpF, regF[B]);
	cc.movss(asmjit::x86::qword_ptr(tmp), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 1]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 4), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 2]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 8), tmpF);
}

void JitCompiler::EmitSFV4()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, konstd[C]);
	auto tmpF = newTempXmmSs();
	cc.cvtsd2ss(tmpF, regF[B]);
	cc.movss(asmjit::x86::qword_ptr(tmp), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 1]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 4), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 2]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 8), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 3]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 12), tmpF);
}

void JitCompiler::EmitSFV4_R()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[A]);
	cc.add(tmp, regD[C]);
	auto tmpF = newTempXmmSs();
	cc.cvtsd2ss(tmpF, regF[B]);
	cc.movss(asmjit::x86::qword_ptr(tmp), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 1]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 4), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 2]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 8), tmpF);
	cc.cvtsd2ss(tmpF, regF[B + 3]);
	cc.movss(asmjit::x86::qword_ptr(tmp, 12), tmpF);
}

void JitCompiler::EmitSBIT()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	auto tmp1 = newTempInt32();
	auto tmp2 = newTempInt32();
	cc.mov(tmp1, asmjit::x86::byte_ptr(regA[A]));
	cc.mov(tmp2, tmp1);
	cc.or_(tmp1, (int)C);
	cc.and_(tmp2, ~(int)C);
	cc.test(regD[B], regD[B]);
	cc.cmove(tmp1, tmp2);
	cc.mov(asmjit::x86::byte_ptr(regA[A]), tmp1);
}

void JitCompiler::EmitMEMCPY_RRK()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	EmitNullPointerThrow(B, X_READ_NIL);
	auto call = CreateCall<void*, void*, const void*, size_t>(&memcpy);
	call->setArg(0, regA[A]);
	call->setArg(1, regA[B]);
	call->setArg(2, asmjit::Imm{konstd[C]});
	call->setInlineComment("call memcpy");
}

void JitCompiler::EmitCOPY_NULLCHECK()
{
	EmitNullPointerThrow(A, X_WRITE_NIL);
	EmitNullPointerThrow(B, X_READ_NIL);
}

void JitCompiler::EmitMEMCPY_RRK_UNCHECKED()
{
	auto call = CreateCall<void*, void*, const void*, size_t>(&memcpy);
	call->setArg(0, regA[A]);
	call->setArg(1, regA[B]);
	call->setArg(2, asmjit::Imm{konstd[C]});
	call->setInlineComment("call memcpy");
}

void JitCompiler::EmitOBJ_WBARRIER()
{
	typedef void(*FuncPtr)(DObject*);
	auto call = CreateCall<void, DObject*>(static_cast<FuncPtr>(GC::WriteBarrier));
	call->setArg(0, regA[A]);
	call->setInlineComment("call GC::WriteBarrier");
}

void JitCompiler::EmitCALL_NATIVE_RR()
{
	typedef void(*CopyFn)(void*, const void*);
	auto call = CreateCall<void, void*, const void*>((const CopyFn)(konsta[C].v));
	call->setArg(0, regA[A]);
	call->setArg(1, regA[B]);
	call->setInlineComment("call C(A, B)");
}