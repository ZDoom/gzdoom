/*
** vmbuilder.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
** Copyright 2016-2017 Christoph Oelckers
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

#include "vmbuilder.h"
#include "codegen.h"
#include "m_argv.h"
#include "c_cvars.h"
#include "jit.h"

CVAR(Bool, strictdecorate, false, CVAR_GLOBALCONFIG | CVAR_ARCHIVE)

struct VMRemap
{
	uint8_t altOp, kReg, kType;
};


#define xx(op, name, mode, alt, kreg, ktype) {OP_##alt, kreg, ktype },
VMRemap opRemap[NUM_OPS] = {
#include "vmops.h"
};
#undef xx

//==========================================================================
//
// VMFunctionBuilder - Constructor
//
//==========================================================================

VMFunctionBuilder::VMFunctionBuilder(int numimplicits)
{
	MaxParam = 0;
	ActiveParam = 0;
	NumImplicits = numimplicits;
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
// VMFunctionBuilder :: BeginStatement
//
// Records the start of a new statement.
//
//==========================================================================

void VMFunctionBuilder::BeginStatement(FxExpression *stmt)
{
	// pop empty statement records.
	while (LineNumbers.Size() > 0 && LineNumbers.Last().InstructionIndex == Code.Size()) LineNumbers.Pop();
	// only add a new entry if the line number differs.
	if (LineNumbers.Size() == 0 || stmt->ScriptPosition.ScriptLine != LineNumbers.Last().LineNumber)
	{
		FStatementInfo si = { (uint16_t)Code.Size(), (uint16_t)stmt->ScriptPosition.ScriptLine };
		LineNumbers.Push(si);
	}
	StatementStack.Push(stmt);
}

void VMFunctionBuilder::EndStatement()
{
	// pop empty statement records.
	while (LineNumbers.Size() > 0 && LineNumbers.Last().InstructionIndex == Code.Size()) LineNumbers.Pop();
	StatementStack.Pop();
	// Re-enter the previous statement.
	if (StatementStack.Size() > 0)
	{
		FStatementInfo si = { (uint16_t)Code.Size(), (uint16_t)StatementStack.Last()->ScriptPosition.ScriptLine };
		LineNumbers.Push(si);
	}
}

void VMFunctionBuilder::MakeFunction(VMScriptFunction *func)
{
	func->Alloc(Code.Size(), IntConstantList.Size(), FloatConstantList.Size(), StringConstantList.Size(), AddressConstantList.Size(), LineNumbers.Size());

	// Copy code block.
	memcpy(func->Code, &Code[0], Code.Size() * sizeof(VMOP));
	memcpy(func->LineInfo, &LineNumbers[0], LineNumbers.Size() * sizeof(LineNumbers[0]));

	// Create constant tables.
	if (IntConstantList.Size() > 0)
	{
		FillIntConstants(func->KonstD);
	}
	if (FloatConstantList.Size() > 0)
	{
		FillFloatConstants(func->KonstF);
	}
	if (AddressConstantList.Size() > 0)
	{
		FillAddressConstants(func->KonstA);
	}
	if (StringConstantList.Size() > 0)
	{
		FillStringConstants(func->KonstS);
	}

	// Assign required register space.
	func->NumRegD = Registers[REGT_INT].MostUsed;
	func->NumRegF = Registers[REGT_FLOAT].MostUsed;
	func->NumRegA = Registers[REGT_POINTER].MostUsed;
	func->NumRegS = Registers[REGT_STRING].MostUsed;
	func->MaxParam = MaxParam;
	func->StackSize = VMFrame::FrameSize(func->NumRegD, func->NumRegF, func->NumRegS, func->NumRegA, func->MaxParam, func->ExtraSpace);

	// Technically, there's no reason why we can't end the function with
	// entries on the parameter stack, but it means the caller probably
	// did something wrong.
	assert(ActiveParam == 0);
}

//==========================================================================
//
// VMFunctionBuilder :: FillIntConstants
//
//==========================================================================

void VMFunctionBuilder::FillIntConstants(int *konst)
{
	memcpy(konst, &IntConstantList[0], sizeof(int) * IntConstantList.Size());
}

//==========================================================================
//
// VMFunctionBuilder :: FillFloatConstants
//
//==========================================================================

void VMFunctionBuilder::FillFloatConstants(double *konst)
{
	memcpy(konst, &FloatConstantList[0], sizeof(double) * FloatConstantList.Size());
}

//==========================================================================
//
// VMFunctionBuilder :: FillAddressConstants
//
//==========================================================================

void VMFunctionBuilder::FillAddressConstants(FVoidObj *konst)
{
	memcpy(konst, &AddressConstantList[0], sizeof(void*) * AddressConstantList.Size());
}

//==========================================================================
//
// VMFunctionBuilder :: FillStringConstants
//
//==========================================================================

void VMFunctionBuilder::FillStringConstants(FString *konst)
{
	for (auto &s : StringConstantList)
	{
		*konst++ = s;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantInt
//
// Returns a constant register initialized with the given value.
//
//==========================================================================

unsigned VMFunctionBuilder::GetConstantInt(int val)
{
	unsigned int *locp = IntConstantMap.CheckKey(val);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		unsigned loc = IntConstantList.Push(val);
		IntConstantMap.Insert(val, loc);
		return loc;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantFloat
//
// Returns a constant register initialized with the given value.
//
//==========================================================================

unsigned VMFunctionBuilder::GetConstantFloat(double val)
{
	unsigned *locp = FloatConstantMap.CheckKey(val);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		unsigned loc = FloatConstantList.Push(val);
		FloatConstantMap.Insert(val, loc);
		return loc;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantString
//
// Returns a constant register initialized with the given value.
//
//==========================================================================

unsigned VMFunctionBuilder::GetConstantString(FString val)
{
	unsigned *locp = StringConstantMap.CheckKey(val);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		unsigned loc = StringConstantList.Push(val);
		StringConstantMap.Insert(val, loc);
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

unsigned VMFunctionBuilder::GetConstantAddress(void *ptr)
{
	unsigned *locp = AddressConstantMap.CheckKey(ptr);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		unsigned loc = AddressConstantList.Push(ptr);
		AddressConstantMap.Insert(ptr, loc);
		return loc;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: AllocConstants*
//
// Returns a range of constant register initialized with the given values.
//
//==========================================================================

unsigned VMFunctionBuilder::AllocConstantsInt(unsigned count, int *values)
{
	unsigned addr = IntConstantList.Reserve(count);
	memcpy(&IntConstantList[addr], values, count * sizeof(int));
	for (unsigned i = 0; i < count; i++)
	{
		IntConstantMap.Insert(values[i], addr + i);
	}
	return addr;
}

unsigned VMFunctionBuilder::AllocConstantsFloat(unsigned count, double *values)
{
	unsigned addr = FloatConstantList.Reserve(count);
	memcpy(&FloatConstantList[addr], values, count * sizeof(double));
	for (unsigned i = 0; i < count; i++)
	{
		FloatConstantMap.Insert(values[i], addr + i);
	}
	return addr;
}

unsigned VMFunctionBuilder::AllocConstantsAddress(unsigned count, void **ptrs)
{
	unsigned addr = AddressConstantList.Reserve(count);
	memcpy(&AddressConstantList[addr], ptrs, count * sizeof(void *));
	for (unsigned i = 0; i < count; i++)
	{
		AddressConstantMap.Insert(ptrs[i], addr+i);
	}
	return addr;
}

unsigned VMFunctionBuilder::AllocConstantsString(unsigned count, FString *ptrs)
{
	unsigned addr = StringConstantList.Reserve(count);
	for (unsigned i = 0; i < count; i++)
	{
		StringConstantList[addr + i] = ptrs[i];
		StringConstantMap.Insert(ptrs[i], addr + i);
	}
	return addr;
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
	memset(Dirty, 0, sizeof(Dirty));
	MostUsed = 0;
}

//==========================================================================
//
// VMFunctionBuilder :: RegAvailability :: Get
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

int VMFunctionBuilder::RegAvailability::Get(int count)
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

	for (i = 0; i < 256 / 32; ++i)
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
						if (i * 32 + firstbit + count > MostUsed)
						{
							MostUsed = i * 32 + firstbit + count;
						}
						Used[i] |= mask << firstbit;
						return i * 32 + firstbit;
					}
					// Needed bits span two words, so check the next word.
					else if (i < 256/32 - 1)
					{ // There is a next word.
						if (((Used[i + 1]) & (mask >> (32 - firstbit))) == 0)
						{ // The next word has the needed open space, too.
							if (i * 32 + firstbit + count > MostUsed)
							{
								MostUsed = i * 32 + firstbit + count;
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
// VMFunctionBuilder :: RegAvailibity :: Return
//
// Marks a range of registers as free again.
//
//==========================================================================

void VMFunctionBuilder::RegAvailability::Return(int reg, int count)
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
		// Unfortunately this can happen if an 'action' function gets called from a non-action context,
		// because for that case it pushes the self pointer a second time without reallocating, so it gets freed twice.
		//assert((Used[firstword] & mask) == mask);
		Used[firstword] &= ~mask;
		Dirty[firstword] |= mask;
	}
	else
	{ // Range is in two words.
		partialmask = mask << firstbit;
		assert((Used[firstword] & partialmask) == partialmask);
		Used[firstword] &= ~partialmask;
		Dirty[firstword] |= partialmask;

		partialmask = mask >> (32 - firstbit);
		assert((Used[firstword + 1] & partialmask) == partialmask);
		Used[firstword + 1] &= ~partialmask;
		Dirty[firstword + 1] |= partialmask;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: RegAvailability :: Reuse
//
// Marks an unused register as in-use. Returns false if the register is
// already in use or true if it was successfully reused.
//
//==========================================================================

bool VMFunctionBuilder::RegAvailability::Reuse(int reg)
{
	assert(reg >= 0 && reg <= 255);
	assert(reg < MostUsed && "Attempt to reuse a register that was never used");

	VM_UWORD mask = 1 << (reg & 31);
	int word = reg / 32;

	if (Used[word] & mask)
	{ // It's already in use!
		return false;
	}
	Used[word] |= mask;
	return true;
}

//==========================================================================
//
// VMFunctionBuilder :: GetAddress
//
//==========================================================================

size_t VMFunctionBuilder::GetAddress()
{
	return Code.Size();
}

//==========================================================================
//
// VMFunctionBuilder :: Emit
//
// Just dumbly output an instruction. Returns instruction position, not
// byte position. (Because all instructions are exactly four bytes long.)
//
//==========================================================================

size_t VMFunctionBuilder::Emit(int opcode, int opa, int opb, int opc)
{
	static uint8_t opcodes[] = { OP_LK, OP_LKF, OP_LKS, OP_LKP };

	assert(opcode >= 0 && opcode < NUM_OPS);
	assert(opa >= 0);
	assert(opb >= 0);
	assert(opc >= 0);

	// The following were just asserts, meaning this would silently create broken code if there was an overflow
	// if this happened in a release build. Not good.
	// These are critical errors that need to be reported to the user.
	// In addition, the limit of 256 constants can easily be exceeded with arrays so this had to be extended to
	// 65535 by adding some checks here that map byte-limited instructions to alternatives that can handle larger indices.
	// (See vmops.h for the remapping info.)

	// Note: OP_CMPS also needs treatment, but I do not expect constant overflow to become an issue with strings, so for now there is no handling.

	if (opa > 255)
	{
		if (opRemap[opcode].kReg != 1 || opa > 32767)
		{
			I_Error("Register limit exceeded");
		}
		int regtype = opRemap[opcode].kType;
		ExpEmit emit(this, regtype);
		Emit(opcodes[regtype], emit.RegNum, opa);
		opcode = opRemap[opcode].altOp;
		opa = emit.RegNum;
		emit.Free(this);
	}
	if (opb > 255)
	{
		if (opRemap[opcode].kReg != 2 || opb > 32767)
		{
			I_Error("Register limit exceeded");
		}
		int regtype = opRemap[opcode].kType;
		ExpEmit emit(this, regtype);
		Emit(opcodes[regtype], emit.RegNum, opb);
		opcode = opRemap[opcode].altOp;
		opb = emit.RegNum;
		emit.Free(this);
	}
	if (opc > 255)
	{
		if (opRemap[opcode].kReg != 4 || opc > 32767)
		{
			I_Error("Register limit exceeded");
		}
		int regtype = opRemap[opcode].kType;
		ExpEmit emit(this, regtype);
		Emit(opcodes[regtype], emit.RegNum, opc);
		opcode = opRemap[opcode].altOp;
		opc = emit.RegNum;
		emit.Free(this);
	}

	if (opcode == OP_CALL || opcode == OP_CALL_K)
	{
		ParamChange(-opb);
	}
	VMOP op;
	op.op = opcode;
	op.a = opa;
	op.b = opb;
	op.c = opc;
	return Code.Push(op);
}

size_t VMFunctionBuilder::Emit(int opcode, int opa, VM_SHALF opbc)
{
	assert(opcode >= 0 && opcode < NUM_OPS);
	assert(opa >= 0 && opa <= 255);

	if (opcode == OP_PARAM)
	{
		int chg;
		if (opa & REGT_MULTIREG2) chg = 2;
		else if (opa & REGT_MULTIREG3) chg = 3;
		else if (opa & REGT_MULTIREG4) chg = 4;
		else chg = 1;
		ParamChange(chg);
	}

	//assert(opbc >= -32768 && opbc <= 32767);	always true due to parameter's width
	VMOP op;
	op.op = opcode;
	op.a = opa;
	op.i16 = opbc;
	return Code.Push(op);
}

size_t VMFunctionBuilder::Emit(int opcode, int opabc)
{
	assert(opcode >= 0 && opcode < NUM_OPS);
	assert(opabc >= -(1 << 23) && opabc <= (1 << 24) - 1);
	if (opcode == OP_PARAMI)
	{
		ParamChange(1);
	}
	VMOP op;
	op.op = opcode;
	op.i24 = opabc;
	return Code.Push(op);
}

//==========================================================================
//
// VMFunctionBuilder :: EmitLoadInt
//
// Loads an integer constant into a register, using either an immediate
// value or a constant register, as appropriate.
//
//==========================================================================

size_t VMFunctionBuilder::EmitLoadInt(int regnum, int value)
{
	assert(regnum >= 0 && regnum < Registers[REGT_INT].MostUsed);
	if (value >= -32768 && value <= 32767)
	{
		return Emit(OP_LI, regnum, value);
	}
	else
	{
		return Emit(OP_LK, regnum, GetConstantInt(value));
	}
}

//==========================================================================
//
// VMFunctionBuilder :: EmitRetInt
//
// Returns an integer, using either an immediate value or a constant
// register, as appropriate.
//
//==========================================================================

size_t VMFunctionBuilder::EmitRetInt(int retnum, bool final, int value)
{
	assert(retnum >= 0 && retnum <= 127);
	if (value >= -32768 && value <= 32767)
	{
		return Emit(OP_RETI, retnum | (final << 7), value);
	}
	else
	{
		return Emit(OP_RET, retnum | (final << 7), REGT_INT | REGT_KONST, GetConstantInt(value));
	}
}

//==========================================================================
//
// VMFunctionBuilder :: Backpatch
//
// Store a JMP instruction at <loc> that points at <target>.
//
//==========================================================================

void VMFunctionBuilder::Backpatch(size_t loc, size_t target)
{
	assert(loc < Code.Size());
	int offset = int(target - loc - 1);
	assert(((offset << 8) >> 8) == offset);
	Code[loc].op = OP_JMP;
	Code[loc].i24 = offset;
}

void VMFunctionBuilder::BackpatchList(TArray<size_t> &locs, size_t target)
{
	for (auto loc : locs)
		Backpatch(loc, target);
}


//==========================================================================
//
// VMFunctionBuilder :: BackpatchToHere
//
// Store a JMP instruction at <loc> that points to the current code gen
// location.
//
//==========================================================================

void VMFunctionBuilder::BackpatchToHere(size_t loc)
{
	Backpatch(loc, Code.Size());
}

void VMFunctionBuilder::BackpatchListToHere(TArray<size_t> &locs)
{
	for (auto loc : locs)
		Backpatch(loc, Code.Size());
}

//==========================================================================
//
// FFunctionBuildList
//
// This list contains all functions yet to build.
// All adding functions return a VMFunction - either a complete one
// for native functions or an empty VMScriptFunction for scripted ones
// This VMScriptFunction object later gets filled in with the actual
// info, but we get the pointer right after registering the function
// with the builder.
//
//==========================================================================
FFunctionBuildList FunctionBuildList;

VMFunction *FFunctionBuildList::AddFunction(PNamespace *gnspc, const VersionInfo &ver, PFunction *functype, FxExpression *code, const FString &name, bool fromdecorate, int stateindex, int statecount, int lumpnum)
{
	if (code != nullptr)
	{
		auto func = code->GetDirectFunction(functype, ver);
		if (func != nullptr)
		{
			delete code;


			return func;
		}
	}

	//Printf("Adding %s\n", name.GetChars());

	Item it;
	assert(gnspc != nullptr);
	it.CurGlobals = gnspc;
	it.Func = functype;
	it.Code = code;
	it.PrintableName = name;
	it.Function = new VMScriptFunction;
	it.Function->Name = functype->SymbolName;
	it.Function->QualifiedName = it.Function->PrintableName = ClassDataAllocator.Strdup(name);
	it.Function->ImplicitArgs = functype->GetImplicitArgs();
	it.Proto = nullptr;
	it.FromDecorate = fromdecorate;
	it.StateIndex = stateindex;
	it.StateCount = statecount;
	it.Lump = lumpnum;
	it.Version = ver;
	assert(it.Func->Variants.Size() == 1);
	it.Func->Variants[0].Implementation = it.Function;

	// set prototype for named functions.
	if (it.Func->SymbolName != NAME_None)
	{
		it.Function->Proto = it.Func->Variants[0].Proto;
		it.Function->ArgFlags = it.Func->Variants[0].ArgFlags;
	}

	mItems.Push(it);
	return it.Function;
}


void FFunctionBuildList::Build()
{
	VMDisassemblyDumper disasmdump(VMDisassemblyDumper::Overwrite);

	for (auto &item : mItems)
	{
		// [Player701] Do not emit code for abstract functions
		bool isAbstract = item.Func->Variants[0].Implementation->VarFlags & VARF_Abstract;
		if (isAbstract) continue;

		assert(item.Code != NULL);

		// We don't know the return type in advance for anonymous functions.
		FCompileContext ctx(item.CurGlobals, item.Func, item.Func->SymbolName == NAME_None ? nullptr : item.Func->Variants[0].Proto, item.FromDecorate, item.StateIndex, item.StateCount, item.Lump, item.Version);

		// Allocate registers for the function's arguments and create local variable nodes before starting to resolve it.
		VMFunctionBuilder buildit(item.Func->GetImplicitArgs());
		for (unsigned i = 0; i < item.Func->Variants[0].Proto->ArgumentTypes.Size(); i++)
		{
			auto type = item.Func->Variants[0].Proto->ArgumentTypes[i];
			auto name = item.Func->Variants[0].ArgNames[i];
			auto flags = item.Func->Variants[0].ArgFlags[i];
			// this won't get resolved and won't get emitted. It is only needed so that the code generator can retrieve the necessary info about this argument to do its work.
			auto local = new FxLocalVariableDeclaration(type, name, nullptr, flags, FScriptPosition());
			if (!(flags & VARF_Out)) local->RegNum = buildit.Registers[type->GetRegType()].Get(type->GetRegCount());
			else local->RegNum = buildit.Registers[REGT_POINTER].Get(1);
			ctx.FunctionArgs.Push(local);
		}

		FScriptPosition::StrictErrors = !item.FromDecorate || strictdecorate;
		item.Code = item.Code->Resolve(ctx);
		// If we need extra space, load the frame pointer into a register so that we do not have to call the wasteful LFP instruction more than once.
		if (item.Function->ExtraSpace > 0)
		{
			buildit.FramePointer = ExpEmit(&buildit, REGT_POINTER);
			buildit.FramePointer.Fixed = true;
			buildit.Emit(OP_LFP, buildit.FramePointer.RegNum);
		}

		// Make sure resolving it didn't obliterate it.
		if (item.Code != nullptr)
		{
			if (!item.Code->CheckReturn())
			{
				auto newcmpd = new FxCompoundStatement(item.Code->ScriptPosition);
				newcmpd->Add(item.Code);
				newcmpd->Add(new FxReturnStatement(nullptr, item.Code->ScriptPosition));
				item.Code = newcmpd->Resolve(ctx);
			}

			item.Proto = ctx.ReturnProto;
			if (item.Proto == nullptr)
			{
				item.Code->ScriptPosition.Message(MSG_ERROR, "Function %s without prototype", item.PrintableName.GetChars());
				continue;
			}

			// Generate prototype for anonymous functions.
			VMScriptFunction *sfunc = item.Function;
			// create a new prototype from the now known return type and the argument list of the function's template prototype.
			if (sfunc->Proto == nullptr)
			{
				sfunc->Proto = NewPrototype(item.Proto->ReturnTypes, item.Func->Variants[0].Proto->ArgumentTypes);
				sfunc->ArgFlags = item.Func->Variants[0].ArgFlags;
			}

			// Emit code
			try
			{
				sfunc->SourceFileName = item.Code->ScriptPosition.FileName.GetChars();	// remember the file name for printing error messages if something goes wrong in the VM.
				buildit.BeginStatement(item.Code);
				item.Code->Emit(&buildit);
				buildit.EndStatement();
				buildit.MakeFunction(sfunc);
				sfunc->NumArgs = 0;
				// NumArgs for the VMFunction must be the amount of stack elements, which can differ from the amount of logical function arguments if vectors are in the list.
				// For the VM a vector is 2 or 3 args, depending on size.
				auto funcVariant = item.Func->Variants[0];
				for (unsigned int i = 0; i < funcVariant.Proto->ArgumentTypes.Size(); i++)
				{
					auto argType = funcVariant.Proto->ArgumentTypes[i];
					auto argFlags = funcVariant.ArgFlags[i];
					if (argFlags & VARF_Out)
					{
						auto argPointer = NewPointer(argType);
						sfunc->NumArgs += argPointer->GetRegCount();
					}
					else
					{
						sfunc->NumArgs += argType->GetRegCount();
					}
				}

				disasmdump.Write(sfunc, item.PrintableName);

				sfunc->Unsafe = ctx.Unsafe;
			}
			catch (CRecoverableError &err)
			{
				// catch errors from the code generator and pring something meaningful.
				item.Code->ScriptPosition.Message(MSG_ERROR, "%s in %s", err.GetMessage(), item.PrintableName.GetChars());
			}
		}
		delete item.Code;
		disasmdump.Flush();
	}
	VMFunction::CreateRegUseInfo();
	FScriptPosition::StrictErrors = strictdecorate;

	if (FScriptPosition::ErrorCounter == 0 && Args->CheckParm("-dumpjit")) DumpJit();
	mItems.Clear();
	mItems.ShrinkToFit();
	FxAlloc.FreeAllBlocks();
}

void FFunctionBuildList::DumpJit()
{
#ifdef HAVE_VM_JIT
	FILE *dump = fopen("dumpjit.txt", "w");
	if (dump == nullptr)
		return;

	for (auto &item : mItems)
	{
		JitDumpLog(dump, item.Function);
	}

	fclose(dump);
#endif // HAVE_VM_JIT
}


void FunctionCallEmitter::AddParameter(VMFunctionBuilder *build, FxExpression *operand)
{
	ExpEmit where = operand->Emit(build);

	if (where.RegType == REGT_NIL)
	{
		operand->ScriptPosition.Message(MSG_ERROR, "Attempted to pass a non-value");
	}
	numparams += where.RegCount;
	if (target->VarFlags & VARF_VarArg)
		for (unsigned i = 0; i < where.RegCount; i++) reginfo.Push(where.RegType & REGT_TYPE);

	emitters.push_back([=](VMFunctionBuilder *build) -> int
	{
		auto op = where;
		if (op.RegType == REGT_NIL)
		{
			build->Emit(OP_PARAM, op.RegType, op.RegNum);
			return 1;
		}
		else
		{
			build->Emit(OP_PARAM, EncodeRegType(op), op.RegNum);
			op.Free(build);
			return op.RegCount;
		}
	});
}

void FunctionCallEmitter::AddParameter(ExpEmit &emit, bool reference)
{
	numparams += emit.RegCount;
	if (target->VarFlags & VARF_VarArg)
	{
		if (reference) reginfo.Push(REGT_POINTER);
		else for (unsigned i = 0; i < emit.RegCount; i++) reginfo.Push(emit.RegType & REGT_TYPE);
	}
	emitters.push_back([=](VMFunctionBuilder *build) ->int
	{
		build->Emit(OP_PARAM, emit.RegType + (reference * REGT_ADDROF) + (emit.Konst * REGT_KONST), emit.RegNum);
		auto op = emit;
		op.Free(build);
		return emit.RegCount;
	});
}

void FunctionCallEmitter::AddParameterPointerConst(void *konst)
{
	numparams++;
	if (target->VarFlags & VARF_VarArg)
		reginfo.Push(REGT_POINTER);
	emitters.push_back([=](VMFunctionBuilder *build) ->int
	{
		build->Emit(OP_PARAM, REGT_POINTER | REGT_KONST, build->GetConstantAddress(konst));
		return 1;
	});
}

void FunctionCallEmitter::AddParameterPointer(int index, bool konst)
{
	numparams++;
	if (target->VarFlags & VARF_VarArg)
		reginfo.Push(REGT_POINTER);
	emitters.push_back([=](VMFunctionBuilder *build) ->int
	{
		build->Emit(OP_PARAM, konst ? REGT_POINTER | REGT_KONST : REGT_POINTER, index);
		return 1;
	});
}

void FunctionCallEmitter::AddParameterFloatConst(double konst)
{
	numparams++;
	if (target->VarFlags & VARF_VarArg)
		reginfo.Push(REGT_FLOAT);
	emitters.push_back([=](VMFunctionBuilder *build) ->int
	{
		build->Emit(OP_PARAM, REGT_FLOAT | REGT_KONST, build->GetConstantFloat(konst));
		return 1;
	});
}

void FunctionCallEmitter::AddParameterIntConst(int konst)
{
	numparams++;
	if (target->VarFlags & VARF_VarArg)
		reginfo.Push(REGT_INT);
	emitters.push_back([=](VMFunctionBuilder *build) ->int
	{
		// Immediates for PARAMI must fit in 24 bits.
		if (((konst << 8) >> 8) == konst)
		{
			build->Emit(OP_PARAMI, konst);
		}
		else
		{
			build->Emit(OP_PARAM, REGT_INT | REGT_KONST, build->GetConstantInt(konst));
		}
		return 1;
	});
}

void FunctionCallEmitter::AddParameterStringConst(const FString &konst)
{
	numparams++;
	if (target->VarFlags & VARF_VarArg)
		reginfo.Push(REGT_STRING);
	emitters.push_back([=](VMFunctionBuilder *build) ->int
	{
		build->Emit(OP_PARAM, REGT_STRING | REGT_KONST, build->GetConstantString(konst));
		return 1;
	});
}

EXTERN_CVAR(Bool, vm_jit)

ExpEmit FunctionCallEmitter::EmitCall(VMFunctionBuilder *build, TArray<ExpEmit> *ReturnRegs)
{
	unsigned paramcount = 0;
	for (auto &func : emitters)
	{
		paramcount += func(build);
	}
	assert(paramcount == numparams);
	if (target->VarFlags & VARF_VarArg)
	{
		// Pass a hidden type information parameter to vararg functions.
		// It would really be nicer to actually pass real types but that'd require a far more complex interface on the compiler side than what we have.
		uint8_t *regbuffer = (uint8_t*)ClassDataAllocator.Alloc(reginfo.Size());	// Allocate in the arena so that the pointer does not need to be maintained.
		memcpy(regbuffer, reginfo.Data(), reginfo.Size());
		build->Emit(OP_PARAM, REGT_POINTER | REGT_KONST, build->GetConstantAddress(regbuffer));
		paramcount++;
	}



	if (virtualselfreg == -1)
	{
		build->Emit(OP_CALL_K, build->GetConstantAddress(target), paramcount, vm_jit ? target->Proto->ReturnTypes.Size() : returns.Size());
	}
	else
	{
		ExpEmit funcreg(build, REGT_POINTER);

		build->Emit(OP_VTBL, funcreg.RegNum, virtualselfreg, target->VirtualIndex);
		build->Emit(OP_CALL, funcreg.RegNum, paramcount, vm_jit? target->Proto->ReturnTypes.Size() : returns.Size());
	}

	assert(returns.Size() < 2 || ReturnRegs != nullptr);

	ExpEmit retreg;
	for (unsigned i = 0; i < returns.Size(); i++)
	{
		ExpEmit reg(build, returns[i].first, returns[i].second);
		build->Emit(OP_RESULT, 0, EncodeRegType(reg), reg.RegNum);
		if (ReturnRegs) ReturnRegs->Push(reg);
		else retreg = reg;
	}
	if (vm_jit)	// The JIT compiler needs this, but the VM interpreter does not.
	{
		for (unsigned i = returns.Size(); i < target->Proto->ReturnTypes.Size(); i++)
		{
			ExpEmit reg(build, target->Proto->ReturnTypes[i]->RegType, target->Proto->ReturnTypes[i]->RegCount);
			build->Emit(OP_RESULT, 0, EncodeRegType(reg), reg.RegNum);
			reg.Free(build);
		}
	}
	return retreg;
}


VMDisassemblyDumper::VMDisassemblyDumper(const FileOperationType operation)
{
	static const char *const DUMP_ARG_NAME = "-dumpdisasm";

	if (Args->CheckParm(DUMP_ARG_NAME))
	{
		dump = fopen("disasm.txt", operation == Overwrite ? "w" : "a");
		namefilter = Args->CheckValue(DUMP_ARG_NAME);
		namefilter.ToLower();
	}
}

VMDisassemblyDumper::~VMDisassemblyDumper()
{
	if (dump != nullptr)
	{
		fprintf(dump, "\n*************************************************************************\n%i code bytes\n%i data bytes\n", codesize * 4, datasize);
		fclose(dump);
	}
}

void VMDisassemblyDumper::Write(VMScriptFunction *sfunc, const FString &fname)
{
	if (dump != nullptr)
	{
		if (namefilter.Len() > 0 && fname.MakeLower().IndexOf(namefilter) == -1)
		{
			return;
		}

		assert(sfunc != nullptr);

		DumpFunction(dump, sfunc, fname, (int)fname.Len());
		codesize += sfunc->CodeSize;
		datasize += sfunc->LineInfoCount * sizeof(FStatementInfo) + sfunc->ExtraSpace + sfunc->NumKonstD * sizeof(int) +
			sfunc->NumKonstA * sizeof(void*) + sfunc->NumKonstF * sizeof(double) + sfunc->NumKonstS * sizeof(FString);
	}
}

void VMDisassemblyDumper::Flush()
{
	if (dump != nullptr)
	{
		fflush(dump);
	}
}
