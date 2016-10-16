
#pragma once

#ifdef _MSC_VER

#if defined(min)
#define llvm_min_bug min
#undef min
#endif
#if defined(max)
#define llvm_max_bug max
#undef max
#endif

#pragma warning(disable: 4146) // warning C4146: unary minus operator applied to unsigned type, result still unsigned
#pragma warning(disable: 4624) // warning C4624: 'llvm::AugmentedUse' : destructor could not be generated because a base class destructor is inaccessible
#pragma warning(disable: 4355) // warning C4355: 'this' : used in base member initializer list
#pragma warning(disable: 4800) // warning C4800: 'const unsigned int' : forcing value to bool 'true' or 'false' (performance warning)
#pragma warning(disable: 4996) // warning C4996: 'std::_Copy_impl': Function call with parameters that may be unsafe - this call relies on the caller to check that the passed values are correct. To disable this warning, use -D_Sclan::SECURE_NO_WARNINGS. See documentation on how to use Visual C++ 'Checked Iterators'
#pragma warning(disable: 4244) // warning C4244: 'return' : conversion from 'uint64_t' to 'unsigned int', possible loss of data
#pragma warning(disable: 4141) // warning C4141: 'inline': used more than once
#pragma warning(disable: 4291) // warning C4291: 'void *llvm::User::operator new(std::size_t,unsigned int,unsigned int)': no matching operator delete found; memory will not be freed if initialization throws an exception
#pragma warning(disable: 4267) // warning C4267: 'return': conversion from 'size_t' to 'unsigned int', possible loss of data
#pragma warning(disable: 4244) // warning C4244: 'initializing': conversion from '__int64' to 'unsigned int', possible loss of data

#endif

#ifdef __APPLE__
#define __STDC_LIMIT_MACROS    // DataTypes.h:57:3: error: "Must #define __STDC_LIMIT_MACROS before #including Support/DataTypes.h"
#define __STDC_CONSTANT_MACROS // DataTypes.h:61:3: error: "Must #define __STDC_CONSTANT_MACROS before " "#including Support/DataTypes.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wredundant-move"
#endif

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Verifier.h>
//#include <llvm/IR/PassManager.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/Host.h>
#include <llvm/CodeGen/AsmPrinter.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/Target/TargetSubtargetInfo.h>

#ifdef __APPLE__
#pragma clang diagnostic pop
#endif

#ifdef _MSC_VER

#if defined(llvm_min_bug)
#define min llvm_min_bug
#undef llvm_min_bug
#endif
#if defined(llvm_max_bug)
#define max llvm_max_bug
#undef llvm_max_bug
#endif

#endif
