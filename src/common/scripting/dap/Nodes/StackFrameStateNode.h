#pragma once

#include "common/scripting/vm/vmintern.h"

#include <dap/protocol.h>
#include "StateNodeBase.h"
#include <common/scripting/dap/PexCache.h>
#include <memory>

namespace DebugServer
{
class StackFrameStateNode : public StateNodeBase, public IStructuredState
{
	VMFrame *m_stackFrame;
	VMFrame m_fakeStackFrame;
	std::shared_ptr<StateNodeBase> m_localScope = nullptr;
	std::shared_ptr<StateNodeBase> m_registersScope = nullptr;
	std::shared_ptr<StateNodeBase> m_globalsScope = nullptr;
	std::shared_ptr<StateNodeBase> m_cvarScope = nullptr;
	public:
	constexpr static const char *LOCAL_SCOPE_NAME = "Local";
	constexpr static const char *REGISTERS_SCOPE_NAME = "Registers";
	constexpr static const char *GLOBALS_SCOPE_NAME = "Global";
	constexpr static const char *CVAR_SCOPE_NAME = "CVars";

	StackFrameStateNode(VMFunction *nativeFunction, VMFrame *parentStackFrame);
	explicit StackFrameStateNode(VMFrame *stackFrame);
	VMFrame *GetStackFrame() const { return m_stackFrame; }

	bool SerializeToProtocol(dap::StackFrame &stackFrame, PexCache *pexCache) const;

	bool GetChildNames(std::vector<std::string> &names) override;
	bool GetChildNode(std::string name, std::shared_ptr<StateNodeBase> &node) override;
};
}
