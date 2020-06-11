
#include "jit.h"
#include "jitintern.h"
#include "printf.h"
#include "v_video.h"
#include "s_soundinternal.h"
#include "texturemanager.h"
#include "palutil.h"

extern PString *TypeString;
extern PStruct *TypeVector2;
extern PStruct *TypeVector3;

JITRuntime* GetJITRuntime();

JitFuncPtr JitCompile(VMScriptFunction* sfunc)
{
#if 0
	if (strcmp(sfunc->PrintableName.GetChars(), "ListMenu.MenuEvent") != 0)
		return nullptr;
#endif

	try
	{
		JITRuntime* jit = GetJITRuntime();
		IRContext context;
		JitCompiler compiler(&context, sfunc);
		IRFunction* func = compiler.Codegen();
		jit->add(&context);
		//std::string text = context.getFunctionAssembly(func);
		return reinterpret_cast<JitFuncPtr>(jit->getPointerToFunction(func->name));
	}
	catch (...)
	{
		Printf("%s: Unexpected JIT error encountered\n", sfunc->PrintableName.GetChars());
		throw;
	}
}

void JitDumpLog(FILE* file, VMScriptFunction* sfunc)
{
	try
	{
		IRContext context;
		JitCompiler compiler(&context, sfunc);
		IRFunction* func = compiler.Codegen();
		std::string text = context.getFunctionAssembly(func);
		fwrite(text.data(), text.size(), 1, file);
	}
	catch (const std::exception& e)
	{
		FString err;
		err.Format("Unexpected JIT error: %s\n", e.what());
		fwrite(err.GetChars(), err.Len(), 1, file);
		fclose(file);

		I_FatalError("Unexpected JIT error: %s\n", e.what());
	}
}

/*
static void OutputJitLog(const char *text)
{
	// Write line by line since I_FatalError seems to cut off long strings
	const char *pos = text;
	const char *end = pos;
	while (*end)
	{
		if (*end == '\n')
		{
			FString substr(pos, (int)(ptrdiff_t)(end - pos));
			Printf("%s\n", substr.GetChars());
			pos = end + 1;
		}
		end++;
	}
	if (pos != end)
		Printf("%s\n", pos);
}
*/

/////////////////////////////////////////////////////////////////////////////

static const char *OpNames[NUM_OPS] =
{
#define xx(op, name, mode, alt, kreg, ktype)	#op,
#include "vmops.h"
#undef xx
};

IRFunction* JitCompiler::Codegen()
{
	Setup();

	int lastLine = -1;

	pc = sfunc->Code;
	auto end = pc + sfunc->CodeSize;
	while (pc != end)
	{
		int i = (int)(ptrdiff_t)(pc - sfunc->Code);
		op = pc->op;

		if (labels[i].block) // This is already a known jump target
		{
			if (cc.GetInsertBlock())
				cc.CreateBr(labels[i].block);
			cc.SetInsertPoint(labels[i].block);
		}
		else // Save start location in case GetLabel gets called later
		{
			if (!cc.GetInsertBlock())
				cc.SetInsertPoint(irfunc->createBasicBlock({}));
			labels[i].block = cc.GetInsertBlock();
		}

		labels[i].index = labels[i].block->code.size();

		int curLine = sfunc->PCToLine(pc);

		FString lineinfo;
		if (op != OP_PARAM && op != OP_PARAMI && op != OP_VTBL)
		{
			lineinfo.Format("line %d: %02x%02x%02x%02x %s", curLine, pc->op, pc->a, pc->b, pc->c, OpNames[op]);
		}

		EmitOpcode();

		// Add line info to first instruction emitted for the opcode
		if (labels[i].block->code.size() != labels[i].index)
		{
			IRInst* inst = labels[i].block->code[labels[i].index];
			inst->fileIndex = 0;
			inst->lineNumber = curLine;
			if (inst->comment.empty())
				inst->comment = lineinfo.GetChars();
			else
				inst->comment = lineinfo.GetChars() + ("; " + inst->comment);
		}

		pc++;
	}

	return irfunc;
}

void JitCompiler::EmitOpcode()
{
	switch (op)
	{
		#define xx(op, name, mode, alt, kreg, ktype)	case OP_##op: Emit##op(); break;
		#include "vmops.h"
		#undef xx

	default:
		I_FatalError("JIT error: Unknown VM opcode %d\n", op);
		break;
	}
}

void JitCompiler::CheckVMFrame()
{
	if (!vmframe)
	{
		vmframe = irfunc->createAlloca(int8Ty, ircontext->getConstantInt(sfunc->StackSize), "vmframe");
	}
}

IRValue* JitCompiler::GetCallReturns()
{
	if (!callReturns)
	{
		callReturns = irfunc->createAlloca(int8Ty, ircontext->getConstantInt(sizeof(VMReturn) * MAX_RETURNS), "callReturns");
	}
	return callReturns;
}

IRBasicBlock* JitCompiler::GetLabel(size_t pos)
{
	if (labels[pos].index != 0) // Jump targets can only point at the start of a basic block
	{
		IRBasicBlock* curbb = labels[pos].block;
		size_t splitpos = labels[pos].index;

		// Split basic block
		IRBasicBlock* newlabelbb = irfunc->createBasicBlock({});
		auto itbegin = curbb->code.begin() + splitpos;
		auto itend = curbb->code.end();
		newlabelbb->code.insert(newlabelbb->code.begin(), itbegin, itend);
		curbb->code.erase(itbegin, itend);

		// Jump from prev block to next
		IRBasicBlock* old = cc.GetInsertBlock();
		cc.SetInsertPoint(curbb);
		cc.CreateBr(newlabelbb);
		cc.SetInsertPoint(old);

		// Update label
		labels[pos].block = newlabelbb;
		labels[pos].index = 0;

		// Update other label references
		for (size_t i = 0; i < labels.Size(); i++)
		{
			if (labels[i].block == curbb && labels[i].index >= splitpos)
			{
				labels[i].block = newlabelbb;
				labels[i].index -= splitpos;
			}
		}
	}
	else if (!labels[pos].block)
	{
		labels[pos].block = irfunc->createBasicBlock({});
	}

	return labels[pos].block;
}

void JitCompiler::Setup()
{
	GetTypes();
	CreateNativeFunctions();

	//static const char *marks = "=======================================================";
	//cc.comment("", 0);
	//cc.comment(marks, 56);

	//FString funcname;
	//funcname.Format("Function: %s", sfunc->PrintableName.GetChars());
	//cc.comment(funcname.GetChars(), funcname.Len());

	//cc.comment(marks, 56);
	//cc.comment("", 0);

	IRFunctionType* functype = ircontext->getFunctionType(int32Ty, { int8PtrTy, int8PtrTy, int32Ty, int8PtrTy, int32Ty });
	irfunc = ircontext->createFunction(functype, sfunc->PrintableName.GetChars());
	irfunc->fileInfo.push_back({ sfunc->PrintableName.GetChars(), sfunc->SourceFileName.GetChars() });

	args = irfunc->args[1];
	numargs = irfunc->args[2];
	ret = irfunc->args[3];
	numret = irfunc->args[4];

	konstd = sfunc->KonstD;
	konstf = sfunc->KonstF;
	konsts = sfunc->KonstS;
	konsta = sfunc->KonstA;

	labels.Resize(sfunc->CodeSize);

	cc.SetInsertPoint(irfunc->createBasicBlock("entry"));

	CreateRegisters();
	IncrementVMCalls();
	SetupFrame();
}

void JitCompiler::SetupFrame()
{
	// the VM version reads this from the stack, but it is constant data
	offsetParams = ((int)sizeof(VMFrame) + 15) & ~15;
	offsetF = offsetParams + (int)(sfunc->MaxParam * sizeof(VMValue));
	offsetS = offsetF + (int)(sfunc->NumRegF * sizeof(double));
	offsetA = offsetS + (int)(sfunc->NumRegS * sizeof(FString));
	offsetD = offsetA + (int)(sfunc->NumRegA * sizeof(void*));
	offsetExtra = (offsetD + (int)(sfunc->NumRegD * sizeof(int32_t)) + 15) & ~15;

	/*if (sfunc->SpecialInits.Size() == 0 && sfunc->NumRegS == 0 && sfunc->ExtraSpace == 0)
	{
		SetupSimpleFrame();
	}
	else*/
	{
		SetupFullVMFrame();
	}
}

void JitCompiler::SetupSimpleFrame()
{
	// This is a simple frame with no constructors or destructors. Allocate it on the stack ourselves.

	int argsPos = 0;
	int regd = 0, regf = 0, rega = 0;
	for (unsigned int i = 0; i < sfunc->Proto->ArgumentTypes.Size(); i++)
	{
		const PType *type = sfunc->Proto->ArgumentTypes[i];
		if (sfunc->ArgFlags.Size() && sfunc->ArgFlags[i] & (VARF_Out | VARF_Ref))
		{
			StoreA(Load(ToInt8PtrPtr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, a))), rega++);
		}
		else if (type == TypeVector2)
		{
			StoreF(Load(ToDoublePtr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f))), regf++);
			StoreF(Load(ToDoublePtr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f))), regf++);
		}
		else if (type == TypeVector3)
		{
			StoreF(Load(ToDoublePtr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f))), regf++);
			StoreF(Load(ToDoublePtr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f))), regf++);
			StoreF(Load(ToDoublePtr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f))), regf++);
		}
		else if (type == TypeFloat64)
		{
			StoreF(Load(ToDoublePtr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, f))), regf++);
		}
		else if (type == TypeString)
		{
			I_FatalError("JIT: Strings are not supported yet for simple frames");
		}
		else if (type->isIntCompatible())
		{
			StoreD(Load(ToInt32Ptr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, i))), regd++);
		}
		else
		{
			StoreA(Load(ToInt8PtrPtr(args, argsPos++ * sizeof(VMValue) + offsetof(VMValue, a))), rega++);
		}
	}

	if (sfunc->NumArgs != argsPos || regd > sfunc->NumRegD || regf > sfunc->NumRegF || rega > sfunc->NumRegA)
		I_FatalError("JIT: sfunc->NumArgs != argsPos || regd > sfunc->NumRegD || regf > sfunc->NumRegF || rega > sfunc->NumRegA");

	for (int i = regd; i < sfunc->NumRegD; i++)
		StoreD(ConstValueD(0), i);

	for (int i = regf; i < sfunc->NumRegF; i++)
		StoreF(ConstValueF(0.0), i);

	for (int i = rega; i < sfunc->NumRegA; i++)
		StoreA(ConstValueA(nullptr), i);
}

void JitCompiler::SetupFullVMFrame()
{
	vmframestack = cc.CreateCall(createFullVMFrame, { ConstValueA(sfunc), args, numargs });

	IRValue* Blocks = Load(ToInt8PtrPtr(vmframestack)); // vmframestack->Blocks
	vmframe = Load(ToInt8PtrPtr(Blocks, VMFrameStack::OffsetLastFrame())); // Blocks->LastFrame

	for (int i = 0; i < sfunc->NumRegD; i++)
		StoreD(Load(ToInt32Ptr(vmframe, offsetD + i * sizeof(int32_t))), i);

	for (int i = 0; i < sfunc->NumRegF; i++)
		StoreF(Load(ToDoublePtr(vmframe, offsetF + i * sizeof(double))), i);

	for (int i = 0; i < sfunc->NumRegS; i++)
		StoreS(OffsetPtr(vmframe, offsetS + i * sizeof(FString)), i);

	for (int i = 0; i < sfunc->NumRegA; i++)
		StoreA(Load(ToInt8PtrPtr(vmframe, offsetA + i * sizeof(void*))), i);
}

void JitCompiler::EmitPopFrame()
{
	//if (sfunc->SpecialInits.Size() != 0 || sfunc->NumRegS != 0 || sfunc->ExtraSpace != 0)
	{
		cc.CreateCall(popFullVMFrame, { vmframestack });
	}
}

void JitCompiler::IncrementVMCalls()
{
	// VMCalls[0]++
	IRValue* vmcallsptr = ircontext->getConstantInt(int32PtrTy, (uint64_t)VMCalls);
	cc.CreateStore(cc.CreateAdd(cc.CreateLoad(vmcallsptr), ircontext->getConstantInt(1)), vmcallsptr);
}

void JitCompiler::CreateRegisters()
{
	regD.Resize(sfunc->NumRegD);
	regF.Resize(sfunc->NumRegF);
	regA.Resize(sfunc->NumRegA);
	regS.Resize(sfunc->NumRegS);

	FString regname;
	IRValue* arraySize = ircontext->getConstantInt(1);

	for (int i = 0; i < sfunc->NumRegD; i++)
	{
		regname.Format("regD%d", i);
		regD[i] = irfunc->createAlloca(int32Ty, arraySize, regname.GetChars());
	}

	for (int i = 0; i < sfunc->NumRegF; i++)
	{
		regname.Format("regF%d", i);
		regF[i] = irfunc->createAlloca(doubleTy, arraySize, regname.GetChars());
	}

	for (int i = 0; i < sfunc->NumRegS; i++)
	{
		regname.Format("regS%d", i);
		regS[i] = irfunc->createAlloca(int8PtrTy, arraySize, regname.GetChars());
	}

	for (int i = 0; i < sfunc->NumRegA; i++)
	{
		regname.Format("regA%d", i);
		regA[i] = irfunc->createAlloca(int8PtrTy, arraySize, regname.GetChars());
	}
}

void JitCompiler::EmitNullPointerThrow(int index, EVMAbortException reason)
{
	auto continuebb = irfunc->createBasicBlock({});
	auto exceptionbb = EmitThrowExceptionLabel(reason);
	cc.CreateCondBr(cc.CreateICmpEQ(LoadA(index), ConstValueA(nullptr)), exceptionbb, continuebb);
	cc.SetInsertPoint(continuebb);
}

void JitCompiler::EmitThrowException(EVMAbortException reason)
{
	cc.CreateCall(throwException, { ConstValueD(reason) });
	cc.CreateRet(ConstValueD(0));
}

IRBasicBlock* JitCompiler::EmitThrowExceptionLabel(EVMAbortException reason)
{
	auto bb = irfunc->createBasicBlock({});
	auto cursor = cc.GetInsertBlock();
	cc.SetInsertPoint(bb);
	EmitThrowException(reason);
	bb->code.front()->lineNumber = sfunc->PCToLine(pc);
	cc.SetInsertPoint(cursor);
	return bb;
}

void JitCompiler::EmitNOP()
{
	// The IR doesn't have a NOP instruction
}

static void ValidateCall(DObject* o, VMFunction* f, int b)
{
	FScopeBarrier::ValidateCall(o->GetClass(), f, b - 1);
}

static void SetReturnString(VMReturn* ret, FString* str)
{
	ret->SetString(*str);
}

static VMFrameStack* CreateFullVMFrame(VMScriptFunction* func, VMValue* args, int numargs)
{
	VMFrameStack* stack = &GlobalVMStack;
	VMFrame* newf = stack->AllocFrame(func);
	VMFillParams(args, newf, numargs);
	return stack;
}

static void PopFullVMFrame(VMFrameStack* vmframestack)
{
	vmframestack->PopFrame();
}

static void ThrowException(int reason)
{
	ThrowAbortException((EVMAbortException)reason, nullptr);
}

static void ThrowArrayOutOfBounds(int index, int size)
{
	if (index >= size)
	{
		ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Max.index = %u, current index = %u\n", size, index);
	}
	else
	{
		ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Negative current index = %i\n", index);
	}
}

static DObject* ReadBarrier(DObject* p) { return GC::ReadBarrier(p); }
static void WriteBarrier(DObject* p) { GC::WriteBarrier(p); }

static void StringAssignmentOperator(FString* to, FString* from) { *to = *from; }
static void StringAssignmentOperatorCStr(FString* to, char** from) { *to = *from; }
static void StringPlusOperator(FString* to, FString* first, FString* second) { *to = *first + *second; }
static int StringLength(FString* str) { return static_cast<int>(str->Len()); }
static int StringCompareNoCase(FString* first, FString* second) { return first->CompareNoCase(*second); }
static int StringCompare(FString* first, FString* second) { return first->Compare(*second); }

static double DoubleModF(double a, double b) { return a - floor(a / b) * b; }
static double DoublePow(double a, double b) { return g_pow(a, b); }
static double DoubleAtan2(double a, double b) { return g_atan2(a, b); }
static double DoubleFabs(double a) { return fabs(a); }
static double DoubleExp(double a) { return g_exp(a); }
static double DoubleLog(double a) { return g_log(a); }
static double DoubleLog10(double a) { return g_log10(a); }
static double DoubleSqrt(double a) { return g_sqrt(a); }
static double DoubleCeil(double a) { return ceil(a); }
static double DoubleFloor(double a) { return floor(a); }
static double DoubleAcos(double a) { return g_acos(a); }
static double DoubleAsin(double a) { return g_asin(a); }
static double DoubleAtan(double a) { return g_atan(a); }
static double DoubleCos(double a) { return g_cos(a); }
static double DoubleSin(double a) { return g_sin(a); }
static double DoubleTan(double a) { return g_tan(a); }
static double DoubleCosDeg(double a) { return g_cosdeg(a); }
static double DoubleSinDeg(double a) { return g_sindeg(a); }
static double DoubleCosh(double a) { return g_cosh(a); }
static double DoubleSinh(double a) { return g_sinh(a); }
static double DoubleTanh(double a) { return g_tanh(a); }
static double DoubleRound(double a) { return round(a); }

static void CastI2S(FString* a, int b) { a->Format("%d", b); }
static void CastU2S(FString* a, int b) { a->Format("%u", b); }
static void CastF2S(FString* a, double b) { a->Format("%.5f", b); }
static void CastV22S(FString* a, double b, double b1) { a->Format("(%.5f, %.5f)", b, b1); }
static void CastV32S(FString* a, double b, double b1, double b2) { a->Format("(%.5f, %.5f, %.5f)", b, b1, b2); }
static void CastP2S(FString* a, void* b) { if (b == nullptr) *a = "null"; else a->Format("%p", b); }
static int CastS2I(FString* b) { return (int)b->ToLong(); }
static double CastS2F(FString* b) { return b->ToDouble(); }
static int CastS2N(FString* b) { return b->Len() == 0 ? NAME_None : FName(*b).GetIndex(); }
static void CastN2S(FString* a, int b) { FName name = FName(ENamedName(b)); *a = name.IsValidName() ? name.GetChars() : ""; }
static int CastS2Co(FString* b) { return V_GetColor(nullptr, *b); }
static void CastCo2S(FString* a, int b) { PalEntry c(b); a->Format("%02x %02x %02x", c.r, c.g, c.b); }
static int CastS2So(FString* b) { return FSoundID(*b); }
static void CastSo2S(FString* a, int b) { *a = soundEngine->GetSoundName(b); }
static void CastSID2S(FString* a, unsigned int b) { VM_CastSpriteIDToString(a, b); }
static void CastTID2S(FString* a, int b) { auto tex = TexMan.GetGameTexture(*(FTextureID*)&b); *a = (tex == nullptr) ? "(null)" : tex->GetName().GetChars(); }
static int CastB_S(FString* s) { return s->Len() > 0; }

static DObject* DynCast(DObject* obj, PClass* cls) { return (obj && obj->IsKindOf(cls)) ? obj : nullptr; }
static PClass* DynCastC(PClass* cls1, PClass* cls2) { return (cls1 && cls1->IsDescendantOf(cls2)) ? cls1 : nullptr; }

void JitCompiler::GetTypes()
{
	voidTy = ircontext->getVoidTy();
	int8Ty = ircontext->getInt8Ty();
	int8PtrTy = ircontext->getInt8PtrTy();
	int8PtrPtrTy = int8PtrTy->getPointerTo(ircontext);
	int16Ty = ircontext->getInt16Ty();
	int16PtrTy = ircontext->getInt16PtrTy();
	int32Ty = ircontext->getInt32Ty();
	int32PtrTy = ircontext->getInt32PtrTy();
	int64Ty = ircontext->getInt64Ty();
	floatTy = ircontext->getFloatTy();
	floatPtrTy = ircontext->getFloatPtrTy();
	doubleTy = ircontext->getDoubleTy();
	doublePtrTy = ircontext->getDoublePtrTy();
}

void JitCompiler::CreateNativeFunctions()
{
	validateCall = CreateNativeFunction(voidTy, { int8PtrTy, int8PtrTy, int32Ty }, "__ValidateCall", ValidateCall);
	setReturnString = CreateNativeFunction(voidTy, { int8PtrTy, int8PtrTy }, "__SetReturnString", SetReturnString);
	createFullVMFrame = CreateNativeFunction(int8PtrTy, { int8PtrTy, int8PtrTy, int32Ty }, "__CreateFullVMFrame", CreateFullVMFrame);
	popFullVMFrame = CreateNativeFunction(voidTy, { int8PtrTy }, "__PopFullVMFrame", PopFullVMFrame);
	throwException = CreateNativeFunction(voidTy, { int32Ty }, "__ThrowException", ThrowException);
	throwArrayOutOfBounds = CreateNativeFunction(voidTy, { int32Ty, int32Ty }, "__ThrowArrayOutOfBounds", ThrowArrayOutOfBounds);
	stringAssignmentOperator = CreateNativeFunction(voidTy, { int8PtrTy, int8PtrTy }, "__StringAssignmentOperator", StringAssignmentOperator);
	stringAssignmentOperatorCStr = CreateNativeFunction(voidTy, { int8PtrTy, int8PtrTy }, "__StringAssignmentOperatorCStr", StringAssignmentOperatorCStr);
	stringPlusOperator = CreateNativeFunction(voidTy, { int8PtrTy, int8PtrTy, int8PtrTy }, "__StringPlusOperator", StringPlusOperator);
	stringCompare = CreateNativeFunction(int32Ty, { int8PtrTy, int8PtrTy }, "__StringCompare", StringCompare);
	stringCompareNoCase = CreateNativeFunction(int32Ty, { int8PtrTy, int8PtrTy }, "__StringCompareNoCase", StringCompareNoCase);
	stringLength = CreateNativeFunction(int32Ty, { int8PtrTy }, "__StringLength", StringLength);
	readBarrier = CreateNativeFunction(int8PtrTy, { int8PtrTy }, "__ReadBarrier", ReadBarrier);
	writeBarrier = CreateNativeFunction(voidTy, { int8PtrTy }, "__WriteBarrier", WriteBarrier);
	doubleModF = CreateNativeFunction(doubleTy, { doubleTy, doubleTy }, "__DoubleModF", DoubleModF);
	doublePow = CreateNativeFunction(doubleTy, { doubleTy, doubleTy }, "__DoublePow", DoublePow);
	doubleAtan2 = CreateNativeFunction(doubleTy, { doubleTy, doubleTy }, "__DoubleAtan2", DoubleAtan2);
	doubleFabs = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleFabs", DoubleFabs);
	doubleExp = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleExp", DoubleExp);
	doubleLog = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleLog", DoubleLog);
	doubleLog10 = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleLog10", DoubleLog10);
	doubleSqrt = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleSqrt", DoubleSqrt);
	doubleCeil = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleCeil", DoubleCeil);
	doubleFloor = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleFloor", DoubleFloor);
	doubleAcos = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleAcos", DoubleAcos);
	doubleAsin = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleAsin", DoubleAsin);
	doubleAtan = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleAtan", DoubleAtan);
	doubleCos = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleCos", DoubleCos);
	doubleSin = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleSin", DoubleSin);
	doubleTan = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleTan", DoubleTan);
	doubleCosDeg = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleCosDeg", DoubleCosDeg);
	doubleSinDeg = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleSinDeg", DoubleSinDeg);
	doubleCosh = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleCosh", DoubleCosh);
	doubleSinh = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleSinh", DoubleSinh);
	doubleTanh = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleTanh", DoubleTanh);
	doubleRound = CreateNativeFunction(doubleTy, { doubleTy }, "__DoubleRound", DoubleRound);
	castI2S = CreateNativeFunction(voidTy, { int8PtrTy, int32Ty }, "__CastI2S", CastI2S);
	castU2S = CreateNativeFunction(voidTy, { int8PtrTy, int32Ty }, "__CastU2S", CastU2S);
	castF2S = CreateNativeFunction(voidTy, { int8PtrTy, doubleTy }, "__CastF2S", CastF2S);
	castV22S = CreateNativeFunction(voidTy, { int8PtrTy, doubleTy, doubleTy }, "__CastV22S", CastV22S);
	castV32S = CreateNativeFunction(voidTy, { int8PtrTy, doubleTy, doubleTy, doubleTy }, "__CastV32S", CastV32S);
	castP2S = CreateNativeFunction(voidTy, { int8PtrTy, int8PtrTy }, "__CastP2S", CastP2S);
	castS2I = CreateNativeFunction(int32Ty, { int8PtrTy }, "__CastS2I", CastS2I);
	castS2F = CreateNativeFunction(doubleTy, { int8PtrTy }, "__CastS2F", CastS2F);
	castS2N = CreateNativeFunction(int32Ty, { int8PtrTy }, "__CastS2N", CastS2N);
	castN2S = CreateNativeFunction(voidTy, { int8PtrTy, int32Ty }, "__CastN2S", CastN2S);
	castS2Co = CreateNativeFunction(int32Ty, { int8PtrTy }, "__CastS2Co", CastS2Co);
	castCo2S = CreateNativeFunction(voidTy, { int8PtrTy, int32Ty }, "__CastCo2S", CastCo2S);
	castS2So = CreateNativeFunction(int32Ty, { int8PtrTy }, "__CastS2So", CastS2So);
	castSo2S = CreateNativeFunction(voidTy, { int8PtrTy, int32Ty }, "__CastSo2S", CastSo2S);
	castSID2S = CreateNativeFunction(voidTy, { int8PtrTy, int32Ty }, "__CastSID2S", CastSID2S);
	castTID2S = CreateNativeFunction(voidTy, { int8PtrTy, int32Ty }, "__CastTID2S", CastTID2S);
	castB_S = CreateNativeFunction(int32Ty, { int8PtrTy }, "__CastB_S", CastB_S);
	dynCast = CreateNativeFunction(int8PtrTy, { int8PtrTy, int8PtrTy }, "__DynCast", DynCast);
	dynCastC = CreateNativeFunction(int8PtrTy, { int8PtrTy, int8PtrTy }, "__DynCastC", DynCastC);
}

IRFunction* JitCompiler::CreateNativeFunction(IRType* returnType, std::vector<IRType*> args, const char* name, void* ptr)
{
	IRFunctionType* functype = ircontext->getFunctionType(returnType, args);
	IRFunction* func = ircontext->createFunction(functype, name);
	ircontext->addGlobalMapping(func, ptr);
	return func;
}
