/*
**  LLVM function
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
