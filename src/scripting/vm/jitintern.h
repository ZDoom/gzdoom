
#include "jit.h"
#include "i_system.h"
#include "types.h"
#include "stats.h"

// To do: get cmake to define these..
#define ASMJIT_BUILD_EMBED
#define ASMJIT_STATIC

#include <asmjit/asmjit.h>
#include <asmjit/x86.h>
#include <functional>
#include <vector>

extern cycle_t VMCycles[10];
extern int VMCalls[10];

#define A				(pc[0].a)
#define B				(pc[0].b)
#define C				(pc[0].c)
#define Cs				(pc[0].cs)
#define BC				(pc[0].i16u)
#define BCs				(pc[0].i16)
#define ABCs			(pc[0].i24)
#define JMPOFS(x)		((x)->i24)

class JitCompiler
{
public:
	JitCompiler(asmjit::CodeHolder *code, VMScriptFunction *sfunc) : cc(code), sfunc(sfunc) { }

	void Codegen();

private:
	// Declare EmitXX functions for the opcodes:
	#define xx(op, name, mode, alt, kreg, ktype)	void Emit##op();
	#include "vmops.h"
	#undef xx

	void Setup();
	void BindLabels();
	void EmitOpcode();
	void EmitPopFrame();

	void EmitDoCall(asmjit::X86Gp ptr);
	void EmitScriptCall(asmjit::X86Gp vmfunc, asmjit::X86Gp paramsptr);

	void EmitDoTail(asmjit::X86Gp ptr);
	void EmitScriptTailCall(asmjit::X86Gp vmfunc, asmjit::X86Gp result, asmjit::X86Gp paramsptr);

	void StoreInOuts(int b);
	void LoadInOuts(int b);
	void LoadReturns(const VMOP *retval, int numret);
	void FillReturns(const VMOP *retval, int numret);
	void LoadCallResult(const VMOP &opdata, bool addrof);

	template <typename Func>
	void EmitComparisonOpcode(Func jmpFunc)
	{
		using namespace asmjit;

		int i = (int)(ptrdiff_t)(pc - sfunc->Code);

		auto successLabel = cc.newLabel();

		auto failLabel = GetLabel(i + 2 + JMPOFS(pc + 1));

		jmpFunc(static_cast<bool>(A & CMP_CHECK), failLabel, successLabel);

		cc.bind(successLabel);
		pc++; // This instruction uses two instruction slots - skip the next one
	}

	static uint64_t ToMemAddress(const void *d)
	{
		return (uint64_t)(ptrdiff_t)d;
	}

	void CallSqrt(const asmjit::X86Xmm &a, const asmjit::X86Xmm &b);

	static void CallAssignString(FString* to, FString* from) {
		*to = *from;
	}

	template<typename RetType, typename P1>
	asmjit::CCFuncCall *CreateCall(RetType(*func)(P1 p1)) { return cc.call(asmjit::imm_ptr(reinterpret_cast<void*>(static_cast<RetType(*)(P1)>(func))), asmjit::FuncSignature1<RetType, P1>()); }

	template<typename RetType, typename P1, typename P2>
	asmjit::CCFuncCall *CreateCall(RetType(*func)(P1 p1, P2 p2)) { return cc.call(asmjit::imm_ptr(reinterpret_cast<void*>(static_cast<RetType(*)(P1, P2)>(func))), asmjit::FuncSignature2<RetType, P1, P2>()); }

	template<typename RetType, typename P1, typename P2, typename P3>
	asmjit::CCFuncCall *CreateCall(RetType(*func)(P1 p1, P2 p2, P3 p3)) { return cc.call(asmjit::imm_ptr(reinterpret_cast<void*>(static_cast<RetType(*)(P1, P2, P3)>(func))), asmjit::FuncSignature3<RetType, P1, P2, P3>()); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4>
	asmjit::CCFuncCall *CreateCall(RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4)) { return cc.call(asmjit::imm_ptr(reinterpret_cast<void*>(static_cast<RetType(*)(P1, P2, P3, P4)>(func))), asmjit::FuncSignature4<RetType, P1, P2, P3, P4>()); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5>
	asmjit::CCFuncCall *CreateCall(RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)) { return cc.call(asmjit::imm_ptr(reinterpret_cast<void*>(static_cast<RetType(*)(P1, P2, P3, P4, P5)>(func))), asmjit::FuncSignature5<RetType, P1, P2, P3, P4, P5>()); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	asmjit::CCFuncCall *CreateCall(RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)) { return cc.call(asmjit::imm_ptr(reinterpret_cast<void*>(static_cast<RetType(*)(P1, P2, P3, P4, P5, P6)>(func))), asmjit::FuncSignature6<RetType, P1, P2, P3, P4, P5, P6>()); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
	asmjit::CCFuncCall *CreateCall(RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7)) { return cc.call(asmjit::imm_ptr(reinterpret_cast<void*>(static_cast<RetType(*)(P1, P2, P3, P4, P5, P6, P7)>(func))), asmjit::FuncSignature7<RetType, P1, P2, P3, P4, P5, P6, P7>()); }

	FString regname;
	size_t tmpPosInt32, tmpPosInt64, tmpPosIntPtr, tmpPosXmmSd, tmpPosXmmSs, tmpPosXmmPd, resultPosInt32, resultPosIntPtr, resultPosXmmSd;
	std::vector<asmjit::X86Gp> regTmpInt32, regTmpInt64, regTmpIntPtr, regResultInt32, regResultIntPtr;
	std::vector<asmjit::X86Xmm> regTmpXmmSd, regTmpXmmSs, regTmpXmmPd, regResultXmmSd;

	void ResetTemp()
	{
		tmpPosInt32 = 0;
		tmpPosInt64 = 0;
		tmpPosIntPtr = 0;
		tmpPosXmmSd = 0;
		tmpPosXmmSs = 0;
		tmpPosXmmPd = 0;
		resultPosInt32 = 0;
		resultPosIntPtr = 0;
		resultPosXmmSd = 0;
	}

	template<typename T, typename NewFunc>
	T newTempRegister(std::vector<T> &tmpVector, size_t &tmpPos, const char *name, NewFunc newCallback)
	{
		if (tmpPos == tmpVector.size())
		{
			regname.Format("%s%d", name, (int)tmpVector.size());
			tmpVector.push_back(newCallback(regname.GetChars()));
		}
		return tmpVector[tmpPos++];
	}

	asmjit::X86Gp newTempInt32() { return newTempRegister(regTmpInt32, tmpPosInt32, "tmpDword", [&](const char *name) { return cc.newInt32(name); }); }
	asmjit::X86Gp newTempInt64() { return newTempRegister(regTmpInt64, tmpPosInt64, "tmpQword", [&](const char *name) { return cc.newInt64(name); }); }
	asmjit::X86Gp newTempIntPtr() { return newTempRegister(regTmpIntPtr, tmpPosIntPtr, "tmpPtr", [&](const char *name) { return cc.newIntPtr(name); }); }
	asmjit::X86Xmm newTempXmmSd() { return newTempRegister(regTmpXmmSd, tmpPosXmmSd, "tmpXmmSd", [&](const char *name) { return cc.newXmmSd(name); }); }
	asmjit::X86Xmm newTempXmmSs() { return newTempRegister(regTmpXmmSs, tmpPosXmmSs, "tmpXmmSs", [&](const char *name) { return cc.newXmmSs(name); }); }
	asmjit::X86Xmm newTempXmmPd() { return newTempRegister(regTmpXmmPd, tmpPosXmmPd, "tmpXmmPd", [&](const char *name) { return cc.newXmmPd(name); }); }

	asmjit::X86Gp newResultInt32() { return newTempRegister(regResultInt32, resultPosInt32, "resultDword", [&](const char *name) { return cc.newInt32(name); }); }
	asmjit::X86Gp newResultIntPtr() { return newTempRegister(regResultIntPtr, resultPosIntPtr, "resultPtr", [&](const char *name) { return cc.newIntPtr(name); }); }
	asmjit::X86Xmm newResultXmmSd() { return newTempRegister(regResultXmmSd, resultPosXmmSd, "resultXmmSd", [&](const char *name) { return cc.newXmmSd(name); }); }

	void EmitNullPointerThrow(int index, EVMAbortException reason);
	void EmitThrowException(EVMAbortException reason);
	void EmitThrowException(EVMAbortException reason, asmjit::X86Gp arg1);

	asmjit::X86Gp CheckRegD(int r0, int r1);
	asmjit::X86Xmm CheckRegF(int r0, int r1);
	asmjit::X86Xmm CheckRegF(int r0, int r1, int r2);
	asmjit::X86Xmm CheckRegF(int r0, int r1, int r2, int r3);
	asmjit::X86Gp CheckRegS(int r0, int r1);
	asmjit::X86Gp CheckRegA(int r0, int r1);

	asmjit::X86Compiler cc;
	VMScriptFunction *sfunc;

	asmjit::X86Gp args;
	asmjit::X86Gp numargs;
	asmjit::X86Gp ret;
	asmjit::X86Gp numret;
	asmjit::X86Gp stack;

	int offsetExtra;
	asmjit::X86Gp vmframe;
	asmjit::X86Gp frameD;
	asmjit::X86Gp frameF;
	asmjit::X86Gp frameS;
	asmjit::X86Gp frameA;
	asmjit::X86Gp params;
	int NumParam = 0; // Actually part of vmframe (f->NumParam), but nobody seems to read that?
	TArray<const VMOP *> ParamOpcodes;
	asmjit::X86Gp callReturns;

	const int *konstd;
	const double *konstf;
	const FString *konsts;
	const FVoidObj *konsta;

	TArray<asmjit::X86Gp> regD;
	TArray<asmjit::X86Xmm> regF;
	TArray<asmjit::X86Gp> regA;
	TArray<asmjit::X86Gp> regS;

	struct OpcodeLabel
	{
		asmjit::CBNode *cursor = nullptr;
		asmjit::Label label;
		bool inUse = false;
	};

	asmjit::Label GetLabel(size_t pos)
	{
		auto &label = labels[pos];
		if (!label.inUse)
		{
			label.label = cc.newLabel();
			label.inUse = true;
		}
		return label.label;
	}

	TArray<OpcodeLabel> labels;

	const VMOP *pc;
	VM_UBYTE op;
};

class AsmJitException : public std::exception
{
public:
	AsmJitException(asmjit::Error error, const char *message) noexcept : error(error), message(message)
	{
	}

	const char* what() const noexcept override
	{
		return message.GetChars();
	}

	asmjit::Error error;
	FString message;
};

class ThrowingErrorHandler : public asmjit::ErrorHandler
{
public:
	bool handleError(asmjit::Error err, const char *message, asmjit::CodeEmitter *origin) override
	{
		throw AsmJitException(err, message);
	}
};
