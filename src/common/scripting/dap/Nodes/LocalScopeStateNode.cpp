#include "LocalScopeStateNode.h"

#include "common/scripting/dap/GameInterfaces.h"
#include "common/scripting/dap/Nodes/StateNodeBase.h"
#include <common/scripting/dap/Utilities.h>
#include <common/scripting/dap/RuntimeState.h>

namespace DebugServer
{
static constexpr const char *const LOCAL = "Local";
static constexpr const char *const SELF = "self";
static constexpr const char *const INVOKER = "invoker";

LocalScopeStateNode::LocalScopeStateNode(VMFrame *stackFrame) : m_stackFrame(stackFrame) { }


std::string LocalScopeStateNode::GetLineQualifiedName(const std::string &name, int line)
{
	return name + " @ line " + std::to_string(line);
}

int LocalScopeStateNode::GetLineFromLineQualifiedName(const std::string &name)
{
	auto pos = name.find(" @ line ");
	if (pos == std::string::npos) {
		return -1;
	}
	auto lineNumberStr = name.substr(pos + 8);
	return std::stoi(lineNumberStr);
}

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
	if (m_state.m_locals.empty()) m_state = GetLocalsState(m_stackFrame);

	auto scriptFunc = dynamic_cast<VMScriptFunction *>(m_stackFrame->Func);
	std::vector<size_t> localIdx;

	for (size_t i = 0; i < m_state.m_locals.size(); i++)
	{
		auto &local = m_state.m_locals[i];
		if (scriptFunc && scriptFunc->PCToLine(m_stackFrame->PC) <= local.Line)
		{
			continue;
		}
		auto oldPos = std::find(names.begin(), names.end(), local.Name);
		auto name = local.Name;
		if (oldPos != names.end()){
			auto oldIdx = oldPos - names.begin();
			auto &oldName = names.at(oldIdx);
			auto oldLocalIdx = localIdx.at(oldIdx);
			auto oldLocalLine = m_state.m_locals.at(oldLocalIdx).Line;
			oldName = GetLineQualifiedName(local.Name, oldLocalLine);
			name = GetLineQualifiedName(local.Name, local.Line);
		}
		localIdx.push_back(i);
		names.push_back(name);
	}
	return true;
}


void LocalScopeStateNode::CacheChildren()
{
		if (m_state.m_locals.empty()) m_state = GetLocalsState(m_stackFrame);
		m_children.clear();
		PClass *invoker = nullptr;
		caseless_path_map<int> name_to_line;
		std::vector<std::pair<std::string, std::shared_ptr<StateNodeBase>>> nodes;
		if (m_stackFrame->Func->ImplicitArgs >= 2 && m_state.m_locals.size() >= 2 && m_state.m_locals[1].Name == INVOKER)
		{
			invoker = GetClassDescriptor(m_state.m_locals[1].Type);
		}
		else if (
			IsFunctionAction(m_stackFrame->Func) && m_stackFrame->Func->ImplicitArgs >= 1 && m_state.m_locals.size() >= 1
			&& m_state.m_locals[0].Name == SELF) // Try 'self' ?
		{
			invoker = GetClassDescriptor(m_state.m_locals[0].Type);
		}
		for (auto &local : m_state.m_locals)
		{
			const VMFrame * current_frame = m_stackFrame;
			auto scriptFunc = dynamic_cast<VMScriptFunction *>(m_stackFrame->Func);
			if (scriptFunc && scriptFunc->PCToLine(m_stackFrame->PC) <= local.Line)
			{
				continue;
			}
			std::string name = local.Name;
			auto node = RuntimeState::CreateNodeForVariable(name, local.Value, local.Type, current_frame, invoker);
			if (m_children.find(name) != m_children.end()){
				std::shared_ptr<IProtocolVariableSerializableWithName> oldNamedNode = std::dynamic_pointer_cast<IProtocolVariableSerializableWithName>(m_children[name]);
				std::shared_ptr<IProtocolVariableSerializableWithName> newNamedNode = std::dynamic_pointer_cast<IProtocolVariableSerializableWithName>(node);
				if (oldNamedNode && newNamedNode) {
					auto oldLine = name_to_line[name];
					auto newLine = local.Line;
					std::string oldName = GetLineQualifiedName(name, oldLine);
					std::string newName = GetLineQualifiedName(name, newLine);
					oldNamedNode->SetName(oldName);
					newNamedNode->SetName(newName);
					m_children[oldName] = m_children[name];
					m_children.erase(name);
					name_to_line.erase(name);
					name_to_line[oldName] = oldLine;
					name = newName;
				}
			}
			name_to_line[name] = local.Line;
			m_children[name] = node;
		}

}

bool LocalScopeStateNode::GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node)
{
	if (m_state.m_locals.empty())
		m_state = GetLocalsState(m_stackFrame);

	if (m_children.empty())
	{
		CacheChildren();
	}
	if (m_children.find(name) != m_children.end())
	{
		node = m_children[name];
		return true;
	}
	return false;
}
}
