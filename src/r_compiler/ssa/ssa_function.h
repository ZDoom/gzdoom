
#pragma once

#include <string>
#include <vector>

namespace llvm { class Value; }
namespace llvm { class Type; }
namespace llvm { class Function; }

class SSAInt;
class SSAValue;

class SSAFunction
{
public:
	SSAFunction(const std::string name);
	void set_return_type(llvm::Type *type);
	void add_parameter(llvm::Type *type);
	void create_public();
	void create_private();
	SSAValue parameter(int index);

	llvm::Function *func;

private:
	std::string name;
	llvm::Type *return_type;
	std::vector<llvm::Type *> parameters;
};
