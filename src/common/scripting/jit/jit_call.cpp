
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

	auto exceptionbb = EmitThrowExceptionLabel(X_READ_NIL);
	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpNE(LoadA(b), ConstValueA(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);

	IRValue* ptrObject = LoadA(b);
	IRValue* ptrClass = Load(ToPtrPtr(ptrObject, ConstValueD(myoffsetof(DObject, Class))));
	IRValue* ptrArray = Load(ToPtrPtr(ptrClass, ConstValueD(myoffsetof(PClass, Virtuals) + myoffsetof(FArray, Array))));
	IRValue* ptrFunc = Load(ToPtrPtr(ptrArray, ConstValueD(c * (int)sizeof(void*))));
	StoreA(ptrFunc, a);
}

void JitCompiler::EmitCALL()
{
	EmitVMCall(LoadA(A), nullptr);
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
		EmitVMCall(ConstValueA(target), target);
	}

	pc += C; // Skip RESULTs
}

void JitCompiler::EmitVMCall(IRValue* vmfunc, VMFunction* target)
{
	CheckVMFrame();

	int numparams = StoreCallParams();
	if (numparams != B)
		I_Error("OP_CALL parameter count does not match the number of preceding OP_PARAM instructions");

	if (pc > sfunc->Code && (pc - 1)->op == OP_VTBL)
		EmitVtbl(pc - 1);

	FillReturns(pc + 1, C);

	IRValue* paramsptr = OffsetPtr(vmframe, offsetParams);
	IRValue* scriptcall = OffsetPtr(vmfunc, myoffsetof(VMScriptFunction, ScriptCall));

	/*IRValue* call =*/ cc.CreateCall(cc.CreateBitCast(scriptcall, GetFunctionType5<int, VMFunction*, VMValue*, int, VMReturn*, int>()), { vmfunc, paramsptr, ConstValueD(B), GetCallReturns(), ConstValueD(C) });
	//call->setComment(target ? target->PrintableName.GetChars() : "VMCall");

	LoadInOuts();
	LoadReturns(pc + 1, C);

	ParamOpcodes.Clear();
}

int JitCompiler::StoreCallParams()
{
	IRValue* stackPtr;
	int numparams = 0;
	for (unsigned int i = 0; i < ParamOpcodes.Size(); i++)
	{
		int slot = numparams++;

		if (ParamOpcodes[i]->op == OP_PARAMI)
		{
			int abcs = ParamOpcodes[i]->i24;
			Store32(ConstValueD(abcs), ToInt32Ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, i)));
			continue;
		}

		int bc = ParamOpcodes[i]->i16u;

		switch (ParamOpcodes[i]->a)
		{
		case REGT_NIL:
			StorePtr(ConstValueA(nullptr), OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)));
			break;
		case REGT_INT:
			Store32(LoadD(bc), ToInt32Ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, i)));
			break;
		case REGT_INT | REGT_ADDROF:
			stackPtr = OffsetPtr(vmframe, offsetD + (int)(bc * sizeof(int32_t)));
			Store32(LoadD(bc), ToInt32Ptr(stackPtr));
			StorePtr(stackPtr, OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)));
			break;
		case REGT_INT | REGT_KONST:
			Store32(ConstD(bc), ToInt32Ptr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, i)));
			break;
		case REGT_STRING:
			StorePtr(LoadS(bc), OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, sp)));
			break;
		case REGT_STRING | REGT_ADDROF:
			StorePtr(LoadS(bc), OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)));
			break;
		case REGT_STRING | REGT_KONST:
			StorePtr(ConstS(bc), OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, sp)));
			break;
		case REGT_POINTER:
			StorePtr(LoadA(bc), OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)));
			break;
		case REGT_POINTER | REGT_ADDROF:
			stackPtr = OffsetPtr(vmframe, offsetA + (int)(bc * sizeof(void*)));
			StorePtr(LoadA(bc), stackPtr);
			StorePtr(stackPtr, OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)));
			break;
		case REGT_POINTER | REGT_KONST:
			StorePtr(ConstA(bc), OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)));
			break;
		case REGT_FLOAT:
			StoreDouble(LoadF(bc), OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, f)));
			break;
		case REGT_FLOAT | REGT_MULTIREG2:
			for (int j = 0; j < 2; j++)
			{
				StoreDouble(LoadF(bc + j), ToDoublePtr(vmframe, offsetParams + (slot + j) * sizeof(VMValue) + myoffsetof(VMValue, f)));
			}
			numparams++;
			break;
		case REGT_FLOAT | REGT_MULTIREG3:
			for (int j = 0; j < 3; j++)
			{
				StoreDouble(LoadF(bc + j), ToDoublePtr(vmframe, offsetParams + (slot + j) * sizeof(VMValue) + myoffsetof(VMValue, f)));
			}
			numparams += 2;
			break;
		case REGT_FLOAT | REGT_ADDROF:
			stackPtr = OffsetPtr(vmframe, offsetF + (int)(bc * sizeof(double)));
			// When passing the address to a float we don't know if the receiving function will treat it as float, vec2 or vec3.
			for (int j = 0; j < 3; j++)
			{
				if ((unsigned int)(bc + j) < regF.Size())
					StoreDouble(LoadF(bc + j), ToDoublePtr(stackPtr, j * sizeof(double)));
			}
			StorePtr(stackPtr, OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, a)));
			break;
		case REGT_FLOAT | REGT_KONST:
			StoreDouble(ConstF(bc), OffsetPtr(vmframe, offsetParams + slot * sizeof(VMValue) + myoffsetof(VMValue, f)));
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
		StoreD(Load(ToInt32Ptr(vmframe, offsetD + regnum * sizeof(int32_t))), regnum);
		break;
	case REGT_FLOAT:
		StoreF(Load(ToDoublePtr(vmframe, offsetF + regnum * sizeof(double))), regnum);
		if (addrof)
		{
			// When passing the address to a float we don't know if the receiving function will treat it as float, vec2 or vec3.
			if ((unsigned int)regnum + 1 < regF.Size())
				StoreF(Load(ToDoublePtr(vmframe, offsetF + (regnum + 1) * sizeof(double))), regnum + 1);
			if ((unsigned int)regnum + 2 < regF.Size())
				StoreF(Load(ToDoublePtr(vmframe, offsetF + (regnum + 2) * sizeof(double))), regnum + 2);
		}
		else if (type & REGT_MULTIREG2)
		{
			StoreF(Load(ToDoublePtr(vmframe, offsetF + (regnum + 1) * sizeof(double))), regnum + 1);
		}
		else if (type & REGT_MULTIREG3)
		{
			StoreF(Load(ToDoublePtr(vmframe, offsetF + (regnum + 1) * sizeof(double))), regnum + 1);
			StoreF(Load(ToDoublePtr(vmframe, offsetF + (regnum + 2) * sizeof(double))), regnum + 2);
		}
		break;
	case REGT_STRING:
		// We don't have to do anything in this case. String values are never moved to virtual registers.
		break;
	case REGT_POINTER:
		StoreA(Load(ToPtrPtr(vmframe, offsetA + regnum * sizeof(void*))), regnum);
		break;
	default:
		I_Error("Unknown OP_RESULT/OP_PARAM type encountered in LoadCallResult\n");
		break;
	}
}

void JitCompiler::FillReturns(const VMOP *retval, int numret)
{
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

		IRValue* valueptr;
		switch (type & REGT_TYPE)
		{
		case REGT_INT:
			valueptr = OffsetPtr(vmframe, offsetD + (int)(regnum * sizeof(int32_t)));
			break;
		case REGT_FLOAT:
			valueptr = OffsetPtr(vmframe, offsetF + (int)(regnum * sizeof(double)));
			break;
		case REGT_STRING:
			valueptr = OffsetPtr(vmframe, offsetS + (int)(regnum * sizeof(FString)));
			break;
		case REGT_POINTER:
			valueptr = OffsetPtr(vmframe, offsetA + (int)(regnum * sizeof(void*)));
			break;
		default:
			I_Error("Unknown OP_RESULT type encountered in FillReturns\n");
			break;
		}

		StorePtr(valueptr, ToPtrPtr(GetCallReturns(), i * sizeof(VMReturn) + myoffsetof(VMReturn, Location)));
		Store8(ConstValueD(type), ToPtrPtr(GetCallReturns(), i * sizeof(VMReturn) + myoffsetof(VMReturn, RegType)));
	}
}

void JitCompiler::EmitNativeCall(VMNativeFunction *target)
{
	if (pc > sfunc->Code && (pc - 1)->op == OP_VTBL)
	{
		I_Error("Native direct member function calls not implemented\n");
	}

	if (target->ImplicitArgs > 0)
	{
		auto exceptionbb = EmitThrowExceptionLabel(X_READ_NIL);
		auto continuebb = irfunc->createBasicBlock({});

		assert(ParamOpcodes.Size() > 0);
		const VMOP *param = ParamOpcodes[0];
		const int bc = param->i16u;

		switch (param->a & REGT_TYPE)
		{
		case REGT_STRING:  cc.CreateCondBr(cc.CreateICmpEQ(LoadS(bc), ConstValueS(nullptr)), exceptionbb, continuebb); break;
		case REGT_POINTER: cc.CreateCondBr(cc.CreateICmpEQ(LoadA(bc), ConstValueA(nullptr)), exceptionbb, continuebb); break;
		default:
			I_Error("Unexpected register type for self pointer\n");
			break;
		}

		cc.SetInsertPoint(continuebb);
	}

	std::vector<IRValue*> args;
	for (unsigned int i = 0; i < ParamOpcodes.Size(); i++)
	{
		if (ParamOpcodes[i]->op == OP_PARAMI)
		{
			int abcs = ParamOpcodes[i]->i24;
			args.push_back(ConstValueD(abcs));
		}
		else // OP_PARAM
		{
			int bc = ParamOpcodes[i]->i16u;
			switch (ParamOpcodes[i]->a)
			{
			case REGT_NIL:
				args.push_back(ConstValueA(nullptr));
				break;
			case REGT_INT:
				args.push_back(LoadD(bc));
				break;
			case REGT_INT | REGT_KONST:
				args.push_back(ConstD(bc));
				break;
			case REGT_STRING | REGT_ADDROF:	// AddrOf string is essentially the same - a reference to the register, just not constant on the receiving side.
			case REGT_STRING:
				args.push_back(LoadS(bc));
				break;
			case REGT_STRING | REGT_KONST:
				args.push_back(ConstS(bc));
				break;
			case REGT_POINTER:
				args.push_back(LoadA(bc));
				break;
			case REGT_POINTER | REGT_KONST:
				args.push_back(ConstA(bc));
				break;
			case REGT_FLOAT:
				args.push_back(LoadF(bc));
				break;
			case REGT_FLOAT | REGT_MULTIREG2:
				args.push_back(LoadF(bc));
				args.push_back(LoadF(bc + 1));
				break;
			case REGT_FLOAT | REGT_MULTIREG3:
				args.push_back(LoadF(bc));
				args.push_back(LoadF(bc + 1));
				args.push_back(LoadF(bc + 2));
				break;
			case REGT_FLOAT | REGT_KONST:
				args.push_back(ConstF(bc));
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

	int numparams = (int)args.size();
	if (numparams != B)
		I_Error("OP_CALL parameter count does not match the number of preceding OP_PARAM instructions\n");

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
			args.push_back(LoadS(regnum));
		}
		else
		{
			switch (type & REGT_TYPE)
			{
			case REGT_INT:
				args.push_back(OffsetPtr(vmframe, offsetD + (int)(regnum * sizeof(int32_t))));
				break;
			case REGT_FLOAT:
				args.push_back(OffsetPtr(vmframe, offsetF + (int)(regnum * sizeof(double))));
				break;
			case REGT_STRING:
				args.push_back(OffsetPtr(vmframe, offsetS + (int)(regnum * sizeof(FString))));
				break;
			case REGT_POINTER:
				args.push_back(OffsetPtr(vmframe, offsetA + (int)(regnum * sizeof(void*))));
				break;
			default:
				I_Error("Unknown OP_RESULT type encountered\n");
				break;
			}
		}
	}

	IRValue* result = cc.CreateCall(cc.CreateBitCast(ConstValueA(target->DirectNativeCall), GetFuncSignature()), args);
	//result->setComment(target->PrintableName.GetChars());

	if (startret == 1 && numret > 0)
	{
		int type = retval[0].b;
		int regnum = retval[0].c;

		switch (type)
		{
		case REGT_INT:
			StoreD(result, regnum);
			break;
		case REGT_FLOAT:
			StoreF(result, regnum);
			break;
		case REGT_POINTER:
			StoreA(result, regnum);
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
			StoreD(Load(ToInt32Ptr(vmframe, offsetD + regnum * sizeof(int32_t))), regnum);
			break;
		case REGT_FLOAT:
			StoreF(Load(ToDoublePtr(vmframe, offsetF + regnum * sizeof(double))), regnum);
			break;
		case REGT_FLOAT | REGT_MULTIREG2:
			StoreF(Load(ToDoublePtr(vmframe, offsetF + regnum * sizeof(double))), regnum);
			StoreF(Load(ToDoublePtr(vmframe, offsetF + (regnum + 1) * sizeof(double))), regnum + 1);
			break;
		case REGT_FLOAT | REGT_MULTIREG3:
			StoreF(Load(ToDoublePtr(vmframe, offsetF + regnum * sizeof(double))), regnum);
			StoreF(Load(ToDoublePtr(vmframe, offsetF + (regnum + 1) * sizeof(double))), regnum + 1);
			StoreF(Load(ToDoublePtr(vmframe, offsetF + (regnum + 2) * sizeof(double))), regnum + 2);
			break;
		case REGT_STRING:
			// We don't have to do anything in this case. String values are never moved to virtual registers.
			break;
		case REGT_POINTER:
			StoreA(Load(ToPtrPtr(vmframe, offsetA + regnum * sizeof(void*))), regnum);
			break;
		default:
			I_Error("Unknown OP_RESULT type encountered\n");
			break;
		}
	}

	ParamOpcodes.Clear();
}

IRFunctionType* JitCompiler::GetFuncSignature()
{
	std::vector<IRType*> args;

	// First add parameters as args to the signature

	for (unsigned int i = 0; i < ParamOpcodes.Size(); i++)
	{
		if (ParamOpcodes[i]->op == OP_PARAMI)
		{
			args.push_back(ircontext->getInt32Ty());
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
				args.push_back(ircontext->getInt8PtrTy());
				break;
			case REGT_INT:
			case REGT_INT | REGT_KONST:
				args.push_back(ircontext->getInt32Ty());
				break;
			case REGT_STRING:
			case REGT_STRING | REGT_KONST:
				args.push_back(ircontext->getInt8PtrTy());
				break;
			case REGT_FLOAT:
			case REGT_FLOAT | REGT_KONST:
				args.push_back(ircontext->getDoublePtrTy());
				break;
			case REGT_FLOAT | REGT_MULTIREG2:
				args.push_back(ircontext->getDoublePtrTy());
				args.push_back(ircontext->getDoublePtrTy());
				break;
			case REGT_FLOAT | REGT_MULTIREG3:
				args.push_back(ircontext->getDoublePtrTy());
				args.push_back(ircontext->getDoublePtrTy());
				args.push_back(ircontext->getDoublePtrTy());
				break;

			default:
				I_Error("Unknown REGT value passed to EmitPARAM\n");
				break;
			}
		}
	}

	const VMOP *retval = pc + 1;
	int numret = C;

	IRType* rettype = ircontext->getVoidTy();

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
			rettype = ircontext->getInt32Ty();
			break;
		case REGT_FLOAT:
			rettype = ircontext->getDoublePtrTy();
			break;
		case REGT_POINTER:
			rettype = ircontext->getInt8PtrTy();
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

		args.push_back(ircontext->getInt8PtrTy());
	}

	return ircontext->getFunctionType(rettype, args);
}
