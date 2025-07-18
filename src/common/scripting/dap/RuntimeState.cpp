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

bool RuntimeState::ResolveStateByPath(const std::string requestedPath, std::shared_ptr<StateNodeBase> &node, bool isEvaluate)
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


	std::function<std::shared_ptr<StateNodeBase>(const std::string &, std::shared_ptr<StateNodeBase>&)> traversePathPart = [&](
		const std::string &currentName,
		std::shared_ptr<StateNodeBase> &part)
	{
		auto structured = dynamic_cast<IStructuredState *>(part.get());
		auto currentPath = Join(currentPathElements, ".");
		ToLower(currentPath);
		if (structured)
		{
			std::vector<std::string> childNames;
			structured->GetChildNames(childNames);

			for (auto childName : childNames)
			{
				ToLower(childName);
				uint32_t addedId;
				m_paths->AddOrGetExisting(currentPath + "." + childName, addedId);
			}
		}

		bool foundChild = false;
		std::shared_ptr<StateNodeBase> foundNode = nullptr;
		if (structured)
		{
			// First try normal child nodes
			if (structured->GetChildNode(currentName, foundNode))
			{
				currentPathElements.push_back(currentName);
				return foundNode;
			} else if (isEvaluate) {
				auto virtualContainers = structured->GetVirtualContainerChildren();
				// If not found and we have virtual containers, check them
				if (!virtualContainers.empty())
				{
					for (auto &virtualContainer : virtualContainers)
					{
						if (virtualContainer.second)
						{
							currentPathElements.push_back(virtualContainer.first);
							foundNode = traversePathPart(currentName, virtualContainer.second);
							if (foundNode) {
								return foundNode;
							}
							// pop the last element
							currentPathElements.pop_back();
						}
					}
				}
			}
		}
		foundNode = nullptr;
		return foundNode;
	};

	while (!elements.empty() && currentNode)
	{
		currentNode = traversePathPart(elements.at(0), currentNode);
		if (!currentNode)
		{
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
		auto actualPath = Join(currentPathElements, ".");
		ToLower(actualPath);
		m_paths->AddOrGetExisting(actualPath, id);
		node->SetId(id);
	}
	else
	{
		node->SetId(stackId);
	}

	return true;
}

std::string RuntimeState::GetPathById(const uint32_t id) const
{
	if (id == 1){
		return "1";
	}
	std::string path;
	if (m_paths->Get(id, path))
	{
		return path;
	}
	return "";
}

bool RuntimeState::ResolveStateById(const uint32_t id, std::shared_ptr<StateNodeBase> &node, bool isEvaluate)
{
	std::string path;

	if (m_paths->Get(id, path))
	{
		return ResolveStateByPath(path, node, isEvaluate);
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
