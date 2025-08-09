#include <string>

#include "StackFrameStateNode.h"
#include "StackStateNode.h"
#include "common/scripting/dap/RuntimeState.h"
#include "common/scripting/dap/Utilities.h"

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
		thread.name = StringFormat("(%ld)",  static_cast<int64_t>(thread.id));
	}
	else
	{
		const auto frame = frames.back();
		const auto name = frame->Func ? frame->Func->PrintableName : "<unknown>";
		thread.name = StringFormat("%s (%ld)", name, static_cast<int64_t>(thread.id));
	}

	return true;
}

bool StackStateNode::GetChildNames(std::vector<std::string> &names)
{
	if (!m_children.empty())
	{
		for (const auto &child : m_children)
		{
			names.push_back(std::to_string(child.first));
		}
		return true;
	}
	std::vector<VMFrame *> frames;
	RuntimeState::GetStackFrames(m_stackId, frames);

	size_t frameNum = 0;
	for (size_t i = 0; i < frames.size(); i++)
	{
		if (PCIsAtNativeCall(frames.at(i)))
		{
			names.push_back(std::to_string(frameNum));
			m_children[frameNum] = std::make_shared<StackFrameStateNode>(GetCalledFunction(frames.at(i)), frames.at(i));
			frameNum++;
		}
		names.push_back(std::to_string(frameNum));
		m_children[frameNum] = std::make_shared<StackFrameStateNode>(frames.at(i));
		frameNum++;
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
	if (m_children.empty())
	{
		std::vector<std::string> names;
		GetChildNames(names);
	}
	if (m_children.find(level) != m_children.end())
	{
		node = m_children[level];
		return true;
	}
	return false;
}
}
