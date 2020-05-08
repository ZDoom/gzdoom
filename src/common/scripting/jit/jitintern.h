
#include "jit.h"

#include "types.h"
#include "stats.h"

#include <functional>
#include <vector>

#include "ir/IR.h"

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

#if 1

class JitCompiler
{
public:

private:
	// Declare EmitXX functions for the opcodes:
	#define xx(op, name, mode, alt, kreg, ktype)	void Emit##op();
	#include "vmops.h"
	#undef xx

	IRBasicBlock* GetLabel(size_t pos) { return nullptr; }

	void EmitReadBarrier();

	void EmitNullPointerThrow(int index, EVMAbortException reason);
	void EmitThrowException(EVMAbortException reason);
	IRBasicBlock* EmitThrowExceptionLabel(EVMAbortException reason);

	static void ThrowArrayOutOfBounds(int index, int size);
	static void ThrowException(int reason);

	template<typename RetType, typename P1>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1)) { return nullptr; }

	template<typename RetType, typename P1, typename P2>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2)) { return nullptr; }

	template<typename RetType, typename P1, typename P2, typename P3>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3)) { return nullptr; }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4)) { return nullptr; }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)) { return nullptr; }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)) { return nullptr; }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7)) { return nullptr; }

	template <typename Func>
	void EmitComparisonOpcode(Func jmpFunc)
	{
		/*
		int i = (int)(ptrdiff_t)(pc - sfunc->Code);
		auto successLabel = cc.newLabel();
		auto failLabel = GetLabel(i + 2 + JMPOFS(pc + 1));
		jmpFunc(static_cast<bool>(A & CMP_CHECK), failLabel, successLabel);
		cc.bind(successLabel);
		pc++; // This instruction uses two instruction slots - skip the next one
		*/
	}

	void EmitVectorComparison(int N, bool check, IRBasicBlock* fail, IRBasicBlock* success);

	IRValue* LoadD(int index) { return cc.CreateLoad(regD[index]); }
	IRValue* LoadF(int index) { return cc.CreateLoad(regF[index]); }
	IRValue* LoadA(int index) { return cc.CreateLoad(regA[index]); }
	IRValue* LoadS(int index) { return cc.CreateLoad(regS[index]); }
	IRValue* ConstD(int index) { return ircontext->getConstantInt(konstd[index]); }
	IRValue* ConstF(int index) { return ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[index]); }
	IRValue* ConstA(int index) { return ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)konsta[index].v); }
	IRValue* ConstS(int index) { return ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)&konsts[index]); }
	IRValue* ConstValueD(int value) { return ircontext->getConstantInt(value); }
	IRValue* ConstValueF(double value) { return ircontext->getConstantFloat(ircontext->getDoubleTy(), value); }
	IRValue* ConstValueA(void* value) { return ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)value); }
	IRValue* ConstValueS(void* value) { return ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)value); }
	void StoreD(IRValue* value, int index) { cc.CreateStore(value, regD[index]); }
	void StoreF(IRValue* value, int index) { cc.CreateStore(value, regF[index]); }
	void StoreA(IRValue* value, int index) { cc.CreateStore(value, regA[index]); }
	void StoreS(IRValue* value, int index) { cc.CreateStore(value, regS[index]); }

	IRValue* OffsetPtr(IRValue* ptr, IRValue* offset) { return cc.CreateGEP(ptr, { offset }); }
	IRValue* OffsetPtr(IRValue* ptr, int offset) { return cc.CreateGEP(ptr, { ircontext->getConstantInt(offset) }); }
	IRValue* ToInt8Ptr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt8PtrTy()); }
	IRValue* ToInt16Ptr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt16PtrTy()); }
	IRValue* ToInt32Ptr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt32PtrTy()); }
	IRValue* ToFloatPtr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getFloatPtrTy()); }
	IRValue* ToDoublePtr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getDoublePtrTy()); }
	IRValue* ToPtrPtr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt8PtrTy()->getPointerTo(ircontext)); }
	void Store8(IRValue* value, IRValue* ptr) { cc.CreateStore(cc.CreateTrunc(value, ircontext->getInt8Ty()), ptr); }
	void Store16(IRValue* value, IRValue* ptr) { cc.CreateStore(cc.CreateTrunc(value, ircontext->getInt16Ty()), ptr); }
	void Store32(IRValue* value, IRValue* ptr) { cc.CreateStore(value, ptr); }
	void StoreFloat(IRValue* value, IRValue* ptr) { cc.CreateStore(cc.CreateFPTrunc(value, ircontext->getFloatTy()), ptr); }
	void StoreDouble(IRValue* value, IRValue* ptr) { cc.CreateStore(value, ptr); }
	IRValue* LoadZExt(IRValue* ptr) { return cc.CreateZExt(cc.CreateLoad(ptr), ircontext->getInt32Ty()); }
	IRValue* LoadSExt(IRValue* ptr) { return cc.CreateSExt(cc.CreateLoad(ptr), ircontext->getInt32Ty()); }
	IRValue* LoadFPExt(IRValue* ptr) { return cc.CreateFPExt(cc.CreateLoad(ptr), ircontext->getDoubleTy()); }
	IRValue* Load(IRValue* ptr) { return cc.CreateLoad(ptr); }

	static void CallAssignString(FString* to, FString* from) { *to = *from; }

	VMScriptFunction* sfunc;

	IRContext* ircontext;
	IRFunction* irfunc;
	IRBuilder cc;

	const int* konstd;
	const double* konstf;
	const FString* konsts;
	const FVoidObj* konsta;

	TArray<IRValue*> regD;
	TArray<IRValue*> regF;
	TArray<IRValue*> regA;
	TArray<IRValue*> regS;

	const VMOP* pc;
	VM_UBYTE op;
};

#else

// To do: get cmake to define these..
#define ASMJIT_BUILD_EMBED
#define ASMJIT_STATIC

#include <asmjit/asmjit.h>
#include <asmjit/x86.h>

struct JitLineInfo
{
	ptrdiff_t InstructionIndex = 0;
	int32_t LineNumber = -1;
	asmjit::Label Label;
};

class JitCompiler
{
public:
	JitCompiler(asmjit::CodeHolder *code, VMScriptFunction *sfunc) : cc(code), sfunc(sfunc) { }

	asmjit::CCFunc *Codegen();
	VMScriptFunction *GetScriptFunction() { return sfunc; }

	TArray<JitLineInfo> LineInfo;

private:
	// Declare EmitXX functions for the opcodes:
	#define xx(op, name, mode, alt, kreg, ktype)	void Emit##op();
	#include "vmops.h"
	#undef xx

	asmjit::FuncSignature CreateFuncSignature();

	void Setup();
	void CreateRegisters();
	void IncrementVMCalls();
	void SetupFrame();
	void SetupSimpleFrame();
	void SetupFullVMFrame();
	void BindLabels();
	void EmitOpcode();
	void EmitPopFrame();

	void EmitNativeCall(VMNativeFunction *target);
	void EmitVMCall(asmjit::X86Gp ptr, VMFunction *target);
	void EmitVtbl(const VMOP *op);

	int StoreCallParams();
	void LoadInOuts();
	void LoadReturns(const VMOP *retval, int numret);
	void FillReturns(const VMOP *retval, int numret);
	void LoadCallResult(int type, int regnum, bool addrof);

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

	template<int N>
	void EmitVectorComparison(bool check, asmjit::Label& fail, asmjit::Label& success)
	{
		bool approx = static_cast<bool>(A & CMP_APPROX);
		if (!approx)
		{
			for (int i = 0; i < N; i++)
			{
				cc.ucomisd(regF[B + i], regF[C + i]);
				if (check)
				{
					cc.jp(success);
					if (i == (N - 1))
					{
						cc.je(fail);
					}
					else
					{
						cc.jne(success);
					}
				}
				else
				{
					cc.jp(fail);
					cc.jne(fail);
				}
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

			for (int i = 0; i < N; i++)
			{
				cc.movsd(tmp, regF[B + i]);
				cc.subsd(tmp, regF[C + i]);
				cc.movsd(absMaskXmm, absMask);
				cc.andpd(tmp, absMaskXmm);
				cc.movsd(epsilonXmm, epsilon);
				cc.ucomisd(epsilonXmm, tmp);

				if (check)
				{
					cc.jp(success);
					if (i == (N - 1))
					{
						cc.ja(fail);
					}
					else
					{
						cc.jna(success);
					}
				}
				else
				{
					cc.jp(fail);
					cc.jna(fail);
				}
			}
		}
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

	void EmitReadBarrier();

	void EmitNullPointerThrow(int index, EVMAbortException reason);
	void EmitThrowException(EVMAbortException reason);
	asmjit::Label EmitThrowExceptionLabel(EVMAbortException reason);

	static void ThrowArrayOutOfBounds(int index, int size);
	static void ThrowException(int reason);

	asmjit::X86Gp CheckRegD(int r0, int r1);
	asmjit::X86Xmm CheckRegF(int r0, int r1);
	asmjit::X86Xmm CheckRegF(int r0, int r1, int r2);
	asmjit::X86Xmm CheckRegF(int r0, int r1, int r2, int r3);
	asmjit::X86Gp CheckRegS(int r0, int r1);
	asmjit::X86Gp CheckRegA(int r0, int r1);

	asmjit::X86Compiler cc;
	VMScriptFunction *sfunc;

	asmjit::CCFunc *func = nullptr;
	asmjit::X86Gp args;
	asmjit::X86Gp numargs;
	asmjit::X86Gp ret;
	asmjit::X86Gp numret;
	asmjit::X86Gp stack;

	int offsetParams;
	int offsetF;
	int offsetS;
	int offsetA;
	int offsetD;
	int offsetExtra;

	TArray<const VMOP *> ParamOpcodes;

	void CheckVMFrame();
	asmjit::X86Gp GetCallReturns();

	bool vmframeAllocated = false;
	asmjit::CBNode *vmframeCursor = nullptr;
	asmjit::X86Gp vmframe;

	bool callReturnsAllocated = false;
	asmjit::CBNode *callReturnsCursor = nullptr;
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

void *AddJitFunction(asmjit::CodeHolder* code, JitCompiler *compiler);
asmjit::CodeInfo GetHostCodeInfo();

#endif
