/*
**  LLVM function
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
