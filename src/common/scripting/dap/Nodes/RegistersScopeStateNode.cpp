#include "RegistersScopeStateNode.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>
#include "ValueStateNode.h"

static const char *const PARAMS = "Params";
static const char *const INTS = "Ints";
static const char *const FLOATS = "Floats";
static const char *const STRINGS = "Strings";
static const char *const POINTERS = "Pointers";

namespace DebugServer
{
RegistersScopeStateNode::RegistersScopeStateNode(VMFrame *stackFrame) : m_stackFrame(stackFrame) { }

bool RegistersScopeStateNode::SerializeToProtocol(dap::Scope &scope)
{
	scope.name = "Registers";
	scope.expensive = false;
	scope.presentationHint = "registers";
	scope.variablesReference = GetId();

	std::vector<std::string> childNames;
	GetChildNames(childNames);

	scope.namedVariables = childNames.size();
	scope.indexedVariables = 0;

	return true;
}

bool RegistersScopeStateNode::GetChildNames(std::vector<std::string> &names)
{
	names.emplace_back(PARAMS);
	names.emplace_back(INTS);
	names.emplace_back(FLOATS);
	names.emplace_back(STRINGS);
	names.emplace_back(POINTERS);
	return true;
}

bool RegistersScopeStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (CaseInsensitiveEquals(name, PARAMS))
	{
		node = std::make_shared<ParamsRegistersNode>(name, m_stackFrame);
		return true;
	}
	else if (CaseInsensitiveEquals(name, INTS))
	{
		node = std::make_shared<IntRegistersNode>(name, m_stackFrame);
		return true;
	}
	else if (CaseInsensitiveEquals(name, FLOATS))
	{
		node = std::make_shared<FloatRegistersNode>(name, m_stackFrame);
		return true;
	}
	else if (CaseInsensitiveEquals(name, STRINGS))
	{
		node = std::make_shared<StringRegistersNode>(name, m_stackFrame);
		return true;
	}
	else if (CaseInsensitiveEquals(name, POINTERS))
	{
		node = std::make_shared<PointerRegistersNode>(name, m_stackFrame);
		return true;
	}

	return false;
}

bool RegistersNode::SerializeToProtocol(dap::Variable &variable)
{

	variable.name = m_name;
	variable.type = m_name + " Registers";
	// value will be the max number of registers
	auto max_num_reg = GetNumberOfRegisters();
	variable.value = m_name + "[" + std::to_string(max_num_reg) + "]";
	variable.indexedVariables = max_num_reg;
	variable.variablesReference = GetId();
	return true;
}

bool RegistersNode::GetChildNames(std::vector<std::string> &names)
{
	for (int i = 0; i < GetNumberOfRegisters(); i++)
	{
		names.push_back(std::to_string(i));
	}
	return true;
}

bool RegistersNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	int index = std::stoi(name);
	if (index < 0 || index >= GetNumberOfRegisters())
	{
		return false;
	}
	VMValue val = GetRegisterValue(index);
	node = RuntimeState::CreateNodeForVariable(name, val, GetRegisterType(index));
	return true;
}

int PointerRegistersNode::GetNumberOfRegisters() { return m_stackFrame->NumRegA; }

VMValue PointerRegistersNode::GetRegisterValue(int index) { return m_stackFrame->GetRegA()[index]; }

PType *PointerRegistersNode::GetRegisterType(int index) { return TypeVoidPtr; }

int StringRegistersNode::GetNumberOfRegisters() { return m_stackFrame->NumRegS; }

VMValue StringRegistersNode::GetRegisterValue(int index) { return {&m_stackFrame->GetRegS()[index]}; }

PType *StringRegistersNode::GetRegisterType(int index) { return TypeString; }

int FloatRegistersNode::GetNumberOfRegisters() { return m_stackFrame->NumRegF; }

VMValue FloatRegistersNode::GetRegisterValue(int index) { return m_stackFrame->GetRegF()[index]; }

PType *FloatRegistersNode::GetRegisterType(int index) { return TypeFloat64; }

int IntRegistersNode::GetNumberOfRegisters() { return m_stackFrame->NumRegD; }

VMValue IntRegistersNode::GetRegisterValue(int index) { return m_stackFrame->GetRegD()[index]; }

PType *IntRegistersNode::GetRegisterType(int index) { return TypeSInt32; }

int ParamsRegistersNode::GetNumberOfRegisters() { return m_stackFrame->MaxParam; }

VMValue ParamsRegistersNode::GetRegisterValue(int index) { return m_stackFrame->GetParam()[index]; }

PType *ParamsRegistersNode::GetRegisterType(int index)
{
	// TODO: Is it possible to get the type of parameters?
	return TypeVoidPtr;
}

bool PointerRegistersNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	int index = std::stoi(name);
	if (index < 0 || index >= GetNumberOfRegisters())
	{
		return false;
	}
	VMValue val = GetRegisterValue(index);
	node = RuntimeState::CreateNodeForVariable(name, val, GetRegisterType(index));
	return true;
}

bool ParamsRegistersNode::SerializeToProtocol(dap::Variable &variable)
{

	variable.name = PARAMS;
	variable.type = "Parameter Registers";
	// value will be the max number of registers
	auto max_num_reg = GetNumberOfRegisters();
	variable.value = "Params - Max: " + std::to_string(max_num_reg) + ", In Use: " + std::to_string(m_stackFrame->NumParam);
	variable.indexedVariables = max_num_reg;
	variable.variablesReference = GetId();
	return true;
}

RegistersNode::RegistersNode(std::string name, VMFrame *stackFrame) : m_stackFrame(stackFrame), m_name(name) { }
}
