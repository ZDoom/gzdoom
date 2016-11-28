/*
**  SSA int32 pointer
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
#include "ssa_int_ptr.h"
#include "ssa_scope.h"

SSAIntPtr::SSAIntPtr()
: v(0)
{
}

SSAIntPtr::SSAIntPtr(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAIntPtr::llvm_type()
{
	return llvm::Type::getInt32PtrTy(SSAScope::context());
}

SSAIntPtr SSAIntPtr::operator[](SSAInt index) const
{
	return SSAIntPtr::from_llvm(SSAScope::builder().CreateGEP(v, index.v, SSAScope::hint()));
}

SSAInt SSAIntPtr::load(bool constantScopeDomain) const
{
	auto loadInst = SSAScope::builder().CreateLoad(v, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAInt::from_llvm(loadInst);
}

SSAVec4i SSAIntPtr::load_vec4i(bool constantScopeDomain) const
{
	llvm::PointerType *m4xint32typeptr = llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
	auto loadInst = SSAScope::builder().CreateAlignedLoad(SSAScope::builder().CreateBitCast(v, m4xint32typeptr, SSAScope::hint()), 16, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAVec4i::from_llvm(loadInst);
}

SSAVec4i SSAIntPtr::load_unaligned_vec4i(bool constantScopeDomain) const
{
	llvm::PointerType *m4xint32typeptr = llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
	auto loadInst = SSAScope::builder().CreateAlignedLoad(SSAScope::builder().CreateBitCast(v, m4xint32typeptr, SSAScope::hint()), 1, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAVec4i::from_llvm(loadInst);
}

void SSAIntPtr::store(const SSAInt &new_value)
{
	auto inst = SSAScope::builder().CreateStore(new_value.v, v, false);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

void SSAIntPtr::store_vec4i(const SSAVec4i &new_value)
{
	llvm::PointerType *m4xint32typeptr = llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
	auto inst = SSAScope::builder().CreateAlignedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xint32typeptr, SSAScope::hint()), 16);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

void SSAIntPtr::store_unaligned_vec4i(const SSAVec4i &new_value)
{
	llvm::PointerType *m4xint32typeptr = llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
	auto inst = SSAScope::builder().CreateAlignedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xint32typeptr, SSAScope::hint()), 1);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}
