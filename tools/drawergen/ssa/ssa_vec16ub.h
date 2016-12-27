/*
**  SSA vec16 uint8
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

class SSAVec8s;
class SSAVec4i;

class SSAVec16ub
{
public:
	SSAVec16ub();
	explicit SSAVec16ub(unsigned char constant);
	explicit SSAVec16ub(
		unsigned char constant0, unsigned char constant1, unsigned char constant2, unsigned char constant3, unsigned char constant4, unsigned char constant5, unsigned char constant6, unsigned char constant7,
		unsigned char constant8, unsigned char constant9, unsigned char constant10, unsigned char constant11, unsigned char constant12, unsigned char constant13, unsigned char constant14, unsigned char constant15);
	explicit SSAVec16ub(llvm::Value *v);
	SSAVec16ub(SSAVec8s s0, SSAVec8s s1);
	static SSAVec16ub from_llvm(llvm::Value *v) { return SSAVec16ub(v); }
	static llvm::Type *llvm_type();
	static SSAVec16ub bitcast(SSAVec4i i32);
	static SSAVec16ub shuffle(const SSAVec16ub &i0, int index0, int index1, int index2, int index3, int index4, int index5, int index6, int index7, int index8, int index9, int index10, int index11, int index12, int index13, int index14, int index15);
	static SSAVec16ub shuffle(const SSAVec16ub &i0, const SSAVec16ub &i1, int index0, int index1, int index2, int index3, int index4, int index5, int index6, int index7, int index8, int index9, int index10, int index11, int index12, int index13, int index14, int index15);

	llvm::Value *v;
};

SSAVec16ub operator+(const SSAVec16ub &a, const SSAVec16ub &b);
SSAVec16ub operator-(const SSAVec16ub &a, const SSAVec16ub &b);
SSAVec16ub operator*(const SSAVec16ub &a, const SSAVec16ub &b);
SSAVec16ub operator/(const SSAVec16ub &a, const SSAVec16ub &b);

SSAVec16ub operator+(unsigned char a, const SSAVec16ub &b);
SSAVec16ub operator-(unsigned char a, const SSAVec16ub &b);
SSAVec16ub operator*(unsigned char a, const SSAVec16ub &b);
SSAVec16ub operator/(unsigned char a, const SSAVec16ub &b);

SSAVec16ub operator+(const SSAVec16ub &a, unsigned char b);
SSAVec16ub operator-(const SSAVec16ub &a, unsigned char b);
SSAVec16ub operator*(const SSAVec16ub &a, unsigned char b);
SSAVec16ub operator/(const SSAVec16ub &a, unsigned char b);
