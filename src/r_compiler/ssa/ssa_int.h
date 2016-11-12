/*
**  SSA int32
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

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAFloat;

class SSAInt
{
public:
	SSAInt();
	explicit SSAInt(int constant);
	SSAInt(SSAFloat f, bool uint);
	explicit SSAInt(llvm::Value *v);
	static SSAInt from_llvm(llvm::Value *v) { return SSAInt(v); }
	static llvm::Type *llvm_type();

	static SSAInt MIN(SSAInt a, SSAInt b);
	static SSAInt MAX(SSAInt a, SSAInt b);
	static SSAInt clamp(SSAInt a, SSAInt b, SSAInt c);

	SSAInt add(SSAInt b, bool no_unsigned_wrap, bool no_signed_wrap);
	SSAInt ashr(int bits);

	llvm::Value *v;
};

SSAInt operator+(const SSAInt &a, const SSAInt &b);
SSAInt operator-(const SSAInt &a, const SSAInt &b);
SSAInt operator*(const SSAInt &a, const SSAInt &b);
SSAInt operator/(const SSAInt &a, const SSAInt &b);
SSAInt operator%(const SSAInt &a, const SSAInt &b);

SSAInt operator+(int a, const SSAInt &b);
SSAInt operator-(int a, const SSAInt &b);
SSAInt operator*(int a, const SSAInt &b);
SSAInt operator/(int a, const SSAInt &b);
SSAInt operator%(int a, const SSAInt &b);

SSAInt operator+(const SSAInt &a, int b);
SSAInt operator-(const SSAInt &a, int b);
SSAInt operator*(const SSAInt &a, int b);
SSAInt operator/(const SSAInt &a, int b);
SSAInt operator%(const SSAInt &a, int b);

SSAInt operator<<(const SSAInt &a, int bits);
SSAInt operator>>(const SSAInt &a, int bits);
SSAInt operator<<(const SSAInt &a, const SSAInt &bits);
SSAInt operator>>(const SSAInt &a, const SSAInt &bits);

SSAInt operator&(const SSAInt &a, int b);
SSAInt operator&(const SSAInt &a, const SSAInt &b);
SSAInt operator|(const SSAInt &a, int b);
SSAInt operator|(const SSAInt &a, const SSAInt &b);
SSAInt operator~(const SSAInt &a);
