
#include <memory>
#include "jit.h"
#include "jitintern.h"

#ifdef WIN32
#include <DbgHelp.h>
#else
#include <execinfo.h>
#include <cxxabi.h>
#include <cstring>
#include <cstdlib>
#include <memory>
#endif

struct JitFuncInfo
{
	FString name;
	FString filename;
	TArray<JitLineInfo> LineInfo;
	void *start;
	void *end;
};

static TArray<JitFuncInfo> JitDebugInfo;
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
		codeInfo = rt.codeInfo();
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
		const size_t bytesToAllocate = asmjit::Support::alignUp(MAX(size_t(1024 * 1024), size), VirtMem::info().pageGranularity);
		void *p;
		if (VirtMem::alloc(&p, bytesToAllocate, VirtMem::kAccessReadWrite | VirtMem::kAccessExecute) != asmjit::kErrorOk)
			return nullptr;
		JitBlocks.Push((uint8_t*)p);
		JitBlockSize = bytesToAllocate;
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

static TArray<uint16_t> CreateUnwindInfoWindows(asmjit::FuncNode *func)
{
	using namespace asmjit;
	FuncFrame frame = func->frame();

	// We need a dummy assembler for instruction size calculations
	CodeHolder code;
	code.init(GetHostCodeInfo());
	x86::Assembler assembler(&code);

	// Build UNWIND_CODE codes:

	TArray<uint16_t> codes;
	uint32_t opoffset, opcode, opinfo;

	// Note: this must match exactly what X86Internal::emitProlog does

	x86::Gp zsp = assembler.zsp();   // ESP|RSP register.
	x86::Gp zbp = assembler.zsp();   // EBP|RBP register.
	zbp.setId(x86::Gp::kIdBp);
	x86::Gp gpReg = assembler.zsp(); // General purpose register (temporary).
	x86::Gp saReg = assembler.zsp(); // Stack-arguments base register.
	uint32_t gpSaved = frame.savedRegs(x86::Reg::kGroupGp);

	if (frame.hasPreservedFP())
	{
		// Emit: 'push zbp'
		//       'mov  zbp, zsp'.
		gpSaved &= ~Support::bitMask(x86::Gp::kIdBp);
		assembler.push(zbp);

		opoffset = (uint32_t)assembler.offset();
		opcode = UWOP_PUSH_NONVOL;
		opinfo = x86::Gp::kIdBp;
		codes.Push(opoffset | (opcode << 8) | (opinfo << 12));

		assembler.mov(zbp, zsp);
	}

	if (gpSaved)
	{
		for (uint32_t i = gpSaved, regId = 0; i; i >>= 1, regId++)
		{
			if (!(i & 0x1)) continue;
			// Emit: 'push gp' sequence.
			gpReg.setId(regId);
			assembler.push(gpReg);

			opoffset = (uint32_t)assembler.offset();
			opcode = UWOP_PUSH_NONVOL;
			opinfo = regId;
			codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
		}
	}

	uint32_t saRegId = frame.saRegId();
	if (saRegId != BaseReg::kIdBad && saRegId != x86::Gp::kIdSp)
	{
		saReg.setId(saRegId);
		if (!(frame.hasPreservedFP() && saRegId == x86::Gp::kIdBp))
		{
			// Emit: 'mov saReg, zsp'.
			assembler.mov(saReg, zsp);
		}
	}

	if (frame.hasDynamicAlignment())
	{
		// Emit: 'and zsp, StackAlignment'.
		assembler.and_(zsp, -static_cast<int32_t>(frame.finalStackAlignment()));
	}

	if (frame.hasStackAdjustment())
	{
		// Emit: 'sub zsp, StackAdjustment'.
		assembler.sub(zsp, frame.stackAdjustment());

		uint32_t stackadjust = frame.stackAdjustment();
		if (stackadjust <= 128)
		{
			opoffset = (uint32_t)assembler.offset();
			opcode = UWOP_ALLOC_SMALL;
			opinfo = stackadjust / 8 - 1;
			codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
		}
		else if (stackadjust <= 512 * 1024 - 8)
		{
			opoffset = (uint32_t)assembler.offset();
			opcode = UWOP_ALLOC_LARGE;
			opinfo = 0;
			codes.Push(stackadjust / 8);
			codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
		}
		else
		{
			opoffset = (uint32_t)assembler.offset();
			opcode = UWOP_ALLOC_LARGE;
			opinfo = 1;
			codes.Push((uint16_t)(stackadjust >> 16));
			codes.Push((uint16_t)stackadjust);
			codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
		}
	}

	if (frame.hasDynamicAlignment() && frame.hasDAOffset())
	{
		// Emit: 'mov [zsp + dsaSlot], saReg'.
		x86::Mem saMem = x86::ptr(zsp, int32_t(frame.daOffset()));
		assembler.mov(saMem, saReg);
	}

	uint32_t xmmSaved = frame.savedRegs(x86::Reg::kGroupVec);
	if (xmmSaved)
	{
		int vecOffset = frame.nonGpSaveOffset();
		x86::Mem vecBase = x86::ptr(zsp, vecOffset);
		x86::Reg vecReg = x86::xmm(0);
		bool avx = frame.isAvxEnabled();
		bool aligned = frame.hasAlignedVecSR();
		uint32_t vecInst = aligned ? (avx ? x86::Inst::kIdVmovaps : x86::Inst::kIdMovaps) : (avx ? x86::Inst::kIdVmovups : x86::Inst::kIdMovups);
		uint32_t vecSize = 16;
		for (uint32_t i = xmmSaved, regId = 0; i; i >>= 1, regId++)
		{
			if (!(i & 0x1)) continue;

			// Emit 'movaps|movups [zsp + X], xmm0..15'.
			vecReg.setId(regId);
			assembler.emit(vecInst, vecBase, vecReg);
			vecBase.addOffsetLo32(static_cast<int32_t>(vecSize));

			if (vecBase.offsetLo32() / vecSize < (1 << 16))
			{
				opoffset = (uint32_t)assembler.offset();
				opcode = UWOP_SAVE_XMM128;
				opinfo = regId;
				codes.Push(vecBase.offsetLo32() / vecSize);
				codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
			}
			else
			{
				opoffset = (uint32_t)assembler.offset();
				opcode = UWOP_SAVE_XMM128_FAR;
				opinfo = regId;
				codes.Push((uint16_t)(vecBase.offsetLo32() >> 16));
				codes.Push((uint16_t)vecBase.offsetLo32());
				codes.Push(opoffset | (opcode << 8) | (opinfo << 12));
			}
		}
	}

	// Build the UNWIND_INFO structure:

	uint16_t version = 1, flags = 0, frameRegister = 0, frameOffset = 0;
	uint16_t sizeOfProlog = (uint16_t)assembler.offset();
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

void *AddJitFunction(asmjit::CodeHolder* code, JitCompiler *compiler)
{
	using namespace asmjit;

	FuncNode *func = compiler->Codegen();
	if (code->flatten() != kErrorOk)
		return nullptr;
	if (code->resolveUnresolvedLinks() != kErrorOk)
		return nullptr;

	size_t estimatedCodeSize = Support::alignUp(code->codeSize(), 16);
	if (estimatedCodeSize == 0)
		return nullptr;

#ifdef _WIN64
	TArray<uint16_t> unwindInfo = CreateUnwindInfoWindows(func);
	size_t unwindInfoSize = unwindInfo.Size() * sizeof(uint16_t);
	size_t functionTableSize = sizeof(RUNTIME_FUNCTION);
#else
	size_t unwindInfoSize = 0;
	size_t functionTableSize = 0;
#endif

	uint8_t *p = (uint8_t *)AllocJitMemory(estimatedCodeSize + unwindInfoSize + functionTableSize);
	if (!p)
		return nullptr;

	if (code->relocateToBase((uintptr_t)p) != kErrorOk)
		return nullptr;

	size_t codeSize = code->codeSize();
	for (Section* section : code->sections())
		memcpy(p + size_t(section->offset()), section->data(), size_t(section->bufferSize()));

	size_t unwindStart = Support::alignUp(codeSize, 16);
	JitBlockPos -= codeSize - unwindStart;

#ifdef _WIN64
	uint8_t *baseaddr = JitBlocks.Last();
	uint8_t *startaddr = p;
	uint8_t *endaddr = p + codeSize;
	uint8_t *unwindptr = p + unwindStart;
	memcpy(unwindptr, &unwindInfo[0], unwindInfoSize);

	RUNTIME_FUNCTION *table = (RUNTIME_FUNCTION*)(unwindptr + unwindInfoSize);
	table[0].BeginAddress = (DWORD)(ptrdiff_t)(startaddr - baseaddr);
	table[0].EndAddress = (DWORD)(ptrdiff_t)(endaddr - baseaddr);
#ifndef __MINGW64__
	table[0].UnwindInfoAddress = (DWORD)(ptrdiff_t)(unwindptr - baseaddr);
#else
	table[0].UnwindData = (DWORD)(ptrdiff_t)(unwindptr - baseaddr);
#endif
	BOOLEAN result = RtlAddFunctionTable(table, 1, (DWORD64)baseaddr);
	JitFrames.Push((uint8_t*)table);
	if (result == 0)
		I_Error("RtlAddFunctionTable failed");

	JitDebugInfo.Push({ compiler->GetScriptFunction()->PrintableName, compiler->GetScriptFunction()->SourceFileName, compiler->LineInfo, startaddr, endaddr });
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
		for (int i = 0; i < padding; i++) WriteUInt8(stream, 0);
	}
}

static void WriteCIE(TArray<uint8_t> &stream, const TArray<uint8_t> &cieInstructions, uint8_t returnAddressReg)
{
	unsigned int lengthPos = stream.Size();
	WriteUInt32(stream, 0); // Length
	WriteUInt32(stream, 0); // CIE ID

	WriteUInt8(stream, 1); // CIE Version
	WriteUInt8(stream, 'z');
	WriteUInt8(stream, 'R'); // fde encoding
	WriteUInt8(stream, 0);
	WriteULEB128(stream, 1);
	WriteSLEB128(stream, -1);
	WriteULEB128(stream, returnAddressReg);

	WriteULEB128(stream, 1); // LEB128 augmentation size
	WriteUInt8(stream, 0); // DW_EH_PE_absptr (FDE uses absolute pointers)

	for (unsigned int i = 0; i < cieInstructions.Size(); i++)
		stream.Push(cieInstructions[i]);

	WritePadding(stream);
	WriteLength(stream, lengthPos, stream.Size() - lengthPos - 4);
}

static void WriteFDE(TArray<uint8_t> &stream, const TArray<uint8_t> &fdeInstructions, uint32_t cieLocation, unsigned int &functionStart)
{
	unsigned int lengthPos = stream.Size();
	WriteUInt32(stream, 0); // Length
	uint32_t offsetToCIE = stream.Size() - cieLocation;
	WriteUInt32(stream, offsetToCIE);

	functionStart = stream.Size();
	WriteUInt64(stream, 0); // func start
	WriteUInt64(stream, 0); // func size

	WriteULEB128(stream, 0); // LEB128 augmentation size

	for (unsigned int i = 0; i < fdeInstructions.Size(); i++)
		stream.Push(fdeInstructions[i]);

	WritePadding(stream);
	WriteLength(stream, lengthPos, stream.Size() - lengthPos - 4);
}

static void WriteAdvanceLoc(TArray<uint8_t> &fdeInstructions, uint64_t offset, uint64_t &lastOffset)
{
	uint64_t delta = offset - lastOffset;
	if (delta < (1 << 6))
	{
		WriteUInt8(fdeInstructions, (1 << 6) | delta); // DW_CFA_advance_loc
	}
	else if (delta < (1 << 8))
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

static void WriteDefineCFA(TArray<uint8_t> &cieInstructions, int dwarfRegId, int stackOffset)
{
	WriteUInt8(cieInstructions, 0x0c); // DW_CFA_def_cfa
	WriteULEB128(cieInstructions, dwarfRegId);
	WriteULEB128(cieInstructions, stackOffset);
}

static void WriteDefineStackOffset(TArray<uint8_t> &fdeInstructions, int stackOffset)
{
	WriteUInt8(fdeInstructions, 0x0e); // DW_CFA_def_cfa_offset
	WriteULEB128(fdeInstructions, stackOffset);
}

static void WriteRegisterStackLocation(TArray<uint8_t> &instructions, int dwarfRegId, int stackLocation)
{
	WriteUInt8(instructions, (2 << 6) | dwarfRegId); // DW_CFA_offset
	WriteULEB128(instructions, stackLocation);
}

static TArray<uint8_t> CreateUnwindInfoUnix(asmjit::FuncNode *func, unsigned int &functionStart)
{
	using namespace asmjit;

	// Build .eh_frame:
	//
	// The documentation for this can be found in the DWARF standard
	// The x64 specific details are described in "System V Application Binary Interface AMD64 Architecture Processor Supplement"
	//
	// See appendix D.6 "Call Frame Information Example" in the DWARF 5 spec.
	//
	// The CFI_Parser<A>::decodeFDE parser on the other side..
	// https://github.com/llvm-mirror/libunwind/blob/master/src/DwarfParser.hpp

	// Asmjit -> DWARF register id
	int dwarfRegId[16];
	dwarfRegId[x86::Gp::kIdAx] = 0;
	dwarfRegId[x86::Gp::kIdDx] = 1;
	dwarfRegId[x86::Gp::kIdCx] = 2;
	dwarfRegId[x86::Gp::kIdBx] = 3;
	dwarfRegId[x86::Gp::kIdSi] = 4;
	dwarfRegId[x86::Gp::kIdDi] = 5;
	dwarfRegId[x86::Gp::kIdBp] = 6;
	dwarfRegId[x86::Gp::kIdSp] = 7;
	dwarfRegId[x86::Gp::kIdR8] = 8;
	dwarfRegId[x86::Gp::kIdR9] = 9;
	dwarfRegId[x86::Gp::kIdR10] = 10;
	dwarfRegId[x86::Gp::kIdR11] = 11;
	dwarfRegId[x86::Gp::kIdR12] = 12;
	dwarfRegId[x86::Gp::kIdR13] = 13;
	dwarfRegId[x86::Gp::kIdR14] = 14;
	dwarfRegId[x86::Gp::kIdR15] = 15;
	int dwarfRegRAId = 16;
	int dwarfRegXmmId = 17;

	TArray<uint8_t> cieInstructions;
	TArray<uint8_t> fdeInstructions;

	uint8_t returnAddressReg = dwarfRegRAId;
	int stackOffset = 8; // Offset from RSP to the Canonical Frame Address (CFA) - stack position where the CALL return address is stored

	WriteDefineCFA(cieInstructions, dwarfRegId[x86::Gp::kIdSp], stackOffset);
	WriteRegisterStackLocation(cieInstructions, returnAddressReg, stackOffset);

	FuncFrame frame = func->frame();

	// We need a dummy assembler for instruction size calculations
	CodeHolder code;
	code.init(GetHostCodeInfo());
	x86::Assembler assembler(&code);
	uint64_t lastOffset = 0;

	// Note: the following code must match exactly what X86Internal::emitProlog does

	x86::Gp zsp = assembler.zsp();   // ESP|RSP register.
	x86::Gp zbp = assembler.zsp();   // EBP|RBP register.
	zbp.setId(x86::Gp::kIdBp);
	x86::Gp gpReg = assembler.zsp(); // General purpose register (temporary).
	x86::Gp saReg = assembler.zsp(); // Stack-arguments base register.
	uint32_t gpSaved = frame.savedRegs(x86::Reg::kGroupGp);

	if (frame.hasPreservedFP())
	{
		// Emit: 'push zbp'
		//       'mov  zbp, zsp'.
		gpSaved &= ~Support::bitMask(x86::Gp::kIdBp);
		assembler.push(zbp);

		stackOffset += 8;
		WriteAdvanceLoc(fdeInstructions, assembler.offset(), lastOffset);
		WriteDefineStackOffset(fdeInstructions, stackOffset);
		WriteRegisterStackLocation(fdeInstructions, dwarfRegId[x86::Gp::kIdBp], stackOffset);

		assembler.mov(zbp, zsp);
	}

	if (gpSaved)
	{
		for (uint32_t i = gpSaved, regId = 0; i; i >>= 1, regId++)
		{
			if (!(i & 0x1)) continue;
			// Emit: 'push gp' sequence.
			gpReg.setId(regId);
			assembler.push(gpReg);

			stackOffset += 8;
			WriteAdvanceLoc(fdeInstructions, assembler.offset(), lastOffset);
			WriteDefineStackOffset(fdeInstructions, stackOffset);
			WriteRegisterStackLocation(fdeInstructions, dwarfRegId[regId], stackOffset);
		}
	}

	uint32_t saRegId = frame.saRegId();
	if (saRegId != BaseReg::kIdBad && saRegId != x86::Gp::kIdSp)
	{
		saReg.setId(saRegId);
		if (frame.hasPreservedFP()) {
			if (saRegId != x86::Gp::kIdBp)
				assembler.mov(saReg, zbp);
		}
		else {
			assembler.mov(saReg, zsp);
		}
	}

	if (frame.hasDynamicAlignment())
	{
		// Emit: 'and zsp, StackAlignment'.
		assembler.and_(zsp, -static_cast<int32_t>(frame.finalStackAlignment()));
	}

	if (frame.hasStackAdjustment())
	{
		// Emit: 'sub zsp, StackAdjustment'.
		assembler.sub(zsp, frame.stackAdjustment());

		stackOffset += frame.stackAdjustment();
		WriteAdvanceLoc(fdeInstructions, assembler.offset(), lastOffset);
		WriteDefineStackOffset(fdeInstructions, stackOffset);
	}

	if (frame.hasDynamicAlignment() && frame.hasDAOffset())
	{
		// Emit: 'mov [zsp + dsaSlot], saReg'.
		x86::Mem saMem = x86::ptr(zsp, int32_t(frame.daOffset()));
		assembler.mov(saMem, saReg);
	}

	uint32_t xmmSaved = frame.savedRegs(x86::Reg::kGroupVec);
	if (xmmSaved)
	{
		int vecOffset = frame.nonGpSaveOffset();
		x86::Mem vecBase = x86::ptr(zsp, vecOffset);
		x86::Reg vecReg = x86::xmm(0);
		bool avx = frame.isAvxEnabled();
		bool aligned = frame.hasAlignedVecSR();
		uint32_t vecInst = aligned ? (avx ? x86::Inst::kIdVmovaps : x86::Inst::kIdMovaps) : (avx ? x86::Inst::kIdVmovups : x86::Inst::kIdMovups);
		uint32_t vecSize = 16;
		for (uint32_t i = xmmSaved, regId = 0; i; i >>= 1, regId++)
		{
			if (!(i & 0x1)) continue;

			// Emit 'movaps|movups [zsp + X], xmm0..15'.
			vecReg.setId(regId);
			assembler.emit(vecInst, vecBase, vecReg);
			vecBase.addOffsetLo32(static_cast<int32_t>(vecSize));

			WriteAdvanceLoc(fdeInstructions, assembler.offset(), lastOffset);
			WriteRegisterStackLocation(fdeInstructions, dwarfRegXmmId + regId, stackOffset - vecOffset);
			vecOffset += static_cast<int32_t>(vecSize);
		}
	}

	TArray<uint8_t> stream;
	WriteCIE(stream, cieInstructions, returnAddressReg);
	WriteFDE(stream, fdeInstructions, 0, functionStart);
	WriteUInt32(stream, 0);
	return stream;
}

void *AddJitFunction(asmjit::CodeHolder* code, JitCompiler *compiler)
{
	using namespace asmjit;

	FuncNode *func = compiler->Codegen();
	if (code->flatten() != kErrorOk)
		return nullptr;
	if (code->resolveUnresolvedLinks() != kErrorOk)
		return nullptr;

	size_t estimatedCodeSize = Support::alignUp(code->codeSize(), 16);
	if (estimatedCodeSize == 0)
		return nullptr;

	unsigned int fdeFunctionStart = 0;
	TArray<uint8_t> unwindInfo = CreateUnwindInfoUnix(func, fdeFunctionStart);
	size_t unwindInfoSize = unwindInfo.Size();

	uint8_t *p = (uint8_t *)AllocJitMemory(estimatedCodeSize + unwindInfoSize);
	if (!p)
		return nullptr;

	if (code->relocateToBase((uintptr_t)p) != kErrorOk)
		return nullptr;

	size_t codeSize = code->codeSize();
	for (Section* section : code->sections())
		memcpy(p + size_t(section->offset()), section->data(), size_t(section->bufferSize()));

	size_t unwindStart = Support::alignUp(codeSize, 16);
	JitBlockPos -= codeSize - unwindStart;

	uint8_t *baseaddr = JitBlocks.Last();
	uint8_t *startaddr = p;
	uint8_t *endaddr = p + codeSize;
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

	JitDebugInfo.Push({ compiler->GetScriptFunction()->PrintableName, compiler->GetScriptFunction()->SourceFileName, compiler->LineInfo, startaddr, endaddr });

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
		asmjit::VirtMem::release(p, 1024 * 1024);
	}
	JitDebugInfo.Clear();
	JitFrames.Clear();
	JitBlocks.Clear();
	JitBlockPos = 0;
	JitBlockSize = 0;
}

static int CaptureStackTrace(int max_frames, void **out_frames)
{
	memset(out_frames, 0, sizeof(void *) * max_frames);

#ifdef _WIN64
	// RtlCaptureStackBackTrace doesn't support RtlAddFunctionTable..

	CONTEXT context;
	RtlCaptureContext(&context);

	UNWIND_HISTORY_TABLE history;
	memset(&history, 0, sizeof(UNWIND_HISTORY_TABLE));

	ULONG64 establisherframe = 0;
	PVOID handlerdata = nullptr;

	int frame;
	for (frame = 0; frame < max_frames; frame++)
	{
		ULONG64 imagebase;
		PRUNTIME_FUNCTION rtfunc = RtlLookupFunctionEntry(context.Rip, &imagebase, &history);

		KNONVOLATILE_CONTEXT_POINTERS nvcontext;
		memset(&nvcontext, 0, sizeof(KNONVOLATILE_CONTEXT_POINTERS));
		if (!rtfunc)
		{
			// Leaf function
			context.Rip = (ULONG64)(*(PULONG64)context.Rsp);
			context.Rsp += 8;
		}
		else
		{
			RtlVirtualUnwind(UNW_FLAG_NHANDLER, imagebase, context.Rip, rtfunc, &context, &handlerdata, &establisherframe, &nvcontext);
		}

		if (!context.Rip)
			break;

		out_frames[frame] = (void*)context.Rip;
	}
	return frame;

#elif defined(WIN32)
	// JIT isn't supported here, so just do nothing.
	return 0;//return RtlCaptureStackBackTrace(0, MIN(max_frames, 32), out_frames, nullptr);
#else
	return backtrace(out_frames, max_frames);
#endif
}

#ifdef WIN32
class NativeSymbolResolver
{
public:
	NativeSymbolResolver() { SymInitialize(GetCurrentProcess(), nullptr, TRUE); }
	~NativeSymbolResolver() { SymCleanup(GetCurrentProcess()); }

	FString GetName(void *frame)
	{
		FString s;

		unsigned char buffer[sizeof(IMAGEHLP_SYMBOL64) + 128];
		IMAGEHLP_SYMBOL64 *symbol64 = reinterpret_cast<IMAGEHLP_SYMBOL64*>(buffer);
		memset(symbol64, 0, sizeof(IMAGEHLP_SYMBOL64) + 128);
		symbol64->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
		symbol64->MaxNameLength = 128;

		DWORD64 displacement = 0;
		BOOL result = SymGetSymFromAddr64(GetCurrentProcess(), (DWORD64)frame, &displacement, symbol64);
		if (result)
		{
			IMAGEHLP_LINE64 line64;
			DWORD displacement = 0;
			memset(&line64, 0, sizeof(IMAGEHLP_LINE64));
			line64.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			result = SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)frame, &displacement, &line64);
			if (result)
			{
				s.Format("Called from %s at %s, line %d\n", symbol64->Name, line64.FileName, (int)line64.LineNumber);
			}
			else
			{
				s.Format("Called from %s\n", symbol64->Name);
			}
		}

		return s;
	}
};
#else
class NativeSymbolResolver
{
public:
	FString GetName(void *frame)
	{
		FString s;
		char **strings;
		void *frames[1] = { frame };
		strings = backtrace_symbols(frames, 1);

		// Decode the strings
		char *ptr = strings[0];
		char *filename = ptr;
		const char *function = "";

		// Find function name
		while (*ptr)
		{
			if (*ptr == '(')	// Found function name
			{
				*(ptr++) = 0;
				function = ptr;
				break;
			}
			ptr++;
		}

		// Find offset
		if (function[0])	// Only if function was found
		{
			while (*ptr)
			{
				if (*ptr == '+')	// Found function offset
				{
					*(ptr++) = 0;
					break;
				}
				if (*ptr == ')')	// Not found function offset, but found, end of function
				{
					*(ptr++) = 0;
					break;
				}
				ptr++;
			}
		}

		int status;
		char *new_function = abi::__cxa_demangle(function, nullptr, nullptr, &status);
		if (new_function)	// Was correctly decoded
		{
			function = new_function;
		}

		s.Format("Called from %s at %s\n", function, filename);

		if (new_function)
		{
			free(new_function);
		}

		free(strings);
		return s;
	}
};
#endif

int JITPCToLine(uint8_t *pc, const JitFuncInfo *info)
{
	int PCIndex = int(pc - ((uint8_t *) (info->start)));
	if (info->LineInfo.Size () == 1) return info->LineInfo[0].LineNumber;
	for (unsigned i = 1; i < info->LineInfo.Size (); i++)
	{
		if (info->LineInfo[i].InstructionIndex >= PCIndex)
		{
			return info->LineInfo[i - 1].LineNumber;
		}
	}
	return -1;
}

FString JitGetStackFrameName(NativeSymbolResolver *nativeSymbols, void *pc)
{
	for (unsigned int i = 0; i < JitDebugInfo.Size(); i++)
	{
		const auto &info = JitDebugInfo[i];
		if (pc >= info.start && pc < info.end)
		{
			int line = JITPCToLine ((uint8_t *)pc, &info);

			FString s;

			if (line == -1)
				s.Format("Called from %s at %s\n", info.name.GetChars(), info.filename.GetChars());
			else
				s.Format("Called from %s at %s, line %d\n", info.name.GetChars(), info.filename.GetChars(), line);

			return s;
		}
	}

	return nativeSymbols ? nativeSymbols->GetName(pc) : FString();
}

FString JitCaptureStackTrace(int framesToSkip, bool includeNativeFrames)
{
	void *frames[32];
	int numframes = CaptureStackTrace(32, frames);

	std::unique_ptr<NativeSymbolResolver> nativeSymbols;
	if (includeNativeFrames)
		nativeSymbols.reset(new NativeSymbolResolver());

	FString s;
	for (int i = framesToSkip + 1; i < numframes; i++)
	{
		s += JitGetStackFrameName(nativeSymbols.get(), frames[i]);
	}
	return s;
}
