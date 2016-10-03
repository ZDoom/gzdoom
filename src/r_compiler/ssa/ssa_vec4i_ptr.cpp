
#include "r_compiler/llvm_include.h"
#include "ssa_vec4i_ptr.h"
#include "ssa_scope.h"

SSAVec4iPtr::SSAVec4iPtr()
: v(0)
{
}

SSAVec4iPtr::SSAVec4iPtr(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAVec4iPtr::llvm_type()
{
	return llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
}

SSAVec4iPtr SSAVec4iPtr::operator[](SSAInt index) const
{
	return SSAVec4iPtr::from_llvm(SSAScope::builder().CreateGEP(v, index.v, SSAScope::hint()));
}

SSAVec4i SSAVec4iPtr::load() const
{
	return SSAVec4i::from_llvm(SSAScope::builder().CreateLoad(v, false, SSAScope::hint()));
}

SSAVec4i SSAVec4iPtr::load_unaligned() const
{
	return SSAVec4i::from_llvm(SSAScope::builder().Insert(new llvm::LoadInst(v, SSAScope::hint(), false, 4)));
}

void SSAVec4iPtr::store(const SSAVec4i &new_value)
{
	SSAScope::builder().CreateStore(new_value.v, v, false);
}

void SSAVec4iPtr::store_unaligned(const SSAVec4i &new_value)
{
	SSAScope::builder().CreateAlignedStore(new_value.v, v, 4, false);
}
