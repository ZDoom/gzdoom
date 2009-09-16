#include <math.h>
#include "vm.h"

#define IMPLEMENT_VMEXEC

#if !defined(COMPGOTO) && defined(__GNUC__)
#define COMPGOTO 1
#endif

#if COMPGOTO
#define OP(x)	x
#define NEXTOP	do { unsigned op = *pc; a = pc[1]; pc += 4; goto *ops[op]; } while(0)
#else
#define OP(x)	case OP_##x
#define NEXTOP	break
#endif

#define luai_nummod(a,b)        ((a) - floor((a)/(b))*(b))

#define A				(*(pc - 3))
#define B				(*(pc - 2))
#define C				(			*(pc - 1))
#define Cs				(*(VM_SBYTE *)(pc - 1))
#define BC				(*(VM_UHALF *)(pc - 2))
#define BCs				(*(VM_SHALF *)(pc - 2))

#ifdef WORDS_BIGENDIAN
#define JMPOFS(x)		((*(VM_SWORD *)(x) << 8) >> 6)
#else
#define JMPOFS(x)		((*(VM_SWORD *)(x) >> 6) & ~3)
#endif

#define KC				(konst[C])
#define RC				(reg.i[C])

#define PA				(reg.a[A])
#define PB				(reg.a[B])

#define ASSERTD(x)		assert((unsigned)(x) < f->NumRegD)
#define ASSERTF(x)		assert((unsigned)(x) < f->NumRegF)
#define ASSERTA(x)		assert((unsigned)(x) < f->NumRegA)
#define ASSERTS(x)		assert((unsigned)(x) < f->NumRegS)

#define ASSERTKD(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstD)
#define ASSERTKF(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstF)
#define ASSERTKA(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstA)
#define ASSERTKS(x)		assert(sfunc != NULL && (unsigned)(x) < sfunc->NumKonstS)

#define THROW(x)

#define CMPJMP(test) \
	if ((test) == (a & CMP_CHECK)) { \
		assert(*pc == OP_JMP); \
		pc += 4 + JMPOFS(pc); \
	} else { \
		pc += 4; \
	}

enum
{
	X_READ_NIL,
	X_WRITE_NIL,
	X_TOO_MANY_TRIES,
};

#define GETADDR(a,o,x) \
	if (a == 0) { THROW(x); } \
	ptr = (VM_SBYTE *)a + x \

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

int (*VMExec)(VMFrameStack *stack, const VM_UBYTE *pc, VMReturn *ret, int numret) =
#ifdef NDEBUG
VMExec_Unchecked::Exec
#else
VMExec_Checked::Exec
#endif
;

void VMSelectEngine(EVMEngine engine)
{
	switch (engine)
	{
	case VMEngine_Default:
#ifdef NDEBUG
		VMExec = VMExec_Unchecked::Exec;
#else
		VMExec = VMExec_Checked::Exec;
#endif
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
// Takes parameters from the paramater stack and stores them in the callee's
// registers.
//
//===========================================================================

void VMFillParams(VMValue *params, VMFrame *callee, int numparam)
{
	unsigned int regd, regf, regs, rega;
	VMScriptFunction *calleefunc = static_cast<VMScriptFunction *>(callee->Func);
	const VMRegisters calleereg(callee);

	assert(calleefunc != NULL && !calleefunc->Native);
	assert(numparam == calleefunc->NumArgs);
	assert(REGT_INT == 0 && REGT_FLOAT == 1 && REGT_STRING == 2 && REGT_POINTER == 3);

	regd = regf = regs = rega = 0;
	for (int i = 0; i < numparam; ++i)
	{
		VMValue &p = params[i];
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
