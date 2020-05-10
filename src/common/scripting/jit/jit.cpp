
#include "jit.h"
#include "jitintern.h"
#include "printf.h"

extern PString *TypeString;
extern PStruct *TypeVector2;
extern PStruct *TypeVector3;

#if 1

JitFuncPtr JitCompile(VMScriptFunction* sfunc)
{
	return nullptr;
}

void JitDumpLog(FILE* file, VMScriptFunction* sfunc)
{
}

#else

static void OutputJitLog(const asmjit::StringLogger &logger);

JitFuncPtr JitCompile(VMScriptFunction *sfunc)
{
#if 0
	if (strcmp(sfunc->PrintableName.GetChars(), "StatusScreen.drawNum") != 0)
		return nullptr;
#endif

	using namespace asmjit;
	StringLogger logger;
	try
	{
		ThrowingErrorHandler errorHandler;
		CodeHolder code;
		code.init(GetHostCodeInfo());
		code.setErrorHandler(&errorHandler);
		code.setLogger(&logger);

		JitCompiler compiler(&code, sfunc);
		return reinterpret_cast<JitFuncPtr>(AddJitFunction(&code, &compiler));
	}
	catch (const CRecoverableError &e)
	{
		OutputJitLog(logger);
		Printf("%s: Unexpected JIT error: %s\n",sfunc->PrintableName.GetChars(), e.what());
		return nullptr;
	}
}

void JitDumpLog(FILE *file, VMScriptFunction *sfunc)
{
	using namespace asmjit;
	StringLogger logger;
	try
	{
		ThrowingErrorHandler errorHandler;
		CodeHolder code;
		code.init(GetHostCodeInfo());
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

#endif

/////////////////////////////////////////////////////////////////////////////

static const char *OpNames[NUM_OPS] =
{
#define xx(op, name, mode, alt, kreg, ktype)	#op,
#include "vmops.h"
#undef xx
};

IRFunction* JitCompiler::Codegen()
{
	Setup();

	int lastLine = -1;

	pc = sfunc->Code;
	auto end = pc + sfunc->CodeSize;
	while (pc != end)
	{
		int i = (int)(ptrdiff_t)(pc - sfunc->Code);
		op = pc->op;

		int curLine = sfunc->PCToLine(pc);
		if (curLine != lastLine)
		{
			lastLine = curLine;

			/*auto label = cc.newLabel();
			cc.bind(label);

			JitLineInfo info;
			info.Label = label;
			info.LineNumber = curLine;
			LineInfo.Push(info);*/
		}

#if 0
		if (op != OP_PARAM && op != OP_PARAMI && op != OP_VTBL)
		{
			FString lineinfo;
			lineinfo.Format("; line %d: %02x%02x%02x%02x %s", curLine, pc->op, pc->a, pc->b, pc->c, OpNames[op]);
			cc.comment("", 0);
			cc.comment(lineinfo.GetChars(), lineinfo.Len());
		}

		labels[i].cursor = cc.getCursor();
#endif

		EmitOpcode();

		pc++;
	}

	BindLabels();

	/*
	auto code = cc.getCode ();
	for (unsigned int j = 0; j < LineInfo.Size (); j++)
	{
		auto info = LineInfo[j];

		if (!code->isLabelValid (info.Label))
		{
			continue;
		}

		info.InstructionIndex = code->getLabelOffset (info.Label);

		LineInfo[j] = info;
	}

	std::stable_sort(LineInfo.begin(), LineInfo.end(), [](const JitLineInfo &a, const JitLineInfo &b) { return a.InstructionIndex < b.InstructionIndex; });
	*/

	return irfunc;
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
#if 0
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
#endif
}

void JitCompiler::CheckVMFrame()
{
	if (!vmframe)
	{
		vmframe = irfunc->createAlloca(ircontext->getInt8Ty(), ircontext->getConstantInt(sfunc->StackSize), "vmframe");
	}
}

IRValue* JitCompiler::GetCallReturns()
{
	if (!callReturns)
	{
		callReturns = irfunc->createAlloca(ircontext->getInt8Ty(), ircontext->getConstantInt(sizeof(VMReturn) * MAX_RETURNS), "callReturns");
	}
	return callReturns;
}

IRBasicBlock* JitCompiler::GetLabel(size_t pos)
{
	return nullptr;
}

void JitCompiler::Setup()
{
#if 0
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

	func = cc.addFunc(FuncSignature5<int, VMFunction *, void *, int, void *, int>());
	cc.setArg(0, unusedFunc);
	cc.setArg(1, args);
	cc.setArg(2, numargs);
	cc.setArg(3, ret);
	cc.setArg(4, numret);

	callReturnsCursor = cc.getCursor();

	konstd = sfunc->KonstD;
	konstf = sfunc->KonstF;
	konsts = sfunc->KonstS;
	konsta = sfunc->KonstA;

	labels.Resize(sfunc->CodeSize);
#endif

	CreateRegisters();
	IncrementVMCalls();
	SetupFrame();
}

void JitCompiler::SetupFrame()
{
	// the VM version reads this from the stack, but it is constant data
	offsetParams = ((int)sizeof(VMFrame) + 15) & ~15;
	offsetF = offsetParams + (int)(sfunc->MaxParam * sizeof(VMValue));
	offsetS = offsetF + (int)(sfunc->NumRegF * sizeof(double));
	offsetA = offsetS + (int)(sfunc->NumRegS * sizeof(FString));
	offsetD = offsetA + (int)(sfunc->NumRegA * sizeof(void*));
	offsetExtra = (offsetD + (int)(sfunc->NumRegD * sizeof(int32_t)) + 15) & ~15;

	if (sfunc->SpecialInits.Size() == 0 && sfunc->NumRegS == 0)
	{
		SetupSimpleFrame();
	}
	else
	{
		SetupFullVMFrame();
	}
}

void JitCompiler::SetupSimpleFrame()
{
#if 0
	// This is a simple frame with no constructors or destructors. Allocate it on the stack ourselves.

	vmframeCursor = cc.getCursor();

	int argsPos = 0;
	int regd = 0, regf = 0, rega = 0;
	for (unsigned int i = 0; i < sfunc->Proto->ArgumentTypes.Size(); i++)
	{
		const PType *type = sfunc->Proto->ArgumentTypes[i];
		if (sfunc->ArgFlags.Size() && sfunc->ArgFlags[i] & (VARF_Out | VARF_Ref))
		{
			cc.mov(regA[rega++], x86::ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, a)));
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
		else if (type == TypeFloat64)
		{
			cc.movsd(regF[regf++], x86::qword_ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f)));
		}
		else if (type == TypeString)
		{
			I_FatalError("JIT: Strings are not supported yet for simple frames");
		}
		else if (type->isIntCompatible())
		{
			cc.mov(regD[regd++], x86::dword_ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, i)));
		}
		else
		{
			cc.mov(regA[rega++], x86::ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, a)));
		}
	}

	if (sfunc->NumArgs != argsPos || regd > sfunc->NumRegD || regf > sfunc->NumRegF || rega > sfunc->NumRegA)
		I_FatalError("JIT: sfunc->NumArgs != argsPos || regd > sfunc->NumRegD || regf > sfunc->NumRegF || rega > sfunc->NumRegA");

	for (int i = regd; i < sfunc->NumRegD; i++)
		cc.xor_(regD[i], regD[i]);

	for (int i = regf; i < sfunc->NumRegF; i++)
		cc.xorpd(regF[i], regF[i]);

	for (int i = rega; i < sfunc->NumRegA; i++)
		cc.xor_(regA[i], regA[i]);
#endif
}

static VMFrameStack *CreateFullVMFrame(VMScriptFunction *func, VMValue *args, int numargs)
{
	VMFrameStack *stack = &GlobalVMStack;
	VMFrame *newf = stack->AllocFrame(func);
	VMFillParams(args, newf, numargs);
	return stack;
}

void JitCompiler::SetupFullVMFrame()
{
#if 0
	stack = cc.newIntPtr("stack");
	auto allocFrame = CreateCall<VMFrameStack *, VMScriptFunction *, VMValue *, int>(CreateFullVMFrame);
	allocFrame->setRet(0, stack);
	allocFrame->setArg(0, imm_ptr(sfunc));
	allocFrame->setArg(1, args);
	allocFrame->setArg(2, numargs);

	vmframe = cc.newIntPtr("vmframe");
	cc.mov(vmframe, x86::ptr(stack)); // stack->Blocks
	cc.mov(vmframe, x86::ptr(vmframe, VMFrameStack::OffsetLastFrame())); // Blocks->LastFrame

	for (int i = 0; i < sfunc->NumRegD; i++)
		cc.mov(regD[i], x86::dword_ptr(vmframe, offsetD + i * sizeof(int32_t)));

	for (int i = 0; i < sfunc->NumRegF; i++)
		cc.movsd(regF[i], x86::qword_ptr(vmframe, offsetF + i * sizeof(double)));

	for (int i = 0; i < sfunc->NumRegS; i++)
		cc.lea(regS[i], x86::ptr(vmframe, offsetS + i * sizeof(FString)));

	for (int i = 0; i < sfunc->NumRegA; i++)
		cc.mov(regA[i], x86::ptr(vmframe, offsetA + i * sizeof(void*)));
#endif
}

static void PopFullVMFrame(VMFrameStack *stack)
{
	stack->PopFrame();
}

void JitCompiler::EmitPopFrame()
{
#if 0
	if (sfunc->SpecialInits.Size() != 0 || sfunc->NumRegS != 0)
	{
		auto popFrame = CreateCall<void, VMFrameStack *>(PopFullVMFrame);
		popFrame->setArg(0, stack);
	}
#endif
}

void JitCompiler::IncrementVMCalls()
{
#if 0
	// VMCalls[0]++
	auto vmcallsptr = newTempIntPtr();
	auto vmcalls = newTempInt32();
	cc.mov(vmcallsptr, asmjit::imm_ptr(VMCalls));
	cc.mov(vmcalls, asmjit::x86::dword_ptr(vmcallsptr));
	cc.add(vmcalls, (int)1);
	cc.mov(asmjit::x86::dword_ptr(vmcallsptr), vmcalls);
#endif
}

void JitCompiler::CreateRegisters()
{
	regD.Resize(sfunc->NumRegD);
	regF.Resize(sfunc->NumRegF);
	regA.Resize(sfunc->NumRegA);
	regS.Resize(sfunc->NumRegS);

	FString regname;
	IRType* type;
	IRValue* arraySize = ircontext->getConstantInt(1);

	type = ircontext->getInt32Ty();
	for (int i = 0; i < sfunc->NumRegD; i++)
	{
		regname.Format("regD%d", i);
		regD[i] = irfunc->createAlloca(type, arraySize, regname.GetChars());
	}

	type = ircontext->getDoubleTy();
	for (int i = 0; i < sfunc->NumRegF; i++)
	{
		regname.Format("regF%d", i);
		regF[i] = irfunc->createAlloca(type, arraySize, regname.GetChars());
	}

	type = ircontext->getInt8PtrTy();
	for (int i = 0; i < sfunc->NumRegS; i++)
	{
		regname.Format("regS%d", i);
		regS[i] = irfunc->createAlloca(type, arraySize, regname.GetChars());
	}

	for (int i = 0; i < sfunc->NumRegA; i++)
	{
		regname.Format("regA%d", i);
		regA[i] = irfunc->createAlloca(type, arraySize, regname.GetChars());
	}
}

void JitCompiler::EmitNullPointerThrow(int index, EVMAbortException reason)
{
	auto continuebb = irfunc->createBasicBlock({});
	auto exceptionbb = EmitThrowExceptionLabel(reason);
	cc.CreateCondBr(cc.CreateICmpEQ(LoadA(index), ConstValueA(nullptr)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
}

void JitCompiler::EmitThrowException(EVMAbortException reason)
{
	cc.CreateCall(GetNativeFunc<void, int>("__ThrowException", &JitCompiler::ThrowException), { ConstValueD(reason) });
}

void JitCompiler::ThrowException(int reason)
{
	ThrowAbortException((EVMAbortException)reason, nullptr);
}

IRBasicBlock* JitCompiler::EmitThrowExceptionLabel(EVMAbortException reason)
{
	auto bb = irfunc->createBasicBlock({});
	auto cursor = cc.GetInsertBlock();
	cc.SetInsertPoint(bb);
	//JitLineInfo info;
	//info.Label = bb;
	//info.LineNumber = sfunc->PCToLine(pc);
	//LineInfo.Push(info);
	EmitThrowException(reason);
	cc.SetInsertPoint(cursor);
	return bb;
}

void JitCompiler::EmitNOP()
{
	// The IR doesn't have a NOP instruction
}
