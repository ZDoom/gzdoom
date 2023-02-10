#ifndef VMUTIL_H
#define VMUTIL_H

#include "dobject.h"
#include "vmintern.h"
#include <vector>
#include <functional>

class VMFunctionBuilder;
class FxExpression;
class FxLocalVariableDeclaration;

struct ExpEmit
{
	ExpEmit() : RegNum(0), RegType(REGT_NIL), RegCount(1), Konst(false), Fixed(false), Final(false), Target(false) {}
	ExpEmit(int reg, int type, bool konst = false, bool fixed = false) : RegNum(reg), RegType(type), RegCount(1), Konst(konst), Fixed(fixed), Final(false), Target(false) {}
	ExpEmit(VMFunctionBuilder *build, int type, int count = 1);
	void Free(VMFunctionBuilder *build);
	void Reuse(VMFunctionBuilder *build);

	uint16_t RegNum;
	uint8_t RegType, RegCount;
	// We are at 8 bytes for this struct, no matter what, so it's rather pointless to squeeze these flags into bitfields.
	bool Konst, Fixed, Final, Target;
};

class VMFunctionBuilder
{
public:
	// Keeps track of which registers are available by way of a bitmask table.
	class RegAvailability
	{
	public:
		RegAvailability();
		int GetMostUsed() { return MostUsed; }
		int Get(int count);			// Returns the first register in the range
		void Return(int reg, int count);
		bool Reuse(int regnum);

		bool IsDirty(int reg) const
		{
			const int firstword = reg / 32;
			const int firstbit = reg & 31;
			return Dirty[firstword] & (1 << firstbit);
		}

	private:
		VM_UWORD Used[256/32];		// Bitmap of used registers (bit set means reg is used)
		VM_UWORD Dirty[256/32];
		int MostUsed;

		friend class VMFunctionBuilder;
	};

	VMFunctionBuilder(int numimplicits);
	~VMFunctionBuilder();

	void BeginStatement(FxExpression *stmt);
	void EndStatement();
	void MakeFunction(VMScriptFunction *func);

	// Returns the constant register holding the value.
	unsigned GetConstantInt(int val);
	unsigned GetConstantFloat(double val);
	unsigned GetConstantAddress(void *ptr);
	unsigned GetConstantString(FString str);

	unsigned AllocConstantsInt(unsigned int count, int *values);
	unsigned AllocConstantsFloat(unsigned int count, double *values);
	unsigned AllocConstantsAddress(unsigned int count, void **ptrs);
	unsigned AllocConstantsString(unsigned int count, FString *ptrs);


	// Returns the address of the next instruction to be emitted.
	size_t GetAddress();

	// Returns the address of the newly-emitted instruction.
	size_t Emit(int opcode, int opa, int opb, int opc);
	size_t Emit(int opcode, int opa, VM_SHALF opbc);
	size_t Emit(int opcode, int opabc);
	size_t EmitLoadInt(int regnum, int value);
	size_t EmitRetInt(int retnum, bool final, int value);

	void Backpatch(size_t addr, size_t target);
	void BackpatchToHere(size_t addr);
	void BackpatchList(TArray<size_t> &addrs, size_t target);
	void BackpatchListToHere(TArray<size_t> &addrs);

	// Write out complete constant tables.
	void FillIntConstants(int *konst);
	void FillFloatConstants(double *konst);
	void FillAddressConstants(FVoidObj *konst);
	void FillStringConstants(FString *strings);

	// PARAM increases ActiveParam; CALL decreases it.
	void ParamChange(int delta);

	// Track available registers.
	RegAvailability Registers[4];

	// amount of implicit parameters so that proper code can be emitted for method calls
	int NumImplicits;

	// keep the frame pointer, if needed, in a register because the LFP opcode is hideously inefficient, requiring more than 20 instructions on x64.
	ExpEmit FramePointer;
	TArray<FxLocalVariableDeclaration *> ConstructedStructs;

private:
	TArray<FStatementInfo> LineNumbers;
	TArray<FxExpression *> StatementStack;

	TArray<int> IntConstantList;
	TArray<double> FloatConstantList;
	TArray<void *> AddressConstantList;
	TArray<FString> StringConstantList;
	// These map from the constant value to its position in the constant table.
	TMap<int, unsigned> IntConstantMap;
	TMap<double, unsigned> FloatConstantMap;
	TMap<void *, unsigned> AddressConstantMap;
	TMap<FString, unsigned> StringConstantMap;

	int MaxParam;
	int ActiveParam;

	TArray<VMOP> Code;

};

void DumpFunction(FILE *dump, VMScriptFunction *sfunc, const char *label, int labellen);


//==========================================================================
//
//
//
//==========================================================================
class FxExpression;

class FFunctionBuildList
{
	struct Item
	{
		PFunction *Func = nullptr;
		FxExpression *Code = nullptr;
		PPrototype *Proto = nullptr;
		VMScriptFunction *Function = nullptr;
		PNamespace *CurGlobals = nullptr;
		FString PrintableName;
		int StateIndex;
		int StateCount;
		int Lump;
		VersionInfo Version;
		bool FromDecorate;
	};

	TArray<Item> mItems;

	void DumpJit(bool include_gzdoom_pk3);

public:
	VMFunction *AddFunction(PNamespace *curglobals, const VersionInfo &ver, PFunction *func, FxExpression *code, const FString &name, bool fromdecorate, int currentstate, int statecnt, int lumpnum);
	void Build();
};

extern FFunctionBuildList FunctionBuildList;


//==========================================================================
//
// Function call parameter collector
//
//==========================================================================
extern int EncodeRegType(ExpEmit reg);

class FunctionCallEmitter
{
	// std::function and TArray are not compatible so this has to use std::vector instead.
	std::vector<std::function<int(VMFunctionBuilder *)>> emitters;
	TArray<std::pair<int, int>> returns;
	TArray<uint8_t> reginfo;
	unsigned numparams = 0;	// This counts the number of pushed elements, which can differ from the number of emitters with vectors.
	VMFunction *target = nullptr;
	class PFunctionPointer *fnptr = nullptr;
	int virtualselfreg = -1;
	bool is_vararg;
public:
	FunctionCallEmitter(VMFunction *func);
	FunctionCallEmitter(class PFunctionPointer *func);

	void SetVirtualReg(int virtreg)
	{
		virtualselfreg = virtreg;
	}

	void AddParameter(VMFunctionBuilder *build, FxExpression *operand);
	void AddParameter(ExpEmit &emit, bool reference);
	void AddParameterPointerConst(void *konst);
	void AddParameterPointer(int index, bool konst);
	void AddParameterFloatConst(double konst);
	void AddParameterIntConst(int konst);
	void AddParameterStringConst(const FString &konst);
	ExpEmit EmitCall(VMFunctionBuilder *build, TArray<ExpEmit> *ReturnRegs = nullptr);
	void AddReturn(int regtype, int regcount = 1)
	{
		returns.Push({ regtype, regcount });
	}
	unsigned Count() const
	{
		return numparams;
	}
};

class VMDisassemblyDumper
{
public:
	enum FileOperationType
	{
		Overwrite,
		Append
	};

	explicit VMDisassemblyDumper(const FileOperationType operation);
	~VMDisassemblyDumper();

	void Write(VMScriptFunction *sfunc, const FString &fname);
	void Flush();

private:
	FILE *dump = nullptr;
	FString namefilter;
	int codesize = 0;
	int datasize = 0;
};

#endif
