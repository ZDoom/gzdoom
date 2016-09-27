
#include "ssa_vec4f_ptr.h"
#include "ssa_scope.h"
#include "r_compiler/llvm_include.h"

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

SSAVec4f SSAVec4fPtr::load() const
{
	return SSAVec4f::from_llvm(SSAScope::builder().CreateLoad(v, false, SSAScope::hint()));
}

SSAVec4f SSAVec4fPtr::load_unaligned() const
{
	return SSAVec4f::from_llvm(SSAScope::builder().Insert(new llvm::LoadInst(v, SSAScope::hint(), false, 4), SSAScope::hint()));
}

void SSAVec4fPtr::store(const SSAVec4f &new_value)
{
	SSAScope::builder().CreateStore(new_value.v, v, false);
}

void SSAVec4fPtr::store_unaligned(const SSAVec4f &new_value)
{
	SSAScope::builder().CreateAlignedStore(new_value.v, v, 4, false);
}
