/*
**  SSA int16
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "r_compiler/llvm_include.h"
#include "ssa_short.h"
#include "ssa_float.h"
#include "ssa_int.h"
#include "ssa_scope.h"

SSAShort::SSAShort()
: v(0)
{
}

SSAShort::SSAShort(int constant)
: v(0)
{
	v = llvm::ConstantInt::get(SSAScope::context(), llvm::APInt(16, constant, true));
}

SSAShort::SSAShort(SSAFloat f)
: v(0)
{
	v = SSAScope::builder().CreateFPToSI(f.v, llvm::Type::getInt16Ty(SSAScope::context()), SSAScope::hint());
}

SSAShort::SSAShort(llvm::Value *v)
: v(v)
{
}

llvm::Type *SSAShort::llvm_type()
{
	return llvm::Type::getInt16Ty(SSAScope::context());
}

SSAInt SSAShort::zext_int()
{
	return SSAInt::from_llvm(SSAScope::builder().CreateZExt(v, SSAInt::llvm_type(), SSAScope::hint()));
}

SSAShort operator+(const SSAShort &a, const SSAShort &b)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateAdd(a.v, b.v, SSAScope::hint()));
}

SSAShort operator-(const SSAShort &a, const SSAShort &b)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateSub(a.v, b.v, SSAScope::hint()));
}

SSAShort operator*(const SSAShort &a, const SSAShort &b)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateMul(a.v, b.v, SSAScope::hint()));
}

SSAShort operator/(const SSAShort &a, const SSAShort &b)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateSDiv(a.v, b.v, SSAScope::hint()));
}

SSAShort operator%(const SSAShort &a, const SSAShort &b)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateSRem(a.v, b.v, SSAScope::hint()));
}

SSAShort operator+(int a, const SSAShort &b)
{
	return SSAShort(a) + b;
}

SSAShort operator-(int a, const SSAShort &b)
{
	return SSAShort(a) - b;
}

SSAShort operator*(int a, const SSAShort &b)
{
	return SSAShort(a) * b;
}

SSAShort operator/(int a, const SSAShort &b)
{
	return SSAShort(a) / b;
}

SSAShort operator%(int a, const SSAShort &b)
{
	return SSAShort(a) % b;
}

SSAShort operator+(const SSAShort &a, int b)
{
	return a + SSAShort(b);
}

SSAShort operator-(const SSAShort &a, int b)
{
	return a - SSAShort(b);
}

SSAShort operator*(const SSAShort &a, int b)
{
	return a * SSAShort(b);
}

SSAShort operator/(const SSAShort &a, int b)
{
	return a / SSAShort(b);
}

SSAShort operator%(const SSAShort &a, int b)
{
	return a % SSAShort(b);
}

SSAShort operator<<(const SSAShort &a, int bits)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateShl(a.v, bits, SSAScope::hint()));
}

SSAShort operator>>(const SSAShort &a, int bits)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateLShr(a.v, bits, SSAScope::hint()));
}

SSAShort operator<<(const SSAShort &a, const SSAInt &bits)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateShl(a.v, bits.v, SSAScope::hint()));
}

SSAShort operator>>(const SSAShort &a, const SSAInt &bits)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateLShr(a.v, bits.v, SSAScope::hint()));
}

SSAShort operator&(const SSAShort &a, int b)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateAnd(a.v, b, SSAScope::hint()));
}

SSAShort operator&(const SSAShort &a, const SSAShort &b)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateAnd(a.v, b.v, SSAScope::hint()));
}

SSAShort operator|(const SSAShort &a, int b)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateOr(a.v, b, SSAScope::hint()));
}

SSAShort operator|(const SSAShort &a, const SSAShort &b)
{
	return SSAShort::from_llvm(SSAScope::builder().CreateOr(a.v, b.v, SSAScope::hint()));
}
