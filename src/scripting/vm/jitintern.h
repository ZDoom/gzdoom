
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

	static bool CanJit(VMScriptFunction *sfunc);

private:
	// Declare EmitXX functions for the opcodes:
	#define xx(op, name, mode, alt, kreg, ktype)	void Emit##op();
	#include "vmops.h"
	#undef xx

	void Setup();
	void EmitOpcode();

	void EmitDoCall(asmjit::X86Gp ptr);
	void StoreInOuts(int b);
	void LoadReturns(const VMOP *retval, int numret, bool inout);
	void FillReturns(const VMOP *retval, int numret);
	static int DoCall(VMFrameStack *stack, VMFunction *call, int b, int c, VMValue *param, VMReturn *returns, JitExceptionInfo *exceptinfo);

	template <typename Func>
	void EmitComparisonOpcode(Func jmpFunc)
	{
		using namespace asmjit;

		int i = (int)(ptrdiff_t)(pc - sfunc->Code);

		auto successLabel = cc.newLabel();

		auto failLabel = labels[i + 2 + JMPOFS(pc + 1)];

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

	asmjit::X86Gp stack;
	asmjit::X86Gp ret;
	asmjit::X86Gp numret;
	asmjit::X86Gp exceptInfo;

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

	TArray<asmjit::Label> labels;

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
