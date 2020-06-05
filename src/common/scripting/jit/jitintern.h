
#include "jit.h"

#include "types.h"
#include "stats.h"

#include <functional>
#include <vector>

#include "ir/IR.h"

extern cycle_t VMCycles[10];
extern int VMCalls[10];

#define A				(pc[0].a)
#define B				(pc[0].b)
#define C				(pc[0].c)
#define Cs				(pc[0].cs)
#define BC				(pc[0].i16u)
#define BCs				(pc[0].i16)
#define ABCs			(pc[0].i24)
#define JMPOFS(x)		((x)->i24)

struct JitLabel
{
	IRBasicBlock* block = nullptr;
	size_t index = 0;
};

class JitCompiler
{
public:
	JitCompiler(IRContext* ircontext, VMScriptFunction* sfunc) : ircontext(ircontext), sfunc(sfunc) { }

	IRFunction* Codegen();

private:
	// Declare EmitXX functions for the opcodes:
	#define xx(op, name, mode, alt, kreg, ktype)	void Emit##op();
	#include "vmops.h"
	#undef xx

	IRBasicBlock* GetLabel(size_t pos);

	void EmitNativeCall(VMNativeFunction* target);
	void EmitVMCall(IRValue* ptr, VMFunction* target);
	void EmitVtbl(const VMOP* op);
	int StoreCallParams();
	void LoadInOuts();
	void LoadReturns(const VMOP* retval, int numret);
	void FillReturns(const VMOP* retval, int numret);
	void LoadCallResult(int type, int regnum, bool addrof);

	void EmitReadBarrier();

	void EmitNullPointerThrow(int index, EVMAbortException reason);
	void EmitThrowException(EVMAbortException reason);
	IRBasicBlock* EmitThrowExceptionLabel(EVMAbortException reason);

	void Setup();
	void CreateRegisters();
	void IncrementVMCalls();
	void SetupFrame();
	void SetupSimpleFrame();
	void SetupFullVMFrame();
	void EmitOpcode();
	void EmitPopFrame();

	void CheckVMFrame();
	IRValue* GetCallReturns();

	IRFunctionType* GetFuncSignature();

	void GetTypes();
	void CreateNativeFunctions();
	IRFunction* CreateNativeFunction(IRType* returnType, std::vector<IRType*> args, const char* name, void* ptr);

	template <typename Func>
	void EmitComparisonOpcode(Func jmpFunc)
	{
		int i = (int)(ptrdiff_t)(pc - sfunc->Code);
		IRBasicBlock* successbb = irfunc->createBasicBlock({});
		IRBasicBlock* failbb = GetLabel(i + 2 + JMPOFS(pc + 1));
		IRValue* result = jmpFunc(static_cast<bool>(A & CMP_CHECK));
		cc.CreateCondBr(result, failbb, successbb);
		cc.SetInsertPoint(successbb);
		pc++; // This instruction uses two instruction slots - skip the next one
	}

	IRValue* EmitVectorComparison(int N, bool check);

	IRValue* LoadD(int index) { return cc.CreateLoad(regD[index]); }
	IRValue* LoadF(int index) { return cc.CreateLoad(regF[index]); }
	IRValue* LoadA(int index) { return cc.CreateLoad(regA[index]); }
	IRValue* LoadS(int index) { return cc.CreateLoad(regS[index]); }
	IRValue* ConstD(int index) { return ircontext->getConstantInt(konstd[index]); }
	IRValue* ConstF(int index) { return ircontext->getConstantFloat(doubleTy, konstf[index]); }
	IRValue* ConstA(int index) { return ircontext->getConstantInt(int8PtrTy, (uint64_t)konsta[index].v); }
	IRValue* ConstS(int index) { return ircontext->getConstantInt(int8PtrTy, (uint64_t)&konsts[index]); }
	IRValue* ConstValueD(int value) { return ircontext->getConstantInt(value); }
	IRValue* ConstValueF(double value) { return ircontext->getConstantFloat(doubleTy, value); }
	IRValue* ConstValueA(void* value) { return ircontext->getConstantInt(int8PtrTy, (uint64_t)value); }
	IRValue* ConstValueS(void* value) { return ircontext->getConstantInt(int8PtrTy, (uint64_t)value); }
	void StoreD(IRValue* value, int index) { cc.CreateStore(value, regD[index]); }
	void StoreF(IRValue* value, int index) { cc.CreateStore(value, regF[index]); }
	void StoreA(IRValue* value, int index) { cc.CreateStore(value, regA[index]); }
	void StoreS(IRValue* value, int index) { cc.CreateStore(value, regS[index]); }

	IRValue* OffsetPtr(IRValue* ptr, IRValue* offset) { return cc.CreateGEP(ptr, { offset }); }
	IRValue* OffsetPtr(IRValue* ptr, int offset) { return cc.CreateGEP(ptr, { ircontext->getConstantInt(offset) }); }
	IRValue* ToInt8Ptr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), int8PtrTy); }
	IRValue* ToInt8Ptr(IRValue* ptr, int offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), int8PtrTy); }
	IRValue* ToInt16Ptr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), int16PtrTy); }
	IRValue* ToInt32Ptr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), int32PtrTy); }
	IRValue* ToInt32Ptr(IRValue* ptr) { return cc.CreateBitCast(ptr, int32PtrTy); }
	IRValue* ToInt32Ptr(IRValue* ptr, int offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), int32PtrTy); }
	IRValue* ToFloatPtr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), floatPtrTy); }
	IRValue* ToDoublePtr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), doublePtrTy); }
	IRValue* ToDoublePtr(IRValue* ptr, int offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), doublePtrTy); }
	IRValue* ToDoublePtr(IRValue* ptr) { return cc.CreateBitCast(ptr, doublePtrTy); }
	IRValue* ToInt8PtrPtr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), int8PtrPtrTy); }
	IRValue* ToInt8PtrPtr(IRValue* ptr, int offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), int8PtrPtrTy); }
	IRValue* ToInt8PtrPtr(IRValue* ptr) { return cc.CreateBitCast(ptr, int8PtrPtrTy); }
	IRValue* Trunc8(IRValue* value) { return cc.CreateTrunc(value, int8Ty); }
	IRValue* Trunc16(IRValue* value) { return cc.CreateTrunc(value, int16Ty); }
	IRValue* FPTrunc(IRValue* value) { return cc.CreateFPTrunc(value, floatTy); }
	IRValue* ZExt(IRValue* value) { return cc.CreateZExt(value, int32Ty); }
	IRValue* SExt(IRValue* value) { return cc.CreateSExt(value, int32Ty); }
	IRValue* FPExt(IRValue* value) { return cc.CreateFPExt(value, doubleTy); }
	void Store(IRValue* value, IRValue* ptr) { cc.CreateStore(value, ptr); }
	IRValue* Load(IRValue* ptr) { return cc.CreateLoad(ptr); }

	VMScriptFunction* sfunc = nullptr;

	IRContext* ircontext = nullptr;
	IRFunction* irfunc = nullptr;
	IRBuilder cc;

	IRValue* args = nullptr;
	IRValue* numargs = nullptr;
	IRValue* ret = nullptr;
	IRValue* numret = nullptr;

	const int* konstd = nullptr;
	const double* konstf = nullptr;
	const FString* konsts = nullptr;
	const FVoidObj* konsta = nullptr;

	TArray<IRValue*> regD;
	TArray<IRValue*> regF;
	TArray<IRValue*> regA;
	TArray<IRValue*> regS;

	const VMOP* pc = nullptr;
	VM_UBYTE op = 0;

	TArray<const VMOP*> ParamOpcodes;
	IRValue* callReturns = nullptr;

	IRValue* vmframestack = nullptr;
	IRValue* vmframe = nullptr;
	int offsetParams = 0;
	int offsetF = 0;
	int offsetS = 0;
	int offsetA = 0;
	int offsetD = 0;
	int offsetExtra = 0;

	TArray<JitLabel> labels;

	IRType* voidTy = nullptr;
	IRType* int8Ty = nullptr;
	IRType* int8PtrTy = nullptr;
	IRType* int8PtrPtrTy = nullptr;
	IRType* int16Ty = nullptr;
	IRType* int16PtrTy = nullptr;
	IRType* int32Ty = nullptr;
	IRType* int32PtrTy = nullptr;
	IRType* int64Ty = nullptr;
	IRType* floatTy = nullptr;
	IRType* floatPtrTy = nullptr;
	IRType* doubleTy = nullptr;
	IRType* doublePtrTy = nullptr;

	IRFunction* validateCall = nullptr;
	IRFunction* setReturnString = nullptr;
	IRFunction* createFullVMFrame = nullptr;
	IRFunction* popFullVMFrame = nullptr;
	IRFunction* throwException = nullptr;
	IRFunction* throwArrayOutOfBounds = nullptr;
	IRFunction* stringAssignmentOperator = nullptr;
	IRFunction* stringAssignmentOperatorCStr = nullptr;
	IRFunction* stringPlusOperator = nullptr;
	IRFunction* stringCompare = nullptr;
	IRFunction* stringCompareNoCase = nullptr;
	IRFunction* stringLength = nullptr;
	IRFunction* readBarrier = nullptr;
	IRFunction* writeBarrier = nullptr;
	IRFunction* doubleModF = nullptr;
	IRFunction* doublePow = nullptr;
	IRFunction* doubleAtan2 = nullptr;
	IRFunction* doubleFabs = nullptr;
	IRFunction* doubleExp = nullptr;
	IRFunction* doubleLog = nullptr;
	IRFunction* doubleLog10 = nullptr;
	IRFunction* doubleSqrt = nullptr;
	IRFunction* doubleCeil = nullptr;
	IRFunction* doubleFloor = nullptr;
	IRFunction* doubleAcos = nullptr;
	IRFunction* doubleAsin = nullptr;
	IRFunction* doubleAtan = nullptr;
	IRFunction* doubleCos = nullptr;
	IRFunction* doubleSin = nullptr;
	IRFunction* doubleTan = nullptr;
	IRFunction* doubleCosDeg = nullptr;
	IRFunction* doubleSinDeg = nullptr;
	IRFunction* doubleCosh = nullptr;
	IRFunction* doubleSinh = nullptr;
	IRFunction* doubleTanh = nullptr;
	IRFunction* doubleRound = nullptr;
	IRFunction* castI2S = nullptr;
	IRFunction* castU2S = nullptr;
	IRFunction* castF2S = nullptr;
	IRFunction* castV22S = nullptr;
	IRFunction* castV32S = nullptr;
	IRFunction* castP2S = nullptr;
	IRFunction* castS2I = nullptr;
	IRFunction* castS2F = nullptr;
	IRFunction* castS2N = nullptr;
	IRFunction* castN2S = nullptr;
	IRFunction* castS2Co = nullptr;
	IRFunction* castCo2S = nullptr;
	IRFunction* castS2So = nullptr;
	IRFunction* castSo2S = nullptr;
	IRFunction* castSID2S = nullptr;
	IRFunction* castTID2S = nullptr;
	IRFunction* castB_S = nullptr;
	IRFunction* dynCast = nullptr;
	IRFunction* dynCastC = nullptr;
};
