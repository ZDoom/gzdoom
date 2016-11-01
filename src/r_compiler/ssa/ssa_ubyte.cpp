/*
**  SSA uint8
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
#include "ssa_ubyte.h"
#include "ssa_int.h"
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

SSAInt SSAUByte::zext_int()
{
	return SSAInt::from_llvm(SSAScope::builder().CreateZExt(v, SSAInt::llvm_type(), SSAScope::hint()));
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
