
#include "ssa_float_ptr.h"
#include "ssa_scope.h"
#include "r_compiler/llvm_include.h"

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
	// return SSAVec4f::from_llvm(SSAScope::builder().CreateCall(get_intrinsic(llvm::Intrinsic::x86_sse2_loadu_dq), SSAScope::builder().CreateBitCast(v, llvm::PointerType::getUnqual(llvm::IntegerType::get(SSAScope::context(), 8)))));
}

void SSAFloatPtr::store(const SSAFloat &new_value)
{
	SSAScope::builder().CreateStore(new_value.v, v, false);
}

void SSAFloatPtr::store_vec4f(const SSAVec4f &new_value)
{
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	SSAScope::builder().CreateAlignedStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()), 16);
}

void SSAFloatPtr::store_unaligned_vec4f(const SSAVec4f &new_value)
{
	/*llvm::Value *values[2] =
	{
		SSAScope::builder().CreateBitCast(v, llvm::Type::getFloatPtrTy(SSAScope::context())),
		new_value.v
	};
	SSAScope::builder().CreateCall(SSAScope::intrinsic(llvm::Intrinsic::x86_sse_storeu_ps), values);*/
	llvm::PointerType *m4xfloattypeptr = llvm::VectorType::get(llvm::Type::getFloatTy(SSAScope::context()), 4)->getPointerTo();
	SSAScope::builder().CreateStore(new_value.v, SSAScope::builder().CreateBitCast(v, m4xfloattypeptr, SSAScope::hint()));
}
