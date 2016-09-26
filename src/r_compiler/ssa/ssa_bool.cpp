
#include "ssa_bool.h"
#include "ssa_scope.h"
#include "r_compiler/llvm_include.h"

SSABool::SSABool()
: v(0)
{
}
/*
SSABool::SSABool(bool constant)
: v(0)
{
}
*/
SSABool::SSABool(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSABool::llvm_type()
{
	return llvm::Type::getInt1Ty(SSAScope::context());
}

SSABool operator&&(const SSABool &a, const SSABool &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateAnd(a.v, b.v, SSAScope::hint()));
}

SSABool operator||(const SSABool &a, const SSABool &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateOr(a.v, b.v, SSAScope::hint()));
}

SSABool operator!(const SSABool &a)
{
	return SSABool::from_llvm(SSAScope::builder().CreateNot(a.v, SSAScope::hint()));
}

SSABool operator<(const SSAInt &a, const SSAInt &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateICmpSLT(a.v, b.v, SSAScope::hint()));
}

SSABool operator<=(const SSAInt &a, const SSAInt &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateICmpSLE(a.v, b.v, SSAScope::hint()));
}

SSABool operator==(const SSAInt &a, const SSAInt &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateICmpEQ(a.v, b.v, SSAScope::hint()));
}

SSABool operator>=(const SSAInt &a, const SSAInt &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateICmpSGE(a.v, b.v, SSAScope::hint()));
}

SSABool operator>(const SSAInt &a, const SSAInt &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateICmpSGT(a.v, b.v, SSAScope::hint()));
}

/////////////////////////////////////////////////////////////////////////////

SSABool operator<(const SSAFloat &a, const SSAFloat &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateFCmpOLT(a.v, b.v, SSAScope::hint()));
}

SSABool operator<=(const SSAFloat &a, const SSAFloat &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateFCmpOLE(a.v, b.v, SSAScope::hint()));
}

SSABool operator==(const SSAFloat &a, const SSAFloat &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateFCmpOEQ(a.v, b.v, SSAScope::hint()));
}

SSABool operator>=(const SSAFloat &a, const SSAFloat &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateFCmpOGE(a.v, b.v, SSAScope::hint()));
}

SSABool operator>(const SSAFloat &a, const SSAFloat &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateFCmpOGT(a.v, b.v, SSAScope::hint()));
}
