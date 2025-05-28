#pragma once

#include <common/scripting/dap/GameInterfaces.h>

#include <dap/protocol.h>
#include "StateNodeBase.h"

namespace DebugServer
{
class StatePointerNode : public StateNodeNamedVariable, public IStructuredState
{
	const VMValue m_value;
	PClass *m_OwningType;
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	public:
	StatePointerNode(std::string name, VMValue variable, PClass *owningType);
	bool SerializeToProtocol(dap::Variable &variable) override;
	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
