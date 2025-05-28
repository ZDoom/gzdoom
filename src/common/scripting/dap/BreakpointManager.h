#pragma once
#include <map>
#include <set>
#include <dap/protocol.h>
#include <dap/session.h>

#include "GameInterfaces.h"
#include "IdProvider.h"

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
			NONE = -1,
			Line,
			Function,
			Instruction
		};
		Type type;
		int ref;
		std::string funcBreakpointText;
		const char *nativeFuncName;
		dap::Breakpoint bpoint;
	};

	struct ScriptBreakpoints
	{
		int ref {-1};
		dap::Source source;
		std::time_t modificationTime {0};
		std::map<void *, BreakpointInfo> breakpoints;
	};


	explicit BreakpointManager(PexCache *pexCache) : m_pexCache(pexCache) { }
	int64_t GetBreakpointID();
	int AddInvalidBreakpoint(
		std::vector<dap::Breakpoint> &breakpoints, int line, void *address, const std::string &reason, const dap::optional<dap::Source> &source);
	bool AddBreakpointInfo(
		const std::shared_ptr<Binary> &binary,
		VMScriptFunction *function,
		int line,
		void *p_instrRef,
		int offset,
		BreakpointInfo::Type type,
		std::vector<dap::Breakpoint> &r_bpoint,
		const std::string &funcText = {});
	void GetBpointsForResponse(BreakpointInfo::Type type, std::vector<dap::Breakpoint> &responseBpoints);
	dap::ResponseOrError<dap::SetBreakpointsResponse> SetBreakpoints(const dap::Source &src, const dap::SetBreakpointsRequest &request);
	dap::ResponseOrError<dap::SetFunctionBreakpointsResponse> SetFunctionBreakpoints(const dap::SetFunctionBreakpointsRequest &request);
	dap::ResponseOrError<dap::SetInstructionBreakpointsResponse> SetInstructionBreakpoints(const dap::SetInstructionBreakpointsRequest &request);
	void ClearBreakpoints(bool emitChanged = false);
	void ClearBreakpointsForScript(int ref, BreakpointInfo::Type type, bool emitChanged = false);
	bool GetExecutionIsAtValidBreakpoint(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc);
	inline bool IsAtNativeBreakpoint(VMFrameStack *stack);
	void SetBPStoppedEventInfo(VMFrameStack *stack, dap::StoppedEvent &event);
	private:

	PexCache *m_pexCache;
	std::map<void *, std::vector<BreakpointInfo>> m_breakpoints;
	// set of case-insensitive strings
	std::map<std::string_view, BreakpointInfo, ci_less> m_nativeFunctionBreakpoints;
	IdProvider m_idProvider;
	int64_t m_CurrentID = 1;
	size_t times_seen = 0;

	void ClearBreakpointsType(BreakpointInfo::Type type);
};
}
