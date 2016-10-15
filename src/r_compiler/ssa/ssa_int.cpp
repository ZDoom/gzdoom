
#include "r_compiler/llvm_include.h"
#include "ssa_int.h"
#include "ssa_float.h"
#include "ssa_bool.h"
#include "ssa_scope.h"

SSAInt::SSAInt()
: v(0)
{
}

SSAInt::SSAInt(int constant)
: v(0)
{
	v = llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(32, constant, true));
}

SSAInt::SSAInt(SSAFloat f)
: v(0)
{
	v = SSAScope::builder().CreateFPToSI(f.v, llvm::Type::getInt32Ty(SSAScope::context()), SSAScope::hint());
}

SSAInt::SSAInt(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAInt::llvm_type()
{
	return llvm::Type::getInt32Ty(SSAScope::context());
}

SSAInt SSAInt::MIN(SSAInt a, SSAInt b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateSelect((a < b).v, a.v, b.v, SSAScope::hint()));
}

SSAInt SSAInt::MAX(SSAInt a, SSAInt b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateSelect((a > b).v, a.v, b.v, SSAScope::hint()));
}

SSAInt SSAInt::add(SSAInt b, bool no_unsigned_wrap, bool no_signed_wrap)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateAdd(v, b.v, SSAScope::hint(), no_unsigned_wrap, no_signed_wrap));
}

SSAInt SSAInt::ashr(int bits)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateAShr(v, bits, SSAScope::hint()));
}

SSAInt operator+(const SSAInt &a, const SSAInt &b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateAdd(a.v, b.v, SSAScope::hint()));
}

SSAInt operator-(const SSAInt &a, const SSAInt &b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateSub(a.v, b.v, SSAScope::hint()));
}

SSAInt operator*(const SSAInt &a, const SSAInt &b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateMul(a.v, b.v, SSAScope::hint()));
}

SSAInt operator/(const SSAInt &a, const SSAInt &b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateSDiv(a.v, b.v, SSAScope::hint()));
}

SSAInt operator%(const SSAInt &a, const SSAInt &b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateSRem(a.v, b.v, SSAScope::hint()));
}

SSAInt operator+(int a, const SSAInt &b)
{
	return SSAInt(a) + b;
}

SSAInt operator-(int a, const SSAInt &b)
{
	return SSAInt(a) - b;
}

SSAInt operator*(int a, const SSAInt &b)
{
	return SSAInt(a) * b;
}

SSAInt operator/(int a, const SSAInt &b)
{
	return SSAInt(a) / b;
}

SSAInt operator%(int a, const SSAInt &b)
{
	return SSAInt(a) % b;
}

SSAInt operator+(const SSAInt &a, int b)
{
	return a + SSAInt(b);
}

SSAInt operator-(const SSAInt &a, int b)
{
	return a - SSAInt(b);
}

SSAInt operator*(const SSAInt &a, int b)
{
	return a * SSAInt(b);
}

SSAInt operator/(const SSAInt &a, int b)
{
	return a / SSAInt(b);
}

SSAInt operator%(const SSAInt &a, int b)
{
	return a % SSAInt(b);
}

SSAInt operator<<(const SSAInt &a, int bits)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateShl(a.v, bits, SSAScope::hint()));
}

SSAInt operator>>(const SSAInt &a, int bits)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateLShr(a.v, bits, SSAScope::hint()));
}

SSAInt operator<<(const SSAInt &a, const SSAInt &bits)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateShl(a.v, bits.v, SSAScope::hint()));
}

SSAInt operator>>(const SSAInt &a, const SSAInt &bits)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateLShr(a.v, bits.v, SSAScope::hint()));
}

SSAInt operator&(const SSAInt &a, int b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateAnd(a.v, b, SSAScope::hint()));
}

SSAInt operator&(const SSAInt &a, const SSAInt &b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateAnd(a.v, b.v, SSAScope::hint()));
}

SSAInt operator|(const SSAInt &a, int b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateOr(a.v, b, SSAScope::hint()));
}

SSAInt operator|(const SSAInt &a, const SSAInt &b)
{
	return SSAInt::from_llvm(SSAScope::builder().CreateOr(a.v, b.v, SSAScope::hint()));
}
