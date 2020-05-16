
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

	// VMScriptFunction* GetScriptFunction() { return sfunc; }

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

	static void ThrowArrayOutOfBounds(int index, int size);
	static void ThrowException(int reason);

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

	template<typename T> IRType* GetIRType() { return ircontext->getInt8PtrTy(); }
	template<> IRType* GetIRType<void>() { return ircontext->getVoidTy(); }
	template<> IRType* GetIRType<char>() { return ircontext->getInt8Ty(); }
	template<> IRType* GetIRType<unsigned char>() { return ircontext->getInt8Ty(); }
	template<> IRType* GetIRType<short>() { return ircontext->getInt16Ty(); }
	template<> IRType* GetIRType<unsigned short>() { return ircontext->getInt16Ty(); }
	template<> IRType* GetIRType<int>() { return ircontext->getInt32Ty(); }
	template<> IRType* GetIRType<unsigned int>() { return ircontext->getInt32Ty(); }
	template<> IRType* GetIRType<long long>() { return ircontext->getInt64Ty(); }
	template<> IRType* GetIRType<unsigned long long>() { return ircontext->getInt64Ty(); }
	template<> IRType* GetIRType<float>() { return ircontext->getFloatTy(); }
	template<> IRType* GetIRType<double>() { return ircontext->getDoubleTy(); }

	template<typename RetType>
	IRFunctionType* GetFunctionType0() { return ircontext->getFunctionType(GetIRType<RetType>(), { }); }

	template<typename RetType, typename P1>
	IRFunctionType* GetFunctionType1() { return ircontext->getFunctionType(GetIRType<RetType>(), { GetIRType<P1>() }); }

	template<typename RetType, typename P1, typename P2>
	IRFunctionType* GetFunctionType2() { return ircontext->getFunctionType(GetIRType<RetType>(), { GetIRType<P1>(), GetIRType<P2>() }); }

	template<typename RetType, typename P1, typename P2, typename P3>
	IRFunctionType* GetFunctionType3() { return ircontext->getFunctionType(GetIRType<RetType>(), { GetIRType<P1>(), GetIRType<P2>(), GetIRType<P3>() }); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4>
	IRFunctionType* GetFunctionType4() { return ircontext->getFunctionType(GetIRType<RetType>(), { GetIRType<P1>(), GetIRType<P2>(), GetIRType<P3>(), GetIRType<P4>() }); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5>
	IRFunctionType* GetFunctionType5() { return ircontext->getFunctionType(GetIRType<RetType>(), { GetIRType<P1>(), GetIRType<P2>(), GetIRType<P3>(), GetIRType<P4>(), GetIRType<P5>() }); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	IRFunctionType* GetFunctionType6() { return ircontext->getFunctionType(GetIRType<RetType>(), { GetIRType<P1>(), GetIRType<P2>(), GetIRType<P3>(), GetIRType<P4>(), GetIRType<P5>(), GetIRType<P6>() }); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
	IRFunctionType* GetFunctionType7() { return ircontext->getFunctionType(GetIRType<RetType>(), { GetIRType<P1>(), GetIRType<P2>(), GetIRType<P3>(), GetIRType<P4>(), GetIRType<P5>(), GetIRType<P6>(), GetIRType<P7>() }); }

	IRFunction* GetNativeFunc(const char* name, void* ptr, IRFunctionType* functype)
	{
		IRFunction* func = ircontext->getFunction(name);
		if (!func)
		{
			func = ircontext->createFunction(functype, name);
			ircontext->addGlobalMapping(func, ptr);
		}
		return func;
	}

	template<typename RetType>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)()) { return GetNativeFunc(name, func, GetFunctionType0<RetType>()); }

	template<typename RetType, typename P1>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1)) { return GetNativeFunc(name, func, GetFunctionType1<RetType, P1>()); }

	template<typename RetType, typename P1, typename P2>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2)) { return GetNativeFunc(name, func, GetFunctionType2<RetType, P1, P2>()); }

	template<typename RetType, typename P1, typename P2, typename P3>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3)) { return GetNativeFunc(name, func, GetFunctionType3<RetType, P1, P2, P3>()); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4)) { return GetNativeFunc(name, func, GetFunctionType4<RetType, P1, P2, P3, P4>()); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5)) { return GetNativeFunc(name, func, GetFunctionType5<RetType, P1, P2, P3, P4, P5>()); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)) { return GetNativeFunc(name, func, GetFunctionType6<RetType, P1, P2, P3, P4, P5, P6>()); }

	template<typename RetType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6, typename P7>
	IRFunction* GetNativeFunc(const char* name, RetType(*func)(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6, P7 p7)) { return GetNativeFunc(name, func, GetFunctionType7<RetType, P1, P2, P3, P4, P5, P6, P7>()); }

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
	IRValue* ConstF(int index) { return ircontext->getConstantFloat(ircontext->getDoubleTy(), konstf[index]); }
	IRValue* ConstA(int index) { return ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)konsta[index].v); }
	IRValue* ConstS(int index) { return ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)&konsts[index]); }
	IRValue* ConstValueD(int value) { return ircontext->getConstantInt(value); }
	IRValue* ConstValueF(double value) { return ircontext->getConstantFloat(ircontext->getDoubleTy(), value); }
	IRValue* ConstValueA(void* value) { return ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)value); }
	IRValue* ConstValueS(void* value) { return ircontext->getConstantInt(ircontext->getInt8PtrTy(), (uint64_t)value); }
	void StoreD(IRValue* value, int index) { cc.CreateStore(value, regD[index]); }
	void StoreF(IRValue* value, int index) { cc.CreateStore(value, regF[index]); }
	void StoreA(IRValue* value, int index) { cc.CreateStore(value, regA[index]); }
	void StoreS(IRValue* value, int index) { cc.CreateStore(value, regS[index]); }

	IRValue* OffsetPtr(IRValue* ptr, IRValue* offset) { return cc.CreateGEP(ptr, { offset }); }
	IRValue* OffsetPtr(IRValue* ptr, int offset) { return cc.CreateGEP(ptr, { ircontext->getConstantInt(offset) }); }
	IRValue* ToInt8Ptr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt8PtrTy()); }
	IRValue* ToInt8Ptr(IRValue* ptr, int offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt8PtrTy()); }
	IRValue* ToInt16Ptr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt16PtrTy()); }
	IRValue* ToInt32Ptr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt32PtrTy()); }
	IRValue* ToInt32Ptr(IRValue* ptr) { return cc.CreateBitCast(ptr, ircontext->getInt32PtrTy()); }
	IRValue* ToInt32Ptr(IRValue* ptr, int offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt32PtrTy()); }
	IRValue* ToFloatPtr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getFloatPtrTy()); }
	IRValue* ToDoublePtr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getDoublePtrTy()); }
	IRValue* ToDoublePtr(IRValue* ptr, int offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getDoublePtrTy()); }
	IRValue* ToDoublePtr(IRValue* ptr) { return cc.CreateBitCast(ptr, ircontext->getDoublePtrTy()); }
	IRValue* ToInt8PtrPtr(IRValue* ptr, IRValue* offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt8PtrTy()->getPointerTo(ircontext)); }
	IRValue* ToInt8PtrPtr(IRValue* ptr, int offset) { return cc.CreateBitCast(OffsetPtr(ptr, offset), ircontext->getInt8PtrTy()->getPointerTo(ircontext)); }
	IRValue* ToInt8PtrPtr(IRValue* ptr) { return cc.CreateBitCast(ptr, ircontext->getInt8PtrTy()->getPointerTo(ircontext)); }
	IRValue* Trunc8(IRValue* value) { return cc.CreateTrunc(value, ircontext->getInt8Ty()); }
	IRValue* Trunc16(IRValue* value) { return cc.CreateTrunc(value, ircontext->getInt16Ty()); }
	IRValue* FPTrunc(IRValue* value) { return cc.CreateFPTrunc(value, ircontext->getFloatTy()); }
	IRValue* ZExt(IRValue* value) { return cc.CreateZExt(value, ircontext->getInt32Ty()); }
	IRValue* SExt(IRValue* value) { return cc.CreateSExt(value, ircontext->getInt32Ty()); }
	IRValue* FPExt(IRValue* value) { return cc.CreateFPExt(value, ircontext->getDoubleTy()); }
	void Store(IRValue* value, IRValue* ptr) { cc.CreateStore(value, ptr); }
	IRValue* Load(IRValue* ptr) { return cc.CreateLoad(ptr); }

	static void CallAssignString(FString* to, FString* from) { *to = *from; }

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
};
