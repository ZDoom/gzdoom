
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
	cc.CreateCall(GetNativeFunc<void, FString*, FString*, FString*>("__ConcatString", ConcatString), { LoadS(A), LoadS(B), LoadS(C) });
}

static int StringLength(FString* str)
{
	return static_cast<int>(str->Len());
}

void JitCompiler::EmitLENS()
{
	StoreD(cc.CreateCall(GetNativeFunc<int, FString*>("__StringLength", StringLength), { LoadS(B) }), A);
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
	EmitComparisonOpcode([&](bool check) {

		IRValue* arg0 = static_cast<bool>(A & CMP_BK) ? ConstS(B) : LoadS(B);
		IRValue* arg1 = static_cast<bool>(A & CMP_CK) ? ConstS(C) : LoadS(C);

		IRValue* result;
		if (static_cast<bool>(A & CMP_APPROX))
			result = cc.CreateCall(GetNativeFunc<int, FString*, FString*>("__StringCompareNoCase", StringCompareNoCase), { arg0, arg1 });
		else
			result = cc.CreateCall(GetNativeFunc<int, FString*, FString*>("__StringCompare", StringCompare), { arg0, arg1 });

		IRValue* zero = ConstValueD(0);

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

		return result;
	});
}

/////////////////////////////////////////////////////////////////////////////
// Integer math.

void JitCompiler::EmitSLL_RR()
{
	StoreD(cc.CreateShl(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitSLL_RI()
{
	StoreD(cc.CreateShl(LoadD(B), ConstValueD(C)), A);
}

void JitCompiler::EmitSLL_KR()
{
	StoreD(cc.CreateShl(ConstD(B), LoadD(C)), A);
}

void JitCompiler::EmitSRL_RR()
{
	StoreD(cc.CreateLShr(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitSRL_RI()
{
	StoreD(cc.CreateLShr(LoadD(B), ConstValueD(C)), A);
}

void JitCompiler::EmitSRL_KR()
{
	StoreD(cc.CreateLShr(ConstD(B), LoadD(C)), A);
}

void JitCompiler::EmitSRA_RR()
{
	StoreD(cc.CreateAShr(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitSRA_RI()
{
	StoreD(cc.CreateAShr(LoadD(B), ConstValueD(C)), A);
}

void JitCompiler::EmitSRA_KR()
{
	StoreD(cc.CreateAShr(ConstD(B), LoadD(C)), A);
}

void JitCompiler::EmitADD_RR()
{
	StoreD(cc.CreateAdd(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitADD_RK()
{
	StoreD(cc.CreateAdd(LoadD(B), ConstD(C)), A);
}

void JitCompiler::EmitADDI()
{
	StoreD(cc.CreateAdd(LoadD(B), ConstValueD(Cs)), A);
}

void JitCompiler::EmitSUB_RR()
{
	StoreD(cc.CreateSub(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitSUB_RK()
{
	StoreD(cc.CreateSub(LoadD(B), ConstD(C)), A);
}

void JitCompiler::EmitSUB_KR()
{
	StoreD(cc.CreateSub(ConstD(B), LoadD(C)), A);
}

void JitCompiler::EmitMUL_RR()
{
	StoreD(cc.CreateMul(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitMUL_RK()
{
	StoreD(cc.CreateMul(LoadD(B), ConstD(C)), A);
}

void JitCompiler::EmitDIV_RR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(C), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreD(cc.CreateSDiv(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitDIV_RK()
{
	if (konstd[C] != 0)
	{
		StoreD(cc.CreateSDiv(LoadD(B), ConstD(C)), A);
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
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(C), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreD(cc.CreateSDiv(ConstD(B), LoadD(B)), A);
}

void JitCompiler::EmitDIVU_RR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(C), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreD(cc.CreateUDiv(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitDIVU_RK()
{
	if (konstd[C] != 0)
	{
		StoreD(cc.CreateUDiv(LoadD(B), ConstD(C)), A);
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
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(C), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreD(cc.CreateUDiv(ConstD(B), LoadD(B)), A);
}

void JitCompiler::EmitMOD_RR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(C), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreD(cc.CreateSRem(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitMOD_RK()
{
	if (konstd[C] != 0)
	{
		StoreD(cc.CreateSRem(LoadD(B), ConstD(C)), A);
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
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(C), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreD(cc.CreateSRem(ConstD(B), LoadD(B)), A);
}

void JitCompiler::EmitMODU_RR()
{
	auto exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(C), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreD(cc.CreateURem(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitMODU_RK()
{
	if (konstd[C] != 0)
	{
		StoreD(cc.CreateURem(LoadD(B), ConstD(C)), A);
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
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(C), ConstValueD(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreD(cc.CreateURem(ConstD(B), LoadD(B)), A);
}

void JitCompiler::EmitAND_RR()
{
	StoreD(cc.CreateAnd(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitAND_RK()
{
	StoreD(cc.CreateAnd(LoadD(B), ConstD(C)), A);
}

void JitCompiler::EmitOR_RR()
{
	StoreD(cc.CreateOr(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitOR_RK()
{
	StoreD(cc.CreateOr(LoadD(B), ConstD(C)), A);
}

void JitCompiler::EmitXOR_RR()
{
	StoreD(cc.CreateXor(LoadD(B), LoadD(C)), A);
}

void JitCompiler::EmitXOR_RK()
{
	StoreD(cc.CreateXor(LoadD(B), ConstD(C)), A);
}

void JitCompiler::EmitMIN_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadD(B);
	IRValue* val1 = LoadD(C);

	cc.CreateCondBr(cc.CreateICmpSLE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreD(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreD(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMIN_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadD(B);
	IRValue* val1 = ConstD(C);

	cc.CreateCondBr(cc.CreateICmpSLE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreD(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreD(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAX_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadD(B);
	IRValue* val1 = LoadD(C);

	cc.CreateCondBr(cc.CreateICmpSGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreD(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreD(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAX_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadD(B);
	IRValue* val1 = ConstD(C);

	cc.CreateCondBr(cc.CreateICmpSGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreD(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreD(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMINU_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadD(B);
	IRValue* val1 = LoadD(C);

	cc.CreateCondBr(cc.CreateICmpULE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreD(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreD(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMINU_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadD(B);
	IRValue* val1 = ConstD(C);

	cc.CreateCondBr(cc.CreateICmpULE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreD(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreD(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAXU_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadD(B);
	IRValue* val1 = LoadD(C);

	cc.CreateCondBr(cc.CreateICmpUGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreD(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreD(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAXU_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadD(B);
	IRValue* val1 = ConstD(C);

	cc.CreateCondBr(cc.CreateICmpUGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreD(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreD(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitABS()
{
	IRValue* srcB = LoadD(B);
	IRValue* tmp = cc.CreateAShr(srcB, ConstValueD(31));
	StoreD(cc.CreateSub(cc.CreateXor(tmp, srcB), tmp), A);
}

void JitCompiler::EmitNEG()
{
	StoreD(cc.CreateNeg(LoadD(B)), A);
}

void JitCompiler::EmitNOT()
{
	StoreD(cc.CreateNot(LoadD(B)), A);
}

void JitCompiler::EmitEQ_R()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpEQ(LoadD(B), LoadD(C));
		else       result = cc.CreateICmpNE(LoadD(B), LoadD(C));
		return result;
	});
}

void JitCompiler::EmitEQ_K()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpEQ(LoadD(B), ConstD(C));
		else       result = cc.CreateICmpNE(LoadD(B), ConstD(C));
		return result;
	});
}

void JitCompiler::EmitLT_RR()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLT(LoadD(B), LoadD(C));
		else       result = cc.CreateICmpSGE(LoadD(B), LoadD(C));
		return result;
	});
}

void JitCompiler::EmitLT_RK()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLT(LoadD(B), ConstD(C));
		else       result = cc.CreateICmpSGE(LoadD(B), ConstD(C));
		return result;
	});
}

void JitCompiler::EmitLT_KR()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLT(ConstD(B), LoadD(C));
		else       result = cc.CreateICmpSGE(ConstD(B), LoadD(C));
		return result;
	});
}

void JitCompiler::EmitLE_RR()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLE(LoadD(B), LoadD(C));
		else       result = cc.CreateICmpSGT(LoadD(B), LoadD(C));
		return result;
	});
}

void JitCompiler::EmitLE_RK()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLE(LoadD(B), ConstD(C));
		else       result = cc.CreateICmpSGT(LoadD(B), ConstD(C));
		return result;
	});
}

void JitCompiler::EmitLE_KR()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpSLE(ConstD(B), LoadD(C));
		else       result = cc.CreateICmpSGT(ConstD(B), LoadD(C));
		return result;
	});
}

void JitCompiler::EmitLTU_RR()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpULT(LoadD(B), LoadD(C));
		else       result = cc.CreateICmpUGE(LoadD(B), LoadD(C));
		return result;
	});
}

void JitCompiler::EmitLTU_RK()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpULT(LoadD(B), ConstD(C));
		else       result = cc.CreateICmpUGE(LoadD(B), ConstD(C));
		return result;
	});
}

void JitCompiler::EmitLTU_KR()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpULT(ConstD(B), LoadD(C));
		else       result = cc.CreateICmpUGE(ConstD(B), LoadD(C));
		return result;
	});
}

void JitCompiler::EmitLEU_RR()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpULE(LoadD(B), LoadD(C));
		else       result = cc.CreateICmpUGT(LoadD(B), LoadD(C));
		return result;
	});
}

void JitCompiler::EmitLEU_RK()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpULE(LoadD(B), ConstD(C));
		else       result = cc.CreateICmpUGT(LoadD(B), ConstD(C));
		return result;
	});
}

void JitCompiler::EmitLEU_KR()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpULE(ConstD(B), LoadD(C));
		else       result = cc.CreateICmpUGT(ConstD(B), LoadD(C));
		return result;
	});
}

/////////////////////////////////////////////////////////////////////////////
// Double-precision floating point math.

void JitCompiler::EmitADDF_RR()
{
	StoreF(cc.CreateFAdd(LoadF(B), LoadF(C)), A);
}

void JitCompiler::EmitADDF_RK()
{
	StoreF(cc.CreateFAdd(LoadF(B), ConstF(C)), A);
}

void JitCompiler::EmitSUBF_RR()
{
	StoreF(cc.CreateFSub(LoadF(B), LoadF(C)), A);
}

void JitCompiler::EmitSUBF_RK()
{
	StoreF(cc.CreateFSub(LoadF(B), ConstF(C)), A);
}

void JitCompiler::EmitSUBF_KR()
{
	StoreF(cc.CreateFSub(ConstF(B), LoadF(C)), A);
}

void JitCompiler::EmitMULF_RR()
{
	StoreF(cc.CreateFMul(LoadF(B), LoadF(C)), A);
}

void JitCompiler::EmitMULF_RK()
{
	StoreF(cc.CreateFMul(LoadF(B), ConstF(C)), A);
}

void JitCompiler::EmitDIVF_RR()
{
	IRBasicBlock* exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateFCmpUEQ(LoadF(C), ConstValueF(0.0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreF(cc.CreateFDiv(LoadF(B), LoadF(C)), A);
}

void JitCompiler::EmitDIVF_RK()
{
	if (konstf[C] == 0.)
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
	else
	{
		StoreF(cc.CreateFDiv(LoadF(B), ConstF(C)), A);
	}
}

void JitCompiler::EmitDIVF_KR()
{
	IRBasicBlock* exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateFCmpUEQ(LoadF(C), ConstValueF(0.0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreF(cc.CreateFDiv(ConstF(B), LoadF(C)), A);
}

static double DoubleModF(double a, double b)
{
	return a - floor(a / b) * b;
}

void JitCompiler::EmitMODF_RR()
{
	IRBasicBlock* exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateFCmpUEQ(LoadF(C), ConstValueF(0.0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreF(cc.CreateCall(GetNativeFunc<double, double, double>("__DoubleModF", DoubleModF), { LoadF(B), LoadF(C) }), A);
}

void JitCompiler::EmitMODF_RK()
{
	if (konstf[C] == 0.)
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
	else
	{
		StoreF(cc.CreateCall(GetNativeFunc<double, double, double>("__DoubleModF", DoubleModF), { LoadF(B), ConstF(C) }), A);
	}
}

void JitCompiler::EmitMODF_KR()
{
	IRBasicBlock* exceptionbb = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateFCmpUEQ(LoadF(C), ConstValueF(0.0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	StoreF(cc.CreateCall(GetNativeFunc<double, double, double>("__DoubleModF", DoubleModF), { ConstF(B), LoadF(C) }), A);
}

void JitCompiler::EmitPOWF_RR()
{
	StoreF(cc.CreateCall(GetNativeFunc<double, double, double>("__g_pow", g_pow), { LoadF(B), LoadF(C) }), A);
}

void JitCompiler::EmitPOWF_RK()
{
	StoreF(cc.CreateCall(GetNativeFunc<double, double, double>("__g_pow", g_pow), { LoadF(B), ConstF(C) }), A);
}

void JitCompiler::EmitPOWF_KR()
{
	StoreF(cc.CreateCall(GetNativeFunc<double, double, double>("__g_pow", g_pow), { ConstF(B), LoadF(C) }), A);
}

void JitCompiler::EmitMINF_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadF(B);
	IRValue* val1 = LoadF(C);

	cc.CreateCondBr(cc.CreateFCmpULE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreF(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreF(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMINF_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadF(B);
	IRValue* val1 = ConstF(C);

	cc.CreateCondBr(cc.CreateFCmpULE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreF(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreF(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}
	
void JitCompiler::EmitMAXF_RR()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadF(B);
	IRValue* val1 = LoadF(C);

	cc.CreateCondBr(cc.CreateFCmpUGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreF(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreF(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitMAXF_RK()
{
	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* val0 = LoadF(B);
	IRValue* val1 = ConstF(C);

	cc.CreateCondBr(cc.CreateFCmpUGE(val0, val1), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreF(val0, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreF(val1, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitATAN2()
{
	StoreF(cc.CreateFMul(cc.CreateCall(GetNativeFunc<double, double, double>("__g_atan2", g_atan2), { LoadF(B), LoadF(C) }), ConstValueF(180 / M_PI)), A);
}

void JitCompiler::EmitFLOP()
{
	if (C == FLOP_NEG)
	{
		StoreF(cc.CreateFNeg(LoadF(B)), A);
	}
	else
	{
		IRValue* v = LoadF(B);

		if (C == FLOP_TAN_DEG)
		{
			v = cc.CreateFMul(v, ConstValueF(M_PI / 180));
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
			result = cc.CreateFMul(result, ConstValueF(180 / M_PI));
		}

		StoreF(result, A);
	}
}

void JitCompiler::EmitEQF_R()
{
	EmitComparisonOpcode([&](bool check) {
		return EmitVectorComparison(1, check);
	});
}

void JitCompiler::EmitEQF_K()
{
	EmitComparisonOpcode([&](bool check) {
		bool approx = static_cast<bool>(A & CMP_APPROX);
		if (!approx)
		{
			IRValue* result;
			if (check)
				result = cc.CreateFCmpUEQ(LoadF(B), ConstF(C));
			else
				result = cc.CreateFCmpUNE(LoadF(B), ConstF(C));

			return result;
		}
		else
		{
			IRValue* diff = cc.CreateFSub(ConstF(C), LoadF(B));
			IRValue* result;
			if (check)
				result = cc.CreateAnd(
					cc.CreateFCmpUGT(diff, ConstValueF(-VM_EPSILON)),
					cc.CreateFCmpULT(diff, ConstValueF(VM_EPSILON)));
			else
				result = cc.CreateOr(
					cc.CreateFCmpULE(diff, ConstValueF(-VM_EPSILON)),
					cc.CreateFCmpUGE(diff, ConstValueF(VM_EPSILON)));

			return result;
		}
	});
}

void JitCompiler::EmitLTF_RR()
{
	EmitComparisonOpcode([&](bool check) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LTF_RR.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULT(LoadF(B), LoadF(C));
		else
			result = cc.CreateFCmpUGE(LoadF(B), LoadF(C));

		return result;
	});
}

void JitCompiler::EmitLTF_RK()
{
	EmitComparisonOpcode([&](bool check) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LTF_RK.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULT(LoadF(B), ConstF(C));
		else
			result = cc.CreateFCmpUGE(LoadF(B), ConstF(C));

		return result;
	});
}

void JitCompiler::EmitLTF_KR()
{
	EmitComparisonOpcode([&](bool check) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LTF_KR.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULT(ConstF(B), LoadF(C));
		else
			result = cc.CreateFCmpUGE(ConstF(B), LoadF(C));

		return result;
	});
}

void JitCompiler::EmitLEF_RR()
{
	EmitComparisonOpcode([&](bool check) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LEF_RR.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULE(LoadF(B), LoadF(C));
		else
			result = cc.CreateFCmpUGT(LoadF(B), LoadF(C));

		return result;
	});
}

void JitCompiler::EmitLEF_RK()
{
	EmitComparisonOpcode([&](bool check) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LEF_RK.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULE(LoadF(B), ConstF(C));
		else
			result = cc.CreateFCmpUGT(LoadF(B), ConstF(C));

		return result;
	});
}

void JitCompiler::EmitLEF_KR()
{
	EmitComparisonOpcode([&](bool check) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LEF_KR.\n");

		IRValue* result;
		if (check)
			result = cc.CreateFCmpULE(ConstF(B), LoadF(C));
		else
			result = cc.CreateFCmpUGT(ConstF(B), LoadF(C));

		return result;
	});
}

/////////////////////////////////////////////////////////////////////////////
// Vector math. (2D)

void JitCompiler::EmitNEGV2()
{
	StoreF(cc.CreateFNeg(LoadF(B)), A);
	StoreF(cc.CreateFNeg(LoadF(B + 1)), A + 1);
}

void JitCompiler::EmitADDV2_RR()
{
	StoreF(cc.CreateFAdd(LoadF(B), LoadF(C)), A);
	StoreF(cc.CreateFAdd(LoadF(B + 1), LoadF(C + 1)), A + 1);
}

void JitCompiler::EmitSUBV2_RR()
{
	StoreF(cc.CreateFSub(LoadF(B), LoadF(C)), A);
	StoreF(cc.CreateFSub(LoadF(B + 1), LoadF(C + 1)), A + 1);
}

void JitCompiler::EmitDOTV2_RR()
{
	StoreF(
		cc.CreateFAdd(
			cc.CreateFMul(LoadF(B), LoadF(C)),
			cc.CreateFMul(LoadF(B + 1), LoadF(C + 1))),
		A);
}

void JitCompiler::EmitMULVF2_RR()
{
	IRValue* rc = LoadF(C);
	StoreF(cc.CreateFMul(LoadF(B), rc), A);
	StoreF(cc.CreateFMul(LoadF(B + 1), rc), A + 1);
}

void JitCompiler::EmitMULVF2_RK()
{
	IRValue* rc = ConstF(C);
	StoreF(cc.CreateFMul(LoadF(B), rc), A);
	StoreF(cc.CreateFMul(LoadF(B + 1), rc), A + 1);
}

void JitCompiler::EmitDIVVF2_RR()
{
	IRValue* rc = LoadF(C);
	StoreF(cc.CreateFDiv(LoadF(B), rc), A);
	StoreF(cc.CreateFDiv(LoadF(B + 1), rc), A + 1);
}

void JitCompiler::EmitDIVVF2_RK()
{
	IRValue* rc = ConstF(C);
	StoreF(cc.CreateFDiv(LoadF(B), rc), A);
	StoreF(cc.CreateFDiv(LoadF(B + 1), rc), A + 1);
}

void JitCompiler::EmitLENV2()
{
	IRValue* x = LoadF(B);
	IRValue* y = LoadF(B + 1);
	IRValue* dotproduct = cc.CreateFAdd(cc.CreateFMul(x, x), cc.CreateFMul(y, y));
	IRValue* len = cc.CreateCall(GetNativeFunc<double, double>("__g_sqrt", g_sqrt), { dotproduct });
	StoreF(len, A);
}

void JitCompiler::EmitEQV2_R()
{
	EmitComparisonOpcode([&](bool check) {
		return EmitVectorComparison(2, check);
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
	StoreF(cc.CreateFNeg(LoadF(B)), A);
	StoreF(cc.CreateFNeg(LoadF(B + 1)), A + 1);
	StoreF(cc.CreateFNeg(LoadF(B + 2)), A + 2);
}

void JitCompiler::EmitADDV3_RR()
{
	StoreF(cc.CreateFAdd(LoadF(B), LoadF(C)), A);
	StoreF(cc.CreateFAdd(LoadF(B + 1), LoadF(C + 1)), A + 1);
	StoreF(cc.CreateFAdd(LoadF(B + 2), LoadF(C + 2)), A + 2);
}

void JitCompiler::EmitSUBV3_RR()
{
	StoreF(cc.CreateFSub(LoadF(B), LoadF(C)), A);
	StoreF(cc.CreateFSub(LoadF(B + 1), LoadF(C + 1)), A + 1);
	StoreF(cc.CreateFSub(LoadF(B + 2), LoadF(C + 2)), A + 2);
}

void JitCompiler::EmitDOTV3_RR()
{
	StoreF(
		cc.CreateFAdd(
			cc.CreateFAdd(
				cc.CreateFMul(LoadF(B), LoadF(C)),
				cc.CreateFMul(LoadF(B + 1), LoadF(C + 1))),
			cc.CreateFMul(LoadF(B + 2), LoadF(C + 2))),
		A);
}

void JitCompiler::EmitCROSSV_RR()
{
	IRValue* a0 = LoadF(B);
	IRValue* a1 = LoadF(B + 1);
	IRValue* a2 = LoadF(B + 2);
	IRValue* b0 = LoadF(C);
	IRValue* b1 = LoadF(C + 1);
	IRValue* b2 = LoadF(C + 2);
	StoreF(cc.CreateFSub(cc.CreateFMul(a1, b2), cc.CreateFMul(a2, b1)), A);
	StoreF(cc.CreateFSub(cc.CreateFMul(a2, b0), cc.CreateFMul(a0, b2)), A + 1);
	StoreF(cc.CreateFSub(cc.CreateFMul(a0, b1), cc.CreateFMul(a1, b0)), A + 2);
}

void JitCompiler::EmitMULVF3_RR()
{
	IRValue* rc = LoadF(C);
	StoreF(cc.CreateFMul(LoadF(B), rc), A);
	StoreF(cc.CreateFMul(LoadF(B + 1), rc), A + 1);
	StoreF(cc.CreateFMul(LoadF(B + 2), rc), A + 2);
}

void JitCompiler::EmitMULVF3_RK()
{
	IRValue* rc = ConstF(C);
	StoreF(cc.CreateFMul(LoadF(B), rc), A);
	StoreF(cc.CreateFMul(LoadF(B + 1), rc), A + 1);
	StoreF(cc.CreateFMul(LoadF(B + 2), rc), A + 2);
}

void JitCompiler::EmitDIVVF3_RR()
{
	IRValue* rc = LoadF(C);
	StoreF(cc.CreateFDiv(LoadF(B), rc), A);
	StoreF(cc.CreateFDiv(LoadF(B + 1), rc), A + 1);
	StoreF(cc.CreateFDiv(LoadF(B + 2), rc), A + 2);
}

void JitCompiler::EmitDIVVF3_RK()
{
	IRValue* rc = ConstF(C);
	StoreF(cc.CreateFDiv(LoadF(B), rc), A);
	StoreF(cc.CreateFDiv(LoadF(B + 1), rc), A + 1);
	StoreF(cc.CreateFDiv(LoadF(B + 2), rc), A + 2);
}

void JitCompiler::EmitLENV3()
{
	IRValue* x = LoadF(B);
	IRValue* y = LoadF(B + 1);
	IRValue* z = LoadF(B + 2);
	IRValue* dotproduct = cc.CreateFAdd(cc.CreateFAdd(cc.CreateFMul(x, x), cc.CreateFMul(y, y)), cc.CreateFMul(z, z));
	IRValue* len = cc.CreateCall(GetNativeFunc<double, double>("__g_sqrt", g_sqrt), { dotproduct });
	StoreF(len, A);
}

void JitCompiler::EmitEQV3_R()
{
	EmitComparisonOpcode([&](bool check) {
		return EmitVectorComparison(3, check);
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

	IRValue* nullvalue = ConstValueA(nullptr);
	IRValue* ptr = LoadA(B);

	cc.CreateCondBr(cc.CreateICmpEQ(ptr, nullvalue), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreA(ptr, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreA(OffsetPtr(ptr, LoadD(C)), A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitADDA_RK()
{
	// Leave null pointers as null pointers

	IRBasicBlock* ifbb = irfunc->createBasicBlock({});
	IRBasicBlock* elsebb = irfunc->createBasicBlock({});
	IRBasicBlock* endbb = irfunc->createBasicBlock({});

	IRValue* nullvalue = ConstValueA(nullptr);
	IRValue* ptr = LoadA(B);

	cc.CreateCondBr(cc.CreateICmpEQ(ptr, nullvalue), ifbb, elsebb);
	cc.SetInsertPoint(ifbb);
	StoreA(ptr, A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(elsebb);
	StoreA(OffsetPtr(ptr, ConstD(C)), A);
	cc.CreateBr(endbb);
	cc.SetInsertPoint(endbb);
}

void JitCompiler::EmitSUBA()
{
	StoreA(OffsetPtr(LoadA(B), cc.CreateNeg(LoadD(C))), A);
}

void JitCompiler::EmitEQA_R()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpEQ(LoadA(B), LoadA(C));
		else       result = cc.CreateICmpNE(LoadA(B), LoadA(C));
		return result;
	});
}

void JitCompiler::EmitEQA_K()
{
	EmitComparisonOpcode([&](bool check) {
		IRValue* result;
		if (check) result = cc.CreateICmpEQ(LoadA(B), ConstA(C));
		else       result = cc.CreateICmpNE(LoadA(B), ConstA(C));
		return result;
	});
}

IRValue* JitCompiler::EmitVectorComparison(int N, bool check)
{
	IRValue* result = nullptr;
	bool approx = static_cast<bool>(A & CMP_APPROX);
	for (int i = 0; i < N; i++)
	{
		IRValue* elementresult;
		if (!approx)
		{
			if (check)
				elementresult = cc.CreateFCmpUEQ(LoadF(B + i), LoadF(C + i));
			else
				elementresult = cc.CreateFCmpUNE(LoadF(B + i), LoadF(C + i));
		}
		else
		{
			IRValue* diff = cc.CreateFSub(LoadF(C + i), LoadF(B + i));
			if (check)
				elementresult = cc.CreateAnd(
					cc.CreateFCmpUGT(diff, ConstValueF(-VM_EPSILON)),
					cc.CreateFCmpULT(diff, ConstValueF(VM_EPSILON)));
			else
				elementresult = cc.CreateOr(
					cc.CreateFCmpULE(diff, ConstValueF(-VM_EPSILON)),
					cc.CreateFCmpUGE(diff, ConstValueF(VM_EPSILON)));
		}

		if (i == 0)
			result = elementresult;
		else
			result = cc.CreateAnd(result, elementresult);
	}
	return result;
}
