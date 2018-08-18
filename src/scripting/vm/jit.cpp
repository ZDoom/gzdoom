
#include "jit.h"
#include "i_system.h"

// To do: get cmake to define these..
#define ASMJIT_BUILD_EMBED
#define ASMJIT_STATIC

#include <asmjit/asmjit.h>
#include <asmjit/x86.h>
#include <functional>

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

static asmjit::JitRuntime jit;

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

static bool CanJit(VMScriptFunction *sfunc)
{
	int size = sfunc->CodeSize;
	for (int i = 0; i < size; i++)
	{
		const VMOP *pc = sfunc->Code + i;
		VM_UBYTE op = pc->op;
		int a = pc->a;

		switch (op)
		{
		default:
			return false;
		case OP_NOP:
		case OP_LI:
		case OP_LK:
		case OP_LKF:
		//case OP_LKS:
		case OP_LKP:
		case OP_LK_R:
		case OP_LKF_R:
		//case OP_LKS_R:
		//case OP_LKP_R:
		case OP_LB:
		case OP_LB_R:
		case OP_LH:
		case OP_LH_R:
		case OP_LW:
		case OP_LW_R:
		case OP_LBU:
		case OP_LBU_R:
		case OP_LHU:
		case OP_LHU_R:
		case OP_LSP:
		case OP_LSP_R:
		case OP_LDP:
		case OP_LDP_R:
		case OP_LV2:
		case OP_LV2_R:
		case OP_LV3:
		case OP_LV3_R:
		case OP_SB:
		case OP_SB_R:
		case OP_SH:
		case OP_SH_R:
		case OP_SW:
		case OP_SW_R:
		case OP_SSP:
		case OP_SSP_R:
		case OP_SDP:
		case OP_SDP_R:
		case OP_SV2:
		case OP_SV2_R:
		case OP_SV3:
		case OP_SV3_R:
		case OP_MOVE:
		case OP_MOVEF:
		//case OP_MOVES:
		//case OP_MOVEA:
		case OP_MOVEV2:
		case OP_MOVEV3:
			break;
		case OP_RET:
			if (B != REGT_NIL)
			{
				int regtype = B;
				int regnum = C;
				switch (regtype & REGT_TYPE)
				{
				case REGT_STRING:
				case REGT_POINTER:
					return false;
				}
			}
			break;
		case OP_RETI:
		case OP_SLL_RR:
		case OP_SLL_RI:
		case OP_SLL_KR:
		case OP_SRL_RR:
		case OP_SRL_RI:
		case OP_SRL_KR:
		case OP_SRA_RR:
		case OP_SRA_RI:
		case OP_SRA_KR:
		case OP_ADD_RR:
		case OP_ADD_RK:
		case OP_ADDI:
		case OP_SUB_RR:
		case OP_SUB_RK:
		case OP_SUB_KR:
		case OP_MUL_RR:
		case OP_MUL_RK:
		case OP_DIV_RR:
		case OP_DIV_RK:
		case OP_DIV_KR:
		case OP_DIVU_RR:
		case OP_DIVU_RK:
		case OP_DIVU_KR:
		//case OP_MOD_RR:
		//case OP_MOD_RK:
		//case OP_MOD_KR:
		//case OP_MODU_RR:
		//case OP_MODU_RK:
		//case OP_MODU_KR:
		case OP_AND_RR:
		case OP_AND_RK:
		case OP_OR_RR:
		case OP_OR_RK:
		case OP_XOR_RR:
		case OP_XOR_RK:
		//case OP_MIN_RR:
		//case OP_MIN_RK:
		//case OP_MAX_RR:
		//case OP_MAX_RK:
		//case OP_ABS:
		case OP_NEG:
		case OP_NOT:
		case OP_EQ_R:
		case OP_EQ_K:
		case OP_LT_RR:
		case OP_LT_RK:
		case OP_LT_KR:
		case OP_LE_RR:
		case OP_LE_RK:
		case OP_LE_KR:
		case OP_LTU_RR:
		case OP_LTU_RK:
		case OP_LTU_KR:
		case OP_LEU_RR:
		case OP_LEU_RK:
		case OP_LEU_KR:
		case OP_ADDF_RR:
		case OP_ADDF_RK:
		case OP_SUBF_RR:
		case OP_SUBF_RK:
		case OP_SUBF_KR:
		case OP_MULF_RR:
		case OP_MULF_RK:
		case OP_DIVF_RR:
		case OP_DIVF_RK:
		case OP_DIVF_KR:
		case OP_EQF_R:
		case OP_EQF_K:
		case OP_LTF_RR:
		case OP_LTF_RK:
		case OP_LTF_KR:
		case OP_LEF_RR:
		case OP_LEF_RK:
		case OP_LEF_KR:
		case OP_NEGV2:
		case OP_ADDV2_RR:
		case OP_SUBV2_RR:
		case OP_DOTV2_RR:
		case OP_MULVF2_RR:
		case OP_MULVF2_RK:
		case OP_DIVVF2_RR:
		case OP_DIVVF2_RK:
		case OP_LENV2:
		// case OP_EQV2_R:
		// case OP_EQV2_K:
		case OP_NEGV3:
		case OP_ADDV3_RR:
		case OP_SUBV3_RR:
		case OP_DOTV3_RR:
		case OP_CROSSV_RR:
		case OP_MULVF3_RR:
		case OP_MULVF3_RK:
		case OP_DIVVF3_RR:
		case OP_DIVVF3_RK:
		case OP_LENV3:
		//case OP_EQV3_R:
		//case OP_EQV3_K:
		case OP_ADDA_RR:
		case OP_ADDA_RK:
		case OP_SUBA:
			break;
		}
	}
	return true;
}

template <typename Func>
void emitComparisonOpcode(asmjit::X86Compiler& cc, const TArray<asmjit::Label>& labels, const VMOP* pc, int i, Func compFunc) {
	using namespace asmjit;

	auto tmp = cc.newInt32();

	compFunc(tmp);

	bool check = static_cast<bool>(A & CMP_CHECK);

	cc.test(tmp, tmp);
	if (check) cc.je (labels[i + 2]);
	else       cc.jne(labels[i + 2]);

	cc.jmp(labels[i + 2 + JMPOFS(pc + 1)]);
}

static int64_t ToMemAddress(const void *d)
{
	return (int64_t)(ptrdiff_t)d;
}

JitFuncPtr JitCompile(VMScriptFunction *sfunc)
{
#if 0 // For debugging
	if (strcmp(sfunc->Name.GetChars(), "EmptyFunction") != 0)
		return nullptr;
#else
	if (!CanJit(sfunc))
		return nullptr;
#endif

	using namespace asmjit;
	try
	{
		ThrowingErrorHandler errorHandler;
		//FileLogger logger(stdout);
		CodeHolder code;
		code.init(jit.getCodeInfo());
		code.setErrorHandler(&errorHandler);
		//code.setLogger(&logger);

		X86Compiler cc(&code);

		X86Gp stack = cc.newIntPtr("stack"); // VMFrameStack *stack
		X86Gp vmregs = cc.newIntPtr("vmregs"); // void *vmregs
		X86Gp ret = cc.newIntPtr("ret"); // VMReturn *ret
		X86Gp numret = cc.newInt32("numret"); // int numret

		cc.addFunc(FuncSignature4<int, void *, void *, void *, int>());
		cc.setArg(0, stack);
		cc.setArg(1, vmregs);
		cc.setArg(2, ret);
		cc.setArg(3, numret);

		const int *konstd = sfunc->KonstD;
		const double *konstf = sfunc->KonstF;
		const FString *konsts = sfunc->KonstS;
		const FVoidObj *konsta = sfunc->KonstA;

		TArray<X86Gp> regD(sfunc->NumRegD, true);
		TArray<X86Xmm> regF(sfunc->NumRegF, true);
		TArray<X86Gp> regA(sfunc->NumRegA, true);
		//TArray<X86Gp> regS(sfunc->NumRegS, true);

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

		/*
		typedef void(*FuncPtr)();
		cc.call((ptrdiff_t)(FuncPtr)[] {
			Printf("c++ from asmjit\n");
		}, FuncSignature0<void>());
		*/

		int size = sfunc->CodeSize;

		TArray<Label> labels(size, true);

		for (int i = 0; i < size; i++) labels[i] = cc.newLabel();

		for (int i = 0; i < size; i++)
		{
			const VMOP *pc = sfunc->Code + i;
			VM_UBYTE op = pc->op;
			int a = pc->a;
			int b;// , c;

			cc.bind(labels[i]);

			switch (op)
			{
			default:
				I_FatalError("JIT error: Unknown VM opcode %d\n", op);
				break;

			case OP_NOP: // no operation
				cc.nop ();
				break;

			// Load constants.
			case OP_LI: // load immediate signed 16-bit constant
				cc.mov (regD[a], BCs);
				break;
			case OP_LK: // load integer constant
				cc.mov (regD[a], konstd[BC]);
				break;
			case OP_LKF: // load float constant
				{
					auto tmp = cc.newIntPtr ();
					cc.mov (tmp, ToMemAddress(konstf + BC));
					cc.movsd (regF[a], x86::qword_ptr (tmp));
				}
				break;
			//case OP_LKS: // load string constant
				//cc.mov(regS[a], konsts[BC]);
				//break;
			case OP_LKP: // load pointer constant
				cc.mov(regA[a], (int64_t) konsta[BC].v);
				break;
			case OP_LK_R: // load integer constant indexed
				cc.mov(regD[a], x86::ptr(ToMemAddress(konstd), regD[B], 2, C * 4));
				break;
			case OP_LKF_R: // load float constant indexed
				{
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, ToMemAddress(konstf + BC));
					cc.movsd(regF[a], x86::qword_ptr(tmp, regD[B], 3, C * 8));
				}
				break;
			case OP_LKS_R: // load string constant indexed
				//cc.mov(regS[a], konsts[regD[B] + C]);
				break;
			case OP_LKP_R: // load pointer constant indexed
				//cc.mov(b, regD[B] + C);
				//cc.mov(regA[a], konsta[b].v);
				break;
			//case OP_LFP: // load frame pointer
			//case OP_META: // load a class's meta data address
			//case OP_CLSS: // load a class's descriptor address

			// Load from memory. rA = *(rB + rkC)
			case OP_LB: // load byte
				NULL_POINTER_CHECK (PB, KC, X_READ_NIL);
				cc.movsx (regD[a], x86::byte_ptr (PB, KC));
				break;
			case OP_LB_R:
				NULL_POINTER_CHECK (PB, RC, X_READ_NIL);
				cc.movsx (regD[a], x86::byte_ptr (PB, RC));
				break;
			case OP_LH: // load halfword
				NULL_POINTER_CHECK (PB, KC, X_READ_NIL);
				cc.movsx (regD[a], x86::word_ptr (PB, KC));
				break;
			case OP_LH_R:
				NULL_POINTER_CHECK (PB, RC, X_READ_NIL);
				cc.movsx (regD[a], x86::word_ptr (PB, RC));
				break;
			case OP_LW: // load word
				NULL_POINTER_CHECK (PB, KC, X_READ_NIL);
				cc.mov (regD[a], x86::dword_ptr (PB, KC));
				break;
			case OP_LW_R:
				NULL_POINTER_CHECK (PB, RC, X_READ_NIL);
				cc.mov (regD[a], x86::dword_ptr (PB, RC));
				break;
			case OP_LBU: // load byte unsigned
				NULL_POINTER_CHECK (PB, KC, X_READ_NIL);
				cc.mov (regD[a], x86::byte_ptr (PB, KC));
				break;
			case OP_LBU_R:
				NULL_POINTER_CHECK (PB, RC, X_READ_NIL);
				cc.mov (regD[a], x86::byte_ptr (PB, RC));
				break;
			case OP_LHU: // load halfword unsigned
				NULL_POINTER_CHECK (PB, KC, X_READ_NIL);
				cc.mov (regD[a], x86::word_ptr (PB, KC));
				break;
			case OP_LHU_R:
				NULL_POINTER_CHECK (PB, RC, X_READ_NIL);
				cc.mov (regD[a], x86::word_ptr (PB, RC));
				break;
			case OP_LSP: // load single-precision fp
				NULL_POINTER_CHECK (PB, KC, X_READ_NIL);
				cc.movss (regF[a], x86::dword_ptr (PB, KC));
				break;
			case OP_LSP_R:
				NULL_POINTER_CHECK (PB, RC, X_READ_NIL);
				cc.movss (regF[a], x86::dword_ptr (PB, RC));
				break;
			case OP_LDP: // load double-precision fp
				NULL_POINTER_CHECK (PB, KC, X_READ_NIL);
				cc.movsd (regF[a], x86::qword_ptr (PB, KC));
				break;
			case OP_LDP_R:
				NULL_POINTER_CHECK (PB, RC, X_READ_NIL);
				cc.movsd (regF[a], x86::qword_ptr (PB, RC));
				break;
			//case OP_LS: // load string
			//case OP_LS_R:
			//case OP_LO: // load object
			//case OP_LO_R:
			//case OP_LP: // load pointer
			//case OP_LP_R:
			case OP_LV2: // load vector2
				NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
				{
					auto tmp = cc.newIntPtr ();
					cc.mov(tmp, PB);
					cc.add(tmp, KC);
					cc.movsd(regF[a], x86::qword_ptr(tmp));
					cc.movsd(regF[a+1], x86::qword_ptr(tmp, 8));
				}
				break;
			case OP_LV2_R: // Not used?
				NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
				{
					auto tmp = cc.newIntPtr ();
					cc.mov(tmp, PB);
					cc.add(tmp, RC);
					cc.movsd(regF[a], x86::qword_ptr(tmp));
					cc.movsd(regF[a+1], x86::qword_ptr(tmp, 8));
				}
				break;
			case OP_LV3: // load vector3
				NULL_POINTER_CHECK(PB, KC, X_READ_NIL);
				{
					auto tmp = cc.newIntPtr ();
					cc.mov(tmp, PB);
					cc.add(tmp, KC);
					cc.movsd(regF[a], x86::qword_ptr(tmp));
					cc.movsd(regF[a+1], x86::qword_ptr(tmp, 8));
					cc.movsd(regF[a+2], x86::qword_ptr(tmp, 16));
				}
				break;
			case OP_LV3_R:
				NULL_POINTER_CHECK(PB, RC, X_READ_NIL);
				{
					auto tmp = cc.newIntPtr ();
					cc.mov(tmp, PB);
					cc.add(tmp, RC);
					cc.movsd(regF[a], x86::qword_ptr(tmp));
					cc.movsd(regF[a+1], x86::qword_ptr(tmp, 8));
					cc.movsd(regF[a+2], x86::qword_ptr(tmp, 16));
				}
				break;
			//case OP_LCS: // load string from char ptr.
			//case OP_LCS_R:
			/*case OP_LBIT: // rA = !!(*rB & C)  -- *rB is a byte
				NULL_POINTER_CHECK (PB, 0, X_READ_NIL);
				{
					auto tmp = cc.newInt8 ();
					cc.mov (regD[a], PB);
					cc.and_ (regD[a], C);
					cc.test (regD[a], regD[a]);
					cc.sete (tmp);
					cc.movzx (regD[a], tmp);
				}
				break;*/

			// Store instructions. *(rA + rkC) = rB
			case OP_SB: // store byte
				NULL_POINTER_CHECK(PA, KC, X_WRITE_NIL);
				cc.mov(x86::byte_ptr(PA, KC), regD[B]);
				break;
			case OP_SB_R:
				NULL_POINTER_CHECK(PA, RC, X_WRITE_NIL);
				cc.mov(x86::byte_ptr(PA, RC), regD[B]);
				break;
			case OP_SH: // store halfword
				NULL_POINTER_CHECK(PA, KC, X_WRITE_NIL);
				cc.mov(x86::word_ptr(PA, KC), regD[B]);
				break;
			case OP_SH_R:
				NULL_POINTER_CHECK(PA, RC, X_WRITE_NIL);
				cc.mov(x86::word_ptr(PA, RC), regD[B]);
				break;
			case OP_SW: // store word
				NULL_POINTER_CHECK(PA, KC, X_WRITE_NIL);
				cc.mov(x86::dword_ptr(PA, KC), regD[B]);
				break;
			case OP_SW_R:
				NULL_POINTER_CHECK(PA, RC, X_WRITE_NIL);
				cc.mov(x86::dword_ptr(PA, RC), regD[B]);
				break;
			case OP_SSP: // store single-precision fp
				NULL_POINTER_CHECK (PB, KC, X_WRITE_NIL);
				cc.movss(x86::dword_ptr(PA, KC), regF[B]);
				break;
			case OP_SSP_R:
				NULL_POINTER_CHECK (PB, RC, X_WRITE_NIL);
				cc.movss(x86::dword_ptr(PA, RC), regF[B]);
				break;
			case OP_SDP: // store double-precision fp
				NULL_POINTER_CHECK (PB, KC, X_WRITE_NIL);
				cc.movsd(x86::qword_ptr(PA, KC), regF[B]);
				break;
			case OP_SDP_R:
				NULL_POINTER_CHECK (PB, RC, X_WRITE_NIL);
				cc.movsd(x86::qword_ptr(PA, RC), regF[B]);
				break;
			//case OP_SS: // store string
			//case OP_SS_R:
			//case OP_SO: // store object pointer with write barrier (only needed for non thinkers and non types)
			//case OP_SO_R:
			//case OP_SP: // store pointer
			//case OP_SP_R:
			case OP_SV2: // store vector2
				NULL_POINTER_CHECK (PB, KC, X_WRITE_NIL);
				{
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, PB);
					cc.add(tmp, KC);
					cc.movsd(x86::qword_ptr(tmp), regF[B]);
					cc.movsd(x86::qword_ptr(tmp, 8), regF[B+1]);
				}
				break;
			case OP_SV2_R:
				NULL_POINTER_CHECK (PB, RC, X_WRITE_NIL);
				{
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, PB);
					cc.add(tmp, RC);
					cc.movsd(x86::qword_ptr(tmp), regF[B]);
					cc.movsd(x86::qword_ptr(tmp, 8), regF[B+1]);
				}
				break;
			case OP_SV3: // store vector3
				NULL_POINTER_CHECK (PB, KC, X_WRITE_NIL);
				{
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, PB);
					cc.add(tmp, KC);
					cc.movsd(x86::qword_ptr(tmp), regF[B]);
					cc.movsd(x86::qword_ptr(tmp, 8), regF[B+1]);
					cc.movsd(x86::qword_ptr(tmp, 16), regF[B+2]);
				}
				break;
			case OP_SV3_R:
				NULL_POINTER_CHECK (PB, RC, X_WRITE_NIL);
				{
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, PB);
					cc.add(tmp, RC);
					cc.movsd(x86::qword_ptr(tmp), regF[B]);
					cc.movsd(x86::qword_ptr(tmp, 8), regF[B+1]);
					cc.movsd(x86::qword_ptr(tmp, 16), regF[B+2]);
				}
				break;
			//case OP_SBIT: // *rA |= C if rB is true, *rA &= ~C otherwise

			// Move instructions.
			case OP_MOVE: // dA = dB
				cc.mov(regD[a], regD[B]);
				break;
			case OP_MOVEF: // fA = fB
				cc.movsd(regF[a], regF[B]);
				break;
			//case OP_MOVES: // sA = sB
			case OP_MOVEA: // aA = aB
				break;
			case OP_MOVEV2: // fA = fB (2 elements)
				b = B;
				cc.movsd(regF[a], regF[b]);
				cc.movsd(regF[a + 1], regF[b + 1]);
				break;
			case OP_MOVEV3: // fA = fB (3 elements)
				b = B;
				cc.movsd(regF[a], regF[b]);
				cc.movsd(regF[a + 1], regF[b + 1]);
				cc.movsd(regF[a + 2], regF[b + 2]);
				break;
			//case OP_CAST: // xA = xB, conversion specified by C
			//case OP_CASTB: // xA = !!xB, type specified by C
			//case OP_DYNCAST_R: // aA = dyn_cast<aC>(aB);
			//case OP_DYNCAST_K: // aA = dyn_cast<aKC>(aB);
			//case OP_DYNCASTC_R: // aA = dyn_cast<aC>(aB); for class types
			//case OP_DYNCASTC_K: // aA = dyn_cast<aKC>(aB);

			// Control flow.
			//case OP_TEST: // if (dA != BC) then pc++
			//case OP_TESTN: // if (dA != -BC) then pc++
			//case OP_JMP: // pc += ABC		-- The ABC fields contain a signed 24-bit offset.
			//case OP_IJMP: // pc += dA + BC	-- BC is a signed offset. The target instruction must be a JMP.
			//case OP_PARAM: // push parameter encoded in BC for function call (B=regtype, C=regnum)
			//case OP_PARAMI: // push immediate, signed integer for function call
			//case OP_CALL: // Call function pkA with parameter count B and expected result count C
			//case OP_CALL_K:
			//case OP_VTBL: // dereferences a virtual method table.
			//case OP_SCOPE: // Scope check at runtime.
			//case OP_TAIL: // Call+Ret in a single instruction
			//case OP_TAIL_K:
			//case OP_RESULT: // Result should go in register encoded in BC (in caller, after CALL)
			case OP_RET: // Copy value from register encoded in BC to return value A, possibly returning
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
					case REGT_POINTER:
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
				break;
			case OP_RETI: // Copy immediate from BC to return value A, possibly returning
				{
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
				break;

			//case OP_NEW:
			//case OP_NEW_K:
			//case OP_THROW: // A == 0: Throw exception object pB, A == 1: Throw exception object pkB, A >= 2: Throw VM exception of type BC
			//case OP_BOUND: // if rA < 0 or rA >= BC, throw exception
			//case OP_BOUND_K: // if rA < 0 or rA >= const[BC], throw exception
			//case OP_BOUND_R: // if rA < 0 or rA >= rB, throw exception

			// String instructions.
			//case OP_CONCAT: // sA = sB..sC
			//case OP_LENS: // dA = sB.Length
			//case OP_CMPS: // if ((skB op skC) != (A & 1)) then pc++

			// Integer math.
			case OP_SLL_RR: // dA = dkB << diC
				BINARY_OP_INT(shl, regD[a], regD[B], regD[C]);
				break;
			case OP_SLL_RI:
				BINARY_OP_INT(shl, regD[a], regD[B], C);
				break;
			case OP_SLL_KR:
				BINARY_OP_INT(shl, regD[a], konstd[B], regD[C]);
				break;

			case OP_SRL_RR: // dA = dkB >> diC  -- unsigned
				BINARY_OP_INT(shr, regD[a], regD[B], regD[C]);
				break;
			case OP_SRL_RI:
				BINARY_OP_INT(shr, regD[a], regD[B], C);
				break;
			case OP_SRL_KR:
				BINARY_OP_INT(shr, regD[a], regD[B], C);
				break;

			case OP_SRA_RR: // dA = dkB >> diC  -- signed
				BINARY_OP_INT(sar, regD[a], regD[B], regD[C]);
				break;
			case OP_SRA_RI:
				BINARY_OP_INT(sar, regD[a], regD[B], C);
				break;
			case OP_SRA_KR:
				BINARY_OP_INT(sar, regD[a], konstd[B], regD[C]);
				break;

			case OP_ADD_RR: // dA = dB + dkC
				BINARY_OP_INT(add, regD[a], regD[B], regD[C]);
				break;
			case OP_ADD_RK:
				BINARY_OP_INT(add, regD[a], regD[B], konstd[C]);
				break;
			case OP_ADDI: // dA = dB + C		-- C is a signed 8-bit constant
				BINARY_OP_INT(add, regD[a], regD[B], Cs);
				break;

			case OP_SUB_RR: // dA = dkB - dkC
				BINARY_OP_INT(sub, regD[a], regD[B], regD[C]);
				break;
			case OP_SUB_RK:
				BINARY_OP_INT(sub, regD[a], regD[B], konstd[C]);
				break;
			case OP_SUB_KR:
				BINARY_OP_INT(sub, regD[a], konstd[B], regD[C]);
				break;
			case OP_MUL_RR: // dA = dB * dkC
				BINARY_OP_INT(mul, regD[a], regD[B], regD[C]);
				break;
			case OP_MUL_RK:
				BINARY_OP_INT(mul, regD[a], regD[B], konstd[C]);
				break;
			case OP_DIV_RR: // dA = dkB / dkC (signed)
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				cc.mov(tmp0, regD[B]);
				cc.cdq(tmp1, tmp0);
				cc.idiv(tmp1, tmp0, regD[C]);
				cc.mov(regD[A], tmp0);
				break;
			}
			case OP_DIV_RK:
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				auto konstTmp = cc.newIntPtr();
				cc.mov(tmp0, regD[B]);
				cc.cdq(tmp1, tmp0);
				cc.mov(konstTmp, ToMemAddress(&konstd[C]));
				cc.idiv(tmp1, tmp0, x86::ptr(konstTmp));
				cc.mov(regD[A], tmp0);
				break;
			}
			case OP_DIV_KR:
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				cc.mov(tmp0, konstd[B]);
				cc.cdq(tmp1, tmp0);
				cc.idiv(tmp1, tmp0, regD[C]);
				cc.mov(regD[A], tmp0);
				break;
			}
			case OP_DIVU_RR: // dA = dkB / dkC (unsigned)
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				cc.mov(tmp0, regD[B]);
				cc.mov(tmp1, 0);
				cc.div(tmp1, tmp0, regD[C]);
				cc.mov(regD[A], tmp0);
				break;
			}
			case OP_DIVU_RK:
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				auto konstTmp = cc.newIntPtr();
				cc.mov(tmp0, regD[B]);
				cc.mov(tmp1, 0);
				cc.mov(konstTmp, ToMemAddress(&konstd[C]));
				cc.div(tmp1, tmp0, x86::ptr(konstTmp));
				cc.mov(regD[A], tmp0);
				break;
			}
			case OP_DIVU_KR:
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				cc.mov(tmp0, konstd[B]);
				cc.mov(tmp1, 0);
				cc.div(tmp1, tmp0, regD[C]);
				cc.mov(regD[A], tmp0);
				break;
			}
			case OP_MOD_RR: // dA = dkB % dkC (signed)
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				cc.mov(tmp0, regD[B]);
				cc.cdq(tmp1, tmp0);
				cc.idiv(tmp1, tmp0, regD[C]);
				cc.mov(regD[A], tmp1);
				break;
			}
			case OP_MOD_RK:
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				auto konstTmp = cc.newIntPtr();
				cc.mov(tmp0, regD[B]);
				cc.cdq(tmp1, tmp0);
				cc.mov(konstTmp, ToMemAddress(&konstd[C]));
				cc.idiv(tmp1, tmp0, x86::ptr(konstTmp));
				cc.mov(regD[A], tmp1);
				break;
			}
			case OP_MOD_KR:
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				cc.mov(tmp0, konstd[B]);
				cc.cdq(tmp1, tmp0);
				cc.idiv(tmp1, tmp0, regD[C]);
				cc.mov(regD[A], tmp1);
				break;
			}
			case OP_MODU_RR: // dA = dkB % dkC (unsigned)
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				cc.mov(tmp0, regD[B]);
				cc.mov(tmp1, 0);
				cc.div(tmp1, tmp0, regD[C]);
				cc.mov(regD[A], tmp1);
				break;
			}
			case OP_MODU_RK:
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				auto konstTmp = cc.newIntPtr();
				cc.mov(tmp0, regD[B]);
				cc.mov(tmp1, 0);
				cc.mov(konstTmp, ToMemAddress(&konstd[C]));
				cc.div(tmp1, tmp0, x86::ptr(konstTmp));
				cc.mov(regD[A], tmp1);
				break;
			}
			case OP_MODU_KR:
			{
				auto tmp0 = cc.newInt32();
				auto tmp1 = cc.newInt32();
				cc.mov(tmp0, konstd[B]);
				cc.mov(tmp1, 0);
				cc.div(tmp1, tmp0, regD[C]);
				cc.mov(regD[A], tmp1);
				break;
			}
			case OP_AND_RR: // dA = dB & dkC
				BINARY_OP_INT(and_, regD[a], regD[B], regD[C]);
				break;
			case OP_AND_RK:
				BINARY_OP_INT(and_, regD[a], regD[B], konstd[C]);
				break;
			case OP_OR_RR: // dA = dB | dkC
				BINARY_OP_INT(or_, regD[a], regD[B], regD[C]);
				break;
			case OP_OR_RK:
				BINARY_OP_INT(or_, regD[a], regD[B], konstd[C]);
				break;
			case OP_XOR_RR: // dA = dB ^ dkC
				BINARY_OP_INT(xor_, regD[a], regD[B], regD[C]);
				break;
			case OP_XOR_RK:
				BINARY_OP_INT(xor_, regD[a], regD[B], konstd[C]);
				break;
			case OP_MIN_RR: // dA = min(dB,dkC)
			{
				auto tmp0 = cc.newXmmSs();
				auto tmp1 = cc.newXmmSs();
				cc.movd(tmp0, regD[B]);
				cc.movd(tmp1, regD[C]);
				cc.pminsd(tmp0, tmp1);
				cc.movd(regD[a], tmp0);
				break;
			}
			case OP_MIN_RK:
			{
				auto tmp0 = cc.newXmmSs();
				auto tmp1 = cc.newXmmSs();
				auto konstTmp = cc.newIntPtr();
				cc.mov(konstTmp, ToMemAddress(&konstd[C]));
				cc.movd(tmp0, regD[B]);
				cc.movss(tmp1, x86::dword_ptr(konstTmp));
				cc.pminsd(tmp0, tmp1);
				cc.movd(regD[a], tmp0);
				break;
			}
			case OP_MAX_RR: // dA = max(dB,dkC)
			{
				auto tmp0 = cc.newXmmSs();
				auto tmp1 = cc.newXmmSs();
				cc.movd(tmp0, regD[B]);
				cc.movd(tmp1, regD[C]);
				cc.pmaxsd(tmp0, tmp1);
				cc.movd(regD[a], tmp0);
				break;
			}
			case OP_MAX_RK:
			{
				auto tmp0 = cc.newXmmSs();
				auto tmp1 = cc.newXmmSs();
				auto konstTmp = cc.newIntPtr();
				cc.mov(konstTmp, ToMemAddress(&konstd[C]));
				cc.movd(tmp0, regD[B]);
				cc.movss(tmp1, x86::dword_ptr(konstTmp));
				cc.pmaxsd(tmp0, tmp1);
				cc.movd(regD[a], tmp0);
				break;
			}
			case OP_ABS: // dA = abs(dB)
			{
				auto tmp = cc.newInt32();
				cc.mov(tmp, regD[B]);
				cc.sar(tmp, 31);
				cc.mov(regD[A], tmp);
				cc.xor_(regD[A], regD[B]);
				cc.sub(regD[A], tmp);
				break;
			}
			case OP_NEG: // dA = -dB
			{
				auto tmp = cc.newInt32 ();
				cc.xor_(tmp, tmp);
				cc.sub(tmp, regD[B]);
				cc.mov(regD[a], tmp);
				break;
			}
			case OP_NOT: // dA = ~dB
				cc.mov(regD[a], regD[B]);
				cc.not_(regD[a]);
				break;
			case OP_EQ_R: // if ((dB == dkC) != A) then pc++
			{
				auto compLambda = [&] (X86Gp& result) {
					cc.cmp(regD[B], regD[C]);
					cc.sete(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_EQ_K:
			{
				auto compLambda = [&] (X86Gp& result) { 
					cc.cmp(regD[B], konstd[C]);
					cc.sete(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LT_RR: // if ((dkB < dkC) != A) then pc++
			{
				auto compLambda = [&](X86Gp& result) {
					cc.cmp(regD[B], regD[C]);
					cc.setl(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LT_RK:
			{
				auto compLambda = [&](X86Gp& result) {
					cc.cmp(regD[B], konstd[C]);
					cc.setl(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LT_KR:
			{
				auto compLambda = [&](X86Gp& result) {
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, ToMemAddress(&konstd[B]));
					cc.cmp(x86::ptr(tmp), regD[C]);
					cc.setl(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LE_RR: // if ((dkB <= dkC) != A) then pc++
			{
				auto compLambda = [&](X86Gp& result) {
					cc.cmp(regD[B], regD[C]);
					cc.setle(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LE_RK:
			{
				auto compLambda = [&](X86Gp& result) {
					cc.cmp(regD[B], konstd[C]);
					cc.setle(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LE_KR:
			{
				auto compLambda = [&](X86Gp& result) {
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, ToMemAddress(&konstd[B]));
					cc.cmp(x86::ptr(tmp), regD[C]);
					cc.setle(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LTU_RR: // if ((dkB < dkC) != A) then pc++		-- unsigned
			{
				auto compLambda = [&](X86Gp& result) {
					cc.cmp(regD[B], regD[C]);
					cc.setb(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LTU_RK:
			{
				auto compLambda = [&](X86Gp& result) {
					cc.cmp(regD[B], konstd[C]);
					cc.setb(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LTU_KR:
			{
				auto compLambda = [&](X86Gp& result) {
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, ToMemAddress(&konstd[B]));
					cc.cmp(x86::ptr(tmp), regD[C]);
					cc.setb(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LEU_RR: // if ((dkB <= dkC) != A) then pc++		-- unsigned
			{
				auto compLambda = [&](X86Gp& result) {
					cc.cmp(regD[B], regD[C]);
					cc.setbe(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LEU_RK:
			{
				auto compLambda = [&](X86Gp& result) {
					cc.cmp(regD[B], konstd[C]);
					cc.setbe(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LEU_KR:
			{
				auto compLambda = [&](X86Gp& result) {
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, ToMemAddress(&konstd[C]));
					cc.cmp(x86::ptr(tmp), regD[B]);
					cc.setbe(result);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}

			// Double-precision floating point math.
			case OP_ADDF_RR: // fA = fB + fkC
				cc.movsd(regF[a], regF[B]);
				cc.addsd(regF[a], regF[C]);
				break;
			case OP_ADDF_RK:
			{
				auto tmp = cc.newIntPtr();
				cc.movsd(regF[a], regF[B]);
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.addsd(regF[a], x86::qword_ptr(tmp));
				break;
			}
			case OP_SUBF_RR: // fA = fkB - fkC
				cc.movsd(regF[a], regF[B]);
				cc.subsd(regF[a], regF[C]);
				break;
			case OP_SUBF_RK:
			{
				auto tmp = cc.newIntPtr();
				cc.movsd(regF[a], regF[B]);
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.subsd(regF[a], x86::qword_ptr(tmp));
				break;
			}
			case OP_SUBF_KR:
			{
				auto tmp = cc.newIntPtr();
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.movsd(regF[a], x86::qword_ptr(tmp));
				cc.subsd(regF[a], regF[B]);
				break;
			}
			case OP_MULF_RR: // fA = fB * fkC
				cc.movsd(regF[a], regF[B]);
				cc.mulsd(regF[a], regF[C]);
				break;
			case OP_MULF_RK:
			{
				auto tmp = cc.newIntPtr();
				cc.movsd(regF[a], regF[B]);
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.mulsd(regF[a], x86::qword_ptr(tmp));
				break;
			}
			case OP_DIVF_RR: // fA = fkB / fkC
				cc.movsd(regF[a], regF[B]);
				cc.divsd(regF[a], regF[C]);
				break;
			case OP_DIVF_RK:
			{
				auto tmp = cc.newIntPtr();
				cc.movsd(regF[a], regF[B]);
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.divsd(regF[a], x86::qword_ptr(tmp));
				break;
			}
			case OP_DIVF_KR:
			{
				auto tmp = cc.newIntPtr();
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.movsd(regF[a], x86::qword_ptr(tmp));
				cc.divsd(regF[a], regF[B]);
				break;
			}
			//case OP_MODF_RR: // fA = fkB % fkC
			//case OP_MODF_RK:
			//case OP_MODF_KR:
			//case OP_POWF_RR: // fA = fkB ** fkC
			//case OP_POWF_RK:
			//case OP_POWF_KR:
			//case OP_MINF_RR: // fA = min(fB),fkC)
			//case OP_MINF_RK:
			//case OP_MAXF_RR: // fA = max(fB),fkC)
			//case OP_MAXF_RK:
			//case OP_ATAN2: // fA = atan2(fB,fC), result is in degrees
			//case OP_FLOP: // fA = f(fB), where function is selected by C
			case OP_EQF_R: // if ((fB == fkC) != (A & 1)) then pc++
			{
				auto compLambda = [&](X86Gp& result) {
					bool approx = static_cast<bool>(A & CMP_APPROX);
					if (!approx) {
						auto tmp = cc.newInt32();
						cc.ucomisd(regF[B], regF[C]);
						cc.sete(result);
						cc.setnp(tmp);
						cc.and_(result, tmp);
						cc.and_(result, 1);
					}
					else {
						auto tmp = cc.newXmmSd();

						const int64_t absMaskInt = 0x7FFFFFFFFFFFFFFF;
						auto absMask = cc.newDoubleConst(kConstScopeLocal, reinterpret_cast<const double&>(absMaskInt));
						auto absMaskXmm = cc.newXmmPd();

						auto epsilon = cc.newDoubleConst(kConstScopeLocal, VM_EPSILON);
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
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_EQF_K:
			{
				auto compLambda = [&](X86Gp& result) {
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
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LTF_RR: // if ((fkB < fkC) != (A & 1)) then pc++
			{
				auto compLambda = [&](X86Gp& result) {
					cc.ucomisd(regF[C], regF[B]);
					cc.seta(result);
					cc.and_(result, 1);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LTF_RK:
			{
				auto compLambda = [&](X86Gp& result) {
					auto constTmp = cc.newIntPtr();
					auto xmmTmp = cc.newXmmSd();
					cc.mov(constTmp, ToMemAddress(&konstf[C]));
					cc.movsd(xmmTmp, x86::qword_ptr(constTmp));

					cc.ucomisd(xmmTmp, regF[B]);
					cc.seta(result);
					cc.and_(result, 1);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LTF_KR:
			{
				auto compLambda = [&](X86Gp& result) {
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, ToMemAddress(&konstf[B]));

					cc.ucomisd(regF[C], x86::qword_ptr(tmp));
					cc.seta(result);
					cc.and_(result, 1);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LEF_RR: // if ((fkb <= fkC) != (A & 1)) then pc++
			{
				auto compLambda = [&](X86Gp& result) {
					cc.ucomisd(regF[C], regF[B]);
					cc.setae(result);
					cc.and_(result, 1);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LEF_RK:
			{
				auto compLambda = [&](X86Gp& result) {
					auto constTmp = cc.newIntPtr();
					auto xmmTmp = cc.newXmmSd();
					cc.mov(constTmp, ToMemAddress(&konstf[C]));
					cc.movsd(xmmTmp, x86::qword_ptr(constTmp));

					cc.ucomisd(xmmTmp, regF[B]);
					cc.setae(result);
					cc.and_(result, 1);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}
			case OP_LEF_KR:
			{
				auto compLambda = [&](X86Gp& result) {
					auto tmp = cc.newIntPtr();
					cc.mov(tmp, ToMemAddress(&konstf[B]));

					cc.ucomisd(regF[C], x86::qword_ptr(tmp));
					cc.setae(result);
					cc.and_(result, 1);
				};
				emitComparisonOpcode(cc, labels, pc, i, compLambda);

				break;
			}

			// Vector math. (2D)
			case OP_NEGV2: // vA = -vB
				cc.xorpd(regF[a], regF[a]);
				cc.xorpd(regF[a + 1], regF[a + 1]);
				cc.subsd(regF[a], regF[B]);
				cc.subsd(regF[a + 1], regF[B + 1]);
				break;
			case OP_ADDV2_RR: // vA = vB + vkC
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.addsd(regF[a], regF[C]);
				cc.addsd(regF[a + 1], regF[C + 1]);
				break;
			case OP_SUBV2_RR: // vA = vkB - vkC
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.subsd(regF[a], regF[C]);
				cc.subsd(regF[a + 1], regF[C + 1]);
				break;
			case OP_DOTV2_RR: // va = vB dot vkC
			{
				auto tmp = cc.newXmmSd();
				cc.movsd(regF[a], regF[B]);
				cc.mulsd(regF[a], regF[C]);
				cc.movsd(tmp, regF[B + 1]);
				cc.mulsd(tmp, regF[C + 1]);
				cc.addsd(regF[a], tmp);
				break;
			}
			case OP_MULVF2_RR: // vA = vkB * fkC
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.mulsd(regF[a], regF[C]);
				cc.mulsd(regF[a + 1], regF[C]);
				break;
			case OP_MULVF2_RK:
			{
				auto tmp = cc.newIntPtr();
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.mulsd(regF[a], x86::qword_ptr(tmp));
				cc.mulsd(regF[a + 1], x86::qword_ptr(tmp));
				break;
			}
			case OP_DIVVF2_RR: // vA = vkB / fkC
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.divsd(regF[a], regF[C]);
				cc.divsd(regF[a + 1], regF[C]);
				break;
			case OP_DIVVF2_RK:
			{
				auto tmp = cc.newIntPtr();
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.divsd(regF[a], x86::qword_ptr(tmp));
				cc.divsd(regF[a + 1], x86::qword_ptr(tmp));
				break;
			}
			case OP_LENV2: // fA = vB.Length
			{
				auto tmp = cc.newXmmSd();
				cc.movsd(regF[a], regF[B]);
				cc.mulsd(regF[a], regF[B]);
				cc.movsd(tmp, regF[B + 1]);
				cc.mulsd(tmp, regF[B + 1]);
				cc.addsd(regF[a], tmp);
				cc.sqrtsd(regF[a], regF[a]);
				break;
			}
			//case OP_EQV2_R: // if ((vB == vkC) != A) then pc++ (inexact if A & 32)
			//case OP_EQV2_K: // this will never be used.

			// Vector math. (3D)
			case OP_NEGV3: // vA = -vB
				cc.xorpd(regF[a], regF[a]);
				cc.xorpd(regF[a + 1], regF[a + 1]);
				cc.xorpd(regF[a + 2], regF[a + 2]);
				cc.subsd(regF[a], regF[B]);
				cc.subsd(regF[a + 1], regF[B + 1]);
				cc.subsd(regF[a + 2], regF[B + 2]);
				break;
			case OP_ADDV3_RR: // vA = vB + vkC
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.movsd(regF[a + 2], regF[B + 2]);
				cc.addsd(regF[a], regF[C]);
				cc.addsd(regF[a + 1], regF[C + 1]);
				cc.addsd(regF[a + 2], regF[C + 2]);
				break;
			case OP_SUBV3_RR: // vA = vkB - vkC
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.movsd(regF[a + 2], regF[B + 2]);
				cc.subsd(regF[a], regF[C]);
				cc.subsd(regF[a + 1], regF[C + 1]);
				cc.subsd(regF[a + 2], regF[C + 2]);
				break;
			case OP_DOTV3_RR: // va = vB dot vkC
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
				break;
			}
			case OP_CROSSV_RR: // vA = vkB cross vkC
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

				break;
			}
			case OP_MULVF3_RR: // vA = vkB * fkC
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.movsd(regF[a + 2], regF[B + 2]);
				cc.mulsd(regF[a], regF[C]);
				cc.mulsd(regF[a + 1], regF[C]);
				cc.mulsd(regF[a + 2], regF[C]);
				break;
			case OP_MULVF3_RK:
			{
				auto tmp = cc.newIntPtr();
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.movsd(regF[a + 2], regF[B + 2]);
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.mulsd(regF[a], x86::qword_ptr(tmp));
				cc.mulsd(regF[a + 1], x86::qword_ptr(tmp));
				cc.mulsd(regF[a + 2], x86::qword_ptr(tmp));
				break;
			}
			case OP_DIVVF3_RR: // vA = vkB / fkC
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.movsd(regF[a + 2], regF[B + 2]);
				cc.divsd(regF[a], regF[C]);
				cc.divsd(regF[a + 1], regF[C]);
				cc.divsd(regF[a + 2], regF[C]);
				break;
			case OP_DIVVF3_RK:
			{
				auto tmp = cc.newIntPtr();
				cc.movsd(regF[a], regF[B]);
				cc.movsd(regF[a + 1], regF[B + 1]);
				cc.movsd(regF[a + 2], regF[B + 2]);
				cc.mov(tmp, ToMemAddress(&konstf[C]));
				cc.divsd(regF[a], x86::qword_ptr(tmp));
				cc.divsd(regF[a + 1], x86::qword_ptr(tmp));
				cc.divsd(regF[a + 2], x86::qword_ptr(tmp));
				break;
			}
			case OP_LENV3: // fA = vB.Length
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
				cc.sqrtsd(regF[a], regF[a]);
				break;
			}
			//case OP_EQV3_R: // if ((vB == vkC) != A) then pc++ (inexact if A & 32)
			//case OP_EQV3_K: // this will never be used.

			// Pointer math.
			case OP_ADDA_RR: // pA = pB + dkC
			{
				auto tmp = cc.newIntPtr();
				Label label = cc.newLabel();

				cc.mov(tmp, regA[B]);

				// Check if zero, the first operand is zero, if it is, don't add.
				cc.cmp(tmp, 0);
				cc.je(label);

				cc.add(tmp, regD[C]);

				cc.bind(label);
				cc.mov(regA[a], tmp);

				break;
			}

			case OP_ADDA_RK:
			{
				auto tmp = cc.newIntPtr();
				Label label = cc.newLabel();

				cc.mov(tmp, regA[B]);

				// Check if zero, the first operand is zero, if it is, don't add.
				cc.cmp(tmp, 0);
				cc.je(label);

				cc.add(tmp, konstd[C]);

				cc.bind(label);
				cc.mov(regA[a], tmp);

				break;
			}

			case OP_SUBA: // dA = pB - pC
			{
				auto tmp = cc.newIntPtr();
				cc.mov(tmp, regA[B]);
				cc.sub(tmp, regD[C]);
				cc.mov(regA[a], tmp);
				break;
			}

			//case OP_EQA_R: // if ((pB == pkC) != A) then pc++
			//case OP_EQA_K:
			}
		}

		cc.endFunc();
		cc.finalize();

		JitFuncPtr fn = nullptr;
		Error err = jit.add(&fn, &code);
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
