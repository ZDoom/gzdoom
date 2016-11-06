/*
** vmframe.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
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
#include "vm.h"

IMPLEMENT_CLASS(VMException)
IMPLEMENT_ABSTRACT_POINTY_CLASS(VMFunction)
 DECLARE_POINTER(Proto)
END_POINTERS
IMPLEMENT_CLASS(VMScriptFunction)
IMPLEMENT_CLASS(VMNativeFunction)

VMScriptFunction::VMScriptFunction(FName name)
{
	Native = false;
	Name = name;
	Code = NULL;
	KonstD = NULL;
	KonstF = NULL;
	KonstS = NULL;
	KonstA = NULL;
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

void VMScriptFunction::Alloc(int numops, int numkonstd, int numkonstf, int numkonsts, int numkonsta)
{
	assert(Code == NULL);
	assert(numops > 0);
	assert(numkonstd >= 0 && numkonstd <= 255);
	assert(numkonstf >= 0 && numkonstf <= 255);
	assert(numkonsts >= 0 && numkonsts <= 255);
	assert(numkonsta >= 0 && numkonsta <= 255);
	void *mem = M_Malloc(numops * sizeof(VMOP) +
						 numkonstd * sizeof(int) +
						 numkonstf * sizeof(double) +
						 numkonsts * sizeof(FString) +
						 numkonsta * (sizeof(FVoidObj) + 1));
	Code = (VMOP *)mem;
	mem = (void *)((VMOP *)mem + numops);

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
// Allocates a frame from the stack with the desired number of registers.
//
//===========================================================================

VMFrame *VMFrameStack::AllocFrame(int numregd, int numregf, int numregs, int numrega)
{
	assert((unsigned)numregd < 255);
	assert((unsigned)numregf < 255);
	assert((unsigned)numregs < 255);
	assert((unsigned)numrega < 255);
	// To keep the arguments to this function simpler, it assumes that every
	// register might be used as a parameter for a single call.
	int numparam = numregd + numregf + numregs + numrega;
	int size = VMFrame::FrameSize(numregd, numregf, numregs, numrega, numparam, 0);
	VMFrame *frame = Alloc(size);
	frame->NumRegD = numregd;
	frame->NumRegF = numregf;
	frame->NumRegS = numregs;
	frame->NumRegA = numrega;
	frame->MaxParam = numparam;
	frame->InitRegS();
	return frame;
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
	bool allocated = false;
	try
	{
		if (func->Native)
		{
			return static_cast<VMNativeFunction *>(func)->NativeCall(this, params, func->DefaultArgs, numparams, results, numresults);
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
	catch (EVMAbortException exception)
	{
		if (allocated)
		{
			PopFrame();
		}
		if (trap != nullptr)
		{
			*trap = nullptr;
		}

		Printf("VM execution aborted: ");
		switch (exception)
		{
		case X_READ_NIL:
			Printf("tried to read from address zero.");
			break;

		case X_WRITE_NIL:
			Printf("tried to write to address zero.");
			break;

		case X_TOO_MANY_TRIES:
			Printf("too many try-catch blocks.");
			break;

		case X_ARRAY_OUT_OF_BOUNDS:
			Printf("array access out of bounds.");
			break;

		case X_DIVISION_BY_ZERO:
			Printf("division by zero.");
			break;

		case X_BAD_SELF:
			Printf("invalid self pointer.");
			break;
		}
		Printf("\n");

		return -1;
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

class AActor;
void CallAction(VMFrameStack *stack, VMFunction *vmfunc, AActor *self)
{
	// Without the type cast this picks the 'void *' assignment...
	VMValue params[3] = { (DObject*)self, (DObject*)self, VMValue(nullptr, ATAG_GENERIC) };
	stack->Call(vmfunc, params, vmfunc->ImplicitArgs, nullptr, 0, nullptr);
}
