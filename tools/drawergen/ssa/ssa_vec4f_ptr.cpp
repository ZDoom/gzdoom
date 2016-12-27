/*
**  SSA vec4 float pointer
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
#include "ssa_vec4f_ptr.h"
#include "ssa_scope.h"

SSAVec4fPtr::SSAVec4fPtr()
: v(0)
{
}

SSAVec4fPtr::SSAVec4fPtr(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAVec4fPtr::llvm_type()
{
	return llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
}

SSAVec4fPtr SSAVec4fPtr::operator[](SSAInt index) const
{
	return SSAVec4fPtr::from_llvm(SSAScope::builder().CreateGEP(v, index.v, SSAScope::hint()));
}

SSAVec4f SSAVec4fPtr::load(bool constantScopeDomain) const
{
	auto loadInst = SSAScope::builder().CreateAlignedLoad(v, 16, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAVec4f::from_llvm(loadInst);
}

SSAVec4f SSAVec4fPtr::load_unaligned(bool constantScopeDomain) const
{
	auto loadInst = SSAScope::builder().CreateAlignedLoad(v, 1, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAVec4f::from_llvm(loadInst);
}

void SSAVec4fPtr::store(const SSAVec4f &new_value)
{
	auto inst = SSAScope::builder().CreateAlignedStore(new_value.v, v, 16, false);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

void SSAVec4fPtr::store_unaligned(const SSAVec4f &new_value)
{
	auto inst = SSAScope::builder().CreateAlignedStore(new_value.v, v, 4, false);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}
