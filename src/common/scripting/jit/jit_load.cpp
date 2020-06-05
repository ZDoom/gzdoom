
#include "jitintern.h"

/////////////////////////////////////////////////////////////////////////////
// Load constants.

void JitCompiler::EmitLI()
{
	StoreD(ConstValueD(BCs), A);
}

void JitCompiler::EmitLK()
{
	StoreD(ConstD(BC), A);
}

void JitCompiler::EmitLKF()
{
	StoreF(ConstF(BC), A);
}

void JitCompiler::EmitLKS()
{
	cc.CreateCall(stringAssignmentOperator, { LoadS(A), ConstS(BC) });
}

void JitCompiler::EmitLKP()
{
	StoreA(ConstA(BC), A);
}

void JitCompiler::EmitLK_R()
{
	IRValue* base = ircontext->getConstantInt(int32PtrTy, (uint64_t)&konstd[C]);
	StoreD(Load(OffsetPtr(base, LoadD(B))), A);
}

void JitCompiler::EmitLKF_R()
{
	IRValue* base = ircontext->getConstantInt(doublePtrTy, (uint64_t)&konstf[C]);
	StoreF(Load(OffsetPtr(base, LoadD(B))), A);
}

void JitCompiler::EmitLKS_R()
{
	IRValue* base = ircontext->getConstantInt(int8PtrPtrTy, (uint64_t)&konsts[C]);
	cc.CreateCall(stringAssignmentOperator, { LoadS(A), Load(OffsetPtr(base, LoadD(B))) });
}

void JitCompiler::EmitLKP_R()
{
	IRValue* base = ircontext->getConstantInt(int8PtrPtrTy, (uint64_t)&konsta[C]);
	StoreA(Load(OffsetPtr(base, LoadD(B))), A);
}

void JitCompiler::EmitLFP()
{
	CheckVMFrame();
	StoreA(OffsetPtr(vmframe, offsetExtra), A);
}

void JitCompiler::EmitMETA()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_READ_NIL);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(LoadA(B), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);

	IRValue* ptrObject = LoadA(B);
	IRValue* ptrClass = Load(ToInt8PtrPtr(ptrObject, ConstValueD(myoffsetof(DObject, Class))));
	IRValue* ptrMeta = Load(ToInt8PtrPtr(ptrClass, ConstValueD(myoffsetof(PClass, Meta))));
	StoreA(ptrMeta, A);
}

void JitCompiler::EmitCLSS()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_READ_NIL);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(LoadA(B), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);

	IRValue* ptrObject = LoadA(B);
	IRValue* ptrClass = Load(ToInt8PtrPtr(ptrObject, ConstValueD(myoffsetof(DObject, Class))));
	StoreA(ptrClass, A);
}

/////////////////////////////////////////////////////////////////////////////
// Load from memory. rA = *(rB + rkC)

void JitCompiler::EmitLB()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(SExt(Load(ToInt8Ptr(LoadA(B), ConstD(C)))), A);
}

void JitCompiler::EmitLB_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(SExt(Load(ToInt8Ptr(LoadA(B), LoadD(C)))), A);
}

void JitCompiler::EmitLH()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(SExt(Load(ToInt16Ptr(LoadA(B), ConstD(C)))), A);
}

void JitCompiler::EmitLH_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(SExt(Load(ToInt16Ptr(LoadA(B), LoadD(C)))), A);
}

void JitCompiler::EmitLW()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(SExt(Load(ToInt32Ptr(LoadA(B), ConstD(C)))), A);
}

void JitCompiler::EmitLW_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(SExt(Load(ToInt32Ptr(LoadA(B), LoadD(C)))), A);
}

void JitCompiler::EmitLBU()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(ZExt(Load(ToInt8Ptr(LoadA(B), ConstD(C)))), A);
}

void JitCompiler::EmitLBU_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(ZExt(Load(ToInt8Ptr(LoadA(B), LoadD(C)))), A);
}

void JitCompiler::EmitLHU()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(ZExt(Load(ToInt16Ptr(LoadA(B), ConstD(C)))), A);
}

void JitCompiler::EmitLHU_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreD(ZExt(Load(ToInt16Ptr(LoadA(B), LoadD(C)))), A);
}

void JitCompiler::EmitLSP()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreF(FPExt(Load(ToDoublePtr(LoadA(B), ConstD(C)))), A);
}

void JitCompiler::EmitLSP_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreF(FPExt(Load(ToDoublePtr(LoadA(B), LoadD(C)))), A);
}

void JitCompiler::EmitLDP()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreF(Load(ToDoublePtr(LoadA(B), ConstD(C))), A);
}

void JitCompiler::EmitLDP_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreF(Load(ToDoublePtr(LoadA(B), LoadD(C))), A);
}

void JitCompiler::EmitLS()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.CreateCall(stringAssignmentOperator, { LoadS(A), OffsetPtr(LoadA(B), ConstD(C)) });
}

void JitCompiler::EmitLS_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.CreateCall(stringAssignmentOperator, { LoadS(A), OffsetPtr(LoadA(B), LoadD(C)) });
}

#if 0 // Inline read barrier impl

void JitCompiler::EmitReadBarrier()
{
	auto isnull = cc.newLabel();
	cc.test(regA[A], regA[A]);
	cc.je(isnull);

	auto mask = newTempIntPtr();
	cc.mov(mask.r32(), asmjit::x86::dword_ptr(regA[A], myoffsetof(DObject, ObjectFlags)));
	cc.shl(mask, 63 - 5); // put OF_EuthanizeMe (1 << 5) in the highest bit
	cc.sar(mask, 63); // sign extend so all bits are set if OF_EuthanizeMe was set
	cc.not_(mask);
	cc.and_(regA[A], mask);

	cc.bind(isnull);
}

void JitCompiler::EmitLO()
{
	EmitNullPointerThrow(B, X_READ_NIL);

	cc.mov(regA[A], asmjit::x86::ptr(regA[B], konstd[C]));
	EmitReadBarrier();
}

void JitCompiler::EmitLO_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);

	cc.mov(regA[A], asmjit::x86::ptr(regA[B], regD[C]));
	EmitReadBarrier();
}

#else

void JitCompiler::EmitLO()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreA(cc.CreateCall(readBarrier, { OffsetPtr(LoadA(B), ConstD(C)) }), A);
}

void JitCompiler::EmitLO_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreA(cc.CreateCall(readBarrier, { OffsetPtr(LoadA(B), LoadD(C)) }), A);
}

#endif

void JitCompiler::EmitLP()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreA(OffsetPtr(LoadA(B), ConstD(C)), A);
}

void JitCompiler::EmitLP_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	StoreA(OffsetPtr(LoadA(B), LoadD(C)), A);
}

void JitCompiler::EmitLV2()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	IRValue* base = ToFloatPtr(LoadA(B), ConstD(C));
	StoreF(Load(base), A);
	StoreF(Load(OffsetPtr(base, 1)), A + 1);
}

void JitCompiler::EmitLV2_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	IRValue* base = ToFloatPtr(LoadA(B), LoadD(C));
	StoreF(Load(base), A);
	StoreF(Load(OffsetPtr(base, 1)), A + 1);
}

void JitCompiler::EmitLV3()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	IRValue* base = ToFloatPtr(LoadA(B), ConstD(C));
	StoreF(Load(base), A);
	StoreF(Load(OffsetPtr(base, 1)), A + 1);
	StoreF(Load(OffsetPtr(base, 2)), A + 2);
}

void JitCompiler::EmitLV3_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	IRValue* base = ToFloatPtr(LoadA(B), LoadD(C));
	StoreF(Load(base), A);
	StoreF(Load(OffsetPtr(base, 1)), A + 1);
	StoreF(Load(OffsetPtr(base, 2)), A + 2);
}

void JitCompiler::EmitLCS()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.CreateCall(stringAssignmentOperatorCStr, { LoadS(A), OffsetPtr(LoadA(B), ConstD(C)) });
}

void JitCompiler::EmitLCS_R()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	cc.CreateCall(stringAssignmentOperatorCStr, { LoadS(A), OffsetPtr(LoadA(B), LoadD(C)) });
}

void JitCompiler::EmitLBIT()
{
	EmitNullPointerThrow(B, X_READ_NIL);
	IRValue* value = Load(LoadA(B));
	value = cc.CreateAnd(value, ircontext->getConstantInt(C));
	value = cc.CreateICmpNE(value, ircontext->getConstantInt(0));
	value = cc.CreateZExt(value, int32Ty);
	StoreD(value, A);
}
