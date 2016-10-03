
#pragma once

#include <vector>

namespace llvm { class Type; }

class SSAStructType
{
public:
	void add_parameter(llvm::Type *type);
	llvm::Type *llvm_type();
	llvm::Type *llvm_type_packed();

private:
	std::vector<llvm::Type *> elements;
};
