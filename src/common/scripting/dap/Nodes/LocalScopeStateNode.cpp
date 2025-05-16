#include "LocalScopeStateNode.h"

#include "StructStateNode.h"
#include "common/scripting/dap/GameInterfaces.h"
#include "types.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>

namespace DebugServer
{
static const char *const LOCAL = "Local";
static const char *const SELF = "self";
static const char *const INVOKER = "invoker";
static const char *const STATE_POINTER = "state_pointer";

LocalScopeStateNode::LocalScopeStateNode(VMFrame *stackFrame) : m_stackFrame(stackFrame) { }

bool LocalScopeStateNode::SerializeToProtocol(dap::Scope &scope)
{
	scope.name = LOCAL;
	scope.expensive = false;

	scope.variablesReference = GetId();
	scope.presentationHint = "locals";
	std::vector<std::string> childNames;
	GetChildNames(childNames);

	scope.namedVariables = childNames.size();
	scope.indexedVariables = 0;

	return true;
}


bool LocalScopeStateNode::GetChildNames(std::vector<std::string> &names)
{
	if (m_state.m_locals.empty())
		m_state = GetLocalsState(m_stackFrame);

	auto scriptFunc = dynamic_cast<VMScriptFunction *>(m_stackFrame->Func);
	for (auto &local : m_state.m_locals)
	{
		if (scriptFunc && scriptFunc->PCToLine(m_stackFrame->PC) <= local.Line)
		{
			continue;
		}
		names.push_back(local.Name);
	}
	return true;
}



bool LocalScopeStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (m_state.m_locals.empty())
		m_state = GetLocalsState(m_stackFrame);

	if (m_children.empty())
	{
		for (auto &local : m_state.m_locals)
		{
			const VMFrame * current_frame = m_stackFrame;
			m_children[local.Name] = RuntimeState::CreateNodeForVariable(local.Name, local.Value, local.Type, current_frame);
		}
	}
	if (m_children.find(name) != m_children.end())
	{
		node = m_children[name];
		return true;
	}
	return false;
}
}
