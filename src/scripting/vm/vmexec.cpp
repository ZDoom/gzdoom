/*
** vmexec.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
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

#include <math.h>
#include <v_video.h>
#include <s_sound.h>
#include "dobject.h"
#include "xs_Float.h"
#include "r_state.h"
#include "textures/textures.h"
#include "math/cmath.h"

// This must be a separate function because the VC compiler would otherwise allocate memory on the stack for every separate instance of the exception object that may get thrown.
void ThrowAbortException(EVMAbortException reason, const char *moreinfo, ...);
// intentionally implemented in a different source file tp prevent inlining.
void ThrowVMException(VMException *x);

#define IMPLEMENT_VMEXEC

#if !defined(COMPGOTO) && defined(__GNUC__)
#define COMPGOTO 1
#endif

#if COMPGOTO
#define OP(x)	x
#define NEXTOP	do { pc++; unsigned op = pc->op; a = pc->a; goto *ops[op]; } while(0)
#else
#define OP(x)	case OP_##x
#define NEXTOP	pc++; break
#endif

#define luai_nummod(a,b)        ((a) - floor((a)/(b))*(b))

#define A				(pc[0].a)
#define B				(pc[0].b)
#define C				(pc[0].c)
#define Cs				(pc[0].cs)
#define BC				(pc[0].i16u)
#define BCs				(pc[0].i16)
#define ABCs			(pc[0].i24)
#define JMPOFS(x)		((x)->i24)

#define KC				(konstd[C])
#define RC				(reg.d[C])

#define PA				(reg.a[A])
#define PB				(reg.a[B])

#define ASSERTD(x)		assert((unsigned)(x) < f->NumRegD)
#define ASSERTF(x)		assert((unsigned)(x) < f->NumRegF)
#define ASSERTA(x)		assert((unsigned)(x) < f->NumRegA)
#define ASSERTS(x)		assert((unsigned)(x) < f->NumRegS)
#define ASSERTO(x)		assert((unsigned)(x) < f->NumRegA && reg.atag[x] == ATAG_OBJECT)

#define ASSERTKD(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstD)
#define ASSERTKF(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstF)
#define ASSERTKA(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstA)
#define ASSERTKS(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstS)

#define CMPJMP(test) \
	if ((test) == (a & CMP_CHECK)) { \
		assert(pc[1].op == OP_JMP); \
		pc += 1 + JMPOFS(pc+1); \
	} else { \
		pc += 1; \
	}

#define GETADDR(a,o,x) \
	if (a == NULL) { ThrowAbortException(x, nullptr); } \
	ptr = (VM_SBYTE *)a + o

static const VM_UWORD ZapTable[16] =
{
	0x00000000, 0x000000FF, 0x0000FF00, 0x0000FFFF,
	0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00FFFFFF,
	0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF,
	0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF
};

#ifdef NDEBUG
#define WAS_NDEBUG 1
#else
#define WAS_NDEBUG 0
#endif

#if WAS_NDEBUG
#undef NDEBUG
#endif
#undef assert
#include <assert.h>
struct VMExec_Checked
{
#include "vmexec.h"
};
#if WAS_NDEBUG
#define NDEBUG
#endif

#if !WAS_NDEBUG
#define NDEBUG
#endif
#undef assert
#include <assert.h>
struct VMExec_Unchecked
{
#include "vmexec.h"
};
#if !WAS_NDEBUG
#undef NDEBUG
#endif
#undef assert
#include <assert.h>

int (*VMExec)(VMFrameStack *stack, const VMOP *pc, VMReturn *ret, int numret) =
#ifdef NDEBUG
VMExec_Unchecked::Exec
#else
VMExec_Checked::Exec
#endif
;

// Note: If the VM is being used in multiple threads, this should be declared as thread_local.
// ZDoom doesn't need this at the moment so this is disabled.

thread_local VMFrameStack GlobalVMStack;


//===========================================================================
//
// VMSelectEngine
//
// Selects the VM engine, either checked or unchecked. Default will decide
// based on the NDEBUG preprocessor definition.
//
//===========================================================================

void VMSelectEngine(EVMEngine engine)
{
	switch (engine)
	{
	case VMEngine_Default:
#ifdef NDEBUG
		VMExec = VMExec_Unchecked::Exec;
#else
#endif
		VMExec = VMExec_Checked::Exec;
		break;
	case VMEngine_Unchecked:
		VMExec = VMExec_Unchecked::Exec;
		break;
	case VMEngine_Checked:
		VMExec = VMExec_Checked::Exec;
		break;
	}
}

//===========================================================================
//
// VMFillParams
//
// Takes parameters from the parameter stack and stores them in the callee's
// registers.
//
//===========================================================================

void VMFillParams(VMValue *params, VMFrame *callee, int numparam)
{
	unsigned int regd, regf, regs, rega;
	VMScriptFunction *calleefunc = static_cast<VMScriptFunction *>(callee->Func);
	const VMRegisters calleereg(callee);

	assert(calleefunc != NULL && !calleefunc->Native);
	assert(numparam == calleefunc->NumArgs || ((int)calleefunc->DefaultArgs.Size() == calleefunc->NumArgs));
	assert(REGT_INT == 0 && REGT_FLOAT == 1 && REGT_STRING == 2 && REGT_POINTER == 3);

	regd = regf = regs = rega = 0;
	for (int i = 0; i < calleefunc->NumArgs; ++i)
	{
		// get all actual parameters and fill the rest from the defaults.
		VMValue &p = i < numparam? params[i] : calleefunc->DefaultArgs[i];
		if (p.Type < REGT_STRING)
		{
			if (p.Type == REGT_INT)
			{
				calleereg.d[regd++] = p.i;
			}
			else // p.Type == REGT_FLOAT
			{
				calleereg.f[regf++] = p.f;
			}
		}
		else if (p.Type == REGT_STRING)
		{
			calleereg.s[regs++] = p.s();
		}
		else
		{
			assert(p.Type == REGT_POINTER);
			calleereg.a[rega] = p.a;
			calleereg.atag[rega++] = p.atag;
		}
	}
}


