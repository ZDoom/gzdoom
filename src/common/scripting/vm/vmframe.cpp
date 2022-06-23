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
#include "stats.h"
#include "c_dispatch.h"

#include "vmintern.h"
#include "types.h"
#include "jit.h"
#include "c_cvars.h"
#include "version.h"

#ifdef HAVE_VM_JIT
#ifdef __DragonFly__
CUSTOM_CVAR(Bool, vm_jit, false, CVAR_NOINITCALL)
#else
CUSTOM_CVAR(Bool, vm_jit, true, CVAR_NOINITCALL)
#endif
{
	Printf("You must restart " GAMENAME " for this change to take effect.\n");
	Printf("This cvar is currently not saved. You must specify it on the command line.");
}
#else
CVAR(Bool, vm_jit, false, CVAR_NOINITCALL|CVAR_NOSET)
FString JitCaptureStackTrace(int framesToSkip, bool includeNativeFrames, int maxFrames) { return FString(); }
void JitRelease() {}
#endif

cycle_t VMCycles[10];
int VMCalls[10];

#if 0
IMPLEMENT_CLASS(VMException, false, false)
#endif

TArray<VMFunction *> VMFunction::AllFunctions;

// Creates the register type list for a function.
// Native functions only need this to assert their parameters in debug mode, script functions use this to load their registers from the VMValues.
void VMFunction::CreateRegUse()
{
#ifdef NDEBUG
	if (VarFlags & VARF_Native) return;	// we do not need this for native functions in release builds.
#endif
	int count = 0;
	if (!Proto)
	{
		//if (RegTypes) return;
		//Printf(TEXTCOLOR_ORANGE "Function without prototype needs register info manually set: %s\n", PrintableName.GetChars());
		return;
	}
	assert(Proto->isPrototype());

	for (auto arg : Proto->ArgumentTypes)
	{
		count += arg? arg->GetRegCount() : 1;
	}
	uint8_t *regp;
	RegTypes = regp = (uint8_t*)ClassDataAllocator.Alloc(count);
	count = 0;
	for (unsigned i = 0; i < Proto->ArgumentTypes.Size(); i++)
	{
		auto arg = Proto->ArgumentTypes[i];
		auto flg = ArgFlags.Size() > i ? ArgFlags[i] : 0;
		if (arg == nullptr)
		{
			// Marker for start of varargs.
			*regp++ = REGT_NIL;
		}
		else if ((flg & VARF_Out) && !arg->isPointer())
		{
			*regp++ = REGT_POINTER;
		}
		else for (int j = 0; j < arg->GetRegCount(); j++)
		{
			*regp++ = arg->GetRegType();
		}
	}
}

VMScriptFunction::VMScriptFunction(FName name)
{
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
	ScriptCall = &VMScriptFunction::FirstScriptCall;
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
	void *mem = ClassDataAllocator.Alloc(numops * sizeof(VMOP) +
						 numkonstd * sizeof(int) +
						 numkonstf * sizeof(double) +
						 numkonsts * sizeof(FString) +
						 numkonsta * sizeof(FVoidObj) +
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

static bool CanJit(VMScriptFunction *func)
{
	// Asmjit has a 256 register limit. Stay safely away from it as the jit compiler uses a few for temporaries as well.
	// Any function exceeding the limit will use the VM - a fair punishment to someone for writing a function so bloated ;)

	int maxregs = 200;
	if (func->NumRegA + func->NumRegD + func->NumRegF + func->NumRegS < maxregs)
		return true;

	Printf(TEXTCOLOR_ORANGE "%s is using too many registers (%d of max %d)! Function will not use native code.\n", func->PrintableName.GetChars(), func->NumRegA + func->NumRegD + func->NumRegF + func->NumRegS, maxregs);

	return false;
}

int VMScriptFunction::FirstScriptCall(VMFunction *func, VMValue *params, int numparams, VMReturn *ret, int numret)
{
	// [Player701] Check that we aren't trying to call an abstract function.
	// This shouldn't happen normally, but if it does, let's catch this explicitly
	// rather than let GZDoom crash.
	if (func->VarFlags & VARF_Abstract)
	{
		ThrowAbortException(X_OTHER, "attempt to call abstract function %s.", func->PrintableName.GetChars());
	}
#ifdef HAVE_VM_JIT
	if (vm_jit && CanJit(static_cast<VMScriptFunction*>(func)))
	{
		func->ScriptCall = JitCompile(static_cast<VMScriptFunction*>(func));
		if (!func->ScriptCall)
			func->ScriptCall = VMExec;
	}
	else
#endif // HAVE_VM_JIT
	{
		func->ScriptCall = VMExec;
	}

	return func->ScriptCall(func, params, numparams, ret, numret);
}

int VMNativeFunction::NativeScriptCall(VMFunction *func, VMValue *params, int numparams, VMReturn *returns, int numret)
{
	try
	{
		VMCycles[0].Unclock();
		numret = static_cast<VMNativeFunction *>(func)->NativeCall(VM_INVOKE(params, numparams, returns, numret, func->RegTypes));
		VMCycles[0].Clock();

		return numret;
	}
	catch (CVMAbortException &err)
	{
		err.MaybePrintMessage();
		err.stacktrace.AppendFormat("Called from %s\n", func->PrintableName.GetChars());
		throw;
	}
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
		Blocks = NULL;
	}
	if (UnusedBlocks != NULL)
	{
		BlockHeader *block, *next;
		for (block = UnusedBlocks; block != NULL; block = next)
		{
			next = block->NextBlock;
			delete[] (VM_UBYTE *)block;
		}
		UnusedBlocks = NULL;
	}
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
	VMFrame *frame = Alloc(func->StackSize);
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

int VMCall(VMFunction *func, VMValue *params, int numparams, VMReturn *results, int numresults/*, VMException **trap*/)
{
#if 0
	try
#endif
	{	
		if (func->VarFlags & VARF_Native)
		{
			return static_cast<VMNativeFunction *>(func)->NativeCall(VM_INVOKE(params, numparams, results, numresults, func->RegTypes));
		}
		else
		{
			auto code = static_cast<VMScriptFunction *>(func)->Code;
			// handle empty functions consisting of a single return explicitly so that empty virtual callbacks do not need to set up an entire VM frame.
			// code cann be null here in case of some non-fatal DECORATE errors.
			if (code == nullptr || code->word == (0x00808000|OP_RET))
			{
				return 0;
			}
			else if (code->word == (0x00048000|OP_RET))
			{
				if (numresults == 0) return 0;
				results[0].SetInt(static_cast<VMScriptFunction *>(func)->KonstD[0]);
				return 1;
			}
			else
			{
				VMCycles[0].Clock();

				auto sfunc = static_cast<VMScriptFunction *>(func);
				int numret = sfunc->ScriptCall(sfunc, params, numparams, results, numresults);
				VMCycles[0].Unclock();
				return numret;
			}
		}
	}
#if 0
	catch (VMException *exception)
	{
		if (trap != NULL)
		{
			*trap = exception;
			return -1;
		}
		throw;
	}
#endif
}

int VMCallWithDefaults(VMFunction *func, TArray<VMValue> &params, VMReturn *results, int numresults/*, VMException **trap = NULL*/)
{
	if (func->DefaultArgs.Size() > params.Size())
	{
		auto oldp = params.Size();
		params.Resize(func->DefaultArgs.Size());
		for (unsigned i = oldp; i < params.Size(); i++)
		{
			params[i] = func->DefaultArgs[i];
		}
	}
	return VMCall(func, params.Data(), params.Size(), results, numresults);
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

	case X_FORMAT_ERROR:
		AppendMessage("string format failed.");
		break;

	case X_OTHER:
		// no prepended message.
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
		// [Player701] avoid double space
		if (reason != X_OTHER)
		{
			AppendMessage(" ");
		}
		size_t len = strlen(m_Message);
		myvsnprintf(m_Message + len, MAX_ERRORTEXT - len, moreinfo, ap);
	}

	if (vm_jit)
		stacktrace = JitCaptureStackTrace(1, false);
	else
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
}

void ThrowAbortException(VMScriptFunction *sfunc, VMOP *line, EVMAbortException reason, const char *moreinfo, ...)
{
	va_list ap;
	va_start(ap, moreinfo);

	CVMAbortException err(reason, moreinfo, ap);

	err.stacktrace.AppendFormat("Called from %s at %s, line %d\n", sfunc->PrintableName.GetChars(), sfunc->SourceFileName.GetChars(), sfunc->PCToLine(line));
	throw err;
}

DEFINE_ACTION_FUNCTION(DObject, ThrowAbortException)
{
	PARAM_PROLOGUE;
	FString s = FStringFormat(VM_ARGS_NAMES);
	ThrowAbortException(X_OTHER, s.GetChars());
	return 0;
}

void NullParam(const char *varname)
{
	ThrowAbortException(X_READ_NIL, "In function parameter %s", varname);
}

#if 0
void ThrowVMException(VMException *x)
{
	throw x;
}
#endif


void ClearGlobalVMStack()
{
	while (GlobalVMStack.PopFrame() != nullptr)
	{
	}
}


ADD_STAT(VM)
{
	double added = 0;
	int addedc = 0;
	double peak = 0;
	for (auto d : VMCycles)
	{
		added += d.TimeMS();
		peak = max<double>(peak, d.TimeMS());
	}
	for (auto d : VMCalls) addedc += d;
	memmove(&VMCycles[1], &VMCycles[0], 9 * sizeof(cycle_t));
	memmove(&VMCalls[1], &VMCalls[0], 9 * sizeof(int));
	VMCycles[0].Reset();
	VMCalls[0] = 0;
	return FStringf("VM time in last 10 tics: %f ms, %d calls, peak = %f ms", added, addedc, peak);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------
CCMD(vmengine)
{
	if (argv.argc() == 2)
	{
		if (stricmp(argv[1], "default") == 0)
		{
			VMSelectEngine(VMEngine_Default);
			return;
		}
		else if (stricmp(argv[1], "checked") == 0)
		{
			VMSelectEngine(VMEngine_Checked);
			return;
		}
		else if (stricmp(argv[1], "unchecked") == 0)
		{
			VMSelectEngine(VMEngine_Unchecked);
			return;
		}
	}
	Printf("Usage: vmengine <default|checked|unchecked>\n");
}

