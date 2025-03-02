#pragma once

#include "IdMap.h"

#include <memory>
#include <vector>
#include "Nodes/StateNodeBase.h"
#include "vm.h"

namespace DebugServer
{
class RuntimeState
{
	std::unique_ptr<IdMap<std::string>> m_paths;
	public:
	static VMFrameStack *m_GlobalVMStack;
	explicit RuntimeState(const std::shared_ptr<IdProvider> &idProvider);
	void Reset();
	bool ResolveStateByPath(std::string requestedPath, std::shared_ptr<StateNodeBase> &node);
	bool ResolveStateById(uint32_t id, std::shared_ptr<StateNodeBase> &node);
	bool ResolveChildrenByParentPath(std::string requestedPath, std::vector<std::shared_ptr<StateNodeBase>> &nodes);
	bool ResolveChildrenByParentId(uint32_t id, std::vector<std::shared_ptr<StateNodeBase>> &nodes);

	static std::shared_ptr<StateNodeBase> CreateNodeForVariable(std::string name, VMValue variable, PType *type);

	static VMFrameStack *GetStack(uint32_t stackId);
	static VMFrame *GetFrame(uint32_t stackId, uint32_t level);

	static void GetStackFrames(VMFrameStack *stack, std::vector<VMFrame *> &frames);
	static bool GetStackFrames(uint32_t stackId, std::vector<VMFrame *> &frames);
};
}
