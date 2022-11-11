
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
	auto rc = CheckRegS(C, A);
	auto call = CreateCall<void, FString*, FString*, FString*>(ConcatString);
	call->setArg(0, regS[A]);
	call->setArg(1, regS[B]);
	call->setArg(2, rc);
}

static int StringLength(FString* str)
{
	return static_cast<int>(str->Len());
}

void JitCompiler::EmitLENS()
{
	auto result = newResultInt32();
	auto call = CreateCall<int, FString*>(StringLength);
	call->setRet(0, result);
	call->setArg(0, regS[B]);
	cc.mov(regD[A], result);
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
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {

		auto call = CreateCall<int, FString*, FString*>(static_cast<bool>(A & CMP_APPROX) ? StringCompareNoCase : StringCompare);

		auto result = newResultInt32();
		call->setRet(0, result);

		if (static_cast<bool>(A & CMP_BK)) call->setArg(0, asmjit::imm_ptr(&konsts[B]));
		else                               call->setArg(0, regS[B]);

		if (static_cast<bool>(A & CMP_CK)) call->setArg(1, asmjit::imm_ptr(&konsts[C]));
		else                               call->setArg(1, regS[C]);

		int method = A & CMP_METHOD_MASK;
		if (method == CMP_EQ) {
			cc.test(result, result);
			if (check) cc.jz(fail);
			else       cc.jnz(fail);
		}
		else if (method == CMP_LT) {
			cc.cmp(result, 0);
			if (check) cc.jl(fail);
			else       cc.jnl(fail);
		}
		else {
			cc.cmp(result, 0);
			if (check) cc.jle(fail);
			else       cc.jnle(fail);
		}
	});
}

/////////////////////////////////////////////////////////////////////////////
// Integer math.

void JitCompiler::EmitSLL_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.shl(regD[A], rc);
}

void JitCompiler::EmitSLL_RI()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.shl(regD[A], C);
}

void JitCompiler::EmitSLL_KR()
{
	auto rc = CheckRegD(C, A);
	cc.mov(regD[A], konstd[B]);
	cc.shl(regD[A], rc);
}

void JitCompiler::EmitSRL_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.shr(regD[A], rc);
}

void JitCompiler::EmitSRL_RI()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.shr(regD[A], C);
}

void JitCompiler::EmitSRL_KR()
{
	auto rc = CheckRegD(C, A);
	cc.mov(regD[A], konstd[B]);
	cc.shr(regD[A], rc);
}

void JitCompiler::EmitSRA_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.sar(regD[A], rc);
}

void JitCompiler::EmitSRA_RI()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.sar(regD[A], C);
}

void JitCompiler::EmitSRA_KR()
{
	auto rc = CheckRegD(C, A);
	cc.mov(regD[A], konstd[B]);
	cc.sar(regD[A], rc);
}

void JitCompiler::EmitADD_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.add(regD[A], rc);
}

void JitCompiler::EmitADD_RK()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.add(regD[A], konstd[C]);
}

void JitCompiler::EmitADDI()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.add(regD[A], Cs);
}

void JitCompiler::EmitSUB_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.sub(regD[A], rc);
}

void JitCompiler::EmitSUB_RK()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.sub(regD[A], konstd[C]);
}

void JitCompiler::EmitSUB_KR()
{
	auto rc = CheckRegD(C, A);
	cc.mov(regD[A], konstd[B]);
	cc.sub(regD[A], rc);
}

void JitCompiler::EmitMUL_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.imul(regD[A], rc);
}

void JitCompiler::EmitMUL_RK()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.imul(regD[A], konstd[C]);
}

void JitCompiler::EmitDIV_RR()
{
	auto tmp0 = newTempInt32();
	auto tmp1 = newTempInt32();

	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	cc.test(regD[C], regD[C]);
	cc.je(label);

	cc.mov(tmp0, regD[B]);
	cc.cdq(tmp1, tmp0);
	cc.idiv(tmp1, tmp0, regD[C]);
	cc.mov(regD[A], tmp0);
}

void JitCompiler::EmitDIV_RK()
{
	if (konstd[C] != 0)
	{
		auto tmp0 = newTempInt32();
		auto tmp1 = newTempInt32();
		auto konstTmp = newTempIntPtr();
		cc.mov(tmp0, regD[B]);
		cc.cdq(tmp1, tmp0);
		cc.mov(konstTmp, asmjit::imm_ptr(&konstd[C]));
		cc.idiv(tmp1, tmp0, asmjit::x86::ptr(konstTmp));
		cc.mov(regD[A], tmp0);
	}
	else
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
}

void JitCompiler::EmitDIV_KR()
{
	auto tmp0 = newTempInt32();
	auto tmp1 = newTempInt32();

	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	cc.test(regD[C], regD[C]);
	cc.je(label);

	cc.mov(tmp0, konstd[B]);
	cc.cdq(tmp1, tmp0);
	cc.idiv(tmp1, tmp0, regD[C]);
	cc.mov(regD[A], tmp0);
}

void JitCompiler::EmitDIVU_RR()
{
	auto tmp0 = newTempInt32();
	auto tmp1 = newTempInt32();

	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	cc.test(regD[C], regD[C]);
	cc.je(label);

	cc.mov(tmp0, regD[B]);
	cc.mov(tmp1, 0);
	cc.div(tmp1, tmp0, regD[C]);
	cc.mov(regD[A], tmp0);
}

void JitCompiler::EmitDIVU_RK()
{
	if (konstd[C] != 0)
	{
		auto tmp0 = newTempInt32();
		auto tmp1 = newTempInt32();
		auto konstTmp = newTempIntPtr();
		cc.mov(tmp0, regD[B]);
		cc.mov(tmp1, 0);
		cc.mov(konstTmp, asmjit::imm_ptr(&konstd[C]));
		cc.div(tmp1, tmp0, asmjit::x86::ptr(konstTmp));
		cc.mov(regD[A], tmp0);
	}
	else
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
}

void JitCompiler::EmitDIVU_KR()
{
	auto tmp0 = newTempInt32();
	auto tmp1 = newTempInt32();

	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	cc.test(regD[C], regD[C]);
	cc.je(label);

	cc.mov(tmp0, konstd[B]);
	cc.mov(tmp1, 0);
	cc.div(tmp1, tmp0, regD[C]);
	cc.mov(regD[A], tmp0);
}

void JitCompiler::EmitMOD_RR()
{
	auto tmp0 = newTempInt32();
	auto tmp1 = newTempInt32();

	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	cc.test(regD[C], regD[C]);
	cc.je(label);

	cc.mov(tmp0, regD[B]);
	cc.cdq(tmp1, tmp0);
	cc.idiv(tmp1, tmp0, regD[C]);
	cc.mov(regD[A], tmp1);
}

void JitCompiler::EmitMOD_RK()
{
	if (konstd[C] != 0)
	{
		auto tmp0 = newTempInt32();
		auto tmp1 = newTempInt32();
		auto konstTmp = newTempIntPtr();
		cc.mov(tmp0, regD[B]);
		cc.cdq(tmp1, tmp0);
		cc.mov(konstTmp, asmjit::imm_ptr(&konstd[C]));
		cc.idiv(tmp1, tmp0, asmjit::x86::ptr(konstTmp));
		cc.mov(regD[A], tmp1);
	}
	else
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
}

void JitCompiler::EmitMOD_KR()
{
	auto tmp0 = newTempInt32();
	auto tmp1 = newTempInt32();

	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	cc.test(regD[C], regD[C]);
	cc.je(label);

	cc.mov(tmp0, konstd[B]);
	cc.cdq(tmp1, tmp0);
	cc.idiv(tmp1, tmp0, regD[C]);
	cc.mov(regD[A], tmp1);
}

void JitCompiler::EmitMODU_RR()
{
	auto tmp0 = newTempInt32();
	auto tmp1 = newTempInt32();

	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	cc.test(regD[C], regD[C]);
	cc.je(label);

	cc.mov(tmp0, regD[B]);
	cc.mov(tmp1, 0);
	cc.div(tmp1, tmp0, regD[C]);
	cc.mov(regD[A], tmp1);
}

void JitCompiler::EmitMODU_RK()
{
	if (konstd[C] != 0)
	{
		auto tmp0 = newTempInt32();
		auto tmp1 = newTempInt32();
		auto konstTmp = newTempIntPtr();
		cc.mov(tmp0, regD[B]);
		cc.mov(tmp1, 0);
		cc.mov(konstTmp, asmjit::imm_ptr(&konstd[C]));
		cc.div(tmp1, tmp0, asmjit::x86::ptr(konstTmp));
		cc.mov(regD[A], tmp1);
	}
	else
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
}

void JitCompiler::EmitMODU_KR()
{
	auto tmp0 = newTempInt32();
	auto tmp1 = newTempInt32();

	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	cc.test(regD[C], regD[C]);
	cc.je(label);

	cc.mov(tmp0, konstd[B]);
	cc.mov(tmp1, 0);
	cc.div(tmp1, tmp0, regD[C]);
	cc.mov(regD[A], tmp1);
}

void JitCompiler::EmitAND_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.and_(regD[A], rc);
}

void JitCompiler::EmitAND_RK()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.and_(regD[A], konstd[C]);
}

void JitCompiler::EmitOR_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.or_(regD[A], rc);
}

void JitCompiler::EmitOR_RK()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.or_(regD[A], konstd[C]);
}

void JitCompiler::EmitXOR_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.xor_(regD[A], rc);
}

void JitCompiler::EmitXOR_RK()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.xor_(regD[A], konstd[C]);
}

void JitCompiler::EmitMIN_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.cmp(rc, regD[A]);
	cc.cmovl(regD[A], rc);
}

void JitCompiler::EmitMIN_RK()
{
	auto rc = newTempInt32();
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.mov(rc, asmjit::imm(konstd[C]));
	cc.cmp(rc, regD[A]);
	cc.cmovl(regD[A], rc);
}

void JitCompiler::EmitMAX_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.cmp(rc, regD[A]);
	cc.cmovg(regD[A], rc);
}

void JitCompiler::EmitMAX_RK()
{
	auto rc = newTempInt32();
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.mov(rc, asmjit::imm(konstd[C]));
	cc.cmp(rc, regD[A]);
	cc.cmovg(regD[A], rc);
}

void JitCompiler::EmitMINU_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.cmp(rc, regD[A]);
	cc.cmovb(regD[A], rc);
}

void JitCompiler::EmitMINU_RK()
{
	auto rc = newTempInt32();
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.mov(rc, asmjit::imm(konstd[C]));
	cc.cmp(rc, regD[A]);
	cc.cmovb(regD[A], rc);
}

void JitCompiler::EmitMAXU_RR()
{
	auto rc = CheckRegD(C, A);
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.cmp(rc, regD[A]);
	cc.cmova(regD[A], rc);
}

void JitCompiler::EmitMAXU_RK()
{
	auto rc = newTempInt32();
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.mov(rc, asmjit::imm(konstd[C]));
	cc.cmp(rc, regD[A]);
	cc.cmova(regD[A], rc);
}

void JitCompiler::EmitABS()
{
	auto srcB = CheckRegD(B, A);
	auto tmp = newTempInt32();
	cc.mov(tmp, regD[B]);
	cc.sar(tmp, 31);
	cc.mov(regD[A], tmp);
	cc.xor_(regD[A], srcB);
	cc.sub(regD[A], tmp);
}

void JitCompiler::EmitNEG()
{
	auto srcB = CheckRegD(B, A);
	cc.xor_(regD[A], regD[A]);
	cc.sub(regD[A], srcB);
}

void JitCompiler::EmitNOT()
{
	if (A != B)
		cc.mov(regD[A], regD[B]);
	cc.not_(regD[A]);
}

void JitCompiler::EmitEQ_R()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], regD[C]);
		if (check) cc.je(fail);
		else       cc.jne(fail);
	});
}

void JitCompiler::EmitEQ_K()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], konstd[C]);
		if (check) cc.je(fail);
		else       cc.jne(fail);
	});
}

void JitCompiler::EmitLT_RR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], regD[C]);
		if (check) cc.jl(fail);
		else       cc.jnl(fail);
	});
}

void JitCompiler::EmitLT_RK()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], konstd[C]);
		if (check) cc.jl(fail);
		else       cc.jnl(fail);
	});
}

void JitCompiler::EmitLT_KR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		auto tmp = newTempIntPtr();
		cc.mov(tmp, asmjit::imm_ptr(&konstd[B]));
		cc.cmp(asmjit::x86::ptr(tmp), regD[C]);
		if (check) cc.jl(fail);
		else       cc.jnl(fail);
	});
}

void JitCompiler::EmitLE_RR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], regD[C]);
		if (check) cc.jle(fail);
		else       cc.jnle(fail);
	});
}

void JitCompiler::EmitLE_RK()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], konstd[C]);
		if (check) cc.jle(fail);
		else       cc.jnle(fail);
	});
}

void JitCompiler::EmitLE_KR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		auto tmp = newTempIntPtr();
		cc.mov(tmp, asmjit::imm_ptr(&konstd[B]));
		cc.cmp(asmjit::x86::ptr(tmp), regD[C]);
		if (check) cc.jle(fail);
		else       cc.jnle(fail);
	});
}

void JitCompiler::EmitLTU_RR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], regD[C]);
		if (check) cc.jb(fail);
		else       cc.jnb(fail);
	});
}

void JitCompiler::EmitLTU_RK()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], konstd[C]);
		if (check) cc.jb(fail);
		else       cc.jnb(fail);
	});
}

void JitCompiler::EmitLTU_KR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		auto tmp = newTempIntPtr();
		cc.mov(tmp, asmjit::imm_ptr(&konstd[B]));
		cc.cmp(asmjit::x86::ptr(tmp), regD[C]);
		if (check) cc.jb(fail);
		else       cc.jnb(fail);
	});
}

void JitCompiler::EmitLEU_RR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], regD[C]);
		if (check) cc.jbe(fail);
		else       cc.jnbe(fail);
	});
}

void JitCompiler::EmitLEU_RK()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regD[B], konstd[C]);
		if (check) cc.jbe(fail);
		else       cc.jnbe(fail);
	});
}

void JitCompiler::EmitLEU_KR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		auto tmp = newTempIntPtr();
		cc.mov(tmp, asmjit::imm_ptr(&konstd[B]));
		cc.cmp(asmjit::x86::ptr(tmp), regD[C]);
		if (check) cc.jbe(fail);
		else       cc.jnbe(fail);
	});
}

/////////////////////////////////////////////////////////////////////////////
// Double-precision floating point math.

void JitCompiler::EmitADDF_RR()
{
	auto rc = CheckRegF(C, A);
	if (A != B)
		cc.movsd(regF[A], regF[B]);
	cc.addsd(regF[A], rc);
}

void JitCompiler::EmitADDF_RK()
{
	auto tmp = newTempIntPtr();
	if (A != B)
		cc.movsd(regF[A], regF[B]);
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.addsd(regF[A], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitSUBF_RR()
{
	auto rc = CheckRegF(C, A);
	if (A != B)
		cc.movsd(regF[A], regF[B]);
	cc.subsd(regF[A], rc);
}

void JitCompiler::EmitSUBF_RK()
{
	auto tmp = newTempIntPtr();
	if (A != B)
		cc.movsd(regF[A], regF[B]);
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.subsd(regF[A], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitSUBF_KR()
{
	auto rc = CheckRegF(C, A);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, asmjit::imm_ptr(&konstf[B]));
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.subsd(regF[A], rc);
}

void JitCompiler::EmitMULF_RR()
{
	auto rc = CheckRegF(C, A);
	if (A != B)
		cc.movsd(regF[A], regF[B]);
	cc.mulsd(regF[A], rc);
}

void JitCompiler::EmitMULF_RK()
{
	auto tmp = newTempIntPtr();
	if (A != B)
		cc.movsd(regF[A], regF[B]);
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitDIVF_RR()
{
	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto zero = newTempXmmSd();
	cc.xorpd(zero, zero);
	cc.ucomisd(regF[C], zero);
	cc.je(label);

	auto rc = CheckRegF(C, A);
	cc.movsd(regF[A], regF[B]);
	cc.divsd(regF[A], rc);
}

void JitCompiler::EmitDIVF_RK()
{
	if (konstf[C] == 0.)
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
	else
	{
		auto tmp = newTempIntPtr();
		cc.movsd(regF[A], regF[B]);
		cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
		cc.divsd(regF[A], asmjit::x86::qword_ptr(tmp));
	}
}

void JitCompiler::EmitDIVF_KR()
{
	auto rc = CheckRegF(C, A);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, asmjit::imm_ptr(&konstf[B]));
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.divsd(regF[A], rc);
}

static double DoubleModF(double a, double b)
{
	return a - floor(a / b) * b;
}

void JitCompiler::EmitMODF_RR()
{
	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto zero = newTempXmmSd();
	cc.xorpd(zero, zero);
	cc.ucomisd(regF[C], zero);
	cc.je(label);

	auto result = newResultXmmSd();
	auto call = CreateCall<double, double, double>(DoubleModF);
	call->setRet(0, result);
	call->setArg(0, regF[B]);
	call->setArg(1, regF[C]);
	cc.movsd(regF[A], result);
}

void JitCompiler::EmitMODF_RK()
{
	if (konstf[C] == 0.)
	{
		EmitThrowException(X_DIVISION_BY_ZERO);
	}
	else
	{
		auto tmpPtr = newTempIntPtr();
		cc.mov(tmpPtr, asmjit::imm_ptr(&konstf[C]));

		auto tmp = newTempXmmSd();
		cc.movsd(tmp, asmjit::x86::qword_ptr(tmpPtr));

		auto result = newResultXmmSd();
		auto call = CreateCall<double, double, double>(DoubleModF);
		call->setRet(0, result);
		call->setArg(0, regF[B]);
		call->setArg(1, tmp);
		cc.movsd(regF[A], result);
	}
}

void JitCompiler::EmitMODF_KR()
{
	using namespace asmjit;

	auto label = EmitThrowExceptionLabel(X_DIVISION_BY_ZERO);
	auto zero = newTempXmmSd();
	cc.xorpd(zero, zero);
	cc.ucomisd(regF[C], zero);
	cc.je(label);

	auto tmp = newTempXmmSd();
	cc.movsd(tmp, x86::ptr(ToMemAddress(&konstf[B])));

	auto result = newResultXmmSd();
	auto call = CreateCall<double, double, double>(DoubleModF);
	call->setRet(0, result);
	call->setArg(0, tmp);
	call->setArg(1, regF[C]);
	cc.movsd(regF[A], result);
}

void JitCompiler::EmitPOWF_RR()
{
	auto result = newResultXmmSd();
	auto call = CreateCall<double, double, double>(g_pow);
	call->setRet(0, result);
	call->setArg(0, regF[B]);
	call->setArg(1, regF[C]);
	cc.movsd(regF[A], result);
}

void JitCompiler::EmitPOWF_RK()
{
	auto tmp = newTempIntPtr();
	auto tmp2 = newTempXmmSd();
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.movsd(tmp2, asmjit::x86::qword_ptr(tmp));

	auto result = newResultXmmSd();
	auto call = CreateCall<double, double, double>(g_pow);
	call->setRet(0, result);
	call->setArg(0, regF[B]);
	call->setArg(1, tmp2);
	cc.movsd(regF[A], result);
}

void JitCompiler::EmitPOWF_KR()
{
	auto tmp = newTempIntPtr();
	auto tmp2 = newTempXmmSd();
	cc.mov(tmp, asmjit::imm_ptr(&konstf[B]));
	cc.movsd(tmp2, asmjit::x86::qword_ptr(tmp));

	auto result = newResultXmmSd();
	auto call = CreateCall<double, double, double>(g_pow);
	call->setRet(0, result);
	call->setArg(0, tmp2);
	call->setArg(1, regF[C]);
	cc.movsd(regF[A], result);
}

void JitCompiler::EmitMINF_RR()
{
	auto rc = CheckRegF(C, A);
	if (A != B)
		cc.movsd(regF[A], regF[B]);
	cc.minpd(regF[A], rc);  // minsd requires SSE 4.1
}

void JitCompiler::EmitMINF_RK()
{
	auto rb = CheckRegF(B, A);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.minpd(regF[A], rb); // minsd requires SSE 4.1
}

void JitCompiler::EmitMAXF_RR()
{
	auto rc = CheckRegF(C, A);
	if (A != B)
		cc.movsd(regF[A], regF[B]);
	cc.maxpd(regF[A], rc); // maxsd requires SSE 4.1
}

void JitCompiler::EmitMAXF_RK()
{
	auto rb = CheckRegF(B, A);
	auto tmp = newTempIntPtr();
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.maxpd(regF[A], rb); // maxsd requires SSE 4.1
}

void JitCompiler::EmitATAN2()
{
	auto result = newResultXmmSd();
	auto call = CreateCall<double, double, double>(g_atan2);
	call->setRet(0, result);
	call->setArg(0, regF[B]);
	call->setArg(1, regF[C]);
	cc.movsd(regF[A], result);

	static const double constant = 180 / M_PI;
	auto tmp = newTempIntPtr();
	cc.mov(tmp, asmjit::imm_ptr(&constant));
	cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitFLOP()
{
	if (C == FLOP_NEG)
	{
		auto mask = cc.newDoubleConst(asmjit::kConstScopeLocal, -0.0);
		auto maskXmm = newTempXmmSd();
		cc.movsd(maskXmm, mask);
		if (A != B)
			cc.movsd(regF[A], regF[B]);
		cc.xorpd(regF[A], maskXmm);
	}
	else
	{
		auto v = newTempXmmSd();
		cc.movsd(v, regF[B]);

		if (C == FLOP_TAN_DEG)
		{
			static const double constant = M_PI / 180;
			auto tmp = newTempIntPtr();
			cc.mov(tmp, asmjit::imm_ptr(&constant));
			cc.mulsd(v, asmjit::x86::qword_ptr(tmp));
		}

		typedef double(*FuncPtr)(double);
		FuncPtr func = nullptr;
		switch (C)
		{
		default: I_Error("Unknown OP_FLOP subfunction"); break;
		case FLOP_ABS:		func = fabs; break;
		case FLOP_EXP:		func = g_exp; break;
		case FLOP_LOG:		func = g_log; break;
		case FLOP_LOG10:	func = g_log10; break;
		case FLOP_SQRT:		func = g_sqrt; break;
		case FLOP_CEIL:		func = ceil; break;
		case FLOP_FLOOR:	func = floor; break;
		case FLOP_ACOS:		func = g_acos; break;
		case FLOP_ASIN:		func = g_asin; break;
		case FLOP_ATAN:		func = g_atan; break;
		case FLOP_COS:		func = g_cos; break;
		case FLOP_SIN:		func = g_sin; break;
		case FLOP_TAN:		func = g_tan; break;
		case FLOP_ACOS_DEG:	func = g_acos; break;
		case FLOP_ASIN_DEG:	func = g_asin; break;
		case FLOP_ATAN_DEG:	func = g_atan; break;
		case FLOP_COS_DEG:	func = g_cosdeg; break;
		case FLOP_SIN_DEG:	func = g_sindeg; break;
		case FLOP_TAN_DEG:	func = g_tan; break;
		case FLOP_COSH:		func = g_cosh; break;
		case FLOP_SINH:		func = g_sinh; break;
		case FLOP_TANH:		func = g_tanh; break;
		case FLOP_ROUND:	func = round; break;
		}

		auto result = newResultXmmSd();
		auto call = CreateCall<double, double>(func);
		call->setRet(0, result);
		call->setArg(0, v);
		cc.movsd(regF[A], result);

		if (C == FLOP_ACOS_DEG || C == FLOP_ASIN_DEG || C == FLOP_ATAN_DEG)
		{
			static const double constant = 180 / M_PI;
			auto tmp = newTempIntPtr();
			cc.mov(tmp, asmjit::imm_ptr(&constant));
			cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
		}
	}
}

void JitCompiler::EmitEQF_R()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		bool approx = static_cast<bool>(A & CMP_APPROX);
		if (!approx)
		{
			cc.ucomisd(regF[B], regF[C]);
			if (check) {
				cc.jp(success);
				cc.je(fail);
			}
			else {
				cc.jp(fail);
				cc.jne(fail);
			}
		}
		else
		{
			auto tmp = newTempXmmSd();

			const int64_t absMaskInt = 0x7FFFFFFFFFFFFFFF;
			auto absMask = cc.newDoubleConst(asmjit::kConstScopeLocal, reinterpret_cast<const double&>(absMaskInt));
			auto absMaskXmm = newTempXmmPd();

			auto epsilon = cc.newDoubleConst(asmjit::kConstScopeLocal, VM_EPSILON);
			auto epsilonXmm = newTempXmmSd();

			cc.movsd(tmp, regF[B]);
			cc.subsd(tmp, regF[C]);
			cc.movsd(absMaskXmm, absMask);
			cc.andpd(tmp, absMaskXmm);
			cc.movsd(epsilonXmm, epsilon);
			cc.ucomisd(epsilonXmm, tmp);

			if (check) cc.ja(fail);
			else       cc.jna(fail);
		}
	});
}

void JitCompiler::EmitEQF_K()
{
	using namespace asmjit;
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		bool approx = static_cast<bool>(A & CMP_APPROX);
		if (!approx) {
			auto konstTmp = newTempIntPtr();
			cc.mov(konstTmp, asmjit::imm_ptr(&konstf[C]));
			cc.ucomisd(regF[B], x86::qword_ptr(konstTmp));
			if (check) {
				cc.jp(success);
				cc.je(fail);
			}
			else {
				cc.jp(fail);
				cc.jne(fail);
			}
		}
		else {
			auto konstTmp = newTempIntPtr();
			auto subTmp = newTempXmmSd();

			const int64_t absMaskInt = 0x7FFFFFFFFFFFFFFF;
			auto absMask = cc.newDoubleConst(kConstScopeLocal, reinterpret_cast<const double&>(absMaskInt));
			auto absMaskXmm = newTempXmmPd();

			auto epsilon = cc.newDoubleConst(kConstScopeLocal, VM_EPSILON);
			auto epsilonXmm = newTempXmmSd();

			cc.mov(konstTmp, asmjit::imm_ptr(&konstf[C]));

			cc.movsd(subTmp, regF[B]);
			cc.subsd(subTmp, x86::qword_ptr(konstTmp));
			cc.movsd(absMaskXmm, absMask);
			cc.andpd(subTmp, absMaskXmm);
			cc.movsd(epsilonXmm, epsilon);
			cc.ucomisd(epsilonXmm, subTmp);

			if (check) cc.ja(fail);
			else       cc.jna(fail);
		}
	});
}

void JitCompiler::EmitLTF_RR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LTF_RR.\n");

		cc.ucomisd(regF[C], regF[B]);
		if (check) cc.ja(fail);
		else       cc.jna(fail);
	});
}

void JitCompiler::EmitLTF_RK()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LTF_RK.\n");

		auto constTmp = newTempIntPtr();
		auto xmmTmp = newTempXmmSd();
		cc.mov(constTmp, asmjit::imm_ptr(&konstf[C]));
		cc.movsd(xmmTmp, asmjit::x86::qword_ptr(constTmp));

		cc.ucomisd(xmmTmp, regF[B]);
		if (check) cc.ja(fail);
		else       cc.jna(fail);
	});
}

void JitCompiler::EmitLTF_KR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LTF_KR.\n");

		auto tmp = newTempIntPtr();
		cc.mov(tmp, asmjit::imm_ptr(&konstf[B]));

		cc.ucomisd(regF[C], asmjit::x86::qword_ptr(tmp));
		if (check) cc.ja(fail);
		else       cc.jna(fail);
	});
}

void JitCompiler::EmitLEF_RR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LEF_RR.\n");

		cc.ucomisd(regF[C], regF[B]);
		if (check) cc.jae(fail);
		else       cc.jnae(fail);
	});
}

void JitCompiler::EmitLEF_RK()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LEF_RK.\n");

		auto constTmp = newTempIntPtr();
		auto xmmTmp = newTempXmmSd();
		cc.mov(constTmp, asmjit::imm_ptr(&konstf[C]));
		cc.movsd(xmmTmp, asmjit::x86::qword_ptr(constTmp));

		cc.ucomisd(xmmTmp, regF[B]);
		if (check) cc.jae(fail);
		else       cc.jnae(fail);
	});
}

void JitCompiler::EmitLEF_KR()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		if (static_cast<bool>(A & CMP_APPROX)) I_Error("CMP_APPROX not implemented for LEF_KR.\n");

		auto tmp = newTempIntPtr();
		cc.mov(tmp, asmjit::imm_ptr(&konstf[B]));

		cc.ucomisd(regF[C], asmjit::x86::qword_ptr(tmp));
		if (check) cc.jae(fail);
		else       cc.jnae(fail);
	});
}

/////////////////////////////////////////////////////////////////////////////
// Vector math. (2D)

void JitCompiler::EmitNEGV2()
{
	auto mask = cc.newDoubleConst(asmjit::kConstScopeLocal, -0.0);
	auto maskXmm = newTempXmmSd();
	cc.movsd(maskXmm, mask);
	cc.movsd(regF[A], regF[B]);
	cc.xorpd(regF[A], maskXmm);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.xorpd(regF[A + 1], maskXmm);
}

void JitCompiler::EmitADDV2_RR()
{
	auto rc0 = CheckRegF(C, A);
	auto rc1 = CheckRegF(C + 1, A + 1);
	cc.movsd(regF[A], regF[B]);
	cc.addsd(regF[A], rc0);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.addsd(regF[A + 1], rc1);
}

void JitCompiler::EmitSUBV2_RR()
{
	auto rc0 = CheckRegF(C, A);
	auto rc1 = CheckRegF(C + 1, A + 1);
	cc.movsd(regF[A], regF[B]);
	cc.subsd(regF[A], rc0);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.subsd(regF[A + 1], rc1);
}

void JitCompiler::EmitDOTV2_RR()
{
	auto rc0 = CheckRegF(C, A);
	auto rc1 = CheckRegF(C + 1, A);
	auto tmp = newTempXmmSd();
	cc.movsd(regF[A], regF[B]);
	cc.mulsd(regF[A], rc0);
	cc.movsd(tmp, regF[B + 1]);
	cc.mulsd(tmp, rc1);
	cc.addsd(regF[A], tmp);
}

void JitCompiler::EmitMULVF2_RR()
{
	auto rc = CheckRegF(C, A, A + 1);
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.mulsd(regF[A], rc);
	cc.mulsd(regF[A + 1], rc);
}

void JitCompiler::EmitMULVF2_RK()
{
	auto tmp = newTempIntPtr();
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.mulsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitDIVVF2_RR()
{
	auto rc = CheckRegF(C, A, A + 1);
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.divsd(regF[A], rc);
	cc.divsd(regF[A + 1], rc);
}

void JitCompiler::EmitDIVVF2_RK()
{
	auto tmp = newTempIntPtr();
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.divsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.divsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitLENV2()
{
	auto rb0 = CheckRegF(B, A);
	auto rb1 = CheckRegF(B + 1, A);
	auto tmp = newTempXmmSd();
	cc.movsd(regF[A], regF[B]);
	cc.mulsd(regF[A], rb0);
	cc.movsd(tmp, rb1);
	cc.mulsd(tmp, rb1);
	cc.addsd(regF[A], tmp);
	CallSqrt(regF[A], regF[A]);
}

void JitCompiler::EmitEQV2_R()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		EmitVectorComparison<2> (check, fail, success);
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
	auto mask = cc.newDoubleConst(asmjit::kConstScopeLocal, -0.0);
	auto maskXmm = newTempXmmSd();
	cc.movsd(maskXmm, mask);
	cc.movsd(regF[A], regF[B]);
	cc.xorpd(regF[A], maskXmm);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.xorpd(regF[A + 1], maskXmm);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.xorpd(regF[A + 2], maskXmm);
}

void JitCompiler::EmitADDV3_RR()
{
	auto rc0 = CheckRegF(C, A);
	auto rc1 = CheckRegF(C + 1, A + 1);
	auto rc2 = CheckRegF(C + 2, A + 2);
	cc.movsd(regF[A], regF[B]);
	cc.addsd(regF[A], rc0);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.addsd(regF[A + 1], rc1);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.addsd(regF[A + 2], rc2);
}

void JitCompiler::EmitSUBV3_RR()
{
	auto rc0 = CheckRegF(C, A);
	auto rc1 = CheckRegF(C + 1, A + 1);
	auto rc2 = CheckRegF(C + 2, A + 2);
	cc.movsd(regF[A], regF[B]);
	cc.subsd(regF[A], rc0);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.subsd(regF[A + 1], rc1);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.subsd(regF[A + 2], rc2);
}

void JitCompiler::EmitDOTV3_RR()
{
	auto rb1 = CheckRegF(B + 1, A);
	auto rb2 = CheckRegF(B + 2, A);
	auto rc0 = CheckRegF(C, A);
	auto rc1 = CheckRegF(C + 1, A);
	auto rc2 = CheckRegF(C + 2, A);
	auto tmp = newTempXmmSd();
	cc.movsd(regF[A], regF[B]);
	cc.mulsd(regF[A], rc0);
	cc.movsd(tmp, rb1);
	cc.mulsd(tmp, rc1);
	cc.addsd(regF[A], tmp);
	cc.movsd(tmp, rb2);
	cc.mulsd(tmp, rc2);
	cc.addsd(regF[A], tmp);
}

void JitCompiler::EmitCROSSV_RR()
{
	auto tmp = newTempXmmSd();

	auto a0 = CheckRegF(B, A);
	auto a1 = CheckRegF(B + 1, A + 1);
	auto a2 = CheckRegF(B + 2, A + 2);
	auto b0 = CheckRegF(C, A);
	auto b1 = CheckRegF(C + 1, A + 1);
	auto b2 = CheckRegF(C + 2, A + 2);

	// r0 = a1b2 - a2b1
	cc.movsd(regF[A], a1);
	cc.mulsd(regF[A], b2);
	cc.movsd(tmp, a2);
	cc.mulsd(tmp, b1);
	cc.subsd(regF[A], tmp);

	// r1 = a2b0 - a0b2
	cc.movsd(regF[A + 1], a2);
	cc.mulsd(regF[A + 1], b0);
	cc.movsd(tmp, a0);
	cc.mulsd(tmp, b2);
	cc.subsd(regF[A + 1], tmp);

	// r2 = a0b1 - a1b0
	cc.movsd(regF[A + 2], a0);
	cc.mulsd(regF[A + 2], b1);
	cc.movsd(tmp, a1);
	cc.mulsd(tmp, b0);
	cc.subsd(regF[A + 2], tmp);
}

void JitCompiler::EmitMULVF3_RR()
{
	auto rc = CheckRegF(C, A, A + 1, A + 2);
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.mulsd(regF[A], rc);
	cc.mulsd(regF[A + 1], rc);
	cc.mulsd(regF[A + 2], rc);
}

void JitCompiler::EmitMULVF3_RK()
{
	auto tmp = newTempIntPtr();
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.mulsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
	cc.mulsd(regF[A + 2], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitDIVVF3_RR()
{
	auto rc = CheckRegF(C, A, A + 1, A + 2);
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.divsd(regF[A], rc);
	cc.divsd(regF[A + 1], rc);
	cc.divsd(regF[A + 2], rc);
}

void JitCompiler::EmitDIVVF3_RK()
{
	auto tmp = newTempIntPtr();
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.divsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.divsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
	cc.divsd(regF[A + 2], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitLENV3()
{
	auto rb1 = CheckRegF(B + 1, A);
	auto rb2 = CheckRegF(B + 2, A);
	auto tmp = newTempXmmSd();
	cc.movsd(regF[A], regF[B]);
	cc.mulsd(regF[A], regF[B]);
	cc.movsd(tmp, rb1);
	cc.mulsd(tmp, rb1);
	cc.addsd(regF[A], tmp);
	cc.movsd(tmp, rb2);
	cc.mulsd(tmp, rb2);
	cc.addsd(regF[A], tmp);
	CallSqrt(regF[A], regF[A]);
}

void JitCompiler::EmitEQV3_R()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		EmitVectorComparison<3> (check, fail, success);
	});
}

void JitCompiler::EmitEQV3_K()
{
	I_Error("EQV3_K is not used.");
}

/////////////////////////////////////////////////////////////////////////////
// Vector math. (4D/Quaternion)

void JitCompiler::EmitNEGV4()
{
	auto mask = cc.newDoubleConst(asmjit::kConstScopeLocal, -0.0);
	auto maskXmm = newTempXmmSd();
	cc.movsd(maskXmm, mask);
	cc.movsd(regF[A], regF[B]);
	cc.xorpd(regF[A], maskXmm);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.xorpd(regF[A + 1], maskXmm);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.xorpd(regF[A + 2], maskXmm);
	cc.movsd(regF[A + 3], regF[B + 3]);
	cc.xorpd(regF[A + 3], maskXmm);
}

void JitCompiler::EmitADDV4_RR()
{
	auto rc0 = CheckRegF(C, A);
	auto rc1 = CheckRegF(C + 1, A + 1);
	auto rc2 = CheckRegF(C + 2, A + 2);
	auto rc3 = CheckRegF(C + 3, A + 3);
	cc.movsd(regF[A], regF[B]);
	cc.addsd(regF[A], rc0);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.addsd(regF[A + 1], rc1);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.addsd(regF[A + 2], rc2);
	cc.movsd(regF[A + 3], regF[B + 3]);
	cc.addsd(regF[A + 3], rc3);
}

void JitCompiler::EmitSUBV4_RR()
{
	auto rc0 = CheckRegF(C, A);
	auto rc1 = CheckRegF(C + 1, A + 1);
	auto rc2 = CheckRegF(C + 2, A + 2);
	auto rc3 = CheckRegF(C + 3, A + 3);
	cc.movsd(regF[A], regF[B]);
	cc.subsd(regF[A], rc0);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.subsd(regF[A + 1], rc1);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.subsd(regF[A + 2], rc2);
	cc.movsd(regF[A + 3], regF[B + 3]);
	cc.subsd(regF[A + 3], rc3);
}

void JitCompiler::EmitDOTV4_RR()
{
	auto rb1 = CheckRegF(B + 1, A);
	auto rb2 = CheckRegF(B + 2, A);
	auto rb3 = CheckRegF(B + 3, A);
	auto rc0 = CheckRegF(C, A);
	auto rc1 = CheckRegF(C + 1, A);
	auto rc2 = CheckRegF(C + 2, A);
	auto rc3 = CheckRegF(C + 3, A);
	auto tmp = newTempXmmSd();
	cc.movsd(regF[A], regF[B]);
	cc.mulsd(regF[A], rc0);
	cc.movsd(tmp, rb1);
	cc.mulsd(tmp, rc1);
	cc.addsd(regF[A], tmp);
	cc.movsd(tmp, rb2);
	cc.mulsd(tmp, rc2);
	cc.addsd(regF[A], tmp);
	cc.movsd(tmp, rb3);
	cc.mulsd(tmp, rc3);
	cc.addsd(regF[A], tmp);
}

void JitCompiler::EmitMULVF4_RR()
{
	auto rc = CheckRegF(C, A, A + 1, A + 2, A + 3);
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.movsd(regF[A + 3], regF[B + 3]);
	cc.mulsd(regF[A], rc);
	cc.mulsd(regF[A + 1], rc);
	cc.mulsd(regF[A + 2], rc);
	cc.mulsd(regF[A + 3], rc);
}

void JitCompiler::EmitMULVF4_RK()
{
	auto tmp = newTempIntPtr();
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.movsd(regF[A + 3], regF[B + 3]);
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.mulsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
	cc.mulsd(regF[A + 2], asmjit::x86::qword_ptr(tmp));
	cc.mulsd(regF[A + 3], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitDIVVF4_RR()
{
	auto rc = CheckRegF(C, A, A + 1, A + 2, A + 3);
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.movsd(regF[A + 3], regF[B + 3]);
	cc.divsd(regF[A], rc);
	cc.divsd(regF[A + 1], rc);
	cc.divsd(regF[A + 2], rc);
	cc.divsd(regF[A + 3], rc);
}

void JitCompiler::EmitDIVVF4_RK()
{
	auto tmp = newTempIntPtr();
	cc.movsd(regF[A], regF[B]);
	cc.movsd(regF[A + 1], regF[B + 1]);
	cc.movsd(regF[A + 2], regF[B + 2]);
	cc.movsd(regF[A + 3], regF[B + 3]);
	cc.mov(tmp, asmjit::imm_ptr(&konstf[C]));
	cc.divsd(regF[A], asmjit::x86::qword_ptr(tmp));
	cc.divsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
	cc.divsd(regF[A + 2], asmjit::x86::qword_ptr(tmp));
	cc.divsd(regF[A + 3], asmjit::x86::qword_ptr(tmp));
}

void JitCompiler::EmitLENV4()
{
	auto rb1 = CheckRegF(B + 1, A);
	auto rb2 = CheckRegF(B + 2, A);
	auto rb3 = CheckRegF(B + 3, A);
	auto tmp = newTempXmmSd();
	cc.movsd(regF[A], regF[B]);
	cc.mulsd(regF[A], regF[B]);
	cc.movsd(tmp, rb1);
	cc.mulsd(tmp, rb1);
	cc.addsd(regF[A], tmp);
	cc.movsd(tmp, rb2);
	cc.mulsd(tmp, rb2);
	cc.addsd(regF[A], tmp);
	cc.movsd(tmp, rb3);
	cc.mulsd(tmp, rb3);
	cc.addsd(regF[A], tmp);
	CallSqrt(regF[A], regF[A]);
}

void JitCompiler::EmitEQV4_R()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		EmitVectorComparison<4> (check, fail, success);
	});
}

void JitCompiler::EmitEQV4_K()
{
	I_Error("EQV4_K is not used.");
}

/////////////////////////////////////////////////////////////////////////////
// Pointer math.

void JitCompiler::EmitADDA_RR()
{
	auto tmp = newTempIntPtr();
	auto label = cc.newLabel();

	cc.mov(tmp, regA[B]);

	// Check if zero, the first operand is zero, if it is, don't add.
	cc.cmp(tmp, 0);
	cc.je(label);

	auto tmpptr = newTempIntPtr();
	cc.mov(tmpptr, regD[C]);
	cc.add(tmp, tmpptr);

	cc.bind(label);
	cc.mov(regA[A], tmp);
}

void JitCompiler::EmitADDA_RK()
{
	auto tmp = newTempIntPtr();
	auto label = cc.newLabel();

	cc.mov(tmp, regA[B]);

	// Check if zero, the first operand is zero, if it is, don't add.
	cc.cmp(tmp, 0);
	cc.je(label);

	cc.add(tmp, konstd[C]);

	cc.bind(label);
	cc.mov(regA[A], tmp);
}

void JitCompiler::EmitSUBA()
{
	auto tmp = newTempIntPtr();
	cc.mov(tmp, regA[B]);
	cc.sub(tmp, regD[C]);
	cc.mov(regA[A], tmp);
}

void JitCompiler::EmitEQA_R()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		cc.cmp(regA[B], regA[C]);
		if (check) cc.je(fail);
		else       cc.jne(fail);
	});
}

void JitCompiler::EmitEQA_K()
{
	EmitComparisonOpcode([&](bool check, asmjit::Label& fail, asmjit::Label& success) {
		auto tmp = newTempIntPtr();
		cc.mov(tmp, asmjit::imm_ptr(konsta[C].v));
		cc.cmp(regA[B], tmp);
		if (check) cc.je(fail);
		else       cc.jne(fail);
	});
}

void JitCompiler::CallSqrt(const asmjit::X86Xmm &a, const asmjit::X86Xmm &b)
{
	auto result = newResultXmmSd();
	auto call = CreateCall<double, double>(g_sqrt);
	call->setRet(0, result);
	call->setArg(0, b);
	cc.movsd(a, result);
}
