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

#pragma once

#include "ssa_int.h"
#include "ssa_ubyte.h"
#include "ssa_float.h"

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAVec4i;

class SSABool
{
public:
	SSABool();
	explicit SSABool(bool constant);
	explicit SSABool(llvm::Value *v);
	static SSABool from_llvm(llvm::Value *v) { return SSABool(v); }
	static llvm::Type *llvm_type();

	SSAInt zext_int();
	SSAInt select(SSAInt a, SSAInt b);
	SSAUByte select(SSAUByte a, SSAUByte b);
	SSAVec4i select(SSAVec4i a, SSAVec4i b);

	static SSABool compare_uge(const SSAUByte &a, const SSAUByte &b);

	llvm::Value *v;
};

SSABool operator&&(const SSABool &a, const SSABool &b);
SSABool operator||(const SSABool &a, const SSABool &b);

SSABool operator!(const SSABool &a);

SSABool operator<(const SSAInt &a, const SSAInt &b);
SSABool operator<=(const SSAInt &a, const SSAInt &b);
SSABool operator==(const SSAInt &a, const SSAInt &b);
SSABool operator>=(const SSAInt &a, const SSAInt &b);
SSABool operator>(const SSAInt &a, const SSAInt &b);

SSABool operator<(const SSAUByte &a, const SSAUByte &b);
SSABool operator<=(const SSAUByte &a, const SSAUByte &b);
SSABool operator==(const SSAUByte &a, const SSAUByte &b);
SSABool operator>=(const SSAUByte &a, const SSAUByte &b);
SSABool operator>(const SSAUByte &a, const SSAUByte &b);

SSABool operator<(const SSAFloat &a, const SSAFloat &b);
SSABool operator<=(const SSAFloat &a, const SSAFloat &b);
SSABool operator==(const SSAFloat &a, const SSAFloat &b);
SSABool operator>=(const SSAFloat &a, const SSAFloat &b);
SSABool operator>(const SSAFloat &a, const SSAFloat &b);
