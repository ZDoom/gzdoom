#ifndef VM_H
#define VM_H

#include "zstring.h"
#include "autosegs.h"
#include "vectors.h"
#include "cmdlib.h"
#include "doomerrors.h"

#define MAX_RETURNS		8	// Maximum number of results a function called by script code can return
#define MAX_TRY_DEPTH	8	// Maximum number of nested TRYs in a single function


typedef unsigned char		VM_UBYTE;
typedef signed char			VM_SBYTE;
typedef unsigned short		VM_UHALF;
typedef signed short		VM_SHALF;
typedef unsigned int		VM_UWORD;
typedef signed int			VM_SWORD;
typedef VM_UBYTE			VM_ATAG;

#define VM_EPSILON			(1/65536.0)

#ifdef __BIG_ENDIAN__
#define VM_DEFINE_OP2(TYPE, ARG1, ARG2) TYPE ARG2, ARG1
#define VM_DEFINE_OP4(TYPE, ARG1, ARG2, ARG3, ARG4) TYPE ARG4, ARG3, ARG2, ARG1
#else // little endian
#define VM_DEFINE_OP2(TYPE, ARG1, ARG2) TYPE ARG1, ARG2
#define VM_DEFINE_OP4(TYPE, ARG1, ARG2, ARG3, ARG4) TYPE ARG1, ARG2, ARG3, ARG4
#endif // __BIG_ENDIAN__

union VMOP
{
	struct
	{
		VM_DEFINE_OP4(VM_UBYTE, op, a, b, c);
	};
	struct
	{
		VM_DEFINE_OP4(VM_SBYTE, pad0, as, bs, cs);
	};
	struct
	{
		VM_DEFINE_OP2(VM_SWORD, pad1:8, i24:24);
	};
	struct
	{
		VM_DEFINE_OP2(VM_SWORD, pad2:16, i16:16);
	};
	struct
	{
		VM_DEFINE_OP2(VM_UHALF, pad3, i16u);
	};
	VM_UWORD word;

	// Interesting fact: VC++ produces better code for i16 when it's defined
	// as a bitfield than when it's defined as two discrete units.
	// Compare:
	//		mov		eax,dword ptr [op]		; As two discrete units
	//		shr		eax,10h
	//		movsx	eax,ax
	// versus:
	//		mov		eax,dword ptr [op]		; As a bitfield
	//		sar		eax,10h
};

#undef VM_DEFINE_OP4
#undef VM_DEFINE_OP2

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
	FLOP_EXP,
	FLOP_LOG,
	FLOP_LOG10,
	FLOP_SQRT,
	FLOP_CEIL,
	FLOP_FLOOR,

	FLOP_ACOS,			// This group works with radians
	FLOP_ASIN,
	FLOP_ATAN,
	FLOP_COS,
	FLOP_SIN,
	FLOP_TAN,

	FLOP_ACOS_DEG,		// This group works with degrees
	FLOP_ASIN_DEG,
	FLOP_ATAN_DEG,
	FLOP_COS_DEG,
	FLOP_SIN_DEG,
	FLOP_TAN_DEG,

	FLOP_COSH,
	FLOP_SINH,
	FLOP_TANH,
};

// Cast operations
enum
{
	CAST_I2F,
	CAST_I2S,
	CAST_U2F,
	CAST_U2S,
	CAST_F2I,
	CAST_F2U,
	CAST_F2S,
	CAST_P2S,
	CAST_S2I,
	CAST_S2F,
	CAST_S2N,
	CAST_N2S,
	CAST_S2Co,
	CAST_S2So,
	CAST_Co2S,
	CAST_So2S,
	CAST_V22S,
	CAST_V32S,
	CAST_SID2S,
	CAST_TID2S,

	CASTB_I,
	CASTB_F,
	CASTB_A,
	CASTB_S
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
	REGT_MULTIREG2	= 8,
	REGT_MULTIREG3	= 16,	// (e.g. a vector)
	REGT_MULTIREG	= 24,
	REGT_ADDROF		= 32,	// used with PARAM: pass address of this register

	REGT_NIL		= 128	// parameter was omitted
};

#define RET_FINAL	(0x80)	// Used with RET and RETI in the destination slot: this is the final return value


// Tags for address registers
enum
{
	ATAG_GENERIC,			// pointer to something; we don't care what
	ATAG_OBJECT,			// pointer to an object; will be followed by GC

	// The following are all for documentation during debugging and are
	// functionally no different than ATAG_GENERIC (meaning they are useless because they trigger asserts all over the place.)

	/*
	ATAG_FRAMEPOINTER,		// pointer to extra stack frame space for this function
	ATAG_DREGISTER,			// pointer to a data register
	ATAG_FREGISTER,			// pointer to a float register
	ATAG_SREGISTER,			// pointer to a string register
	ATAG_AREGISTER,			// pointer to an address register
	*/

	ATAG_RNG,				// pointer to FRandom
	ATAG_STATE  = ATAG_GENERIC,			// pointer to FState (cannot have its own type because there's no means to track inside the VM.)
};

enum EVMAbortException
{
	X_READ_NIL,
	X_WRITE_NIL,
	X_TOO_MANY_TRIES,
	X_ARRAY_OUT_OF_BOUNDS,
	X_DIVISION_BY_ZERO,
	X_BAD_SELF,
};

class CVMAbortException : public CDoomError
{
public:
	static FString stacktrace;
	CVMAbortException(EVMAbortException reason, const char *moreinfo, va_list ap);
	void MaybePrintMessage();
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
	MODE_CMP,

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
	MODE_ACMP		= MODE_CMP << MODE_ASHIFT,

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
	VM_SHALF TagOfs;	// for pointers: Offset from Location to ATag; set to 0 if the caller is native code and doesn't care
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
	void SetPointer(void *val, int tag)
	{
		assert(RegType == REGT_POINTER);
		*(void **)Location = val;
		if (TagOfs != 0)
		{
			*((VM_ATAG *)Location + TagOfs) = tag;
		}
	}

	void IntAt(int *loc)
	{
		Location = loc;
		TagOfs = 0;
		RegType = REGT_INT;
	}
	void FloatAt(double *loc)
	{
		Location = loc;
		TagOfs = 0;
		RegType = REGT_FLOAT;
	}
	void StringAt(FString *loc)
	{
		Location = loc;
		TagOfs = 0;
		RegType = REGT_STRING;
	}
	void PointerAt(void **loc)
	{
		Location = loc;
		TagOfs = 0;
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
		struct { int pad[3]; VM_UBYTE Type; };
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
		this->atag = atag;
		Type = REGT_POINTER;
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

class VMFunction : public DObject
{
	DECLARE_ABSTRACT_CLASS(VMFunction, DObject);
	HAS_OBJECT_POINTERS;
public:
	bool Native;
	bool Final = false;				// cannot be overridden
	bool Unsafe = false;			// Contains references to class fields that are unsafe for psp and item state calls.
	BYTE ImplicitArgs = 0;	// either 0 for static, 1 for method or 3 for action
	unsigned VirtualIndex = ~0u;
	FName Name;
	TArray<VMValue> DefaultArgs;
	FString PrintableName;	// so that the VM can print meaningful info if something in this function goes wrong.

	class PPrototype *Proto;

	VMFunction(FName name = NAME_None) : Native(false), ImplicitArgs(0), Name(name), Proto(NULL) {}
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
		assert(((size_t)this & 15) == 0 && "VM frame is unaligned");
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
	DECLARE_CLASS(VMException, DObject);
};

union FVoidObj
{
	DObject *o;
	void *v;
};

struct FStatementInfo
{
	uint16_t InstructionIndex;
	uint16_t LineNumber;
};

class VMScriptFunction : public VMFunction
{
	DECLARE_CLASS(VMScriptFunction, VMFunction);
public:
	VMScriptFunction(FName name=NAME_None);
	~VMScriptFunction();
	size_t PropagateMark();
	void Alloc(int numops, int numkonstd, int numkonstf, int numkonsts, int numkonsta, int numlinenumbers);

	VM_ATAG *KonstATags() { return (VM_UBYTE *)(KonstA + NumKonstA); }
	const VM_ATAG *KonstATags() const { return (VM_UBYTE *)(KonstA + NumKonstA); }

	VMOP *Code;
	FStatementInfo *LineInfo;
	FString SourceFileName;
	int *KonstD;
	double *KonstF;
	FString *KonstS;
	FVoidObj *KonstA;
	int ExtraSpace;
	int CodeSize;			// Size of code in instructions (not bytes)
	unsigned LineInfoCount;
	VM_UBYTE NumRegD;
	VM_UBYTE NumRegF;
	VM_UBYTE NumRegS;
	VM_UBYTE NumRegA;
	VM_UHALF NumKonstD;
	VM_UHALF NumKonstF;
	VM_UHALF NumKonstS;
	VM_UHALF NumKonstA;
	VM_UHALF MaxParam;		// Maximum number of parameters this function has on the stack at once
	VM_UBYTE NumArgs;		// Number of arguments this function takes
	TArray<FTypeAndOffset> SpecialInits;	// list of all contents on the extra stack which require construction and destruction

	void InitExtra(void *addr);
	void DestroyExtra(void *addr);
	int AllocExtraStack(PType *type);
	int PCToLine(const VMOP *pc);
};

class VMFrameStack
{
public:
	VMFrameStack();
	~VMFrameStack();
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

		void InitFreeSpace()
		{
			FreeSpace = (VM_UBYTE *)(((size_t)(this + 1) + 15) & ~15);
		}
	};
	BlockHeader *Blocks;
	BlockHeader *UnusedBlocks;
	VMFrame *Alloc(int size);
};

class VMNativeFunction : public VMFunction
{
	DECLARE_CLASS(VMNativeFunction, VMFunction);
public:
	typedef int (*NativeCallType)(VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret);

	VMNativeFunction() : NativeCall(NULL) { Native = true; }
	VMNativeFunction(NativeCallType call) : NativeCall(call) { Native = true; }
	VMNativeFunction(NativeCallType call, FName name) : VMFunction(name), NativeCall(call) { Native = true; }

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

extern thread_local VMFrameStack GlobalVMStack;


void VMSelectEngine(EVMEngine engine);
extern int (*VMExec)(VMFrameStack *stack, const VMOP *pc, VMReturn *ret, int numret);
void VMFillParams(VMValue *params, VMFrame *callee, int numparam);

void VMDumpConstants(FILE *out, const VMScriptFunction *func);
void VMDisasm(FILE *out, const VMOP *code, int codesize, const VMScriptFunction *func);

// Use this in the prototype for a native function.
#define VM_ARGS			VMValue *param, TArray<VMValue> &defaultparam, int numparam, VMReturn *ret, int numret
#define VM_ARGS_NAMES	param, defaultparam, numparam, ret, numret

// Use these to collect the parameters in a native function.
// variable name <x> at position <p>
void NullParam(const char *varname);

#define PARAM_NULLCHECK(ptr, var) (ptr == nullptr? NullParam(#var), ptr : ptr)

// For required parameters.
#define PARAM_INT_AT(p,x)			assert((p) < numparam); assert(param[p].Type == REGT_INT); int x = param[p].i;
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
#define PARAM_OBJECT_AT(p,x,type)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER && (param[p].atag == ATAG_OBJECT || param[p].a == NULL)); type *x = (type *)param[p].a; assert(x == NULL || x->IsKindOf(RUNTIME_CLASS(type)));
#define PARAM_CLASS_AT(p,x,base)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER && (param[p].atag == ATAG_OBJECT || param[p].a == NULL)); base::MetaClass *x = (base::MetaClass *)param[p].a; assert(x == NULL || x->IsDescendantOf(RUNTIME_CLASS(base)));
#define PARAM_POINTER_NOT_NULL_AT(p,x,type)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER); type *x = (type *)PARAM_NULLCHECK(param[p].a, #x);
#define PARAM_OBJECT_NOT_NULL_AT(p,x,type)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER && (param[p].atag == ATAG_OBJECT || param[p].a == NULL)); type *x = (type *)PARAM_NULLCHECK(param[p].a, #x); assert(x == NULL || x->IsKindOf(RUNTIME_CLASS(type)));
#define PARAM_CLASS_NOT_NULL_AT(p,x,base)	assert((p) < numparam); assert(param[p].Type == REGT_POINTER && (param[p].atag == ATAG_OBJECT || param[p].a == NULL)); base::MetaClass *x = (base::MetaClass *)PARAM_NULLCHECK(param[p].a, #x); assert(x == NULL || x->IsDescendantOf(RUNTIME_CLASS(base)));

#define PARAM_EXISTS(p)					((p) < numparam)
#define ASSERTINT(p)					assert((p).Type == REGT_INT)
#define ASSERTFLOAT(p)					assert((p).Type == REGT_FLOAT)
#define ASSERTSTRING(p)					assert((p).Type == REGT_STRING)
#define ASSERTOBJECT(p)					assert((p).Type == REGT_POINTER && ((p).atag == ATAG_OBJECT || (p).a == nullptr))
#define ASSERTPOINTER(p)				assert((p).Type == REGT_POINTER && (p).atag == ATAG_GENERIC)
#define ASSERTSTATE(p)					assert((p).Type == REGT_POINTER && ((p).atag == ATAG_GENERIC || (p).atag == ATAG_STATE))

#define PARAM_INT_DEF_AT(p,x)			int x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = param[p].i; } else { ASSERTINT(defaultparam[p]); x = defaultparam[p].i; }
#define PARAM_BOOL_DEF_AT(p,x)			bool x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = !!param[p].i; } else { ASSERTINT(defaultparam[p]); x = !!defaultparam[p].i; }
#define PARAM_NAME_DEF_AT(p,x)			FName x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = ENamedName(param[p].i); } else { ASSERTINT(defaultparam[p]); x = ENamedName(defaultparam[p].i); }
#define PARAM_SOUND_DEF_AT(p,x)			FSoundID x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = FSoundID(param[p].i); } else { ASSERTINT(defaultparam[p]); x = FSoundID(defaultparam[p].i); }
#define PARAM_COLOR_DEF_AT(p,x)			PalEntry x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = param[p].i; } else { ASSERTINT(defaultparam[p]); x = defaultparam[p].i; }
#define PARAM_FLOAT_DEF_AT(p,x)			double x; if (PARAM_EXISTS(p)) { ASSERTFLOAT(param[p]); x = param[p].f; } else { ASSERTFLOAT(defaultparam[p]); x = defaultparam[p].f; }
#define PARAM_ANGLE_DEF_AT(p,x)			DAngle x; if (PARAM_EXISTS(p)) { ASSERTFLOAT(param[p]); x = param[p].f; } else { ASSERTFLOAT(defaultparam[p]); x = defaultparam[p].f; }
#define PARAM_STRING_DEF_AT(p,x)		FString x; if (PARAM_EXISTS(p)) { ASSERTSTRING(param[p]); x = param[p].s; } else { ASSERTSTRING(defaultparam[p]); x = defaultparam[p].s; }
#define PARAM_STATE_DEF_AT(p,x)			FState *x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = (FState*)StateLabels.GetState(param[p].i, self->GetClass()); } else { ASSERTINT(defaultparam[p]); x = (FState*)StateLabels.GetState(defaultparam[p].i, self->GetClass()); }
#define PARAM_STATE_ACTION_DEF_AT(p,x)	FState *x; if (PARAM_EXISTS(p)) { ASSERTINT(param[p]); x = (FState*)StateLabels.GetState(param[p].i, stateowner->GetClass()); } else { ASSERTINT(defaultparam[p]); x = (FState*)StateLabels.GetState(defaultparam[p].i, stateowner->GetClass()); }
#define PARAM_POINTER_DEF_AT(p,x,t)		t *x; if (PARAM_EXISTS(p)) { ASSERTPOINTER(param[p]); x = (t*)param[p].a; } else { ASSERTPOINTER(defaultparam[p]); x = (t*)defaultparam[p].a; }
#define PARAM_OBJECT_DEF_AT(p,x,t)		t *x; if (PARAM_EXISTS(p)) { ASSERTOBJECT(param[p]); x = (t*)param[p].a; } else { ASSERTOBJECT(defaultparam[p]); x = (t*)defaultparam[p].a; }
#define PARAM_CLASS_DEF_AT(p,x,t)		t::MetaClass *x; if (PARAM_EXISTS(p)) { ASSERTOBJECT(param[p]); x = (t::MetaClass*)param[p].a; } else { ASSERTOBJECT(defaultparam[p]); x = (t::MetaClass*)defaultparam[p].a; }
#define PARAM_CLASS_DEF_NOT_NULL_AT(p,x,t)		t::MetaClass *x; if (PARAM_EXISTS(p)) { ASSERTOBJECT(param[p]); x = (t::MetaClass*)PARAM_NULLCHECK(param[p].a, #x); } else { ASSERTOBJECT(defaultparam[p]); x = (t::MetaClass*)PARAM_NULLCHECK(defaultparam[p].a, #x); }

// The above, but with an automatically increasing position index.
#define PARAM_PROLOGUE				int paramnum = -1;

#define PARAM_INT(x)				++paramnum; PARAM_INT_AT(paramnum,x)
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
#define PARAM_OBJECT(x,type)		++paramnum; PARAM_OBJECT_AT(paramnum,x,type)
#define PARAM_CLASS(x,base)			++paramnum; PARAM_CLASS_AT(paramnum,x,base)
#define PARAM_POINTER_NOT_NULL(x,type)		++paramnum; PARAM_POINTER_NOT_NULL_AT(paramnum,x,type)
#define PARAM_OBJECT_NOT_NULL(x,type)		++paramnum; PARAM_OBJECT_NOT_NULL_AT(paramnum,x,type)
#define PARAM_CLASS_NOT_NULL(x,base)		++paramnum; PARAM_CLASS_NOT_NULL_AT(paramnum,x,base)

#define PARAM_INT_DEF(x)			++paramnum; PARAM_INT_DEF_AT(paramnum,x)
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
	unsigned FieldOffset;
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

#define DEFINE_FIELD_NAMED_X(cls, icls, name, scriptname) \
	static const FieldDesc VMField_##icls##_##scriptname = { "A" #cls, #scriptname, (unsigned)myoffsetof(icls, name), (unsigned)sizeof(icls::name), 0 }; \
	extern FieldDesc const *const VMField_##icls##_##scriptname##_HookPtr; \
	MSVC_FSEG FieldDesc const *const VMField_##icls##_##scriptname##_HookPtr GCC_FSEG = &VMField_##icls##_##scriptname;

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

class AActor;


#define ACTION_RETURN_STATE(v) do { FState *state = v; if (numret > 0) { assert(ret != NULL); ret->SetPointer(state, ATAG_STATE); return 1; } return 0; } while(0)
#define ACTION_RETURN_POINTER(v) do { void *state = v; if (numret > 0) { assert(ret != NULL); ret->SetPointer(state, ATAG_GENERIC); return 1; } return 0; } while(0)
#define ACTION_RETURN_OBJECT(v) do { auto state = v; if (numret > 0) { assert(ret != NULL); ret->SetPointer(state, ATAG_OBJECT); return 1; } return 0; } while(0)
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

// Standard parameters for all action functons
//   self         - Actor this action is to operate on (player if a weapon)
//   stateowner   - Actor this action really belongs to (may be an item)
//   callingstate - State this action was called from
#define PARAM_ACTION_PROLOGUE(type) \
	PARAM_PROLOGUE; \
	PARAM_OBJECT	 (self, AActor); \
	PARAM_OBJECT (stateowner, type) \
	PARAM_POINTER  (stateinfo, FStateParamInfo) \

// Number of action paramaters
#define NAP 3

#define PARAM_SELF_PROLOGUE(type) \
	PARAM_PROLOGUE; \
	PARAM_OBJECT(self, type);

// for structs we need to check for ATAG_GENERIC instead of ATAG_OBJECT
#define PARAM_SELF_STRUCT_PROLOGUE(type) \
	PARAM_PROLOGUE; \
	PARAM_POINTER(self, type);

class PFunction;

VMFunction *FindVMFunction(PClass *cls, const char *name);
#define DECLARE_VMFUNC(cls, name) static VMFunction *name; if (name == nullptr) name = FindVMFunction(RUNTIME_CLASS(cls), #name);



#endif
