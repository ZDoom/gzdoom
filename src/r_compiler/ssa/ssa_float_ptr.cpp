
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

SSAFloat SSAFloatPtr::load() const
{
	return SSAFloat::from_llvm(SSAScope::builder().CreateLoad(v, false, SSAScope::hint()));
}

SSAVec4f SSAFloatPtr::load_vec4f() const
{
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	return SSAVec4f::from_llvm(SSAScope::builder().CreateLoad(SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()), false, SSAScope::hint()));
}

SSAVec4f SSAFloatPtr::load_unaligned_vec4f() const
{
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	return SSAVec4f::from_llvm(SSAScope::builder().Insert(new llvm::LoadInst(SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()), SSAScope::hint(), false, 4), SSAScope::hint()));
}

void SSAFloatPtr::store(const SSAFloat &new_value)
{
	SSAScope::builder().CreateStore(new_value.v, v, false);
}

void SSAFloatPtr::store_vec4f(const SSAVec4f &new_value)
{
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	SSAScope::builder().CreateStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()));
}

void SSAFloatPtr::store_unaligned_vec4f(const SSAVec4f &new_value)
{
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	SSAScope::builder().CreateAlignedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()), 4);
}
