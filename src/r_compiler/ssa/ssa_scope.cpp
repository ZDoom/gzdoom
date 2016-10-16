
#include "r_compiler/llvm_include.h"
#include "ssa_scope.h"
#include "ssa_int.h"

SSAScope::SSAScope(llvm::LLVMContext *context, llvm::Module *module, llvm::IRBuilder<> *builder)
: _context(context), _module(module), _builder(builder)
{
	instance = this;

	_constant_scope_domain = llvm::MDNode::get(SSAScope::context(), { llvm::MDString::get(SSAScope::context(), "ConstantScopeDomain") });
	_constant_scope = llvm::MDNode::getDistinct(SSAScope::context(), { _constant_scope_domain });
	_constant_scope_list = llvm::MDNode::get(SSAScope::context(), { _constant_scope });
}

SSAScope::~SSAScope()
{
	instance = 0;
}

llvm::LLVMContext &SSAScope::context()
{
	return *instance->_context;
}

llvm::Module *SSAScope::module()
{
	return instance->_module;
}

llvm::IRBuilder<> &SSAScope::builder()
{
	return *instance->_builder;
}

llvm::Function *SSAScope::intrinsic(llvm::Intrinsic::ID id, llvm::ArrayRef<llvm::Type *> parameter_types)
{
	llvm::Function *func = module()->getFunction(llvm::Intrinsic::getName(id));
	if (func == 0)
		func = llvm::Function::Create(llvm::Intrinsic::getType(context(), id, parameter_types), llvm::Function::ExternalLinkage, llvm::Intrinsic::getName(id, parameter_types), module());
	return func;
}

llvm::Value *SSAScope::alloc_stack(llvm::Type *type)
{
	return alloc_stack(type, SSAInt(1));
}

llvm::Value *SSAScope::alloc_stack(llvm::Type *type, SSAInt size)
{
	// Allocas must be created at top of entry block for the PromoteMemoryToRegisterPass to work
	llvm::BasicBlock &entry = SSAScope::builder().GetInsertBlock()->getParent()->getEntryBlock();
	llvm::IRBuilder<> alloca_builder(&entry, entry.begin());
	return alloca_builder.CreateAlloca(type, size.v, hint());
}

llvm::MDNode *SSAScope::constant_scope_list()
{
	return instance->_constant_scope_list;
}

const std::string &SSAScope::hint()
{
	return instance->_hint;
}

void SSAScope::set_hint(const std::string &new_hint)
{
	if (new_hint.empty())
		instance->_hint = "tmp";
	else
		instance->_hint = new_hint;
}

SSAScope *SSAScope::instance = 0;
