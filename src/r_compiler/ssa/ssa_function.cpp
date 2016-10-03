
#include "r_compiler/llvm_include.h"
#include "ssa_function.h"
#include "ssa_int.h"
#include "ssa_scope.h"
#include "ssa_value.h"

SSAFunction::SSAFunction(const std::string name)
: name(name), return_type(llvm::Type::getVoidTy(SSAScope::context())), func()
{
}

void SSAFunction::set_return_type(llvm::Type *type)
{
	return_type = type;
}

void SSAFunction::add_parameter(llvm::Type *type)
{
	parameters.push_back(type);
}

void SSAFunction::create_public()
{
	func = SSAScope::module()->getFunction(name.c_str());
	if (func == 0)
	{
		llvm::FunctionType *function_type = llvm::FunctionType::get(return_type, parameters, false);
		func = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, name.c_str(), SSAScope::module());
		//func->setCallingConv(llvm::CallingConv::X86_StdCall);
	}
	llvm::BasicBlock *entry = llvm::BasicBlock::Create(SSAScope::context(), "entry", func);
	SSAScope::builder().SetInsertPoint(entry);
}

void SSAFunction::create_private()
{
	func = SSAScope::module()->getFunction(name.c_str());
	if (func == 0)
	{
		llvm::FunctionType *function_type = llvm::FunctionType::get(return_type, parameters, false);
		func = llvm::Function::Create(function_type, llvm::Function::PrivateLinkage, name.c_str(), SSAScope::module());
		func->addFnAttr(llvm::Attribute::AlwaysInline);
	}
	llvm::BasicBlock *entry = llvm::BasicBlock::Create(SSAScope::context(), "entry", func);
	SSAScope::builder().SetInsertPoint(entry);
}

SSAValue SSAFunction::parameter(int index)
{
	llvm::Function::arg_iterator arg_it = func->arg_begin();
	for (int i = 0; i < index; i++)
		++arg_it;
	return SSAValue::from_llvm(static_cast<llvm::Argument*>(arg_it));
}
