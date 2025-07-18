#pragma once

#include <common/scripting/dap/GameInterfaces.h>
#include <dap/protocol.h>

#include "StateNodeBase.h"

namespace DebugServer
{
class LocalScopeStateNode : public StateNodeBase, public IProtocolScopeSerializable, public IStructuredState
{
	VMFrame *m_stackFrame;
	FrameLocalsState m_state;
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;

	void CacheChildren();
	public:
	LocalScopeStateNode(VMFrame *stackFrame);

	static std::string GetLineQualifiedName(const std::string &name, int line);
	static int GetLineFromLineQualifiedName(const std::string &name);

	bool SerializeToProtocol(dap::Scope &scope) override;
	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
