#ifndef IMPLEMENT_VMEXEC
#error vmexec.h must not be #included outside vmexec.cpp. Use vm.h instead.
#endif


static int Exec(VMFrameStack *stack, const VMOP *pc, VMReturn *ret, int numret)
{
#if COMPGOTO
	static const void * const ops[256] =
	{
#define xx(op,sym,mode) &&op
#include "vmops.h"
	};
#endif
	const VMOP *exception_frames[MAX_TRY_DEPTH];
	int try_depth = 0;
	VMFrame *f = stack->TopFrame();
	VMScriptFunction *sfunc;
	const VMRegisters reg(f);
	const int *konstd;
	const double *konstf;
	const FString *konsts;
	const FVoidObj *konsta;
	const VM_ATAG *konstatag;

	if (f->Func != NULL && !f->Func->Native)
	{
		sfunc = static_cast<VMScriptFunction *>(f->Func);
		konstd = sfunc->KonstD;
		konstf = sfunc->KonstF;
		konsts = sfunc->KonstS;
		konsta = sfunc->KonstA;
		konstatag = sfunc->KonstATags();
	}
	else
	{
		sfunc = NULL;
		konstd = NULL;
		konstf = NULL;
		konsts = NULL;
		konsta = NULL;
		konstatag = NULL;
	}

	void *ptr;
	double fb, fc;
	const double *fbp, *fcp;
	int a, b, c;

begin:
	try
	{
#if !COMPGOTO
	VM_UBYTE op;
	for(;;) switch(op = pc->op, a = pc->a, pc++, op)
#else
	NEXTOP;
#endif
	{
	OP(LI):
		ASSERTD(a);
		reg.d[a] = BCs;
		NEXTOP;
	OP(LK):
		ASSERTD(a); ASSERTKD(BC);
		reg.d[a] = konstd[BC];
		NEXTOP;
	OP(LKF):
		ASSERTF(a); ASSERTKF(BC);
		reg.f[a] = konstf[BC];
		NEXTOP;
	OP(LKS):
		ASSERTS(a); ASSERTKS(BC);
		reg.s[a] = konsts[BC];
		NEXTOP;
	OP(LKP):
		ASSERTA(a); ASSERTKA(BC);
		reg.a[a] = konsta[BC].v;
		reg.atag[a] = konstatag[BC];
		NEXTOP;
	OP(LFP):
		ASSERTA(a); assert(sfunc != NULL); assert(sfunc->ExtraSpace > 0);
		reg.a[a] = f->GetExtra();
		reg.atag[a] = ATAG_FRAMEPOINTER;
		NEXTOP;

	OP(LB):
		ASSERTD(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.d[a] = *(VM_SBYTE *)ptr;
		NEXTOP;
	OP(LB_R):
		ASSERTD(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.d[a] = *(VM_SBYTE *)ptr;
		NEXTOP;
	OP(LH):
		ASSERTD(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.d[a] = *(VM_SHALF *)ptr;
		NEXTOP;
	OP(LH_R):
		ASSERTD(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.d[a] = *(VM_SHALF *)ptr;
		NEXTOP;
	OP(LW):
		ASSERTD(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.d[a] = *(VM_SWORD *)ptr;
		NEXTOP;
	OP(LW_R):
		ASSERTD(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.d[a] = *(VM_SWORD *)ptr;
		NEXTOP;
	OP(LBU):
		ASSERTD(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.d[a] = *(VM_UBYTE *)ptr;
		NEXTOP;
	OP(LBU_R):
		ASSERTD(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.d[a] = *(VM_UBYTE *)ptr;
		NEXTOP;
	OP(LHU):
		ASSERTD(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.d[a] = *(VM_UHALF *)ptr;
		NEXTOP;
	OP(LHU_R):
		ASSERTD(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.d[a] = *(VM_UHALF *)ptr;
		NEXTOP;

	OP(LSP):
		ASSERTF(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.f[a] = *(float *)ptr;
		NEXTOP;
	OP(LSP_R):
		ASSERTF(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.f[a] = *(float *)ptr;
		NEXTOP;
	OP(LDP):
		ASSERTF(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.f[a] = *(double *)ptr;
		NEXTOP;
	OP(LDP_R):
		ASSERTF(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.f[a] = *(double *)ptr;
		NEXTOP;

	OP(LS):
		ASSERTS(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.s[a] = *(FString *)ptr;
		NEXTOP;
	OP(LS_R):
		ASSERTS(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.s[a] = *(FString *)ptr;
		NEXTOP;
	OP(LO):
		ASSERTA(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.a[a] = *(void **)ptr;
		reg.atag[a] = ATAG_OBJECT;
		NEXTOP;
	OP(LO_R):
		ASSERTA(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.a[a] = *(void **)ptr;
		reg.atag[a] = ATAG_OBJECT;
		NEXTOP;
	OP(LP):
		ASSERTA(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.a[a] = *(void **)ptr;
		reg.atag[a] = ATAG_GENERIC;
		NEXTOP;
	OP(LP_R):
		ASSERTA(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.a[a] = *(void **)ptr;
		reg.atag[a] = ATAG_GENERIC;
		NEXTOP;
	OP(LV):
		ASSERTF(a+2); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		{
			float *v = (float *)ptr;
			reg.f[a] = v[0];
			reg.f[a+1] = v[1];
			reg.f[a+2] = v[2];
		}
		NEXTOP;
	OP(LV_R):
		ASSERTF(a+2); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		{
			float *v = (float *)ptr;
			reg.f[a] = v[0];
			reg.f[a+1] = v[1];
			reg.f[a+2] = v[2];
		}
		NEXTOP;
	OP(LBIT):
		ASSERTD(a); ASSERTA(B);
		GETADDR(PB,0,X_READ_NIL);
		reg.d[a] = !!(*(VM_UBYTE *)ptr & C);
		NEXTOP;

	OP(SB):
		ASSERTA(a); ASSERTD(B); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		*(VM_SBYTE *)ptr = reg.d[B];
		NEXTOP;
	OP(SB_R):
		ASSERTA(a); ASSERTD(B); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		*(VM_SBYTE *)ptr = reg.d[B];
		NEXTOP;
	OP(SH):
		ASSERTA(a); ASSERTD(B); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		*(VM_SHALF *)ptr = reg.d[B];
		NEXTOP;
	OP(SH_R):
		ASSERTA(a); ASSERTD(B); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		*(VM_SHALF *)ptr = reg.d[B];
		NEXTOP;
	OP(SW):
		ASSERTA(a); ASSERTD(B); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		*(VM_SWORD *)ptr = reg.d[B];
		NEXTOP;
	OP(SW_R):
		ASSERTA(a); ASSERTD(B); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		*(VM_SWORD *)ptr = reg.d[B];
		NEXTOP;
	OP(SSP):
		ASSERTA(a); ASSERTF(B); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		*(float *)ptr = (float)reg.f[B];
		NEXTOP;
	OP(SSP_R):
		ASSERTA(a); ASSERTF(B); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		*(float *)ptr = (float)reg.f[B];
		NEXTOP;
	OP(SDP):
		ASSERTA(a); ASSERTF(B); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		*(double *)ptr = reg.f[B];
		NEXTOP;
	OP(SDP_R):
		ASSERTA(a); ASSERTF(B); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		*(double *)ptr = reg.f[B];
		NEXTOP;
	OP(SS):
		ASSERTA(a); ASSERTS(B); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		*(FString *)ptr = reg.s[B];
		NEXTOP;
	OP(SS_R):
		ASSERTA(a); ASSERTS(B); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		*(FString *)ptr = reg.s[B];
		NEXTOP;
	OP(SP):
		ASSERTA(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		*(void **)ptr = reg.a[B];
		NEXTOP;
	OP(SP_R):
		ASSERTA(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		*(void **)ptr = reg.a[B];
		NEXTOP;
	OP(SV):
		ASSERTA(a); ASSERTF(B+2); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		{
			float *v = (float *)ptr;
			v[0] = (float)reg.f[B];
			v[1] = (float)reg.f[B+1];
			v[2] = (float)reg.f[B+2];
		}
		NEXTOP;
	OP(SV_R):
		ASSERTA(a); ASSERTF(B+2); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		{
			float *v = (float *)ptr;
			v[0] = (float)reg.f[B];
			v[1] = (float)reg.f[B+1];
			v[2] = (float)reg.f[B+2];
		}
		NEXTOP;
	OP(SBIT):
		ASSERTA(a); ASSERTD(B);
		GETADDR(PA,0,X_WRITE_NIL);
		if (reg.d[B])
		{
			*(VM_UBYTE *)ptr |= C;
		}
		else
		{
			*(VM_UBYTE *)ptr &= ~C;
		}
		NEXTOP;

	OP(MOVE):
		ASSERTD(a); ASSERTD(B);
		reg.d[a] = reg.d[B];
		NEXTOP;
	OP(MOVEF):
		ASSERTF(a); ASSERTF(B);
		reg.f[a] = reg.f[B];
		NEXTOP;
	OP(MOVES):
		ASSERTS(a); ASSERTS(B);
		reg.s[a] = reg.s[B];
		NEXTOP;
	OP(MOVEA):
		ASSERTA(a); ASSERTA(B);
		reg.a[a] = reg.a[B];
		reg.atag[a] = reg.atag[B];
		NEXTOP;
	OP(CAST):
		if (C == CAST_I2F)
		{
			ASSERTF(a); ASSERTD(B);
			reg.f[A] = reg.d[B];
		}
		else if (C == CAST_F2I)
		{
			ASSERTD(a); ASSERTF(B);
			reg.d[A] = (int)reg.f[B];
		}
		else
		{
			DoCast(reg, f, a, B, C);
		}
		NEXTOP;
	OP(DYNCAST_R):
		// UNDONE
		NEXTOP;
	OP(DYNCAST_K):
		// UNDONE
		NEXTOP;
	
	OP(TEST):
		ASSERTD(a);
		if (reg.d[a] != BC)
		{
			pc++;
		}
		NEXTOP;
	OP(JMP):
		pc += JMPOFS(pc - 1);
		NEXTOP;
	OP(IJMP):
		ASSERTD(a);
		pc += (BCs + reg.d[a]);
		assert(pc->op == OP_JMP);
		pc += 1 + JMPOFS(pc);
		NEXTOP;
	OP(PARAMI):
		assert(f->NumParam < sfunc->MaxParam);
		{
			VMValue *param = &reg.param[f->NumParam++];
			::new(param) VMValue(ABCs);
		}
		NEXTOP;
	OP(PARAM):
		assert(f->NumParam < sfunc->MaxParam);
		{
			VMValue *param = &reg.param[f->NumParam++];
			b = B;
			if (b == REGT_NIL)
			{
				::new(param) VMValue();
			}
			else
			{
				switch(b & (REGT_TYPE | REGT_KONST | REGT_ADDROF))
				{
				case REGT_INT:
					assert(C < f->NumRegD);
					::new(param) VMValue(reg.d[C]);
					break;
				case REGT_INT | REGT_ADDROF:
					assert(C < f->NumRegD);
					::new(param) VMValue(&reg.d[C], ATAG_DREGISTER);
					break;
				case REGT_INT | REGT_KONST:
					assert(C < sfunc->NumKonstD);
					::new(param) VMValue(konstd[C]);
					break;
				case REGT_STRING:
					assert(C < f->NumRegS);
					::new(param) VMValue(reg.s[C]);
					break;
				case REGT_STRING | REGT_ADDROF:
					assert(C < f->NumRegS);
					::new(param) VMValue(&reg.s[C], ATAG_SREGISTER);
					break;
				case REGT_STRING | REGT_KONST:
					assert(C < sfunc->NumKonstS);
					::new(param) VMValue(konsts[C]);
					break;
				case REGT_POINTER:
					assert(C < f->NumRegA);
					::new(param) VMValue(reg.a[C], reg.atag[C]);
					break;
				case REGT_POINTER | REGT_ADDROF:
					assert(C < f->NumRegA);
					::new(param) VMValue(&reg.a[C], ATAG_AREGISTER);
					break;
				case REGT_POINTER | REGT_KONST:
					assert(C < sfunc->NumKonstA);
					::new(param) VMValue(konsta[C].v, konstatag[C]);
					break;
				case REGT_FLOAT:
					if (b & REGT_MULTIREG)
					{
						assert(C < f->NumRegF - 2);
						assert(f->NumParam < sfunc->MaxParam - 1);
						::new(param) VMValue(reg.f[C]);
						::new(param+1) VMValue(reg.f[C+1]);
						::new(param+2) VMValue(reg.f[C+2]);
						f->NumParam += 2;
					}
					else
					{
						assert(C < f->NumRegF);
						::new(param) VMValue(reg.f[C]);
					}
					break;
				case REGT_FLOAT | REGT_ADDROF:
					assert(C < f->NumRegF);
					::new(param) VMValue(&reg.f[C], ATAG_FREGISTER);
					break;
				case REGT_FLOAT | REGT_KONST:
					if (b & REGT_MULTIREG)
					{
						assert(C < sfunc->NumKonstF - 2);
						assert(f->NumParam < sfunc->MaxParam - 1);
						::new(param) VMValue(konstf[C]);
						::new(param+1) VMValue(konstf[C+1]);
						::new(param+2) VMValue(konstf[C+2]);
						f->NumParam += 2;
					}
					else
					{
						assert(C < sfunc->NumKonstF);
						::new(param) VMValue(konstf[C]);
					}
					break;
				default:
					assert(0);
					break;
				}
			}
		}
		NEXTOP;
	OP(CALL_K):
		ASSERTKA(a);
		assert(konstatag[a] == ATAG_OBJECT);
		ptr = konsta[a].o;
		goto Do_CALL;
	OP(CALL):
		ASSERTA(a);
		ptr = reg.a[a];
	Do_CALL:
		assert(B <= f->NumParam);
		assert(C <= MAX_RETURNS);
		{
			VMFunction *call = (VMFunction *)ptr;
			VMReturn returns[MAX_RETURNS];
			int numret;

			FillReturns(reg, f, returns, pc, C);
			if (call->Native)
			{
				numret = static_cast<VMNativeFunction *>(call)->NativeCall(stack, reg.param + f->NumParam - B, B, returns, C);
			}
			else
			{
				VMScriptFunction *script = static_cast<VMScriptFunction *>(call);
				VMFrame *newf = stack->AllocFrame(script);
				VMFillParams(reg.param + f->NumParam - B, newf, B);
				try
				{
					numret = Exec(stack, script->Code, returns, C);
				}
				catch(...)
				{
					stack->PopFrame();
					throw;
				}
				stack->PopFrame();
			}
			assert(numret == C && "Number of parameters returned differs from what was expected by the caller");
			for (b = B; b != 0; --b)
			{
				reg.param[--f->NumParam].~VMValue();
			}
			pc += C;			// Skip RESULTs
		}
		NEXTOP;
	OP(TAIL_K):
		ASSERTKA(a);
		assert(konstatag[a] == ATAG_OBJECT);
		ptr = konsta[a].o;
		goto Do_TAILCALL;
	OP(TAIL):
		ASSERTA(a);
		ptr = reg.a[a];
	Do_TAILCALL:
		// Whereas the CALL instruction uses its third operand to specify how many return values
		// it expects, TAIL ignores its third operand and uses whatever was passed to this Exec call.
		assert(B <= f->NumParam);
		assert(C <= MAX_RETURNS);
		{
			VMFunction *call = (VMFunction *)ptr;

			if (call->Native)
			{
				return static_cast<VMNativeFunction *>(call)->NativeCall(stack, reg.param + f->NumParam - B, B, ret, numret);
			}
			else
			{ // FIXME: Not a true tail call
				VMScriptFunction *script = static_cast<VMScriptFunction *>(call);
				VMFrame *newf = stack->AllocFrame(script);
				VMFillParams(reg.param + f->NumParam - B, newf, B);
				try
				{
					numret = Exec(stack, script->Code, ret, numret);
				}
				catch(...)
				{
					stack->PopFrame();
					throw;
				}
				stack->PopFrame();
				return numret;
			}
		}
		NEXTOP;
	OP(RET):
		if (B == REGT_NIL)
		{ // No return values
			return 0;
		}
		assert(ret != NULL || numret == 0);
		{
			int retnum = a & ~RET_FINAL;
			if (retnum < numret)
			{
				SetReturn(reg, f, &ret[retnum], B, C);
			}
			if (a & RET_FINAL)
			{
				return retnum < numret ? retnum + 1 : numret;
			}
		}
		NEXTOP;
	OP(RETI):
		assert(ret != NULL || numret == 0);
		{
			int retnum = a & ~RET_FINAL;
			if (retnum < numret)
			{
				ret[retnum].SetInt(BCs);
			}
			if (a & RET_FINAL)
			{
				return retnum < numret ? retnum + 1 : numret;
			}
		}
		NEXTOP;
	OP(RESULT):
		// This instruction is just a placeholder to indicate where a return
		// value should be stored. It does nothing on its own and should not
		// be executed.
		assert(0);
		NEXTOP;

	OP(TRY):
		assert(try_depth < MAX_TRY_DEPTH);
		if (try_depth >= MAX_TRY_DEPTH)
		{
			THROW(X_TOO_MANY_TRIES);
		}
		assert((pc + JMPOFS(pc - 1))->op == OP_CATCH);
		exception_frames[try_depth++] = pc + JMPOFS(pc - 1);
		NEXTOP;
	OP(UNTRY):
		assert(a <= try_depth);
		try_depth -= a;
		NEXTOP;
	OP(THROW):
		if (a == 0)
		{
			ASSERTA(B);
			throw((VMException *)reg.a[B]);
		}
		else
		{
			ASSERTKA(B);
			assert(konstatag[B] == ATAG_OBJECT);
			throw((VMException *)konsta[B].o);
		}
		NEXTOP;
	OP(CATCH):
		// This instruction is handled by our own catch handler and should
		// not be executed by the normal VM code.
		assert(0);
		NEXTOP;

	OP(BOUND):
		if (reg.d[a] >= BC)
		{
			THROW(X_ARRAY_OUT_OF_BOUNDS);
		}
		NEXTOP;

	OP(CONCAT):
		ASSERTS(a); ASSERTS(B); ASSERTS(C);
		{
			FString *rB = &reg.s[B];
			FString *rC = &reg.s[C];
			FString concat(*rB);
			for (++rB; rB <= rC; ++rB)
			{
				concat += *rB;
			}
			reg.s[a] = concat;
		}
		NEXTOP;
	OP(LENS):
		ASSERTD(a); ASSERTS(B);
		reg.d[a] = (int)reg.s[B].Len();
		NEXTOP;
	
	OP(CMPS):
		// String comparison is a fairly expensive operation, so I've
		// chosen to conserve a few opcodes by condensing all the
		// string comparisons into a single one.
		{
			const FString *b, *c;
			int test, method;
			bool cmp;

			if (a & CMP_BK)
			{
				ASSERTKS(B);
				b = &konsts[B];
			}
			else
			{
				ASSERTS(B);
				b = &reg.s[B];
			}
			if (a & CMP_CK)
			{
				ASSERTKS(C);
				c = &konsts[C];
			}
			else
			{
				ASSERTS(C);
				c = &reg.s[C];
			}
			test = (a & CMP_APPROX) ? b->CompareNoCase(*c) : b->Compare(*c);
			method = a & CMP_METHOD_MASK;
			if (method == CMP_EQ)
			{
				cmp = !test;
			}
			else if (method == CMP_LT)
			{
				cmp = (test < 0);
			}
			else
			{
				assert(method == CMP_LE);
				cmp = (test <= 0);
			}
			if (cmp == (a & CMP_CHECK))
			{
				assert(pc->op == OP_JMP);
				pc += 1 + JMPOFS(pc);
			}
			else
			{
				pc += 1;
			}
		}
		NEXTOP;

	OP(SLL_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] << reg.d[C];
		NEXTOP;
	OP(SLL_RI):
		ASSERTD(a); ASSERTD(B); assert(C <= 31);
		reg.d[a] = reg.d[B] << C;
		NEXTOP;
	OP(SLL_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		reg.d[a] = konstd[B] << reg.d[C];
		NEXTOP;

	OP(SRL_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = (unsigned)reg.d[B] >> reg.d[C];
		NEXTOP;
	OP(SRL_RI):
		ASSERTD(a); ASSERTD(B); assert(C <= 31);
		reg.d[a] = (unsigned)reg.d[B] >> C;
		NEXTOP;
	OP(SRL_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		reg.d[a] = (unsigned)konstd[B] >> C;
		NEXTOP;

	OP(SRA_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] >> reg.d[C];
		NEXTOP;
	OP(SRA_RI):
		ASSERTD(a); ASSERTD(B); assert(C <= 31);
		reg.d[a] = reg.d[B] >> C;
		NEXTOP;
	OP(SRA_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		reg.d[a] = konstd[B] >> reg.d[C];
		NEXTOP;

	OP(ADD_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] + reg.d[C];
		NEXTOP;
	OP(ADD_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = reg.d[B] + konstd[C];
		NEXTOP;
	OP(ADDI):
		ASSERTD(a); ASSERTD(B);
		reg.d[a] = reg.d[B] + Cs;
		NEXTOP;

	OP(SUB_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] - reg.d[C];
		NEXTOP;
	OP(SUB_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = reg.d[B] - konstd[C];
		NEXTOP;
	OP(SUB_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		reg.d[a] = konstd[B] - reg.d[C];
		NEXTOP;

	OP(MUL_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] * reg.d[C];
		NEXTOP;
	OP(MUL_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = reg.d[B] * konstd[C];
		NEXTOP;

	OP(DIV_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] / reg.d[C];
		NEXTOP;
	OP(DIV_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = reg.d[B] / konstd[C];
		NEXTOP;
	OP(DIV_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		reg.d[a] = konstd[B] / reg.d[C];
		NEXTOP;

	OP(MOD_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] % reg.d[C];
		NEXTOP;
	OP(MOD_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = reg.d[B] % konstd[C];
		NEXTOP;
	OP(MOD_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		reg.d[a] = konstd[B] % reg.d[C];
		NEXTOP;

	OP(AND_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] & reg.d[C];
		NEXTOP;
	OP(AND_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = reg.d[B] & konstd[C];
		NEXTOP;

	OP(OR_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] | reg.d[C];
		NEXTOP;
	OP(OR_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = reg.d[B] | konstd[C];
		NEXTOP;

	OP(XOR_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] ^ reg.d[C];
		NEXTOP;
	OP(XOR_RK):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] ^ konstd[C];
		NEXTOP;

	OP(MIN_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] < reg.d[C] ? reg.d[B] : reg.d[C];
		NEXTOP;
	OP(MIN_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = reg.d[B] < konstd[C] ? reg.d[B] : konstd[C];
		NEXTOP;
	OP(MAX_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] > reg.d[C] ? reg.d[B] : reg.d[C];
		NEXTOP;
	OP(MAX_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = reg.d[B] > konstd[C] ? reg.d[B] : konstd[C];
		NEXTOP;

	OP(ABS):
		ASSERTD(a); ASSERTD(B);
		reg.d[a] = abs(reg.d[B]);
		NEXTOP;

	OP(NEG):
		ASSERTD(a); ASSERTD(B);
		reg.d[a] = -reg.d[B];
		NEXTOP;

	OP(NOT):
		ASSERTD(a); ASSERTD(B);
		reg.d[a] = ~reg.d[B];
		NEXTOP;

	OP(SEXT):
		ASSERTD(a); ASSERTD(B);
		reg.d[a] = (VM_SWORD)(reg.d[B] << C) >> C;
		NEXTOP;

	OP(ZAP_R):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] & ZapTable[(reg.d[C] & 15) ^ 15];
		NEXTOP;
	OP(ZAP_I):
		ASSERTD(a); ASSERTD(B);
		reg.d[a] = reg.d[B] & ZapTable[(C & 15) ^ 15];
		NEXTOP;
	OP(ZAPNOT_R):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = reg.d[B] & ZapTable[reg.d[C] & 15];
		NEXTOP;
	OP(ZAPNOT_I):
		ASSERTD(a); ASSERTD(B);
		reg.d[a] = reg.d[B] & ZapTable[C & 15];
		NEXTOP;

	OP(EQ_R):
		ASSERTD(B); ASSERTD(C);
		CMPJMP(reg.d[B] == reg.d[C]);
		NEXTOP;
	OP(EQ_K):
		ASSERTD(B); ASSERTKD(C);
		CMPJMP(reg.d[B] == konstd[C]);
		NEXTOP;
	OP(LT_RR):
		ASSERTD(B); ASSERTD(C);
		CMPJMP(reg.d[B] < reg.d[C]);
		NEXTOP;
	OP(LT_RK):
		ASSERTD(B); ASSERTKD(C);
		CMPJMP(reg.d[B] < konstd[C]);
		NEXTOP;
	OP(LT_KR):
		ASSERTKD(B); ASSERTD(C);
		CMPJMP(konstd[B] < reg.d[C]);
		NEXTOP;
	OP(LE_RR):
		ASSERTD(B); ASSERTD(C);
		CMPJMP(reg.d[B] <= reg.d[C]);
		NEXTOP;
	OP(LE_RK):
		ASSERTD(B); ASSERTKD(C);
		CMPJMP(reg.d[B] <= konstd[C]);
		NEXTOP;
	OP(LE_KR):
		ASSERTKD(B); ASSERTD(C);
		CMPJMP(konstd[B] <= reg.d[C]);
		NEXTOP;
	OP(LTU_RR):
		ASSERTD(B); ASSERTD(C);
		CMPJMP((VM_UWORD)reg.d[B] < (VM_UWORD)reg.d[C]);
		NEXTOP;
	OP(LTU_RK):
		ASSERTD(B); ASSERTKD(C);
		CMPJMP((VM_UWORD)reg.d[B] < (VM_UWORD)konstd[C]);
		NEXTOP;
	OP(LTU_KR):
		ASSERTKD(B); ASSERTD(C);
		CMPJMP((VM_UWORD)konstd[B] < (VM_UWORD)reg.d[C]);
		NEXTOP;
	OP(LEU_RR):
		ASSERTD(B); ASSERTD(C);
		CMPJMP((VM_UWORD)reg.d[B] <= (VM_UWORD)reg.d[C]);
		NEXTOP;
	OP(LEU_RK):
		ASSERTD(B); ASSERTKD(C);
		CMPJMP((VM_UWORD)reg.d[B] <= (VM_UWORD)konstd[C]);
		NEXTOP;
	OP(LEU_KR):
		ASSERTKD(B); ASSERTD(C);
		CMPJMP((VM_UWORD)konstd[B] <= (VM_UWORD)reg.d[C]);
		NEXTOP;

	OP(ADDF_RR):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		reg.f[a] = reg.f[B] + reg.f[C];
		NEXTOP;
	OP(ADDF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		reg.f[a] = reg.f[B] + konstf[C];
		NEXTOP;

	OP(SUBF_RR):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		reg.f[a] = reg.f[B] - reg.f[C];
		NEXTOP;
	OP(SUBF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		reg.f[a] = reg.f[B] - konstf[C];
		NEXTOP;
	OP(SUBF_KR):
		ASSERTF(a); ASSERTKF(B); ASSERTF(C);
		reg.f[a] = konstf[B] - reg.f[C];
		NEXTOP;

	OP(MULF_RR):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		reg.f[a] = reg.f[B] * reg.f[C];
		NEXTOP;
	OP(MULF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		reg.f[a] = reg.f[B] * konstf[C];
		NEXTOP;

	OP(DIVF_RR):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		reg.f[a] = reg.f[B] / reg.f[C];
		NEXTOP;
	OP(DIVF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		reg.f[a] = reg.f[B] / konstf[C];
		NEXTOP;
	OP(DIVF_KR):
		ASSERTF(a); ASSERTKF(B); ASSERTF(C);
		reg.f[a] = konstf[B] / reg.f[C];
		NEXTOP;

	OP(MODF_RR):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		fb = reg.f[B]; fc = reg.f[C];
	Do_MODF:
		reg.f[a] = luai_nummod(fb, fc);
		NEXTOP;
	OP(MODF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		fb = reg.f[B]; fc = konstf[C];
		goto Do_MODF;
		NEXTOP;
	OP(MODF_KR):
		ASSERTF(a); ASSERTKF(B); ASSERTF(C);
		fb = konstf[B]; fc = reg.f[C];
		goto Do_MODF;
		NEXTOP;

	OP(POWF_RR):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		reg.f[a] = pow(reg.f[B], reg.f[C]);
		NEXTOP;
	OP(POWF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		reg.f[a] = pow(reg.f[B], konstf[C]);
		NEXTOP;
	OP(POWF_KR):
		ASSERTF(a); ASSERTKF(B); ASSERTF(C);
		reg.f[a] = pow(konstf[B], reg.f[C]);
		NEXTOP;

	OP(MINF_RR):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		reg.f[a] = reg.f[B] < reg.f[C] ? reg.f[B] : reg.f[C];
		NEXTOP;
	OP(MINF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		reg.f[a] = reg.f[B] < konstf[C] ? reg.f[B] : konstf[C];
		NEXTOP;
	OP(MAXF_RR):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		reg.f[a] = reg.f[B] > reg.f[C] ? reg.f[B] : reg.f[C];
		NEXTOP;
	OP(MAXF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		reg.f[a] = reg.f[B] > konstf[C] ? reg.f[B] : konstf[C];
		NEXTOP;

	OP(ATAN2):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		reg.f[a] = g_atan2(reg.f[B], reg.f[C]) * (180 / M_PI);
		NEXTOP;

	OP(FLOP):
		ASSERTF(a); ASSERTF(B);
		fb = reg.f[B];
		reg.f[a] = (C == FLOP_ABS) ? fabs(fb) : (C == FLOP_NEG) ? -fb : DoFLOP(C, fb);
		NEXTOP;
	
	OP(EQF_R):
		ASSERTF(B); ASSERTF(C);
		if (a & CMP_APPROX)
		{
			CMPJMP(fabs(reg.f[C] - reg.f[B]) < VM_EPSILON);
		}
		else
		{
			CMPJMP(reg.f[C] == reg.f[B]);
		}
		NEXTOP;
	OP(EQF_K):
		ASSERTF(B); ASSERTKF(C);
		if (a & CMP_APPROX)
		{
			CMPJMP(fabs(konstf[C] - reg.f[B]) < VM_EPSILON);
		}
		else
		{
			CMPJMP(konstf[C] == reg.f[B]);
		}
		NEXTOP;
	OP(LTF_RR):
		ASSERTF(B); ASSERTF(C);
		if (a & CMP_APPROX)
		{
			CMPJMP((reg.f[B] - reg.f[C]) < -VM_EPSILON);
		}
		else
		{
			CMPJMP(reg.f[B] < reg.f[C]);
		}
		NEXTOP;
	OP(LTF_RK):
		ASSERTF(B); ASSERTKF(C);
		if (a & CMP_APPROX)
		{
			CMPJMP((reg.f[B] - konstf[C]) < -VM_EPSILON);
		}
		else
		{
			CMPJMP(reg.f[B] < konstf[C]);
		}
		NEXTOP;
	OP(LTF_KR):
		ASSERTKF(B); ASSERTF(C);
		if (a & CMP_APPROX)
		{
			CMPJMP((konstf[B] - reg.f[C]) < -VM_EPSILON);
		}
		else
		{
			CMPJMP(konstf[B] < reg.f[C]);
		}
		NEXTOP;
	OP(LEF_RR):
		ASSERTF(B); ASSERTF(C);
		if (a & CMP_APPROX)
		{
			CMPJMP((reg.f[B] - reg.f[C]) <= -VM_EPSILON);
		}
		else
		{
			CMPJMP(reg.f[B] <= reg.f[C]);
		}
		NEXTOP;
	OP(LEF_RK):
		ASSERTF(B); ASSERTKF(C);
		if (a & CMP_APPROX)
		{
			CMPJMP((reg.f[B] - konstf[C]) <= -VM_EPSILON);
		}
		else
		{
			CMPJMP(reg.f[B] <= konstf[C]);
		}
		NEXTOP;
	OP(LEF_KR):
		ASSERTKF(B); ASSERTF(C);
		if (a & CMP_APPROX)
		{
			CMPJMP((konstf[B] - reg.f[C]) <= -VM_EPSILON);
		}
		else
		{
			CMPJMP(konstf[B] <= reg.f[C]);
		}
		NEXTOP;

	OP(NEGV):
		ASSERTF(a+2); ASSERTF(B+2);
		reg.f[a] = -reg.f[B];
		reg.f[a+1] = -reg.f[B+1];
		reg.f[a+2] = -reg.f[B+2];
		NEXTOP;

	OP(ADDV_RR):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTF(C+2);
		fcp = &reg.f[C];
	Do_ADDV:
		fbp = &reg.f[B];
		reg.f[a] = fbp[0] + fcp[0];
		reg.f[a+1] = fbp[1] + fcp[1];
		reg.f[a+2] = fbp[2] + fcp[2];
		NEXTOP;
	OP(ADDV_RK):
		fcp = &konstf[C];
		goto Do_ADDV;

	OP(SUBV_RR):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTF(C+2);
		fbp = &reg.f[B];
		fcp = &reg.f[C];
	Do_SUBV:
		reg.f[a] = fbp[0] - fcp[0];
		reg.f[a+1] = fbp[1] - fcp[1];
		reg.f[a+2] = fbp[2] - fcp[2];
		NEXTOP;
	OP(SUBV_RK):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTKF(C+2);
		fbp = &reg.f[B];
		fcp = &konstf[C];
		goto Do_SUBV;
	OP(SUBV_KR):
		ASSERTF(A+2); ASSERTKF(B+2); ASSERTF(C+2);
		fbp = &konstf[B];
		fcp = &reg.f[C];
		goto Do_SUBV;

	OP(DOTV_RR):
		ASSERTF(a); ASSERTF(B+2); ASSERTF(C+2);
		reg.f[a] = reg.f[B] * reg.f[C] + reg.f[B+1] * reg.f[C+1] + reg.f[B+2] * reg.f[C+2];
		NEXTOP;
	OP(DOTV_RK):
		ASSERTF(a); ASSERTF(B+2); ASSERTKF(C+2);
		reg.f[a] = reg.f[B] * konstf[C] + reg.f[B+1] * konstf[C+1] + reg.f[B+2] * konstf[C+2];
		NEXTOP;

	OP(CROSSV_RR):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTF(C+2);
		fbp = &reg.f[B];
		fcp = &reg.f[C];
	Do_CROSSV:
		{
			double t[3];
			t[2] = fbp[0] * fcp[1] - fbp[1] * fcp[0];
			t[1] = fbp[2] * fcp[0] - fbp[0] * fcp[2];
			t[0] = fbp[1] * fcp[2] - fbp[2] * fcp[1];
			reg.f[a] = t[0]; reg.f[a+1] = t[1]; reg.f[a+2] = t[2];
		}
		NEXTOP;
	OP(CROSSV_RK):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTKF(C+2);
		fbp = &reg.f[B];
		fcp = &konstf[C];
		goto Do_CROSSV;
	OP(CROSSV_KR):
		ASSERTF(a+2); ASSERTKF(B+2); ASSERTF(C+2);
		fbp = &reg.f[B];
		fcp = &konstf[C];
		goto Do_CROSSV;

	OP(MULVF_RR):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTF(C);
		fc = reg.f[C];
		fbp = &reg.f[B];
	Do_MULV:
		reg.f[a] = fbp[0] * fc;
		reg.f[a+1] = fbp[1] * fc;
		reg.f[a+2] = fbp[2] * fc;
		NEXTOP;
	OP(MULVF_RK):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTKF(C);
		fc = konstf[C];
		fbp = &reg.f[B];
		goto Do_MULV;
	OP(MULVF_KR):
		ASSERTF(a+2); ASSERTKF(B+2); ASSERTF(C);
		fc = reg.f[C];
		fbp = &konstf[B];
		goto Do_MULV;

	OP(LENV):
		ASSERTF(a); ASSERTF(B+2);
		reg.f[a] = g_sqrt(reg.f[B] * reg.f[B] + reg.f[B+1] * reg.f[B+1] + reg.f[B+2] * reg.f[B+2]);
		NEXTOP;

	OP(EQV_R):
		ASSERTF(B+2); ASSERTF(C+2);
		fcp = &reg.f[C];
	Do_EQV:
		if (a & CMP_APPROX)
		{
			CMPJMP(fabs(reg.f[B  ] - fcp[0]) < VM_EPSILON &&
				   fabs(reg.f[B+1] - fcp[1]) < VM_EPSILON &&
				   fabs(reg.f[B+2] - fcp[2]) < VM_EPSILON);
		}
		else
		{
			CMPJMP(reg.f[B] == fcp[0] && reg.f[B+1] == fcp[1] && reg.f[B+2] == fcp[2]);
		}
		NEXTOP;
	OP(EQV_K):
		ASSERTF(B+2); ASSERTKF(C+2);
		fcp = &konstf[C];
		goto Do_EQV;

	OP(ADDA_RR):
		ASSERTA(a); ASSERTA(B); ASSERTD(C);
		c = reg.d[C];
	Do_ADDA:
		if (reg.a[B] == NULL)	// Leave NULL pointers as NULL pointers
		{
			c = 0;
		}
		reg.a[a] = (VM_UBYTE *)reg.a[B] + c;
		reg.atag[a] = c == 0 ? reg.atag[B] : (int)ATAG_GENERIC;
		NEXTOP;
	OP(ADDA_RK):
		ASSERTA(a); ASSERTA(B); ASSERTKD(C);
		c = konstd[C];
		goto Do_ADDA;

	OP(SUBA):
		ASSERTD(a); ASSERTA(B); ASSERTA(C);
		reg.d[a] = (VM_UWORD)((VM_UBYTE *)reg.a[B] - (VM_UBYTE *)reg.a[C]);
		NEXTOP;

	OP(EQA_R):
		ASSERTA(B); ASSERTA(C);
		CMPJMP(reg.a[B] == reg.a[C]);
		NEXTOP;
	OP(EQA_K):
		ASSERTA(B); ASSERTKA(C);
		CMPJMP(reg.a[B] == konsta[C].v);
		NEXTOP;

	OP(NOP):
		NEXTOP;
	}
	}
	catch(VMException *exception)
	{
		// Try to find a handler for the exception.
		PClass *extype = exception->GetClass();

		while(--try_depth >= 0)
		{
			pc = exception_frames[try_depth];
			assert(pc->op == OP_CATCH);
			while (pc->a > 1)
			{
				// CATCH must be followed by JMP if it doesn't terminate a catch chain.
				assert(pc[1].op == OP_JMP);

				PClass *type;
				int b = pc->b;

				if (pc->a == 2)
				{
					ASSERTA(b);
					type = (PClass *)reg.a[b];
				}
				else
				{
					assert(pc->a == 3);
					ASSERTKA(b);
					assert(konstatag[b] == ATAG_OBJECT);
					type = (PClass *)konsta[b].o;
				}
				ASSERTA(pc->c);
				if (type == extype)
				{
					// Found a handler. Store the exception in pC, skip the JMP,
					// and begin executing its code.
					reg.a[pc->c] = exception;
					reg.atag[pc->c] = ATAG_OBJECT;
					pc += 2;
					goto begin;
				}
				// This catch didn't handle it. Try the next one.
				pc += 1 + JMPOFS(pc + 1);
				assert(pc->op == OP_CATCH);
			}
			if (pc->a == 1)
			{
				// Catch any type of VMException. This terminates the chain.
				ASSERTA(pc->c);
				reg.a[pc->c] = exception;
				reg.atag[pc->c] = ATAG_OBJECT;
				pc += 1;
				goto begin;
			}
			// This frame failed. Try the next one out.
		}
		// Nothing caught it. Rethrow and let somebody else deal with it.
		throw;
	}
	return 0;
}

static double DoFLOP(int flop, double v)
{
	switch(flop)
	{
	case FLOP_ABS:		return fabs(v);
	case FLOP_NEG:		return -v;
	case FLOP_EXP:		return g_exp(v);
	case FLOP_LOG:		return g_log(v);
	case FLOP_LOG10:	return g_log10(v);
	case FLOP_SQRT:		return g_sqrt(v);
	case FLOP_CEIL:		return ceil(v);
	case FLOP_FLOOR:	return floor(v);

	case FLOP_ACOS:		return g_acos(v);
	case FLOP_ASIN:		return g_asin(v);
	case FLOP_ATAN:		return g_atan(v);
	case FLOP_COS:		return g_cos(v);
	case FLOP_SIN:		return g_sin(v);
	case FLOP_TAN:		return g_tan(v);

	case FLOP_ACOS_DEG:	return g_acos(v) * (180 / M_PI);
	case FLOP_ASIN_DEG:	return g_asin(v) * (180 / M_PI);
	case FLOP_ATAN_DEG:	return g_atan(v) * (180 / M_PI);
	case FLOP_COS_DEG:	return g_cosdeg(v);
	case FLOP_SIN_DEG:	return g_sindeg(v);
	case FLOP_TAN_DEG:	return g_tan(v * (M_PI / 180));

	case FLOP_COSH:		return g_cosh(v);
	case FLOP_SINH:		return g_sinh(v);
	case FLOP_TANH:		return g_tanh(v);
	}
	assert(0);
	return 0;
}

static void DoCast(const VMRegisters &reg, const VMFrame *f, int a, int b, int cast)
{
	switch (cast)
	{
	case CAST_I2F:
		ASSERTF(a); ASSERTD(b);
		reg.f[a] = reg.d[b];
		break;
	case CAST_I2S:
		ASSERTS(a); ASSERTD(b);
		reg.s[a].Format("%d", reg.d[b]);
		break;

	case CAST_F2I:
		ASSERTD(a); ASSERTF(b);
		reg.d[a] = (int)reg.f[b];
		break;
	case CAST_F2S:
		ASSERTS(a); ASSERTD(b);
		reg.s[a].Format("%.14g", reg.f[b]);
		break;

	case CAST_P2S:
		ASSERTS(a); ASSERTA(b);
		reg.s[a].Format("%s<%p>", reg.atag[b] == ATAG_OBJECT ? "Object" : "Pointer", reg.a[b]);
		break;

	case CAST_S2I:
		ASSERTD(a); ASSERTS(b);
		reg.d[a] = (VM_SWORD)reg.s[b].ToLong();
		break;
	case CAST_S2F:
		ASSERTF(a); ASSERTS(b);
		reg.f[a] = reg.s[b].ToDouble();
		break;

	default:
		assert(0);
	}
}

//===========================================================================
//
// FillReturns
//
// Fills in an array of pointers to locations to store return values in.
//
//===========================================================================

static void FillReturns(const VMRegisters &reg, VMFrame *frame, VMReturn *returns, const VMOP *retval, int numret)
{
	int i, type, regnum;
	VMReturn *ret;

	assert(REGT_INT == 0 && REGT_FLOAT == 1 && REGT_STRING == 2 && REGT_POINTER == 3);

	for (i = 0, ret = returns; i < numret; ++i, ++ret, ++retval)
	{
		assert(retval->op == OP_RESULT);				// opcode
		ret->TagOfs = 0;
		ret->RegType = type = retval->b;
		regnum = retval->c;
		assert(!(type & REGT_KONST));
		type &= REGT_TYPE;
		if (type < REGT_STRING)
		{
			if (type == REGT_INT)
			{
				assert(regnum < frame->NumRegD);
				ret->Location = &reg.d[regnum];
			}
			else // type == REGT_FLOAT
			{
				assert(regnum < frame->NumRegF);
				ret->Location = &reg.f[regnum];
			}
		}
		else if (type == REGT_STRING)
		{
			assert(regnum < frame->NumRegS);
			ret->Location = &reg.s[regnum];
		}
		else
		{
			assert(type == REGT_POINTER);
			assert(regnum < frame->NumRegA);
			ret->Location = &reg.a[regnum];
			ret->TagOfs = (VM_SHALF)(&frame->GetRegATag()[regnum] - (VM_ATAG *)ret->Location);
		}
	}
}

//===========================================================================
//
// SetReturn
//
// Used by script code to set a return value.
//
//===========================================================================

static void SetReturn(const VMRegisters &reg, VMFrame *frame, VMReturn *ret, VM_UBYTE regtype, int regnum)
{
	const void *src;
	VMScriptFunction *func = static_cast<VMScriptFunction *>(frame->Func);

	assert(func != NULL && !func->Native);
	assert((regtype & ~REGT_KONST) == ret->RegType);

	switch (regtype & REGT_TYPE)
	{
	case REGT_INT:
		assert(!(regtype & REGT_MULTIREG));
		if (regtype & REGT_KONST)
		{
			assert(regnum < func->NumKonstD);
			src = &func->KonstD[regnum];
		}
		else
		{
			assert(regnum < frame->NumRegD);
			src = &reg.d[regnum];
		}
		ret->SetInt(*(int *)src);
		break;

	case REGT_FLOAT:
		if (regtype & REGT_KONST)
		{
			assert(regnum + ((regtype & REGT_KONST) ? 2u : 0u) < func->NumKonstF);
			src = &func->KonstF[regnum];
		}
		else
		{
			assert(regnum + ((regtype & REGT_KONST) ? 2u : 0u) < frame->NumRegF);
			src = &reg.f[regnum];
		}
		if (regtype & REGT_MULTIREG)
		{
			ret->SetVector((double *)src);
		}
		else
		{
			ret->SetFloat(*(double *)src);
		}
		break;

	case REGT_STRING:
		assert(!(regtype & REGT_MULTIREG));
		if (regtype & REGT_KONST)
		{
			assert(regnum < func->NumKonstS);
			src = &func->KonstS[regnum];
		}
		else
		{
			assert(regnum < frame->NumRegS);
			src = &reg.s[regnum];
		}
		ret->SetString(*(const FString *)src);
		break;

	case REGT_POINTER:
		assert(!(regtype & REGT_MULTIREG));
		if (regtype & REGT_KONST)
		{
			assert(regnum < func->NumKonstA);
			ret->SetPointer(func->KonstA[regnum].v, func->KonstATags()[regnum]);
		}
		else
		{
			assert(regnum < frame->NumRegA);
			ret->SetPointer(reg.a[regnum], reg.atag[regnum]);
		}
		break;
	}
}
