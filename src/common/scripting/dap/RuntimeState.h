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
	std::shared_ptr<StateNodeBase> m_root;
	public:
	static VMFrameStack *m_GlobalVMStack;
	explicit RuntimeState(const std::shared_ptr<IdProvider> &idProvider);
	void Reset();
	bool ResolveStateByPath(std::string requestedPath, std::shared_ptr<StateNodeBase> &node, bool isEvaluate = false);
	bool ResolveStateById(uint32_t id, std::shared_ptr<StateNodeBase> &node, bool isEvaluate = false);
	bool ResolveChildrenByParentPath(std::string requestedPath, std::vector<std::shared_ptr<StateNodeBase>> &nodes, size_t start = 0, size_t count = INT_MAX);
	bool ResolveChildrenByParentId(uint32_t id, std::vector<std::shared_ptr<StateNodeBase>> &nodes, size_t start = 0, size_t count = INT_MAX);
	std::string GetPathById(const uint32_t id) const;

	static std::shared_ptr<StateNodeBase>
	CreateNodeForVariable(std::string name, VMValue variable, PType *p_type, const VMFrame *current_frame = nullptr, PClass *stateOwningClass = nullptr);

	static VMFrameStack *GetStack(uint32_t stackId);
	static VMFrame *GetFrame(uint32_t stackId, uint32_t level);

	static void GetStackFrames(VMFrameStack *stack, std::vector<VMFrame *> &frames);
	static bool GetStackFrames(uint32_t stackId, std::vector<VMFrame *> &frames);
};
}
