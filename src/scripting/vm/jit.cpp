
#include "jit.h"
#include "jitintern.h"

extern PString *TypeString;
extern PStruct *TypeVector2;
extern PStruct *TypeVector3;

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
	if (strcmp(sfunc->PrintableName.GetChars(), "StatusScreen.drawNum") != 0)
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

void JitDumpLog(FILE *file, VMScriptFunction *sfunc)
{
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

		fwrite(logger.getString(), logger.getLength(), 1, file);
	}
	catch (const std::exception &e)
	{
		fwrite(logger.getString(), logger.getLength(), 1, file);

		FString err;
		err.Format("Unexpected JIT error: %s\n", e.what());
		fwrite(err.GetChars(), err.Len(), 1, file);
		fclose(file);

		I_FatalError("Unexpected JIT error: %s\n", e.what());
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

		FString lineinfo;
		lineinfo.Format("; line %d: %02x%02x%02x%02x %s", sfunc->PCToLine(pc), pc->op, pc->a, pc->b, pc->c, OpNames[op]);
		cc.comment("", 0);
		cc.comment(lineinfo.GetChars(), lineinfo.Len());

		labels[i].cursor = cc.getCursor();
		ResetTemp();
		EmitOpcode();

		pc++;
	}

	BindLabels();

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

void JitCompiler::BindLabels()
{
	asmjit::CBNode *cursor = cc.getCursor();
	unsigned int size = labels.Size();
	for (unsigned int i = 0; i < size; i++)
	{
		const OpcodeLabel &label = labels[i];
		if (label.inUse)
		{
			cc.setCursor(label.cursor);
			cc.bind(label.label);
		}
	}
	cc.setCursor(cursor);
}

void JitCompiler::Setup()
{
	using namespace asmjit;

	ResetTemp();

	static const char *marks = "=======================================================";
	cc.comment("", 0);
	cc.comment(marks, 56);

	FString funcname;
	funcname.Format("Function: %s", sfunc->PrintableName.GetChars());
	cc.comment(funcname.GetChars(), funcname.Len());

	cc.comment(marks, 56);
	cc.comment("", 0);

	auto unusedFunc = cc.newIntPtr("func"); // VMFunction*
	args = cc.newIntPtr("args"); // VMValue *params
	numargs = cc.newInt32("numargs"); // int numargs
	ret = cc.newIntPtr("ret"); // VMReturn *ret
	numret = cc.newInt32("numret"); // int numret

	cc.addFunc(FuncSignature5<int, VMFunction *, void *, int, void *, int>());
	cc.setArg(0, unusedFunc);
	cc.setArg(1, args);
	cc.setArg(2, numargs);
	cc.setArg(3, ret);
	cc.setArg(4, numret);

	auto stackalloc = cc.newStack(sizeof(VMReturn) * MAX_RETURNS, alignof(VMReturn));
	callReturns = cc.newIntPtr("callReturns");
	cc.lea(callReturns, stackalloc);

	konstd = sfunc->KonstD;
	konstf = sfunc->KonstF;
	konsts = sfunc->KonstS;
	konsta = sfunc->KonstA;

	regD.Resize(sfunc->NumRegD);
	regF.Resize(sfunc->NumRegF);
	regA.Resize(sfunc->NumRegA);
	regS.Resize(sfunc->NumRegS);

	for (int i = 0; i < sfunc->NumRegD; i++)
	{
		FString regname;
		regname.Format("regD%d", i);
		regD[i] = cc.newInt32(regname.GetChars());
	}

	for (int i = 0; i < sfunc->NumRegF; i++)
	{
		FString regname;
		regname.Format("regF%d", i);
		regF[i] = cc.newXmmSd(regname.GetChars());
	}

	for (int i = 0; i < sfunc->NumRegS; i++)
	{
		FString regname;
		regname.Format("regS%d", i);
		regS[i] = cc.newIntPtr(regname.GetChars());
	}

	for (int i = 0; i < sfunc->NumRegA; i++)
	{
		FString regname;
		regname.Format("regA%d", i);
		regA[i] = cc.newIntPtr(regname.GetChars());
	}

	labels.Resize(sfunc->CodeSize);

	// VMCalls[0]++
	auto vmcallsptr = newTempIntPtr();
	auto vmcalls = newTempInt32();
	cc.mov(vmcallsptr, imm_ptr(VMCalls));
	cc.mov(vmcalls, x86::dword_ptr(vmcallsptr));
	cc.add(vmcalls, (int)1);
	cc.mov(x86::dword_ptr(vmcallsptr), vmcalls);

	frameD = cc.newIntPtr();
	frameF = cc.newIntPtr();
	frameS = cc.newIntPtr();
	frameA = cc.newIntPtr();
	params = cc.newIntPtr();

	// the VM version reads this from the stack, but it is constant data
	int offsetParams = ((int)sizeof(VMFrame) + 15) & ~15;
	int offsetF = offsetParams + (int)(sfunc->MaxParam * sizeof(VMValue));
	int offsetS = offsetF + (int)(sfunc->NumRegF * sizeof(double));
	int offsetA = offsetS + (int)(sfunc->NumRegS * sizeof(FString));
	int offsetD = offsetA + (int)(sfunc->NumRegA * sizeof(void*));
	offsetExtra = (offsetD + (int)(sfunc->NumRegD * sizeof(int32_t)) + 15) & ~15;

	vmframe = cc.newIntPtr("vmframe");

	if (sfunc->SpecialInits.Size() == 0 && sfunc->NumRegS == 0)
	{
		// This is a simple frame with no constructors or destructors. Allocate it on the stack ourselves.

		auto vmstack = cc.newStack(sfunc->StackSize, 16);
		cc.lea(vmframe, vmstack);
		cc.lea(params, x86::ptr(vmframe, offsetParams));
		cc.lea(frameF, x86::ptr(vmframe, offsetF));
		cc.lea(frameS, x86::ptr(vmframe, offsetS));
		cc.lea(frameA, x86::ptr(vmframe, offsetA));
		cc.lea(frameD, x86::ptr(vmframe, offsetD));

		auto slowinit = cc.newLabel();
		auto endinit = cc.newLabel();

#if 0 // this crashes sometimes
		cc.cmp(numargs, sfunc->NumArgs);
		cc.jne(slowinit);

		// Is there a better way to know the type than this?
		int argsPos = 0;
		int regd = 0, regf = 0, rega = 0;
		for (unsigned int i = 0; i < sfunc->Proto->ArgumentTypes.Size(); i++)
		{
			const PType *type = sfunc->Proto->ArgumentTypes[i];
			if (type->isPointer())
			{
				cc.mov(regA[rega++], x86::ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, a)));
			}
			else if (type->isIntCompatible())
			{
				cc.mov(regD[regd++], x86::dword_ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, i)));
			}
			else if (type == TypeVector2)
			{
				cc.movsd(regF[regf++], x86::qword_ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f)));
				cc.movsd(regF[regf++], x86::qword_ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f)));
			}
			else if (type == TypeVector3)
			{
				cc.movsd(regF[regf++], x86::qword_ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f)));
				cc.movsd(regF[regf++], x86::qword_ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f)));
				cc.movsd(regF[regf++], x86::qword_ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f)));
			}
			else if (type->isFloat())
			{
				cc.movsd(regF[regf++], x86::qword_ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f)));
			}
			else if (type == TypeString)
			{
				I_FatalError("JIT: Strings are not supported yet for simple frames");
			}
		}

		if (sfunc->NumArgs != argsPos || regd > sfunc->NumRegD || regf > sfunc->NumRegF || rega > sfunc->NumRegA)
			I_FatalError("JIT: sfunc->NumArgs != argsPos || regd > sfunc->NumRegD || regf > sfunc->NumRegF || rega > sfunc->NumRegA");

		cc.jmp(endinit);
#endif
		cc.bind(slowinit);

		auto sfuncptr = cc.newIntPtr();
		cc.mov(sfuncptr, imm_ptr(sfunc));
		if (cc.is64Bit())
			cc.mov(x86::qword_ptr(vmframe, offsetof(VMFrame, Func)), sfuncptr);
		else
			cc.mov(x86::dword_ptr(vmframe, offsetof(VMFrame, Func)), sfuncptr);
		cc.mov(x86::byte_ptr(vmframe, offsetof(VMFrame, NumRegD)), sfunc->NumRegD);
		cc.mov(x86::byte_ptr(vmframe, offsetof(VMFrame, NumRegF)), sfunc->NumRegF);
		cc.mov(x86::byte_ptr(vmframe, offsetof(VMFrame, NumRegS)), sfunc->NumRegS);
		cc.mov(x86::byte_ptr(vmframe, offsetof(VMFrame, NumRegA)), sfunc->NumRegA);
		cc.mov(x86::word_ptr(vmframe, offsetof(VMFrame, MaxParam)), sfunc->MaxParam);
		cc.mov(x86::word_ptr(vmframe, offsetof(VMFrame, NumParam)), 0);

		auto fillParams = CreateCall<void, VMFrame *, VMValue *, int>([](VMFrame *newf, VMValue *args, int numargs) {
			try
			{
				VMFillParams(args, newf, numargs);
			}
			catch (...)
			{
				VMThrowException(std::current_exception());
			}
		});
		fillParams->setArg(0, vmframe);
		fillParams->setArg(1, args);
		fillParams->setArg(2, numargs);

		for (int i = 0; i < sfunc->NumRegD; i++)
			cc.mov(regD[i], x86::dword_ptr(frameD, i * sizeof(int32_t)));

		for (int i = 0; i < sfunc->NumRegF; i++)
			cc.movsd(regF[i], x86::qword_ptr(frameF, i * sizeof(double)));

		for (int i = 0; i < sfunc->NumRegS; i++)
			cc.lea(regS[i], x86::ptr(frameS, i * sizeof(FString)));

		for (int i = 0; i < sfunc->NumRegA; i++)
			cc.mov(regA[i], x86::ptr(frameA, i * sizeof(void*)));

		cc.bind(endinit);
	}
	else
	{
		stack = cc.newIntPtr("stack");
		auto allocFrame = CreateCall<VMFrameStack *, VMScriptFunction *, VMValue *, int>([](VMScriptFunction *func, VMValue *args, int numargs) -> VMFrameStack* {
			try
			{
				VMFrameStack *stack = &GlobalVMStack;
				VMFrame *newf = stack->AllocFrame(func);
				CurrentJitExceptInfo->vmframes++;
				VMFillParams(args, newf, numargs);
				return stack;
			}
			catch (...)
			{
				VMThrowException(std::current_exception());
				return nullptr;
			}
		});
		allocFrame->setRet(0, stack);
		allocFrame->setArg(0, imm_ptr(sfunc));
		allocFrame->setArg(1, args);
		allocFrame->setArg(2, numargs);

		cc.mov(vmframe, x86::ptr(stack)); // stack->Blocks
		cc.mov(vmframe, x86::ptr(vmframe, VMFrameStack::OffsetLastFrame())); // Blocks->LastFrame

		cc.lea(params, x86::ptr(vmframe, offsetParams));
		cc.lea(frameF, x86::ptr(vmframe, offsetF));
		cc.lea(frameS, x86::ptr(vmframe, offsetS));
		cc.lea(frameA, x86::ptr(vmframe, offsetA));
		cc.lea(frameD, x86::ptr(vmframe, offsetD));

		for (int i = 0; i < sfunc->NumRegD; i++)
			cc.mov(regD[i], x86::dword_ptr(frameD, i * sizeof(int32_t)));

		for (int i = 0; i < sfunc->NumRegF; i++)
			cc.movsd(regF[i], x86::qword_ptr(frameF, i * sizeof(double)));

		for (int i = 0; i < sfunc->NumRegS; i++)
			cc.lea(regS[i], x86::ptr(frameS, i * sizeof(FString)));

		for (int i = 0; i < sfunc->NumRegA; i++)
			cc.mov(regA[i], x86::ptr(frameA, i * sizeof(void*)));
	}
}

void JitCompiler::EmitPopFrame()
{
	if (sfunc->SpecialInits.Size() != 0 || sfunc->NumRegS != 0)
	{
		auto popFrame = CreateCall<void, VMFrameStack *>([](VMFrameStack *stack) {
			try
			{
				stack->PopFrame();
				CurrentJitExceptInfo->vmframes--;
			}
			catch (...)
			{
				VMThrowException(std::current_exception());
			}
		});
		popFrame->setArg(0, stack);
	}
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
	auto call = CreateCall<void, VMScriptFunction *, VMOP *, int>([](VMScriptFunction *func, VMOP *line, int r) {
		try
		{
			ThrowAbortException(func, line, (EVMAbortException)r, nullptr);
		}
		catch (...)
		{
			VMThrowException(std::current_exception());
		}
	});
	call->setArg(0, asmjit::imm_ptr(sfunc));
	call->setArg(1, asmjit::imm_ptr(pc));
	call->setArg(2, asmjit::imm(reason));
}

void JitCompiler::EmitThrowException(EVMAbortException reason, asmjit::X86Gp arg1)
{
	// To do: fix throw message and use arg1
	EmitThrowException(reason);
}

asmjit::X86Gp JitCompiler::CheckRegD(int r0, int r1)
{
	if (r0 != r1)
	{
		return regD[r0];
	}
	else
	{
		auto copy = newTempInt32();
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
		auto copy = newTempXmmSd();
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
		auto copy = newTempXmmSd();
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
		auto copy = newTempXmmSd();
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
		auto copy = newTempIntPtr();
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
		auto copy = newTempIntPtr();
		cc.mov(copy, regA[r0]);
		return copy;
	}
}

void JitCompiler::EmitNOP()
{
	cc.nop();
}
