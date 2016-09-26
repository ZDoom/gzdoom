
#pragma once

#include "ssa_int.h"
#include "ssa_vec4i.h"

namespace llvm { class Value; }
namespace llvm { class Type; }

class SSAVec4iPtr
{
public:
	SSAVec4iPtr();
	explicit SSAVec4iPtr(llvm::Value *v);
	static SSAVec4iPtr from_llvm(llvm::Value *v) { return SSAVec4iPtr(v); }
	static llvm::Type *llvm_type();
	SSAVec4iPtr operator[](SSAInt index) const;
	SSAVec4i load() const;
	SSAVec4i load_unaligned() const;
	void store(const SSAVec4i &new_value);
	void store_unaligned(const SSAVec4i &new_value);

	llvm::Value *v;
};
