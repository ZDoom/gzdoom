#include "DummyNode.h"

#include <utility>
namespace DebugServer
{
DummyNode::DummyNode(std::string name, std::string value, std::string type) : StateNodeNamedVariable(std::move(name)), m_value(std::move(value)), m_type(std::move(type)) { }

bool DummyNode::SerializeToProtocol(dap::Variable &variable)
{
	SetVariableName(variable);
	variable.value = m_value;
	variable.type = m_type;
	return true;
}

DummyWithChildrenNode::DummyWithChildrenNode(std::string name, std::string value, std::string type, caseless_path_map<std::shared_ptr<StateNodeBase>> children)
		: StateNodeNamedVariable(std::move(name)), m_value(std::move(value)), m_type(std::move(type)), m_children(std::move(children))
{
}

bool DummyWithChildrenNode::GetChildNames(std::vector<std::string> &names)
{
	for (const auto &child : m_children)
	{
		names.push_back(child.first);
	}
	return true;
}

bool DummyWithChildrenNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (m_children.find(name) != m_children.end())
	{
		node = m_children[name];
		return true;
	}
	return false;
}

bool DummyWithChildrenNode::SerializeToProtocol(dap::Variable &variable)
{
	SetVariableName(variable);
	variable.value = m_value;
	variable.type = m_type;
	variable.variablesReference = GetId();
	variable.namedVariables = m_children.size();
	return true;
}
}