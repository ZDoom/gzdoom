/*
** vmframe.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
** Copyright 2016 Christoph Oelckers
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

#include <new>
#include "dobject.h"
#include "v_text.h"

IMPLEMENT_CLASS(VMException, false, false)
IMPLEMENT_CLASS(VMFunction, true, true)

IMPLEMENT_POINTERS_START(VMFunction)
	IMPLEMENT_POINTER(Proto)
IMPLEMENT_POINTERS_END

IMPLEMENT_CLASS(VMScriptFunction, false, false)
IMPLEMENT_CLASS(VMNativeFunction, false, false)

VMScriptFunction::VMScriptFunction(FName name)
{
	Native = false;
	Name = name;
	LineInfo = nullptr;
	Code = NULL;
	KonstD = NULL;
	KonstF = NULL;
	KonstS = NULL;
	KonstA = NULL;
	LineInfoCount = 0;
	ExtraSpace = 0;
	CodeSize = 0;
	NumRegD = 0;
	NumRegF = 0;
	NumRegS = 0;
	NumRegA = 0;
	NumKonstD = 0;
	NumKonstF = 0;
	NumKonstS = 0;
	NumKonstA = 0;
	MaxParam = 0;
	NumArgs = 0;
}

VMScriptFunction::~VMScriptFunction()
{
	if (Code != NULL)
	{
		if (KonstS != NULL)
		{
			for (int i = 0; i < NumKonstS; ++i)
			{
				KonstS[i].~FString();
			}
		}
		M_Free(Code);
	}
}

void VMScriptFunction::Alloc(int numops, int numkonstd, int numkonstf, int numkonsts, int numkonsta, int numlinenumbers)
{
	assert(Code == NULL);
	assert(numops > 0);
	assert(numkonstd >= 0 && numkonstd <= 65535);
	assert(numkonstf >= 0 && numkonstf <= 65535);
	assert(numkonsts >= 0 && numkonsts <= 65535);
	assert(numkonsta >= 0 && numkonsta <= 65535);
	assert(numlinenumbers >= 0 && numlinenumbers <= 65535);
	void *mem = M_Malloc(numops * sizeof(VMOP) +
						 numkonstd * sizeof(int) +
						 numkonstf * sizeof(double) +
						 numkonsts * sizeof(FString) +
						 numkonsta * (sizeof(FVoidObj) + 1) +
						 numlinenumbers * sizeof(FStatementInfo));
	Code = (VMOP *)mem;
	mem = (void *)((VMOP *)mem + numops);

	if (numlinenumbers > 0)
	{
		LineInfo = (FStatementInfo*)mem;
		LineInfoCount = numlinenumbers;
		mem = LineInfo + numlinenumbers;
	}
	else
	{
		LineInfo = nullptr;
		LineInfoCount = 0;
	}
	if (numkonstd > 0)
	{
		KonstD = (int *)mem;
		mem = (void *)((int *)mem + numkonstd);
	}
	else
	{
		KonstD = NULL;
	}
	if (numkonstf > 0)
	{
		KonstF = (double *)mem;
		mem = (void *)((double *)mem + numkonstf);
	}
	else
	{
		KonstF = NULL;
	}
	if (numkonsts > 0)
	{
		KonstS = (FString *)mem;
		for (int i = 0; i < numkonsts; ++i)
		{
			::new(&KonstS[i]) FString;
		}
		mem = (void *)((FString *)mem + numkonsts);
	}
	else
	{
		KonstS = NULL;
	}
	if (numkonsta > 0)
	{
		KonstA = (FVoidObj *)mem;
	}
	else
	{
		KonstA = NULL;
	}
	CodeSize = numops;
	NumKonstD = numkonstd;
	NumKonstF = numkonstf;
	NumKonstS = numkonsts;
	NumKonstA = numkonsta;
}

size_t VMScriptFunction::PropagateMark()
{
	if (KonstA != NULL)
	{
		FVoidObj *konsta = KonstA;
		VM_UBYTE *atag = KonstATags();
		for (int count = NumKonstA; count > 0; --count)
		{
			if (*atag++ == ATAG_OBJECT)
			{
				GC::Mark(konsta->o);
			}
			konsta++;
		}
	}
	return NumKonstA * sizeof(void *) + Super::PropagateMark();
}

void VMScriptFunction::InitExtra(void *addr)
{
	char *caddr = (char*)addr;

	for (auto tao : SpecialInits)
	{
		tao.first->InitializeValue(caddr + tao.second, nullptr);
	}
}

void VMScriptFunction::DestroyExtra(void *addr)
{
	char *caddr = (char*)addr;

	for (auto tao : SpecialInits)
	{
		tao.first->DestroyValue(caddr + tao.second);
	}
}

int VMScriptFunction::AllocExtraStack(PType *type)
{
	int address = ((ExtraSpace + type->Align - 1) / type->Align) * type->Align;
	ExtraSpace = address + type->Size;
	type->SetDefaultValue(nullptr, address, &SpecialInits);
	return address;
}

int VMScriptFunction::PCToLine(const VMOP *pc)
{
	int PCIndex = int(pc - Code);
	if (LineInfoCount == 1) return LineInfo[0].LineNumber;
	for (unsigned i = 1; i < LineInfoCount; i++)
	{
		if (LineInfo[i].InstructionIndex > PCIndex)
		{
			return LineInfo[i - 1].LineNumber;
		}
	}
	return -1;
}

//===========================================================================
//
// VMFrame :: InitRegS
//
// Initialize the string registers of a newly-allocated VMFrame.
//
//===========================================================================

void VMFrame::InitRegS()
{
	FString *regs = GetRegS();
	for (int i = 0; i < NumRegS; ++i)
	{
		::new(&regs[i]) FString;
	}
}

//===========================================================================
//
// VMFrameStack - Constructor
//
//===========================================================================

VMFrameStack::VMFrameStack()
{
	Blocks = NULL;
	UnusedBlocks = NULL;
}

//===========================================================================
//
// VMFrameStack - Destructor
//
//===========================================================================

VMFrameStack::~VMFrameStack()
{
	while (PopFrame() != NULL)
	{ }
	if (Blocks != NULL)
	{
		BlockHeader *block, *next;
		for (block = Blocks; block != NULL; block = next)
		{
			next = block->NextBlock;
			delete[] (VM_UBYTE *)block;
		}
	}
	if (UnusedBlocks != NULL)
	{
		BlockHeader *block, *next;
		for (block = UnusedBlocks; block != NULL; block = next)
		{
			next = block->NextBlock;
			delete[] (VM_UBYTE *)block;
		}
	}
	Blocks = NULL;
	UnusedBlocks = NULL;
}

//===========================================================================
//
// VMFrameStack :: AllocFrame
//
// Allocates a frame from the stack suitable for calling a particular
// function.
//
//===========================================================================

VMFrame *VMFrameStack::AllocFrame(VMScriptFunction *func)
{
	int size = VMFrame::FrameSize(func->NumRegD, func->NumRegF, func->NumRegS, func->NumRegA,
		func->MaxParam, func->ExtraSpace);
	VMFrame *frame = Alloc(size);
	frame->Func = func;
	frame->NumRegD = func->NumRegD;
	frame->NumRegF = func->NumRegF;
	frame->NumRegS = func->NumRegS;
	frame->NumRegA = func->NumRegA;
	frame->MaxParam = func->MaxParam;
	frame->Func = func;
	frame->InitRegS();
	if (func->SpecialInits.Size())
	{
		func->InitExtra(frame->GetExtra());
	}
	return frame;
}

//===========================================================================
//
// VMFrameStack :: Alloc
//
// Allocates space for a frame. Its size will be rounded up to a multiple
// of 16 bytes.
//
//===========================================================================

VMFrame *VMFrameStack::Alloc(int size)
{
	BlockHeader *block;
	VMFrame *frame, *parent;

	size = (size + 15) & ~15;
	block = Blocks;
	if (block != NULL)
	{
		parent = block->LastFrame;
	}
	else
	{
		parent = NULL;
	}
	if (block == NULL || ((VM_UBYTE *)block + block->BlockSize) < (block->FreeSpace + size))
	{ // Not enough space. Allocate a new block.
		int blocksize = ((sizeof(BlockHeader) + 15) & ~15) + size;
		BlockHeader **blockp;
		if (blocksize < BLOCK_SIZE)
		{
			blocksize = BLOCK_SIZE;
		}
		for (blockp = &UnusedBlocks, block = *blockp; block != NULL; block = block->NextBlock)
		{
			if (block->BlockSize >= blocksize)
			{
				break;
			}
		}
		if (block != NULL)
		{
			*blockp = block->NextBlock;
		}
		else
		{
			block = (BlockHeader *)new VM_UBYTE[blocksize];
			block->BlockSize = blocksize;
		}
		block->InitFreeSpace();
		block->LastFrame = NULL;
		block->NextBlock = Blocks;
		Blocks = block;
	}
	frame = (VMFrame *)block->FreeSpace;
	memset(frame, 0, size);
	frame->ParentFrame = parent;
	block->FreeSpace += size;
	block->LastFrame = frame;
	return frame;
}


//===========================================================================
//
// VMFrameStack :: PopFrame
//
// Pops the top frame off the stack, returning a pointer to the new top
// frame.
//
//===========================================================================

VMFrame *VMFrameStack::PopFrame()
{
	if (Blocks == NULL)
	{
		return NULL;
	}
	VMFrame *frame = Blocks->LastFrame;
	if (frame == NULL)
	{
		return NULL;
	}
	auto Func = static_cast<VMScriptFunction *>(frame->Func);
	if (Func->SpecialInits.Size())
	{
		Func->DestroyExtra(frame->GetExtra());
	}
	// Free any string registers this frame had.
	FString *regs = frame->GetRegS();
	for (int i = frame->NumRegS; i != 0; --i)
	{
		(regs++)->~FString();
	}
	// Free any parameters this frame left behind.
	VMValue *param = frame->GetParam();
	for (int i = frame->NumParam; i != 0; --i)
	{
		(param++)->~VMValue();
	}
	VMFrame *parent = frame->ParentFrame;
	if (parent == NULL)
	{
		// Popping the last frame off the stack.
		if (Blocks != NULL)
		{
			assert(Blocks->NextBlock == NULL);
			Blocks->LastFrame = NULL;
			Blocks->InitFreeSpace();
		}
		return NULL;
	}
	if ((VM_UBYTE *)parent < (VM_UBYTE *)Blocks || (VM_UBYTE *)parent >= (VM_UBYTE *)Blocks + Blocks->BlockSize)
	{ // Parent frame is in a different block, so move this one to the unused list.
		BlockHeader *next = Blocks->NextBlock;
		assert(next != NULL);
		assert((VM_UBYTE *)parent >= (VM_UBYTE *)next && (VM_UBYTE *)parent < (VM_UBYTE *)next + next->BlockSize);
		Blocks->NextBlock = UnusedBlocks;
		UnusedBlocks = Blocks;
		Blocks = next;
	}
	else
	{
		Blocks->LastFrame = parent;
		Blocks->FreeSpace = (VM_UBYTE *)frame;
	}
	return parent;
}

//===========================================================================
//
// VMFrameStack :: Call
//
// Calls a function, either native or scripted. If an exception occurs while
// executing, the stack is cleaned up. If trap is non-NULL, it is set to the
// VMException that was caught and the return value is negative. Otherwise,
// any caught exceptions will be rethrown. Under normal termination, the
// return value is the number of results from the function.
//
//===========================================================================

int VMFrameStack::Call(VMFunction *func, VMValue *params, int numparams, VMReturn *results, int numresults, VMException **trap)
{
	assert(this == &GlobalVMStack);	// why would anyone even want to create a local stack?
	bool allocated = false;
	try
	{	
		if (func->Native)
		{
			return static_cast<VMNativeFunction *>(func)->NativeCall(params, func->DefaultArgs, numparams, results, numresults);
		}
		else
		{
			AllocFrame(static_cast<VMScriptFunction *>(func));
			allocated = true;
			VMFillParams(params, TopFrame(), numparams);
			int numret = VMExec(this, static_cast<VMScriptFunction *>(func)->Code, results, numresults);
			PopFrame();
			return numret;
		}
	}
	catch (VMException *exception)
	{
		if (allocated)
		{
			PopFrame();
		}
		if (trap != NULL)
		{
			*trap = exception;
			return -1;
		}
		throw;
	}
	catch (...)
	{
		if (allocated)
		{
			PopFrame();
		}
		throw;
	}
}

// Exception stuff for the VM is intentionally placed there, because having this in vmexec.cpp would subject it to inlining
// which we do not want because it increases the local stack requirements of Exec which are already too high.
FString CVMAbortException::stacktrace;

CVMAbortException::CVMAbortException(EVMAbortException reason, const char *moreinfo, va_list ap)
{
	SetMessage("VM execution aborted: ");
	switch (reason)
	{
	case X_READ_NIL:
		AppendMessage("tried to read from address zero.");
		break;

	case X_WRITE_NIL:
		AppendMessage("tried to write to address zero.");
		break;

	case X_TOO_MANY_TRIES:
		AppendMessage("too many try-catch blocks.");
		break;

	case X_ARRAY_OUT_OF_BOUNDS:
		AppendMessage("array access out of bounds.");
		break;

	case X_DIVISION_BY_ZERO:
		AppendMessage("division by zero.");
		break;

	case X_BAD_SELF:
		AppendMessage("invalid self pointer.");
		break;

	default:
	{
		size_t len = strlen(m_Message);
		mysnprintf(m_Message + len, MAX_ERRORTEXT - len, "Unknown reason %d", reason);
		break;
	}
	}
	if (moreinfo != nullptr)
	{
		AppendMessage(" ");
		size_t len = strlen(m_Message);
		myvsnprintf(m_Message + len, MAX_ERRORTEXT - len, moreinfo, ap);
	}
	stacktrace = "";
}

// Print this only once on the first catch block.
void CVMAbortException::MaybePrintMessage()
{
	auto m = GetMessage();
	if (m != nullptr)
	{
		Printf(TEXTCOLOR_RED);
		Printf("%s\n", m);
		SetMessage("");
	}
}


void ThrowAbortException(EVMAbortException reason, const char *moreinfo, ...)
{
	va_list ap;
	va_start(ap, moreinfo);
	throw CVMAbortException(reason, moreinfo, ap);
	va_end(ap);
}

void NullParam(const char *varname)
{
	ThrowAbortException(X_READ_NIL, "In function parameter %s", varname);
}

void ThrowVMException(VMException *x)
{
	throw x;
}
