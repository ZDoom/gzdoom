/*
**  SSA uint8 pointer
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

#include "ssa_ubyte.h"
#include "ssa_int.h"
#include "ssa_vec4i.h"
#include "ssa_vec8s.h"
#include "ssa_vec16ub.h"

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAUBytePtr
{
public:
	SSAUBytePtr();
	explicit SSAUBytePtr(llvm::Value *v);
	static SSAUBytePtr from_llvm(llvm::Value *v) { return SSAUBytePtr(v); }
	static llvm::Type *llvm_type();
	SSAUBytePtr operator[](SSAInt index) const;
	SSAUBytePtr operator[](int index) const { return (*this)[SSAInt(index)]; }
	SSAUByte load(bool constantScopeDomain) const;
	SSAVec4i load_vec4ub(bool constantScopeDomain) const;
	SSAVec16ub load_vec16ub(bool constantScopeDomain) const;
	SSAVec16ub load_unaligned_vec16ub(bool constantScopeDomain) const;
	void store(const SSAUByte &new_value);
	void store_vec4ub(const SSAVec4i &new_value);
	void store_vec16ub(const SSAVec16ub &new_value);
	void store_unaligned_vec16ub(const SSAVec16ub &new_value);

	llvm::Value *v;
};
