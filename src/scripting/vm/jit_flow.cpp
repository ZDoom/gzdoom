
#include "jitintern.h"

void JitCompiler::EmitTEST()
{
	int i = (int)(ptrdiff_t)(pc - sfunc->Code);
	cc.cmp(regD[A], BC);
	cc.jne(labels[i + 2]);
}
	
void JitCompiler::EmitTESTN()
{
	int bc = BC;
	int i = (int)(ptrdiff_t)(pc - sfunc->Code);
	cc.cmp(regD[A], -bc);
	cc.jne(labels[i + 2]);
}

void JitCompiler::EmitJMP()
{
	auto dest = pc + JMPOFS(pc) + 1;
	int i = (int)(ptrdiff_t)(dest - sfunc->Code);
	cc.jmp(labels[i]);
}

void JitCompiler::EmitIJMP()
{
	// This uses the whole function as potential jump targets. Can the range be reduced?

	int i = (int)(ptrdiff_t)(pc - sfunc->Code);
	auto val = newTempInt32();
	cc.mov(val, regD[A]);
	cc.add(val, i + (int)BCs + 1);

	int size = sfunc->CodeSize;
	for (i = 0; i < size; i++)
	{
		if (sfunc->Code[i].op == OP_JMP)
		{
			int target = i + JMPOFS(&sfunc->Code[i]) + 1;

			cc.cmp(val, i);
			cc.je(labels[target]);
		}
	}

	// This should never happen. It means we are jumping to something that is not a JMP instruction!
	EmitThrowException(X_OTHER);
}

void JitCompiler::EmitVTBL()
{
	auto notnull = cc.newLabel();
	cc.test(regA[B], regA[B]);
	cc.jnz(notnull);
	EmitThrowException(X_READ_NIL);
	cc.bind(notnull);

	auto result = newResultIntPtr();
	auto call = CreateCall<VMFunction*, DObject*, int>([](DObject *o, int c) -> VMFunction* {
		auto p = o->GetClass();
		assert(c < (int)p->Virtuals.Size());
		return p->Virtuals[c];
	});
	call->setRet(0, result);
	call->setArg(0, regA[B]);
	call->setArg(1, asmjit::Imm(C));
	cc.mov(regA[A], result);
}

void JitCompiler::EmitSCOPE()
{
	auto notnull = cc.newLabel();
	cc.test(regA[A], regA[A]);
	cc.jnz(notnull);
	EmitThrowException(X_READ_NIL);
	cc.bind(notnull);

	auto f = newTempIntPtr();
	cc.mov(f, asmjit::imm_ptr(konsta[C].v));

	typedef int(*FuncPtr)(DObject*, VMFunction*, int);
	auto call = CreateCall<void, DObject*, VMFunction*, int>([](DObject *o, VMFunction *f, int b) {
		try
		{
			FScopeBarrier::ValidateCall(o->GetClass(), f, b - 1);
		}
		catch (...)
		{
			VMThrowException(std::current_exception());
		}
	});
	call->setArg(0, regA[A]);
	call->setArg(1, f);
	call->setArg(2, asmjit::Imm(B));
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
			auto call = CreateCall<void, VMReturn*, FString*>([](VMReturn* ret, FString* str) -> void {
				ret->SetString(*str);
			});
			call->setArg(0, ptr);
			if (regtype & REGT_KONST) call->setArg(1, asmjit::imm_ptr(&konsts[regnum]));
			else                      call->setArg(1, regS[regnum]);
			break;
		}
		case REGT_POINTER:
			if (cc.is64Bit())
			{
				if (regtype & REGT_KONST)
					cc.mov(x86::qword_ptr(location), asmjit::imm_ptr(konsta[regnum].v));
				else
					cc.mov(x86::qword_ptr(location), regA[regnum]);
			}
			else
			{
				if (regtype & REGT_KONST)
					cc.mov(x86::dword_ptr(location), asmjit::imm_ptr(konsta[regnum].v));
				else
					cc.mov(x86::dword_ptr(location), regA[regnum]);
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

void JitCompiler::EmitNEW()
{
	auto result = newResultIntPtr();
	auto call = CreateCall<DObject*, PClass*, int>([](PClass *cls, int c) -> DObject* {
		try
		{
			if (!cls->ConstructNative)
			{
				ThrowAbortException(X_OTHER, "Class %s requires native construction", cls->TypeName.GetChars());
			}
			else if (cls->bAbstract)
			{
				ThrowAbortException(X_OTHER, "Cannot instantiate abstract class %s", cls->TypeName.GetChars());
			}
			else if (cls->IsDescendantOf(NAME_Actor)) // Creating actors here must be outright prohibited
			{
				ThrowAbortException(X_OTHER, "Cannot create actors with 'new'");
			}

			// [ZZ] validate readonly and between scope construction
			if (c) FScopeBarrier::ValidateNew(cls, c - 1);
			return cls->CreateNew();
		}
		catch (...)
		{
			VMThrowException(std::current_exception());
			return nullptr;
		}
	});
	call->setRet(0, result);
	call->setArg(0, regA[B]);
	call->setArg(1, asmjit::Imm(C));

	cc.mov(regA[A], result);
}

void JitCompiler::EmitNEW_K()
{
	PClass *cls = (PClass*)konsta[B].v;
	if (!cls->ConstructNative)
	{
		EmitThrowException(X_OTHER); // "Class %s requires native construction", cls->TypeName.GetChars()
	}
	else if (cls->bAbstract)
	{
		EmitThrowException(X_OTHER); // "Cannot instantiate abstract class %s", cls->TypeName.GetChars()
	}
	else if (cls->IsDescendantOf(NAME_Actor)) // Creating actors here must be outright prohibited
	{
		EmitThrowException(X_OTHER); // "Cannot create actors with 'new'"
	}
	else
	{
		auto result = newResultIntPtr();
		auto regcls = newTempIntPtr();
		cc.mov(regcls, asmjit::imm_ptr(konsta[B].v));
		auto call = CreateCall<DObject*, PClass*, int>([](PClass *cls, int c) -> DObject* {
			try
			{
				if (c) FScopeBarrier::ValidateNew(cls, c - 1);
				return cls->CreateNew();
			}
			catch (...)
			{
				VMThrowException(std::current_exception());
				return nullptr;
			}
		});
		call->setRet(0, result);
		call->setArg(0, regcls);
		call->setArg(1, asmjit::Imm(C));

		cc.mov(regA[A], result);
	}
}

void JitCompiler::EmitTHROW()
{
	EmitThrowException(EVMAbortException(BC));
}

void JitCompiler::EmitBOUND()
{
	auto label1 = cc.newLabel();
	auto label2 = cc.newLabel();
	cc.cmp(regD[A], (int)BC);
	cc.jl(label1);
	EmitThrowException(X_ARRAY_OUT_OF_BOUNDS); // "Max.index = %u, current index = %u\n", BC, reg.d[A]
	cc.bind(label1);
	cc.cmp(regD[A], (int)0);
	cc.jge(label2);
	EmitThrowException(X_ARRAY_OUT_OF_BOUNDS); // "Negative current index = %i\n", reg.d[A]
	cc.bind(label2);
}

void JitCompiler::EmitBOUND_K()
{
	auto label1 = cc.newLabel();
	auto label2 = cc.newLabel();
	cc.cmp(regD[A], (int)konstd[BC]);
	cc.jl(label1);
	EmitThrowException(X_ARRAY_OUT_OF_BOUNDS); // "Max.index = %u, current index = %u\n", konstd[BC], reg.d[A]
	cc.bind(label1);
	cc.cmp(regD[A], (int)0);
	cc.jge(label2);
	EmitThrowException(X_ARRAY_OUT_OF_BOUNDS); // "Negative current index = %i\n", reg.d[A]
	cc.bind(label2);
}

void JitCompiler::EmitBOUND_R()
{
	auto label1 = cc.newLabel();
	auto label2 = cc.newLabel();
	cc.cmp(regD[A], regD[B]);
	cc.jl(label1);
	EmitThrowException(X_ARRAY_OUT_OF_BOUNDS); // "Max.index = %u, current index = %u\n", reg.d[B], reg.d[A]
	cc.bind(label1);
	cc.cmp(regD[A], (int)0);
	cc.jge(label2);
	EmitThrowException(X_ARRAY_OUT_OF_BOUNDS); // "Negative current index = %i\n", reg.d[A]
	cc.bind(label2);
}
