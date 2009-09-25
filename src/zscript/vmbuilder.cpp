#include "vmbuilder.h"

//==========================================================================
//
// VMFunctionBuilder - Constructor
//
//==========================================================================

VMFunctionBuilder::VMFunctionBuilder()
{
	NumIntConstants = 0;
	NumFloatConstants = 0;
	NumAddressConstants = 0;
	NumStringConstants = 0;
	MaxParam = 0;
	ActiveParam = 0;
}

//==========================================================================
//
// VMFunctionBuilder - Destructor
//
//==========================================================================

VMFunctionBuilder::~VMFunctionBuilder()
{
}

//==========================================================================
//
// VMFunctionBuilder :: MakeFunction
//
// Creates a new VMScriptFunction out of the data passed to this class.
//
//==========================================================================

VMScriptFunction *VMFunctionBuilder::MakeFunction()
{
	VMScriptFunction *func = new VMScriptFunction;

	// Copy code block.
	memcpy(func->AllocCode(Code.Size()), &Code[0], Code.Size());

	// Create constant tables.
	if (NumIntConstants > 0)
	{
		FillIntConstants(func->AllocKonstD(NumIntConstants));
	}
	if (NumFloatConstants > 0)
	{
		FillFloatConstants(func->AllocKonstF(NumFloatConstants));
	}
	if (NumAddressConstants > 0)
	{
		func->AllocKonstA(NumAddressConstants);
		FillAddressConstants(func->KonstA, func->KonstATags());
	}
	if (NumStringConstants > 0)
	{
		FillStringConstants(func->AllocKonstS(NumStringConstants));
	}

	// Assign required register space.
	func->NumRegD = IntRegisters.MostUsed;
	func->NumRegF = FloatRegisters.MostUsed;
	func->NumRegA = AddressRegisters.MostUsed;
	func->NumRegS = StringRegisters.MostUsed;
	func->MaxParam = MaxParam;

	// Technically, there's no reason why we can't end the function with
	// entries on the parameter stack, but it means the caller probably
	// did something wrong.
	assert(ActiveParam == 0);

	return func;
}

//==========================================================================
//
// VMFunctionBuilder :: FillIntConstants
//
//==========================================================================

void VMFunctionBuilder::FillIntConstants(int *konst)
{
	TMapIterator<int, int> it(IntConstants);
	TMap<int, int>::Pair *pair;

	while (it.NextPair(pair))
	{
		konst[pair->Value] = pair->Key;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: FillFloatConstants
//
//==========================================================================

void VMFunctionBuilder::FillFloatConstants(double *konst)
{
	TMapIterator<double, int> it(FloatConstants);
	TMap<double, int>::Pair *pair;

	while (it.NextPair(pair))
	{
		konst[pair->Value] = pair->Key;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: FillAddressConstants
//
//==========================================================================

void VMFunctionBuilder::FillAddressConstants(FVoidObj *konst, VM_ATAG *tags)
{
	TMapIterator<void *, AddrKonst> it(AddressConstants);
	TMap<void *, AddrKonst>::Pair *pair;

	while (it.NextPair(pair))
	{
		konst[pair->Value.KonstNum].v = pair->Key;
		tags[pair->Value.KonstNum] = pair->Value.Tag;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: FillStringConstants
//
//==========================================================================

void VMFunctionBuilder::FillStringConstants(FString *konst)
{
	TMapIterator<FString, int> it(StringConstants);
	TMap<FString, int>::Pair *pair;

	while (it.NextPair(pair))
	{
		konst[pair->Value] = pair->Key;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantInt
//
// Returns a constant register initialized with the given value, or -1 if
// there were no more constants free.
//
//==========================================================================

int VMFunctionBuilder::GetConstantInt(int val)
{
	int *locp = IntConstants.CheckKey(val);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		int loc = NumIntConstants++;
		IntConstants.Insert(val, loc);
		return loc;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantFloat
//
// Returns a constant register initialized with the given value, or -1 if
// there were no more constants free.
//
//==========================================================================

int VMFunctionBuilder::GetConstantFloat(double val)
{
	int *locp = FloatConstants.CheckKey(val);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		int loc = NumFloatConstants++;
		FloatConstants.Insert(val, loc);
		return loc;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantString
//
// Returns a constant register initialized with the given value, or -1 if
// there were no more constants free.
//
//==========================================================================

int VMFunctionBuilder::GetConstantString(FString val)
{
	int *locp = StringConstants.CheckKey(val);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		int loc = NumStringConstants++;
		StringConstants.Insert(val, loc);
		return loc;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantAddress
//
// Returns a constant register initialized with the given value, or -1 if
// there were no more constants free.
//
//==========================================================================

int VMFunctionBuilder::GetConstantAddress(void *ptr, VM_ATAG tag)
{
	AddrKonst *locp = AddressConstants.CheckKey(ptr);
	if (locp != NULL)
	{
		// There should only be one tag associated with a memory location.
		assert(locp->Tag == tag);
		return locp->KonstNum;
	}
	else
	{
		AddrKonst loc = { NumAddressConstants++, tag };
		AddressConstants.Insert(ptr, loc);
		return loc.KonstNum;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: ParamChange
//
// Adds delta to ActiveParam and keeps track of MaxParam.
//
//==========================================================================

void VMFunctionBuilder::ParamChange(int delta)
{
	assert(delta > 0 || -delta <= ActiveParam);
	ActiveParam += delta;
	if (ActiveParam > MaxParam)
	{
		MaxParam = ActiveParam;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: RegAvailability - Constructor
//
//==========================================================================

VMFunctionBuilder::RegAvailability::RegAvailability()
{
	memset(Used, 0, sizeof(Used));
	MostUsed = 0;
}

//==========================================================================
//
// VMFunctionBuilder :: RegAvailibity :: GetReg
//
// Gets one or more unused registers. If getting multiple registers, they
// will all be consecutive. Returns -1 if there were not enough consecutive
// registers to satisfy the request.
//
// Preference is given to low-numbered registers in an attempt to keep
// the maximum register count low so as to preserve VM stack space when this
// function is executed.
//
//==========================================================================

int VMFunctionBuilder::RegAvailability::GetReg(int count)
{
	VM_UWORD mask;
	int i, firstbit;

	// Getting fewer than one register makes no sense, and
	// the algorithm used here can only obtain ranges of up to 32 bits.
	if (count < 1 || count > 32)
	{
		return -1;
	}

	mask = count == 32 ? ~0u : (1 << count) - 1;

	for (i = 0; i < 256/32; ++i)
	{
		// Find the first word with free registers
		VM_UWORD bits = Used[i];
		if (bits != ~0u)
		{
			// Are there enough consecutive bits to satisfy the request?
			// Search by 16, then 8, then 1 bit at a time for the first
			// free register.
			if ((bits & 0xFFFF) == 0xFFFF)
			{
				firstbit = ((bits & 0xFF0000) == 0xFF0000) ? 24 : 16;
			}
			else
			{
				firstbit = ((bits & 0xFF) == 0xFF) ? 8 : 0;
			}
			for (; firstbit < 32; ++firstbit)
			{
				if (((bits >> firstbit) & mask) == 0)
				{
					if (firstbit + count <= 32)
					{ // Needed bits all fit in one word, so we got it.
						if (firstbit + count > MostUsed)
						{
							MostUsed = firstbit + count;
						}
						Used[i] |= mask << firstbit;
						return i * 32 + firstbit;
					}
					// Needed bits span two words, so check the next word.
					else if (i < 256/32 - 1)
					{ // There is a next word.
						if (((Used[i + 1]) & (mask >> (32 - firstbit))) == 0)
						{ // The next word has the needed open space, too.
							if (firstbit + count > MostUsed)
							{
								MostUsed = firstbit + count;
							}
							Used[i] |= mask << firstbit;
							Used[i + 1] |= mask >> (32 - firstbit);
							return i * 32 + firstbit;
						}
						else
						{ // Skip to the next word, because we know we won't find
						  // what we need if we stay inside this one. All bits
						  // from firstbit to the end of the word are 0. If the
						  // next word does not start with the x amount of 0's, we
						  // need to satisfy the request, then it certainly won't
						  // have the x+1 0's we would need if we started at
						  // firstbit+1 in this one.
							firstbit = 32;
						}
					}
					else
					{ // Out of words.
						break;
					}
				}
			}
		}
	}
	// No room!
	return -1;
}

//==========================================================================
//
// VMFunctionBuilder :: RegAvailibity :: ReturnReg
//
// Marks a range of registers as free again.
//
//==========================================================================

void VMFunctionBuilder::RegAvailability::ReturnReg(int reg, int count)
{
	assert(count >= 1 && count <= 32);
	assert(reg >= 0 && reg + count <= 256);

	VM_UWORD mask, partialmask;
	int firstword, firstbit;

	mask = count == 32 ? ~0u : (1 << count) - 1;
	firstword = reg / 32;
	firstbit = reg & 31;

	if (firstbit + count <= 32)
	{ // Range is all in one word.
		mask <<= firstbit;
		// If we are trying to return registers that are already free,
		// it probably means that the caller messed up somewhere.
		assert((Used[firstword] & mask) == mask);
		Used[firstword] &= ~mask;
	}
	else
	{ // Range is in two words.
		partialmask = mask << firstbit;
		assert((Used[firstword] & partialmask) == partialmask);
		Used[firstword] &= ~partialmask;

		partialmask = mask >> (32 - firstbit);
		assert((Used[firstword + 1] & partialmask) == partialmask);
		Used[firstword + 1] &= ~partialmask;
	}
}

void VMFunctionBuilder::RegAvailability::Dump()
{
	Printf("%032B %032B %032B %032B\n%032B %032B %032B %032B\n",
		Used[0], Used[1], Used[2], Used[3], Used[4], Used[5], Used[6], Used[7]);
}
