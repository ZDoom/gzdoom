
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
#define KC				(konstd[C])
#define RC				(regD[C])
#define PA				(regA[A])
#define PB				(regA[B])

#define ASSERTD(x)		assert((unsigned)(x) < sfunc->NumRegD)
#define ASSERTF(x)		assert((unsigned)(x) < sfunc->NumRegF)
#define ASSERTA(x)		assert((unsigned)(x) < sfunc->NumRegA)
#define ASSERTS(x)		assert((unsigned)(x) < sfunc->NumRegS)
#define ASSERTKD(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstD)
#define ASSERTKF(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstF)
#define ASSERTKA(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstA)
#define ASSERTKS(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstS)

// [pbeta] TODO: VM aborts
#define NULL_POINTER_CHECK(a,o,x) 
#define BINARY_OP_INT(op,out,r,l) \
{ \
	auto tmp = cc.newInt32(); \
	cc.mov(tmp, r); \
	cc.op(tmp, l); \
	cc.mov(out, tmp); \
}

class JitCompiler
{
public:
	JitCompiler(asmjit::CodeHolder *code, VMScriptFunction *sfunc) : cc(code), sfunc(sfunc) { }

	void Codegen()
	{
		using namespace asmjit;

		Setup();

		int size = sfunc->CodeSize;
		for (int i = 0; i < size; i++)
		{
			pc = sfunc->Code + i;
			op = pc->op;
			a = pc->a;

			cc.bind(labels[i]);

			EmitFuncPtr opcodeFunc = GetOpcodeEmitFunc(op);
			if (!opcodeFunc)
				I_FatalError("JIT error: Unknown VM opcode %d\n", op);
			(this->*opcodeFunc)();
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
			// EMIT_OP(LKP_R);
			// EMIT_OP(LFP);
			// EMIT_OP(META);
			// EMIT_OP(CLSS);
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
			// EMIT_OP(LO);
			// EMIT_OP(LO_R);
			// EMIT_OP(LP);
			// EMIT_OP(LP_R);
			EMIT_OP(LV2);
			EMIT_OP(LV2_R);
			EMIT_OP(LV3);
			EMIT_OP(LV3_R);
			//EMIT_OP(LCS);
			//EMIT_OP(LCS_R);
			//EMIT_OP(LBIT);
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
			//EMIT_OP(SO);
			//EMIT_OP(SO_R);
			//EMIT_OP(SP);
			//EMIT_OP(SP_R);
			EMIT_OP(SV2);
			EMIT_OP(SV2_R);
			EMIT_OP(SV3);
			EMIT_OP(SV3_R);
			// EMIT_OP(SBIT);
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
			//EMIT_OP(VTBL);
			//EMIT_OP(SCOPE);
			//EMIT_OP(TAIL);
			//EMIT_OP(TAIL_K);
			EMIT_OP(RESULT);
			EMIT_OP(RET);
			EMIT_OP(RETI);
			//EMIT_OP(NEW);
			//EMIT_OP(NEW_K);
			//EMIT_OP(THROW);
			//EMIT_OP(BOUND);
			//EMIT_OP(BOUND_K);
			//EMIT_OP(BOUND_R);
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
			// EMIT_OP(MODF_RR);
			// EMIT_OP(MODF_RK);
			// EMIT_OP(MODF_KR);
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
			// EMIT_OP(EQV2_R);
			// EMIT_OP(EQV2_K);
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
			// EMIT_OP(EQV3_R);
			// EMIT_OP(EQV3_K);
			EMIT_OP(ADDA_RR);
			EMIT_OP(ADDA_RK);
			EMIT_OP(SUBA);
			// EMIT_OP(EQA_R);
			// EMIT_OP(EQA_K);
		}
	}

	void EmitNOP()
	{
		cc.nop();
	}

	// Load constants.

	void EmitLI()
	{
		cc.mov(regD[a], BCs);
	}

	void EmitLK()
	{
		cc.mov(regD[a], konstd[BC]);
	}

	void EmitLKF()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(konstf + BC));
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp));
	}

	/*void EmitLKS() // load string constant
	{
		cc.mov(regS[a], konsts[BC]);
	}*/

	void EmitLKP()
	{
		cc.mov(regA[a], (int64_t)konsta[BC].v);
	}

	void EmitLK_R()
	{
		cc.mov(regD[a], asmjit::x86::ptr(ToMemAddress(konstd), regD[B], 2, C * 4));
	}

	void EmitLKF_R()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(konstf + BC));
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp, regD[B], 3, C * 8));
	}

	/*void EmitLKS_R() // load string constant indexed
	{
		//cc.mov(regS[a], konsts[regD[B] + C]);
	}

	void EmitLKP_R() // load pointer constant indexed
	{
		//cc.mov(b, regD[B] + C);
		//cc.mov(regA[a], konsta[b].v);
	}

	void EmitLFP() // load frame pointer
	{
	}

	void EmitMETA() // load a class's meta data address
	{
	}

	void EmitCLSS() // load a class's descriptor address
	{
	}*/

	// Load from memory. rA = *(rB + rkC)

	void EmitLB()
	{
		NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
		cc.movsx(regD[a], asmjit::x86::byte_ptr(PB, KC));
	}

	void EmitLB_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
		cc.movsx(regD[a], asmjit::x86::byte_ptr(PB, RC));
	}

	void EmitLH()
	{
		NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
		cc.movsx(regD[a], asmjit::x86::word_ptr(PB, KC));
	}

	void EmitLH_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
		cc.movsx(regD[a], asmjit::x86::word_ptr(PB, RC));
	}

	void EmitLW()
	{
		NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
		cc.mov(regD[a], asmjit::x86::dword_ptr(PB, KC));
	}

	void EmitLW_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
		cc.mov(regD[a], asmjit::x86::dword_ptr(PB, RC));
	}

	void EmitLBU()
	{
		NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
		cc.mov(regD[a], asmjit::x86::byte_ptr(PB, KC));
	}

	void EmitLBU_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
		cc.mov(regD[a], asmjit::x86::byte_ptr(PB, RC));
	}

	void EmitLHU()
	{
		NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
		cc.mov(regD[a], asmjit::x86::word_ptr(PB, KC));
	}

	void EmitLHU_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
		cc.mov(regD[a], asmjit::x86::word_ptr(PB, RC));
	}

	void EmitLSP()
	{
		NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
		cc.movss(regF[a], asmjit::x86::dword_ptr(PB, KC));
	}

	void EmitLSP_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
		cc.movss(regF[a], asmjit::x86::dword_ptr(PB, RC));
	}

	void EmitLDP()
	{
		NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
		cc.movsd(regF[a], asmjit::x86::qword_ptr(PB, KC));
	}

	void EmitLDP_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
		cc.movsd(regF[a], asmjit::x86::qword_ptr(PB, RC));
	}

	/*void EmitLS() // load string
	{
	}

	void EmitLS_R()
	{
	}

	void EmitLO() // load object
	{
	}

	void EmitLO_R()
	{
	}

	void EmitLP() // load pointer
	{
	}

	void EmitLP_R()
	{
	}*/

	void EmitLV2()
	{
		NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, PB);
		cc.add(tmp, KC);
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.movsd(regF[a + 1], asmjit::x86::qword_ptr(tmp, 8));
	}

	void EmitLV2_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, PB);
		cc.add(tmp, RC);
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.movsd(regF[a + 1], asmjit::x86::qword_ptr(tmp, 8));
	}

	void EmitLV3()
	{
		NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, PB);
		cc.add(tmp, KC);
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.movsd(regF[a + 1], asmjit::x86::qword_ptr(tmp, 8));
		cc.movsd(regF[a + 2], asmjit::x86::qword_ptr(tmp, 16));
	}

	void EmitLV3_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, PB);
		cc.add(tmp, RC);
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.movsd(regF[a + 1], asmjit::x86::qword_ptr(tmp, 8));
		cc.movsd(regF[a + 2], asmjit::x86::qword_ptr(tmp, 16));
	}

	/*void EmitLCS() // load string from char ptr.
	{
	}

	void EmitLCS_R()
	{
	}

	void EmitLBIT() // rA = !!(*rB & C)  -- *rB is a byte
	{
		NULL_POINTER_CHECK(PB, 0, X_READ_NIL);
		auto tmp = cc.newInt8();
		cc.mov(regD[a], PB);
		cc.and_(regD[a], C);
		cc.test(regD[a], regD[a]);
		cc.sete(tmp);
		cc.movzx(regD[a], tmp);
	}*/

	// Store instructions. *(rA + rkC) = rB

	void EmitSB()
	{
		NULL_POINTER_CHECK(PA, KC, X_WRITE_NIL);
		cc.mov(asmjit::x86::byte_ptr(PA, KC), regD[B].r8Lo());
	}

	void EmitSB_R()
	{
		NULL_POINTER_CHECK(PA, RC, X_WRITE_NIL);
		cc.mov(asmjit::x86::byte_ptr(PA, RC), regD[B].r8Lo());
	}

	void EmitSH()
	{
		NULL_POINTER_CHECK(PA, KC, X_WRITE_NIL);
		cc.mov(asmjit::x86::word_ptr(PA, KC), regD[B].r16());
	}

	void EmitSH_R()
	{
		NULL_POINTER_CHECK(PA, RC, X_WRITE_NIL);
		cc.mov(asmjit::x86::word_ptr(PA, RC), regD[B].r16());
	}

	void EmitSW()
	{
		NULL_POINTER_CHECK(PA, KC, X_WRITE_NIL);
		cc.mov(asmjit::x86::dword_ptr(PA, KC), regD[B]);
	}

	void EmitSW_R()
	{
		NULL_POINTER_CHECK(PA, RC, X_WRITE_NIL);
		cc.mov(asmjit::x86::dword_ptr(PA, RC), regD[B]);
	}

	void EmitSSP()
	{
		NULL_POINTER_CHECK(PB, KC, X_WRITE_NIL);
		cc.movss(asmjit::x86::dword_ptr(PA, KC), regF[B]);
	}

	void EmitSSP_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_WRITE_NIL);
		cc.movss(asmjit::x86::dword_ptr(PA, RC), regF[B]);
	}

	void EmitSDP()
	{
		NULL_POINTER_CHECK(PB, KC, X_WRITE_NIL);
		cc.movsd(asmjit::x86::qword_ptr(PA, KC), regF[B]);
	}

	void EmitSDP_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_WRITE_NIL);
		cc.movsd(asmjit::x86::qword_ptr(PA, RC), regF[B]);
	}

	//void EmitSS() {} // store string
	//void EmitSS_R() {}
	//void EmitSO() {} // store object pointer with write barrier (only needed for non thinkers and non types)
	//void EmitSO_R() {}
	//void EmitSP() {} // store pointer
	//void EmitSP_R() {}

	void EmitSV2()
	{
		NULL_POINTER_CHECK(PB, KC, X_WRITE_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, PB);
		cc.add(tmp, KC);
		cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	}

	void EmitSV2_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_WRITE_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, PB);
		cc.add(tmp, RC);
		cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
	}

	void EmitSV3()
	{
		NULL_POINTER_CHECK(PB, KC, X_WRITE_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, PB);
		cc.add(tmp, KC);
		cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
	}

	void EmitSV3_R()
	{
		NULL_POINTER_CHECK(PB, RC, X_WRITE_NIL);
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, PB);
		cc.add(tmp, RC);
		cc.movsd(asmjit::x86::qword_ptr(tmp), regF[B]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 8), regF[B + 1]);
		cc.movsd(asmjit::x86::qword_ptr(tmp, 16), regF[B + 2]);
	}

	// void EmitSBIT() {} // *rA |= C if rB is true, *rA &= ~C otherwise

	// Move instructions.

	void EmitMOVE()
	{
		cc.mov(regD[a], regD[B]);
	}
	void EmitMOVEF()
	{
		cc.movsd(regF[a], regF[B]);
	}

	// void EmitMOVES() {} // sA = sB

	void EmitMOVEA()
	{
		cc.mov(regA[a], regA[B]);
	}

	void EmitMOVEV2()
	{
		int b = B;
		cc.movsd(regF[a], regF[b]);
		cc.movsd(regF[a + 1], regF[b + 1]);
	}

	void EmitMOVEV3()
	{
		int b = B;
		cc.movsd(regF[a], regF[b]);
		cc.movsd(regF[a + 1], regF[b + 1]);
		cc.movsd(regF[a + 2], regF[b + 2]);
	}

	void EmitCAST()
	{
		int b = B;
		switch (C)
		{
		case CAST_I2F:
			cc.cvtsi2sd(regF[a], regD[b]);
			break;
		case CAST_U2F:
		{
			auto tmp = cc.newInt64();
			cc.xor_(tmp, tmp);
			cc.mov(tmp.r32(), regD[b]);
			cc.cvtsi2sd(regF[a], tmp);
			break;
		}
		case CAST_F2I:
			cc.cvttsd2si(regD[a], regF[b]);
			break;
		case CAST_F2U:
		{
			auto tmp = cc.newInt64();
			cc.cvttsd2si(tmp, regF[b]);
			cc.mov(regD[a], tmp.r32());
			break;
		}
		/*case CAST_I2S:
			reg.s[a].Format("%d", reg.d[b]);
			break;
		case CAST_U2S:
			reg.s[a].Format("%u", reg.d[b]);
			break;
		case CAST_F2S:
			reg.s[a].Format("%.5f", reg.f[b]);	// keep this small. For more precise conversion there should be a conversion function.
			break;
		case CAST_V22S:
			reg.s[a].Format("(%.5f, %.5f)", reg.f[b], reg.f[b + 1]);
			break;
		case CAST_V32S:
			reg.s[a].Format("(%.5f, %.5f, %.5f)", reg.f[b], reg.f[b + 1], reg.f[b + 2]);
			break;
		case CAST_P2S:
		{
			if (reg.a[b] == nullptr) reg.s[a] = "null";
			else reg.s[a].Format("%p", reg.a[b]);
			break;
		}
		case CAST_S2I:
			reg.d[a] = (VM_SWORD)reg.s[b].ToLong();
			break;
		case CAST_S2F:
			reg.f[a] = reg.s[b].ToDouble();
			break;
		case CAST_S2N:
			reg.d[a] = reg.s[b].Len() == 0 ? FName(NAME_None) : FName(reg.s[b]);
			break;
		case CAST_N2S:
		{
			FName name = FName(ENamedName(reg.d[b]));
			reg.s[a] = name.IsValidName() ? name.GetChars() : "";
			break;
		}
		case CAST_S2Co:
			reg.d[a] = V_GetColor(NULL, reg.s[b]);
			break;
		case CAST_Co2S:
			reg.s[a].Format("%02x %02x %02x", PalEntry(reg.d[b]).r, PalEntry(reg.d[b]).g, PalEntry(reg.d[b]).b);
			break;
		case CAST_S2So:
			reg.d[a] = FSoundID(reg.s[b]);
			break;
		case CAST_So2S:
			reg.s[a] = S_sfx[reg.d[b]].name;
			break;
		case CAST_SID2S:
			reg.s[a] = unsigned(reg.d[b]) >= sprites.Size() ? "TNT1" : sprites[reg.d[b]].name;
			break;
		case CAST_TID2S:
		{
			auto tex = TexMan[*(FTextureID*)&(reg.d[b])];
			reg.s[a] = tex == nullptr ? "(null)" : tex->Name.GetChars();
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
			cc.mov(regD[a], regD[B]);
			cc.shr(regD[a], 31);
		}
		else if (C == CASTB_F)
		{
			auto zero = cc.newXmm();
			auto one = cc.newInt32();
			cc.xorpd(zero, zero);
			cc.mov(one, 1);
			cc.ucomisd(regF[a], zero);
			cc.setp(regD[a]);
			cc.cmovne(regD[a], one);
		}
		else if (C == CASTB_A)
		{
			cc.test(regA[a], regA[a]);
			cc.setne(regD[a]);
		}
		else
		{
			//reg.d[a] = reg.s[B].Len() > 0;
		}
	}

	void EmitDYNCAST_R()
	{
		using namespace asmjit;
		typedef DObject*(*FuncPtr)(DObject*, PClass*);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>([](DObject *obj, PClass *cls) -> DObject* {
			return (obj && obj->IsKindOf(cls)) ? obj : nullptr;
		}))), FuncSignature2<void*, void*, void*>());
		call->setRet(0, regA[a]);
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
		call->setRet(0, regA[a]);
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
		call->setRet(0, regA[a]);
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
		call->setRet(0, regA[a]);
		call->setArg(0, regA[B]);
		call->setArg(1, c);
	}

	// Control flow.

	void EmitTEST()
	{
		int i = (int)(ptrdiff_t)(pc - sfunc->Code);
		cc.cmp(regD[a], BC);
		cc.jne(labels[i + 1]);
	}
	
	void EmitTESTN()
	{
		int bc = BC;
		int i = (int)(ptrdiff_t)(pc - sfunc->Code);
		cc.cmp(regD[a], -bc);
		cc.jne(labels[i + 1]);
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
	//void EmitVTBL() {} // dereferences a virtual method table.
	//void EmitSCOPE() {} // Scope check at runtime.
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

	//void EmitNEW() {}
	//void EmitNEW_K() {}
	//void EmitTHROW() {} // A == 0: Throw exception object pB, A == 1: Throw exception object pkB, A >= 2: Throw VM exception of type BC
	//void EmitBOUND() {} // if rA < 0 or rA >= BC, throw exception
	//void EmitBOUND_K() {} // if rA < 0 or rA >= const[BC], throw exception
	//void EmitBOUND_R() {} // if rA < 0 or rA >= rB, throw exception

	// String instructions.

	//void EmitCONCAT() {} // sA = sB..sC
	//void EmitLENS() {} // dA = sB.Length
	//void EmitCMPS() {} // if ((skB op skC) != (A & 1)) then pc++

	// Integer math.

	void EmitSLL_RR()
	{
		BINARY_OP_INT(shl, regD[a], regD[B], regD[C]);
	}

	void EmitSLL_RI()
	{
		BINARY_OP_INT(shl, regD[a], regD[B], C);
	}

	void EmitSLL_KR()
	{
		BINARY_OP_INT(shl, regD[a], konstd[B], regD[C]);
	}

	void EmitSRL_RR()
	{
		BINARY_OP_INT(shr, regD[a], regD[B], regD[C]);
	}

	void EmitSRL_RI()
	{
		BINARY_OP_INT(shr, regD[a], regD[B], C);
	}

	void EmitSRL_KR()
	{
		BINARY_OP_INT(shr, regD[a], regD[B], C);
	}

	void EmitSRA_RR()
	{
		BINARY_OP_INT(sar, regD[a], regD[B], regD[C]);
	}

	void EmitSRA_RI()
	{
		BINARY_OP_INT(sar, regD[a], regD[B], C);
	}

	void EmitSRA_KR()
	{
		BINARY_OP_INT(sar, regD[a], konstd[B], regD[C]);
	}

	void EmitADD_RR()
	{
		BINARY_OP_INT(add, regD[a], regD[B], regD[C]);
	}

	void EmitADD_RK()
	{
		BINARY_OP_INT(add, regD[a], regD[B], konstd[C]);
	}

	void EmitADDI()
	{
		BINARY_OP_INT(add, regD[a], regD[B], Cs);
	}

	void EmitSUB_RR()
	{
		BINARY_OP_INT(sub, regD[a], regD[B], regD[C]);
	}

	void EmitSUB_RK()
	{
		BINARY_OP_INT(sub, regD[a], regD[B], konstd[C]);
	}

	void EmitSUB_KR()
	{
		BINARY_OP_INT(sub, regD[a], konstd[B], regD[C]);
	}

	void EmitMUL_RR()
	{
		BINARY_OP_INT(mul, regD[a], regD[B], regD[C]);
	}

	void EmitMUL_RK()
	{
		BINARY_OP_INT(mul, regD[a], regD[B], konstd[C]);
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
		BINARY_OP_INT(and_, regD[a], regD[B], regD[C]);
	}

	void EmitAND_RK()
	{
		BINARY_OP_INT(and_, regD[a], regD[B], konstd[C]);
	}

	void EmitOR_RR()
	{
		BINARY_OP_INT(or_, regD[a], regD[B], regD[C]);
	}

	void EmitOR_RK()
	{
		BINARY_OP_INT(or_, regD[a], regD[B], konstd[C]);
	}

	void EmitXOR_RR()
	{
		BINARY_OP_INT(xor_, regD[a], regD[B], regD[C]);
	}

	void EmitXOR_RK()
	{
		BINARY_OP_INT(xor_, regD[a], regD[B], konstd[C]);
	}

	void EmitMIN_RR()
	{
		auto tmp0 = cc.newXmmSs();
		auto tmp1 = cc.newXmmSs();
		cc.movd(tmp0, regD[B]);
		cc.movd(tmp1, regD[C]);
		cc.pminsd(tmp0, tmp1);
		cc.movd(regD[a], tmp0);
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
		cc.movd(regD[a], tmp0);
	}

	void EmitMAX_RR()
	{
		auto tmp0 = cc.newXmmSs();
		auto tmp1 = cc.newXmmSs();
		cc.movd(tmp0, regD[B]);
		cc.movd(tmp1, regD[C]);
		cc.pmaxsd(tmp0, tmp1);
		cc.movd(regD[a], tmp0);
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
		cc.movd(regD[a], tmp0);
	}

	void EmitABS()
	{
		auto tmp = cc.newInt32();
		cc.mov(tmp, regD[B]);
		cc.sar(tmp, 31);
		cc.mov(regD[A], tmp);
		cc.xor_(regD[A], regD[B]);
		cc.sub(regD[A], tmp);
	}

	void EmitNEG()
	{
		auto tmp = cc.newInt32();
		cc.xor_(tmp, tmp);
		cc.sub(tmp, regD[B]);
		cc.mov(regD[a], tmp);
	}

	void EmitNOT()
	{
		cc.mov(regD[a], regD[B]);
		cc.not_(regD[a]);
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
		cc.movsd(regF[a], regF[B]);
		cc.addsd(regF[a], regF[C]);
	}

	void EmitADDF_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[a], regF[B]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.addsd(regF[a], asmjit::x86::qword_ptr(tmp));
	}

	void EmitSUBF_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.subsd(regF[a], regF[C]);
	}

	void EmitSUBF_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[a], regF[B]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.subsd(regF[a], asmjit::x86::qword_ptr(tmp));
	}

	void EmitSUBF_KR()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.subsd(regF[a], regF[B]);
	}

	void EmitMULF_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.mulsd(regF[a], regF[C]);
	}

	void EmitMULF_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[a], regF[B]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.mulsd(regF[a], asmjit::x86::qword_ptr(tmp));
	}

	void EmitDIVF_RR()
	{
		auto label = cc.newLabel();

		cc.ptest(regF[C], regF[C]);
		cc.jne(label);

		EmitThrowException(X_DIVISION_BY_ZERO);

		cc.bind(label);

		cc.movsd(regF[a], regF[B]);
		cc.divsd(regF[a], regF[C]);
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
			cc.movsd(regF[a], regF[B]);
			cc.mov(tmp, ToMemAddress(&konstf[C]));
			cc.divsd(regF[a], asmjit::x86::qword_ptr(tmp));
		}
	}

	void EmitDIVF_KR()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&konstf[B]));
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.divsd(regF[a], regF[C]);
	}

	// void EmitMODF_RR() { } // fA = fkB % fkC
	// void EmitMODF_RK() { }
	// void EmitMODF_KR() { }

	void EmitPOWF_RR()
	{
		using namespace asmjit;
		typedef double(*FuncPtr)(double, double);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(g_pow))), FuncSignature2<double, double, double>());
		call->setRet(0, regF[a]);
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
		call->setRet(0, regF[a]);
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
		call->setRet(0, regF[a]);
		call->setArg(0, tmp2);
		call->setArg(1, regF[C]);
	}

	void EmitMINF_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.minsd(regF[a], regF[C]);
	}

	void EmitMINF_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.minsd(regF[a], regF[B]);
	}
	
	void EmitMAXF_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.maxsd(regF[a], regF[C]);
	}

	void EmitMAXF_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.movsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.maxsd(regF[a], regF[B]);
	}
	
	void EmitATAN2()
	{
		using namespace asmjit;
		typedef double(*FuncPtr)(double, double);
		auto call = cc.call(ToMemAddress(reinterpret_cast<const void*>(static_cast<FuncPtr>(g_atan2))), FuncSignature2<double, double, double>());
		call->setRet(0, regF[a]);
		call->setArg(0, regF[B]);
		call->setArg(1, regF[C]);

		static const double constant = 180 / M_PI;
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, ToMemAddress(&constant));
		cc.mulsd(regF[a], asmjit::x86::qword_ptr(tmp));
	}

	void EmitFLOP()
	{
		if (C == FLOP_NEG)
		{
			auto mask = cc.newDoubleConst(asmjit::kConstScopeLocal, -0.0);
			auto maskXmm = cc.newXmmSd();
			cc.movsd(maskXmm, mask);
			cc.movsd(regF[a], regF[B]);
			cc.xorpd(regF[a], maskXmm);
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
			call->setRet(0, regF[a]);
			call->setArg(0, v);

			if (C == FLOP_ACOS_DEG || C == FLOP_ASIN_DEG || C == FLOP_ATAN_DEG)
			{
				static const double constant = 180 / M_PI;
				auto tmp = cc.newIntPtr();
				cc.mov(tmp, ToMemAddress(&constant));
				cc.mulsd(regF[a], asmjit::x86::qword_ptr(tmp));
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
			cc.ucomisd(regF[C], regF[B]);
			cc.seta(result);
			cc.and_(result, 1);
		});
	}

	void EmitLTF_RK()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
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
			cc.ucomisd(regF[C], regF[B]);
			cc.setae(result);
			cc.and_(result, 1);
		});
	}

	void EmitLEF_RK()
	{
		EmitComparisonOpcode([&](asmjit::X86Gp& result) {
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
		cc.movsd(regF[a], regF[B]);
		cc.xorpd(regF[a], maskXmm);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.xorpd(regF[a + 1], maskXmm);
	}

	void EmitADDV2_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.addsd(regF[a], regF[C]);
		cc.addsd(regF[a + 1], regF[C + 1]);
	}

	void EmitSUBV2_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.subsd(regF[a], regF[C]);
		cc.subsd(regF[a + 1], regF[C + 1]);
	}

	void EmitDOTV2_RR()
	{
		auto tmp = cc.newXmmSd();
		cc.movsd(regF[a], regF[B]);
		cc.mulsd(regF[a], regF[C]);
		cc.movsd(tmp, regF[B + 1]);
		cc.mulsd(tmp, regF[C + 1]);
		cc.addsd(regF[a], tmp);
	}

	void EmitMULVF2_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.mulsd(regF[a], regF[C]);
		cc.mulsd(regF[a + 1], regF[C]);
	}

	void EmitMULVF2_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.mulsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.mulsd(regF[a + 1], asmjit::x86::qword_ptr(tmp));
	}

	void EmitDIVVF2_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.divsd(regF[a], regF[C]);
		cc.divsd(regF[a + 1], regF[C]);
	}

	void EmitDIVVF2_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.divsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.divsd(regF[a + 1], asmjit::x86::qword_ptr(tmp));
	}

	void EmitLENV2()
	{
		auto tmp = cc.newXmmSd();
		cc.movsd(regF[a], regF[B]);
		cc.mulsd(regF[a], regF[B]);
		cc.movsd(tmp, regF[B + 1]);
		cc.mulsd(tmp, regF[B + 1]);
		cc.addsd(regF[a], tmp);
		CallSqrt(regF[a], regF[a]);
	}

	// void EmitEQV2_R() { } // if ((vB == vkC) != A) then pc++ (inexact if A & 32)
	// void EmitEQV2_K() { } // this will never be used.

	// Vector math. (3D)

	void EmitNEGV3()
	{
		auto mask = cc.newDoubleConst(asmjit::kConstScopeLocal, -0.0);
		auto maskXmm = cc.newXmmSd();
		cc.movsd(maskXmm, mask);
		cc.movsd(regF[a], regF[B]);
		cc.xorpd(regF[a], maskXmm);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.xorpd(regF[a + 1], maskXmm);
		cc.movsd(regF[a + 2], regF[B + 2]);
		cc.xorpd(regF[a + 2], maskXmm);
	}

	void EmitADDV3_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.movsd(regF[a + 2], regF[B + 2]);
		cc.addsd(regF[a], regF[C]);
		cc.addsd(regF[a + 1], regF[C + 1]);
		cc.addsd(regF[a + 2], regF[C + 2]);
	}

	void EmitSUBV3_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.movsd(regF[a + 2], regF[B + 2]);
		cc.subsd(regF[a], regF[C]);
		cc.subsd(regF[a + 1], regF[C + 1]);
		cc.subsd(regF[a + 2], regF[C + 2]);
	}

	void EmitDOTV3_RR()
	{
		auto tmp = cc.newXmmSd();
		cc.movsd(regF[a], regF[B]);
		cc.mulsd(regF[a], regF[C]);
		cc.movsd(tmp, regF[B + 1]);
		cc.mulsd(tmp, regF[C + 1]);
		cc.addsd(regF[a], tmp);
		cc.movsd(tmp, regF[B + 2]);
		cc.mulsd(tmp, regF[C + 2]);
		cc.addsd(regF[a], tmp);
	}

	void EmitCROSSV_RR()
	{
		auto tmp = cc.newXmmSd();
		auto& a0 = regF[B]; auto& a1 = regF[B + 1]; auto& a2 = regF[B + 2];
		auto& b0 = regF[C]; auto& b1 = regF[C + 1]; auto& b2 = regF[C + 2];

		// r0 = a1b2 - a2b1
		cc.movsd(regF[a], a1);
		cc.mulsd(regF[a], b2);
		cc.movsd(tmp, a2);
		cc.mulsd(tmp, b1);
		cc.subsd(regF[a], tmp);

		// r1 = a2b0 - a0b2
		cc.movsd(regF[a + 1], a2);
		cc.mulsd(regF[a + 1], b0);
		cc.movsd(tmp, a0);
		cc.mulsd(tmp, b2);
		cc.subsd(regF[a + 1], tmp);

		// r2 = a0b1 - a1b0
		cc.movsd(regF[a + 2], a0);
		cc.mulsd(regF[a + 2], b1);
		cc.movsd(tmp, a1);
		cc.mulsd(tmp, b0);
		cc.subsd(regF[a + 2], tmp);
	}

	void EmitMULVF3_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.movsd(regF[a + 2], regF[B + 2]);
		cc.mulsd(regF[a], regF[C]);
		cc.mulsd(regF[a + 1], regF[C]);
		cc.mulsd(regF[a + 2], regF[C]);
	}

	void EmitMULVF3_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.movsd(regF[a + 2], regF[B + 2]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.mulsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.mulsd(regF[a + 1], asmjit::x86::qword_ptr(tmp));
		cc.mulsd(regF[a + 2], asmjit::x86::qword_ptr(tmp));
	}

	void EmitDIVVF3_RR()
	{
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.movsd(regF[a + 2], regF[B + 2]);
		cc.divsd(regF[a], regF[C]);
		cc.divsd(regF[a + 1], regF[C]);
		cc.divsd(regF[a + 2], regF[C]);
	}

	void EmitDIVVF3_RK()
	{
		auto tmp = cc.newIntPtr();
		cc.movsd(regF[a], regF[B]);
		cc.movsd(regF[a + 1], regF[B + 1]);
		cc.movsd(regF[a + 2], regF[B + 2]);
		cc.mov(tmp, ToMemAddress(&konstf[C]));
		cc.divsd(regF[a], asmjit::x86::qword_ptr(tmp));
		cc.divsd(regF[a + 1], asmjit::x86::qword_ptr(tmp));
		cc.divsd(regF[a + 2], asmjit::x86::qword_ptr(tmp));
	}

	void EmitLENV3()
	{
		auto tmp = cc.newXmmSd();
		cc.movsd(regF[a], regF[B]);
		cc.mulsd(regF[a], regF[B]);
		cc.movsd(tmp, regF[B + 1]);
		cc.mulsd(tmp, regF[B + 1]);
		cc.addsd(regF[a], tmp);
		cc.movsd(tmp, regF[B + 2]);
		cc.mulsd(tmp, regF[B + 2]);
		cc.addsd(regF[a], tmp);
		CallSqrt(regF[a], regF[a]);
	}

	// void EmitEQV3_R() { } // if ((vB == vkC) != A) then pc++ (inexact if A & 32)
	// void EmitEQV3_K() { } // this will never be used.

	// Pointer math.

	void EmitADDA_RR()
	{
		auto tmp = cc.newIntPtr();
		auto label = cc.newLabel();

		cc.mov(tmp, regA[B]);

		// Check if zero, the first operand is zero, if it is, don't add.
		cc.cmp(tmp, 0);
		cc.je(label);

		cc.add(tmp, regD[C]);

		cc.bind(label);
		cc.mov(regA[a], tmp);
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
		cc.mov(regA[a], tmp);
	}

	void EmitSUBA()
	{
		auto tmp = cc.newIntPtr();
		cc.mov(tmp, regA[B]);
		cc.sub(tmp, regD[C]);
		cc.mov(regA[a], tmp);
	}

	// void EmitEQA_R() { } // if ((pB == pkC) != A) then pc++
	// void EmitEQA_K() { }

	void Setup()
	{
		using namespace asmjit;

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
				regD[i] = cc.newInt32();
				cc.mov(regD[i], x86::dword_ptr(initreg, i * 4));
			}
		}
		if (sfunc->NumRegF > 0)
		{
			cc.mov(initreg, x86::ptr(vmregs, sizeof(void*)));
			for (int i = 0; i < sfunc->NumRegF; i++)
			{
				regF[i] = cc.newXmmSd ();
				cc.movsd(regF[i], x86::qword_ptr(initreg, i * 8));
			}
		}
		/*if (sfunc->NumRegS > 0)
		{
			cc.mov(initreg, x86::ptr(vmregs, sizeof(void*) * 2));
			for (int i = 0; i < sfunc->NumRegS; i++)
			{
				regS[i] = cc.newGpd();
			}
		}*/
		if (sfunc->NumRegA > 0)
		{
			cc.mov(initreg, x86::ptr(vmregs, sizeof(void*) * 3));
			for (int i = 0; i < sfunc->NumRegA; i++)
			{
				regA[i] = cc.newIntPtr();
				cc.mov(regA[i], x86::ptr(initreg, i * 4));
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

		compFunc(tmp);

		bool check = static_cast<bool>(A & CMP_CHECK);

		cc.test(tmp, tmp);
		if (check) cc.je(labels[i + 2]);
		else       cc.jne(labels[i + 2]);

		cc.jmp(labels[i + 2 + JMPOFS(pc + 1)]);
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
	int a;
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

JitFuncPtr JitCompile(VMScriptFunction *sfunc)
{
#if 0 // For debugging
	if (strcmp(sfunc->Name.GetChars(), "EmptyFunction") != 0)
		return nullptr;
#else
	if (!JitCompiler::CanJit(sfunc))
		return nullptr;
#endif

	using namespace asmjit;
	try
	{
		auto *jit = JitGetRuntime();

		ThrowingErrorHandler errorHandler;
		//FileLogger logger(stdout);
		CodeHolder code;
		code.init(jit->getCodeInfo());
		code.setErrorHandler(&errorHandler);
		//code.setLogger(&logger);

		JitCompiler compiler(&code, sfunc);
		compiler.Codegen();

		JitFuncPtr fn = nullptr;
		Error err = jit->add(&fn, &code);
		if (err)
			I_FatalError("JitRuntime::add failed: %d", err);
		return fn;
	}
	catch (const std::exception &e)
	{
		I_FatalError("Unexpected JIT error: %s", e.what());
		return nullptr;
	}
}
