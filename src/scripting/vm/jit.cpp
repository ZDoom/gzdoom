
#include "jit.h"
#include "i_system.h"
#include "types.h"

// To do: get cmake to define these..
#define ASMJIT_BUILD_EMBED
#define ASMJIT_STATIC

#include <asmjit/asmjit.h>
#include <asmjit/x86.h>
#include <functional>

#define A				(pc[0].a)
#define B				(pc[0].b)
#define C				(pc[0].c)
#define Cs				(pc[0].cs)
#define BC				(pc[0].i16u)
#define BCs				(pc[0].i16)
#define ABCs			(pc[0].i24)
#define JMPOFS(x)		((x)->i24)

static const char *OpNames[NUM_OPS] =
{
#define xx(op, name, mode, alt, kreg, ktype)	#op
#include "vmops.h"
};

class JitCompiler
{
public:
	JitCompiler(asmjit::CodeHolder *code, VMScriptFunction *sfunc) : cc(code), sfunc(sfunc) { }

	void Codegen()
	{
		using namespace asmjit;

		Setup();

		pc = sfunc->Code;
		auto end = pc + sfunc->CodeSize;
		while (pc != end)
		{
			int i = (int)(ptrdiff_t)(pc - sfunc->Code);
			op = pc->op;

			cc.bind(labels[i]);

			FString lineinfo;
			lineinfo.Format("; %s(line %d): %02x%02x%02x%02x %s", sfunc->PrintableName.GetChars(), sfunc->PCToLine(pc), pc->op, pc->a, pc->b, pc->c, OpNames[op]);
			cc.comment(lineinfo.GetChars(), lineinfo.Len());

			EmitFuncPtr opcodeFunc = GetOpcodeEmitFunc(op);
			if (!opcodeFunc)
				I_FatalError("JIT error: Unknown VM opcode %d\n", op);
			(this->*opcodeFunc)();

			pc++;
		}

		cc.endFunc();
		cc.finalize();
	}

	static bool CanJit(VMScriptFunction *sfunc)
	{
		int size = sfunc->CodeSize;
		for (int i = 0; i < size; i++)
		{
			auto opcodeFunc = GetOpcodeEmitFunc(sfunc->Code[i].op);
			if (!opcodeFunc)
				return false;

			if (sfunc->Code[i].op == OP_RET)
			{
				auto pc = sfunc->Code + i;
				if (B != REGT_NIL)
				{
					int regtype = B;
					if ((regtype & REGT_TYPE) == REGT_STRING)
						return false;
				}
			}
			else if (sfunc->Code[i].op == OP_CAST)
			{
				auto pc = sfunc->Code + i;
				switch (C)
				{
				case CAST_I2F:
				case CAST_U2F:
				case CAST_F2I:
				case CAST_F2U:
					break;
				default:
					return false;
				}
			}
			else if (sfunc->Code[i].op == OP_CASTB)
			{
				auto pc = sfunc->Code + i;
				if (C == CASTB_S)
					return false;
			}
		}
		return true;
	}

private:
	typedef void(JitCompiler::*EmitFuncPtr)();

	static EmitFuncPtr GetOpcodeEmitFunc(VM_UBYTE op)
	{
		#define EMIT_OP(x) case OP_##x: return &JitCompiler::Emit##x

		switch (op)
		{
			default: return nullptr;
			EMIT_OP(NOP);
			EMIT_OP(LI);
			EMIT_OP(LK);
			EMIT_OP(LKF);
			// EMIT_OP(LKS);
			EMIT_OP(LKP);
			EMIT_OP(LK_R);
			EMIT_OP(LKF_R);
			// EMIT_OP(LKS_R);
			EMIT_OP(LKP_R);
			// EMIT_OP(LFP);
			EMIT_OP(META);
			EMIT_OP(CLSS);
			EMIT_OP(LB);
			EMIT_OP(LB_R);
			EMIT_OP(LH);
			EMIT_OP(LH_R);
			EMIT_OP(LW);
			EMIT_OP(LW_R);
			EMIT_OP(LBU);
			EMIT_OP(LBU_R);
			EMIT_OP(LHU);
			EMIT_OP(LHU_R);
			EMIT_OP(LSP);
			EMIT_OP(LSP_R);
			EMIT_OP(LDP);
			EMIT_OP(LDP_R);
			// EMIT_OP(LS);
			// EMIT_OP(LS_R);
			EMIT_OP(LO);
			EMIT_OP(LO_R);
			EMIT_OP(LP);
			EMIT_OP(LP_R);
			EMIT_OP(LV2);
			EMIT_OP(LV2_R);
			EMIT_OP(LV3);
			EMIT_OP(LV3_R);
			//EMIT_OP(LCS);
			//EMIT_OP(LCS_R);
			EMIT_OP(LBIT);
			EMIT_OP(SB);
			EMIT_OP(SB_R);
			EMIT_OP(SH);
			EMIT_OP(SH_R);
			EMIT_OP(SW);
			EMIT_OP(SW_R);
			EMIT_OP(SSP);
			EMIT_OP(SSP_R);
			EMIT_OP(SDP);
			EMIT_OP(SDP_R);
			//EMIT_OP(SS);
			//EMIT_OP(SS_R);
			EMIT_OP(SO);
			EMIT_OP(SO_R);
			EMIT_OP(SP);
			EMIT_OP(SP_R);
			EMIT_OP(SV2);
			EMIT_OP(SV2_R);
			EMIT_OP(SV3);
			EMIT_OP(SV3_R);
			EMIT_OP(SBIT);
			EMIT_OP(MOVE);
			EMIT_OP(MOVEF);
			// EMIT_OP(MOVES);
			EMIT_OP(MOVEA);
			EMIT_OP(MOVEV2);
			EMIT_OP(MOVEV3);
			EMIT_OP(CAST);
			EMIT_OP(CASTB);
			EMIT_OP(DYNCAST_R);
			EMIT_OP(DYNCAST_K);
			EMIT_OP(DYNCASTC_R);
			EMIT_OP(DYNCASTC_K);
			EMIT_OP(TEST);
			EMIT_OP(TESTN);
			EMIT_OP(JMP);
			//EMIT_OP(IJMP);
			//EMIT_OP(PARAM);
			//EMIT_OP(PARAMI);
			//EMIT_OP(CALL);
			//EMIT_OP(CALL_K);
			EMIT_OP(VTBL);
			EMIT_OP(SCOPE);
			//EMIT_OP(TAIL);
			//EMIT_OP(TAIL_K);
			EMIT_OP(RESULT);
			EMIT_OP(RET);
			EMIT_OP(RETI);
			EMIT_OP(NEW);
			EMIT_OP(NEW_K);
			EMIT_OP(THROW);
			EMIT_OP(BOUND);
			EMIT_OP(BOUND_K);
			EMIT_OP(BOUND_R);
			//EMIT_OP(CONCAT);
			//EMIT_OP(LENS);
			//EMIT_OP(CMPS);
			EMIT_OP(SLL_RR);
			EMIT_OP(SLL_RI);
			EMIT_OP(SLL_KR);
			EMIT_OP(SRL_RR);
			EMIT_OP(SRL_RI);
			EMIT_OP(SRL_KR);
			EMIT_OP(SRA_RR);
			EMIT_OP(SRA_RI);
			EMIT_OP(SRA_KR);
			EMIT_OP(ADD_RR);
			EMIT_OP(ADD_RK);
			EMIT_OP(ADDI);
			EMIT_OP(SUB_RR);
			EMIT_OP(SUB_RK);
			EMIT_OP(SUB_KR);
			EMIT_OP(MUL_RR);
			EMIT_OP(MUL_RK);
			EMIT_OP(DIV_RR);
			EMIT_OP(DIV_RK);
			EMIT_OP(DIV_KR);
			EMIT_OP(DIVU_RR);
			EMIT_OP(DIVU_RK);
			EMIT_OP(DIVU_KR);
			EMIT_OP(MOD_RR);
			EMIT_OP(MOD_RK);
			EMIT_OP(MOD_KR);
			EMIT_OP(MODU_RR);
			EMIT_OP(MODU_RK);
			EMIT_OP(MODU_KR);
			EMIT_OP(AND_RR);
			EMIT_OP(AND_RK);
			EMIT_OP(OR_RR);
			EMIT_OP(OR_RK);
			EMIT_OP(XOR_RR);
			EMIT_OP(XOR_RK);
			EMIT_OP(MIN_RR);
			EMIT_OP(MIN_RK);
			EMIT_OP(MAX_RR);
			EMIT_OP(MAX_RK);
			EMIT_OP(ABS);
			EMIT_OP(NEG);
			EMIT_OP(NOT);
			EMIT_OP(EQ_R);
			EMIT_OP(EQ_K);
			EMIT_OP(LT_RR);
			EMIT_OP(LT_RK);
			EMIT_OP(LT_KR);
			EMIT_OP(LE_RR);
			EMIT_OP(LE_RK);
			EMIT_OP(LE_KR);
			EMIT_OP(LTU_RR);
			EMIT_OP(LTU_RK);
			EMIT_OP(LTU_KR);
			EMIT_OP(LEU_RR);
			EMIT_OP(LEU_RK);
			EMIT_OP(LEU_KR);
			EMIT_OP(ADDF_RR);
			EMIT_OP(ADDF_RK);
			EMIT_OP(SUBF_RR);
			EMIT_OP(SUBF_RK);
			EMIT_OP(SUBF_KR);
			EMIT_OP(MULF_RR);
			EMIT_OP(MULF_RK);
			EMIT_OP(DIVF_RR);
			EMIT_OP(DIVF_RK);
			EMIT_OP(DIVF_KR);
			EMIT_OP(MODF_RR);
			EMIT_OP(MODF_RK);
			EMIT_OP(MODF_KR);
			EMIT_OP(POWF_RR);
			EMIT_OP(POWF_RK);
			EMIT_OP(POWF_KR);
			EMIT_OP(MINF_RR);
			EMIT_OP(MINF_RK);
			EMIT_OP(MAXF_RR);
			EMIT_OP(MAXF_RK);
			EMIT_OP(ATAN2);
			EMIT_OP(FLOP);
			EMIT_OP(EQF_R);
			EMIT_OP(EQF_K);
			EMIT_OP(LTF_RR);
			EMIT_OP(LTF_RK);
			EMIT_OP(LTF_KR);
			EMIT_OP(LEF_RR);
			EMIT_OP(LEF_RK);
			EMIT_OP(LEF_KR);
			EMIT_OP(NEGV2);
			EMIT_OP(ADDV2_RR);
			EMIT_OP(SUBV2_RR);
			EMIT_OP(DOTV2_RR);
			EMIT_OP(MULVF2_RR);
			EMIT_OP(MULVF2_RK);
			EMIT_OP(DIVVF2_RR);
			EMIT_OP(DIVVF2_RK);
			EMIT_OP(LENV2);
			EMIT_OP(EQV2_R);
			EMIT_OP(EQV2_K);
			EMIT_OP(NEGV3);
			EMIT_OP(ADDV3_RR);
			EMIT_OP(SUBV3_RR);
			EMIT_OP(DOTV3_RR);
			EMIT_OP(CROSSV_RR);
			EMIT_OP(MULVF3_RR);
			EMIT_OP(MULVF3_RK);
			EMIT_OP(DIVVF3_RR);
			EMIT_OP(DIVVF3_RK);
			EMIT_OP(LENV3);
			EMIT_OP(EQV3_R);
			EMIT_OP(EQV3_K);
			EMIT_OP(ADDA_RR);
			EMIT_OP(ADDA_RK);
			EMIT_OP(SUBA);
			EMIT_OP(EQA_R);
			EMIT_OP(EQA_K);
		}
	}

	void EmitNOP()
	{
		cc.nop();
	}

	// Load constants.

	void EmitLI()
	{
		cc.mov(regD[A], BCs);
	}

	void EmitLK()
	{
		cc.mov(regD[A], konstd[BC]);
	}

	void EmitLKF()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(konstf + BC));
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
	}

	//void EmitLKS() { } // load string constant

	void EmitLKP()
	{
		cc.mov(regA[A], (int64_t)konsta[BC].v);
	}

	void EmitLK_R()
	{
		cc.mov(regD[A], asmjit::x86::ptr(ToMemAddress(konstd), regD[B], 2, C * sizeof(int32_t)));
	}

	void EmitLKF_R()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(konstf + BC));
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp, regD[B], 3, C * sizeof(double)));
	}

	//void EmitLKS_R() { } // load string constant indexed

	void EmitLKP_R()
	{
		cc.mov(regA[A], asmjit::x86::ptr(ToMemAddress(konsta), regD[B], 2, C * sizeof(void*)));
	}

	//void EmitLFP() { } // load frame pointer

	void EmitMETA()
	{
		typedef void*(*FuncPtr)(void*);

		auto label = cc.newLabel();
		cc.test(regA[B], regA[B]);
		cc.jne(label);
		EmitThrowException(X_READ_NIL);
		cc.bind(label);

		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](void *o) -> void*
		{
			return static_cast<DObject*>(o)->GetClass()->Meta;
		}))), asmjit::FuncSignature1<void*, void*>());
		call->setRet(0, regA[A]);
		call->setArg(0, regA[B]);
	}

	void EmitCLSS()
	{
		typedef void*(*FuncPtr)(void*);

		auto label = cc.newLabel();
		cc.test(regA[B], regA[B]);
		cc.jne(label);
		EmitThrowException(X_READ_NIL);
		cc.bind(label);

		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](void *o) -> void*
		{
			return static_cast<DObject*>(o)->GetClass();
		}))), asmjit::FuncSignature1<void*, void*>());
		call->setRet(0, regA[A]);
		call->setArg(0, regA[B]);
	}

	// Load from memory. rA = *(rB + rkC)

	void EmitLB()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.movsx(regD[A], asmjit::x86::byte_ptr(regA[B], konstd[C]));
	}

	void EmitLB_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.movsx(regD[A], asmjit::x86::byte_ptr(regA[B], regD[C]));
	}

	void EmitLH()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.movsx(regD[A], asmjit::x86::word_ptr(regA[B], konstd[C]));
	}

	void EmitLH_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.movsx(regD[A], asmjit::x86::word_ptr(regA[B], regD[C]));
	}

	void EmitLW()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.mov(regD[A], asmjit::x86::dword_ptr(regA[B], konstd[C]));
	}

	void EmitLW_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.mov(regD[A], asmjit::x86::dword_ptr(regA[B], regD[C]));
	}

	void EmitLBU()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.mov(regD[A], asmjit::x86::byte_ptr(regA[B], konstd[C]));
	}

	void EmitLBU_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.mov(regD[A], asmjit::x86::byte_ptr(regA[B], regD[C]));
	}

	void EmitLHU()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.mov(regD[A], asmjit::x86::word_ptr(regA[B], konstd[C]));
	}

	void EmitLHU_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.mov(regD[A], asmjit::x86::word_ptr(regA[B], regD[C]));
	}

	void EmitLSP()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.movss(regF[A], asmjit::x86::dword_ptr(regA[B], konstd[C]));
	}

	void EmitLSP_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.movss(regF[A], asmjit::x86::dword_ptr(regA[B], regD[C]));
	}

	void EmitLDP()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.movsd(regF[A], asmjit::x86::qword_ptr(regA[B], konstd[C]));
	}

	void EmitLDP_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.movsd(regF[A], asmjit::x86::qword_ptr(regA[B], regD[C]));
	}

	//void EmitLS() { } // load string
	//void EmitLS_R() { }

	void EmitLO()
	{
		EmitNullPointerThrow(B, X_READ_NIL);

		auto ptr = cc.newIntPtr();
		cc.mov(ptr, asmjit::x86::ptr(regA[B], konstd[C]));

		typedef void*(*FuncPtr)(void*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](void *ptr) -> void*
		{
			DObject *p = static_cast<DObject *>(ptr);
			return GC::ReadBarrier(p);
		}))), asmjit::FuncSignature1<void*, void*>());
		call->setRet(0, regA[A]);
		call->setArg(0, ptr);
	}

	void EmitLO_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);

		auto ptr = cc.newIntPtr();
		cc.mov(ptr, asmjit::x86::ptr(regA[B], regD[C]));

		typedef void*(*FuncPtr)(void*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](void *ptr) -> void*
		{
			DObject *p = static_cast<DObject *>(ptr);
			return GC::ReadBarrier(p);
		}))), asmjit::FuncSignature1<void*, void*>());
		call->setRet(0, regA[A]);
		call->setArg(0, ptr);
	}

	void EmitLP()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.mov(regA[A], asmjit::x86::ptr(regA[B], konstd[C]));
	}

	void EmitLP_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.mov(regA[A], asmjit::x86::ptr(regA[B], regD[C]));
	}

	void EmitLV2()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.add(tmp, konstd[C]);
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.movsd(regF[A + 1], asmjit::x86::qword_ptr(tmp, 8));
	}

	void EmitLV2_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.add(tmp, regD[C]);
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.movsd(regF[A + 1], asmjit::x86::qword_ptr(tmp, 8));
	}

	void EmitLV3()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.add(tmp, konstd[C]);
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.movsd(regF[A + 1], asmjit::x86::qword_ptr(tmp, 8));
		cc.movsd(regF[A + 2], asmjit::x86::qword_ptr(tmp, 16));
	}

	void EmitLV3_R()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.add(tmp, regD[C]);
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.movsd(regF[A + 1], asmjit::x86::qword_ptr(tmp, 8));
		cc.movsd(regF[A + 2], asmjit::x86::qword_ptr(tmp, 16));
	}

	//void EmitLCS() { } // load string from char ptr.
	//void EmitLCS_R() { }

	void EmitLBIT()
	{
		EmitNullPointerThrow(B, X_READ_NIL);
		cc.movsx(regD[A], asmjit::x86::byte_ptr(regA[B]));
		cc.and_(regD[A], C);
		cc.cmp(regD[A], 0);
		cc.setne(regD[A]);
	}

	// Store instructions. *(rA + rkC) = rB

	void EmitSB()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.mov(asmjit::x86::byte_ptr(regA[A], konstd[C]), regD[B].r8Lo());
	}

	void EmitSB_R()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.mov(asmjit::x86::byte_ptr(regA[A], regD[C]), regD[B].r8Lo());
	}

	void EmitSH()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.mov(asmjit::x86::word_ptr(regA[A], konstd[C]), regD[B].r16());
	}

	void EmitSH_R()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.mov(asmjit::x86::word_ptr(regA[A], regD[C]), regD[B].r16());
	}

	void EmitSW()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.mov(asmjit::x86::dword_ptr(regA[A], konstd[C]), regD[B]);
	}

	void EmitSW_R()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.mov(asmjit::x86::dword_ptr(regA[A], regD[C]), regD[B]);
	}

	void EmitSSP()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.movss(asmjit::x86::dword_ptr(regA[A], konstd[C]), regF[B]);
	}

	void EmitSSP_R()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.movss(asmjit::x86::dword_ptr(regA[A], regD[C]), regF[B]);
	}

	void EmitSDP()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.movsd(asmjit::x86::qword_ptr(regA[A], konstd[C]), regF[B]);
	}

	void EmitSDP_R()
	{
		EmitNullPointerThrow(A, X_WRITE_NIL);
		cc.movsd(asmjit::x86::qword_ptr(regA[A], regD[C]), regF[B]);
	}

	//void EmitSS() {} // store string
	//void EmitSS_R() {}

	void EmitSO()
	{
		cc.mov(asmjit::x86::ptr(regA[A], konstd[C]), regA[B]);

		typedef void(*FuncPtr)(DObject*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(GC::WriteBarrier))), asmjit::FuncSignature1<void, void*>());
		call->setArg(0, regA[B]);
	}

	void EmitSO_R()
	{
		cc.mov(asmjit::x86::ptr(regA[A], regD[C]), regA[B]);

		typedef void(*FuncPtr)(DObject*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(GC::WriteBarrier))), asmjit::FuncSignature1<void, void*>());
		call->setArg(0, regA[B]);
	}

	void EmitSP()
	{
		cc.mov(asmjit::x86::ptr(regA[A], konstd[C]), regA[B]);
	}

	void EmitSP_R()
	{
		cc.mov(asmjit::x86::ptr(regA[A], regD[C]), regA[B]);
	}

	void EmitSV2()
	{
		EmitNullPointerThrow(B, X_WRITE_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.add(tmp, konstd[C]);
		cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	}

	void EmitSV2_R()
	{
		EmitNullPointerThrow(B, X_WRITE_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.add(tmp, regD[C]);
		cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	}

	void EmitSV3()
	{
		EmitNullPointerThrow(B, X_WRITE_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.add(tmp, konstd[C]);
		cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
	}

	void EmitSV3_R()
	{
		EmitNullPointerThrow(B, X_WRITE_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.add(tmp, regD[C]);
		cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
	}

	void EmitSBIT()
	{
		EmitNullPointerThrow(B, X_WRITE_NIL);
		auto tmp1 = cc.newInt32();
		auto tmp2 = cc.newInt32();
		cc.mov(tmp1, asmjit::x86::byte_ptr(regA[A]));
		cc.mov(tmp2, tmp1);
		cc.or_(tmp1, (int)C);
		cc.and_(tmp2, ~(int)C);
		cc.test(regD[B], regD[B]);
		cc.cmove(tmp1, tmp2);
		cc.mov(asmjit::x86::byte_ptr(regA[A]), tmp1);
	}

	// Move instructions.

	void EmitMOVE()
	{
		cc.mov(regD[A], regD[B]);
	}
	void EmitMOVEF()
	{
		cc.movsd(regF[A], regF[B]);
	}

	// void EmitMOVES() {} // sA = sB

	void EmitMOVEA()
	{
		cc.mov(regA[A], regA[B]);
	}

	void EmitMOVEV2()
	{
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
	}

	void EmitMOVEV3()
	{
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.movsd(regF[A + 2], regF[B + 2]);
	}

	void EmitCAST()
	{
		switch (C)
		{
		case CAST_I2F:
			cc.cvtsi2sd(regF[A], regD[B]);
			break;
		case CAST_U2F:
		{
			auto tmp = cc.newInt64();
			cc.xor_(tmp, tmp);
			cc.mov(tmp.r32(), regD[B]);
			cc.cvtsi2sd(regF[A], tmp);
			break;
		}
		case CAST_F2I:
			cc.cvttsd2si(regD[A], regF[B]);
			break;
		case CAST_F2U:
		{
			auto tmp = cc.newInt64();
			cc.cvttsd2si(tmp, regF[B]);
			cc.mov(regD[A], tmp.r32());
			break;
		}
		/*case CAST_I2S:
			reg.s[A].Format("%d", reg.d[B]);
			break;
		case CAST_U2S:
			reg.s[A].Format("%u", reg.d[B]);
			break;
		case CAST_F2S:
			reg.s[A].Format("%.5f", reg.f[B]);	// keep this small. For more precise conversion there should be a conversion function.
			break;
		case CAST_V22S:
			reg.s[A].Format("(%.5f, %.5f)", reg.f[B], reg.f[b + 1]);
			break;
		case CAST_V32S:
			reg.s[A].Format("(%.5f, %.5f, %.5f)", reg.f[B], reg.f[b + 1], reg.f[b + 2]);
			break;
		case CAST_P2S:
		{
			if (reg.a[B] == nullptr) reg.s[A] = "null";
			else reg.s[A].Format("%p", reg.a[B]);
			break;
		}
		case CAST_S2I:
			reg.d[A] = (VM_SWORD)reg.s[B].ToLong();
			break;
		case CAST_S2F:
			reg.f[A] = reg.s[B].ToDouble();
			break;
		case CAST_S2N:
			reg.d[A] = reg.s[B].Len() == 0 ? FName(NAME_None) : FName(reg.s[B]);
			break;
		case CAST_N2S:
		{
			FName name = FName(ENamedName(reg.d[B]));
			reg.s[A] = name.IsValidName() ? name.GetChars() : "";
			break;
		}
		case CAST_S2Co:
			reg.d[A] = V_GetColor(NULL, reg.s[B]);
			break;
		case CAST_Co2S:
			reg.s[A].Format("%02x %02x %02x", PalEntry(reg.d[B]).r, PalEntry(reg.d[B]).g, PalEntry(reg.d[B]).b);
			break;
		case CAST_S2So:
			reg.d[A] = FSoundID(reg.s[B]);
			break;
		case CAST_So2S:
			reg.s[A] = S_sfx[reg.d[B]].name;
			break;
		case CAST_SID2S:
			reg.s[A] = unsigned(reg.d[B]) >= sprites.Size() ? "TNT1" : sprites[reg.d[B]].name;
			break;
		case CAST_TID2S:
		{
			auto tex = TexMan[*(FTextureID*)&(reg.d[B])];
			reg.s[A] = tex == nullptr ? "(null)" : tex->Name.GetChars();
			break;
		}*/
		default:
			assert(0);
		}
	}

	void EmitCASTB()
	{
		if (C == CASTB_I)
		{
			cc.mov(regD[A], regD[B]);
			cc.shr(regD[A], 31);
		}
		else if (C == CASTB_F)
		{
			auto zero = cc.newXmm();
			auto one = cc.newInt32();
			cc.xorpd(zero, zero);
			cc.mov(one, 1);
			cc.ucomisd(regF[A], zero);
			cc.setp(regD[A]);
			cc.cmovne(regD[A], one);
		}
		else if (C == CASTB_A)
		{
			cc.test(regA[A], regA[A]);
			cc.setne(regD[A]);
		}
		else
		{
			//reg.d[A] = reg.s[B].Len() > 0;
		}
	}

	void EmitDYNCAST_R()
	{
		using namespace asmjit;
		typedef DObject*(*FuncPtr)(DObject*, PClass*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](DObject *obj, PClass *cls) -> DObject* {
			return (obj && obj->IsKindOf(cls)) ? obj : nullptr;
		}))), FuncSignature2<void*, void*, void*>());
		call->setRet(0, regA[A]);
		call->setArg(0, regA[B]);
		call->setArg(1, regA[C]);
	}

	void EmitDYNCAST_K()
	{
		using namespace asmjit;
		auto c = cc.newIntPtr();
		cc.mov(c, ToMemAddress(konsta[C].o));
		typedef DObject*(*FuncPtr)(DObject*, PClass*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](DObject *obj, PClass *cls) -> DObject* {
			return (obj && obj->IsKindOf(cls)) ? obj : nullptr;
		}))), FuncSignature2<void*, void*, void*>());
		call->setRet(0, regA[A]);
		call->setArg(0, regA[B]);
		call->setArg(1, c);
	}

	void EmitDYNCASTC_R()
	{
		using namespace asmjit;
		typedef PClass*(*FuncPtr)(PClass*, PClass*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](PClass *cls1, PClass *cls2) -> PClass* {
			return (cls1 && cls1->IsDescendantOf(cls2)) ? cls1 : nullptr;
		}))), FuncSignature2<void*, void*, void*>());
		call->setRet(0, regA[A]);
		call->setArg(0, regA[B]);
		call->setArg(1, regA[C]);
	}

	void EmitDYNCASTC_K()
	{
		using namespace asmjit;
		auto c = cc.newIntPtr();
		cc.mov(c, ToMemAddress(konsta[C].o));
		typedef PClass*(*FuncPtr)(PClass*, PClass*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](PClass *cls1, PClass *cls2) -> PClass* {
			return (cls1 && cls1->IsDescendantOf(cls2)) ? cls1 : nullptr;
		}))), FuncSignature2<void*, void*, void*>());
		call->setRet(0, regA[A]);
		call->setArg(0, regA[B]);
		call->setArg(1, c);
	}

	// Control flow.

	void EmitTEST()
	{
		int i = (int)(ptrdiff_t)(pc - sfunc->Code);
		cc.cmp(regD[A], BC);
		cc.jne(labels[i + 2]);
	}
	
	void EmitTESTN()
	{
		int bc = BC;
		int i = (int)(ptrdiff_t)(pc - sfunc->Code);
		cc.cmp(regD[A], -bc);
		cc.jne(labels[i + 2]);
	}

	void EmitJMP()
	{
		auto dest = pc + JMPOFS(pc) + 1;
		int i = (int)(ptrdiff_t)(dest - sfunc->Code);
		cc.jmp(labels[i]);
	}

	//void EmitIJMP() {} // pc += dA + BC	-- BC is a signed offset. The target instruction must be a JMP.
	//void EmitPARAM() {} // push parameter encoded in BC for function call (B=regtype, C=regnum)
	//void EmitPARAMI() {} // push immediate, signed integer for function call
	//void EmitCALL() {} // Call function pkA with parameter count B and expected result count C
	//void EmitCALL_K() {}

	void EmitVTBL()
	{
		auto notnull = cc.newLabel();
		cc.test(regA[B], regA[B]);
		cc.jnz(notnull);
		EmitThrowException(X_READ_NIL);
		cc.bind(notnull);

		auto result = cc.newInt32();
		typedef VMFunction*(*FuncPtr)(DObject*, int);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](DObject *o, int c) -> VMFunction* {
			auto p = o->GetClass();
			assert(c < (int)p->Virtuals.Size());
			return p->Virtuals[c];
		}))), asmjit::FuncSignature2<void*, void*, int>());
		call->setRet(0, result);
		call->setArg(0, regA[A]);
		call->setArg(1, asmjit::Imm(C));
	}

	void EmitSCOPE()
	{
		auto notnull = cc.newLabel();
		cc.test(regA[A], regA[A]);
		cc.jnz(notnull);
		EmitThrowException(X_READ_NIL);
		cc.bind(notnull);

		auto f = cc.newIntPtr();
		cc.mov(f, ToMemAddress(konsta[C].v));

		auto result = cc.newInt32();
		typedef int(*FuncPtr)(DObject*, VMFunction*, int);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](DObject *o, VMFunction *f, int b) -> int {
			try
			{
				FScopeBarrier::ValidateCall(o->GetClass(), f, b - 1);
				return 1;
			}
			catch (const CVMAbortException &)
			{
				// To do: pass along the exception info
				return 0;
			}
		}))), asmjit::FuncSignature3<int, void*, void*, int>());
		call->setRet(0, result);
		call->setArg(0, regA[A]);
		call->setArg(1, f);
		call->setArg(2, asmjit::Imm(B));

		auto notzero = cc.newLabel();
		cc.test(result, result);
		cc.jnz(notzero);
		EmitThrowException(X_OTHER);
		cc.bind(notzero);
	}

	//void EmitTAIL() {} // Call+Ret in a single instruction
	//void EmitTAIL_K() {}

	void EmitRESULT()
	{
		// This instruction is just a placeholder to indicate where a return
		// value should be stored. It does nothing on its own and should not
		// be executed.
	}

	void EmitRET()
	{
		using namespace asmjit;
		if (B == REGT_NIL)
		{
			X86Gp vReg = cc.newInt32();
			cc.mov(vReg, 0);
			cc.ret(vReg);
		}
		else
		{
			int a = A;
			int retnum = a & ~RET_FINAL;

			X86Gp reg_retnum = cc.newInt32();
			X86Gp location = cc.newIntPtr();
			Label L_endif = cc.newLabel();

			cc.mov(reg_retnum, retnum);
			cc.test(reg_retnum, numret);
			cc.jg(L_endif);

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
					auto tmp = cc.newInt64();
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
				break;
			case REGT_POINTER:
				#ifdef ASMJIT_ARCH_X64
				if (regtype & REGT_KONST)
					cc.mov(x86::qword_ptr(location), ToMemAddress(konsta[regnum].v));
				else
					cc.mov(x86::qword_ptr(location), regA[regnum]);
				#else
				if (regtype & REGT_KONST)
					cc.mov(x86::dword_ptr(location), ToMemAddress(konsta[regnum].v));
				else
					cc.mov(x86::dword_ptr(location), regA[regnum]);
				#endif
				break;
			}

			if (a & RET_FINAL)
			{
				cc.add(reg_retnum, 1);
				cc.ret(reg_retnum);
			}

			cc.bind(L_endif);
			if (a & RET_FINAL)
				cc.ret(numret);
		}
	}

	void EmitRETI()
	{
		using namespace asmjit;

		int a = A;
		int retnum = a & ~RET_FINAL;

		X86Gp reg_retnum = cc.newInt32();
		X86Gp location = cc.newIntPtr();
		Label L_endif = cc.newLabel();

		cc.mov(reg_retnum, retnum);
		cc.test(reg_retnum, numret);
		cc.jg(L_endif);

		cc.mov(location, x86::ptr(ret, retnum * sizeof(VMReturn)));
		cc.mov(x86::dword_ptr(location), BCs);

		if (a & RET_FINAL)
		{
			cc.add(reg_retnum, 1);
			cc.ret(reg_retnum);
		}

		cc.bind(L_endif);
		if (a & RET_FINAL)
			cc.ret(numret);
	}

	void EmitNEW()
	{
		auto result = cc.newIntPtr();
		typedef DObject*(*FuncPtr)(PClass*, int);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](PClass *cls, int c) -> DObject* {
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
			catch (const CVMAbortException &)
			{
				// To do: pass along the exception info
				return nullptr;
			}
		}))), asmjit::FuncSignature2<void*, void*, int>());
		call->setRet(0, result);
		call->setArg(0, regA[B]);
		call->setArg(1, asmjit::Imm(C));

		auto notnull = cc.newLabel();
		cc.test(result, result);
		cc.jnz(notnull);
		EmitThrowException(X_OTHER);
		cc.bind(notnull);
		cc.mov(regA[A], result);
	}

	void EmitNEW_K()
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
			auto result = cc.newIntPtr();
			auto regcls = cc.newIntPtr();
			cc.mov(regcls, ToMemAddress(konsta[B].v));
			typedef DObject*(*FuncPtr)(PClass*, int);
			auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](PClass *cls, int c) -> DObject* {
				try
				{
					if (c) FScopeBarrier::ValidateNew(cls, c - 1);
					return cls->CreateNew();
				}
				catch (const CVMAbortException &)
				{
					// To do: pass along the exception info
					return nullptr;
				}
			}))), asmjit::FuncSignature2<void*, void*, int>());
			call->setRet(0, result);
			call->setArg(0, regcls);
			call->setArg(1, asmjit::Imm(C));

			auto notnull = cc.newLabel();
			cc.test(result, result);
			cc.jnz(notnull);
			EmitThrowException(X_OTHER);
			cc.bind(notnull);
			cc.mov(regA[A], result);
		}
	}

	void EmitTHROW()
	{
		EmitThrowException(EVMAbortException(BC));
	}

	void EmitBOUND()
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

	void EmitBOUND_K()
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

	void EmitBOUND_R()
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

	// String instructions.

	//void EmitCONCAT() {} // sA = sB..sC
	//void EmitLENS() {} // dA = sB.Length
	//void EmitCMPS() {} // if ((skB op skC) != (A & 1)) then pc++

	// Integer math.

	void EmitSLL_RR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], regD[B]);
		cc.shl(regD[A], rc);
	}

	void EmitSLL_RI()
	{
		cc.mov(regD[A], regD[B]);
		cc.shl(regD[A], C);
	}

	void EmitSLL_KR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], konstd[B]);
		cc.shl(regD[A], rc);
	}

	void EmitSRL_RR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], regD[B]);
		cc.shr(regD[A], rc);
	}

	void EmitSRL_RI()
	{
		cc.mov(regD[A], regD[B]);
		cc.shr(regD[A], C);
	}

	void EmitSRL_KR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], konstd[B]);
		cc.shr(regD[A], rc);
	}

	void EmitSRA_RR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], regD[B]);
		cc.sar(regD[A], rc);
	}

	void EmitSRA_RI()
	{
		cc.mov(regD[A], regD[B]);
		cc.sar(regD[A], C);
	}

	void EmitSRA_KR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], konstd[B]);
		cc.sar(regD[A], rc);
	}

	void EmitADD_RR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], konstd[B]);
		cc.add(regD[A], rc);
	}

	void EmitADD_RK()
	{
		cc.mov(regD[A], konstd[B]);
		cc.add(regD[A], konstd[C]);
	}

	void EmitADDI()
	{
		cc.mov(regD[A], konstd[B]);
		cc.add(regD[A], Cs);
	}

	void EmitSUB_RR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], regD[B]);
		cc.sub(regD[A], rc);
	}

	void EmitSUB_RK()
	{
		cc.mov(regD[A], konstd[B]);
		cc.sub(regD[A], konstd[C]);
	}

	void EmitSUB_KR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], konstd[B]);
		cc.sub(regD[A], rc);
	}

	void EmitMUL_RR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], regD[B]);
		cc.imul(regD[A], rc);
	}

	void EmitMUL_RK()
	{
		cc.mov(regD[A], regD[B]);
		cc.imul(regD[A], konstd[C]);
	}

	void EmitDIV_RR()
	{
		auto tmp0 = cc.newInt32();
		auto tmp1 = cc.newInt32();
		auto label = cc.newLabel();

		cc.test(regD[C], regD[C]);
		cc.jne(label);

		EmitThrowException(X_DIVISION_BY_ZERO);

		cc.bind(label);
		cc.mov(tmp0, regD[B]);
		cc.cdq(tmp1, tmp0);
		cc.idiv(tmp1, tmp0, regD[C]);
		cc.mov(regD[A], tmp0);
	}

	void EmitDIV_RK()
	{
		if (konstd[C] != 0)
		{
			auto tmp0 = cc.newInt32();
			auto tmp1 = cc.newInt32();
			auto konstTmp = cc.newIntPtr();
			cc.mov(tmp0, regD[B]);
			cc.cdq(tmp1, tmp0);
			cc.mov(konstTmp, ToMemAddress(&konstd[C]));
			cc.idiv(tmp1, tmp0, asmjit::x86::ptr(konstTmp));
			cc.mov(regD[A], tmp0);
		}
		else EmitThrowException(X_DIVISION_BY_ZERO);
	}

	void EmitDIV_KR()
	{
		auto tmp0 = cc.newInt32();
		auto tmp1 = cc.newInt32();
		auto label = cc.newLabel();

		cc.test(regD[C], regD[C]);
		cc.jne(label);

		EmitThrowException(X_DIVISION_BY_ZERO);

		cc.bind(label);
		cc.mov(tmp0, konstd[B]);
		cc.cdq(tmp1, tmp0);
		cc.idiv(tmp1, tmp0, regD[C]);
		cc.mov(regD[A], tmp0);
	}

	void EmitDIVU_RR()
	{
		auto tmp0 = cc.newInt32();
		auto tmp1 = cc.newInt32();
		auto label = cc.newLabel();

		cc.test(regD[C], regD[C]);
		cc.jne(label);

		EmitThrowException(X_DIVISION_BY_ZERO);

		cc.bind(label);
		cc.mov(tmp0, regD[B]);
		cc.mov(tmp1, 0);
		cc.div(tmp1, tmp0, regD[C]);
		cc.mov(regD[A], tmp0);
	}

	void EmitDIVU_RK()
	{
		if (konstd[C] != 0)
		{
			auto tmp0 = cc.newInt32();
			auto tmp1 = cc.newInt32();
			auto konstTmp = cc.newIntPtr();
			cc.mov(tmp0, regD[B]);
			cc.mov(tmp1, 0);
			cc.mov(konstTmp, ToMemAddress(&konstd[C]));
			cc.div(tmp1, tmp0, asmjit::x86::ptr(konstTmp));
			cc.mov(regD[A], tmp0);
		}
		else EmitThrowException(X_DIVISION_BY_ZERO);
	}

	void EmitDIVU_KR()
	{
		auto tmp0 = cc.newInt32();
		auto tmp1 = cc.newInt32();
		auto label = cc.newLabel();

		cc.test(regD[C], regD[C]);
		cc.jne(label);

		EmitThrowException(X_DIVISION_BY_ZERO);

		cc.bind(label);
		cc.mov(tmp0, konstd[B]);
		cc.mov(tmp1, 0);
		cc.div(tmp1, tmp0, regD[C]);
		cc.mov(regD[A], tmp0);
	}

	void EmitMOD_RR()
	{
		auto tmp0 = cc.newInt32();
		auto tmp1 = cc.newInt32();
		auto label = cc.newLabel();

		cc.test(regD[C], regD[C]);
		cc.jne(label);

		EmitThrowException(X_DIVISION_BY_ZERO);

		cc.bind(label);
		cc.mov(tmp0, regD[B]);
		cc.cdq(tmp1, tmp0);
		cc.idiv(tmp1, tmp0, regD[C]);
		cc.mov(regD[A], tmp1);
	}

	void EmitMOD_RK()
	{
		if (konstd[C] != 0)
		{
			auto tmp0 = cc.newInt32();
			auto tmp1 = cc.newInt32();
			auto konstTmp = cc.newIntPtr();
			cc.mov(tmp0, regD[B]);
			cc.cdq(tmp1, tmp0);
			cc.mov(konstTmp, ToMemAddress(&konstd[C]));
			cc.idiv(tmp1, tmp0, asmjit::x86::ptr(konstTmp));
			cc.mov(regD[A], tmp1);
		}
		else EmitThrowException(X_DIVISION_BY_ZERO);
	}

	void EmitMOD_KR()
	{
		auto tmp0 = cc.newInt32();
		auto tmp1 = cc.newInt32();
		auto label = cc.newLabel();

		cc.test(regD[C], regD[C]);
		cc.jne(label);

		EmitThrowException(X_DIVISION_BY_ZERO);

		cc.bind(label);
		cc.mov(tmp0, konstd[B]);
		cc.cdq(tmp1, tmp0);
		cc.idiv(tmp1, tmp0, regD[C]);
		cc.mov(regD[A], tmp1);
	}

	void EmitMODU_RR()
	{
		auto tmp0 = cc.newInt32();
		auto tmp1 = cc.newInt32();
		auto label = cc.newLabel();

		cc.test(regD[C], regD[C]);
		cc.jne(label);

		EmitThrowException(X_DIVISION_BY_ZERO);

		cc.bind(label);
		cc.mov(tmp0, regD[B]);
		cc.mov(tmp1, 0);
		cc.div(tmp1, tmp0, regD[C]);
		cc.mov(regD[A], tmp1);
	}

	void EmitMODU_RK()
	{
		if (konstd[C] != 0)
		{
			auto tmp0 = cc.newInt32();
			auto tmp1 = cc.newInt32();
			auto konstTmp = cc.newIntPtr();
			cc.mov(tmp0, regD[B]);
			cc.mov(tmp1, 0);
			cc.mov(konstTmp, ToMemAddress(&konstd[C]));
			cc.div(tmp1, tmp0, asmjit::x86::ptr(konstTmp));
			cc.mov(regD[A], tmp1);
		}
		else EmitThrowException(X_DIVISION_BY_ZERO);
	}

	void EmitMODU_KR()
	{
		auto tmp0 = cc.newInt32();
		auto tmp1 = cc.newInt32();
		auto label = cc.newLabel();

		cc.test(regD[C], regD[C]);
		cc.jne(label);

		EmitThrowException(X_DIVISION_BY_ZERO);

		cc.bind(label);
		cc.mov(tmp0, konstd[B]);
		cc.mov(tmp1, 0);
		cc.div(tmp1, tmp0, regD[C]);
		cc.mov(regD[A], tmp1);
	}

	void EmitAND_RR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], regD[B]);
		cc.and_(regD[A], rc);
	}

	void EmitAND_RK()
	{
		cc.mov(regD[A], regD[B]);
		cc.and_(regD[A], konstd[C]);
	}

	void EmitOR_RR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], regD[B]);
		cc.or_(regD[A], rc);
	}

	void EmitOR_RK()
	{
		cc.mov(regD[A], regD[B]);
		cc.or_(regD[A], konstd[C]);
	}

	void EmitXOR_RR()
	{
		auto rc = CheckRegD(C, A);
		cc.mov(regD[A], regD[B]);
		cc.xor_(regD[A], rc);
	}

	void EmitXOR_RK()
	{
		cc.mov(regD[A], regD[B]);
		cc.xor_(regD[A], konstd[C]);
	}

	void EmitMIN_RR()
	{
		auto tmp0 = cc.newXmmSs();
		auto tmp1 = cc.newXmmSs();
		cc.movd(tmp0, regD[B]);
		cc.movd(tmp1, regD[C]);
		cc.pminsd(tmp0, tmp1);
		cc.movd(regD[A], tmp0);
	}

	void EmitMIN_RK()
	{
		auto tmp0 = cc.newXmmSs();
		auto tmp1 = cc.newXmmSs();
		auto konstTmp = cc.newIntPtr();
		cc.mov(konstTmp, ToMemAddress(&konstd[C]));
		cc.movd(tmp0, regD[B]);
		cc.movss(tmp1, asmjit::x86::dword_ptr(konstTmp));
		cc.pminsd(tmp0, tmp1);
		cc.movd(regD[A], tmp0);
	}

	void EmitMAX_RR()
	{
		auto tmp0 = cc.newXmmSs();
		auto tmp1 = cc.newXmmSs();
		cc.movd(tmp0, regD[B]);
		cc.movd(tmp1, regD[C]);
		cc.pmaxsd(tmp0, tmp1);
		cc.movd(regD[A], tmp0);
	}

	void EmitMAX_RK()
	{
		auto tmp0 = cc.newXmmSs();
		auto tmp1 = cc.newXmmSs();
		auto konstTmp = cc.newIntPtr();
		cc.mov(konstTmp, ToMemAddress(&konstd[C]));
		cc.movd(tmp0, regD[B]);
		cc.movss(tmp1, asmjit::x86::dword_ptr(konstTmp));
		cc.pmaxsd(tmp0, tmp1);
		cc.movd(regD[A], tmp0);
	}

	void EmitABS()
	{
		auto srcB = CheckRegD(B, A);
		auto tmp = cc.newInt32();
		cc.mov(tmp, regD[B]);
		cc.sar(tmp, 31);
		cc.mov(regD[A], tmp);
		cc.xor_(regD[A], srcB);
		cc.sub(regD[A], tmp);
	}

	void EmitNEG()
	{
		auto srcB = CheckRegD(B, A);
		cc.xor_(regD[A], regD[A]);
		cc.sub(regD[A], srcB);
	}

	void EmitNOT()
	{
		cc.mov(regD[A], regD[B]);
		cc.not_(regD[A]);
	}

	void EmitEQ_R()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], regD[C]);
			cc.sete(result);
		});
	}

	void EmitEQ_K()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], konstd[C]);
			cc.sete(result);
		});
	}

	void EmitLT_RR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], regD[C]);
			cc.setl(result);
		});
	}

	void EmitLT_RK()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], konstd[C]);
			cc.setl(result);
		});
	}

	void EmitLT_KR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			auto tmp = cc.newIntPtr();
			cc.mov(tmp, ToMemAddress(&konstd[B]));
			cc.cmp(asmjit::x86::ptr(tmp), regD[C]);
			cc.setl(result);
		});
	}

	void EmitLE_RR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], regD[C]);
			cc.setle(result);
		});
	}

	void EmitLE_RK()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], konstd[C]);
			cc.setle(result);
		});
	}

	void EmitLE_KR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			auto tmp = cc.newIntPtr();
			cc.mov(tmp, ToMemAddress(&konstd[B]));
			cc.cmp(asmjit::x86::ptr(tmp), regD[C]);
			cc.setle(result);
		});
	}

	void EmitLTU_RR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], regD[C]);
			cc.setb(result);
		});
	}

	void EmitLTU_RK()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], konstd[C]);
			cc.setb(result);
		});
	}

	void EmitLTU_KR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			auto tmp = cc.newIntPtr();
			cc.mov(tmp, ToMemAddress(&konstd[B]));
			cc.cmp(asmjit::x86::ptr(tmp), regD[C]);
			cc.setb(result);
		});
	}

	void EmitLEU_RR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], regD[C]);
			cc.setbe(result);
		});
	}

	void EmitLEU_RK()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regD[B], konstd[C]);
			cc.setbe(result);
		});
	}

	void EmitLEU_KR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			auto tmp = cc.newIntPtr();
			cc.mov(tmp, ToMemAddress(&konstd[C]));
			cc.cmp(asmjit::x86::ptr(tmp), regD[B]);
			cc.setbe(result);
		});
	}

	// Double-precision floating point math.

	void EmitADDF_RR()
	{
		auto rc = CheckRegF(C, A);
		cc.movsd(regF[A], regF[B]);
		cc.addsd(regF[A], rc);
	}

	void EmitADDF_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[A], regF[B]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.addsd(regF[A], asmjit::x86::qword_ptr(tmp));
	}

	void EmitSUBF_RR()
	{
		auto rc = CheckRegF(C, A);
		cc.movsd(regF[A], regF[B]);
		cc.subsd(regF[A], rc);
	}

	void EmitSUBF_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[A], regF[B]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.subsd(regF[A], asmjit::x86::qword_ptr(tmp));
	}

	void EmitSUBF_KR()
	{
		auto rc = CheckRegF(C, A);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&konstf[B]));
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.subsd(regF[A], rc);
	}

	void EmitMULF_RR()
	{
		auto rc = CheckRegF(C, A);
		cc.movsd(regF[A], regF[B]);
		cc.mulsd(regF[A], rc);
	}

	void EmitMULF_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[A], regF[B]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
	}

	void EmitDIVF_RR()
	{
		auto label = cc.newLabel();
		cc.ptest(regF[C], regF[C]);
		cc.jne(label);
		EmitThrowException(X_DIVISION_BY_ZERO);
		cc.bind(label);
		auto rc = CheckRegF(C, A);
		cc.movsd(regF[A], regF[B]);
		cc.divsd(regF[A], rc);
	}

	void EmitDIVF_RK()
	{
		if (konstf[C] == 0.)
		{
			EmitThrowException(X_DIVISION_BY_ZERO);
		}
		else
		{
			auto tmp = cc.newIntPtr();
			cc.movsd(regF[A], regF[B]);
			cc.mov(tmp, ToMemAddress(&konstf[C]));
			cc.divsd(regF[A], asmjit::x86::qword_ptr(tmp));
		}
	}

	void EmitDIVF_KR()
	{
		auto rc = CheckRegF(C, A);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&konstf[B]));
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.divsd(regF[A], rc);
	}

	void EmitMODF_RR()
	{
		using namespace asmjit;
		typedef double(*FuncPtr)(double, double);

		auto label = cc.newLabel();
		cc.ptest(regF[C], regF[C]);
		cc.jne(label);
		EmitThrowException(X_DIVISION_BY_ZERO);
		cc.bind(label);

		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](double a, double b) -> double
		{
			return a - floor(a / b) * b;
		}))), FuncSignature2<double, double, double>());
		call->setRet(0, regF[A]);
		call->setArg(0, regF[B]);
		call->setArg(1, regF[C]);
	}

	void EmitMODF_RK()
	{
		using namespace asmjit;
		typedef double(*FuncPtr)(double, double);

		auto label = cc.newLabel();
		cc.ptest(regF[C], regF[C]);
		cc.jne(label);
		EmitThrowException(X_DIVISION_BY_ZERO);
		cc.bind(label);

		auto tmp = cc.newXmm();
		cc.movsd(tmp, x86::ptr(ToMemAddress(&konstf[C])));

		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](double a, double b) -> double {
			return a - floor(a / b) * b;
		}))), FuncSignature2<double, double, double>());
		call->setRet(0, regF[A]);
		call->setArg(0, regF[B]);
		call->setArg(1, tmp);
	}

	void EmitMODF_KR()
	{
		using namespace asmjit;
		typedef double(*FuncPtr)(double, double);

		auto label = cc.newLabel();
		cc.ptest(regF[C], regF[C]);
		cc.jne(label);
		EmitThrowException(X_DIVISION_BY_ZERO);
		cc.bind(label);

		auto tmp = cc.newXmm();
		cc.movsd(tmp, x86::ptr(ToMemAddress(&konstf[B])));

		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](double a, double b) -> double {
			return a - floor(a / b) * b;
		}))), FuncSignature2<double, double, double>());
		call->setRet(0, regF[A]);
		call->setArg(0, tmp);
		call->setArg(1, regF[C]);
	}

	void EmitPOWF_RR()
	{
		using namespace asmjit;
		typedef double(*FuncPtr)(double, double);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(g_pow))), FuncSignature2<double, double, double>());
		call->setRet(0, regF[A]);
		call->setArg(0, regF[B]);
		call->setArg(1, regF[C]);
	}

	void EmitPOWF_RK()
	{
		auto tmp = cc.newIntPtr();
		auto tmp2 = cc.newXmm();
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.movsd(tmp2, asmjit::x86::qword_ptr(tmp));

		using namespace asmjit;
		typedef double(*FuncPtr)(double, double);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(g_pow))), FuncSignature2<double, double, double>());
		call->setRet(0, regF[A]);
		call->setArg(0, regF[B]);
		call->setArg(1, tmp2);
	}

	void EmitPOWF_KR()
	{
		auto tmp = cc.newIntPtr();
		auto tmp2 = cc.newXmm();
		cc.mov(tmp, ToMemAddress(&konstf[B]));
		cc.movsd(tmp2, asmjit::x86::qword_ptr(tmp));

		using namespace asmjit;
		typedef double(*FuncPtr)(double, double);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(g_pow))), FuncSignature2<double, double, double>());
		call->setRet(0, regF[A]);
		call->setArg(0, tmp2);
		call->setArg(1, regF[C]);
	}

	void EmitMINF_RR()
	{
		auto rc = CheckRegF(C, A);
		cc.movsd(regF[A], regF[B]);
		cc.minsd(regF[A], rc);
	}

	void EmitMINF_RK()
	{
		auto rb = CheckRegF(B, A);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.minsd(regF[A], rb);
	}
	
	void EmitMAXF_RR()
	{
		auto rc = CheckRegF(C, A);
		cc.movsd(regF[A], regF[B]);
		cc.maxsd(regF[A], rc);
	}

	void EmitMAXF_RK()
	{
		auto rb = CheckRegF(B, A);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.movsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.maxsd(regF[A], rb);
	}
	
	void EmitATAN2()
	{
		using namespace asmjit;
		typedef double(*FuncPtr)(double, double);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(g_atan2))), FuncSignature2<double, double, double>());
		call->setRet(0, regF[A]);
		call->setArg(0, regF[B]);
		call->setArg(1, regF[C]);

		static const double constant = 180 / M_PI;
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&constant));
		cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
	}

	void EmitFLOP()
	{
		if (C == FLOP_NEG)
		{
			auto mask = cc.newDoubleConst(asmjit::kConstScopeLocal, -0.0);
			auto maskXmm = cc.newXmmSd();
			cc.movsd(maskXmm, mask);
			cc.movsd(regF[A], regF[B]);
			cc.xorpd(regF[A], maskXmm);
		}
		else
		{
			auto v = cc.newXmm();
			cc.movsd(v, regF[B]);

			if (C == FLOP_TAN_DEG)
			{
				static const double constant = M_PI / 180;
				auto tmp = cc.newIntPtr();
				cc.mov(tmp, ToMemAddress(&constant));
				cc.mulsd(v, asmjit::x86::qword_ptr(tmp));
			}

			typedef double(*FuncPtr)(double);
			FuncPtr func = nullptr;
			switch (C)
			{
			default: I_FatalError("Unknown OP_FLOP subfunction");
			case FLOP_ABS:		func = fabs; break;
			case FLOP_EXP:		func = g_exp; break;
			case FLOP_LOG:		func = g_log; break;
			case FLOP_LOG10:	func = g_log10; break;
			case FLOP_SQRT:		func = g_sqrt; break;
			case FLOP_CEIL:		func = ceil; break;
			case FLOP_FLOOR:	func = floor; break;
			case FLOP_ACOS:		func = g_acos; break;
			case FLOP_ASIN:		func = g_asin; break;
			case FLOP_ATAN:		func = g_atan; break;
			case FLOP_COS:		func = g_cos; break;
			case FLOP_SIN:		func = g_sin; break;
			case FLOP_TAN:		func = g_tan; break;
			case FLOP_ACOS_DEG:	func = g_acos; break;
			case FLOP_ASIN_DEG:	func = g_asin; break;
			case FLOP_ATAN_DEG:	func = g_atan; break;
			case FLOP_COS_DEG:	func = g_cosdeg; break;
			case FLOP_SIN_DEG:	func = g_sindeg; break;
			case FLOP_TAN_DEG:	func = g_tan; break;
			case FLOP_COSH:		func = g_cosh; break;
			case FLOP_SINH:		func = g_sinh; break;
			case FLOP_TANH:		func = g_tanh; break;
			}

			auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(func)), asmjit::FuncSignature1<double, double>());
			call->setRet(0, regF[A]);
			call->setArg(0, v);

			if (C == FLOP_ACOS_DEG || C == FLOP_ASIN_DEG || C == FLOP_ATAN_DEG)
			{
				static const double constant = 180 / M_PI;
				auto tmp = cc.newIntPtr();
				cc.mov(tmp, ToMemAddress(&constant));
				cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
			}
		}
	}

	void EmitEQF_R()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			bool approx = static_cast<bool>(A & CMP_APPROX);
			if (!approx)
			{
				auto tmp = cc.newInt32();
				cc.ucomisd(regF[B], regF[C]);
				cc.sete(result);
				cc.setnp(tmp);
				cc.and_(result, tmp);
				cc.and_(result, 1);
			}
			else
			{
				auto tmp = cc.newXmmSd();

				const int64_t absMaskInt = 0x7FFFFFFFFFFFFFFF;
				auto absMask = cc.newDoubleConst(asmjit::kConstScopeLocal, reinterpret_cast<const double&>(absMaskInt));
				auto absMaskXmm = cc.newXmmPd();

				auto epsilon = cc.newDoubleConst(asmjit::kConstScopeLocal, VM_EPSILON);
				auto epsilonXmm = cc.newXmmSd();

				cc.movsd(tmp, regF[B]);
				cc.subsd(tmp, regF[C]);
				cc.movsd(absMaskXmm, absMask);
				cc.andpd(tmp, absMaskXmm);
				cc.movsd(epsilonXmm, epsilon);
				cc.ucomisd(epsilonXmm, tmp);
				cc.seta(result);
				cc.and_(result, 1);
			}
		});
	}

	void EmitEQF_K()
	{
		using namespace asmjit;
		EmitComparisonOpcode([&](X86Gp& result) {
			bool approx = static_cast<bool>(A & CMP_APPROX);
			if (!approx) {
				auto konstTmp = cc.newIntPtr();
				auto parityTmp = cc.newInt32();
				cc.mov(konstTmp, ToMemAddress(&konstf[C]));

				cc.ucomisd(regF[B], x86::qword_ptr(konstTmp));
				cc.sete(result);
				cc.setnp(parityTmp);
				cc.and_(result, parityTmp);
				cc.and_(result, 1);
			}
			else {
				auto konstTmp = cc.newIntPtr();
				auto subTmp = cc.newXmmSd();

				const int64_t absMaskInt = 0x7FFFFFFFFFFFFFFF;
				auto absMask = cc.newDoubleConst(kConstScopeLocal, reinterpret_cast<const double&>(absMaskInt));
				auto absMaskXmm = cc.newXmmPd();

				auto epsilon = cc.newDoubleConst(kConstScopeLocal, VM_EPSILON);
				auto epsilonXmm = cc.newXmmSd();

				cc.mov(konstTmp, ToMemAddress(&konstf[C]));

				cc.movsd(subTmp, regF[B]);
				cc.subsd(subTmp, x86::qword_ptr(konstTmp));
				cc.movsd(absMaskXmm, absMask);
				cc.andpd(subTmp, absMaskXmm);
				cc.movsd(epsilonXmm, epsilon);
				cc.ucomisd(epsilonXmm, subTmp);
				cc.seta(result);
				cc.and_(result, 1);
			}
		});
	}

	void EmitLTF_RR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			if (static_cast<bool>(A & CMP_APPROX)) I_FatalError("CMP_APPROX not implemented for LTF_RR.\n");

			cc.ucomisd(regF[C], regF[B]);
			cc.seta(result);
			cc.and_(result, 1);
		});
	}

	void EmitLTF_RK()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			if (static_cast<bool>(A & CMP_APPROX)) I_FatalError("CMP_APPROX not implemented for LTF_RK.\n");

			auto constTmp = cc.newIntPtr();
			auto xmmTmp = cc.newXmmSd();
			cc.mov(constTmp, ToMemAddress(&konstf[C]));
			cc.movsd(xmmTmp, asmjit::x86::qword_ptr(constTmp));

			cc.ucomisd(xmmTmp, regF[B]);
			cc.seta(result);
			cc.and_(result, 1);
		});
	}

	void EmitLTF_KR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			if (static_cast<bool>(A & CMP_APPROX)) I_FatalError("CMP_APPROX not implemented for LTF_KR.\n");

			auto tmp = cc.newIntPtr();
			cc.mov(tmp, ToMemAddress(&konstf[B]));

			cc.ucomisd(regF[C], asmjit::x86::qword_ptr(tmp));
			cc.seta(result);
			cc.and_(result, 1);
		});
	}

	void EmitLEF_RR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			if (static_cast<bool>(A & CMP_APPROX)) I_FatalError("CMP_APPROX not implemented for LEF_RR.\n");

			cc.ucomisd(regF[C], regF[B]);
			cc.setae(result);
			cc.and_(result, 1);
		});
	}

	void EmitLEF_RK()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			if (static_cast<bool>(A & CMP_APPROX)) I_FatalError("CMP_APPROX not implemented for LEF_RK.\n");

			auto constTmp = cc.newIntPtr();
			auto xmmTmp = cc.newXmmSd();
			cc.mov(constTmp, ToMemAddress(&konstf[C]));
			cc.movsd(xmmTmp, asmjit::x86::qword_ptr(constTmp));

			cc.ucomisd(xmmTmp, regF[B]);
			cc.setae(result);
			cc.and_(result, 1);
		});
	}

	void EmitLEF_KR()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			if (static_cast<bool>(A & CMP_APPROX)) I_FatalError("CMP_APPROX not implemented for LEF_KR.\n");

			auto tmp = cc.newIntPtr();
			cc.mov(tmp, ToMemAddress(&konstf[B]));

			cc.ucomisd(regF[C], asmjit::x86::qword_ptr(tmp));
			cc.setae(result);
			cc.and_(result, 1);
		});
	}

	// Vector math. (2D)

	void EmitNEGV2()
	{
		auto mask = cc.newDoubleConst(asmjit::kConstScopeLocal, -0.0);
		auto maskXmm = cc.newXmmSd();
		cc.movsd(maskXmm, mask);
		cc.movsd(regF[A], regF[B]);
		cc.xorpd(regF[A], maskXmm);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.xorpd(regF[A + 1], maskXmm);
	}

	void EmitADDV2_RR()
	{
		auto rc0 = CheckRegF(C, A);
		auto rc1 = CheckRegF(C + 1, A + 1);
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.addsd(regF[A], rc0);
		cc.addsd(regF[A + 1], rc1);
	}

	void EmitSUBV2_RR()
	{
		auto rc0 = CheckRegF(C, A);
		auto rc1 = CheckRegF(C + 1, A + 1);
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.subsd(regF[A], rc0);
		cc.subsd(regF[A + 1], rc1);
	}

	void EmitDOTV2_RR()
	{
		auto rc0 = CheckRegF(C, A);
		auto rc1 = CheckRegF(C + 1, A);
		auto tmp = cc.newXmmSd();
		cc.movsd(regF[A], regF[B]);
		cc.mulsd(regF[A], rc0);
		cc.movsd(tmp, regF[B + 1]);
		cc.mulsd(tmp, rc1);
		cc.addsd(regF[A], tmp);
	}

	void EmitMULVF2_RR()
	{
		auto rc = CheckRegF(C, A, A + 1);
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.mulsd(regF[A], rc);
		cc.mulsd(regF[A + 1], rc);
	}

	void EmitMULVF2_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.mulsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
	}

	void EmitDIVVF2_RR()
	{
		auto rc = CheckRegF(C, A, A + 1);
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.divsd(regF[A], rc);
		cc.divsd(regF[A + 1], rc);
	}

	void EmitDIVVF2_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.divsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.divsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
	}

	void EmitLENV2()
	{
		auto rb0 = CheckRegF(B, A);
		auto rb1 = CheckRegF(B + 1, A);
		auto tmp = cc.newXmmSd();
		cc.movsd(regF[A], regF[B]);
		cc.mulsd(regF[A], rb0);
		cc.movsd(tmp, rb1);
		cc.mulsd(tmp, rb1);
		cc.addsd(regF[A], tmp);
		CallSqrt(regF[A], regF[A]);
	}

	void EmitEQV2_R()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			if (static_cast<bool>(A & CMP_APPROX)) I_FatalError("CMP_APPROX not implemented for EQV2_R.\n");

			auto parityTmp = cc.newInt32();
			auto result1Tmp = cc.newInt32();
			cc.ucomisd(regF[B], regF[C]);
			cc.sete(result);
			cc.setnp(parityTmp);
			cc.and_(result, parityTmp);
			cc.and_(result, 1);

			cc.ucomisd(regF[B + 1], regF[C + 1]);
			cc.sete(result1Tmp);
			cc.setnp(parityTmp);
			cc.and_(result1Tmp, parityTmp);
			cc.and_(result1Tmp, 1);

			cc.and_(result, result1Tmp);
		});
	}

	void EmitEQV2_K()
	{
		I_FatalError("EQV2_K is not used.");
	}

	// Vector math. (3D)

	void EmitNEGV3()
	{
		auto mask = cc.newDoubleConst(asmjit::kConstScopeLocal, -0.0);
		auto maskXmm = cc.newXmmSd();
		cc.movsd(maskXmm, mask);
		cc.movsd(regF[A], regF[B]);
		cc.xorpd(regF[A], maskXmm);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.xorpd(regF[A + 1], maskXmm);
		cc.movsd(regF[A + 2], regF[B + 2]);
		cc.xorpd(regF[A + 2], maskXmm);
	}

	void EmitADDV3_RR()
	{
		auto rc0 = CheckRegF(C, A);
		auto rc1 = CheckRegF(C + 1, A + 1);
		auto rc2 = CheckRegF(C + 2, A + 2);
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.movsd(regF[A + 2], regF[B + 2]);
		cc.addsd(regF[A], rc0);
		cc.addsd(regF[A + 1], rc1);
		cc.addsd(regF[A + 2], rc2);
	}

	void EmitSUBV3_RR()
	{
		auto rc0 = CheckRegF(C, A);
		auto rc1 = CheckRegF(C + 1, A + 1);
		auto rc2 = CheckRegF(C + 2, A + 2);
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.movsd(regF[A + 2], regF[B + 2]);
		cc.subsd(regF[A], rc0);
		cc.subsd(regF[A + 1], rc1);
		cc.subsd(regF[A + 2], rc2);
	}

	void EmitDOTV3_RR()
	{
		auto rb1 = CheckRegF(B + 1, A);
		auto rb2 = CheckRegF(B + 2, A);
		auto rc0 = CheckRegF(C, A);
		auto rc1 = CheckRegF(C + 1, A);
		auto rc2 = CheckRegF(C + 2, A);
		auto tmp = cc.newXmmSd();
		cc.movsd(regF[A], regF[B]);
		cc.mulsd(regF[A], rc0);
		cc.movsd(tmp, rb1);
		cc.mulsd(tmp, rc1);
		cc.addsd(regF[A], tmp);
		cc.movsd(tmp, rb2);
		cc.mulsd(tmp, rc2);
		cc.addsd(regF[A], tmp);
	}

	void EmitCROSSV_RR()
	{
		auto tmp = cc.newXmmSd();

		auto a0 = CheckRegF(B, A);
		auto a1 = CheckRegF(B + 1, A + 1);
		auto a2 = CheckRegF(B + 2, A + 2);
		auto b0 = CheckRegF(C, A);
		auto b1 = CheckRegF(C + 1, A + 1);
		auto b2 = CheckRegF(C + 2, A + 2);

		// r0 = a1b2 - a2b1
		cc.movsd(regF[A], a1);
		cc.mulsd(regF[A], b2);
		cc.movsd(tmp, a2);
		cc.mulsd(tmp, b1);
		cc.subsd(regF[A], tmp);

		// r1 = a2b0 - a0b2
		cc.movsd(regF[A + 1], a2);
		cc.mulsd(regF[A + 1], b0);
		cc.movsd(tmp, a0);
		cc.mulsd(tmp, b2);
		cc.subsd(regF[A + 1], tmp);

		// r2 = a0b1 - a1b0
		cc.movsd(regF[A + 2], a0);
		cc.mulsd(regF[A + 2], b1);
		cc.movsd(tmp, a1);
		cc.mulsd(tmp, b0);
		cc.subsd(regF[A + 2], tmp);
	}

	void EmitMULVF3_RR()
	{
		auto rc = CheckRegF(C, A, A + 1, A + 2);
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.movsd(regF[A + 2], regF[B + 2]);
		cc.mulsd(regF[A], rc);
		cc.mulsd(regF[A + 1], rc);
		cc.mulsd(regF[A + 2], rc);
	}

	void EmitMULVF3_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.movsd(regF[A + 2], regF[B + 2]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.mulsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.mulsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
		cc.mulsd(regF[A + 2], asmjit::x86::qword_ptr(tmp));
	}

	void EmitDIVVF3_RR()
	{
		auto rc = CheckRegF(C, A, A + 1, A + 2);
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.movsd(regF[A + 2], regF[B + 2]);
		cc.divsd(regF[A], rc);
		cc.divsd(regF[A + 1], rc);
		cc.divsd(regF[A + 2], rc);
	}

	void EmitDIVVF3_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[A], regF[B]);
		cc.movsd(regF[A + 1], regF[B + 1]);
		cc.movsd(regF[A + 2], regF[B + 2]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.divsd(regF[A], asmjit::x86::qword_ptr(tmp));
		cc.divsd(regF[A + 1], asmjit::x86::qword_ptr(tmp));
		cc.divsd(regF[A + 2], asmjit::x86::qword_ptr(tmp));
	}

	void EmitLENV3()
	{
		auto rb1 = CheckRegF(B + 1, A);
		auto rb2 = CheckRegF(B + 2, A);
		auto tmp = cc.newXmmSd();
		cc.movsd(regF[A], regF[B]);
		cc.mulsd(regF[A], regF[B]);
		cc.movsd(tmp, rb1);
		cc.mulsd(tmp, rb1);
		cc.addsd(regF[A], tmp);
		cc.movsd(tmp, rb2);
		cc.mulsd(tmp, rb2);
		cc.addsd(regF[A], tmp);
		CallSqrt(regF[A], regF[A]);
	}

	void EmitEQV3_R()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			if (static_cast<bool>(A & CMP_APPROX)) I_FatalError("CMP_APPROX not implemented for EQV3_R.\n");

			auto parityTmp = cc.newInt32();
			auto result1Tmp = cc.newInt32();
			cc.ucomisd(regF[B], regF[C]);
			cc.sete(result);
			cc.setnp(parityTmp);
			cc.and_(result, parityTmp);
			cc.and_(result, 1);

			cc.ucomisd(regF[B + 1], regF[C + 1]);
			cc.sete(result1Tmp);
			cc.setnp(parityTmp);
			cc.and_(result1Tmp, parityTmp);
			cc.and_(result1Tmp, 1);

			cc.and_(result, result1Tmp);

			cc.ucomisd(regF[B + 2], regF[C + 2]);
			cc.sete(result1Tmp);
			cc.setnp(parityTmp);
			cc.and_(result1Tmp, parityTmp);
			cc.and_(result1Tmp, 1);

			cc.and_(result, result1Tmp);
		});
	}
	
	void EmitEQV3_K()
	{
		I_FatalError("EQV3_K is not used.");
	}

	// Pointer math.

	void EmitADDA_RR()
	{
		auto tmp = cc.newIntPtr();
		auto label = cc.newLabel();

		cc.mov(tmp, regA[B]);

		// Check if zero, the first operand is zero, if it is, don't add.
		cc.cmp(tmp, 0);
		cc.je(label);

		auto tmpptr = cc.newIntPtr();
		cc.mov(tmpptr, regD[C]);
		cc.add(tmp, tmpptr);

		cc.bind(label);
		cc.mov(regA[A], tmp);
	}

	void EmitADDA_RK()
	{
		auto tmp = cc.newIntPtr();
		auto label = cc.newLabel();

		cc.mov(tmp, regA[B]);

		// Check if zero, the first operand is zero, if it is, don't add.
		cc.cmp(tmp, 0);
		cc.je(label);

		cc.add(tmp, konstd[C]);

		cc.bind(label);
		cc.mov(regA[A], tmp);
	}

	void EmitSUBA()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.sub(tmp, regD[C]);
		cc.mov(regA[A], tmp);
	}

	void EmitEQA_R()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			cc.cmp(regA[B], regA[C]);
			cc.sete(result);
		});
	}

	void EmitEQA_K()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
			auto tmp = cc.newIntPtr();
			cc.mov(tmp, ToMemAddress(konsta[C].v));
			cc.cmp(regA[B], tmp);
			cc.sete(result);
		});
	}

	void Setup()
	{
		using namespace asmjit;

		FString funcname;
		funcname.Format("Function: %s", sfunc->PrintableName.GetChars());
		cc.comment(funcname.GetChars(), funcname.Len());

		stack = cc.newIntPtr("stack"); // VMFrameStack *stack
		vmregs = cc.newIntPtr("vmregs"); // void *vmregs
		ret = cc.newIntPtr("ret"); // VMReturn *ret
		numret = cc.newInt32("numret"); // int numret
		exceptInfo = cc.newIntPtr("exceptinfo"); // JitExceptionInfo *exceptInfo

		cc.addFunc(FuncSignature5<int, void *, void *, void *, int, void *>());
		cc.setArg(0, stack);
		cc.setArg(1, vmregs);
		cc.setArg(2, ret);
		cc.setArg(3, numret);
		cc.setArg(4, exceptInfo);

		konstd = sfunc->KonstD;
		konstf = sfunc->KonstF;
		konsts = sfunc->KonstS;
		konsta = sfunc->KonstA;

		regD.Resize(sfunc->NumRegD);
		regF.Resize(sfunc->NumRegF);
		regA.Resize(sfunc->NumRegA);
		//regS.Resize(sfunc->NumRegS);

		X86Gp initreg = cc.newIntPtr();
		if (sfunc->NumRegD > 0)
		{
			cc.mov(initreg, x86::ptr(vmregs, 0));
			for (int i = 0; i < sfunc->NumRegD; i++)
			{
				FString regname;
				regname.Format("regD%d", i);
				regD[i] = cc.newInt32(regname.GetChars());
				cc.mov(regD[i], x86::dword_ptr(initreg, i * sizeof(int32_t)));
			}
		}
		if (sfunc->NumRegF > 0)
		{
			cc.mov(initreg, x86::ptr(vmregs, sizeof(void*)));
			for (int i = 0; i < sfunc->NumRegF; i++)
			{
				FString regname;
				regname.Format("regF%d", i);
				regF[i] = cc.newXmmSd(regname.GetChars());
				cc.movsd(regF[i], x86::qword_ptr(initreg, i * sizeof(double)));
			}
		}
		/*if (sfunc->NumRegS > 0)
		{
			cc.mov(initreg, x86::ptr(vmregs, sizeof(void*) * 2));
			for (int i = 0; i < sfunc->NumRegS; i++)
			{
				FString regname;
				regname.Format("regS%d", i);
				regS[i] = cc.newGpd(regname.GetChars());
			}
		}*/
		if (sfunc->NumRegA > 0)
		{
			cc.mov(initreg, x86::ptr(vmregs, sizeof(void*) * 3));
			for (int i = 0; i < sfunc->NumRegA; i++)
			{
				FString regname;
				regname.Format("regA%d", i);
				regA[i] = cc.newIntPtr(regname.GetChars());
				cc.mov(regA[i], x86::ptr(initreg, i * sizeof(void*)));
			}
		}

		int size = sfunc->CodeSize;
		labels.Resize(size);
		for (int i = 0; i < size; i++) labels[i] = cc.newLabel();
	}

	template <typename Func>
	void EmitComparisonOpcode(Func compFunc)
	{
		using namespace asmjit;

		int i = (int)(ptrdiff_t)(pc - sfunc->Code);

		auto tmp = cc.newInt32();
		cc.xor_(tmp, tmp);
		compFunc(tmp);

		bool check = static_cast<bool>(A & CMP_CHECK);

		auto jmplabel = labels[i + 2 + JMPOFS(pc + 1)];

		cc.test(tmp, tmp);
		if (check) cc.jne(jmplabel);
		else       cc.je(jmplabel);

		pc++; // This instruction uses two instruction slots - skip the next one
	}

	static int64_t ToMemAddress(const void *d)
	{
		return (int64_t)(ptrdiff_t)d;
	}

	void CallSqrt(const asmjit::X86Xmm &a, const asmjit::X86Xmm &b)
	{
		using namespace asmjit;
		typedef double(*FuncPtr)(double);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(g_sqrt))), FuncSignature1<double, double>());
		call->setRet(0, a);
		call->setArg(0, b);
	}

	void EmitNullPointerThrow(int index, EVMAbortException reason)
	{
		auto label = cc.newLabel();
		cc.test(regA[index], regA[index]);
		cc.jne(label);
		EmitThrowException(reason);
		cc.bind(label);
	}

	void EmitThrowException(EVMAbortException reason)
	{
		using namespace asmjit;

		// Update JitExceptionInfo struct
		cc.mov(x86::dword_ptr(exceptInfo, 0 * 4), (int32_t)reason);
#ifdef ASMJIT_ARCH_X64
		cc.mov(x86::qword_ptr(exceptInfo, 4 * 4), ToMemAddress(pc));
#else
		cc.mov(x86::dword_ptr(exceptInfo, 4 * 4), ToMemAddress(pc));
#endif

		// Return from function
		X86Gp vReg = cc.newInt32();
		cc.mov(vReg, 0);
		cc.ret(vReg);
	}

	void EmitThrowException(EVMAbortException reason, asmjit::X86Gp arg1)
	{
		using namespace asmjit;

		// Update JitExceptionInfo struct
		cc.mov(x86::dword_ptr(exceptInfo, 0 * 4), (int32_t)reason);
		cc.mov(x86::dword_ptr(exceptInfo, 1 * 4), arg1);
#ifdef ASMJIT_ARCH_X64
		cc.mov(x86::qword_ptr(exceptInfo, 4 * 4), ToMemAddress(pc));
#else
		cc.mov(x86::dword_ptr(exceptInfo, 4 * 4), ToMemAddress(pc));
#endif

		// Return from function
		X86Gp vReg = cc.newInt32();
		cc.mov(vReg, 0);
		cc.ret(vReg);
	}

	asmjit::X86Gp CheckRegD(int r0, int r1)
	{
		if (r0 != r1)
		{
			return regD[r0];
		}
		else
		{
			auto copy = cc.newInt32();
			cc.mov(copy, regD[r0]);
			return copy;
		}
	}

	asmjit::X86Xmm CheckRegF(int r0, int r1)
	{
		if (r0 != r1)
		{
			return regF[r0];
		}
		else
		{
			auto copy = cc.newXmm();
			cc.movsd(copy, regF[r0]);
			return copy;
		}
	}

	asmjit::X86Xmm CheckRegF(int r0, int r1, int r2)
	{
		if (r0 != r1 && r0 != r2)
		{
			return regF[r0];
		}
		else
		{
			auto copy = cc.newXmm();
			cc.movsd(copy, regF[r0]);
			return copy;
		}
	}

	asmjit::X86Xmm CheckRegF(int r0, int r1, int r2, int r3)
	{
		if (r0 != r1 && r0 != r2 && r0 != r3)
		{
			return regF[r0];
		}
		else
		{
			auto copy = cc.newXmm();
			cc.movsd(copy, regF[r0]);
			return copy;
		}
	}

	asmjit::X86Gp CheckRegA(int r0, int r1)
	{
		if (r0 != r1)
		{
			return regA[r0];
		}
		else
		{
			auto copy = cc.newIntPtr();
			cc.mov(copy, regA[r0]);
			return copy;
		}
	}

	asmjit::X86Compiler cc;
	VMScriptFunction *sfunc;

	asmjit::X86Gp stack;
	asmjit::X86Gp vmregs;
	asmjit::X86Gp ret;
	asmjit::X86Gp numret;
	asmjit::X86Gp exceptInfo;

	const int *konstd;
	const double *konstf;
	const FString *konsts;
	const FVoidObj *konsta;

	TArray<asmjit::X86Gp> regD;
	TArray<asmjit::X86Xmm> regF;
	TArray<asmjit::X86Gp> regA;
	//TArray<asmjit::X86Gp> regS;

	TArray<asmjit::Label> labels;

	const VMOP *pc;
	VM_UBYTE op;
};

class AsmJitException : public std::exception
{
public:
	AsmJitException(asmjit::Error error, const char *message) noexcept : error(error), message(message)
	{
	}

	const char* what() const noexcept override
	{
		return message.GetChars();
	}

	asmjit::Error error;
	FString message;
};

class ThrowingErrorHandler : public asmjit::ErrorHandler
{
public:
	bool handleError(asmjit::Error err, const char *message, asmjit::CodeEmitter *origin) override
	{
		throw AsmJitException(err, message);
	}
};

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
	if (strcmp(sfunc->Name.GetChars(), "CanPickup") != 0)
		return nullptr;
#else
	if (!JitCompiler::CanJit(sfunc))
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
