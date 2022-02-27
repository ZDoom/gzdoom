
#include "jitintern.h"

void JitCompiler::EmitTEST()
{
	int i = (int)(ptrdiff_t)(pc - sfunc->Code);
	cc.cmp(regD[A], BC);
	cc.jne(GetLabel(i + 2));
}

void JitCompiler::EmitTESTN()
{
	int bc = BC;
	int i = (int)(ptrdiff_t)(pc - sfunc->Code);
	cc.cmp(regD[A], -bc);
	cc.jne(GetLabel(i + 2));
}

void JitCompiler::EmitJMP()
{
	auto dest = pc + JMPOFS(pc) + 1;
	int i = (int)(ptrdiff_t)(dest - sfunc->Code);
	cc.jmp(GetLabel(i));
}

void JitCompiler::EmitIJMP()
{
	int base = (int)(ptrdiff_t)(pc - sfunc->Code) + 1;
	auto val = newTempInt32();
	cc.mov(val, regD[A]);

	for (int i = 0; i < (int)BCs; i++)
	{
		if (sfunc->Code[base +i].op == OP_JMP)
		{
			int target = base + i + JMPOFS(&sfunc->Code[base + i]) + 1;

			cc.cmp(val, i);
			cc.je(GetLabel(target));
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
	auto label = EmitThrowExceptionLabel(X_READ_NIL);
	cc.test(regA[A], regA[A]);
	cc.jz(label);

	auto f = newTempIntPtr();
	cc.mov(f, asmjit::imm_ptr(konsta[C].v));

	typedef int(*FuncPtr)(DObject*, VMFunction*, int);
	auto call = CreateCall<void, DObject*, VMFunction*, int>(ValidateCall);
	call->setArg(0, regA[A]);
	call->setArg(1, f);
	call->setArg(2, asmjit::Imm(B));
}

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

void JitCompiler::EmitTHROW()
{
	EmitThrowException(EVMAbortException(BC));
}

void JitCompiler::EmitBOUND()
{
	auto cursor = cc.getCursor();
	auto label = cc.newLabel();
	cc.bind(label);
	auto call = CreateCall<void, int, int>(&JitCompiler::ThrowArrayOutOfBounds);
	call->setArg(0, regD[A]);
	call->setArg(1, asmjit::imm(BC));
	cc.setCursor(cursor);

	cc.cmp(regD[A], (int)BC);
	cc.jae(label);

	JitLineInfo info;
	info.Label = label;
	info.LineNumber = sfunc->PCToLine(pc);
	LineInfo.Push(info);
}

void JitCompiler::EmitBOUND_K()
{
	auto cursor = cc.getCursor();
	auto label = cc.newLabel();
	cc.bind(label);
	auto call = CreateCall<void, int, int>(&JitCompiler::ThrowArrayOutOfBounds);
	call->setArg(0, regD[A]);
	call->setArg(1, asmjit::imm(konstd[BC]));
	cc.setCursor(cursor);

	cc.cmp(regD[A], (int)konstd[BC]);
	cc.jae(label);

	JitLineInfo info;
	info.Label = label;
	info.LineNumber = sfunc->PCToLine(pc);
	LineInfo.Push(info);
}

void JitCompiler::EmitBOUND_R()
{
	auto cursor = cc.getCursor();
	auto label = cc.newLabel();
	cc.bind(label);
	auto call = CreateCall<void, int, int>(&JitCompiler::ThrowArrayOutOfBounds);
	call->setArg(0, regD[A]);
	call->setArg(1, regD[B]);
	cc.setCursor(cursor);

	cc.cmp(regD[A], regD[B]);
	cc.jae(label);

	JitLineInfo info;
	info.Label = label;
	info.LineNumber = sfunc->PCToLine(pc);
	LineInfo.Push(info);
}

void JitCompiler::ThrowArrayOutOfBounds(int index, int size)
{
	if (index >= size)
	{
		ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Size = %u, current index = %u\n", size, index);
	}
	else
	{
		ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Negative current index = %i\n", index);
	}
}
