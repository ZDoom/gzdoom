/*
** vmexec.h
** VM bytecode interpreter
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
** Copyright 2016-2017 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef IMPLEMENT_VMEXEC
#error vmexec.h must not be #included outside vmexec.cpp. Use vm.h instead.
#endif

static int ExecScriptFunc(VMFrameStack *stack, VMReturn *ret, int numret)
{
#if COMPGOTO
	static const void * const ops[256] =
	{
#define xx(op,sym,mode,alt,kreg,ktype) &&op,
#include "vmops.h"
	};
#endif
	//const VMOP *exception_frames[MAX_TRY_DEPTH];
	//int try_depth = 0;
	VMFrame *f = stack->TopFrame();
	VMScriptFunction *sfunc = static_cast<VMScriptFunction *>(f->Func);
	const int *konstd = sfunc->KonstD;
	const double *konstf = sfunc->KonstF;
	const FString *konsts = sfunc->KonstS;
	const FVoidObj *konsta = sfunc->KonstA;
	const VMOP *pc = sfunc->Code;

	assert(!(f->Func->VarFlags & VARF_Native) && "Only script functions should ever reach VMExec");

	const VMRegisters reg(f);

	void *ptr;
	double fb, fc;
	const double *fbp, *fcp;
	int a, b, c;

//begin:
	try
	{
#if !COMPGOTO
	VM_UBYTE op;
	for(;;) switch(op = pc->op, a = pc->a, op)
#else
	pc--;
	NEXTOP;
#endif
	{
#if !COMPGOTO
	default:
		assert(0 && "Undefined opcode hit");
		NEXTOP;
#endif
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
		NEXTOP;

	OP(LK_R) :
		ASSERTD(a); ASSERTD(B);
		reg.d[a] = konstd[reg.d[B] + C];
		NEXTOP;
	OP(LKF_R) :
		ASSERTF(a); ASSERTD(B);
		reg.f[a] = konstf[reg.d[B] + C];
		NEXTOP;
	OP(LKS_R) :
		ASSERTS(a); ASSERTD(B);
		reg.s[a] = konsts[reg.d[B] + C];
		NEXTOP;
	OP(LKP_R) :
		ASSERTA(a); ASSERTD(B);
		b = reg.d[B] + C;
		reg.a[a] = konsta[b].v;
		NEXTOP;

	OP(LFP):
		ASSERTA(a); assert(sfunc != NULL); assert(sfunc->ExtraSpace > 0);
		reg.a[a] = f->GetExtra();
		NEXTOP;

	OP(CLSS):
	{
		ASSERTA(a); ASSERTA(B);
		DObject *o = (DObject*)reg.a[B];
		if (o == nullptr)
		{
			ThrowAbortException(X_READ_NIL, nullptr);
			return 0;
		}
		reg.a[a] = o->GetClass();
		NEXTOP;
	}

	OP(META):
	{
		ASSERTA(a); ASSERTA(B);
		DObject *o = (DObject*)reg.a[B];
		if (o == nullptr)
		{
			ThrowAbortException(X_READ_NIL, nullptr);
			return 0;
		}
		reg.a[a] = o->GetClass()->Meta;
		NEXTOP;
	}

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
	OP(LCS):
		ASSERTS(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.s[a] = *(const char **)ptr;
		NEXTOP;
	OP(LCS_R):
		ASSERTS(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.s[a] = *(const char **)ptr;
		NEXTOP;
	OP(LO):
		ASSERTA(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.a[a] = GC::ReadBarrier(*(DObject **)ptr);
		NEXTOP;
	OP(LO_R):
		ASSERTA(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.a[a] = GC::ReadBarrier(*(DObject **)ptr);
		NEXTOP;
	OP(LP):
		ASSERTA(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		reg.a[a] = *(void **)ptr;
		NEXTOP;
	OP(LP_R):
		ASSERTA(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		reg.a[a] = *(void **)ptr;
		NEXTOP;
	OP(LV2):
		ASSERTF(a+1); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		{
			auto v = (double *)ptr;
			reg.f[a] = v[0];
			reg.f[a+1] = v[1];
		}
		NEXTOP;
	OP(LV2_R):
		ASSERTF(a+1); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		{
			auto v = (double *)ptr;
			reg.f[a] = v[0];
			reg.f[a+1] = v[1];
		}
		NEXTOP;
	OP(LV3):
		ASSERTF(a+2); ASSERTA(B); ASSERTKD(C);
		GETADDR(PB,KC,X_READ_NIL);
		{
			auto v = (double *)ptr;
			reg.f[a] = v[0];
			reg.f[a+1] = v[1];
			reg.f[a+2] = v[2];
		}
		NEXTOP;
	OP(LV3_R):
		ASSERTF(a+2); ASSERTA(B); ASSERTD(C);
		GETADDR(PB,RC,X_READ_NIL);
		{
			auto v = (double *)ptr;
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
#ifdef _DEBUG
		// Should never happen, if it does it indicates a compiler side problem.
		if (((FString*)ptr)->GetChars() == nullptr) ThrowAbortException(X_OTHER, "Uninitialized string");
#endif
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
	OP(SO):
		ASSERTA(a); ASSERTA(B); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		*(void **)ptr = reg.a[B];
		GC::WriteBarrier((DObject*)*(void **)ptr);
		NEXTOP;
	OP(SO_R):
		ASSERTA(a); ASSERTA(B); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		GC::WriteBarrier((DObject*)*(void **)ptr);
		NEXTOP;
	OP(SV2):
		ASSERTA(a); ASSERTF(B+1); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		{
			auto v = (double *)ptr;
			v[0] = reg.f[B];
			v[1] = reg.f[B+1];
		}
		NEXTOP;
	OP(SV2_R):
		ASSERTA(a); ASSERTF(B+1); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		{
			auto v = (double *)ptr;
			v[0] = reg.f[B];
			v[1] = reg.f[B+1];
		}
		NEXTOP;
	OP(SV3):
		ASSERTA(a); ASSERTF(B+2); ASSERTKD(C);
		GETADDR(PA,KC,X_WRITE_NIL);
		{
			auto v = (double *)ptr;
			v[0] = reg.f[B];
			v[1] = reg.f[B+1];
			v[2] = reg.f[B+2];
		}
		NEXTOP;
	OP(SV3_R):
		ASSERTA(a); ASSERTF(B+2); ASSERTD(C);
		GETADDR(PA,RC,X_WRITE_NIL);
		{
			auto v = (double *)ptr;
			v[0] = reg.f[B];
			v[1] = reg.f[B+1];
			v[2] = reg.f[B+2];
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
	{
		ASSERTA(a); ASSERTA(B);
		b = B;
		reg.a[a] = reg.a[b];
		NEXTOP;
	}
	OP(MOVEV2):
	{
		ASSERTF(a); ASSERTF(B);
		b = B;
		reg.f[a] = reg.f[b];
		reg.f[a + 1] = reg.f[b + 1];
		NEXTOP;
	}
	OP(MOVEV3):
	{
		ASSERTF(a); ASSERTF(B);
		b = B;
		reg.f[a] = reg.f[b];
		reg.f[a + 1] = reg.f[b + 1];
		reg.f[a + 2] = reg.f[b + 2];
		NEXTOP;
	}
	OP(DYNCAST_R) :
		ASSERTA(a); ASSERTA(B);	ASSERTA(C);
		b = B;
		reg.a[a] = (reg.a[b] && ((DObject*)(reg.a[b]))->IsKindOf((PClass*)(reg.a[C]))) ? reg.a[b] : nullptr;
		NEXTOP;
	OP(DYNCAST_K) :
		ASSERTA(a); ASSERTA(B);	ASSERTKA(C);
		b = B;
		reg.a[a] = (reg.a[b] && ((DObject*)(reg.a[b]))->IsKindOf((PClass*)(konsta[C].o))) ? reg.a[b] : nullptr;
		NEXTOP;
	OP(DYNCASTC_R) :
		ASSERTA(a); ASSERTA(B);	ASSERTA(C);
		b = B;
		reg.a[a] = (reg.a[b] && ((PClass*)(reg.a[b]))->IsDescendantOf((PClass*)(reg.a[C]))) ? reg.a[b] : nullptr;
		NEXTOP;
	OP(DYNCASTC_K) :
		ASSERTA(a); ASSERTA(B);	ASSERTKA(C);
		b = B;
		reg.a[a] = (reg.a[b] && ((PClass*)(reg.a[b]))->IsDescendantOf((PClass*)(konsta[C].o))) ? reg.a[b] : nullptr;
		NEXTOP;
	OP(CAST):
		if (C == CAST_I2F)
		{
			ASSERTF(a); ASSERTD(B);
			reg.f[a] = reg.d[B];
		}
		else if (C == CAST_F2I)
		{
			ASSERTD(a); ASSERTF(B);
			reg.d[a] = (int)reg.f[B];
		}
		else
		{
			DoCast(reg, f, a, B, C);
		}
		NEXTOP;
	
	OP(CASTB):
		if (C == CASTB_I)
		{
			ASSERTD(a); ASSERTD(B);
			reg.d[a] = !!reg.d[B];
		}
		else if (C == CASTB_F)
		{
			ASSERTD(a); ASSERTF(B);
			reg.d[a] = reg.f[B] != 0;
		}
		else if (C == CASTB_A)
		{
			ASSERTD(a); ASSERTA(B);
			reg.d[a] = reg.a[B] != nullptr;
		}
		else
		{
			ASSERTD(a); ASSERTS(B);
			reg.d[a] = reg.s[B].Len() > 0;
		}
		NEXTOP;
	
	OP(TEST):
		ASSERTD(a);
		if (reg.d[a] != BC)
		{
			pc++;
		}
		NEXTOP;
	OP(TESTN):
		ASSERTD(a);
		if (-reg.d[a] != BC)
		{
			pc++;
		}
		NEXTOP;
	OP(JMP):
		pc += JMPOFS(pc);
		NEXTOP;
	OP(IJMP):
		ASSERTD(a);
		pc += (reg.d[a]);
		assert(pc[1].op == OP_JMP);
		pc += 1 + JMPOFS(pc+1);
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
			b = BC;
			if (a == REGT_NIL)
			{
				::new(param) VMValue();
			}
			else
			{
				switch(a)
				{
				case REGT_INT:
					assert(b < f->NumRegD);
					::new(param) VMValue(reg.d[b]);
					break;
				case REGT_INT | REGT_ADDROF:
					assert(b < f->NumRegD);
					::new(param) VMValue(&reg.d[b]);
					break;
				case REGT_INT | REGT_KONST:
					assert(b < sfunc->NumKonstD);
					::new(param) VMValue(konstd[b]);
					break;
				case REGT_STRING:
					assert(b < f->NumRegS);
					::new(param) VMValue(&reg.s[b]);
					break;
				case REGT_STRING | REGT_ADDROF:
					assert(b < f->NumRegS);
					::new(param) VMValue((void*)&reg.s[b]);	// Note that this may not use the FString* version of the constructor!
					break;
				case REGT_STRING | REGT_KONST:
					assert(b < sfunc->NumKonstS);
					::new(param) VMValue(&konsts[b]);
					break;
				case REGT_POINTER:
					assert(b < f->NumRegA);
					::new(param) VMValue(reg.a[b]);
					break;
				case REGT_POINTER | REGT_ADDROF:
					assert(b < f->NumRegA);
					::new(param) VMValue(&reg.a[b]);
					break;
				case REGT_POINTER | REGT_KONST:
					assert(b < sfunc->NumKonstA);
					::new(param) VMValue(konsta[b].v);
					break;
				case REGT_FLOAT:
					assert(b < f->NumRegF);
					::new(param) VMValue(reg.f[b]);
					break;
				case REGT_FLOAT | REGT_MULTIREG2:
					assert(b < f->NumRegF - 1);
					assert(f->NumParam < sfunc->MaxParam);
					::new(param) VMValue(reg.f[b]);
					::new(param + 1) VMValue(reg.f[b + 1]);
					f->NumParam++;
					break;
				case REGT_FLOAT | REGT_MULTIREG3:
					assert(b < f->NumRegF - 2);
					assert(f->NumParam < sfunc->MaxParam - 1);
					::new(param) VMValue(reg.f[b]);
					::new(param + 1) VMValue(reg.f[b + 1]);
					::new(param + 2) VMValue(reg.f[b + 2]);
					f->NumParam += 2;
					break;
				case REGT_FLOAT | REGT_ADDROF:
					assert(b < f->NumRegF);
					::new(param) VMValue(&reg.f[b]);
					break;
				case REGT_FLOAT | REGT_KONST:
					assert(b < sfunc->NumKonstF);
					::new(param) VMValue(konstf[b]);
					break;
				default:
					assert(0);
					break;
				}
			}
		}
		NEXTOP;
	OP(VTBL):
		ASSERTA(a); ASSERTA(B);
		{
			auto o = (DObject*)reg.a[B];
			if (o == nullptr)
			{
				ThrowAbortException(X_READ_NIL, nullptr);
				return 0;
			}
			auto p = o->GetClass();
			assert(C < p->Virtuals.Size());
			reg.a[a] = p->Virtuals[C];
		}
		NEXTOP;
	OP(SCOPE):
		{
			ASSERTA(a); ASSERTKA(C);
			auto o = (DObject*)reg.a[a];
			if (o == nullptr)
			{
				ThrowAbortException(X_READ_NIL, nullptr);
				return 0;
			}
			FScopeBarrier::ValidateCall(o->GetClass(), (VMFunction*)konsta[C].v, B - 1);
		}
		NEXTOP;

	OP(CALL_K):
		ASSERTKA(a);
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

			b = B;
			FillReturns(reg, f, returns, pc+1, C);
			if (call->VarFlags & VARF_Native)
			{
				try
				{
					VMCycles[0].Unclock();
					numret = static_cast<VMNativeFunction *>(call)->NativeCall(VM_INVOKE(reg.param + f->NumParam - b, b, returns, C, call->RegTypes));
					VMCycles[0].Clock();
				}
				catch (CVMAbortException &err)
				{
					err.MaybePrintMessage();
					err.stacktrace.AppendFormat("Called from %s\n", call->PrintableName.GetChars());
					// PrintParameters(reg.param + f->NumParam - B, B);
					throw;
				}
			}
			else
			{
				auto sfunc = static_cast<VMScriptFunction *>(call);
				numret = sfunc->ScriptCall(sfunc, reg.param + f->NumParam - b, b, returns, C);
			}
			assert(numret == C && "Number of parameters returned differs from what was expected by the caller");
			f->NumParam -= B;
			pc += C;			// Skip RESULTs
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

#if 0
	OP(TRY):
		assert(try_depth < MAX_TRY_DEPTH);
		if (try_depth >= MAX_TRY_DEPTH)
		{
			ThrowAbortException(X_TOO_MANY_TRIES, nullptr);
		}
		assert((pc + JMPOFS(pc) + 1)->op == OP_CATCH);
		exception_frames[try_depth++] = pc + JMPOFS(pc) + 1;
		NEXTOP;
	OP(UNTRY):
		assert(a <= try_depth);
		try_depth -= a;
		NEXTOP;
#endif
	OP(THROW):
#if 0
		if (a == 0)
		{
			ASSERTA(B);
			ThrowVMException((VMException *)reg.a[B]);
		}
		else if (a == 1)
		{
			ASSERTKA(B);
			assert(AssertObject(konsta[B].o));
			ThrowVMException((VMException *)konsta[B].o);
		}
		else
#endif
		{
			ThrowAbortException(EVMAbortException(BC), nullptr);
		}
		NEXTOP;
#if 0
	OP(CATCH):
		// This instruction is handled by our own catch handler and should
		// not be executed by the normal VM code.
		assert(0);
		NEXTOP;
#endif

	OP(BOUND):
		if (reg.d[a] >= BC)
		{
			ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Max.index = %u, current index = %u\n", BC, reg.d[a]);
			return 0;
		}
		else if (reg.d[a] < 0)
		{
			ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Negative current index = %i\n", reg.d[a]);
			return 0;
		}
		NEXTOP;

	OP(BOUND_K):
		ASSERTKD(BC);
		if (reg.d[a] >= konstd[BC])
		{
			ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Max.index = %u, current index = %u\n", konstd[BC], reg.d[a]);
			return 0;
		}
		else if (reg.d[a] < 0)
		{
			ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Negative current index = %i\n", reg.d[a]);
			return 0;
		}
		NEXTOP;

	OP(BOUND_R):
		ASSERTD(B);
		if (reg.d[a] >= reg.d[B])
		{
			ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Max.index = %u, current index = %u\n", reg.d[B], reg.d[a]);
			return 0;
		}
		else if (reg.d[a] < 0)
		{
			ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Negative current index = %i\n", reg.d[a]);
			return 0;
		}
		NEXTOP;

	OP(CONCAT):
		ASSERTS(a); ASSERTS(B); ASSERTS(C);
		reg.s[a] = reg.s[B] + reg.s[C];
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
				assert(pc[1].op == OP_JMP);
				pc += 1 + JMPOFS(pc+1);
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
		reg.d[a] = (unsigned)konstd[B] >> reg.d[C];
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
		if (reg.d[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = reg.d[B] / reg.d[C];
		NEXTOP;
	OP(DIV_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		if (konstd[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = reg.d[B] / konstd[C];
		NEXTOP;
	OP(DIV_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		if (reg.d[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = konstd[B] / reg.d[C];
		NEXTOP;

	OP(DIVU_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		if (reg.d[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = int((unsigned)reg.d[B] / (unsigned)reg.d[C]);
		NEXTOP;
	OP(DIVU_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		if (konstd[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = int((unsigned)reg.d[B] / (unsigned)konstd[C]);
		NEXTOP;
	OP(DIVU_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		if (reg.d[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = int((unsigned)konstd[B] / (unsigned)reg.d[C]);
		NEXTOP;

	OP(MOD_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		if (reg.d[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = reg.d[B] % reg.d[C];
		NEXTOP;
	OP(MOD_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		if (konstd[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = reg.d[B] % konstd[C];
		NEXTOP;
	OP(MOD_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		if (reg.d[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = konstd[B] % reg.d[C];
		NEXTOP;

	OP(MODU_RR):
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		if (reg.d[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = int((unsigned)reg.d[B] % (unsigned)reg.d[C]);
		NEXTOP;
	OP(MODU_RK):
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		if (konstd[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = int((unsigned)reg.d[B] % (unsigned)konstd[C]);
		NEXTOP;
	OP(MODU_KR):
		ASSERTD(a); ASSERTKD(B); ASSERTD(C);
		if (reg.d[C] == 0)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.d[a] = int((unsigned)konstd[B] % (unsigned)reg.d[C]);
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
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
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

	OP(MINU_RR) :
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = (unsigned)reg.d[B] < (unsigned)reg.d[C] ? reg.d[B] : reg.d[C];
		NEXTOP;
	OP(MINU_RK) :
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = (unsigned)reg.d[B] < (unsigned)konstd[C] ? reg.d[B] : konstd[C];
		NEXTOP;
	OP(MAXU_RR) :
		ASSERTD(a); ASSERTD(B); ASSERTD(C);
		reg.d[a] = (unsigned)reg.d[B] > (unsigned)reg.d[C] ? reg.d[B] : reg.d[C];
		NEXTOP;
	OP(MAXU_RK) :
		ASSERTD(a); ASSERTD(B); ASSERTKD(C);
		reg.d[a] = (unsigned)reg.d[B] > (unsigned)konstd[C] ? reg.d[B] : konstd[C];
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
		if (reg.f[C] == 0.)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.f[a] = reg.f[B] / reg.f[C];
		NEXTOP;
	OP(DIVF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		if (konstf[C] == 0.)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.f[a] = reg.f[B] / konstf[C];
		NEXTOP;
	OP(DIVF_KR):
		ASSERTF(a); ASSERTKF(B); ASSERTF(C);
		if (reg.f[C] == 0.)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
		reg.f[a] = konstf[B] / reg.f[C];
		NEXTOP;

	OP(MODF_RR):
		ASSERTF(a); ASSERTF(B); ASSERTF(C);
		fb = reg.f[B]; fc = reg.f[C];
	Do_MODF:
		if (fc == 0.)
		{
			ThrowAbortException(X_DIVISION_BY_ZERO, nullptr);
			return 0;
		}
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
		reg.f[a] = g_pow(reg.f[B], reg.f[C]);
		NEXTOP;
	OP(POWF_RK):
		ASSERTF(a); ASSERTF(B); ASSERTKF(C);
		reg.f[a] = g_pow(reg.f[B], konstf[C]);
		NEXTOP;
	OP(POWF_KR):
		ASSERTF(a); ASSERTKF(B); ASSERTF(C);
		reg.f[a] = g_pow(konstf[B], reg.f[C]);
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

	OP(NEGV2):
		ASSERTF(a+1); ASSERTF(B+1);
		reg.f[a] = -reg.f[B];
		reg.f[a+1] = -reg.f[B+1];
		NEXTOP;

	OP(ADDV2_RR):
		ASSERTF(a+1); ASSERTF(B+1); ASSERTF(C+1);
		fcp = &reg.f[C];
		fbp = &reg.f[B];
		reg.f[a] = fbp[0] + fcp[0];
		reg.f[a+1] = fbp[1] + fcp[1];
		NEXTOP;

	OP(SUBV2_RR):
		ASSERTF(a+1); ASSERTF(B+1); ASSERTF(C+1);
		fbp = &reg.f[B];
		fcp = &reg.f[C];
		reg.f[a] = fbp[0] - fcp[0];
		reg.f[a+1] = fbp[1] - fcp[1];
		NEXTOP;

	OP(DOTV2_RR):
		ASSERTF(a); ASSERTF(B+1); ASSERTF(C+1);
		reg.f[a] = reg.f[B] * reg.f[C] + reg.f[B+1] * reg.f[C+1];
		NEXTOP;

	OP(MULVF2_RR):
		ASSERTF(a+1); ASSERTF(B+1); ASSERTF(C);
		fc = reg.f[C];
		fbp = &reg.f[B];
	Do_MULV2:
		reg.f[a] = fbp[0] * fc;
		reg.f[a+1] = fbp[1] * fc;
		NEXTOP;
	OP(MULVF2_RK):
		ASSERTF(a+1); ASSERTF(B+1); ASSERTKF(C);
		fc = konstf[C];
		fbp = &reg.f[B];
		goto Do_MULV2;

	OP(DIVVF2_RR):
		ASSERTF(a+1); ASSERTF(B+1); ASSERTF(C);
		fc = reg.f[C];
		fbp = &reg.f[B];
	Do_DIVV2:
		reg.f[a] = fbp[0] / fc;
		reg.f[a+1] = fbp[1] / fc;
		NEXTOP;
	OP(DIVVF2_RK):
		ASSERTF(a+1); ASSERTF(B+1); ASSERTKF(C);
		fc = konstf[C];
		fbp = &reg.f[B];
		goto Do_DIVV2;

	OP(LENV2):
		ASSERTF(a); ASSERTF(B+1);
		reg.f[a] = g_sqrt(reg.f[B] * reg.f[B] + reg.f[B+1] * reg.f[B+1]);
		NEXTOP;

	OP(EQV2_R):
		ASSERTF(B+1); ASSERTF(C+1);
		fcp = &reg.f[C];
	Do_EQV2:
		if (a & CMP_APPROX)
		{
			CMPJMP(fabs(reg.f[B  ] - fcp[0]) < VM_EPSILON &&
				   fabs(reg.f[B+1] - fcp[1]) < VM_EPSILON);
		}
		else
		{
			CMPJMP(reg.f[B] == fcp[0] && reg.f[B+1] == fcp[1]);
		}
		NEXTOP;
	OP(EQV2_K):
		ASSERTF(B+1); ASSERTKF(C+1);
		fcp = &konstf[C];
		goto Do_EQV2;

	OP(NEGV3):
		ASSERTF(a+2); ASSERTF(B+2);
		reg.f[a] = -reg.f[B];
		reg.f[a+1] = -reg.f[B+1];
		reg.f[a+2] = -reg.f[B+2];
		NEXTOP;

	OP(ADDV3_RR):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTF(C+2);
		fcp = &reg.f[C];
		fbp = &reg.f[B];
		reg.f[a] = fbp[0] + fcp[0];
		reg.f[a+1] = fbp[1] + fcp[1];
		reg.f[a+2] = fbp[2] + fcp[2];
		NEXTOP;

	OP(SUBV3_RR):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTF(C+2);
		fbp = &reg.f[B];
		fcp = &reg.f[C];
		reg.f[a] = fbp[0] - fcp[0];
		reg.f[a+1] = fbp[1] - fcp[1];
		reg.f[a+2] = fbp[2] - fcp[2];
		NEXTOP;

	OP(DOTV3_RR):
		ASSERTF(a); ASSERTF(B+2); ASSERTF(C+2);
		reg.f[a] = reg.f[B] * reg.f[C] + reg.f[B+1] * reg.f[C+1] + reg.f[B+2] * reg.f[C+2];
		NEXTOP;

	OP(CROSSV_RR):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTF(C+2);
		fbp = &reg.f[B];
		fcp = &reg.f[C];
		{
			double t[3];
			t[2] = fbp[0] * fcp[1] - fbp[1] * fcp[0];
			t[1] = fbp[2] * fcp[0] - fbp[0] * fcp[2];
			t[0] = fbp[1] * fcp[2] - fbp[2] * fcp[1];
			reg.f[a] = t[0]; reg.f[a+1] = t[1]; reg.f[a+2] = t[2];
		}
		NEXTOP;

	OP(MULVF3_RR):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTF(C);
		fc = reg.f[C];
		fbp = &reg.f[B];
	Do_MULV3:
		reg.f[a] = fbp[0] * fc;
		reg.f[a+1] = fbp[1] * fc;
		reg.f[a+2] = fbp[2] * fc;
		NEXTOP;
	OP(MULVF3_RK):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTKF(C);
		fc = konstf[C];
		fbp = &reg.f[B];
		goto Do_MULV3;

	OP(DIVVF3_RR):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTF(C);
		fc = reg.f[C];
		fbp = &reg.f[B];
	Do_DIVV3:
		reg.f[a] = fbp[0] / fc;
		reg.f[a+1] = fbp[1] / fc;
		reg.f[a+2] = fbp[2] / fc;
		NEXTOP;
	OP(DIVVF3_RK):
		ASSERTF(a+2); ASSERTF(B+2); ASSERTKF(C);
		fc = konstf[C];
		fbp = &reg.f[B];
		goto Do_DIVV3;

	OP(LENV3):
		ASSERTF(a); ASSERTF(B+2);
		reg.f[a] = g_sqrt(reg.f[B] * reg.f[B] + reg.f[B+1] * reg.f[B+1] + reg.f[B+2] * reg.f[B+2]);
		NEXTOP;

	OP(EQV3_R):
		ASSERTF(B+2); ASSERTF(C+2);
		fcp = &reg.f[C];
	Do_EQV3:
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
	OP(EQV3_K):
		ASSERTF(B+2); ASSERTKF(C+2);
		fcp = &konstf[C];
		goto Do_EQV3;

	OP(ADDA_RR):
		ASSERTA(a); ASSERTA(B); ASSERTD(C);
		c = reg.d[C];
	Do_ADDA:
		if (reg.a[B] == NULL)	// Leave NULL pointers as NULL pointers
		{
			c = 0;
		}
		reg.a[a] = (VM_UBYTE *)reg.a[B] + c;
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
#if 0
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
					type = (PClass *)konsta[b].o;
				}
				ASSERTA(pc->c);
				if (type == extype)
				{
					// Found a handler. Store the exception in pC, skip the JMP,
					// and begin executing its code.
					reg.a[pc->c] = exception;
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
				pc += 1;
				goto begin;
			}
			// This frame failed. Try the next one out.
		}
		// Nothing caught it. Rethrow and let somebody else deal with it.
		throw;
	}
#endif
	catch (CVMAbortException &err)
	{
		err.MaybePrintMessage();
		err.stacktrace.AppendFormat("Called from %s at %s, line %d\n", sfunc->PrintableName.GetChars(), sfunc->SourceFileName.GetChars(), sfunc->PCToLine(pc));
		// PrintParameters(reg.param + f->NumParam - B, B);
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

	case FLOP_ROUND:	return round(v);
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
	case CAST_U2F:
		ASSERTF(a); ASSERTD(b);
		reg.f[a] = unsigned(reg.d[b]);
		break;
	case CAST_I2S:
		ASSERTS(a); ASSERTD(b);
		reg.s[a].Format("%d", reg.d[b]);
		break;
	case CAST_U2S:
		ASSERTS(a); ASSERTD(b);
		reg.s[a].Format("%u", reg.d[b]);
		break;

	case CAST_F2I:
		ASSERTD(a); ASSERTF(b);
		reg.d[a] = (int)reg.f[b];
		break;
	case CAST_F2U:
		ASSERTD(a); ASSERTF(b);
		reg.d[a] = (int)(unsigned)reg.f[b];
		break;
	case CAST_F2S:
		ASSERTS(a); ASSERTF(b);
		reg.s[a].Format("%.5f", reg.f[b]);	// keep this small. For more precise conversion there should be a conversion function.
		break;
	case CAST_V22S:
		ASSERTS(a); ASSERTF(b+1);
		reg.s[a].Format("(%.5f, %.5f)", reg.f[b], reg.f[b + 1]);
		break;
	case CAST_V32S:
		ASSERTS(a); ASSERTF(b + 2);
		reg.s[a].Format("(%.5f, %.5f, %.5f)", reg.f[b], reg.f[b + 1], reg.f[b + 2]);
		break;

	case CAST_P2S:
	{
		ASSERTS(a); ASSERTA(b);
		if (reg.a[b] == nullptr) reg.s[a] = "null";
		else reg.s[a].Format("%p", reg.a[b]);
		break; 
	}

	case CAST_S2I:
		ASSERTD(a); ASSERTS(b);
		reg.d[a] = (VM_SWORD)reg.s[b].ToLong();
		break;
	case CAST_S2F:
		ASSERTF(a); ASSERTS(b);
		reg.f[a] = reg.s[b].ToDouble();
		break;

	case CAST_S2N:
		ASSERTD(a); ASSERTS(b);
		reg.d[a] = reg.s[b].Len() == 0? NAME_None : FName(reg.s[b]).GetIndex();
		break;

	case CAST_N2S:
	{
		ASSERTS(a); ASSERTD(b);
		FName name = FName(ENamedName(reg.d[b]));
		reg.s[a] = name.IsValidName() ? name.GetChars() : "";
		break; 
	}

	case CAST_S2Co:
		ASSERTD(a); ASSERTS(b);
		reg.d[a] = V_GetColor(NULL, reg.s[b]);
		break;

	case CAST_Co2S:
		ASSERTS(a); ASSERTD(b);
		reg.s[a].Format("%02x %02x %02x", PalEntry(reg.d[b]).r, PalEntry(reg.d[b]).g, PalEntry(reg.d[b]).b);
		break;

	case CAST_S2So:
		ASSERTD(a); ASSERTS(b);
		reg.d[a] = FSoundID(reg.s[b]);
		break;

	case CAST_So2S:
		ASSERTS(a); ASSERTD(b);
		reg.s[a] = soundEngine->GetSoundName(reg.d[b]);
		break;

	case CAST_SID2S:
		ASSERTS(a); ASSERTD(b);
		VM_CastSpriteIDToString(&reg.s[a], reg.d[b]);
		break;

	case CAST_TID2S:
	{
		ASSERTS(a); ASSERTD(b);
		auto tex = TexMan.GetGameTexture(*(FTextureID*)&(reg.d[b]));
		reg.s[a] = tex == nullptr ? "(null)" : tex->GetName().GetChars();
		break;
	}

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

	assert(func != NULL && !(func->VarFlags & VARF_Native));
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
			assert(regnum < func->NumKonstF);
			src = &func->KonstF[regnum];
		}
		else
		{
			assert(regnum < frame->NumRegF);
			src = &reg.f[regnum];
		}
		if (regtype & REGT_MULTIREG3)
		{
			ret->SetVector((double *)src);
		}
		else if (regtype & REGT_MULTIREG2)
		{
			ret->SetVector2((double *)src);
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
			ret->SetPointer(func->KonstA[regnum].v);
		}
		else
		{
			assert(regnum < frame->NumRegA);
			ret->SetPointer(reg.a[regnum]);
		}
		break;
	}
}

static int Exec(VMFunction *func, VMValue *params, int numparams, VMReturn *ret, int numret)
{
	VMCalls[0]++;
	VMFrameStack *stack = &GlobalVMStack;
	VMFrame *newf = stack->AllocFrame(static_cast<VMScriptFunction*>(func));
	VMFillParams(params, newf, numparams);
	try
	{
		numret = ExecScriptFunc(stack, ret, numret);
	}
	catch (...)
	{
		stack->PopFrame();
		throw;
	}
	stack->PopFrame();
	return numret;
}
