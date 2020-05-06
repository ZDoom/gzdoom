
#include "jitintern.h"
#include "basics.h"

/////////////////////////////////////////////////////////////////////////////
// String instructions.

static void ConcatString(FString* to, FString* first, FString* second)
{
	*to = *first + *second;
}

void JitCompiler::EmitCONCAT()
{
	cc.CreateCall(GetNativeFunc<void, FString*, FString*, FString*>("__ConcatString", ConcatString), { cc.CreateLoad(regS[A]), cc.CreateLoad(regS[B]), cc.CreateLoad(regS[C]) });
}

static int StringLength(FString* str)
{
	return static_cast<int>(str->Len());
}

void JitCompiler::EmitLENS()
{
	cc.CreateStore(cc.CreateCall(GetNativeFunc<int, FString*>("__StringLength", StringLength), { cc.CreateLoad(regS[B]) }), regD[A]);
}

static int StringCompareNoCase(FString* first, FString* second)
{
	return first->CompareNoCase(*second);
}

static int StringCompare(FString* first, FString* second)
{
	return first->Compare(*second);
}

void JitCompiler::EmitCMPS()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {

		IRFunction* comparefunc = static_cast<bool>(A & CMP_APPROX) ? GetNativeFunc<int, FString*, FString*>("__StringCompareNoCase", StringCompareNoCase) : GetNativeFunc<int, FString*, FString*>("__StringCompare", StringCompare);
		IRValue* arg0;
		IRValue* arg1;

		if (static_cast<bool>(A & CMP_BK)) arg0 = ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)&konsts[B]);
		else                               arg0 = cc.CreateLoad(regS[B]);

		if (static_cast<bool>(A & CMP_CK)) arg1 = ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)&konsts[C]);
		else                               arg1 = cc.CreateLoad(regS[C]);

		IRValue* result = cc.CreateCall(comparefunc, { arg0, arg1 });
		IRValue* zero = ircontext->getConstantInt(0);

		int method = A & CMP_METHOD_MASK;
		if (method == CMP_EQ)
		{
			if (check) result = cc.CreateICmpEQ(result, zero);
			else       result = cc.CreateICmpNE(result, zero);
		}
		else if (method == CMP_LT)
		{
			if (check) result = cc.CreateICmpSLT(result, zero);
			else       result = cc.CreateICmpSGE(result, zero);
		}
		else
		{
			if (check) result = cc.CreateICmpSLE(result, zero);
			else       result = cc.CreateICmpSGT(result, zero);
		}

		cc.CreateCondBr(result, fail, success);
	});
}

/////////////////////////////////////////////////////////////////////////////
// Integer math.

void JitCompiler::EmitSLL_RR()
{
	cc.CreateStore(cc.CreateShl(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitSLL_RI()
{
	cc.CreateStore(cc.CreateShl(cc.CreateLoad(regD[B]), ircontext->getConstantInt(C)), regD[A]);
}

void JitCompiler::EmitSLL_KR()
{
	cc.CreateStore(cc.CreateShl(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitSRL_RR()
{
	cc.CreateStore(cc.CreateLShr(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitSRL_RI()
{
	cc.CreateStore(cc.CreateLShr(cc.CreateLoad(regD[B]), ircontext->getConstantInt(C)), regD[A]);
}

void JitCompiler::EmitSRL_KR()
{
	cc.CreateStore(cc.CreateLShr(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitSRA_RR()
{
	cc.CreateStore(cc.CreateAShr(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitSRA_RI()
{
	cc.CreateStore(cc.CreateAShr(cc.CreateLoad(regD[B]), ircontext->getConstantInt(C)), regD[A]);
}

void JitCompiler::EmitSRA_KR()
{
	cc.CreateStore(cc.CreateAShr(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitADD_RR()
{
	cc.CreateStore(cc.CreateAdd(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitADD_RK()
{
	cc.CreateStore(cc.CreateAdd(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
}

void JitCompiler::EmitADDI()
{
	cc.CreateStore(cc.CreateAdd(cc.CreateLoad(regD[B]), ircontext->getConstantInt(Cs)), regD[A]);
}

void JitCompiler::EmitSUB_RR()
{
	cc.CreateStore(cc.CreateSub(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitSUB_RK()
{
	cc.CreateStore(cc.CreateSub(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
}

void JitCompiler::EmitSUB_KR()
{
	cc.CreateStore(cc.CreateSub(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitMUL_RR()
{
	cc.CreateStore(cc.CreateMul(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitMUL_RK()
{
	cc.CreateStore(cc.CreateMul(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
}

void JitCompiler::EmitDIV_RR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(cc.CreateLoad(regD[C]), ircontext->getConstantInt(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateSDiv(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitDIV_RK()
{
	if (konstd[C] != 0)
	{
		cc.CreateStore(cc.CreateSDiv(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
	}
	else
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
}

void JitCompiler::EmitDIV_KR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(cc.CreateLoad(regD[C]), ircontext->getConstantInt(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateSDiv(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[B])), regD[A]);
}

void JitCompiler::EmitDIVU_RR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(cc.CreateLoad(regD[C]), ircontext->getConstantInt(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateUDiv(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitDIVU_RK()
{
	if (konstd[C] != 0)
	{
		cc.CreateStore(cc.CreateUDiv(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
	}
	else
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
}

void JitCompiler::EmitDIVU_KR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(cc.CreateLoad(regD[C]), ircontext->getConstantInt(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateUDiv(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[B])), regD[A]);
}

void JitCompiler::EmitMOD_RR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(cc.CreateLoad(regD[C]), ircontext->getConstantInt(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateSRem(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitMOD_RK()
{
	if (konstd[C] != 0)
	{
		cc.CreateStore(cc.CreateSRem(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
	}
	else
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
}

void JitCompiler::EmitMOD_KR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(cc.CreateLoad(regD[C]), ircontext->getConstantInt(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateSRem(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[B])), regD[A]);
}

void JitCompiler::EmitMODU_RR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(cc.CreateLoad(regD[C]), ircontext->getConstantInt(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateURem(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitMODU_RK()
{
	if (konstd[C] != 0)
	{
		cc.CreateStore(cc.CreateURem(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
	}
	else
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
}

void JitCompiler::EmitMODU_KR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(cc.CreateLoad(regD[C]), ircontext->getConstantInt(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateURem(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[B])), regD[A]);
}

void JitCompiler::EmitAND_RR()
{
	cc.CreateStore(cc.CreateAnd(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitAND_RK()
{
	cc.CreateStore(cc.CreateAnd(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
}

void JitCompiler::EmitOR_RR()
{
	cc.CreateStore(cc.CreateOr(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitOR_RK()
{
	cc.CreateStore(cc.CreateOr(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
}

void JitCompiler::EmitXOR_RR()
{
	cc.CreateStore(cc.CreateXor(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C])), regD[A]);
}

void JitCompiler::EmitXOR_RK()
{
	cc.CreateStore(cc.CreateXor(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C])), regD[A]);
}

void JitCompiler::EmitMIN_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regD[B]);
	IRValue* val1 = cc.CreateLoad(regD[C]);

	cc.CreateCondBr(cc.CreateICmpSLE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMIN_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regD[B]);
	IRValue* val1 = ircontext->getConstantInt(konstd[C]);

	cc.CreateCondBr(cc.CreateICmpSLE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAX_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regD[B]);
	IRValue* val1 = cc.CreateLoad(regD[C]);

	cc.CreateCondBr(cc.CreateICmpSGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAX_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regD[B]);
	IRValue* val1 = ircontext->getConstantInt(konstd[C]);

	cc.CreateCondBr(cc.CreateICmpSGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMINU_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regD[B]);
	IRValue* val1 = cc.CreateLoad(regD[C]);

	cc.CreateCondBr(cc.CreateICmpULE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMINU_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regD[B]);
	IRValue* val1 = ircontext->getConstantInt(konstd[C]);

	cc.CreateCondBr(cc.CreateICmpULE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAXU_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regD[B]);
	IRValue* val1 = cc.CreateLoad(regD[C]);

	cc.CreateCondBr(cc.CreateICmpUGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAXU_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regD[B]);
	IRValue* val1 = ircontext->getConstantInt(konstd[C]);

	cc.CreateCondBr(cc.CreateICmpUGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regD[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitABS()
{
	IRValue* srcB = cc.CreateLoad(regD[B]);
	IRValue* tmp = cc.CreateAShr(srcB, ircontext->getConstantInt(31));
	cc.CreateStore(cc.CreateSub(cc.CreateXor(tmp, srcB), tmp), regD[A]);
}

void JitCompiler::EmitNEG()
{
	cc.CreateStore(cc.CreateNeg(cc.CreateLoad(regD[B])), regD[A]);
}

void JitCompiler::EmitNOT()
{
	cc.CreateStore(cc.CreateNot(cc.CreateLoad(regD[B])), regD[A]);
}

void JitCompiler::EmitEQ_R()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpEQ(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		else       result = cc.CreateICmpNE(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitEQ_K()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpEQ(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		else       result = cc.CreateICmpNE(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLT_RR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLT(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		else       result = cc.CreateICmpSGE(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLT_RK()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLT(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		else       result = cc.CreateICmpSGE(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLT_KR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLT(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C]));
		else       result = cc.CreateICmpSGE(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLE_RR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLE(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		else       result = cc.CreateICmpSGT(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLE_RK()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLE(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		else       result = cc.CreateICmpSGT(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLE_KR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLE(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C]));
		else       result = cc.CreateICmpSGT(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLTU_RR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpULT(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		else       result = cc.CreateICmpUGE(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLTU_RK()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpULT(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		else       result = cc.CreateICmpUGE(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLTU_KR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpULT(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C]));
		else       result = cc.CreateICmpUGE(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLEU_RR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpULE(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		else       result = cc.CreateICmpUGT(cc.CreateLoad(regD[B]), cc.CreateLoad(regD[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLEU_RK()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpULE(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		else       result = cc.CreateICmpUGT(cc.CreateLoad(regD[B]), ircontext->getConstantInt(konstd[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLEU_KR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpULE(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C]));
		else       result = cc.CreateICmpUGT(ircontext->getConstantInt(konstd[B]), cc.CreateLoad(regD[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

/////////////////////////////////////////////////////////////////////////////
// Double-precision floating point math.

void JitCompiler::EmitADDF_RR()
{
	cc.CreateStore(cc.CreateFAdd(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])), regF[A]);
}

void JitCompiler::EmitADDF_RK()
{
	cc.CreateStore(cc.CreateFAdd(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C])), regF[A]);
}

void JitCompiler::EmitSUBF_RR()
{
	cc.CreateStore(cc.CreateFSub(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])), regF[A]);
}

void JitCompiler::EmitSUBF_RK()
{
	cc.CreateStore(cc.CreateFSub(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C])), regF[A]);
}

void JitCompiler::EmitSUBF_KR()
{
	cc.CreateStore(cc.CreateFSub(ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[B]), cc.CreateLoad(regF[C])), regF[A]);
}

void JitCompiler::EmitMULF_RR()
{
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])), regF[A]);
}

void JitCompiler::EmitMULF_RK()
{
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C])), regF[A]);
}

void JitCompiler::EmitDIVF_RR()
{
	IRBasicBlock* exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateFCmpUEQ(cc.CreateLoad(regF[C]), ircontext->getConstantFloat(ircontext->getDoubleTy(), 0.0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])), regF[A]);
}

void JitCompiler::EmitDIVF_RK()
{
	if (konstf[C] == 0.)
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
	else
	{
		cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C])), regF[A]);
	}
}

void JitCompiler::EmitDIVF_KR()
{
	IRBasicBlock* exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateFCmpUEQ(cc.CreateLoad(regF[C]), ircontext->getConstantFloat(ircontext->getDoubleTy(), 0.0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateFDiv(ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[B]), cc.CreateLoad(regF[C])), regF[A]);
}

static double DoubleModF(double a, double b)
{
	return a - floor(a / b) * b;
}

void JitCompiler::EmitMODF_RR()
{
	IRBasicBlock* exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateFCmpUEQ(cc.CreateLoad(regF[C]), ircontext->getConstantFloat(ircontext->getDoubleTy(), 0.0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateCall(GetNativeFunc<double, double, double>("__DoubleModF", DoubleModF), { cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C]) }), regF[A]);
}

void JitCompiler::EmitMODF_RK()
{
	if (konstf[C] == 0.)
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
	else
	{
		cc.CreateStore(cc.CreateCall(GetNativeFunc<double, double, double>("__DoubleModF", DoubleModF), { cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]) }), regF[A]);
	}
}

void JitCompiler::EmitMODF_KR()
{
	IRBasicBlock* exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateFCmpUEQ(cc.CreateLoad(regF[C]), ircontext->getConstantFloat(ircontext->getDoubleTy(), 0.0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateStore(cc.CreateCall(GetNativeFunc<double, double, double>("__DoubleModF", DoubleModF), { ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[B]), cc.CreateLoad(regF[C]) }), regF[A]);
}

void JitCompiler::EmitPOWF_RR()
{
	cc.CreateStore(cc.CreateCall(GetNativeFunc<double, double, double>("__g_pow", g_pow), { cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C]) }), regF[A]);
}

void JitCompiler::EmitPOWF_RK()
{
	cc.CreateStore(cc.CreateCall(GetNativeFunc<double, double, double>("__g_pow", g_pow), { cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]) }), regF[A]);
}

void JitCompiler::EmitPOWF_KR()
{
	cc.CreateStore(cc.CreateCall(GetNativeFunc<double, double, double>("__g_pow", g_pow), { ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[B]), cc.CreateLoad(regF[C]) }), regF[A]);
}

void JitCompiler::EmitMINF_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regF[B]);
	IRValue* val1 = cc.CreateLoad(regF[C]);

	cc.CreateCondBr(cc.CreateFCmpULE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regF[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regF[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMINF_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regF[B]);
	IRValue* val1 = ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]);

	cc.CreateCondBr(cc.CreateFCmpULE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regF[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regF[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}
	
void JitCompiler::EmitMAXF_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regF[B]);
	IRValue* val1 = cc.CreateLoad(regF[C]);

	cc.CreateCondBr(cc.CreateFCmpUGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regF[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regF[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAXF_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = cc.CreateLoad(regF[B]);
	IRValue* val1 = ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]);

	cc.CreateCondBr(cc.CreateFCmpUGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(val0, regF[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(val1, regF[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitATAN2()
{
	cc.CreateStore(cc.CreateFMul(cc.CreateCall(GetNativeFunc<double, double, double>("__g_atan2", g_atan2), { cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C]) }), ircontext->getConstantFloat(ircontext->getFloatTy(), 180 / M_PI)), regF[A]);
}

void JitCompiler::EmitFLOP()
{
	if (C == FLOP_NEG)
	{
		cc.CreateStore(cc.CreateFNeg(cc.CreateLoad(regF[B])), regF[A]);
	}
	else
	{
		IRValue* v = cc.CreateLoad(regF[B]);

		if (C == FLOP_TAN_DEG)
		{
			v = cc.CreateFMul(v, ircontext->getConstantFloat(ircontext->getFloatTy(), M_PI / 180));
		}

		typedef double(*FuncPtr)(double);
		const char* funcname = "";
		FuncPtr func = nullptr;
		switch (C)
		{
		default: I_Error("Unknown OP_FLOP subfunction");
		case FLOP_ABS:		func = fabs; funcname = "__fabs"; break;
		case FLOP_EXP:		func = g_exp; funcname = "__g_exp"; break;
		case FLOP_LOG:		func = g_log; funcname = "__g_log"; break;
		case FLOP_LOG10:	func = g_log10; funcname = "__g_log10"; break;
		case FLOP_SQRT:		func = g_sqrt; funcname = "__g_sqrt"; break;
		case FLOP_CEIL:		func = ceil; funcname = "__ceil"; break;
		case FLOP_FLOOR:	func = floor; funcname = "__floor"; break;
		case FLOP_ACOS:		func = g_acos; funcname = "__g_acos"; break;
		case FLOP_ASIN:		func = g_asin; funcname = "__g_asin"; break;
		case FLOP_ATAN:		func = g_atan; funcname = "__g_atan"; break;
		case FLOP_COS:		func = g_cos; funcname = "__g_cos"; break;
		case FLOP_SIN:		func = g_sin; funcname = "__g_sin"; break;
		case FLOP_TAN:		func = g_tan; funcname = "__g_tan"; break;
		case FLOP_ACOS_DEG:	func = g_acos; funcname = "__g_acos"; break;
		case FLOP_ASIN_DEG:	func = g_asin; funcname = "__g_asin"; break;
		case FLOP_ATAN_DEG:	func = g_atan; funcname = "__g_atan"; break;
		case FLOP_COS_DEG:	func = g_cosdeg; funcname = "__g_cosdeg"; break;
		case FLOP_SIN_DEG:	func = g_sindeg; funcname = "__g_sindeg"; break;
		case FLOP_TAN_DEG:	func = g_tan; funcname = "__g_tan"; break;
		case FLOP_COSH:		func = g_cosh; funcname = "__g_cosh"; break;
		case FLOP_SINH:		func = g_sinh; funcname = "__g_sinh"; break;
		case FLOP_TANH:		func = g_tanh; funcname = "__g_tanh"; break;
		case FLOP_ROUND:	func = round; funcname = "__round"; break;
		}

		IRValue* result = cc.CreateCall(GetNativeFunc<double, double>(funcname, func), { v });

		if (C == FLOP_ACOS_DEG || C == FLOP_ASIN_DEG || C == FLOP_ATAN_DEG)
		{
			result = cc.CreateFMul(result, ircontext->getConstantFloat(ircontext->getFloatTy(), 180 / M_PI));
		}

		cc.CreateStore(result, regF[A]);
	}
}

void JitCompiler::EmitEQF_R()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		EmitVectorComparison(1, check, fail, success);
	});
}

void JitCompiler::EmitEQF_K()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		bool approx = static_cast<bool>(A & CMP_APPROX);
		if (!approx)
		{
			IRValue* result;
			if (check)
				result = cc.CreateFCmpUEQ(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]));
			else
				result = cc.CreateFCmpUNE(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]));

			cc.CreateCondBr(result, fail, success);
		}
		else
		{
			IRValue* diff = cc.CreateFSub(ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]), cc.CreateLoad(regF[B]));
			IRValue* result;
			if (check)
				result = cc.CreateAnd(
					cc.CreateFCmpUGT(diff, ircontext->getConstantFloat(ircontext->getDoubleTy(), -VM_EPSILON)),
					cc.CreateFCmpULT(diff, ircontext->getConstantFloat(ircontext->getDoubleTy(), VM_EPSILON)));
			else
				result = cc.CreateOr(
					cc.CreateFCmpULE(diff, ircontext->getConstantFloat(ircontext->getDoubleTy(), -VM_EPSILON)),
					cc.CreateFCmpUGE(diff, ircontext->getConstantFloat(ircontext->getDoubleTy(), VM_EPSILON)));

			cc.CreateCondBr(result, fail, success);
		}
	});
}

void JitCompiler::EmitLTF_RR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LTF_RR.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULT(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C]));
		else
			result = cc.CreateFCmpUGE(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C]));

		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLTF_RK()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LTF_RK.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULT(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]));
		else
			result = cc.CreateFCmpUGE(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]));

		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLTF_KR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LTF_KR.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULT(ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[B]), cc.CreateLoad(regF[C]));
		else
			result = cc.CreateFCmpUGE(ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[B]), cc.CreateLoad(regF[C]));

		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLEF_RR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LEF_RR.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULE(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C]));
		else
			result = cc.CreateFCmpUGT(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C]));

		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLEF_RK()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LEF_RK.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULE(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]));
		else
			result = cc.CreateFCmpUGT(cc.CreateLoad(regF[B]), ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]));

		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitLEF_KR()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LEF_KR.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULE(ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[B]), cc.CreateLoad(regF[C]));
		else
			result = cc.CreateFCmpUGT(ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[B]), cc.CreateLoad(regF[C]));

		cc.CreateCondBr(result, fail, success);
	});
}

/////////////////////////////////////////////////////////////////////////////
// Vector math. (2D)

void JitCompiler::EmitNEGV2()
{
	auto mask = ircontext->getConstantFloat(ircontext->getDoubleTy(), -0.0);
	cc.CreateStore(cc.CreateXor(cc.CreateLoad(regF[B]), mask), regF[A]);
	cc.CreateStore(cc.CreateXor(cc.CreateLoad(regF[B + 1]), mask), regF[A + 1]);
}

void JitCompiler::EmitADDV2_RR()
{
	cc.CreateStore(cc.CreateFAdd(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])), regF[A]);
	cc.CreateStore(cc.CreateFAdd(cc.CreateLoad(regF[B + 1]), cc.CreateLoad(regF[C + 1])), regF[A + 1]);
}

void JitCompiler::EmitSUBV2_RR()
{
	cc.CreateStore(cc.CreateFSub(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])), regF[A]);
	cc.CreateStore(cc.CreateFSub(cc.CreateLoad(regF[B + 1]), cc.CreateLoad(regF[C + 1])), regF[A + 1]);
}

void JitCompiler::EmitDOTV2_RR()
{
	cc.CreateStore(
		cc.CreateFAdd(
			cc.CreateFMul(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])),
			cc.CreateFMul(cc.CreateLoad(regF[B + 1]), cc.CreateLoad(regF[C + 1]))),
		regF[A]);
}

void JitCompiler::EmitMULVF2_RR()
{
	IRValue* rc = cc.CreateLoad(regF[C]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B]), rc), regF[A]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B + 1]), rc), regF[A + 1]);
}

void JitCompiler::EmitMULVF2_RK()
{
	IRValue* rc = ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B]), rc), regF[A]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B + 1]), rc), regF[A + 1]);
}

void JitCompiler::EmitDIVVF2_RR()
{
	IRValue* rc = cc.CreateLoad(regF[C]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B]), rc), regF[A]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B + 1]), rc), regF[A + 1]);
}

void JitCompiler::EmitDIVVF2_RK()
{
	IRValue* rc = ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B]), rc), regF[A]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B + 1]), rc), regF[A + 1]);
}

void JitCompiler::EmitLENV2()
{
	IRValue* x = cc.CreateLoad(regF[B]);
	IRValue* y = cc.CreateLoad(regF[B + 1]);
	IRValue* dotproduct = cc.CreateFAdd(cc.CreateFMul(x, x), cc.CreateFMul(y, y));
	IRValue* len = cc.CreateCall(GetNativeFunc<double, double>("__g_sqrt", g_sqrt), { dotproduct });
	cc.CreateStore(len, regF[A]);
}

void JitCompiler::EmitEQV2_R()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		EmitVectorComparison(2, check, fail, success);
	});
}

void JitCompiler::EmitEQV2_K()
{
	I_Error("EQV2_K is not used.");
}

/////////////////////////////////////////////////////////////////////////////
// Vector math. (3D)

void JitCompiler::EmitNEGV3()
{
	auto mask = ircontext->getConstantFloat(ircontext->getDoubleTy(), -0.0);
	cc.CreateStore(cc.CreateXor(cc.CreateLoad(regF[B]), mask), regF[A]);
	cc.CreateStore(cc.CreateXor(cc.CreateLoad(regF[B + 1]), mask), regF[A + 1]);
	cc.CreateStore(cc.CreateXor(cc.CreateLoad(regF[B + 2]), mask), regF[A + 2]);
}

void JitCompiler::EmitADDV3_RR()
{
	cc.CreateStore(cc.CreateFAdd(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])), regF[A]);
	cc.CreateStore(cc.CreateFAdd(cc.CreateLoad(regF[B + 1]), cc.CreateLoad(regF[C + 1])), regF[A + 1]);
	cc.CreateStore(cc.CreateFAdd(cc.CreateLoad(regF[B + 2]), cc.CreateLoad(regF[C + 2])), regF[A + 2]);
}

void JitCompiler::EmitSUBV3_RR()
{
	cc.CreateStore(cc.CreateFSub(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])), regF[A]);
	cc.CreateStore(cc.CreateFSub(cc.CreateLoad(regF[B + 1]), cc.CreateLoad(regF[C + 1])), regF[A + 1]);
	cc.CreateStore(cc.CreateFSub(cc.CreateLoad(regF[B + 2]), cc.CreateLoad(regF[C + 2])), regF[A + 2]);
}

void JitCompiler::EmitDOTV3_RR()
{
	cc.CreateStore(
		cc.CreateFAdd(
			cc.CreateFAdd(
				cc.CreateFMul(cc.CreateLoad(regF[B]), cc.CreateLoad(regF[C])),
				cc.CreateFMul(cc.CreateLoad(regF[B + 1]), cc.CreateLoad(regF[C + 1]))),
			cc.CreateFMul(cc.CreateLoad(regF[B + 2]), cc.CreateLoad(regF[C + 2]))),
		regF[A]);
}

void JitCompiler::EmitCROSSV_RR()
{
	IRValue* a0 = cc.CreateLoad(regF[B]);
	IRValue* a1 = cc.CreateLoad(regF[B + 1]);
	IRValue* a2 = cc.CreateLoad(regF[B + 2]);
	IRValue* b0 = cc.CreateLoad(regF[C]);
	IRValue* b1 = cc.CreateLoad(regF[C + 1]);
	IRValue* b2 = cc.CreateLoad(regF[C + 2]);
	cc.CreateStore(cc.CreateFSub(cc.CreateFMul(a1, b2), cc.CreateFMul(a2, b1)), regF[A]);
	cc.CreateStore(cc.CreateFSub(cc.CreateFMul(a2, b0), cc.CreateFMul(a0, b2)), regF[A + 1]);
	cc.CreateStore(cc.CreateFSub(cc.CreateFMul(a0, b1), cc.CreateFMul(a1, b0)), regF[A + 2]);
}

void JitCompiler::EmitMULVF3_RR()
{
	IRValue* rc = cc.CreateLoad(regF[C]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B]), rc), regF[A]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B + 1]), rc), regF[A + 1]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B + 2]), rc), regF[A + 2]);
}

void JitCompiler::EmitMULVF3_RK()
{
	IRValue* rc = ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B]), rc), regF[A]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B + 1]), rc), regF[A + 1]);
	cc.CreateStore(cc.CreateFMul(cc.CreateLoad(regF[B + 2]), rc), regF[A + 2]);
}

void JitCompiler::EmitDIVVF3_RR()
{
	IRValue* rc = cc.CreateLoad(regF[C]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B]), rc), regF[A]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B + 1]), rc), regF[A + 1]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B + 2]), rc), regF[A + 2]);
}

void JitCompiler::EmitDIVVF3_RK()
{
	IRValue* rc = ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[C]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B]), rc), regF[A]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B + 1]), rc), regF[A + 1]);
	cc.CreateStore(cc.CreateFDiv(cc.CreateLoad(regF[B + 2]), rc), regF[A + 2]);
}

void JitCompiler::EmitLENV3()
{
	IRValue* x = cc.CreateLoad(regF[B]);
	IRValue* y = cc.CreateLoad(regF[B + 1]);
	IRValue* z = cc.CreateLoad(regF[B + 2]);
	IRValue* dotproduct = cc.CreateFAdd(cc.CreateFAdd(cc.CreateFMul(x, x), cc.CreateFMul(y, y)), cc.CreateFMul(z, z));
	IRValue* len = cc.CreateCall(GetNativeFunc<double, double>("__g_sqrt", g_sqrt), { dotproduct });
	cc.CreateStore(len, regF[A]);
}

void JitCompiler::EmitEQV3_R()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		EmitVectorComparison(3, check, fail, success);
	});
}
	
void JitCompiler::EmitEQV3_K()
{
	I_Error("EQV3_K is not used.");
}

/////////////////////////////////////////////////////////////////////////////
// Pointer math.

void JitCompiler::EmitADDA_RR()
{
	// Leave null pointers as null pointers

	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* nullvalue = ircontext->getConstantInt(ircontext->getInt8PtrTy(), 0);
	IRValue* ptr = cc.CreateLoad(regA[B]);

	cc.CreateCondBr(cc.CreateICmpEQ(ptr, nullvalue), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(ptr, regA[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(cc.CreateGEP(ptr, { cc.CreateLoad(regD[C]) }), regA[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitADDA_RK()
{
	// Leave null pointers as null pointers

	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* nullvalue = ircontext->getConstantInt(ircontext->getInt8PtrTy(), 0);
	IRValue* ptr = cc.CreateLoad(regA[B]);

	cc.CreateCondBr(cc.CreateICmpEQ(ptr, nullvalue), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	cc.CreateStore(ptr, regA[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	cc.CreateStore(cc.CreateGEP(ptr, { ircontext->getConstantInt(konstd[C]) }), regA[A]);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitSUBA()
{
	cc.CreateStore(cc.CreateGEP(cc.CreateLoad(regA[B]), { cc.CreateNeg(cc.CreateLoad(regD[C])) }), regA[A]);
}

void JitCompiler::EmitEQA_R()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpEQ(cc.CreateLoad(regA[B]), cc.CreateLoad(regA[C]));
		else       result = cc.CreateICmpNE(cc.CreateLoad(regA[B]), cc.CreateLoad(regA[C]));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitEQA_K()
{
	EmitComparisonOpcode([&](bool check, IRBasicBlock* fail, IRBasicBlock* success) {
		IRValue* result;
		if (check) result = cc.CreateICmpEQ(cc.CreateLoad(regA[B]), ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)konsta[C].v));
		else       result = cc.CreateICmpNE(cc.CreateLoad(regA[B]), ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)konsta[C].v));
		cc.CreateCondBr(result, fail, success);
	});
}

void JitCompiler::EmitVectorComparison(int N, bool check, IRBasicBlock* fail, IRBasicBlock* success)
{
	IRValue* result = nullptr;
	bool approx = static_cast<bool>(A & CMP_APPROX);
	for (int i = 0; i < N; i++)
	{
		IRValue* elementresult;
		if (!approx)
		{
			if (check)
				elementresult = cc.CreateFCmpUEQ(cc.CreateLoad(regF[B + i]), cc.CreateLoad(regF[C + i]));
			else
				elementresult = cc.CreateFCmpUNE(cc.CreateLoad(regF[B + i]), cc.CreateLoad(regF[C + i]));
		}
		else
		{
			IRValue* diff = cc.CreateFSub(cc.CreateLoad(regF[C + i]), cc.CreateLoad(regF[B + i]));
			if (check)
				elementresult = cc.CreateAnd(
					cc.CreateFCmpUGT(diff, ircontext->getConstantFloat(ircontext->getDoubleTy(), -VM_EPSILON)),
					cc.CreateFCmpULT(diff, ircontext->getConstantFloat(ircontext->getDoubleTy(), VM_EPSILON)));
			else
				elementresult = cc.CreateOr(
					cc.CreateFCmpULE(diff, ircontext->getConstantFloat(ircontext->getDoubleTy(), -VM_EPSILON)),
					cc.CreateFCmpUGE(diff, ircontext->getConstantFloat(ircontext->getDoubleTy(), VM_EPSILON)));
		}

		if (i == 0)
			result = elementresult;
		else
			result = cc.CreateAdd(result, elementresult);
	}
	cc.CreateCondBr(result, fail, success);
}
