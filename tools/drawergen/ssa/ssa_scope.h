/*
**  SSA scope data
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

#pragma once

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
	static llvm::Value *alloc_stack(llvm::Type *type);
	static llvm::Value *alloc_stack(llvm::Type *type, SSAInt size);
	static llvm::MDNode *constant_scope_list();
	static const std::string &hint();
	static void set_hint(const std::string &hint);

private:
	static SSAScope *instance;
	llvm::LLVMContext *_context;
	llvm::Module *_module;
	llvm::IRBuilder<> *_builder;
	llvm::MDNode *_constant_scope_domain;
	llvm::MDNode *_constant_scope;
	llvm::MDNode *_constant_scope_list;
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
