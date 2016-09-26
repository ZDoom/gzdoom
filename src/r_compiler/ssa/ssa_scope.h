
#pragma once

#include "r_compiler/llvm_include.h"

class SSAInt;

class SSAScope
{
public:
	SSAScope(llvm::LLVMContext *context, llvm::Module *module, llvm::IRBuilder<> *builder);
	~SSAScope();
	static llvm::LLVMContext &context();
	static llvm::Module *module();
	static llvm::IRBuilder<> &builder();
	static llvm::Function *intrinsic(llvm::Intrinsic::ID id, llvm::ArrayRef<llvm::Type *> parameter_types = llvm::ArrayRef<llvm::Type*>());
	static llvm::Value *alloca(llvm::Type *type);
	static llvm::Value *alloca(llvm::Type *type, SSAInt size);
	static const std::string &hint();
	static void set_hint(const std::string &hint);

private:
	static SSAScope *instance;
	llvm::LLVMContext *_context;
	llvm::Module *_module;
	llvm::IRBuilder<> *_builder;
	std::string _hint;
};

class SSAScopeHint
{
public:
	SSAScopeHint() : old_hint(SSAScope::hint()) { }
	SSAScopeHint(const std::string &hint) : old_hint(SSAScope::hint()) { SSAScope::set_hint(hint); }
	~SSAScopeHint() { SSAScope::set_hint(old_hint); }
	void set(const std::string &hint) { SSAScope::set_hint(hint); }
	void clear() { SSAScope::set_hint(old_hint); }

private:
	std::string old_hint;
};
