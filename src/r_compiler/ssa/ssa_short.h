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

#pragma once

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAFloat;
class SSAInt;

class SSAShort
{
public:
	SSAShort();
	explicit SSAShort(int constant);
	SSAShort(SSAFloat f);
	explicit SSAShort(llvm::Value *v);
	static SSAShort from_llvm(llvm::Value *v) { return SSAShort(v); }
	static llvm::Type *llvm_type();

	SSAInt zext_int();

	llvm::Value *v;
};

SSAShort operator+(const SSAShort &a, const SSAShort &b);
SSAShort operator-(const SSAShort &a, const SSAShort &b);
SSAShort operator*(const SSAShort &a, const SSAShort &b);
SSAShort operator/(const SSAShort &a, const SSAShort &b);
SSAShort operator%(const SSAShort &a, const SSAShort &b);

SSAShort operator+(int a, const SSAShort &b);
SSAShort operator-(int a, const SSAShort &b);
SSAShort operator*(int a, const SSAShort &b);
SSAShort operator/(int a, const SSAShort &b);
SSAShort operator%(int a, const SSAShort &b);

SSAShort operator+(const SSAShort &a, int b);
SSAShort operator-(const SSAShort &a, int b);
SSAShort operator*(const SSAShort &a, int b);
SSAShort operator/(const SSAShort &a, int b);
SSAShort operator%(const SSAShort &a, int b);

SSAShort operator<<(const SSAShort &a, int bits);
SSAShort operator>>(const SSAShort &a, int bits);
SSAShort operator<<(const SSAShort &a, const SSAInt &bits);
SSAShort operator>>(const SSAShort &a, const SSAInt &bits);

SSAShort operator&(const SSAShort &a, int b);
SSAShort operator&(const SSAShort &a, const SSAShort &b);
SSAShort operator|(const SSAShort &a, int b);
SSAShort operator|(const SSAShort &a, const SSAShort &b);
