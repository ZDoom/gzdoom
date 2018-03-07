/*
** vm.h
** VM <-> native interface
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
** Copyright 2016 Christoph Oelckers
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

#ifndef VM_H
#define VM_H

#include "zstring.h"
#include "autosegs.h"
#include "vectors.h"
#include "cmdlib.h"
#include "doomerrors.h"
#include "memarena.h"
#include "scripting/backend/scopebarrier.h"

class DObject;

extern FMemArena ClassDataAllocator;

#define MAX_RETURNS		8	// Maximum number of results a function called by script code can return
#define MAX_TRY_DEPTH	8	// Maximum number of nested TRYs in a single function


typedef unsigned char		VM_UBYTE;
typedef signed char			VM_SBYTE;
typedef unsigned short		VM_UHALF;
typedef signed short		VM_SHALF;
typedef unsigned int		VM_UWORD;
typedef signed int			VM_SWORD;

#define VM_EPSILON			(1/65536.0)

// Register types for VMParam
enum
{
	REGT_INT		= 0,
	REGT_FLOAT		= 1,
	REGT_STRING		= 2,
	REGT_POINTER	= 3,
	REGT_TYPE		= 3,

	REGT_KONST		= 4,
	REGT_MULTIREG2	= 8,
	REGT_MULTIREG3	= 16,	// (e.g. a vector)
	REGT_MULTIREG	= 24,
	REGT_ADDROF		= 32,	// used with PARAM: pass address of this register

	REGT_NIL		= 128	// parameter was omitted
};

#define RET_FINAL	(0x80)	// Used with RET and RETI in the destination slot: this is the final return value


enum EVMAbortException
{
	X_OTHER,
	X_READ_NIL,
	X_WRITE_NIL,
	X_TOO_MANY_TRIES,
	X_ARRAY_OUT_OF_BOUNDS,
	X_DIVISION_BY_ZERO,
	X_BAD_SELF,
	X_FORMAT_ERROR
};

class CVMAbortException : public CDoomError
{
public:
	static FString stacktrace;
	CVMAbortException(EVMAbortException reason, const char *moreinfo, va_list ap);
	void MaybePrintMessage();
};

// This must be a separate function because the VC compiler would otherwise allocate memory on the stack for every separate instance of the exception object that may get thrown.
void ThrowAbortException(EVMAbortException reason, const char *moreinfo, ...);

struct VMReturn
{
	void *Location;
	VM_UBYTE RegType;	// Same as VMParam RegType, except REGT_KONST is invalid; only used by asserts

	void SetInt(int val)
	{
		assert(RegType == REGT_INT);
		*(int *)Location = val;
	}
	void SetFloat(double val)
	{
		assert(RegType == REGT_FLOAT);
		*(double *)Location = val;
	}
	void SetVector(const double val[3])
	{	
		assert(RegType == (REGT_FLOAT|REGT_MULTIREG3));
		((double *)Location)[0] = val[0];
		((double *)Location)[1] = val[1];
		((double *)Location)[2] = val[2];
	}
	void SetVector(const DVector3 &val)
	{
		assert(RegType == (REGT_FLOAT | REGT_MULTIREG3));
		((double *)Location)[0] = val[0];
		((double *)Location)[1] = val[1];
		((double *)Location)[2] = val[2];
	}
	void SetVector2(const double val[2])
	{
		assert(RegType == (REGT_FLOAT|REGT_MULTIREG2));
		((double *)Location)[0] = val[0];
		((double *)Location)[1] = val[1];
	}
	void SetVector2(const DVector2 &val)
	{
		assert(RegType == (REGT_FLOAT | REGT_MULTIREG2));
		((double *)Location)[0] = val[0];
		((double *)Location)[1] = val[1];
	}
	void SetString(const FString &val)
	{
		assert(RegType == REGT_STRING);
		*(FString *)Location = val;
	}

	void SetPointer(void *val)
	{
		assert(RegType == REGT_POINTER);
		*(void **)Location = val;
	}

	void SetObject(DObject *val)
	{
		assert(RegType == REGT_POINTER);
		*(void **)Location = val;
	}

	void IntAt(int *loc)
	{
		Location = loc;
		RegType = REGT_INT;
	}
	void FloatAt(double *loc)
	{
		Location = loc;
		RegType = REGT_FLOAT;
	}
	void StringAt(FString *loc)
	{
		Location = loc;
		RegType = REGT_STRING;
	}
	void PointerAt(void **loc)
	{
		Location = loc;
		RegType = REGT_POINTER;
	}
	VMReturn() { }
	VMReturn(int *loc) { IntAt(loc); }
	VMReturn(double *loc) { FloatAt(loc); }
	VMReturn(FString *loc) { StringAt(loc); }
	VMReturn(void **loc) { PointerAt(loc); }
};

struct VMRegisters;


struct VMValue
{
	union
	{
		int i;
		void *a;
		double f;
		struct { int pad[3]; VM_UBYTE Type; };
		struct { int foo[4]; } biggest;
		const FString *sp;
	};

	// Unfortunately, FString cannot be used directly.
	// Fortunately, it is relatively simple.
	const FString &s() const { return *sp; }

	VMValue()
	{
		a = NULL;
		Type = REGT_NIL;
	}
	VMValue(const VMValue &o)
	{
		biggest = o.biggest;
	}
	VMValue(int v)
	{
		i = v;
		Type = REGT_INT;
	}
	VMValue(double v)
	{
		f = v;
		Type = REGT_FLOAT;
	}
	VMValue(const char *s) = delete;
	VMValue(const FString &s) = delete;

	VMValue(const FString *s)
	{
		sp = s;
		Type = REGT_STRING;
	}
	VMValue(DObject *v)
	{
		a = v;
		Type = REGT_POINTER;
	}
	VMValue(void *v)
	{
		a = v;
		Type = REGT_POINTER;
	}
	VMValue &operator=(const VMValue &o)
	{
		biggest = o.biggest;
		return *this;
	}
	VMValue &operator=(int v)
	{
		i = v;
		Type = REGT_INT;
		return *this;
	}
	VMValue &operator=(double v)
	{
		f = v;
		Type = REGT_FLOAT;
		return *this;
	}
	VMValue &operator=(const FString *v)
	{
		sp = v;
		Type = REGT_STRING;
		return *this;
	}
	VMValue &operator=(const FString &v) = delete;
	VMValue &operator=(const char *v) = delete;
	VMValue &operator=(DObject *v)
	{
		a = v;
		Type = REGT_POINTER;
		return *this;
	}
	int ToInt()
	{
		if (Type == REGT_INT)
		{
			return i;
		}
		if (Type == REGT_FLOAT)
		{
			return int(f);
		}
		if (Type == REGT_STRING)
		{
			return s().ToLong();
		}
		// FIXME
		return 0;
	}
	double ToDouble()
	{
		if (Type == REGT_FLOAT)
		{
			return f;
		}
		if (Type == REGT_INT)
		{
			return i;
		}
		if (Type == REGT_STRING)
		{
			return s().ToDouble();
		}
		// FIXME
		return 0;
	}
};

class VMFunction
{
public:
	bool Unsafe = false;
	int VarFlags = 0; // [ZZ] this replaces 5+ bool fields
	uint8_t ImplicitArgs = 0;	// either 0 for static, 1 for method or 3 for action
	unsigned VirtualIndex = ~0u;
	FName Name;
	TArray<VMValue> DefaultArgs;
	FString PrintableName;	// so that the VM can print meaningful info if something in this function goes wrong.

	class PPrototype *Proto;

	VMFunction(FName name = NAME_None) : ImplicitArgs(0), Name(name), Proto(NULL)
	{
		AllFunctions.Push(this);
	}
	virtual ~VMFunction() {}

	void *operator new(size_t size)
	{
		return ClassDataAllocator.Alloc(size);
	}

	void operator delete(void *block) {}
	void operator delete[](void *block) {}
	static void DeleteAll()
	{
		for (auto f : AllFunctions)
		{
			f->~VMFunction();
		}
		AllFunctions.Clear();
	}
	static TArray<VMFunction *> AllFunctions;
protected:
};

class VMNativeFunction : public VMFunction
{
public:
	typedef int (*NativeCallType)(VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret);

	// 8 is VARF_Native. I can't write VARF_Native because of circular references between this and dobject/dobjtype.
	VMNativeFunction() : NativeCall(NULL) { VarFlags = 8; }
	VMNativeFunction(NativeCallType call) : NativeCall(call) { VarFlags = 8; }
	VMNativeFunction(NativeCallType call, FName name) : VMFunction(name), NativeCall(call) { VarFlags = 8; }

	// Return value is the number of results.
	NativeCallType NativeCall;
};

int VMCall(VMFunction *func, VMValue *params, int numparams, VMReturn *results, int numresults/*, VMException **trap = NULL*/);

// Use this in the prototype for a native function.
#define VM_ARGS			VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret
#define VM_ARGS_NAMES	param, defaultparam, numparam, ret, numret

// Use these to collect the parameters in a native function.
// variable name <x> at position <p>
void NullParam(const char *varname);

#ifndef NDEBUG
bool AssertObject(void * ob);
#endif

#define PARAM_NULLCHECK(ptr, var) (ptr == nullptr? NullParam(#var), ptr : ptr)

#define ASSERTINT(p)					assert((p).Type == REGT_INT)
#define ASSERTFLOAT(p)					assert((p).Type == REGT_FLOAT)
#define ASSERTSTRING(p)					assert((p).Type == REGT_STRING)
#define ASSERTOBJECT(p)					assert((p).Type == REGT_POINTER && AssertObject(p.a))
#define ASSERTPOINTER(p)				assert((p).Type == REGT_POINTER)

// For required parameters.
#define PARAM_INT_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_INT); int x = param[p].i;
#define PARAM_UINT_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_INT); unsigned x = param[p].i;
#define PARAM_BOOL_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_INT); bool x = !!param[p].i;
#define PARAM_NAME_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_INT); FName x = ENamedName(param[p].i);
#define PARAM_SOUND_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_INT); FSoundID x = param[p].i;
#define PARAM_COLOR_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_INT); PalEntry x; x.d = param[p].i;
#define PARAM_FLOAT_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_FLOAT); double x = param[p].f;
#define PARAM_ANGLE_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_FLOAT); DAngle x = param[p].f;
#define PARAM_STRING_AT(p,x)		assert((p) < numparam); assert(param[p].Type == REGT_STRING); FString x = param[p].s();
#define PARAM_STATE_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_INT); FState *x = (FState *)StateLabels.GetState(param[p].i, self->GetClass());
#define PARAM_STATE_ACTION_AT(p,x)	assert((p) < numparam); assert(param[p].Type == REGT_INT); FState *x = (FState *)StateLabels.GetState(param[p].i, stateowner->GetClass());
#define PARAM_POINTER_AT(p,x,type)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER); type *x = (type *)param[p].a;
#define PARAM_POINTERTYPE_AT(p,x,type)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER); type x = (type )param[p].a;
#define PARAM_OBJECT_AT(p,x,type)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER && AssertObject(param[p].a)); type *x = (type *)param[p].a; assert(x == NULL || x->IsKindOf(RUNTIME_CLASS(type)));
#define PARAM_CLASS_AT(p,x,base)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER); base::MetaClass *x = (base::MetaClass *)param[p].a; assert(x == NULL || x->IsDescendantOf(RUNTIME_CLASS(base)));
#define PARAM_POINTER_NOT_NULL_AT(p,x,type)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER); type *x = (type *)PARAM_NULLCHECK(param[p].a, #x);
#define PARAM_OBJECT_NOT_NULL_AT(p,x,type)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER && (AssertObject(param[p].a))); type *x = (type *)PARAM_NULLCHECK(param[p].a, #x); assert(x == NULL || x->IsKindOf(RUNTIME_CLASS(type)));
#define PARAM_CLASS_NOT_NULL_AT(p,x,base)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER); base::MetaClass *x = (base::MetaClass *)PARAM_NULLCHECK(param[p].a, #x); assert(x == NULL || x->IsDescendantOf(RUNTIME_CLASS(base)));

#define PARAM_EXISTS(p)					((p) < numparam)

#define PARAM_INT_DEF_AT(p,x)			int x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = param[p].i; } else { ASSERTINT(defaultparam[p]); x = defaultparam[p].i; }
#define PARAM_UINT_DEF_AT(p,x)			unsigned x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = param[p].i; } else { ASSERTINT(defaultparam[p]); x = defaultparam[p].i; }
#define PARAM_BOOL_DEF_AT(p,x)			bool x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = !!param[p].i; } else { ASSERTINT(defaultparam[p]); x = !!defaultparam[p].i; }
#define PARAM_NAME_DEF_AT(p,x)			FName x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = ENamedName(param[p].i); } else { ASSERTINT(defaultparam[p]); x = ENamedName(defaultparam[p].i); }
#define PARAM_SOUND_DEF_AT(p,x)			FSoundID x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = FSoundID(param[p].i); } else { ASSERTINT(defaultparam[p]); x = FSoundID(defaultparam[p].i); }
#define PARAM_COLOR_DEF_AT(p,x)			PalEntry x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = param[p].i; } else { ASSERTINT(defaultparam[p]); x = defaultparam[p].i; }
#define PARAM_FLOAT_DEF_AT(p,x)			double x; if (PARAM_EXISTS(p)) { ASSERTFLOAT(param[p]); x = param[p].f; } else { ASSERTFLOAT(defaultparam[p]); x = defaultparam[p].f; }
#define PARAM_ANGLE_DEF_AT(p,x)			DAngle x; if (PARAM_EXISTS(p)) { ASSERTFLOAT(param[p]); x = param[p].f; } else { ASSERTFLOAT(defaultparam[p]); x = defaultparam[p].f; }
#define PARAM_STRING_DEF_AT(p,x)		FString x; if (PARAM_EXISTS(p)) { ASSERTSTRING(param[p]); x = param[p].s(); } else { ASSERTSTRING(defaultparam[p]); x = defaultparam[p].s(); }
#define PARAM_STATE_DEF_AT(p,x)			FState *x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = (FState*)StateLabels.GetState(param[p].i, self->GetClass()); } else { ASSERTINT(defaultparam[p]); x = (FState*)StateLabels.GetState(defaultparam[p].i, self->GetClass()); }
#define PARAM_STATE_ACTION_DEF_AT(p,x)	FState *x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = (FState*)StateLabels.GetState(param[p].i, stateowner->GetClass()); } else { ASSERTINT(defaultparam[p]); x = (FState*)StateLabels.GetState(defaultparam[p].i, stateowner->GetClass()); }
#define PARAM_POINTER_DEF_AT(p,x,t)		t *x; if (PARAM_EXISTS(p)) { ASSERTPOINTER(param[p]); x = (t*)param[p].a; } else { ASSERTPOINTER(defaultparam[p]); x = (t*)defaultparam[p].a; }
#define PARAM_OBJECT_DEF_AT(p,x,t)		t *x; if (PARAM_EXISTS(p)) { ASSERTOBJECT(param[p]); x = (t*)param[p].a; } else { ASSERTOBJECT(defaultparam[p]); x = (t*)defaultparam[p].a; }
#define PARAM_CLASS_DEF_AT(p,x,t)		t::MetaClass *x; if (PARAM_EXISTS(p)) { x = (t::MetaClass*)param[p].a; } else { ASSERTPOINTER(defaultparam[p]); x = (t::MetaClass*)defaultparam[p].a; }
#define PARAM_CLASS_DEF_NOT_NULL_AT(p,x,t)		t::MetaClass *x; if (PARAM_EXISTS(p)) { x = (t::MetaClass*)PARAM_NULLCHECK(param[p].a, #x); } else { x = (t::MetaClass*)PARAM_NULLCHECK(defaultparam[p].a, #x); }

// The above, but with an automatically increasing position index.
#define PARAM_PROLOGUE				int paramnum = -1;

#define PARAM_INT(x)				++paramnum; PARAM_INT_AT(paramnum,x)
#define PARAM_UINT(x)				++paramnum; PARAM_UINT_AT(paramnum,x)
#define PARAM_BOOL(x)				++paramnum; PARAM_BOOL_AT(paramnum,x)
#define PARAM_NAME(x)				++paramnum; PARAM_NAME_AT(paramnum,x)
#define PARAM_SOUND(x)				++paramnum; PARAM_SOUND_AT(paramnum,x)
#define PARAM_COLOR(x)				++paramnum; PARAM_COLOR_AT(paramnum,x)
#define PARAM_FLOAT(x)				++paramnum; PARAM_FLOAT_AT(paramnum,x)
#define PARAM_ANGLE(x)				++paramnum; PARAM_ANGLE_AT(paramnum,x)
#define PARAM_STRING(x)				++paramnum; PARAM_STRING_AT(paramnum,x)
#define PARAM_STATE(x)				++paramnum; PARAM_STATE_AT(paramnum,x)
#define PARAM_STATE_ACTION(x)		++paramnum; PARAM_STATE_ACTION_AT(paramnum,x)
#define PARAM_POINTER(x,type)		++paramnum; PARAM_POINTER_AT(paramnum,x,type)
#define PARAM_POINTERTYPE(x,type)	++paramnum; PARAM_POINTERTYPE_AT(paramnum,x,type)
#define PARAM_OBJECT(x,type)		++paramnum; PARAM_OBJECT_AT(paramnum,x,type)
#define PARAM_CLASS(x,base)			++paramnum; PARAM_CLASS_AT(paramnum,x,base)
#define PARAM_CLASS(x,base)			++paramnum; PARAM_CLASS_AT(paramnum,x,base)
#define PARAM_POINTER_NOT_NULL(x,type)		++paramnum; PARAM_POINTER_NOT_NULL_AT(paramnum,x,type)
#define PARAM_OBJECT_NOT_NULL(x,type)		++paramnum; PARAM_OBJECT_NOT_NULL_AT(paramnum,x,type)
#define PARAM_CLASS_NOT_NULL(x,base)		++paramnum; PARAM_CLASS_NOT_NULL_AT(paramnum,x,base)

#define PARAM_INT_DEF(x)			++paramnum; PARAM_INT_DEF_AT(paramnum,x)
#define PARAM_UINT_DEF(x)			++paramnum; PARAM_UINT_DEF_AT(paramnum,x)
#define PARAM_BOOL_DEF(x)			++paramnum; PARAM_BOOL_DEF_AT(paramnum,x)
#define PARAM_NAME_DEF(x)			++paramnum; PARAM_NAME_DEF_AT(paramnum,x)
#define PARAM_SOUND_DEF(x)			++paramnum; PARAM_SOUND_DEF_AT(paramnum,x)
#define PARAM_COLOR_DEF(x)			++paramnum; PARAM_COLOR_DEF_AT(paramnum,x)
#define PARAM_FLOAT_DEF(x)			++paramnum; PARAM_FLOAT_DEF_AT(paramnum,x)
#define PARAM_ANGLE_DEF(x)			++paramnum; PARAM_ANGLE_DEF_AT(paramnum,x)
#define PARAM_STRING_DEF(x)			++paramnum; PARAM_STRING_DEF_AT(paramnum,x)
#define PARAM_STATE_DEF(x)			++paramnum; PARAM_STATE_DEF_AT(paramnum,x)
#define PARAM_STATE_ACTION_DEF(x)	++paramnum; PARAM_STATE_ACTION_DEF_AT(paramnum,x)
#define PARAM_POINTER_DEF(x,type)	++paramnum; PARAM_POINTER_DEF_AT(paramnum,x,type)
#define PARAM_OBJECT_DEF(x,type)	++paramnum; PARAM_OBJECT_DEF_AT(paramnum,x,type)
#define PARAM_CLASS_DEF(x,base)		++paramnum; PARAM_CLASS_DEF_AT(paramnum,x,base)
#define PARAM_CLASS_DEF_NOT_NULL(x,base)	++paramnum; PARAM_CLASS_DEF_NOT_NULL_AT(paramnum,x,base)

typedef int(*actionf_p)(VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret);/*(VM_ARGS)*/

struct FieldDesc
{
	const char *ClassName;
	const char *FieldName;
	size_t FieldOffset;
	unsigned FieldSize;
	int BitValue;
};

struct AFuncDesc
{
	const char *ClassName;
	const char *FuncName;
	actionf_p Function;
	VMNativeFunction **VMPointer;
};

#if defined(_MSC_VER)
#pragma section(".areg$u",read)
#pragma section(".freg$u",read)

#define MSVC_ASEG __declspec(allocate(".areg$u"))
#define MSVC_FSEG __declspec(allocate(".freg$u"))
#define GCC_ASEG
#define GCC_FSEG
#else
#define MSVC_ASEG
#define MSVC_FSEG
#define GCC_ASEG __attribute__((section(SECTION_AREG))) __attribute__((used))
#define GCC_FSEG __attribute__((section(SECTION_FREG))) __attribute__((used))
#endif

// Macros to handle action functions. These are here so that I don't have to
// change every single use in case the parameters change.

#define DEFINE_ACTION_FUNCTION(cls, name) \
	static int AF_##cls##_##name(VM_ARGS); \
	VMNativeFunction *cls##_##name##_VMPtr; \
	static const AFuncDesc cls##_##name##_Hook = { #cls, #name, AF_##cls##_##name, &cls##_##name##_VMPtr }; \
	extern AFuncDesc const *const cls##_##name##_HookPtr; \
	MSVC_ASEG AFuncDesc const *const cls##_##name##_HookPtr GCC_ASEG = &cls##_##name##_Hook; \
	static int AF_##cls##_##name(VM_ARGS)

// cls is the scripted class name, icls the internal one (e.g. player_t vs. Player)
#define DEFINE_FIELD_X(cls, icls, name) \
	static const FieldDesc VMField_##icls##_##name = { "A" #cls, #name, (unsigned)myoffsetof(icls, name), (unsigned)sizeof(icls::name), 0 }; \
	extern FieldDesc const *const VMField_##icls##_##name##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMField_##icls##_##name##_HookPtr GCC_FSEG = &VMField_##icls##_##name;

// This is for cases where the internal size does not match the part that gets exported.
#define DEFINE_FIELD_UNSIZED(cls, icls, name) \
	static const FieldDesc VMField_##icls##_##name = { "A" #cls, #name, (unsigned)myoffsetof(icls, name), ~0u, 0 }; \
	extern FieldDesc const *const VMField_##icls##_##name##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMField_##icls##_##name##_HookPtr GCC_FSEG = &VMField_##icls##_##name;

#define DEFINE_FIELD_NAMED_X(cls, icls, name, scriptname) \
	static const FieldDesc VMField_##cls##_##scriptname = { "A" #cls, #scriptname, (unsigned)myoffsetof(icls, name), (unsigned)sizeof(icls::name), 0 }; \
	extern FieldDesc const *const VMField_##cls##_##scriptname##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMField_##cls##_##scriptname##_HookPtr GCC_FSEG = &VMField_##cls##_##scriptname;

#define DEFINE_FIELD_X_BIT(cls, icls, name, bitval) \
	static const FieldDesc VMField_##icls##_##name = { "A" #cls, #name, (unsigned)myoffsetof(icls, name), (unsigned)sizeof(icls::name), bitval }; \
	extern FieldDesc const *const VMField_##icls##_##name##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMField_##icls##_##name##_HookPtr GCC_FSEG = &VMField_##cls##_##name;

#define DEFINE_FIELD(cls, name) \
	static const FieldDesc VMField_##cls##_##name = { #cls, #name, (unsigned)myoffsetof(cls, name), (unsigned)sizeof(cls::name), 0 }; \
	extern FieldDesc const *const VMField_##cls##_##name##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMField_##cls##_##name##_HookPtr GCC_FSEG = &VMField_##cls##_##name;

#define DEFINE_FIELD_NAMED(cls, name, scriptname) \
		static const FieldDesc VMField_##cls##_##scriptname = { #cls, #scriptname, (unsigned)myoffsetof(cls, name), (unsigned)sizeof(cls::name), 0 }; \
	extern FieldDesc const *const VMField_##cls##_##scriptname##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMField_##cls##_##scriptname##_HookPtr GCC_FSEG = &VMField_##cls##_##scriptname;

#define DEFINE_FIELD_BIT(cls, name, scriptname, bitval) \
		static const FieldDesc VMField_##cls##_##scriptname = { #cls, #scriptname, (unsigned)myoffsetof(cls, name), (unsigned)sizeof(cls::name), bitval }; \
	extern FieldDesc const *const VMField_##cls##_##scriptname##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMField_##cls##_##scriptname##_HookPtr GCC_FSEG = &VMField_##cls##_##scriptname;

#define DEFINE_GLOBAL(name) \
	static const FieldDesc VMGlobal_##name = { "", #name, (size_t)&name, (unsigned)sizeof(name), 0 }; \
	extern FieldDesc const *const VMGlobal_##name##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMGlobal_##name##_HookPtr GCC_FSEG = &VMGlobal_##name;

#define DEFINE_GLOBAL_NAMED(iname, name) \
	static const FieldDesc VMGlobal_##name = { "", #name, (size_t)&iname, (unsigned)sizeof(iname), 0 }; \
	extern FieldDesc const *const VMGlobal_##name##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMGlobal_##name##_HookPtr GCC_FSEG = &VMGlobal_##name;


class AActor;

#define ACTION_RETURN_STATE(v) do { FState *state = v; if (numret > 0) { assert(ret != NULL); ret->SetPointer(state); return 1; } return 0; } while(0)
#define ACTION_RETURN_POINTER(v) do { void *state = v; if (numret > 0) { assert(ret != NULL); ret->SetPointer(state); return 1; } return 0; } while(0)
#define ACTION_RETURN_OBJECT(v) do { auto state = v; if (numret > 0) { assert(ret != NULL); ret->SetObject(state); return 1; } return 0; } while(0)
#define ACTION_RETURN_FLOAT(v) do { double u = v; if (numret > 0) { assert(ret != nullptr); ret->SetFloat(u); return 1; } return 0; } while(0)
#define ACTION_RETURN_VEC2(v) do { DVector2 u = v; if (numret > 0) { assert(ret != nullptr); ret[0].SetVector2(u); return 1; } return 0; } while(0)
#define ACTION_RETURN_VEC3(v) do { DVector3 u = v; if (numret > 0) { assert(ret != nullptr); ret[0].SetVector(u); return 1; } return 0; } while(0)
#define ACTION_RETURN_INT(v) do { int u = v; if (numret > 0) { assert(ret != NULL); ret->SetInt(u); return 1; } return 0; } while(0)
#define ACTION_RETURN_BOOL(v) ACTION_RETURN_INT(v)
#define ACTION_RETURN_STRING(v) do { FString u = v; if (numret > 0) { assert(ret != NULL); ret->SetString(u); return 1; } return 0; } while(0)

// Checks to see what called the current action function
#define ACTION_CALL_FROM_ACTOR() (stateinfo == nullptr || stateinfo->mStateType == STATE_Actor)
#define ACTION_CALL_FROM_PSPRITE() (self->player && stateinfo != nullptr && stateinfo->mStateType == STATE_Psprite)
#define ACTION_CALL_FROM_INVENTORY() (stateinfo != nullptr && stateinfo->mStateType == STATE_StateChain)

// Standard parameters for all action functions
//   self         - Actor this action is to operate on (player if a weapon)
//   stateowner   - Actor this action really belongs to (may be an item)
//   callingstate - State this action was called from
#define PARAM_ACTION_PROLOGUE(type) \
	PARAM_PROLOGUE; \
	PARAM_OBJECT_NOT_NULL (self, AActor); \
	PARAM_OBJECT (stateowner, type) \
	PARAM_POINTER  (stateinfo, FStateParamInfo) \

// Number of action paramaters
#define NAP 3

#define PARAM_SELF_PROLOGUE(type) \
	PARAM_PROLOGUE; \
	PARAM_OBJECT_NOT_NULL(self, type);

// for structs we cannot do a class validation
#define PARAM_SELF_STRUCT_PROLOGUE(type) \
	PARAM_PROLOGUE; \
	PARAM_POINTER_NOT_NULL(self, type);

class PFunction;

VMFunction *FindVMFunction(PClass *cls, const char *name);
#define DECLARE_VMFUNC(cls, name) static VMFunction *name; if (name == nullptr) name = FindVMFunction(RUNTIME_CLASS(cls), #name);

FString FStringFormat(VM_ARGS);


unsigned GetVirtualIndex(PClass *cls, const char *funcname);

#define IFVIRTUALPTR(self, cls, funcname) \
	static unsigned VIndex = ~0u; \
	if (VIndex == ~0u) { \
		VIndex = GetVirtualIndex(RUNTIME_CLASS(cls), #funcname); \
		assert(VIndex != ~0u); \
	} \
	auto clss = self->GetClass(); \
	VMFunction *func = clss->Virtuals.Size() > VIndex? clss->Virtuals[VIndex] : nullptr;  \
	if (func != nullptr)

#define IFVIRTUAL(cls, funcname) IFVIRTUALPTR(this, cls, funcname)

#define IFVIRTUALPTRNAME(self, cls, funcname) \
	static unsigned VIndex = ~0u; \
	if (VIndex == ~0u) { \
		VIndex = GetVirtualIndex(PClass::FindClass(cls), #funcname); \
		assert(VIndex != ~0u); \
	} \
	auto clss = self->GetClass(); \
	VMFunction *func = clss->Virtuals.Size() > VIndex? clss->Virtuals[VIndex] : nullptr;  \
	if (func != nullptr)

#endif
