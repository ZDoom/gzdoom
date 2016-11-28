/*
**  SSA vec8 int16
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

class SSAVec4i;
class SSAVec16ub;

class SSAVec8s
{
public:
	SSAVec8s();
	explicit SSAVec8s(short constant);
	explicit SSAVec8s(short constant0, short constant1, short constant2, short constant3, short constant4, short constant5, short constant6, short constant7);
	explicit SSAVec8s(llvm::Value *v);
	SSAVec8s(SSAVec4i i0, SSAVec4i i1);
	static SSAVec8s bitcast(SSAVec16ub i8);
	static SSAVec8s shuffle(const SSAVec8s &i0, int index0, int index1, int index2, int index3, int index4, int index5, int index6, int index7);
	static SSAVec8s shuffle(const SSAVec8s &i0, const SSAVec8s &i1, int index0, int index1, int index2, int index3, int index4, int index5, int index6, int index7);
	static SSAVec8s extendhi(SSAVec16ub a);
	static SSAVec8s extendlo(SSAVec16ub a);
	//static SSAVec8s min_sse2(SSAVec8s a, SSAVec8s b);
	//static SSAVec8s max_sse2(SSAVec8s a, SSAVec8s b);
	static SSAVec8s mulhi(SSAVec8s a, SSAVec8s b);
	static SSAVec8s from_llvm(llvm::Value *v) { return SSAVec8s(v); }
	static llvm::Type *llvm_type();

	llvm::Value *v;
};

SSAVec8s operator+(const SSAVec8s &a, const SSAVec8s &b);
SSAVec8s operator-(const SSAVec8s &a, const SSAVec8s &b);
SSAVec8s operator*(const SSAVec8s &a, const SSAVec8s &b);
SSAVec8s operator/(const SSAVec8s &a, const SSAVec8s &b);

SSAVec8s operator+(short a, const SSAVec8s &b);
SSAVec8s operator-(short a, const SSAVec8s &b);
SSAVec8s operator*(short a, const SSAVec8s &b);
SSAVec8s operator/(short a, const SSAVec8s &b);

SSAVec8s operator+(const SSAVec8s &a, short b);
SSAVec8s operator-(const SSAVec8s &a, short b);
SSAVec8s operator*(const SSAVec8s &a, short b);
SSAVec8s operator/(const SSAVec8s &a, short b);

SSAVec8s operator<<(const SSAVec8s &a, int bits);
SSAVec8s operator>>(const SSAVec8s &a, int bits);
