
#include "jitintern.h"

void JitCompiler::EmitPARAM()
{
	using namespace asmjit;

	int index = NumParam++;
	ParamOpcodes.Push(pc);

	X86Gp stackPtr, tmp;
	X86Xmm tmp2;

	switch (B)
	{
	case REGT_NIL:
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), (int64_t)0);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_NIL);
		break;
	case REGT_INT:
		cc.mov(x86::dword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, i)), regD[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_INT);
		break;
	case REGT_INT | REGT_ADDROF:
		stackPtr = cc.newIntPtr();
		cc.mov(stackPtr, frameD);
		cc.add(stackPtr, C * sizeof(int32_t));
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), stackPtr);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_INT | REGT_KONST:
		cc.mov(x86::dword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, i)), konstd[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_INT);
		break;
	//case REGT_STRING:
	//case REGT_STRING | REGT_ADDROF:
	//case REGT_STRING | REGT_KONST:
	case REGT_POINTER:
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), regA[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_POINTER | REGT_ADDROF:
		stackPtr = cc.newIntPtr();
		cc.mov(stackPtr, frameA);
		cc.add(stackPtr, C * sizeof(void*));
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), stackPtr);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_POINTER | REGT_KONST:
		tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(konsta[C].v));
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), tmp);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_FLOAT:
		cc.movsd(x86::qword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, f)), regF[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_FLOAT);
		break;
	case REGT_FLOAT | REGT_MULTIREG2:
		cc.movsd(x86::qword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, f)), regF[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_FLOAT);
		index = NumParam++;
		ParamOpcodes.Push(pc);
		cc.movsd(x86::qword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, f)), regF[C + 1]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_FLOAT);
		break;
	case REGT_FLOAT | REGT_MULTIREG3:
		cc.movsd(x86::qword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, f)), regF[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_FLOAT);
		index = NumParam++;
		ParamOpcodes.Push(pc);
		cc.movsd(x86::qword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, f)), regF[C + 1]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_FLOAT);
		index = NumParam++;
		ParamOpcodes.Push(pc);
		cc.movsd(x86::qword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, f)), regF[C + 2]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_FLOAT);
		break;
	case REGT_FLOAT | REGT_ADDROF:
		stackPtr = cc.newIntPtr();
		cc.mov(stackPtr, frameF);
		cc.add(stackPtr, C * sizeof(double));
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), stackPtr);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_FLOAT | REGT_KONST:
		tmp = cc.newIntPtr();
		tmp2 = cc.newXmmSd();
		cc.mov(tmp, ToMemAddress(konstf + C));
		cc.movsd(tmp2, asmjit::x86::qword_ptr(tmp));
		cc.movsd(x86::qword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, f)), tmp2);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_FLOAT);
		break;
	default:
		I_FatalError("Unknown REGT value passed to EmitPARAM\n");
		break;
	}

}

void JitCompiler::EmitPARAMI()
{
	int index = NumParam++;
	ParamOpcodes.Push(pc);
	cc.mov(asmjit::x86::dword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, i)), (int)ABCs);
	cc.mov(asmjit::x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_INT);
}

void JitCompiler::EmitRESULT()
{
	// This instruction is just a placeholder to indicate where a return
	// value should be stored. It does nothing on its own and should not
	// be executed.
}

void JitCompiler::EmitCALL()
{
	EmitDoCall(regA[A]);
}

void JitCompiler::EmitCALL_K()
{
	auto ptr = cc.newIntPtr();
	cc.mov(ptr, ToMemAddress(konsta[A].o));
	EmitDoCall(ptr);
}

void JitCompiler::EmitTAIL()
{
	I_FatalError("EmitTAIL not implemented\n");
}

void JitCompiler::EmitTAIL_K()
{
	I_FatalError("EmitTAIL_K not implemented\n");
}

void JitCompiler::EmitDoCall(asmjit::X86Gp ptr)
{
	using namespace asmjit;

	if (NumParam < B)
		I_FatalError("OP_CALL parameter count does not match the number of preceding OP_PARAM instructions");

	StoreInOuts(B);
	FillReturns(pc + 1, C);

	X86Gp paramsptr;
	if (B != NumParam)
	{
		paramsptr = cc.newIntPtr();
		cc.mov(paramsptr, params);
		cc.add(paramsptr, (NumParam - B) * sizeof(VMValue));
	}
	else
	{
		paramsptr = params;
	}

	auto result = cc.newInt32();
	auto call = cc.call(ToMemAddress(&JitCompiler::DoCall), FuncSignature7<int, void*, void*, int, int, void*, void*, void*>());
	call->setRet(0, result);
	call->setArg(0, stack);
	call->setArg(1, ptr);
	call->setArg(2, asmjit::Imm(B));
	call->setArg(3, asmjit::Imm(C));
	call->setArg(4, paramsptr);
	call->setArg(5, callReturns);
	call->setArg(6, exceptInfo);

	auto noexception = cc.newLabel();
	auto exceptResult = cc.newInt32();
	cc.mov(exceptResult, x86::dword_ptr(exceptInfo, 0 * 4));
	cc.cmp(exceptResult, (int)-1);
	cc.je(noexception);
	X86Gp vReg = cc.newInt32();
	cc.mov(vReg, 0);
	cc.ret(vReg);
	cc.bind(noexception);

	LoadReturns(pc - B, B, true);
	LoadReturns(pc + 1, C, false);

	NumParam -= B;
	ParamOpcodes.Resize(ParamOpcodes.Size() - B);
}

void JitCompiler::StoreInOuts(int b)
{
	using namespace asmjit;

	for (unsigned int i = ParamOpcodes.Size() - b; i < ParamOpcodes.Size(); i++)
	{
		asmjit::X86Gp stackPtr;
		switch (ParamOpcodes[i]->b)
		{
		case REGT_INT | REGT_ADDROF:
			stackPtr = cc.newIntPtr();
			cc.mov(stackPtr, frameD);
			cc.add(stackPtr, C * sizeof(int32_t));
			cc.mov(x86::dword_ptr(stackPtr), regD[C]);
			break;
		//case REGT_STRING | REGT_ADDROF:
		//	break;
		case REGT_POINTER | REGT_ADDROF:
			stackPtr = cc.newIntPtr();
			cc.mov(stackPtr, frameA);
			cc.add(stackPtr, C * sizeof(void*));
			cc.mov(x86::ptr(stackPtr), regA[C]);
			break;
		case REGT_FLOAT | REGT_ADDROF:
			stackPtr = cc.newIntPtr();
			cc.mov(stackPtr, frameF);
			cc.add(stackPtr, C * sizeof(double));
			cc.movsd(x86::qword_ptr(stackPtr), regF[C]);
			break;
		default:
			break;
		}
	}
}

void JitCompiler::LoadReturns(const VMOP *retval, int numret, bool inout)
{
	for (int i = 0; i < numret; ++i)
	{
		if (!inout && retval[i].op != OP_RESULT)
			I_FatalError("Expected OP_RESULT to follow OP_CALL\n");
		else if (inout && retval[i].op != OP_PARAMI)
			continue;
		else if (inout && retval[i].op != OP_PARAM)
			I_FatalError("Expected OP_PARAM to precede OP_CALL\n");

		int type = retval[i].b;
		int regnum = retval[i].c;

		if (inout && !(type & REGT_ADDROF))
			continue;

		switch (type & REGT_TYPE)
		{
		case REGT_INT:
			cc.mov(regD[regnum], asmjit::x86::dword_ptr(frameD, regnum * sizeof(int32_t)));
			break;
		case REGT_FLOAT:
			cc.movsd(regF[regnum], asmjit::x86::qword_ptr(frameF, regnum * sizeof(double)));
			break;
		/*case REGT_STRING:
			break;*/
		case REGT_POINTER:
			cc.mov(regA[regnum], asmjit::x86::ptr(frameA, regnum * sizeof(void*)));
			break;
		default:
			I_FatalError("Unknown OP_RESULT type encountered in LoadReturns\n");
			break;
		}
	}
}

void JitCompiler::FillReturns(const VMOP *retval, int numret)
{
	using namespace asmjit;

	for (int i = 0; i < numret; ++i)
	{
		if (retval[i].op != OP_RESULT)
		{
			I_FatalError("Expected OP_RESULT to follow OP_CALL\n");
		}

		int type = retval[i].b;
		int regnum = retval[i].c;

		if (type & REGT_KONST)
		{
			I_FatalError("OP_RESULT with REGT_KONST is not allowed\n");
		}

		auto regPtr = cc.newIntPtr();

		switch (type & REGT_TYPE)
		{
		case REGT_INT:
			cc.mov(regPtr, frameD);
			cc.add(regPtr, regnum * sizeof(int32_t));
			break;
		case REGT_FLOAT:
			cc.mov(regPtr, frameF);
			cc.add(regPtr, regnum * sizeof(double));
			break;
		/*case REGT_STRING:
			cc.mov(regPtr, frameS);
			cc.add(regPtr, regnum * sizeof(FString));
			break;*/
		case REGT_POINTER:
			cc.mov(regPtr, frameA);
			cc.add(regPtr, regnum * sizeof(void*));
			break;
		default:
			I_FatalError("Unknown OP_RESULT type encountered in FillReturns\n");
			break;
		}

		cc.mov(x86::ptr(callReturns, i * sizeof(VMReturn) + offsetof(VMReturn, Location)), regPtr);
		cc.mov(x86::byte_ptr(callReturns, i * sizeof(VMReturn) + offsetof(VMReturn, RegType)), type);
	}
}

int JitCompiler::DoCall(VMFrameStack *stack, VMFunction *call, int b, int c, VMValue *param, VMReturn *returns, JitExceptionInfo *exceptinfo)
{
	try
	{
		int numret;
		if (call->VarFlags & VARF_Native)
		{
			try
			{
				VMCycles[0].Unclock();
				numret = static_cast<VMNativeFunction *>(call)->NativeCall(param, call->DefaultArgs, b, returns, c);
				VMCycles[0].Clock();
			}
			catch (CVMAbortException &err)
			{
				err.MaybePrintMessage();
				err.stacktrace.AppendFormat("Called from %s\n", call->PrintableName.GetChars());
				throw;
			}
		}
		else
		{
			VMCalls[0]++;
			VMScriptFunction *script = static_cast<VMScriptFunction *>(call);
			VMFrame *newf = stack->AllocFrame(script);
			VMFillParams(param, newf, b);
			try
			{
				numret = VMExec(stack, script->Code, returns, c);
			}
			catch (...)
			{
				stack->PopFrame();
				throw;
			}
			stack->PopFrame();
		}

		return numret;
	}
	catch (...)
	{
		// To do: store full exception in exceptinfo
		exceptinfo->reason = X_OTHER;
		return 0;
	}
}
