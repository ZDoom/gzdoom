/*
**  SSA boolean
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
#include "ssa_bool.h"
#include "ssa_ubyte.h"
#include "ssa_value.h"
#include "ssa_scope.h"

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

SSAInt SSABool::zext_int()
{
	return SSAInt::from_llvm(SSAScope::builder().CreateZExt(v, SSAInt::llvm_type(), SSAScope::hint()));
}

SSAInt SSABool::select(SSAInt a, SSAInt b)
{
	return SSAValue::from_llvm(SSAScope::builder().CreateSelect(v, a.v, b.v, SSAScope::hint()));
}

SSAUByte SSABool::select(SSAUByte a, SSAUByte b)
{
	return SSAValue::from_llvm(SSAScope::builder().CreateSelect(v, a.v, b.v, SSAScope::hint()));
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

SSABool operator<(const SSAUByte &a, const SSAUByte &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateICmpSLT(a.v, b.v, SSAScope::hint()));
}

SSABool operator<=(const SSAUByte &a, const SSAUByte &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateICmpSLE(a.v, b.v, SSAScope::hint()));
}

SSABool operator==(const SSAUByte &a, const SSAUByte &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateICmpEQ(a.v, b.v, SSAScope::hint()));
}

SSABool operator>=(const SSAUByte &a, const SSAUByte &b)
{
	return SSABool::from_llvm(SSAScope::builder().CreateICmpSGE(a.v, b.v, SSAScope::hint()));
}

SSABool operator>(const SSAUByte &a, const SSAUByte &b)
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
