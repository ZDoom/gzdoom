#pragma once
#include <map>
#include <set>
#include <dap/protocol.h>
#include <dap/session.h>

#include "GameInterfaces.h"

#include "PexCache.h"

namespace DebugServer
{
class BreakpointManager
{
	public:
	struct BreakpointInfo
	{
		enum class Type
		{
			Line,
			Function,
			Instruction
		};
		Type type;
		int ref {-1};
		int64_t breakpointId;
		int instructionNum;
		int lineNum;
		int debugFuncInfoIndex;
		void *address;
		bool isNative;
	};

	struct ScriptBreakpoints
	{
		int ref {-1};
		dap::Source source;
		std::time_t modificationTime {0};
		std::map<void *, BreakpointInfo> breakpoints;
	};

	explicit BreakpointManager(PexCache *pexCache) : m_pexCache(pexCache) { }
	dap::ResponseOrError<dap::SetBreakpointsResponse> SetBreakpoints(const dap::Source &src, const dap::SetBreakpointsRequest &request);
	dap::ResponseOrError<dap::SetFunctionBreakpointsResponse> SetFunctionBreakpoints(const dap::SetFunctionBreakpointsRequest &request);
	dap::ResponseOrError<dap::SetInstructionBreakpointsResponse> SetInstructionBreakpoints(const dap::SetInstructionBreakpointsRequest &request);
	void ClearBreakpoints(bool emitChanged = false);
	void InvalidateAllBreakpointsForScript(int ref, bool emitChanged = false);
	bool GetExecutionIsAtValidBreakpoint(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc);
	private:
	void ClearFunctionBreakpoints();
	bool HasSeenBreakpoint(BreakpointInfo *info);
	PexCache *m_pexCache;
	std::map<void *, BreakpointInfo> m_breakpoints;
	// set of case-insensitive strings
	caseless_path_map<BreakpointInfo> m_nativeFunctionBreakpoints;
	BreakpointInfo *m_last_seen = nullptr;
	size_t times_seen = 0;

	void ClearLineBreakpoints(int ref);
};
}
