
#include "r_compiler/llvm_include.h"
#include "ssa_float_ptr.h"
#include "ssa_scope.h"

SSAFloatPtr::SSAFloatPtr()
: v(0)
{
}

SSAFloatPtr::SSAFloatPtr(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAFloatPtr::llvm_type()
{
	return llvm::Type::getFloatPtrTy(SSAScope::context());
}

SSAFloatPtr SSAFloatPtr::operator[](SSAInt index) const
{
	return SSAFloatPtr::from_llvm(SSAScope::builder().CreateGEP(v, index.v, SSAScope::hint()));
}

SSAFloat SSAFloatPtr::load(bool constantScopeDomain) const
{
	auto loadInst = SSAScope::builder().CreateLoad(v, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAFloat::from_llvm(loadInst);
}

SSAVec4f SSAFloatPtr::load_vec4f(bool constantScopeDomain) const
{
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	auto loadInst = SSAScope::builder().CreateAlignedLoad(SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()), 16, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAVec4f::from_llvm(loadInst);
}

SSAVec4f SSAFloatPtr::load_unaligned_vec4f(bool constantScopeDomain) const
{
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	auto loadInst = SSAScope::builder().CreateAlignedLoad(SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()), 1, false, SSAScope::hint());
	if (constantScopeDomain)
		loadInst->setMetadata(llvm::LLVMContext::MD_alias_scope, SSAScope::constant_scope_list());
	return SSAVec4f::from_llvm(loadInst);
}

void SSAFloatPtr::store(const SSAFloat &new_value)
{
	auto inst = SSAScope::builder().CreateStore(new_value.v, v, false);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

void SSAFloatPtr::store_vec4f(const SSAVec4f &new_value)
{
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	auto inst = SSAScope::builder().CreateAlignedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()), 16);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}

void SSAFloatPtr::store_unaligned_vec4f(const SSAVec4f &new_value)
{
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	auto inst = SSAScope::builder().CreateAlignedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()), 1);
	inst->setMetadata(llvm::LLVMContext::MD_noalias, SSAScope::constant_scope_list());
}
