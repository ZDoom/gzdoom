
#include "jit.h"
#include "jitintern.h"

static TArray<uint8_t*> JitBlocks;
static TArray<uint8_t*> JitFrames;
static size_t JitBlockPos = 0;
static size_t JitBlockSize = 0;

asmjit::CodeInfo GetHostCodeInfo()
{
	static bool firstCall = true;
	static asmjit::CodeInfo codeInfo;

	if (firstCall)
	{
		asmjit::JitRuntime rt;
		codeInfo = rt.getCodeInfo();
		firstCall = false;
	}

	return codeInfo;
}

static void *AllocJitMemory(size_t size)
{
	using namespace asmjit;

	if (JitBlockPos + size <= JitBlockSize)
	{
		uint8_t *p = JitBlocks[JitBlocks.Size() - 1];
		p += JitBlockPos;
		JitBlockPos += size;
		return p;
	}
	else
	{
		size_t allocatedSize = 0;
		void *p = OSUtils::allocVirtualMemory(1024 * 1024, &allocatedSize, OSUtils::kVMWritable | OSUtils::kVMExecutable);
		if (!p)
			return nullptr;
		JitBlocks.Push((uint8_t*)p);
		JitBlockSize = allocatedSize;
		JitBlockPos = size;
		return p;
	}
}

#ifdef WIN32

#define UWOP_PUSH_NONVOL 0
#define UWOP_ALLOC_LARGE 1
#define UWOP_ALLOC_SMALL 2
#define UWOP_SET_FPREG 3
#define UWOP_SAVE_NONVOL 4
#define UWOP_SAVE_NONVOL_FAR 5
#define UWOP_SAVE_XMM128 8
#define UWOP_SAVE_XMM128_FAR 9
#define UWOP_PUSH_MACHFRAME 10

static TArray<uint16_t> CreateUnwindInfoWindows(asmjit::CCFunc *func)
{
	using namespace asmjit;
	FuncFrameLayout layout;
	Error error = layout.init(func->getDetail(), func->getFrameInfo());
	if (error != kErrorOk)
		I_FatalError("FuncFrameLayout.init failed");

	// We need a dummy emitter for instruction size calculations
	CodeHolder code;
	code.init(GetHostCodeInfo());
	X86Assembler assembler(&code);
	X86Emitter *emitter = assembler.asEmitter();

	// Build UNWIND_CODE codes:

	TArray<uint16_t> codes;
	uint32_t opoffset, opcode, opinfo;

	// Note: this must match exactly what X86Internal::emitProlog does

	X86Gp zsp = emitter->zsp();   // ESP|RSP register.
	X86Gp zbp = emitter->zsp();   // EBP|RBP register.
	zbp.setId(X86Gp::kIdBp);
	X86Gp gpReg = emitter->zsp(); // General purpose register (temporary).
	X86Gp saReg = emitter->zsp(); // Stack-arguments base register.
	uint32_t gpSaved = layout.getSavedRegs(X86Reg::kKindGp);

	if (layout.hasPreservedFP())
	{
		// Emit: 'push zbp'
		//       'mov  zbp, zsp'.
		gpSaved &= ~Utils::mask(X86Gp::kIdBp);
		emitter->push(zbp);

		opoffset = (uint32_t)assembler.getOffset();
		opcode = UWOP_PUSH_NONVOL;
		opinfo = X86Gp::kIdBp;
		codes.Push(opoffset | (opcode << 8) | (opinfo << 12));

		emitter->mov(zbp, zsp);
	}

	if (gpSaved)
	{
		for (uint32_t i = gpSaved, regId = 0; i; i >>= 1, regId++)
		{
			if (!(i & 0x1)) continue;
			// Emit: 'push gp' sequence.
			gpReg.setId(regId);
			emitter->push(gpReg);

			opoffset = (uint32_t)assembler.getOffset();
			opcode = UWOP_PUSH_NONVOL;
			opinfo = regId;
			codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
		}
	}

	uint32_t stackArgsRegId = layout.getStackArgsRegId();
	if (stackArgsRegId != Globals::kInvalidRegId && stackArgsRegId != X86Gp::kIdSp)
	{
		saReg.setId(stackArgsRegId);
		if (!(layout.hasPreservedFP() && stackArgsRegId == X86Gp::kIdBp))
		{
			// Emit: 'mov saReg, zsp'.
			emitter->mov(saReg, zsp);
		}
	}

	if (layout.hasDynamicAlignment())
	{
		// Emit: 'and zsp, StackAlignment'.
		emitter->and_(zsp, -static_cast<int32_t>(layout.getStackAlignment()));
	}

	if (layout.hasStackAdjustment())
	{
		// Emit: 'sub zsp, StackAdjustment'.
		emitter->sub(zsp, layout.getStackAdjustment());

		uint32_t stackadjust = layout.getStackAdjustment();
		if (stackadjust <= 128)
		{
			opoffset = (uint32_t)assembler.getOffset();
			opcode = UWOP_ALLOC_SMALL;
			opinfo = stackadjust / 8 - 1;
			codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
		}
		else if (stackadjust <= 512 * 1024 - 8)
		{
			opoffset = (uint32_t)assembler.getOffset();
			opcode = UWOP_ALLOC_LARGE;
			opinfo = 0;
			codes.Push(stackadjust / 8);
			codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
		}
		else
		{
			opoffset = (uint32_t)assembler.getOffset();
			opcode = UWOP_ALLOC_LARGE;
			opinfo = 1;
			codes.Push((uint16_t)(stackadjust >> 16));
			codes.Push((uint16_t)stackadjust);
			codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
		}
	}

	if (layout.hasDynamicAlignment() && layout.hasDsaSlotUsed())
	{
		// Emit: 'mov [zsp + dsaSlot], saReg'.
		X86Mem saMem = x86::ptr(zsp, layout._dsaSlot);
		emitter->mov(saMem, saReg);
	}

	uint32_t xmmSaved = layout.getSavedRegs(X86Reg::kKindVec);
	if (xmmSaved)
	{
		X86Mem vecBase = x86::ptr(zsp, layout.getVecStackOffset());
		X86Reg vecReg = x86::xmm(0);
		bool avx = layout.isAvxEnabled();
		bool aligned = layout.hasAlignedVecSR();
		uint32_t vecInst = aligned ? (avx ? X86Inst::kIdVmovaps : X86Inst::kIdMovaps) : (avx ? X86Inst::kIdVmovups : X86Inst::kIdMovups);
		uint32_t vecSize = 16;
		for (uint32_t i = xmmSaved, regId = 0; i; i >>= 1, regId++)
		{
			if (!(i & 0x1)) continue;

			// Emit 'movaps|movups [zsp + X], xmm0..15'.
			vecReg.setId(regId);
			emitter->emit(vecInst, vecBase, vecReg);
			vecBase.addOffsetLo32(static_cast<int32_t>(vecSize));

			if (vecBase.getOffsetLo32() / vecSize < (1 << 16))
			{
				opoffset = (uint32_t)assembler.getOffset();
				opcode = UWOP_SAVE_XMM128;
				opinfo = regId;
				codes.Push(vecBase.getOffsetLo32() / vecSize);
				codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
			}
			else
			{
				opoffset = (uint32_t)assembler.getOffset();
				opcode = UWOP_SAVE_XMM128_FAR;
				opinfo = regId;
				codes.Push((uint16_t)(vecBase.getOffsetLo32() >> 16));
				codes.Push((uint16_t)vecBase.getOffsetLo32());
				codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
			}
		}
	}

	// Build the UNWIND_INFO structure:

	uint16_t version = 1, flags = 0, frameRegister = 0, frameOffset = 0;
	uint16_t sizeOfProlog = (uint16_t)assembler.getOffset();
	uint16_t countOfCodes = (uint16_t)codes.Size();

	TArray<uint16_t> info;
	info.Push(version | (flags << 3) | (sizeOfProlog << 8));
	info.Push(countOfCodes | (frameRegister << 8) | (frameOffset << 12));

	for (unsigned int i = codes.Size(); i > 0; i--)
		info.Push(codes[i - 1]);

	if (codes.Size() % 2 == 1)
		info.Push(0);

	return info;
}

void *AddJitFunction(asmjit::CodeHolder* code, asmjit::CCFunc *func)
{
	using namespace asmjit;

	size_t codeSize = code->getCodeSize();
	if (codeSize == 0)
		return nullptr;

#ifdef _WIN64
	TArray<uint16_t> unwindInfo = CreateUnwindInfoWindows(func);
	size_t unwindInfoSize = unwindInfo.Size() * sizeof(uint16_t);
	size_t functionTableSize = sizeof(RUNTIME_FUNCTION);
#else
	size_t unwindInfoSize = 0;
	size_t functionTableSize = 0;
#endif

	codeSize = (codeSize + 15) / 16 * 16;

	uint8_t *p = (uint8_t *)AllocJitMemory(codeSize + unwindInfoSize + functionTableSize);
	if (!p)
		return nullptr;

	size_t relocSize = code->relocate(p);
	if (relocSize == 0)
		return nullptr;

	size_t unwindStart = relocSize;
	unwindStart = (unwindStart + 15) / 16 * 16;
	JitBlockPos -= codeSize - unwindStart;

#ifdef _WIN64
	uint8_t *baseaddr = JitBlocks.Last();
	uint8_t *startaddr = p;
	uint8_t *endaddr = p + relocSize;
	uint8_t *unwindptr = p + unwindStart;
	memcpy(unwindptr, &unwindInfo[0], unwindInfoSize);

	RUNTIME_FUNCTION *table = (RUNTIME_FUNCTION*)(unwindptr + unwindInfoSize);
	table[0].BeginAddress = (DWORD)(ptrdiff_t)(startaddr - baseaddr);
	table[0].EndAddress = (DWORD)(ptrdiff_t)(endaddr - baseaddr);
	table[0].UnwindInfoAddress = (DWORD)(ptrdiff_t)(unwindptr - baseaddr);
	BOOLEAN result = RtlAddFunctionTable(table, 1, (DWORD64)baseaddr);
	JitFrames.Push((uint8_t*)table);
	if (result == 0)
		I_FatalError("RtlAddFunctionTable failed");
#endif

	return p;
}

#else

extern "C"
{
	void __register_frame(const void*);
	void __deregister_frame(const void*);
}

static void WriteLength(TArray<uint8_t> &stream, unsigned int pos, unsigned int v)
{
	stream[pos] = v >> 24;
	stream[pos + 1] = (v >> 16) & 0xff;
	stream[pos + 2] = (v >> 8) & 0xff;
	stream[pos + 3] = v & 0xff;
}

static void WriteUInt64(TArray<uint8_t> &stream, uint64_t v)
{
	stream.Push(v >> 56);
	stream.Push((v >> 48) & 0xff);
	stream.Push((v >> 40) & 0xff);
	stream.Push((v >> 32) & 0xff);
	stream.Push((v >> 24) & 0xff);
	stream.Push((v >> 16) & 0xff);
	stream.Push((v >> 8) & 0xff);
	stream.Push(v & 0xff);
}

static void WriteUInt32(TArray<uint8_t> &stream, uint32_t v)
{
	stream.Push(v >> 24);
	stream.Push((v >> 16) & 0xff);
	stream.Push((v >> 8) & 0xff);
	stream.Push(v & 0xff);
}

static void WriteUInt16(TArray<uint8_t> &stream, uint16_t v)
{
	stream.Push((v >> 8) & 0xff);
	stream.Push(v & 0xff);
}

static void WriteUInt8(TArray<uint8_t> &stream, uint8_t v)
{
	stream.Push(v);
}

static void WriteULEB128(TArray<uint8_t> &stream, uint32_t v)
{
	while (true)
	{
		if (v < 128)
		{
			WriteUInt8(stream, v);
			break;
		}
		else
		{
			WriteUInt8(stream, (v & 0x7f) | 0x80);
			v >>= 7;
		}
	}
}

static void WriteSLEB128(TArray<uint8_t> &stream, int32_t v)
{
	if (v >= 0)
	{
		WriteULEB128(stream, v);
	}
	else
	{
		// To do: sign extended version
	}
}

struct FrameDesc
{
	int minInstAlignment = 4;
	int dataAlignmentFactor = -4;
	uint8_t returnAddressReg = 0;

	uint32_t cieLocation = 0;
	uint64_t functionStart = 0;
	uint64_t functionSize = 0;
};

static void WriteCIE(TArray<uint8_t> &stream, const TArray<uint8_t> &cieInstructions, uint8_t returnAddressReg, int minInstAlignment, int dataAlignmentFactor)
{
	unsigned int lengthPos = stream.Size();
	WriteUInt32(stream, 0); // Length

	WriteUInt32(stream, 0); // CIE ID
	WriteUInt8(stream, 1); // CIE Version
	WriteUInt8(stream, 'z');
	WriteUInt8(stream, 'R');
	WriteUInt8(stream, 0);
	WriteULEB128(stream, minInstAlignment);
	WriteSLEB128(stream, dataAlignmentFactor);
	WriteUInt8(stream, returnAddressReg);
	WriteULEB128(stream, 0);

	for (unsigned int i = 0; i < cieInstructions.Size(); i++)
		stream.Push(cieInstructions[i]);

	// Padding and update length field
	unsigned int length = stream.Size() - lengthPos;
	int padding = stream.Size() % 4;
	for (int i = 0; i <= padding; i++) WriteUInt8(stream, 0);
	WriteLength(stream, lengthPos, length);
}

static void WriteFDE(TArray<uint8_t> &stream, const TArray<uint8_t> &fdeInstructions, uint32_t cieLocation, unsigned int &functionStart)
{
	uint32_t offsetToCIE = stream.Size() - cieLocation;

	unsigned int lengthPos = stream.Size();
	WriteUInt32(stream, 0); // Length

	WriteUInt32(stream, offsetToCIE);
	functionStart = stream.Size();
	WriteUInt64(stream, 0); // func start
	WriteUInt64(stream, 0); // func size

	for (unsigned int i = 0; i < fdeInstructions.Size(); i++)
		stream.Push(fdeInstructions[i]);

	// Padding and update length field
	unsigned int length = stream.Size() - lengthPos;
	int padding = stream.Size() % 4;
	for (int i = 0; i <= padding; i++) WriteUInt8(stream, 0);
	WriteLength(stream, lengthPos, length);
}

static void WriteAdvanceLoc(TArray<uint8_t> &fdeInstructions, uint64_t offset, uint64_t &lastOffset)
{
	uint64_t delta = offset - lastOffset;
	if (delta < (1 << 8))
	{
		WriteUInt8(fdeInstructions, 2); // DW_CFA_advance_loc1
		WriteUInt8(fdeInstructions, delta);
	}
	else if (delta < (1 << 16))
	{
		WriteUInt8(fdeInstructions, 3); // DW_CFA_advance_loc2
		WriteUInt16(fdeInstructions, delta);
	}
	else
	{
		WriteUInt8(fdeInstructions, 4); // DW_CFA_advance_loc3
		WriteUInt32(fdeInstructions, delta);
	}
	lastOffset = offset;
}

static TArray<uint8_t> CreateUnwindInfoUnix(asmjit::CCFunc *func, unsigned int &functionStart)
{
	using namespace asmjit;

	FuncFrameLayout layout;
	Error error = layout.init(func->getDetail(), func->getFrameInfo());
	if (error != kErrorOk)
		I_FatalError("FuncFrameLayout.init failed");

	// We need a dummy emitter for instruction size calculations
	CodeHolder code;
	code.init(GetHostCodeInfo());
	X86Assembler assembler(&code);
	X86Emitter *emitter = assembler.asEmitter();

	// Build .eh_frame:
	// (see appendix D.6 "Call Frame Information Example" in the DWARF 5 spec)
	
	TArray<uint8_t> cieInstructions;
	TArray<uint8_t> fdeInstructions;
	uint64_t lastOffset = 0;

	int minInstAlignment = 4; // To do: is this correct?
	int dataAlignmentFactor = -4; // To do: is this correct?
	uint8_t returnAddressReg = 0; // To do: get this from asmjit

	// To do: do we need to write register defaults into the CIE or does the defaults match the x64 calling convention?

	// Note: this must match exactly what X86Internal::emitProlog does

	X86Gp zsp = emitter->zsp();   // ESP|RSP register.
	X86Gp zbp = emitter->zsp();   // EBP|RBP register.
	zbp.setId(X86Gp::kIdBp);
	X86Gp gpReg = emitter->zsp(); // General purpose register (temporary).
	X86Gp saReg = emitter->zsp(); // Stack-arguments base register.
	uint32_t gpSaved = layout.getSavedRegs(X86Reg::kKindGp);

	if (layout.hasPreservedFP())
	{
		// Emit: 'push zbp'
		//       'mov  zbp, zsp'.
		gpSaved &= ~Utils::mask(X86Gp::kIdBp);
		emitter->push(zbp);

		WriteAdvanceLoc(fdeInstructions, assembler.getOffset(), lastOffset);
		// WriteXX(cieInstructions, UWOP_PUSH_NONVOL);
		// WriteXX(cieInstructions, X86Gp::kIdBp);

		emitter->mov(zbp, zsp);
	}

	if (gpSaved)
	{
		for (uint32_t i = gpSaved, regId = 0; i; i >>= 1, regId++)
		{
			if (!(i & 0x1)) continue;
			// Emit: 'push gp' sequence.
			gpReg.setId(regId);
			emitter->push(gpReg);

			WriteAdvanceLoc(fdeInstructions, assembler.getOffset(), lastOffset);
			// WriteXX(cieInstructions, UWOP_PUSH_NONVOL);
			// WriteXX(cieInstructions, regId);
		}
	}

	uint32_t stackArgsRegId = layout.getStackArgsRegId();
	if (stackArgsRegId != Globals::kInvalidRegId && stackArgsRegId != X86Gp::kIdSp)
	{
		saReg.setId(stackArgsRegId);
		if (!(layout.hasPreservedFP() && stackArgsRegId == X86Gp::kIdBp))
		{
			// Emit: 'mov saReg, zsp'.
			emitter->mov(saReg, zsp);
		}
	}

	if (layout.hasDynamicAlignment())
	{
		// Emit: 'and zsp, StackAlignment'.
		emitter->and_(zsp, -static_cast<int32_t>(layout.getStackAlignment()));
	}

	if (layout.hasStackAdjustment())
	{
		// Emit: 'sub zsp, StackAdjustment'.
		emitter->sub(zsp, layout.getStackAdjustment());

		uint32_t stackadjust = layout.getStackAdjustment();
		WriteAdvanceLoc(fdeInstructions, assembler.getOffset(), lastOffset);
		WriteUInt8(fdeInstructions, 0x0e); // DW_CFA_def_cfa_offset
		WriteULEB128(fdeInstructions, stackadjust);
	}

	if (layout.hasDynamicAlignment() && layout.hasDsaSlotUsed())
	{
		// Emit: 'mov [zsp + dsaSlot], saReg'.
		X86Mem saMem = x86::ptr(zsp, layout._dsaSlot);
		emitter->mov(saMem, saReg);
	}

	uint32_t xmmSaved = layout.getSavedRegs(X86Reg::kKindVec);
	if (xmmSaved)
	{
		X86Mem vecBase = x86::ptr(zsp, layout.getVecStackOffset());
		X86Reg vecReg = x86::xmm(0);
		bool avx = layout.isAvxEnabled();
		bool aligned = layout.hasAlignedVecSR();
		uint32_t vecInst = aligned ? (avx ? X86Inst::kIdVmovaps : X86Inst::kIdMovaps) : (avx ? X86Inst::kIdVmovups : X86Inst::kIdMovups);
		uint32_t vecSize = 16;
		for (uint32_t i = xmmSaved, regId = 0; i; i >>= 1, regId++)
		{
			if (!(i & 0x1)) continue;

			// Emit 'movaps|movups [zsp + X], xmm0..15'.
			vecReg.setId(regId);
			emitter->emit(vecInst, vecBase, vecReg);
			vecBase.addOffsetLo32(static_cast<int32_t>(vecSize));

			WriteAdvanceLoc(fdeInstructions, assembler.getOffset(), lastOffset);
			// WriteXX(cieInstructions, UWOP_SAVE_XMM128);
			// WriteXX(cieInstructions, regId);
		}
	}

	TArray<uint8_t> stream;
	WriteCIE(stream, cieInstructions, returnAddressReg, minInstAlignment, dataAlignmentFactor);
	WriteFDE(stream, fdeInstructions, 0, functionStart);
	WriteUInt32(stream, 0);
	return stream;
}

void *AddJitFunction(asmjit::CodeHolder* code, asmjit::CCFunc *func)
{
	using namespace asmjit;

	size_t codeSize = code->getCodeSize();
	if (codeSize == 0)
		return nullptr;

	unsigned int fdeFunctionStart = 0;
	TArray<uint8_t> unwindInfo;// = CreateUnwindInfoUnix(func, fdeFunctionStart);
	size_t unwindInfoSize = unwindInfo.Size();

	codeSize = (codeSize + 15) / 16 * 16;

	uint8_t *p = (uint8_t *)AllocJitMemory(codeSize + unwindInfoSize);
	if (!p)
		return nullptr;

	size_t relocSize = code->relocate(p);
	if (relocSize == 0)
		return nullptr;

	size_t unwindStart = relocSize;
	unwindStart = (unwindStart + 15) / 16 * 16;
	JitBlockPos -= codeSize - unwindStart;

	uint8_t *baseaddr = JitBlocks.Last();
	uint8_t *startaddr = p;
	uint8_t *endaddr = p + relocSize;
	uint8_t *unwindptr = p + unwindStart;
	memcpy(unwindptr, &unwindInfo[0], unwindInfoSize);

	if (unwindInfo.Size() > 0)
	{
		uint64_t *unwindfuncaddr = (uint64_t *)(unwindptr + fdeFunctionStart);
		unwindfuncaddr[0] = (ptrdiff_t)startaddr;
		unwindfuncaddr[1] = (ptrdiff_t)(endaddr - startaddr);

#ifdef __APPLE__
		// On macOS __register_frame takes a single FDE as an argument
		uint8_t *entry = unwindptr;
		while (true)
		{
			uint32_t length = *((uint32_t *)entry);
			if (length == 0)
				break;

			uint32_t offset = *((uint32_t *)(entry + 4));
			if (offset != 0)
			{
				__register_frame(entry);
				JitFrames.Push(entry);
			}
		}
#else
		// On Linux it takes a pointer to the entire .eh_frame
		__register_frame(unwindptr);
		JitFrames.Push(unwindptr);
#endif
	}

	return p;
}
#endif

void JitRelease()
{
#ifdef _WIN64
	for (auto p : JitFrames)
	{
		RtlDeleteFunctionTable((PRUNTIME_FUNCTION)p);
	}
#elif !defined(WIN32)
	for (auto p : JitFrames)
	{
		__deregister_frame(p);
	}
#endif
	for (auto p : JitBlocks)
	{
		asmjit::OSUtils::releaseVirtualMemory(p, 1024 * 1024);
	}
	JitFrames.Clear();
	JitBlocks.Clear();
	JitBlockPos = 0;
	JitBlockSize = 0;
}
