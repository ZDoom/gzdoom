
#include "r_compiler/llvm_include.h"
#include "ssa_ubyte.h"
#include "ssa_scope.h"

SSAUByte::SSAUByte()
: v(0)
{
}

SSAUByte::SSAUByte(unsigned char constant)
: v(0)
{
	v = llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(8, constant, false));
}

SSAUByte::SSAUByte(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAUByte::llvm_type()
{
	return llvm::Type::getInt8Ty(SSAScope::context());
}

SSAUByte operator+(const SSAUByte &a, const SSAUByte &b)
{
	return SSAUByte::from_llvm(SSAScope::builder().CreateAdd(a.v, b.v, SSAScope::hint()));
}

SSAUByte operator-(const SSAUByte &a, const SSAUByte &b)
{
	return SSAUByte::from_llvm(SSAScope::builder().CreateSub(a.v, b.v, SSAScope::hint()));
}

SSAUByte operator*(const SSAUByte &a, const SSAUByte &b)
{
	return SSAUByte::from_llvm(SSAScope::builder().CreateMul(a.v, b.v, SSAScope::hint()));
}
/*
SSAUByte operator/(const SSAUByte &a, const SSAUByte &b)
{
	return SSAScope::builder().CreateDiv(a.v, b.v);
}
*/
SSAUByte operator+(unsigned char a, const SSAUByte &b)
{
	return SSAUByte(a) + b;
}

SSAUByte operator-(unsigned char a, const SSAUByte &b)
{
	return SSAUByte(a) - b;
}

SSAUByte operator*(unsigned char a, const SSAUByte &b)
{
	return SSAUByte(a) * b;
}
/*
SSAUByte operator/(unsigned char a, const SSAUByte &b)
{
	return SSAUByte(a) / b;
}
*/
SSAUByte operator+(const SSAUByte &a, unsigned char b)
{
	return a + SSAUByte(b);
}

SSAUByte operator-(const SSAUByte &a, unsigned char b)
{
	return a - SSAUByte(b);
}

SSAUByte operator*(const SSAUByte &a, unsigned char b)
{
	return a * SSAUByte(b);
}
/*
SSAUByte operator/(const SSAUByte &a, unsigned char b)
{
	return a / SSAUByte(b);
}
*/
SSAUByte operator<<(const SSAUByte &a, unsigned char bits)
{
	return SSAUByte::from_llvm(SSAScope::builder().CreateShl(a.v, bits));
}

SSAUByte operator>>(const SSAUByte &a, unsigned char bits)
{
	return SSAUByte::from_llvm(SSAScope::builder().CreateLShr(a.v, bits));
}
