
#include "jitintern.h"

void JitCompiler::EmitPARAM()
{
	ParamOpcodes.Push(pc);
}

void JitCompiler::EmitPARAMI()
{
	ParamOpcodes.Push(pc);
}

void JitCompiler::EmitRESULT()
{
	// This instruction is just a placeholder to indicate where a return
	// value should be stored. It does nothing on its own and should not
	// be executed.
}

void JitCompiler::EmitVTBL()
{
	// This instruction is handled in the CALL/CALL_K instruction following it
}

static VMFunction *GetVirtual(DObject *o, int c)
{
	auto p = o->GetClass();
	assert(c < (int)p->Virtuals.Size());
	return p->Virtuals[c];
}

void JitCompiler::EmitVtbl(const VMOP *op)
{
	int a = op->a;
	int b = op->b;
	int c = op->c;

	auto label = EmitThrowExceptionLabel(X_READ_NIL);
	cc.test(regA[b], regA[b]);
	cc.jz(label);

	auto result = newResultIntPtr();
	auto call = CreateCall<VMFunction*, DObject*, int>(GetVirtual);
	call->setRet(0, result);
	call->setArg(0, regA[b]);
	call->setArg(1, asmjit::Imm(c));
	cc.mov(regA[a], result);
}

void JitCompiler::EmitCALL()
{
	EmitDoCall(regA[A], nullptr);
}

void JitCompiler::EmitCALL_K()
{
	auto ptr = newTempIntPtr();
	cc.mov(ptr, asmjit::imm_ptr(konsta[A].v));
	EmitDoCall(ptr, static_cast<VMFunction*>(konsta[A].v));
}

void JitCompiler::EmitDoCall(asmjit::X86Gp vmfunc, VMFunction *target)
{
	using namespace asmjit;

	bool simpleFrameTarget = false;
	if (target && (target->VarFlags & VARF_Native))
	{
		VMScriptFunction *starget = static_cast<VMScriptFunction*>(target);
		simpleFrameTarget = starget->SpecialInits.Size() == 0 && starget->NumRegS == 0;
	}

	CheckVMFrame();

	int numparams = StoreCallParams(simpleFrameTarget);
	if (numparams != B)
		I_FatalError("OP_CALL parameter count does not match the number of preceding OP_PARAM instructions");

	if ((pc - 1)->op == OP_VTBL)
		EmitVtbl(pc - 1);

	FillReturns(pc + 1, C);

	X86Gp paramsptr = newTempIntPtr();
	cc.lea(paramsptr, x86::ptr(vmframe, offsetParams));

	EmitScriptCall(vmfunc, paramsptr);

	LoadInOuts();
	LoadReturns(pc + 1, C);

	ParamOpcodes.Clear();

	pc += C; // Skip RESULTs
}

void JitCompiler::EmitScriptCall(asmjit::X86Gp vmfunc, asmjit::X86Gp paramsptr)
{
	using namespace asmjit;

	auto scriptcall = newTempIntPtr();
	cc.mov(scriptcall, x86::ptr(vmfunc, myoffsetof(VMScriptFunction, ScriptCall)));

	auto result = newResultInt32();
	auto call = cc.call(scriptcall, FuncSignature5<int, VMFunction *, VMValue*, int, VMReturn*, int>());
	call->setRet(0, result);
	call->setArg(0, vmfunc);
	call->setArg(1, paramsptr);
	call->setArg(2, Imm(B));
	call->setArg(3, GetCallReturns());
	call->setArg(4, Imm(C));
}

int JitCompiler::StoreCallParams(bool simpleFrameTarget)
{
	using namespace asmjit;

	X86Gp stackPtr = newTempIntPtr();
	X86Gp tmp = newTempIntPtr();
	X86Xmm tmp2 = newTempXmmSd();

	int numparams = 0;
	for (unsigned int i = 0; i < ParamOpcodes.Size(); i++)
	{
		int slot = numparams++;

		if (ParamOpcodes[i]->op == OP_PARAMI)
		{
			int abcs = ParamOpcodes[i]->i24;
			cc.mov(asmjit::x86::dword_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, i)), abcs);
			if (!simpleFrameTarget)
				cc.mov(asmjit::x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_INT);
			continue;
		}

		int bc = ParamOpcodes[i]->i16u;

		switch (ParamOpcodes[i]->a)
		{
		case REGT_NIL:
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), (int64_t)0);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_NIL);
			break;
		case REGT_INT:
			cc.mov(x86::dword_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, i)), regD[bc]);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_INT);
			break;
		case REGT_INT | REGT_ADDROF:
			cc.lea(stackPtr, x86::ptr(vmframe, offsetD + (int)(bc * sizeof(int32_t))));
			cc.mov(x86::dword_ptr(stackPtr), regD[bc]);
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), stackPtr);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_POINTER);
			break;
		case REGT_INT | REGT_KONST:
			cc.mov(x86::dword_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, i)), konstd[bc]);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_INT);
			break;
		case REGT_STRING:
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, sp)), regS[bc]);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_STRING);
			break;
		case REGT_STRING | REGT_ADDROF:
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), regS[bc]);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_POINTER);
			break;
		case REGT_STRING | REGT_KONST:
			cc.mov(tmp, asmjit::imm_ptr(&konsts[bc]));
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, sp)), tmp);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_STRING);
			break;
		case REGT_POINTER:
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), regA[bc]);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_POINTER);
			break;
		case REGT_POINTER | REGT_ADDROF:
			cc.lea(stackPtr, x86::ptr(vmframe, offsetA + (int)(bc * sizeof(void*))));
			cc.mov(x86::ptr(stackPtr), regA[bc]);
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), stackPtr);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_POINTER);
			break;
		case REGT_POINTER | REGT_KONST:
			cc.mov(tmp, asmjit::imm_ptr(konsta[bc].v));
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), tmp);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_POINTER);
			break;
		case REGT_FLOAT:
			cc.movsd(x86::qword_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, f)), regF[bc]);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_FLOAT);
			break;
		case REGT_FLOAT | REGT_MULTIREG2:
			for (int j = 0; j < 2; j++)
			{
				cc.movsd(x86::qword_ptr(vmframe, offsetParams + (slot + j) * sizeof(VMValue) + myoffsetof(VMValue, f)), regF[bc + j]);
				if (!simpleFrameTarget)
					cc.mov(x86::byte_ptr(vmframe, offsetParams + (slot + j) * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_FLOAT);
			}
			numparams++;
			break;
		case REGT_FLOAT | REGT_MULTIREG3:
			for (int j = 0; j < 3; j++)
			{
				cc.movsd(x86::qword_ptr(vmframe, offsetParams + (slot + j) * sizeof(VMValue) + myoffsetof(VMValue, f)), regF[bc + j]);
				if (!simpleFrameTarget)
					cc.mov(x86::byte_ptr(vmframe, offsetParams + (slot + j) * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_FLOAT);
			}
			numparams += 2;
			break;
		case REGT_FLOAT | REGT_ADDROF:
			cc.lea(stackPtr, x86::ptr(vmframe, offsetF + (int)(bc * sizeof(double))));
			// When passing the address to a float we don't know if the receiving function will treat it as float, vec2 or vec3.
			for (int j = 0; j < 3; j++)
			{
				if ((unsigned int)(bc + j) < regF.Size())
					cc.movsd(x86::qword_ptr(stackPtr, j * sizeof(double)), regF[bc + j]);
			}
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), stackPtr);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_POINTER);
			break;
		case REGT_FLOAT | REGT_KONST:
			cc.mov(tmp, asmjit::imm_ptr(konstf + bc));
			cc.movsd(tmp2, asmjit::x86::qword_ptr(tmp));
			cc.movsd(x86::qword_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, f)), tmp2);
			if (!simpleFrameTarget)
				cc.mov(x86::byte_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, Type)), (int)REGT_FLOAT);
			break;

		default:
			I_FatalError("Unknown REGT value passed to EmitPARAM\n");
			break;
		}
	}

	return numparams;
}

void JitCompiler::LoadInOuts()
{
	for (unsigned int i = 0; i < ParamOpcodes.Size(); i++)
	{
		const VMOP &param = *ParamOpcodes[i];
		if (param.op == OP_PARAM && (param.a & REGT_ADDROF))
		{
			LoadCallResult(param.a, param.i16u, true);
		}
	}
}

void JitCompiler::LoadReturns(const VMOP *retval, int numret)
{
	for (int i = 0; i < numret; ++i)
	{
		if (retval[i].op != OP_RESULT)
			I_FatalError("Expected OP_RESULT to follow OP_CALL\n");

		LoadCallResult(retval[i].b, retval[i].c, false);
	}
}

void JitCompiler::LoadCallResult(int type, int regnum, bool addrof)
{
	switch (type & REGT_TYPE)
	{
	case REGT_INT:
		cc.mov(regD[regnum], asmjit::x86::dword_ptr(vmframe, offsetD + regnum * sizeof(int32_t)));
		break;
	case REGT_FLOAT:
		cc.movsd(regF[regnum], asmjit::x86::qword_ptr(vmframe, offsetF + regnum * sizeof(double)));
		if (addrof)
		{
			// When passing the address to a float we don't know if the receiving function will treat it as float, vec2 or vec3.
			if ((unsigned int)regnum + 1 < regF.Size())
				cc.movsd(regF[regnum + 1], asmjit::x86::qword_ptr(vmframe, offsetF + (regnum + 1) * sizeof(double)));
			if ((unsigned int)regnum + 2 < regF.Size())
				cc.movsd(regF[regnum + 2], asmjit::x86::qword_ptr(vmframe, offsetF + (regnum + 2) * sizeof(double)));
		}
		else if (type & REGT_MULTIREG2)
		{
			cc.movsd(regF[regnum + 1], asmjit::x86::qword_ptr(vmframe, offsetF + (regnum + 1) * sizeof(double)));
		}
		else if (type & REGT_MULTIREG3)
		{
			cc.movsd(regF[regnum + 1], asmjit::x86::qword_ptr(vmframe, offsetF + (regnum + 1) * sizeof(double)));
			cc.movsd(regF[regnum + 2], asmjit::x86::qword_ptr(vmframe, offsetF + (regnum + 2) * sizeof(double)));
		}
		break;
	case REGT_STRING:
		// We don't have to do anything in this case. String values are never moved to virtual registers.
		break;
	case REGT_POINTER:
		cc.mov(regA[regnum], asmjit::x86::ptr(vmframe, offsetA + regnum * sizeof(void*)));
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
			cc.lea(regPtr, x86::ptr(vmframe, offsetD + (int)(regnum * sizeof(int32_t))));
			break;
		case REGT_FLOAT:
			cc.lea(regPtr, x86::ptr(vmframe, offsetF + (int)(regnum * sizeof(double))));
			break;
		case REGT_STRING:
			cc.lea(regPtr, x86::ptr(vmframe, offsetS + (int)(regnum * sizeof(FString))));
			break;
		case REGT_POINTER:
			cc.lea(regPtr, x86::ptr(vmframe, offsetA + (int)(regnum * sizeof(void*))));
			break;
		default:
			I_FatalError("Unknown OP_RESULT type encountered in FillReturns\n");
			break;
		}

		cc.mov(x86::ptr(GetCallReturns(), i * sizeof(VMReturn) + myoffsetof(VMReturn, Location)), regPtr);
		cc.mov(x86::byte_ptr(GetCallReturns(), i * sizeof(VMReturn) + myoffsetof(VMReturn, RegType)), type);
	}
}
