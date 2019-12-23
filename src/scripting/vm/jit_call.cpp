
#include "jitintern.h"
#include <map>
#include <memory>

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

void JitCompiler::EmitVtbl(const VMOP *op)
{
	int a = op->a;
	int b = op->b;
	int c = op->c;

	auto label = EmitThrowExceptionLabel(X_READ_NIL);
	cc.test(regA[b], regA[b]);
	cc.jz(label);

	cc.mov(regA[a], asmjit::x86::qword_ptr(regA[b], myoffsetof(DObject, Class)));
	cc.mov(regA[a], asmjit::x86::qword_ptr(regA[a], myoffsetof(PClass, Virtuals) + myoffsetof(FArray, Array)));
	cc.mov(regA[a], asmjit::x86::qword_ptr(regA[a], c * (int)sizeof(void*)));
}

void JitCompiler::EmitCALL()
{
	EmitVMCall(regA[A], nullptr);
	pc += C; // Skip RESULTs
}

void JitCompiler::EmitCALL_K()
{
	VMFunction *target = static_cast<VMFunction*>(konsta[A].v);

	VMNativeFunction *ntarget = nullptr;
	if (target && (target->VarFlags & VARF_Native))
		ntarget = static_cast<VMNativeFunction *>(target);

	if (ntarget && ntarget->DirectNativeCall)
	{
		EmitNativeCall(ntarget);
	}
	else
	{
		auto ptr = newTempIntPtr();
		cc.mov(ptr, asmjit::imm_ptr(target));
		EmitVMCall(ptr, target);
	}

	pc += C; // Skip RESULTs
}

void JitCompiler::EmitVMCall(asmjit::X86Gp vmfunc, VMFunction *target)
{
	using namespace asmjit;

	CheckVMFrame();

	int numparams = StoreCallParams();
	if (numparams != B)
		I_Error("OP_CALL parameter count does not match the number of preceding OP_PARAM instructions");

	if (pc > sfunc->Code && (pc - 1)->op == OP_VTBL)
		EmitVtbl(pc - 1);

	FillReturns(pc + 1, C);

	X86Gp paramsptr = newTempIntPtr();
	cc.lea(paramsptr, x86::ptr(vmframe, offsetParams));

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
	call->setInlineComment(target ? target->PrintableName.GetChars() : "VMCall");

	LoadInOuts();
	LoadReturns(pc + 1, C);

	ParamOpcodes.Clear();
}

int JitCompiler::StoreCallParams()
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
			continue;
		}

		int bc = ParamOpcodes[i]->i16u;

		switch (ParamOpcodes[i]->a)
		{
		case REGT_NIL:
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), (int64_t)0);
			break;
		case REGT_INT:
			cc.mov(x86::dword_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, i)), regD[bc]);
			break;
		case REGT_INT | REGT_ADDROF:
			cc.lea(stackPtr, x86::ptr(vmframe, offsetD + (int)(bc * sizeof(int32_t))));
			cc.mov(x86::dword_ptr(stackPtr), regD[bc]);
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), stackPtr);
			break;
		case REGT_INT | REGT_KONST:
			cc.mov(x86::dword_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, i)), konstd[bc]);
			break;
		case REGT_STRING:
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, sp)), regS[bc]);
			break;
		case REGT_STRING | REGT_ADDROF:
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), regS[bc]);
			break;
		case REGT_STRING | REGT_KONST:
			cc.mov(tmp, asmjit::imm_ptr(&konsts[bc]));
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, sp)), tmp);
			break;
		case REGT_POINTER:
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), regA[bc]);
			break;
		case REGT_POINTER | REGT_ADDROF:
			cc.lea(stackPtr, x86::ptr(vmframe, offsetA + (int)(bc * sizeof(void*))));
			cc.mov(x86::ptr(stackPtr), regA[bc]);
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), stackPtr);
			break;
		case REGT_POINTER | REGT_KONST:
			cc.mov(tmp, asmjit::imm_ptr(konsta[bc].v));
			cc.mov(x86::ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)), tmp);
			break;
		case REGT_FLOAT:
			cc.movsd(x86::qword_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, f)), regF[bc]);
			break;
		case REGT_FLOAT | REGT_MULTIREG2:
			for (int j = 0; j < 2; j++)
			{
				cc.movsd(x86::qword_ptr(vmframe, offsetParams + (slot + j) * sizeof(VMValue) + myoffsetof(VMValue, f)), regF[bc + j]);
			}
			numparams++;
			break;
		case REGT_FLOAT | REGT_MULTIREG3:
			for (int j = 0; j < 3; j++)
			{
				cc.movsd(x86::qword_ptr(vmframe, offsetParams + (slot + j) * sizeof(VMValue) + myoffsetof(VMValue, f)), regF[bc + j]);
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
			break;
		case REGT_FLOAT | REGT_KONST:
			cc.mov(tmp, asmjit::imm_ptr(konstf + bc));
			cc.movsd(tmp2, asmjit::x86::qword_ptr(tmp));
			cc.movsd(x86::qword_ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, f)), tmp2);
			break;

		default:
			I_Error("Unknown REGT value passed to EmitPARAM\n");
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
			I_Error("Expected OP_RESULT to follow OP_CALL\n");

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
		I_Error("Unknown OP_RESULT/OP_PARAM type encountered in LoadCallResult\n");
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
			I_Error("Expected OP_RESULT to follow OP_CALL\n");
		}

		int type = retval[i].b;
		int regnum = retval[i].c;

		if (type & REGT_KONST)
		{
			I_Error("OP_RESULT with REGT_KONST is not allowed\n");
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
			I_Error("Unknown OP_RESULT type encountered in FillReturns\n");
			break;
		}

		cc.mov(x86::ptr(GetCallReturns(), i * sizeof(VMReturn) + myoffsetof(VMReturn, Location)), regPtr);
		cc.mov(x86::byte_ptr(GetCallReturns(), i * sizeof(VMReturn) + myoffsetof(VMReturn, RegType)), type);
	}
}

void JitCompiler::EmitNativeCall(VMNativeFunction *target)
{
	using namespace asmjit;

	if (pc > sfunc->Code && (pc - 1)->op == OP_VTBL)
	{
		I_Error("Native direct member function calls not implemented\n");
	}

	if (target->ImplicitArgs > 0)
	{
		auto label = EmitThrowExceptionLabel(X_READ_NIL);

		assert(ParamOpcodes.Size() > 0);
		const VMOP *param = ParamOpcodes[0];
		const int bc = param->i16u;
		asmjit::X86Gp *reg = nullptr;

		switch (param->a & REGT_TYPE)
		{
		case REGT_STRING:  reg = &regS[bc]; break;
		case REGT_POINTER: reg = &regA[bc]; break;
		default:
			I_Error("Unexpected register type for self pointer\n");
			break;
		}
		
		cc.test(*reg, *reg);
		cc.jz(label);
	}

	asmjit::CBNode *cursorBefore = cc.getCursor();
	auto call = cc.call(imm_ptr(target->DirectNativeCall), CreateFuncSignature());
	call->setInlineComment(target->PrintableName.GetChars());
	asmjit::CBNode *cursorAfter = cc.getCursor();
	cc.setCursor(cursorBefore);

	X86Gp tmp;
	X86Xmm tmp2;

	int numparams = 0;
	for (unsigned int i = 0; i < ParamOpcodes.Size(); i++)
	{
		int slot = numparams++;

		if (ParamOpcodes[i]->op == OP_PARAMI)
		{
			int abcs = ParamOpcodes[i]->i24;
			call->setArg(slot, imm(abcs));
		}
		else // OP_PARAM
		{
			int bc = ParamOpcodes[i]->i16u;
			switch (ParamOpcodes[i]->a)
			{
			case REGT_NIL:
				call->setArg(slot, imm(0));
				break;
			case REGT_INT:
				call->setArg(slot, regD[bc]);
				break;
			case REGT_INT | REGT_KONST:
				call->setArg(slot, imm(konstd[bc]));
				break;
			case REGT_STRING | REGT_ADDROF:	// AddrOf string is essentially the same - a reference to the register, just not constant on the receiving side.
			case REGT_STRING:
				call->setArg(slot, regS[bc]);
				break;
			case REGT_STRING | REGT_KONST:
				tmp = newTempIntPtr();
				cc.mov(tmp, imm_ptr(&konsts[bc]));
				call->setArg(slot, tmp);
				break;
			case REGT_POINTER:
				call->setArg(slot, regA[bc]);
				break;
			case REGT_POINTER | REGT_KONST:
				tmp = newTempIntPtr();
				cc.mov(tmp, imm_ptr(konsta[bc].v));
				call->setArg(slot, tmp);
				break;
			case REGT_FLOAT:
				call->setArg(slot, regF[bc]);
				break;
			case REGT_FLOAT | REGT_MULTIREG2:
				for (int j = 0; j < 2; j++)
					call->setArg(slot + j, regF[bc + j]);
				numparams++;
				break;
			case REGT_FLOAT | REGT_MULTIREG3:
				for (int j = 0; j < 3; j++)
					call->setArg(slot + j, regF[bc + j]);
				numparams += 2;
				break;
			case REGT_FLOAT | REGT_KONST:
				tmp = newTempIntPtr();
				tmp2 = newTempXmmSd();
				cc.mov(tmp, asmjit::imm_ptr(konstf + bc));
				cc.movsd(tmp2, asmjit::x86::qword_ptr(tmp));
				call->setArg(slot, tmp2);
				break;

			case REGT_INT | REGT_ADDROF:
			case REGT_POINTER | REGT_ADDROF:
			case REGT_FLOAT | REGT_ADDROF:
				I_Error("REGT_ADDROF not implemented for native direct calls\n");
				break;

			default:
				I_Error("Unknown REGT value passed to EmitPARAM\n");
				break;
			}
		}
	}

	if (numparams != B)
		I_Error("OP_CALL parameter count does not match the number of preceding OP_PARAM instructions\n");

	// Note: the usage of newResultXX is intentional. Asmjit has a register allocation bug
	// if the return virtual register is already allocated in an argument slot.

	const VMOP *retval = pc + 1;
	int numret = C;

	// Check if first return value was placed in the function's real return value slot
	int startret = 1;
	if (numret > 0)
	{
		int type = retval[0].b;
		switch (type)
		{
		case REGT_INT:
		case REGT_FLOAT:
		case REGT_POINTER:
			break;
		default:
			startret = 0;
			break;
		}
	}

	// Pass return pointers as arguments
	for (int i = startret; i < numret; ++i)
	{
		int type = retval[i].b;
		int regnum = retval[i].c;

		if (type & REGT_KONST)
		{
			I_Error("OP_RESULT with REGT_KONST is not allowed\n");
		}

		CheckVMFrame();

		if ((type & REGT_TYPE) == REGT_STRING)
		{
			// For strings we already have them on the stack and got named registers for them.
			call->setArg(numparams + i - startret, regS[regnum]);
		}
		else
		{
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
				I_Error("Unknown OP_RESULT type encountered\n");
				break;
			}

			call->setArg(numparams + i - startret, regPtr);
		}
	}

	cc.setCursor(cursorAfter);

	if (startret == 1 && numret > 0)
	{
		int type = retval[0].b;
		int regnum = retval[0].c;

		switch (type)
		{
		case REGT_INT:
			tmp = newResultInt32();
			call->setRet(0, tmp);
			cc.mov(regD[regnum], tmp);
			break;
		case REGT_FLOAT:
			tmp2 = newResultXmmSd();
			call->setRet(0, tmp2);
			cc.movsd(regF[regnum], tmp2);
			break;
		case REGT_POINTER:
			tmp = newResultIntPtr();
			call->setRet(0, tmp);
			cc.mov(regA[regnum], tmp);
			break;
		}
	}

	// Move the result into virtual registers
	for (int i = startret; i < numret; ++i)
	{
		int type = retval[i].b;
		int regnum = retval[i].c;

		switch (type)
		{
		case REGT_INT:
			cc.mov(regD[regnum], asmjit::x86::dword_ptr(vmframe, offsetD + regnum * sizeof(int32_t)));
			break;
		case REGT_FLOAT:
			cc.movsd(regF[regnum], asmjit::x86::qword_ptr(vmframe, offsetF + regnum * sizeof(double)));
			break;
		case REGT_FLOAT | REGT_MULTIREG2:
			cc.movsd(regF[regnum], asmjit::x86::qword_ptr(vmframe, offsetF + regnum * sizeof(double)));
			cc.movsd(regF[regnum + 1], asmjit::x86::qword_ptr(vmframe, offsetF + (regnum + 1) * sizeof(double)));
			break;
		case REGT_FLOAT | REGT_MULTIREG3:
			cc.movsd(regF[regnum], asmjit::x86::qword_ptr(vmframe, offsetF + regnum * sizeof(double)));
			cc.movsd(regF[regnum + 1], asmjit::x86::qword_ptr(vmframe, offsetF + (regnum + 1) * sizeof(double)));
			cc.movsd(regF[regnum + 2], asmjit::x86::qword_ptr(vmframe, offsetF + (regnum + 2) * sizeof(double)));
			break;
		case REGT_STRING:
			// We don't have to do anything in this case. String values are never moved to virtual registers.
			break;
		case REGT_POINTER:
			cc.mov(regA[regnum], asmjit::x86::ptr(vmframe, offsetA + regnum * sizeof(void*)));
			break;
		default:
			I_Error("Unknown OP_RESULT type encountered\n");
			break;
		}
	}

	ParamOpcodes.Clear();
}

static std::map<FString, std::unique_ptr<TArray<uint8_t>>> argsCache;

asmjit::FuncSignature JitCompiler::CreateFuncSignature()
{
	using namespace asmjit;

	TArray<uint8_t> args;
	FString key;

	// First add parameters as args to the signature

	for (unsigned int i = 0; i < ParamOpcodes.Size(); i++)
	{
		if (ParamOpcodes[i]->op == OP_PARAMI)
		{
			args.Push(TypeIdOf<int>::kTypeId);
			key += "i";
		}
		else // OP_PARAM
		{
			int bc = ParamOpcodes[i]->i16u;
			switch (ParamOpcodes[i]->a)
			{
			case REGT_NIL:
			case REGT_POINTER:
			case REGT_POINTER | REGT_KONST:
			case REGT_STRING | REGT_ADDROF:
			case REGT_INT | REGT_ADDROF:
			case REGT_POINTER | REGT_ADDROF:
			case REGT_FLOAT | REGT_ADDROF:
				args.Push(TypeIdOf<void*>::kTypeId);
				key += "v";
				break;
			case REGT_INT:
			case REGT_INT | REGT_KONST:
				args.Push(TypeIdOf<int>::kTypeId);
				key += "i";
				break;
			case REGT_STRING:
			case REGT_STRING | REGT_KONST:
				args.Push(TypeIdOf<void*>::kTypeId);
				key += "s";
				break;
			case REGT_FLOAT:
			case REGT_FLOAT | REGT_KONST:
				args.Push(TypeIdOf<double>::kTypeId);
				key += "f";
				break;
			case REGT_FLOAT | REGT_MULTIREG2:
				args.Push(TypeIdOf<double>::kTypeId);
				args.Push(TypeIdOf<double>::kTypeId);
				key += "ff";
				break;
			case REGT_FLOAT | REGT_MULTIREG3:
				args.Push(TypeIdOf<double>::kTypeId);
				args.Push(TypeIdOf<double>::kTypeId);
				args.Push(TypeIdOf<double>::kTypeId);
				key += "fff";
				break;

			default:
				I_Error("Unknown REGT value passed to EmitPARAM\n");
				break;
			}
		}
	}

	const VMOP *retval = pc + 1;
	int numret = C;

	uint32_t rettype = TypeIdOf<void>::kTypeId;

	// Check if first return value can be placed in the function's real return value slot
	int startret = 1;
	if (numret > 0)
	{
		if (retval[0].op != OP_RESULT)
		{
			I_Error("Expected OP_RESULT to follow OP_CALL\n");
		}

		int type = retval[0].b;
		switch (type)
		{
		case REGT_INT:
			rettype = TypeIdOf<int>::kTypeId;
			key += "ri";
			break;
		case REGT_FLOAT:
			rettype = TypeIdOf<double>::kTypeId;
			key += "rf";
			break;
		case REGT_POINTER:
			rettype = TypeIdOf<void*>::kTypeId;
			key += "rv";
			break;
		case REGT_STRING:
		default:
			startret = 0;
			break;
		}
	}

	// Add any additional return values as function arguments
	for (int i = startret; i < numret; ++i)
	{
		if (retval[i].op != OP_RESULT)
		{
			I_Error("Expected OP_RESULT to follow OP_CALL\n");
		}

		args.Push(TypeIdOf<void*>::kTypeId);
		key += "v";
	}

	// FuncSignature only keeps a pointer to its args array. Store a copy of each args array variant.
	std::unique_ptr<TArray<uint8_t>> &cachedArgs = argsCache[key];
	if (!cachedArgs) cachedArgs.reset(new TArray<uint8_t>(args));

	FuncSignature signature;
	signature.init(CallConv::kIdHost, rettype, cachedArgs->Data(), cachedArgs->Size());
	return signature;
}
