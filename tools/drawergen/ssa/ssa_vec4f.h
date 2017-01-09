/*
**  SSA vec4 float
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
class SSAFloat;
class SSAInt;

class SSAVec4f
{
public:
	SSAVec4f();
	explicit SSAVec4f(float constant);
	explicit SSAVec4f(float constant0, float constant1, float constant2, float constant3);
	SSAVec4f(SSAFloat f);
	SSAVec4f(SSAFloat f0, SSAFloat f1, SSAFloat f2, SSAFloat f3);
	explicit SSAVec4f(llvm::Value *v);
	SSAVec4f(SSAVec4i i32);
	SSAFloat operator[](SSAInt index) const;
	SSAFloat operator[](int index) const;
	static SSAVec4f insert_element(SSAVec4f vec4f, SSAFloat value, int index);
	static SSAVec4f bitcast(SSAVec4i i32);
	static void transpose(SSAVec4f &row0, SSAVec4f &row1, SSAVec4f &row2, SSAVec4f &row3);
	static SSAVec4f shuffle(const SSAVec4f &f0, int index0, int index1, int index2, int index3);
	static SSAVec4f shuffle(const SSAVec4f &f0, const SSAVec4f &f1, int index0, int index1, int index2, int index3);
	static SSAVec4f from_llvm(llvm::Value *v) { return SSAVec4f(v); }
	static llvm::Type *llvm_type();

	llvm::Value *v;

private:
	static SSAVec4f shuffle(const SSAVec4f &f0, const SSAVec4f &f1, int mask);
};

SSAVec4f operator+(const SSAVec4f &a, const SSAVec4f &b);
SSAVec4f operator-(const SSAVec4f &a, const SSAVec4f &b);
SSAVec4f operator*(const SSAVec4f &a, const SSAVec4f &b);
SSAVec4f operator/(const SSAVec4f &a, const SSAVec4f &b);

SSAVec4f operator+(float a, const SSAVec4f &b);
SSAVec4f operator-(float a, const SSAVec4f &b);
SSAVec4f operator*(float a, const SSAVec4f &b);
SSAVec4f operator/(float a, const SSAVec4f &b);

SSAVec4f operator+(const SSAVec4f &a, float b);
SSAVec4f operator-(const SSAVec4f &a, float b);
SSAVec4f operator*(const SSAVec4f &a, float b);
SSAVec4f operator/(const SSAVec4f &a, float b);
