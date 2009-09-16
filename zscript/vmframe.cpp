#include <new>
#include "vm.h"


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
		block->FreeSpace = (VM_UBYTE *)block + ((sizeof(BlockHeader) + 15) & ~15);
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
			Blocks->FreeSpace = (VM_UBYTE *)Blocks + ((sizeof(BlockHeader) + 15) & ~15);
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
			return static_cast<VMNativeFunction *>(func)->NativeCall(this, params, numparams, results, numresults);
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
