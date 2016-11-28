/*
**  SSA value
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

#include <vector>

namespace llvm { class Value; }

class SSAInt;
class SSAIndexLookup;

class SSAValue
{
public:
	SSAValue() : v(0) { }

	static SSAValue from_llvm(llvm::Value *v) { SSAValue val; val.v = v; return val; }

	SSAValue load(bool constantScopeDomain);
	void store(llvm::Value *v);

	template<typename Type>
	operator Type()
	{
		return Type::from_llvm(v);
	}

	SSAIndexLookup operator[](int index);
	SSAIndexLookup operator[](SSAInt index);

	llvm::Value *v;
};

class SSAIndexLookup
{
public:
	SSAIndexLookup() : v(0) { }

	llvm::Value *v;
	std::vector<llvm::Value *> indexes;

	SSAValue load(bool constantScopeDomain) { SSAValue value = *this; return value.load(constantScopeDomain); }
	void store(llvm::Value *v) { SSAValue value = *this; return value.store(v); }

	template<typename Type>
	operator Type()
	{
		return Type::from_llvm(v);
	}

	operator SSAValue();
	SSAIndexLookup operator[](int index);
	SSAIndexLookup operator[](SSAInt index);
};
