#pragma once

#include <common/scripting/dap/GameInterfaces.h>
#include <dap/protocol.h>

#include "StateNodeBase.h"

namespace DebugServer
{
class LocalScopeStateNode : public StateNodeBase, public IProtocolScopeSerializable, public IStructuredState
{
	VMFrame *m_stackFrame;
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	public:
	LocalScopeStateNode(VMFrame *stackFrame);

	bool SerializeToProtocol(dap::Scope &scope) override;
	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
