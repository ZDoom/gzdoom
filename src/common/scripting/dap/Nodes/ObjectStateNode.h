#pragma once
#include <common/scripting/dap/GameInterfaces.h>
#include <dap/protocol.h>

#include "StateNodeBase.h"
#include <map>

namespace DebugServer
{
class ObjectStateNode : public StateNodeNamedVariable, public IStructuredState
{
	bool m_subView;

	const VMValue m_value;
	PType *m_ClassType;
	std::vector<std::string> m_cachedNames; // to ensure proper order of children
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_children;
	caseless_path_map<std::shared_ptr<StateNodeBase>> m_virtualChildren;
	PType *m_VMType = nullptr;
	public:
	ObjectStateNode(const std::string &name, VMValue value, PType *asClass, bool subView = false);

	bool SerializeToProtocol(dap::Variable &variable) override;

	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
	void Reset();
	bool IsVirtualStructure() override { return m_subView; }
	caseless_path_map<std::shared_ptr<StateNodeBase>> GetVirtualContainerChildren() override;
};
}
