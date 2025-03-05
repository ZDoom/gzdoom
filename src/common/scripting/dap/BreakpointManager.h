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
	static std::string AddrToString(void *addr);
	int AddInvalidBreakpoint(
		std::vector<dap::Breakpoint> &breakpoints, int line, void *address, const std::string &reason, const dap::optional<dap::Source> &source);
	bool AddBreakpointInfo(
		const std::shared_ptr<Binary> &binary,
		VMScriptFunction *function,
		int line,
		void *p_instrRef,
		int offset,
		BreakpointInfo::Type type,
		std::vector<dap::Breakpoint> &r_bpoint);
	void GetBpointsForResponse(BreakpointInfo::Type type, std::vector<dap::Breakpoint> &responseBpoints);
	dap::ResponseOrError<dap::SetBreakpointsResponse> SetBreakpoints(const dap::Source &src, const dap::SetBreakpointsRequest &request);
	bool AddPositionalBpoint(VMFunction *p_func, void *address, int64_t line);
	dap::ResponseOrError<dap::SetFunctionBreakpointsResponse> SetFunctionBreakpoints(const dap::SetFunctionBreakpointsRequest &request);
	dap::ResponseOrError<dap::SetInstructionBreakpointsResponse> SetInstructionBreakpoints(const dap::SetInstructionBreakpointsRequest &request);
	void ClearBreakpoints(bool emitChanged = false);
	void ClearBreakpointsForScript(int ref, BreakpointInfo::Type type, bool emitChanged = false);
	bool GetExecutionIsAtValidBreakpoint(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc);
	private:
	void ClearFunctionBreakpoints();
	bool HasSeenBreakpoint(BreakpointInfo *info);

	PexCache *m_pexCache;
	std::map<void *, std::vector<BreakpointInfo>> m_breakpoints;
	// set of case-insensitive strings
	caseless_path_map<BreakpointInfo> m_nativeFunctionBreakpoints;
	BreakpointInfo *m_last_seen = nullptr;
	IdProvider m_idProvider;
	int64_t m_CurrentID = 1;
	size_t times_seen = 0;

	void ClearBreakpointsType(BreakpointInfo::Type type);
};
}
