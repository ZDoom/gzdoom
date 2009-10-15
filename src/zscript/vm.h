#ifndef VM_H
#define VM_H

#include "zstring.h"
#include "dobject.h"

#define MAX_RETURNS		8	// Maximum number of results a function called by script code can return
#define MAX_TRY_DEPTH	8	// Maximum number of nested TRYs in a single function


typedef unsigned char		VM_UBYTE;
typedef signed char			VM_SBYTE;
typedef unsigned short		VM_UHALF;
typedef signed short		VM_SHALF;
typedef unsigned int		VM_UWORD;
typedef signed int			VM_SWORD;
typedef VM_UBYTE			VM_ATAG;

#define VM_EPSILON			(1/1024.0)
#define VM_OPSIZE			4		// Number of bytes used by one opcode

enum
{
#include "vmops.h"
NUM_OPS
};

// Flags for A field of CMPS
enum
{
	CMP_CHECK = 1,

	CMP_EQ = 0,
	CMP_LT = 2,
	CMP_LE = 4,
	CMP_METHOD_MASK = 6,

	CMP_BK = 8,
	CMP_CK = 16,
	CMP_APPROX = 32,
};

// Floating point operations for FLOP
enum
{
	FLOP_ABS,
	FLOP_NEG,
	FLOP_ACOS,
	FLOP_ASIN,
	FLOP_ATAN,
	FLOP_COS,
	FLOP_COSH,
	FLOP_EXP,
	FLOP_LOG,
	FLOP_LOG10,
	FLOP_SIN,
	FLOP_SINH,
	FLOP_TAN,
	FLOP_TANH,
	FLOP_SQRT,
	FLOP_CEIL,
	FLOP_FLOOR,
};

// Cast operations
enum
{
	CAST_I2F,
	CAST_I2S,
	CAST_F2I,
	CAST_F2S,
	CAST_P2S,
	CAST_S2I,
	CAST_S2F,
};

// Register types for VMParam
enum
{
	REGT_INT		= 0,
	REGT_FLOAT		= 1,
	REGT_STRING		= 2,
	REGT_POINTER	= 3,
	REGT_TYPE		= 3,

	REGT_KONST		= 4,
	REGT_MULTIREG	= 8,	// (e.g. a vector)
	REGT_FINAL		= 16,	// used with RET: this is the final return value
	REGT_ADDROF		= 32,	// used with PARAM: pass address of this register

	REGT_NIL		= 255	// parameter was omitted
};

// Tags for address registers
enum
{
	ATAG_GENERIC,			// pointer to something; we don't care what
	ATAG_OBJECT,			// pointer to an object; will be followed by GC

	// The following are all for documentation during debugging and are
	// functionally no different than ATAG_GENERIC.

	ATAG_FRAMEPOINTER,		// pointer to extra stack frame space for this function
	ATAG_DREGISTER,			// pointer to a data register
	ATAG_FREGISTER,			// pointer to a float register
	ATAG_SREGISTER,			// pointer to a string register
	ATAG_AREGISTER,			// pointer to an address register

	ATAG_STATE,				// pointer to FState
	ATAG_RNG,				// pointer to FRandom
};

class VMFunction : public DObject
{
	DECLARE_ABSTRACT_CLASS(VMFunction, DObject);
public:
	bool Native;
};

enum EVMOpMode
{
	MODE_ASHIFT		= 0,
	MODE_BSHIFT		= 4,
	MODE_CSHIFT		= 8,
	MODE_BCSHIFT	= 12,

	MODE_ATYPE		= 15 << MODE_ASHIFT,
	MODE_BTYPE		= 15 << MODE_BSHIFT,
	MODE_CTYPE		= 15 << MODE_CSHIFT,
	MODE_BCTYPE		= 31 << MODE_BCSHIFT,

	MODE_I			= 0,
	MODE_F,
	MODE_S,
	MODE_P,
	MODE_V,
	MODE_X,
	MODE_KI,
	MODE_KF,
	MODE_KS,
	MODE_KP,
	MODE_KV,
	MODE_UNUSED,
	MODE_IMMS,
	MODE_IMMZ,
	MODE_JOINT,

	MODE_PARAM,
	MODE_THROW,
	MODE_CATCH,
	MODE_CAST,

	MODE_AI			= MODE_I << MODE_ASHIFT,
	MODE_AF			= MODE_F << MODE_ASHIFT,
	MODE_AS			= MODE_S << MODE_ASHIFT,
	MODE_AP			= MODE_P << MODE_ASHIFT,
	MODE_AV			= MODE_V << MODE_ASHIFT,
	MODE_AX			= MODE_X << MODE_ASHIFT,
	MODE_AKP		= MODE_KP << MODE_ASHIFT,
	MODE_AUNUSED	= MODE_UNUSED << MODE_ASHIFT,
	MODE_AIMMS		= MODE_IMMS << MODE_ASHIFT,
	MODE_AIMMZ		= MODE_IMMZ << MODE_ASHIFT,

	MODE_BI			= MODE_I << MODE_BSHIFT,
	MODE_BF			= MODE_F << MODE_BSHIFT,
	MODE_BS			= MODE_S << MODE_BSHIFT,
	MODE_BP			= MODE_P << MODE_BSHIFT,
	MODE_BV			= MODE_V << MODE_BSHIFT,
	MODE_BX			= MODE_X << MODE_BSHIFT,
	MODE_BKI		= MODE_KI << MODE_BSHIFT,
	MODE_BKF		= MODE_KF << MODE_BSHIFT,
	MODE_BKS		= MODE_KS << MODE_BSHIFT,
	MODE_BKP		= MODE_KP << MODE_BSHIFT,
	MODE_BKV		= MODE_KV << MODE_BSHIFT,
	MODE_BUNUSED	= MODE_UNUSED << MODE_BSHIFT,
	MODE_BIMMS		= MODE_IMMS << MODE_BSHIFT,
	MODE_BIMMZ		= MODE_IMMZ << MODE_BSHIFT,

	MODE_CI			= MODE_I << MODE_CSHIFT,
	MODE_CF			= MODE_F << MODE_CSHIFT,
	MODE_CS			= MODE_S << MODE_CSHIFT,
	MODE_CP			= MODE_P << MODE_CSHIFT,
	MODE_CV			= MODE_V << MODE_CSHIFT,
	MODE_CX			= MODE_X << MODE_CSHIFT,
	MODE_CKI		= MODE_KI << MODE_CSHIFT,
	MODE_CKF		= MODE_KF << MODE_CSHIFT,
	MODE_CKS		= MODE_KS << MODE_CSHIFT,
	MODE_CKP		= MODE_KP << MODE_CSHIFT,
	MODE_CKV		= MODE_KV << MODE_CSHIFT,
	MODE_CUNUSED	= MODE_UNUSED << MODE_CSHIFT,
	MODE_CIMMS		= MODE_IMMS << MODE_CSHIFT,
	MODE_CIMMZ		= MODE_IMMZ << MODE_CSHIFT,

	MODE_BCJOINT	= (MODE_JOINT << MODE_BSHIFT) | (MODE_JOINT << MODE_CSHIFT),
	MODE_BCKI		= MODE_KI << MODE_BCSHIFT,
	MODE_BCKF		= MODE_KF << MODE_BCSHIFT,
	MODE_BCKS		= MODE_KS << MODE_BCSHIFT,
	MODE_BCKP		= MODE_KP << MODE_BCSHIFT,
	MODE_BCIMMS		= MODE_IMMS << MODE_BCSHIFT,
	MODE_BCIMMZ		= MODE_IMMZ << MODE_BCSHIFT,
	MODE_BCPARAM	= MODE_PARAM << MODE_BCSHIFT,
	MODE_BCTHROW	= MODE_THROW << MODE_BCSHIFT,
	MODE_BCCATCH	= MODE_CATCH << MODE_BCSHIFT,
	MODE_BCCAST		= MODE_CAST << MODE_BCSHIFT,

	MODE_ABCJOINT	= (MODE_JOINT << MODE_ASHIFT) | MODE_BCJOINT,
};

struct VMOpInfo
{
	const char *Name;
	int Mode;
};

extern const VMOpInfo OpInfo[NUM_OPS];

struct VMReturn
{
	void *Location;
	VM_SHALF RegNum;	// Used to find ObjFlag index for pointers; set negative if the caller is native code and doesn't care
	VM_UBYTE RegType;	// Same as VMParam RegType, except REGT_KONST is invalid; only used by asserts

	void SetInt(int val)
	{
		*(int *)Location = val;
	}
	void SetFloat(double val)
	{
		*(double *)Location = val;
	}
	void SetVector(const double val[3])
	{
		((double *)Location)[0] = val[0];
		((double *)Location)[1] = val[1];
		((double *)Location)[2] = val[2];
	}
	void SetString(const FString &val)
	{
		*(FString *)Location = val;
	}
	void SetPointer(void *val)
	{
		*(void **)Location = val;
	}

	void IntAt(int *loc)
	{
		Location = loc;
		RegNum = -1;
		RegType = REGT_INT;
	}
	void FloatAt(double *loc)
	{
		Location = loc;
		RegNum = -1;
		RegType = REGT_FLOAT;
	}
	void StringAt(FString *loc)
	{
		Location = loc;
		RegNum = -1;
		RegType = REGT_STRING;
	}
	void PointerAt(void **loc)
	{
		Location = loc;
		RegNum = -1;
		RegType = REGT_POINTER;
	}
};

struct VMRegisters;


struct VMValue
{
	union
	{
		int i;
		struct { void *a; int atag; };
		double f;
		struct { int pad[3]; int Type; };
		struct { int foo[4]; } biggest;
	};

	// Unfortunately, FString cannot be used directly.
	// Fortunately, it is relatively simple.
	FString &s() { return *(FString *)&a; }
	const FString &s() const { return *(FString *)&a; }

	VMValue()
	{
		a = NULL;
		Type = REGT_NIL;
	}
	~VMValue()
	{
		Kill();
	}
	VMValue(const VMValue &o)
	{
		biggest = o.biggest;
		if (Type == REGT_STRING)
		{
			::new(&s()) FString(o.s());
		}
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
	VMValue(const char *s)
	{
		::new(&a) FString(s);
		Type = REGT_STRING;
	}
	VMValue(const FString &s)
	{
		::new(&a) FString(s);
		Type = REGT_STRING;
	}
	VMValue(DObject *v)
	{
		a = v;
		atag = ATAG_OBJECT;
		Type = REGT_POINTER;
	}
	VMValue(void *v)
	{
		a = v;
		atag = ATAG_GENERIC;
		Type = REGT_POINTER;
	}
	VMValue(void *v, int tag)
	{
		a = v;
		atag = tag;
		Type = REGT_POINTER;
	}
	VMValue &operator=(const VMValue &o)
	{
		if (o.Type == REGT_STRING)
		{
			if (Type == REGT_STRING)
			{
				s() = o.s();
			}
			else
			{
				new(&s()) FString(o.s());
				Type = REGT_STRING;
			}
		}
		else
		{
			Kill();
			biggest = o.biggest;
		}
		return *this;
	}
	VMValue &operator=(int v)
	{
		Kill();
		i = v;
		Type = REGT_INT;
		return *this;
	}
	VMValue &operator=(double v)
	{
		Kill();
		f = v;
		Type = REGT_FLOAT;
		return *this;
	}
	VMValue &operator=(const FString &v)
	{
		if (Type == REGT_STRING)
		{
			s() = v;
		}
		else
		{
			::new(&s()) FString(v);
			Type = REGT_STRING;
		}
		return *this;
	}
	VMValue &operator=(const char *v)
	{
		if (Type == REGT_STRING)
		{
			s() = v;
		}
		else
		{
			::new(&s()) FString(v);
			Type = REGT_STRING;
		}
		return *this;
	}
	VMValue &operator=(DObject *v)
	{
		Kill();
		a = v;
		atag = ATAG_OBJECT;
		Type = REGT_POINTER;
		return *this;
	}
	void SetPointer(void *v, VM_ATAG atag=ATAG_GENERIC)
	{
		Kill();
		a = v;
		atag = atag;
		Type = atag;
	}
	void SetNil()
	{
		Kill();
		Type = REGT_NIL;
	}
	bool operator==(const VMValue &o)
	{
		return Test(o) == 0;
	}
	bool operator!=(const VMValue &o)
	{
		return Test(o) != 0;
	}
	bool operator< (const VMValue &o)
	{
		return Test(o) <  0;
	}
	bool operator<=(const VMValue &o)
	{
		return Test(o) <= 0;
	}
	bool operator> (const VMValue &o)
	{
		return Test(o) >  0;
	}
	bool operator>=(const VMValue &o)
	{
		return Test(o) >= 0;
	}
	int Test(const VMValue &o, int inexact=false)
	{
		double diff;

		if (Type == o.Type)
		{
			switch(Type)
			{
			case REGT_NIL:
				return 0;

			case REGT_INT:
				return i - o.i;

			case REGT_FLOAT:
				diff = f - o.f;
do_double:		if (inexact)
				{
					return diff < -VM_EPSILON ? -1 : diff > VM_EPSILON ? 1 : 0;
				}
				return diff < 0 ? -1 : diff > 0 ? 1 : 0;

			case REGT_STRING:
				return inexact ? s().CompareNoCase(o.s()) : s().Compare(o.s());

			case REGT_POINTER:
				return int((const VM_UBYTE *)a - (const VM_UBYTE *)o.a);
			}
			assert(0);		// Should not get here
			return 2;
		}
		if (Type == REGT_FLOAT && o.Type == REGT_INT)
		{
			diff = f - o.i;
			goto do_double;
		}
		if (Type == REGT_INT && o.Type == REGT_FLOAT)
		{
			diff = i - o.f;
			goto do_double;
		}
		// Bad comparison
		return 2;
	}
	FString ToString()
	{
		if (Type == REGT_STRING)
		{
			return s();
		}
		else if (Type == REGT_NIL)
		{
			return "nil";
		}
		FString t;
		if (Type == REGT_INT)
		{
			t.Format ("%d", i);
		}
		else if (Type == REGT_FLOAT)
		{
			t.Format ("%.14g", f);
		}
		else if (Type == REGT_POINTER)
		{
			// FIXME
			t.Format ("Object: %p", a);
		}
		return t;
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
	void Kill()
	{
		if (Type == REGT_STRING)
		{
			s().~FString();
		}
	}
};

// VM frame layout:
//	VMFrame header
//  parameter stack		- 16 byte boundary, 16 bytes each
//	double registers	- 8 bytes each
//	string registers	- 4 or 8 bytes each
//	address registers	- 4 or 8 bytes each
//	data registers		- 4 bytes each
//	address register tags-1 byte each
//	extra space			- 16 byte boundary
struct VMFrame
{
	VMFrame *ParentFrame;
	VMFunction *Func;
	VM_UBYTE NumRegD;
	VM_UBYTE NumRegF;
	VM_UBYTE NumRegS;
	VM_UBYTE NumRegA;
	VM_UHALF MaxParam;
	VM_UHALF NumParam;		// current number of parameters

	static int FrameSize(int numregd, int numregf, int numregs, int numrega, int numparam, int numextra)
	{
		int size = (sizeof(VMFrame) + 15) & ~15;
		size += numparam * sizeof(VMValue);
		size += numregf * sizeof(double);
		size += numrega * (sizeof(void *) + sizeof(VM_UBYTE));
		size += numregs * sizeof(FString);
		size += numregd * sizeof(int);
		if (numextra != 0)
		{
			size = (size + 15) & ~15;
			size += numextra;
		}
		return size;
	}

	int *GetRegD() const
	{
		return (int *)(GetRegA() + NumRegA);
	}

	double *GetRegF() const
	{
		return (double *)(GetParam() + MaxParam);
	}

	FString *GetRegS() const
	{
		return (FString *)(GetRegF() + NumRegF);
	}

	void **GetRegA() const
	{
		return (void **)(GetRegS() + NumRegS);
	}

	VM_ATAG *GetRegATag() const
	{
		return (VM_ATAG *)(GetRegD() + NumRegD);
	}

	VMValue *GetParam() const
	{
		return (VMValue *)(((size_t)(this + 1) + 15) & ~15);
	}

	void *GetExtra() const
	{
		VM_ATAG *ptag = GetRegATag();
		ptrdiff_t ofs = ptag - (VM_ATAG *)this;
		return (VM_UBYTE *)this + ((ofs + NumRegA + 15) & ~15);
	}

	void GetAllRegs(int *&d, double *&f, FString *&s, void **&a, VM_ATAG *&atag, VMValue *&param) const
	{
		// Calling the individual functions produces suboptimal code. :(
		param = GetParam();
		f = (double *)(param + MaxParam);
		s = (FString *)(f + NumRegF);
		a = (void **)(s + NumRegS);
		d = (int *)(a + NumRegA);
		atag = (VM_ATAG *)(d + NumRegD);
	}

	void InitRegS();
};

struct VMRegisters
{
	VMRegisters(const VMFrame *frame)
	{
		frame->GetAllRegs(d, f, s, a, atag, param);
	}

	VMRegisters(const VMRegisters &o)
		: d(o.d), f(o.f), s(o.s), a(o.a), atag(o.atag), param(o.param)
	{ }

	int *d;
	double *f;
	FString *s;
	void **a;
	VM_ATAG *atag;
	VMValue *param;
};

struct VMException : public DObject
{
	DECLARE_CLASS(VMFunction, DObject);
};

union FVoidObj
{
	DObject *o;
	void *v;
};

class VMScriptFunction : public VMFunction
{
	DECLARE_CLASS(VMScriptFunction, VMFunction);
public:
	VMScriptFunction();
	~VMScriptFunction();
	size_t PropagateMark();
	VM_UBYTE *AllocCode(int numops);
	int *AllocKonstD(int numkonst);
	double *AllocKonstF(int numkonst);
	FString *AllocKonstS(int numkonst);
	FVoidObj *AllocKonstA(int numkonst);

	VM_ATAG *KonstATags() { return (VM_UBYTE *)(KonstA + NumKonstA); }
	const VM_ATAG *KonstATags() const { return (VM_UBYTE *)(KonstA + NumKonstA); }

	VM_UBYTE *Code;
	int *KonstD;
	double *KonstF;
	FString *KonstS;
	FVoidObj *KonstA;
	int ExtraSpace;
	int NumCodeBytes;
	VM_UBYTE NumRegD;
	VM_UBYTE NumRegF;
	VM_UBYTE NumRegS;
	VM_UBYTE NumRegA;
	VM_UBYTE NumKonstD;
	VM_UBYTE NumKonstF;
	VM_UBYTE NumKonstS;
	VM_UBYTE NumKonstA;
	VM_UHALF MaxParam;		// Maximum number of parameters this function has on the stack at once
	VM_UBYTE NumArgs;		// Number of arguments this function takes
};

class VMFrameStack
{
public:
	VMFrameStack();
	~VMFrameStack();
	VMFrame *AllocFrame(int numregd, int numregf, int numregs, int numrega);
	VMFrame *AllocFrame(VMScriptFunction *func);
	VMFrame *PopFrame();
	VMFrame *TopFrame()
	{
		assert(Blocks != NULL && Blocks->LastFrame != NULL);
		return Blocks->LastFrame;
	}
	int Call(VMFunction *func, VMValue *params, int numparams, VMReturn *results, int numresults, VMException **trap=NULL);
private:
	enum { BLOCK_SIZE = 4096 };		// Default block size
	struct BlockHeader
	{
		BlockHeader *NextBlock;
		VMFrame *LastFrame;
		VM_UBYTE *FreeSpace;
		int BlockSize;
	};
	BlockHeader *Blocks;
	BlockHeader *UnusedBlocks;
	VMFrame *Alloc(int size);
};

class VMNativeFunction : public VMFunction
{
	DECLARE_CLASS(VMNativeFunction, VMFunction);
public:
	typedef int (*NativeCallType)(VMFrameStack *stack, VMValue *param, int numparam, VMReturn *ret, int numret);

	VMNativeFunction() : NativeCall(NULL) { Native = true; }
	VMNativeFunction(NativeCallType call) : NativeCall(call) { Native = true; }

	// Return value is the number of results.
	NativeCallType NativeCall;
};


class VMParamFiller
{
public:
	VMParamFiller(const VMFrame *frame) : Reg(frame), RegD(0), RegF(0), RegS(0), RegA(0) {}
	VMParamFiller(const VMRegisters *reg) : Reg(*reg), RegD(0), RegF(0), RegS(0), RegA(0) {}

	void ParamInt(int val)
	{
		Reg.d[RegD++] = val;
	}

	void ParamFloat(double val)
	{
		Reg.f[RegF++] = val;
	}

	void ParamString(FString &val)
	{
		Reg.s[RegS++] = val;
	}

	void ParamString(const char *val)
	{
		Reg.s[RegS++] = val;
	}

	void ParamObject(DObject *obj)
	{
		Reg.a[RegA] = obj;
		Reg.atag[RegA] = ATAG_OBJECT;
		RegA++;
	}

	void ParamPointer(void *ptr, VM_ATAG atag)
	{
		Reg.a[RegA] = ptr;
		Reg.atag[RegA] = atag;
		RegA++;
	}

private:
	const VMRegisters Reg;
	int RegD, RegF, RegS, RegA;
};


enum EVMEngine
{
	VMEngine_Default,
	VMEngine_Unchecked,
	VMEngine_Checked
};

void VMSelectEngine(EVMEngine engine);
extern int (*VMExec)(VMFrameStack *stack, const VM_UBYTE *pc, VMReturn *ret, int numret);
void VMFillParams(VMValue *params, VMFrame *callee, int numparam);

void VMDumpConstants(FILE *out, const VMScriptFunction *func);
void VMDisasm(FILE *out, const VM_UBYTE *code, int codesize, const VMScriptFunction *func);

#endif
