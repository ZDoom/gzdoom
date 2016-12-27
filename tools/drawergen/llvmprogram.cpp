/*
**  LLVM code generated drawers
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "precomp.h"
#include "timestamp.h"
#include "llvmprogram.h"

LLVMProgram::LLVMProgram()
{
	mContext = std::make_unique<llvm::LLVMContext>();
}

void LLVMProgram::CreateModule()
{
	mModule = std::make_unique<llvm::Module>("render", context());
}

std::vector<uint8_t> LLVMProgram::GenerateObjectFile(const std::string &triple, const std::string &cpuName, const std::string &features)
{
	using namespace llvm;

	std::string errorstring;

	llvm::Module *module = mModule.get();

	const Target *target = TargetRegistry::lookupTarget(triple, errorstring);
	
#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 9)
	Reloc::Model relocationModel = Reloc::PIC_;
#else
	Optional<Reloc::Model> relocationModel = Reloc::PIC_;
#endif
	
	CodeModel::Model codeModel = CodeModel::Model::Default;

	TargetOptions options;
	options.LessPreciseFPMADOption = true;
	options.AllowFPOpFusion = FPOpFusion::Fast;
	options.UnsafeFPMath = true;
	options.NoInfsFPMath = true;
	options.NoNaNsFPMath = true;
	options.HonorSignDependentRoundingFPMathOption = false;
	options.NoZerosInBSS = false;
	options.GuaranteedTailCallOpt = false;
	options.StackAlignmentOverride = 0;
	options.UseInitArray = true;
	options.DataSections = false;
	options.FunctionSections = false;
	options.JTType = JumpTable::Single; // Create a single table for all jumptable functions
	options.ThreadModel = ThreadModel::POSIX;
	options.DisableIntegratedAS = false;
	options.MCOptions.SanitizeAddress = false;
	options.MCOptions.MCRelaxAll = false; // relax all fixups in the emitted object file
	options.MCOptions.DwarfVersion = 0;
	options.MCOptions.ShowMCInst = false;
	options.MCOptions.ABIName = "";
	options.MCOptions.MCFatalWarnings = false;
	options.MCOptions.ShowMCEncoding = false; // Show encoding in .s output
	options.MCOptions.MCUseDwarfDirectory = false;
	options.MCOptions.AsmVerbose = true;

#if LLVM_VERSION_MAJOR > 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 9)
	options.Reciprocals = TargetRecip({ "all" });
	options.StackSymbolOrdering = true;
	options.UniqueSectionNames = true;
	options.EmulatedTLS = false;
	options.ExceptionModel = ExceptionHandling::None;
	options.EABIVersion = EABI::Default;
	options.DebuggerTuning = DebuggerKind::Default;
	options.MCOptions.MCIncrementalLinkerCompatible = false;
	options.MCOptions.MCNoWarn = false;
	options.MCOptions.PreserveAsmComments = true;
#endif

	CodeGenOpt::Level optimizationLevel = CodeGenOpt::Aggressive;
	machine = target->createTargetMachine(triple, cpuName, features, options, relocationModel, codeModel, optimizationLevel);


#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 8)
	std::string targetTriple = machine->getTargetTriple();
#else
	std::string targetTriple = machine->getTargetTriple().getTriple();
#endif

	module->setTargetTriple(targetTriple);
#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 8)
	module->setDataLayout(new DataLayout(*machine->getSubtargetImpl()->getDataLayout()));
#else
	module->setDataLayout(machine->createDataLayout());
#endif

	legacy::FunctionPassManager PerFunctionPasses(module);
	legacy::PassManager PerModulePasses;

#if LLVM_VERSION_MAJOR > 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 8)
	PerFunctionPasses.add(createTargetTransformInfoWrapperPass(machine->getTargetIRAnalysis()));
	PerModulePasses.add(createTargetTransformInfoWrapperPass(machine->getTargetIRAnalysis()));
#endif

	SmallString<16 * 1024> str;
#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 8)
	raw_svector_ostream vecstream(str);
	formatted_raw_ostream stream(vecstream);
#else
	raw_svector_ostream stream(str);
#endif
	machine->addPassesToEmitFile(PerModulePasses, stream, TargetMachine::CGFT_ObjectFile);

	PassManagerBuilder passManagerBuilder;
	passManagerBuilder.OptLevel = 3;
	passManagerBuilder.SizeLevel = 0;
	passManagerBuilder.Inliner = createFunctionInliningPass();
	passManagerBuilder.SLPVectorize = true;
	passManagerBuilder.LoopVectorize = true;
	passManagerBuilder.LoadCombine = true;
	passManagerBuilder.populateModulePassManager(PerModulePasses);
	passManagerBuilder.populateFunctionPassManager(PerFunctionPasses);

	// Run function passes:
	PerFunctionPasses.doInitialization();
	for (llvm::Function &func : *module)
	{
		if (!func.isDeclaration())
			PerFunctionPasses.run(func);
	}
	PerFunctionPasses.doFinalization();

	// Run module passes:
	PerModulePasses.run(*module);

	// Return the resulting object file
#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 8)
	stream.flush();
	vecstream.flush();
#endif
	std::vector<uint8_t> data;
	data.resize(str.size());
	memcpy(data.data(), str.data(), data.size());
	return data;
}

std::string LLVMProgram::DumpModule()
{
	std::string str;
	llvm::raw_string_ostream stream(str);
#if LLVM_VERSION_MAJOR < 3 || (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 8)
	mModule->print(stream, nullptr);
#else
	mModule->print(stream, nullptr, false, true);
#endif
	return stream.str();
}
