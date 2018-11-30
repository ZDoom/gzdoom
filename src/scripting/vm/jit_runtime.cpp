
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

static void WriteLength64(TArray<uint8_t> &stream, unsigned int pos, unsigned int v)
{
	*(uint64_t*)(&stream[pos]) = v;
}

static void WriteLength(TArray<uint8_t> &stream, unsigned int pos, unsigned int v)
{
	*(uint32_t*)(&stream[pos]) = v;
}

static void WriteUInt64(TArray<uint8_t> &stream, uint64_t v)
{
	for (int i = 0; i < 8; i++)
		stream.Push((v >> (i * 8)) & 0xff);
}

static void WriteUInt32(TArray<uint8_t> &stream, uint32_t v)
{
	for (int i = 0; i < 4; i++)
		stream.Push((v >> (i * 8)) & 0xff);
}

static void WriteUInt16(TArray<uint8_t> &stream, uint16_t v)
{
	for (int i = 0; i < 2; i++)
		stream.Push((v >> (i * 8)) & 0xff);
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
		while (true)
		{
			if (v > -128)
			{
				WriteUInt8(stream, v & 0x7f);
				break;
			}
			else
			{
				WriteUInt8(stream, v);
				v >>= 7;
			}
		}
	}
}

static void WritePadding(TArray<uint8_t> &stream)
{
	int padding = stream.Size() % 8;
	if (padding != 0)
	{
		padding = 8 - padding;
		for (int i = 0; i <= padding; i++) WriteUInt8(stream, 0);
	}
}

static void WriteEmptyAugmentation(TArray<uint8_t> &stream)
{
	int padding = (stream.Size() + 1) % 8;
	if (padding == 0)
	{
		WriteULEB128(stream, 0);
	}
	else
	{
		padding = 8 - padding;
		WriteULEB128(stream, padding);
		for (int i = 0; i <= padding; i++) WriteUInt8(stream, 0);
	}
}

static void WriteCIE(TArray<uint8_t> &stream, const TArray<uint8_t> &cieInstructions, uint8_t returnAddressReg)
{
#ifdef USE_DWARF64
	WriteUInt32(stream, 0xffffffff); // this is a 64-bit entry
	unsigned int lengthPos = stream.Size();
	WriteUInt64(stream, 0); // Length
	WriteUInt64(stream, 0); // CIE ID
#else
	unsigned int lengthPos = stream.Size();
	WriteUInt32(stream, 0); // Length
	WriteUInt32(stream, 0); // CIE ID
#endif
	
	WriteUInt8(stream, 1); // CIE Version
	WriteUInt8(stream, 'z');
	//WriteUInt8(stream, 'R'); // fde encoding
	WriteUInt8(stream, 0);
	WriteULEB128(stream, 1);
	WriteSLEB128(stream, -4);
	WriteUInt8(stream, returnAddressReg);

	WriteEmptyAugmentation(stream);

	for (unsigned int i = 0; i < cieInstructions.Size(); i++)
		stream.Push(cieInstructions[i]);

	WritePadding(stream);
#ifdef USE_DWARF64
	WriteLength64(stream, lengthPos, stream.Size() - lengthPos - 8);
#else
	WriteLength(stream, lengthPos, stream.Size() - lengthPos - 4);
#endif
}

static void WriteFDE(TArray<uint8_t> &stream, const TArray<uint8_t> &fdeInstructions, uint32_t cieLocation, unsigned int &functionStart)
{
#ifdef USE_DWARF64
	WriteUInt32(stream, 0xffffffff); // this is a 64-bit entry
	unsigned int lengthPos = stream.Size();
	WriteUInt64(stream, 0); // Length
	uint32_t offsetToCIE = stream.Size() - cieLocation;
	WriteUInt64(stream, offsetToCIE);
#else
	unsigned int lengthPos = stream.Size();
	WriteUInt32(stream, 0); // Length
	uint32_t offsetToCIE = stream.Size() - cieLocation;
	WriteUInt32(stream, offsetToCIE);
#endif
	
	functionStart = stream.Size();
	WriteUInt64(stream, 0); // func start
	WriteUInt64(stream, 0); // func size
	
	WriteEmptyAugmentation(stream);

	for (unsigned int i = 0; i < fdeInstructions.Size(); i++)
		stream.Push(fdeInstructions[i]);

	WritePadding(stream);
#ifdef USE_DWARF64
	WriteLength64(stream, lengthPos, stream.Size() - lengthPos - 8);
#else
	WriteLength(stream, lengthPos, stream.Size() - lengthPos - 4);
#endif
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

	// Build .eh_frame:
	//
	// The documentation for this can be found in the DWARF standard
	// The x64 specific details are described in "System V Application Binary Interface AMD64 Architecture Processor Supplement"
	//
	// See appendix D.6 "Call Frame Information Example" in the DWARF 5 spec.
	
	// Asmjit -> DWARF register id
	int dwarfRegId[16];
	dwarfRegId[X86Gp::kIdAx] = 0;
	dwarfRegId[X86Gp::kIdDx] = 1;
	dwarfRegId[X86Gp::kIdCx] = 2;
	dwarfRegId[X86Gp::kIdBx] = 3;
	dwarfRegId[X86Gp::kIdSi] = 4;
	dwarfRegId[X86Gp::kIdDi] = 5;
	dwarfRegId[X86Gp::kIdBp] = 6;
	dwarfRegId[X86Gp::kIdSp] = 7;
	dwarfRegId[X86Gp::kIdR8] = 8;
	dwarfRegId[X86Gp::kIdR9] = 9;
	dwarfRegId[X86Gp::kIdR10] = 10;
	dwarfRegId[X86Gp::kIdR11] = 11;
	dwarfRegId[X86Gp::kIdR12] = 12;
	dwarfRegId[X86Gp::kIdR13] = 13;
	dwarfRegId[X86Gp::kIdR14] = 14;
	dwarfRegId[X86Gp::kIdR15] = 15;
	int dwarfRegRAId = 16;
	int dwarfRegXmmId = 17;
	
	TArray<uint8_t> cieInstructions;
	TArray<uint8_t> fdeInstructions;
	uint64_t lastOffset = 0;

	uint8_t returnAddressReg = dwarfRegRAId;

	// Do we need to write register defaults into the CIE or does the defaults match the x64 calling convention?
	// Great! the "System V Application Binary Interface AMD64 Architecture Processor Supplement" doesn't say what the defaults are..
	// This is basically just the x64 calling convention..
	WriteUInt8(cieInstructions, 0x0c); // DW_CFA_def_cfa
	WriteULEB128(cieInstructions, dwarfRegId[X86Gp::kIdSp]);
	WriteULEB128(cieInstructions, 0);
	for (auto regId : { X86Gp::kIdAx, X86Gp::kIdDx, X86Gp::kIdCx, X86Gp::kIdSi, X86Gp::kIdDi, X86Gp::kIdSp, X86Gp::kIdR8, X86Gp::kIdR9, X86Gp::kIdR10, X86Gp::kIdR11 })
	{
		WriteUInt8(cieInstructions, 0x07); // DW_CFA_undefined
		WriteULEB128(cieInstructions, dwarfRegId[regId]);
	}
	for (auto regId : { X86Gp::kIdBx, X86Gp::kIdBp, X86Gp::kIdR12, X86Gp::kIdR13, X86Gp::kIdR14, X86Gp::kIdR15 })
	{
		WriteUInt8(cieInstructions, 0x08); // DW_CFA_same_value
		WriteULEB128(cieInstructions, dwarfRegId[regId]);
	}

	FuncFrameLayout layout;
	Error error = layout.init(func->getDetail(), func->getFrameInfo());
	if (error != kErrorOk)
		I_FatalError("FuncFrameLayout.init failed");

	// We need a dummy emitter for instruction size calculations
	CodeHolder code;
	code.init(GetHostCodeInfo());
	X86Assembler assembler(&code);
	X86Emitter *emitter = assembler.asEmitter();

	// Note: the following code must match exactly what X86Internal::emitProlog does

	X86Gp zsp = emitter->zsp();   // ESP|RSP register.
	X86Gp zbp = emitter->zsp();   // EBP|RBP register.
	zbp.setId(X86Gp::kIdBp);
	X86Gp gpReg = emitter->zsp(); // General purpose register (temporary).
	X86Gp saReg = emitter->zsp(); // Stack-arguments base register.
	uint32_t gpSaved = layout.getSavedRegs(X86Reg::kKindGp);

	int stackOffset = 0;

	if (layout.hasPreservedFP())
	{
		// Emit: 'push zbp'
		//       'mov  zbp, zsp'.
		gpSaved &= ~Utils::mask(X86Gp::kIdBp);
		emitter->push(zbp);

		WriteAdvanceLoc(fdeInstructions, assembler.getOffset(), lastOffset);
		stackOffset += 8;
		WriteUInt8(fdeInstructions, 0x0e); // DW_CFA_def_cfa_offset
		WriteULEB128(fdeInstructions, stackOffset);
		WriteUInt8(fdeInstructions, (2 << 6) | dwarfRegId[X86Gp::kIdBp]); // DW_CFA_offset
		WriteULEB128(fdeInstructions, stackOffset - 8);

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
			stackOffset += 8;
			WriteUInt8(fdeInstructions, 0x0e); // DW_CFA_def_cfa_offset
			WriteULEB128(fdeInstructions, stackOffset);
			WriteUInt8(fdeInstructions, (2 << 6) | dwarfRegId[regId]); // DW_CFA_offset
			WriteULEB128(fdeInstructions, stackOffset - 8);
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

		WriteAdvanceLoc(fdeInstructions, assembler.getOffset(), lastOffset);
		stackOffset += layout.getStackAdjustment();
		WriteUInt8(fdeInstructions, 0x0e); // DW_CFA_def_cfa_offset
		WriteULEB128(fdeInstructions, stackOffset);
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
		stackOffset += layout.getVecStackOffset();
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
			WriteUInt8(fdeInstructions, (2 << 6) | (dwarfRegXmmId + regId)); // DW_CFA_offset
			WriteULEB128(fdeInstructions, stackOffset);
			stackOffset += 8;
		}
	}

	TArray<uint8_t> stream;
	WriteCIE(stream, cieInstructions, returnAddressReg);
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
	TArray<uint8_t> unwindInfo = CreateUnwindInfoUnix(func, fdeFunctionStart);
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

			if (length == 0xffffffff)
			{
				uint64_t length64 = *((uint64_t *)(entry + 4));
				if (length64 == 0)
					break;
				
				uint64_t offset = *((uint64_t *)(entry + 12));
				if (offset != 0)
				{
					__register_frame(entry);
					JitFrames.Push(entry);
				}
				entry += length64 + 12;
			}
			else
			{
				uint32_t offset = *((uint32_t *)(entry + 4));
				if (offset != 0)
				{
					__register_frame(entry);
					JitFrames.Push(entry);
				}
				entry += length + 4;
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
