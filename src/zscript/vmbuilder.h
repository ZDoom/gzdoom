#ifndef VMUTIL_H
#define VMUTIL_H

#include "vm.h"

class VMFunctionBuilder
{
public:
	// Keeps track of which registers are available by way of a bitmask table.
	class RegAvailability
	{
	public:
		RegAvailability();
		int GetReg(int count);		// Returns the first register in the range
		void ReturnReg(int reg, int count);
		void Dump();

	private:
		VM_UWORD Used[256/32];		// Bitmap of used registers (bit set means reg is used)
		int MostUsed;

		friend class VMFunctionBuilder;
	};

	VMFunctionBuilder();
	~VMFunctionBuilder();

	VMScriptFunction *MakeFunction();

	// Returns the constant register holding the value.
	int GetConstantInt(int val);
	int GetConstantFloat(double val);
	int GetConstantAddress(void *ptr, VM_ATAG tag);
	int GetConstantString(FString str);

	// Returns the address of the newly-emitted instruction.
	size_t Emit(int opcode, int opa, int opb, int opc);

	// Write out complete constant tables.
	void FillIntConstants(int *konst);
	void FillFloatConstants(double *konst);
	void FillAddressConstants(FVoidObj *konst, VM_ATAG *tags);
	void FillStringConstants(FString *strings);

	// PARAM increases ActiveParam; CALL decreases it.
	void ParamChange(int delta);

	// Track available registers.
	RegAvailability IntRegisters;
	RegAvailability FloatRegisters;
	RegAvailability AddressRegisters;
	RegAvailability StringRegisters;

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

	TArray<VM_UBYTE> Code;
};

#endif
