
#include "ssa_int_ptr.h"
#include "ssa_scope.h"
#include "r_compiler/llvm_include.h"

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

SSAInt SSAIntPtr::load() const
{
	return SSAInt::from_llvm(SSAScope::builder().CreateLoad(v, false, SSAScope::hint()));
}

SSAVec4i SSAIntPtr::load_vec4i() const
{
	llvm::PointerType *m4xint32typeptr = llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
	return SSAVec4i::from_llvm(SSAScope::builder().CreateLoad(SSAScope::builder().CreateBitCast(v, m4xint32typeptr, SSAScope::hint()), false, SSAScope::hint()));
}

SSAVec4i SSAIntPtr::load_unaligned_vec4i() const
{
	llvm::PointerType *m4xint32typeptr = llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
	return SSAVec4i::from_llvm(SSAScope::builder().Insert(new llvm::LoadInst(SSAScope::builder().CreateBitCast(v, m4xint32typeptr, SSAScope::hint()), SSAScope::hint(), false, 4), SSAScope::hint()));
}

void SSAIntPtr::store(const SSAInt &new_value)
{
	SSAScope::builder().CreateStore(new_value.v, v, false);
}

void SSAIntPtr::store_vec4i(const SSAVec4i &new_value)
{
	llvm::PointerType *m4xint32typeptr = llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
	SSAScope::builder().CreateStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xint32typeptr, SSAScope::hint()));
}

void SSAIntPtr::store_unaligned_vec4i(const SSAVec4i &new_value)
{
	llvm::PointerType *m4xint32typeptr = llvm::VectorType::get(llvm::Type::getInt32Ty(SSAScope::context()), 4)->getPointerTo();
	SSAScope::builder().CreateAlignedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xint32typeptr, SSAScope::hint()), 4);
}
