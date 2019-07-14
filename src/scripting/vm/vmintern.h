#pragma once

#include "vm.h"
#include <csetjmp>

class VMScriptFunction;

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

	FLOP_ROUND,
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
	MODE_PARAM24,
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

	MODE_ABCJOINT	= (MODE_JOINT << MODE_ASHIFT) | (MODE_JOINT << MODE_BSHIFT) | (MODE_JOINT << MODE_CSHIFT),
	MODE_BCJOINT = (MODE_JOINT << MODE_BSHIFT) | (MODE_JOINT << MODE_CSHIFT),
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
};

struct VMOpInfo
{
	const char *Name;
	int Mode;
};

extern const VMOpInfo OpInfo[NUM_OPS];


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
		size += numrega * sizeof(void *);
		size += numregs * sizeof(FString);
		size += numregd * sizeof(int);
		if (numextra != 0)
		{
			size = (size + 15) & ~15;
			size += numextra;
		}
		return size;
	}

	VMValue *GetParam() const
	{
		assert(((size_t)this & 15) == 0 && "VM frame is unaligned");
		return (VMValue *)(((size_t)(this + 1) + 15) & ~15);
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

	int *GetRegD() const
	{
		return (int *)(GetRegA() + NumRegA);
	}

	void *GetExtra() const
	{
		uint8_t *pbeg = (uint8_t*)(GetRegD() + NumRegD);
		ptrdiff_t ofs = pbeg - (uint8_t *)this;
		return (VM_UBYTE *)this + ((ofs + 15) & ~15);
	}

	void GetAllRegs(int *&d, double *&f, FString *&s, void **&a, VMValue *&param) const
	{
		// Calling the individual functions produces suboptimal code. :(
		param = GetParam();
		f = (double *)(param + MaxParam);
		s = (FString *)(f + NumRegF);
		a = (void **)(s + NumRegS);
		d = (int *)(a + NumRegA);
	}

	void InitRegS();
};

struct VMRegisters
{
	VMRegisters(const VMFrame *frame)
	{
		frame->GetAllRegs(d, f, s, a, param);
	}

	VMRegisters(const VMRegisters &o)
		: d(o.d), f(o.f), s(o.s), a(o.a), param(o.param)
	{ }

	int *d;
	double *f;
	FString *s;
	void **a;
	VMValue *param;
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
	static int OffsetLastFrame() { return (int)(ptrdiff_t)offsetof(BlockHeader, LastFrame); }
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
		RegA++;
	}

	void ParamPointer(void *ptr)
	{
		Reg.a[RegA] = ptr;
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
extern int (*VMExec)(VMFunction *func, VMValue *params, int numparams, VMReturn *ret, int numret);
void VMFillParams(VMValue *params, VMFrame *callee, int numparam);

void VMDumpConstants(FILE *out, const VMScriptFunction *func);
void VMDisasm(FILE *out, const VMOP *code, int codesize, const VMScriptFunction *func);

extern thread_local VMFrameStack GlobalVMStack;

typedef std::pair<const class PType *, unsigned> FTypeAndOffset;

typedef int(*JitFuncPtr)(VMFunction *func, VMValue *params, int numparams, VMReturn *ret, int numret);

class VMScriptFunction : public VMFunction
{
public:
	VMScriptFunction(FName name = NAME_None);
	~VMScriptFunction();
	void Alloc(int numops, int numkonstd, int numkonstf, int numkonsts, int numkonsta, int numlinenumbers);

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
	unsigned StackSize;
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

private:
	static int FirstScriptCall(VMFunction *func, VMValue *params, int numparams, VMReturn *ret, int numret);
};
