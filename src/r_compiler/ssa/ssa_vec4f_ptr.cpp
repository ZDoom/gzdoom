
#include "r_compiler/llvm_include.h"
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
	auto inst = SSAScope::builder().CreateAlignedStore(new_value.v, v, 1, false);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}
