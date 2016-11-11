/*
**  SSA float32
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

class SSAInt;

class SSAFloat
{
public:
	SSAFloat();
	SSAFloat(SSAInt i);
	explicit SSAFloat(float constant);
	explicit SSAFloat(llvm::Value *v);
	static SSAFloat from_llvm(llvm::Value *v) { return SSAFloat(v); }
	static llvm::Type *llvm_type();
	static SSAFloat sqrt(SSAFloat f);
	static SSAFloat sin(SSAFloat val);
	static SSAFloat cos(SSAFloat val);
	static SSAFloat pow(SSAFloat val, SSAFloat power);
	static SSAFloat exp(SSAFloat val);
	static SSAFloat log(SSAFloat val);
	static SSAFloat fma(SSAFloat a, SSAFloat b, SSAFloat c);
	static SSAFloat round(SSAFloat val);
	static SSAFloat floor(SSAFloat val);
	static SSAFloat MIN(SSAFloat a, SSAFloat b);
	static SSAFloat MAX(SSAFloat a, SSAFloat b);
	static SSAFloat clamp(SSAFloat a, SSAFloat b, SSAFloat c);

	llvm::Value *v;
};

SSAFloat operator+(const SSAFloat &a, const SSAFloat &b);
SSAFloat operator-(const SSAFloat &a, const SSAFloat &b);
SSAFloat operator*(const SSAFloat &a, const SSAFloat &b);
SSAFloat operator/(const SSAFloat &a, const SSAFloat &b);

SSAFloat operator+(float a, const SSAFloat &b);
SSAFloat operator-(float a, const SSAFloat &b);
SSAFloat operator*(float a, const SSAFloat &b);
SSAFloat operator/(float a, const SSAFloat &b);

SSAFloat operator+(const SSAFloat &a, float b);
SSAFloat operator-(const SSAFloat &a, float b);
SSAFloat operator*(const SSAFloat &a, float b);
SSAFloat operator/(const SSAFloat &a, float b);
