#include "RuntimeState.h"
#include "Utilities.h"
#include "Nodes/StateNodeBase.h"

#include "Nodes/StackStateNode.h"
#include "Nodes/ObjectStateNode.h"
#include "Nodes/StructStateNode.h"
#include "Nodes/ArrayStateNode.h"
#include "Nodes/ValueStateNode.h"
#include "common/scripting/dap/GameInterfaces.h"
#include "vm.h"
#include <common/scripting/dap/Nodes/StatePointerNode.h>

namespace DebugServer
{
VMFrameStack *RuntimeState::m_GlobalVMStack = nullptr;

RuntimeState::RuntimeState(const std::shared_ptr<IdProvider> &idProvider)
{
	m_paths = std::make_unique<IdMap<std::string>>(idProvider);
	Reset();
}

bool RuntimeState::ResolveStateByPath(const std::string requestedPath, std::shared_ptr<StateNodeBase> &node)
{
	const auto path = ToLowerCopy(requestedPath);

	auto elements = Split(path, ".");

	if (elements.empty())
	{
		return false;
	}

	const auto stackIdElement = elements.at(0);
	int stackId;
	if (!ParseInt(stackIdElement, &stackId))
	{
		return false;
	}

	std::vector<std::string> currentPathElements;
	currentPathElements.push_back(stackIdElement);

	elements.erase(elements.begin());

	std::shared_ptr<StateNodeBase> currentNode;
	if (stackId == 1)
	{
		currentNode = m_root;
	}
	else
	{
		currentNode = std::make_shared<StackStateNode>(stackId);
	}
	while (!elements.empty() && currentNode)
	{
		auto structured = dynamic_cast<IStructuredState *>(currentNode.get());

		if (structured)
		{
			std::vector<std::string> childNames;
			structured->GetChildNames(childNames);

			for (auto childName : childNames)
			{
				ToLower(childName);
				auto discoveredElements(currentPathElements);
				discoveredElements.push_back(childName);

				const auto discoveredPath = Join(discoveredElements, ".");

				uint32_t addedId;
				m_paths->AddOrGetExisting(discoveredPath, addedId);
			}
		}

		const auto currentName = elements.at(0);
		currentPathElements.push_back(currentName);

		if (structured && !structured->GetChildNode(currentName, currentNode))
		{
			currentNode = nullptr;
			break;
		}

		elements.erase(elements.begin());
	}

	if (!currentNode)
	{
		return false;
	}

	node = currentNode;

	if (currentPathElements.size() > 1)
	{
		uint32_t id;
		m_paths->AddOrGetExisting(path, id);
		node->SetId(id);
	}
	else
	{
		node->SetId(stackId);
	}

	return true;
}

bool RuntimeState::ResolveStateById(const uint32_t id, std::shared_ptr<StateNodeBase> &node)
{
	std::string path;

	if (m_paths->Get(id, path))
	{
		return ResolveStateByPath(path, node);
	}

	return false;
}

bool RuntimeState::ResolveChildrenByParentPath(const std::string requestedPath, std::vector<std::shared_ptr<StateNodeBase>> &nodes, size_t start, size_t count)
{
	std::shared_ptr<StateNodeBase> resolvedParent;
	if (!ResolveStateByPath(requestedPath, resolvedParent))
	{
		return false;
	}

	auto structured = dynamic_cast<IStructuredState *>(resolvedParent.get());
	if (!structured)
	{
		return false;
	}

	std::vector<std::string> childNames;
	structured->GetChildNames(childNames);
	count = count == 0 ? childNames.size() : count;
	size_t maxCount = std::min(count + start, childNames.size());

	for (size_t i = start; i < maxCount; i++)
	{
		std::shared_ptr<StateNodeBase> childNode;
		if (ResolveStateByPath(StringFormat("%s.%s", requestedPath.c_str(), childNames[i].c_str()), childNode))
		{
			nodes.push_back(childNode);
		}
		else
		{
			maxCount = std::min(childNames.size(), maxCount + 1);
		}
	}

	return true;
}

bool RuntimeState::ResolveChildrenByParentId(const uint32_t id, std::vector<std::shared_ptr<StateNodeBase>> &nodes, size_t start, size_t count)
{
	std::string path;

	if (m_paths->Get(id, path))
	{
		return ResolveChildrenByParentPath(path, nodes, start, count);
	}

	return false;
}

std::shared_ptr<StateNodeBase>
RuntimeState::CreateNodeForVariable(std::string name, VMValue variable, PType *p_type, const VMFrame *current_frame, PClass *stateOwningClass)
{
	(void)current_frame;
	if (p_type->isPointer() && !p_type->isObjectPointer() && !static_cast<PPointer *>(p_type)->PointedType->isStruct())
	{
		auto pointed = static_cast<PPointer *>(p_type)->PointedType;
	}
	if (TypeIsArrayOrArrayPtr(p_type))
	{
		return std::make_shared<ArrayStateNode>(name, variable, p_type);
	}
	if (IsBasicNonPointerType(p_type) || (p_type->isPointer() && !p_type->isObjectPointer() && !static_cast<PPointer *>(p_type)->PointedType->isStruct()))
	{
		return std::make_shared<ValueStateNode>(name, variable, p_type, stateOwningClass);
	}
	if (p_type == TypeState)
	{
		return std::make_shared<StatePointerNode>(name, variable, stateOwningClass);
	}
	if (p_type->isClass() || p_type->isObjectPointer())
	{
		return std::make_shared<ObjectStateNode>(name, variable, p_type);
	}
	if (TypeIsStructOrStructPtr(p_type) || TypeIsNonStructContainer(p_type))
	{
		return std::make_shared<StructStateNode>(name, variable, p_type, current_frame);
	}

	return nullptr;
}

VMFrameStack *RuntimeState::GetStack(uint32_t stackId)
{
	// just one stack!
	return m_GlobalVMStack;
}

VMFrame *RuntimeState::GetFrame(const uint32_t stackId, const uint32_t level)
{
	std::vector<VMFrame *> frames;
	GetStackFrames(stackId, frames);

	if (frames.empty() || level > frames.size() - 1)
	{
		return nullptr;
	}

	return frames.at(level);
}

void RuntimeState::GetStackFrames(VMFrameStack *stack, std::vector<VMFrame *> &frames)
{
	if (!stack->HasFrames())
	{
		return;
	}
	auto frame = stack->TopFrame();

	while (frame)
	{
		frames.push_back(frame);
		frame = frame->ParentFrame;
	}
}

bool RuntimeState::GetStackFrames(const uint32_t stackId, std::vector<VMFrame *> &frames)
{
	const auto stack = GetStack(stackId);
	if (!stack)
	{
		return false;
	}

	GetStackFrames(stack, frames);

	return true;
}
void RuntimeState::Reset()
{
	m_paths->Clear();
	// RuntimeState::m_GlobalVMStack = nullptr;
	m_root = std::make_shared<StackStateNode>(1);
}
}
