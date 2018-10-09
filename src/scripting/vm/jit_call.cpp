
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
		stackPtr = newTempIntPtr();
		cc.mov(stackPtr, frameD);
		cc.add(stackPtr, (int)(C * sizeof(int32_t)));
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), stackPtr);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_INT | REGT_KONST:
		cc.mov(x86::dword_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, i)), konstd[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_INT);
		break;
	case REGT_STRING:
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, sp)), regS[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_STRING);
		break;
	case REGT_STRING | REGT_ADDROF:
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), regS[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_STRING | REGT_KONST:
		tmp = newTempIntPtr();
		cc.mov(tmp, asmjit::imm_ptr(&konsts[C]));
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, sp)), tmp);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_STRING);
		break;
	case REGT_POINTER:
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), regA[C]);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_POINTER | REGT_ADDROF:
		stackPtr = newTempIntPtr();
		cc.mov(stackPtr, frameA);
		cc.add(stackPtr, (int)(C * sizeof(void*)));
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), stackPtr);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_POINTER | REGT_KONST:
		tmp = newTempIntPtr();
		cc.mov(tmp, asmjit::imm_ptr(konsta[C].v));
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
		stackPtr = newTempIntPtr();
		cc.mov(stackPtr, frameF);
		cc.add(stackPtr, (int)(C * sizeof(double)));
		cc.mov(x86::ptr(params, index * sizeof(VMValue) + offsetof(VMValue, a)), stackPtr);
		cc.mov(x86::byte_ptr(params, index * sizeof(VMValue) + offsetof(VMValue, Type)), (int)REGT_POINTER);
		break;
	case REGT_FLOAT | REGT_KONST:
		tmp = newTempIntPtr();
		tmp2 = newTempXmmSd();
		cc.mov(tmp, asmjit::imm_ptr(konstf + C));
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
	auto ptr = newTempIntPtr();
	cc.mov(ptr, asmjit::imm_ptr(konsta[A].o));
	EmitDoCall(ptr);
}

void JitCompiler::EmitTAIL()
{
	EmitDoTail(regA[A]);
}

void JitCompiler::EmitTAIL_K()
{
	auto ptr = newTempIntPtr();
	cc.mov(ptr, asmjit::imm_ptr(konsta[A].o));
	EmitDoTail(ptr);
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
		paramsptr = newTempIntPtr();
		cc.lea(paramsptr, x86::ptr(params, (int)((NumParam - B) * sizeof(VMValue))));
	}
	else
	{
		paramsptr = params;
	}

	auto result = newResultInt32();
	auto call = CreateCall<int, VMFrameStack*, VMFunction*, int, int, VMValue*, VMReturn*, JitExceptionInfo*>(&JitCompiler::DoCall);
	call->setRet(0, result);
	call->setArg(0, stack);
	call->setArg(1, ptr);
	call->setArg(2, Imm(B));
	call->setArg(3, Imm(C));
	call->setArg(4, paramsptr);
	call->setArg(5, callReturns);
	call->setArg(6, exceptInfo);

	EmitCheckForException();

	LoadInOuts(B);
	LoadReturns(pc + 1, C);

	NumParam -= B;
	ParamOpcodes.Resize(ParamOpcodes.Size() - B);
}

void JitCompiler::EmitDoTail(asmjit::X86Gp ptr)
{
	// Whereas the CALL instruction uses its third operand to specify how many return values
	// it expects, TAIL ignores its third operand and uses whatever was passed to this Exec call.

	// Note: this is not a true tail call, but then again, it isn't in the vmexec implementation either..

	using namespace asmjit;

	if (NumParam < B)
		I_FatalError("OP_TAIL parameter count does not match the number of preceding OP_PARAM instructions");

	StoreInOuts(B); // Is REGT_ADDROF even allowed for (true) tail calls?

	X86Gp paramsptr;
	if (B != NumParam)
	{
		paramsptr = newTempIntPtr();
		cc.lea(paramsptr, x86::ptr(params, (int)((NumParam - B) * sizeof(VMValue))));
	}
	else
	{
		paramsptr = params;
	}

	auto result = newResultInt32();
	auto call = CreateCall<int, VMFrameStack*, VMFunction*, int, int, VMValue*, VMReturn*, JitExceptionInfo*>(&JitCompiler::DoCall);
	call->setRet(0, result);
	call->setArg(0, stack);
	call->setArg(1, ptr);
	call->setArg(2, Imm(B));
	call->setArg(3, numret);
	call->setArg(4, paramsptr);
	call->setArg(5, ret);
	call->setArg(6, exceptInfo);

	cc.ret(result);

	NumParam -= B;
	ParamOpcodes.Resize(ParamOpcodes.Size() - B);
}

void JitCompiler::StoreInOuts(int b)
{
	using namespace asmjit;

	for (unsigned int i = ParamOpcodes.Size() - b; i < ParamOpcodes.Size(); i++)
	{
		asmjit::X86Gp stackPtr;
		auto c = ParamOpcodes[i]->c;
		switch (ParamOpcodes[i]->b)
		{
		case REGT_INT | REGT_ADDROF:
			stackPtr = newTempIntPtr();
			cc.mov(stackPtr, frameD);
			cc.add(stackPtr, (int)(c * sizeof(int32_t)));
			cc.mov(x86::dword_ptr(stackPtr), regD[c]);
			break;
		case REGT_STRING | REGT_ADDROF:
			// We don't have to do anything in this case. String values are never moved to virtual registers.
			break;
		case REGT_POINTER | REGT_ADDROF:
			stackPtr = newTempIntPtr();
			cc.mov(stackPtr, frameA);
			cc.add(stackPtr, (int)(c * sizeof(void*)));
			cc.mov(x86::ptr(stackPtr), regA[c]);
			break;
		case REGT_FLOAT | REGT_ADDROF:
			stackPtr = newTempIntPtr();
			cc.mov(stackPtr, frameF);
			cc.add(stackPtr, (int)(c * sizeof(double)));
			cc.movsd(x86::qword_ptr(stackPtr), regF[c]);

			// When passing the address to a float we don't know if the receiving function will treat it as float, vec2 or vec3.
			if ((unsigned int)c + 1 < regF.Size())
			{
				cc.add(stackPtr, (int)sizeof(double));
				cc.movsd(x86::qword_ptr(stackPtr), regF[c + 1]);
			}
			if ((unsigned int)c + 2 < regF.Size())
			{
				cc.add(stackPtr, (int)sizeof(double));
				cc.movsd(x86::qword_ptr(stackPtr), regF[c + 2]);
			}
			break;
		default:
			break;
		}
	}
}

void JitCompiler::LoadInOuts(int b)
{
	for (unsigned int i = ParamOpcodes.Size() - b; i < ParamOpcodes.Size(); i++)
	{
		const VMOP &param = *ParamOpcodes[i];
		if (param.op == OP_PARAM && (param.b & REGT_ADDROF))
		{
			LoadCallResult(param, true);
		}
	}
}

void JitCompiler::LoadReturns(const VMOP *retval, int numret)
{
	for (int i = 0; i < numret; ++i)
	{
		if (retval[i].op != OP_RESULT)
			I_FatalError("Expected OP_RESULT to follow OP_CALL\n");

		LoadCallResult(retval[i], false);
	}
}

void JitCompiler::LoadCallResult(const VMOP &opdata, bool addrof)
{
	int type = opdata.b;
	int regnum = opdata.c;

	switch (type & REGT_TYPE)
	{
	case REGT_INT:
		cc.mov(regD[regnum], asmjit::x86::dword_ptr(frameD, regnum * sizeof(int32_t)));
		break;
	case REGT_FLOAT:
		cc.movsd(regF[regnum], asmjit::x86::qword_ptr(frameF, regnum * sizeof(double)));
		if (addrof)
		{
			// When passing the address to a float we don't know if the receiving function will treat it as float, vec2 or vec3.
			if ((unsigned int)regnum + 1 < regF.Size())
				cc.movsd(regF[regnum + 1], asmjit::x86::qword_ptr(frameF, (regnum + 1) * sizeof(double)));
			if ((unsigned int)regnum + 2 < regF.Size())
				cc.movsd(regF[regnum + 2], asmjit::x86::qword_ptr(frameF, (regnum + 2) * sizeof(double)));
		}
		else if (type & REGT_MULTIREG2)
		{
			cc.movsd(regF[regnum + 1], asmjit::x86::qword_ptr(frameF, (regnum + 1) * sizeof(double)));
		}
		else if (type & REGT_MULTIREG3)
		{
			cc.movsd(regF[regnum + 1], asmjit::x86::qword_ptr(frameF, (regnum + 1) * sizeof(double)));
			cc.movsd(regF[regnum + 2], asmjit::x86::qword_ptr(frameF, (regnum + 2) * sizeof(double)));
		}
		break;
	case REGT_STRING:
		// We don't have to do anything in this case. String values are never moved to virtual registers.
		break;
	case REGT_POINTER:
		cc.mov(regA[regnum], asmjit::x86::ptr(frameA, regnum * sizeof(void*)));
		break;
	default:
		I_FatalError("Unknown OP_RESULT/OP_PARAM type encountered in LoadCallResult\n");
		break;
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

		auto regPtr = newTempIntPtr();

		switch (type & REGT_TYPE)
		{
		case REGT_INT:
			cc.mov(regPtr, frameD);
			cc.add(regPtr, (int)(regnum * sizeof(int32_t)));
			break;
		case REGT_FLOAT:
			cc.mov(regPtr, frameF);
			cc.add(regPtr, (int)(regnum * sizeof(double)));
			break;
		case REGT_STRING:
			cc.mov(regPtr, frameS);
			cc.add(regPtr, (int)(regnum * sizeof(FString)));
			break;
		case REGT_POINTER:
			cc.mov(regPtr, frameA);
			cc.add(regPtr, (int)(regnum * sizeof(void*)));
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
			numret = VMExec(static_cast<VMScriptFunction *>(call), param, b, returns, c);
		}

		return numret;
	}
	catch (...)
	{
		exceptinfo->reason = X_OTHER;
		exceptinfo->cppException = std::current_exception();
		return 0;
	}
}
