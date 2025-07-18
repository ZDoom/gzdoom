#pragma once

#include <common/scripting/dap/GameInterfaces.h>

#include <dap/protocol.h>
#include "StateNodeBase.h"

namespace DebugServer
{
class DummyNode : public StateNodeNamedVariable
{
	const std::string m_value;
	const std::string m_type;
	public:
	DummyNode(std::string name, std::string value, std::string type);
	bool SerializeToProtocol(dap::Variable &variable) override;
};

class DummyWithChildrenNode : public StateNodeNamedVariable, public IStructuredState
{
	const std::string m_value;
	const std::string m_type;
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	public:
	DummyWithChildrenNode(std::string name, std::string value, std::string type, caseless_path_map<std::shared_ptr<StateNodeBase>> children);
	bool SerializeToProtocol(dap::Variable &variable) override;
	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
