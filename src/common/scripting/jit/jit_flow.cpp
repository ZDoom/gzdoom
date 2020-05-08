
#include "jitintern.h"

void JitCompiler::EmitTEST()
{
	int i = (int)(ptrdiff_t)(pc - sfunc->Code);

	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(A), ConstValueD(BC)), GetLabel(i + 2), continuebb);
	cc.SetInsertPoint(continuebb);
}

void JitCompiler::EmitTESTN()
{
	int bc = BC;
	int i = (int)(ptrdiff_t)(pc - sfunc->Code);

	auto continuebb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(A), ConstValueD(-bc)), GetLabel(i + 2), continuebb);
	cc.SetInsertPoint(continuebb);
}

void JitCompiler::EmitJMP()
{
	auto dest = pc + JMPOFS(pc) + 1;
	int i = (int)(ptrdiff_t)(dest - sfunc->Code);
	cc.CreateBr(GetLabel(i));
	cc.SetInsertPoint(nullptr);
}

void JitCompiler::EmitIJMP()
{
	int base = (int)(ptrdiff_t)(pc - sfunc->Code) + 1;
	IRValue* val = LoadD(A);

	for (int i = 0; i < (int)BCs; i++)
	{
		if (sfunc->Code[base +i].op == OP_JMP)
		{
			int target = base + i + JMPOFS(&sfunc->Code[base + i]) + 1;

			IRBasicBlock* elsebb = irfunc->createBasicBlock({});
			cc.CreateCondBr(cc.CreateICmpEQ(val, ConstValueD(i)), GetLabel(target), elsebb);
			cc.SetInsertPoint(elsebb);
		}
	}
	pc += BCs;

	// This should never happen. It means we are jumping to something that is not a JMP instruction!
	EmitThrowException(X_OTHER);
}

static void ValidateCall(DObject *o, VMFunction *f, int b)
{
	FScopeBarrier::ValidateCall(o->GetClass(), f, b - 1);
}

void JitCompiler::EmitSCOPE()
{
	auto continuebb = irfunc->createBasicBlock({});
	auto exceptionbb = EmitThrowExceptionLabel(X_READ_NIL);
	cc.CreateCondBr(cc.CreateICmpEQ(LoadD(A), ConstValueA(0)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
	cc.CreateCall(GetNativeFunc<void, DObject*, VMFunction*, int>("__ValidateCall", ValidateCall), { LoadA(A), ConstA(C), ConstValueD(B) });
}

#if 0

static void SetString(VMReturn* ret, FString* str)
{
	ret->SetString(*str);
}

void JitCompiler::EmitRET()
{
	using namespace asmjit;
	if (B == REGT_NIL)
	{
		EmitPopFrame();
		X86Gp vReg = newTempInt32();
		cc.mov(vReg, 0);
		cc.ret(vReg);
	}
	else
	{
		int a = A;
		int retnum = a & ~RET_FINAL;

		X86Gp reg_retnum = newTempInt32();
		X86Gp location = newTempIntPtr();
		Label L_endif = cc.newLabel();

		cc.mov(reg_retnum, retnum);
		cc.cmp(reg_retnum, numret);
		cc.jge(L_endif);

		cc.mov(location, x86::ptr(ret, retnum * sizeof(VMReturn)));

		int regtype = B;
		int regnum = C;
		switch (regtype & REGT_TYPE)
		{
		case REGT_INT:
			if (regtype & REGT_KONST)
				cc.mov(x86::dword_ptr(location), konstd[regnum]);
			else
				cc.mov(x86::dword_ptr(location), regD[regnum]);
			break;
		case REGT_FLOAT:
			if (regtype & REGT_KONST)
			{
				auto tmp = newTempInt64();
				if (regtype & REGT_MULTIREG3)
				{
					cc.mov(tmp, (((int64_t *)konstf)[regnum]));
					cc.mov(x86::qword_ptr(location), tmp);

					cc.mov(tmp, (((int64_t *)konstf)[regnum + 1]));
					cc.mov(x86::qword_ptr(location, 8), tmp);

					cc.mov(tmp, (((int64_t *)konstf)[regnum + 2]));
					cc.mov(x86::qword_ptr(location, 16), tmp);
				}
				else if (regtype & REGT_MULTIREG2)
				{
					cc.mov(tmp, (((int64_t *)konstf)[regnum]));
					cc.mov(x86::qword_ptr(location), tmp);

					cc.mov(tmp, (((int64_t *)konstf)[regnum + 1]));
					cc.mov(x86::qword_ptr(location, 8), tmp);
				}
				else
				{
					cc.mov(tmp, (((int64_t *)konstf)[regnum]));
					cc.mov(x86::qword_ptr(location), tmp);
				}
			}
			else
			{
				if (regtype & REGT_MULTIREG3)
				{
					cc.movsd(x86::qword_ptr(location), regF[regnum]);
					cc.movsd(x86::qword_ptr(location, 8), regF[regnum + 1]);
					cc.movsd(x86::qword_ptr(location, 16), regF[regnum + 2]);
				}
				else if (regtype & REGT_MULTIREG2)
				{
					cc.movsd(x86::qword_ptr(location), regF[regnum]);
					cc.movsd(x86::qword_ptr(location, 8), regF[regnum + 1]);
				}
				else
				{
					cc.movsd(x86::qword_ptr(location), regF[regnum]);
				}
			}
			break;
		case REGT_STRING:
		{
			auto ptr = newTempIntPtr();
			cc.mov(ptr, ret);
			cc.add(ptr, (int)(retnum * sizeof(VMReturn)));
			auto call = CreateCall<void, VMReturn*, FString*>(SetString);
			call->setArg(0, ptr);
			if (regtype & REGT_KONST) call->setArg(1, asmjit::imm_ptr(&konsts[regnum]));
			else                      call->setArg(1, regS[regnum]);
			break;
		}
		case REGT_POINTER:
			if (cc.is64Bit())
			{
				if (regtype & REGT_KONST)
				{
					auto ptr = newTempIntPtr();
					cc.mov(ptr, asmjit::imm_ptr(konsta[regnum].v));
					cc.mov(x86::qword_ptr(location), ptr);
				}
				else
				{
					cc.mov(x86::qword_ptr(location), regA[regnum]);
				}
			}
			else
			{
				if (regtype & REGT_KONST)
				{
					auto ptr = newTempIntPtr();
					cc.mov(ptr, asmjit::imm_ptr(konsta[regnum].v));
					cc.mov(x86::dword_ptr(location), ptr);
				}
				else
				{
					cc.mov(x86::dword_ptr(location), regA[regnum]);
				}
			}
			break;
		}

		if (a & RET_FINAL)
		{
			cc.add(reg_retnum, 1);
			EmitPopFrame();
			cc.ret(reg_retnum);
		}

		cc.bind(L_endif);
		if (a & RET_FINAL)
		{
			EmitPopFrame();
			cc.ret(numret);
		}
	}
}

void JitCompiler::EmitRETI()
{
	using namespace asmjit;

	int a = A;
	int retnum = a & ~RET_FINAL;

	X86Gp reg_retnum = newTempInt32();
	X86Gp location = newTempIntPtr();
	Label L_endif = cc.newLabel();

	cc.mov(reg_retnum, retnum);
	cc.cmp(reg_retnum, numret);
	cc.jge(L_endif);

	cc.mov(location, x86::ptr(ret, retnum * sizeof(VMReturn)));
	cc.mov(x86::dword_ptr(location), BCs);

	if (a & RET_FINAL)
	{
		cc.add(reg_retnum, 1);
		EmitPopFrame();
		cc.ret(reg_retnum);
	}

	cc.bind(L_endif);
	if (a & RET_FINAL)
	{
		EmitPopFrame();
		cc.ret(numret);
	}
}
#endif

void JitCompiler::EmitTHROW()
{
	EmitThrowException(EVMAbortException(BC));
}

void JitCompiler::EmitBOUND()
{
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	IRBasicBlock* exceptionbb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpUGE(LoadD(A), ConstValueD(BC)), exceptionbb, continuebb);
	cc.SetInsertPoint(exceptionbb);
	//JitLineInfo info;
	//info.Label = exceptionbb;
	//info.LineNumber = sfunc->PCToLine(pc);
	//LineInfo.Push(info);
	cc.CreateCall(GetNativeFunc<void, int, int>("__ThrowArrayOutOfBounds", &JitCompiler::ThrowArrayOutOfBounds), { LoadD(A), ConstValueD(BC) });
	cc.CreateBr(continuebb);
	cc.SetInsertPoint(continuebb);
}

void JitCompiler::EmitBOUND_K()
{
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	IRBasicBlock* exceptionbb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpUGE(LoadD(A), ConstD(BC)), exceptionbb, continuebb);
	cc.SetInsertPoint(exceptionbb);
	//JitLineInfo info;
	//info.Label = exceptionbb;
	//info.LineNumber = sfunc->PCToLine(pc);
	//LineInfo.Push(info);
	cc.CreateCall(GetNativeFunc<void, int, int>("__ThrowArrayOutOfBounds", &JitCompiler::ThrowArrayOutOfBounds), { LoadD(A), ConstD(BC) });
	cc.CreateBr(continuebb);
	cc.SetInsertPoint(continuebb);
}

void JitCompiler::EmitBOUND_R()
{
	IRBasicBlock* continuebb = irfunc->createBasicBlock({});
	IRBasicBlock* exceptionbb = irfunc->createBasicBlock({});
	cc.CreateCondBr(cc.CreateICmpUGE(LoadD(A), LoadD(B)), exceptionbb, continuebb);
	cc.SetInsertPoint(exceptionbb);
	//JitLineInfo info;
	//info.Label = exceptionbb;
	//info.LineNumber = sfunc->PCToLine(pc);
	//LineInfo.Push(info);
	cc.CreateCall(GetNativeFunc<void, int, int>("__ThrowArrayOutOfBounds", &JitCompiler::ThrowArrayOutOfBounds), { LoadD(A), LoadD(BC) });
	cc.CreateBr(continuebb);
	cc.SetInsertPoint(continuebb);
}

void JitCompiler::ThrowArrayOutOfBounds(int index, int size)
{
	if (index >= size)
	{
		ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Max.index = %u, current index = %u\n", size, index);
	}
	else
	{
		ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Negative current index = %i\n", index);
	}
}
