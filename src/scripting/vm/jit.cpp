
#include "jit.h"
#include "jitintern.h"

static asmjit::JitRuntime *jit;
static int jitRefCount = 0;

asmjit::JitRuntime *JitGetRuntime()
{
	if (!jit)
		jit = new asmjit::JitRuntime;
	jitRefCount++;
	return jit;
}

void JitCleanUp(VMScriptFunction *func)
{
	jitRefCount--;
	if (jitRefCount == 0)
	{
		delete jit;
		jit = nullptr;
	}
}

static void OutputJitLog(const asmjit::StringLogger &logger)
{
	// Write line by line since I_FatalError seems to cut off long strings
	const char *pos = logger.getString();
	const char *end = pos;
	while (*end)
	{
		if (*end == '\n')
		{
			FString substr(pos, (int)(ptrdiff_t)(end - pos));
			Printf("%s\n", substr.GetChars());
			pos = end + 1;
		}
		end++;
	}
	if (pos != end)
		Printf("%s\n", pos);
}

//#define DEBUG_JIT

JitFuncPtr JitCompile(VMScriptFunction *sfunc)
{
#if defined(DEBUG_JIT)
	if (strcmp(sfunc->PrintableName.GetChars(), "Key.ShouldStay") != 0)
		return nullptr;
#else
	if (!JitCompiler::CanJit(sfunc))
		return nullptr;
#endif

	//Printf("Jitting function: %s\n", sfunc->PrintableName.GetChars());

	using namespace asmjit;
	StringLogger logger;
	try
	{
		auto *jit = JitGetRuntime();

		ThrowingErrorHandler errorHandler;
		CodeHolder code;
		code.init(jit->getCodeInfo());
		code.setErrorHandler(&errorHandler);
		code.setLogger(&logger);

		JitCompiler compiler(&code, sfunc);
		compiler.Codegen();

		JitFuncPtr fn = nullptr;
		Error err = jit->add(&fn, &code);
		if (err)
			I_FatalError("JitRuntime::add failed: %d", err);

#if defined(DEBUG_JIT)
		OutputJitLog(logger);
#endif

		return fn;
	}
	catch (const std::exception &e)
	{
		OutputJitLog(logger);
		I_FatalError("Unexpected JIT error: %s\n", e.what());
		return nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////

static const char *OpNames[NUM_OPS] =
{
#define xx(op, name, mode, alt, kreg, ktype)	#op,
#include "vmops.h"
#undef xx
};

void JitCompiler::Codegen()
{
	Setup();

	pc = sfunc->Code;
	auto end = pc + sfunc->CodeSize;
	while (pc != end)
	{
		int i = (int)(ptrdiff_t)(pc - sfunc->Code);
		op = pc->op;

		cc.bind(labels[i]);

		FString lineinfo;
		lineinfo.Format("; %s(line %d): %02x%02x%02x%02x %s", sfunc->PrintableName.GetChars(), sfunc->PCToLine(pc), pc->op, pc->a, pc->b, pc->c, OpNames[op]);
		cc.comment(lineinfo.GetChars(), lineinfo.Len());

		EmitOpcode();

		pc++;
	}

	cc.endFunc();
	cc.finalize();
}

void JitCompiler::EmitOpcode()
{
	switch (op)
	{
		#define xx(op, name, mode, alt, kreg, ktype)	case OP_##op: Emit##op(); break;
		#include "vmops.h"
		#undef xx

	default:
		I_FatalError("JIT error: Unknown VM opcode %d\n", op);
		break;
	}
}

bool JitCompiler::CanJit(VMScriptFunction *sfunc)
{
	int size = sfunc->CodeSize;
	for (int i = 0; i < size; i++)
	{
		// Functions not implemented at all yet:

		switch (sfunc->Code[i].op)
		{
		default:
			break;
		case OP_LKS_R:
		case OP_LFP:
		case OP_LCS_R:
		case OP_SS_R:
		case OP_IJMP:
		case OP_TAIL:
		case OP_TAIL_K:
			return false;
		}

		// Partially implemented functions:

		auto pc = sfunc->Code + i;
		if (sfunc->Code[i].op == OP_CAST)
		{
			switch (C)
			{
			case CAST_I2F:
			case CAST_U2F:
			case CAST_F2I:
			case CAST_F2U:
				break;
			default:
				return false;
			}
		}
		else if (sfunc->Code[i].op == OP_CASTB)
		{
			if (C == CASTB_S)
				return false;
		}
		else if (sfunc->Code[i].op == OP_PARAM)
		{
			if (!!(B & REGT_MULTIREG3) || !!(B & REGT_MULTIREG2))
				return false;
		}
		else if (sfunc->Code[i].op == OP_RESULT)
		{
			if (!!(B & REGT_MULTIREG3) || !!(B & REGT_MULTIREG2))
				return false;
		}
	}
	return true;
}

void JitCompiler::Setup()
{
	using namespace asmjit;

	FString funcname;
	funcname.Format("Function: %s", sfunc->PrintableName.GetChars());
	cc.comment(funcname.GetChars(), funcname.Len());

	stack = cc.newIntPtr("stack"); // VMFrameStack *stack
	vmregs = cc.newIntPtr("vmregs"); // VMRegisters *vmregs
	ret = cc.newIntPtr("ret"); // VMReturn *ret
	numret = cc.newInt32("numret"); // int numret
	exceptInfo = cc.newIntPtr("exceptinfo"); // JitExceptionInfo *exceptInfo

	cc.addFunc(FuncSignature5<int, void *, void *, void *, int, void *>());
	cc.setArg(0, stack);
	cc.setArg(1, vmregs);
	cc.setArg(2, ret);
	cc.setArg(3, numret);
	cc.setArg(4, exceptInfo);

	auto stack = cc.newStack(sizeof(VMReturn) * MAX_RETURNS, alignof(VMReturn));
	callReturns = cc.newIntPtr("callReturns");
	cc.lea(callReturns, stack);

	konstd = sfunc->KonstD;
	konstf = sfunc->KonstF;
	konsts = sfunc->KonstS;
	konsta = sfunc->KonstA;

	regD.Resize(sfunc->NumRegD);
	regF.Resize(sfunc->NumRegF);
	regA.Resize(sfunc->NumRegA);
	regS.Resize(sfunc->NumRegS);

	frameD = cc.newIntPtr();
	frameF = cc.newIntPtr();
	frameS = cc.newIntPtr();
	frameA = cc.newIntPtr();
	params = cc.newIntPtr();
	cc.mov(frameD, x86::ptr(vmregs, offsetof(VMRegisters, d)));
	cc.mov(frameF, x86::ptr(vmregs, offsetof(VMRegisters, f)));
	cc.mov(frameS, x86::ptr(vmregs, offsetof(VMRegisters, s)));
	cc.mov(frameA, x86::ptr(vmregs, offsetof(VMRegisters, a)));
	cc.mov(params, x86::ptr(vmregs, offsetof(VMRegisters, param)));

	if (sfunc->NumRegD > 0)
	{
		for (int i = 0; i < sfunc->NumRegD; i++)
		{
			FString regname;
			regname.Format("regD%d", i);
			regD[i] = cc.newInt32(regname.GetChars());
			cc.mov(regD[i], x86::dword_ptr(frameD, i * sizeof(int32_t)));
		}
	}
	if (sfunc->NumRegF > 0)
	{
		for (int i = 0; i < sfunc->NumRegF; i++)
		{
			FString regname;
			regname.Format("regF%d", i);
			regF[i] = cc.newXmmSd(regname.GetChars());
			cc.movsd(regF[i], x86::qword_ptr(frameF, i * sizeof(double)));
		}
	}
	if (sfunc->NumRegS > 0)
	{
		for (int i = 0; i < sfunc->NumRegS; i++)
		{
			FString regname;
			regname.Format("regS%d", i);
			regS[i] = cc.newIntPtr(regname.GetChars());
			cc.mov(regS[i], frameS);
			if (i != 0) cc.add(regS[i], (int)(i * sizeof(FString)));
		}
	}
	if (sfunc->NumRegA > 0)
	{
		for (int i = 0; i < sfunc->NumRegA; i++)
		{
			FString regname;
			regname.Format("regA%d", i);
			regA[i] = cc.newIntPtr(regname.GetChars());
			cc.mov(regA[i], x86::ptr(frameA, i * sizeof(void*)));
		}
	}

	int size = sfunc->CodeSize;
	labels.Resize(size);
	for (int i = 0; i < size; i++) labels[i] = cc.newLabel();
}

void JitCompiler::EmitNullPointerThrow(int index, EVMAbortException reason)
{
	auto label = cc.newLabel();
	cc.test(regA[index], regA[index]);
	cc.jne(label);
	EmitThrowException(reason);
	cc.bind(label);
}

void JitCompiler::EmitThrowException(EVMAbortException reason)
{
	using namespace asmjit;

	// Update JitExceptionInfo struct
	cc.mov(x86::dword_ptr(exceptInfo, 0 * 4), (int32_t)reason);
#ifdef ASMJIT_ARCH_X64
	cc.mov(x86::qword_ptr(exceptInfo, 4 * 4), ToMemAddress(pc));
#else
	cc.mov(x86::dword_ptr(exceptInfo, 4 * 4), ToMemAddress(pc));
#endif

	// Return from function
	X86Gp vReg = cc.newInt32();
	cc.mov(vReg, 0);
	cc.ret(vReg);
}

void JitCompiler::EmitThrowException(EVMAbortException reason, asmjit::X86Gp arg1)
{
	using namespace asmjit;

	// Update JitExceptionInfo struct
	cc.mov(x86::dword_ptr(exceptInfo, 0 * 4), (int32_t)reason);
	cc.mov(x86::dword_ptr(exceptInfo, 1 * 4), arg1);
#ifdef ASMJIT_ARCH_X64
	cc.mov(x86::qword_ptr(exceptInfo, 4 * 4), ToMemAddress(pc));
#else
	cc.mov(x86::dword_ptr(exceptInfo, 4 * 4), ToMemAddress(pc));
#endif

	// Return from function
	X86Gp vReg = cc.newInt32();
	cc.mov(vReg, 0);
	cc.ret(vReg);
}

asmjit::X86Gp JitCompiler::CheckRegD(int r0, int r1)
{
	if (r0 != r1)
	{
		return regD[r0];
	}
	else
	{
		auto copy = cc.newInt32();
		cc.mov(copy, regD[r0]);
		return copy;
	}
}

asmjit::X86Xmm JitCompiler::CheckRegF(int r0, int r1)
{
	if (r0 != r1)
	{
		return regF[r0];
	}
	else
	{
		auto copy = cc.newXmm();
		cc.movsd(copy, regF[r0]);
		return copy;
	}
}

asmjit::X86Xmm JitCompiler::CheckRegF(int r0, int r1, int r2)
{
	if (r0 != r1 && r0 != r2)
	{
		return regF[r0];
	}
	else
	{
		auto copy = cc.newXmm();
		cc.movsd(copy, regF[r0]);
		return copy;
	}
}

asmjit::X86Xmm JitCompiler::CheckRegF(int r0, int r1, int r2, int r3)
{
	if (r0 != r1 && r0 != r2 && r0 != r3)
	{
		return regF[r0];
	}
	else
	{
		auto copy = cc.newXmm();
		cc.movsd(copy, regF[r0]);
		return copy;
	}
}

asmjit::X86Gp JitCompiler::CheckRegS(int r0, int r1)
{
	if (r0 != r1)
	{
		return regS[r0];
	}
	else
	{
		auto copy = cc.newIntPtr();
		cc.mov(copy, regS[r0]);
		return copy;
	}
}

asmjit::X86Gp JitCompiler::CheckRegA(int r0, int r1)
{
	if (r0 != r1)
	{
		return regA[r0];
	}
	else
	{
		auto copy = cc.newIntPtr();
		cc.mov(copy, regA[r0]);
		return copy;
	}
}

void JitCompiler::EmitNOP()
{
	cc.nop();
}
