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
#include <memory>
#include <common/scripting/dap/Nodes/StatePointerNode.h>

namespace DebugServer
{
VMFrameStack *RuntimeState::m_GlobalVMStack = nullptr;

RuntimeState::RuntimeState(const std::shared_ptr<IdProvider> &idProvider) { m_paths = std::make_unique<IdMap<std::string>>(idProvider); }

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

	std::shared_ptr<StateNodeBase> currentNode = std::make_shared<StackStateNode>(stackId);
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

bool RuntimeState::ResolveChildrenByParentPath(const std::string requestedPath, std::vector<std::shared_ptr<StateNodeBase>> &nodes)
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

	for (const auto &childName : childNames)
	{
		std::shared_ptr<StateNodeBase> childNode;
		if (ResolveStateByPath(StringFormat("%s.%s", requestedPath.c_str(), childName.c_str()), childNode))
		{
			nodes.push_back(childNode);
		}
	}

	return true;
}

bool RuntimeState::ResolveChildrenByParentId(const uint32_t id, std::vector<std::shared_ptr<StateNodeBase>> &nodes)
{
	std::string path;

	if (m_paths->Get(id, path))
	{
		return ResolveChildrenByParentPath(path, nodes);
	}

	return false;
}

std::shared_ptr<StateNodeBase> RuntimeState::CreateNodeForVariable(std::string name, VMValue variable, PType *p_type)
{

	if (IsBasicNonPointerType(p_type) || (p_type->isPointer() && !p_type->isObjectPointer() && !static_cast<PPointer *>(p_type)->PointedType->isStruct()))
	{
		return std::make_shared<ValueStateNode>(name, variable, p_type);
	}
	if (p_type == TypeState)
	{
		return std::make_shared<StatePointerNode>(name, variable, dynamic_cast<PStatePointer *>(p_type));
	}
	if (p_type->isClass() || p_type->isObjectPointer())
	{
		return std::make_shared<ObjectStateNode>(name, variable, p_type);
	}
	else if (p_type->isStruct() || p_type->isContainer() || (p_type->isPointer() && static_cast<PPointer *>(p_type)->PointedType->isStruct()))
	{
		return std::make_shared<StructStateNode>(name, variable, p_type);
	}
	else if (p_type->isArray() || p_type->isDynArray() || p_type->isStaticArray())
	{
		return std::make_shared<ArrayStateNode>(name, variable, p_type);
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
	RuntimeState::m_GlobalVMStack = nullptr;
}
}
