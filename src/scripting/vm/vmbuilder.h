#ifndef VMUTIL_H
#define VMUTIL_H

#include "dobject.h"

class VMFunctionBuilder;

struct ExpEmit
{
	ExpEmit() : RegNum(0), RegType(REGT_NIL), RegCount(1), Konst(false), Fixed(false), Final(false), Target(false) {}
	ExpEmit(int reg, int type, bool konst = false, bool fixed = false) : RegNum(reg), RegType(type), RegCount(1), Konst(konst), Fixed(fixed), Final(false), Target(false) {}
	ExpEmit(VMFunctionBuilder *build, int type, int count = 1);
	void Free(VMFunctionBuilder *build);
	void Reuse(VMFunctionBuilder *build);

	BYTE RegNum, RegType, RegCount, Konst:1, Fixed : 1, Final : 1, Target : 1;
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

	private:
		VM_UWORD Used[256/32];		// Bitmap of used registers (bit set means reg is used)
		int MostUsed;

		friend class VMFunctionBuilder;
	};

	VMFunctionBuilder(int numimplicits);
	~VMFunctionBuilder();

	void MakeFunction(VMScriptFunction *func);

	// Returns the constant register holding the value.
	int GetConstantInt(int val);
	int GetConstantFloat(double val);
	int GetConstantAddress(void *ptr, VM_ATAG tag);
	int GetConstantString(FString str);

	// Returns the address of the next instruction to be emitted.
	size_t GetAddress();

	// Returns the address of the newly-emitted instruction.
	size_t Emit(int opcode, int opa, int opb, int opc);
	size_t Emit(int opcode, int opa, VM_SHALF opbc);
	size_t Emit(int opcode, int opabc);
	size_t EmitParamInt(int value);
	size_t EmitLoadInt(int regnum, int value);
	size_t EmitRetInt(int retnum, bool final, int value);

	void Backpatch(size_t addr, size_t target);
	void BackpatchToHere(size_t addr);

	// Write out complete constant tables.
	void FillIntConstants(int *konst);
	void FillFloatConstants(double *konst);
	void FillAddressConstants(FVoidObj *konst, VM_ATAG *tags);
	void FillStringConstants(FString *strings);

	// PARAM increases ActiveParam; CALL decreases it.
	void ParamChange(int delta);

	// Track available registers.
	RegAvailability Registers[4];

	// amount of implicit parameters so that proper code can be emitted for method calls
	int NumImplicits;

	// keep the frame pointer, if needed, in a register because the LFP opcode is hideously inefficient, requiring more than 20 instructions on x64.
	ExpEmit FramePointer;

private:
	struct AddrKonst
	{
		int KonstNum;
		VM_ATAG Tag;
	};
	// These map from the constant value to its position in the constant table.
	TMap<int, int> IntConstants;
	TMap<double, int> FloatConstants;
	TMap<void *, AddrKonst> AddressConstants;
	TMap<FString, int> StringConstants;

	int NumIntConstants;
	int NumFloatConstants;
	int NumAddressConstants;
	int NumStringConstants;

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
		FString PrintableName;
		int StateIndex;
		int StateCount;
		int Lump;
		bool FromDecorate;
	};

	TArray<Item> mItems;

public:
	VMFunction *AddFunction(PFunction *func, FxExpression *code, const FString &name, bool fromdecorate, int currentstate = -1, int statecnt = 0, int lumpnum = -1);
	void Build();
};

extern FFunctionBuildList FunctionBuildList;
#endif
