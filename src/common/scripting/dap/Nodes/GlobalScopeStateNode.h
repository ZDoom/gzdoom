#pragma once

#include <common/scripting/dap/GameInterfaces.h>
#include <dap/protocol.h>

#include "StateNodeBase.h"

namespace DebugServer
{
class GlobalScopeStateNode : public StateNodeBase, public IProtocolScopeSerializable, public IStructuredState
{
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	public:
	GlobalScopeStateNode();

	bool SerializeToProtocol(dap::Scope &scope) override;
	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
