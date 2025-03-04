#include "StackStateNode.h"

#include <common/scripting/dap/RuntimeState.h>
#include <common/scripting/dap/Utilities.h>

#include <string>
#include "StackFrameStateNode.h"

namespace DebugServer
{
StackStateNode::StackStateNode(const uint32_t stackId) : m_stackId(stackId) { }

bool StackStateNode::SerializeToProtocol(dap::Thread &thread) const
{
	thread.id = m_stackId;

	std::vector<VMFrame *> frames;
	RuntimeState::GetStackFrames(m_stackId, frames);

	if (frames.empty())
	{
		thread.name = StringFormat("(%d)", thread.id);
	}
	else
	{
		const auto frame = frames.back();
		const auto name = frame->Func ? frame->Func->QualifiedName : "<unknown>";
		thread.name = StringFormat("%s (%d)", name, thread.id);
	}

	return true;
}

bool StackStateNode::GetChildNames(std::vector<std::string> &names)
{
	std::vector<VMFrame *> frames;
	RuntimeState::GetStackFrames(m_stackId, frames);

	for (size_t i = 0; i < frames.size(); i++)
	{
		names.push_back(std::to_string(i));
	}

	return true;
}

bool StackStateNode::GetChildNode(const std::string name, std::shared_ptr<StateNodeBase> &node)
{
	int level;
	if (!ParseInt(name, &level))
	{
		return false;
	}
	if (m_children.find(level) != m_children.end())
	{
		node = m_children[level];
		return true;
	}

	std::vector<VMFrame *> frames;
	if (!RuntimeState::GetStackFrames(m_stackId, frames))
	{
		return false;
	}

	if ((size_t)level >= frames.size())
	{
		return false;
	}

	m_children[level] = std::make_shared<StackFrameStateNode>(frames.at(level));
	node = m_children[level];
	return true;
}
}
