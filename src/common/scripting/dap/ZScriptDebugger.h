#pragma once

#include <dap/protocol.h>
#include <dap/session.h>
#include <dap/traits.h>
#include "RuntimeEvents.h"
#include "PexCache.h"
#include "BreakpointManager.h"
#include "DebugExecutionManager.h"
#include "IdMap.h"
#include <forward_list>
#include "Protocol/struct_extensions.h"
#include <common/engine/printf.h>
namespace DebugServer
{
enum DisconnectAction
{
	DisconnectDefault, // Attach -> Detach, Launch -> Terminate
	DisconnectTerminate,
	DisconnectDetach
};

enum Severity
{
	kDebug = DMSG_SPAMMY,
	kInfo = DMSG_NOTIFY,
	kWarning = DMSG_WARNING,
	kError = DMSG_ERROR
};

enum VariablesFilter
{
	VariablesNamed,
	VariablesIndexed,
	VariablesBoth
};
class ZScriptDebugger
{
	template <typename T> using IsEvent = dap::traits::EnableIfIsType<dap::Event, T>;
	public:
	ZScriptDebugger();
	~ZScriptDebugger();
	void StartSession(std::shared_ptr<dap::Session> session);
	void EndSession(bool sendTerminateEvent = true);
	bool IsJustMyCode() const { return false; };
	void SetJustMyCode(bool enable) { };
	template <typename T, typename = IsEvent<T>> void SendEvent(const T &event) const;
	void LogGameOutput(Severity severity, const std::string &msg) const;
	void Initialize();

	int GetLastStoppedThreadId() { return 0; }
	dap::ResponseOrError<dap::InitializeResponse> Initialize(const dap::InitializeRequest &request);
	dap::ResponseOrError<dap::LaunchResponse> Launch(const dap::PDSLaunchRequest &request);
	dap::ResponseOrError<dap::AttachResponse> Attach(const dap::PDSAttachRequest &request);
	dap::ResponseOrError<dap::ContinueResponse> Continue(const dap::ContinueRequest &request);
	dap::ResponseOrError<dap::PauseResponse> Pause(const dap::PauseRequest &request);
	dap::ResponseOrError<dap::ThreadsResponse> GetThreads(const dap::ThreadsRequest &request);
	dap::ResponseOrError<dap::SetBreakpointsResponse> SetBreakpoints(const dap::SetBreakpointsRequest &request);
	dap::ResponseOrError<dap::SetFunctionBreakpointsResponse> SetFunctionBreakpoints(const dap::SetFunctionBreakpointsRequest &request);
	dap::ResponseOrError<dap::StackTraceResponse> GetStackTrace(const dap::StackTraceRequest &request);
	dap::ResponseOrError<dap::StepInResponse> StepIn(const dap::StepInRequest &request);
	dap::ResponseOrError<dap::StepOutResponse> StepOut(const dap::StepOutRequest &request);
	dap::ResponseOrError<dap::NextResponse> Next(const dap::NextRequest &request);
	dap::ResponseOrError<dap::ScopesResponse> GetScopes(const dap::ScopesRequest &request);
	dap::ResponseOrError<dap::VariablesResponse> GetVariables(const dap::VariablesRequest &request);
	dap::ResponseOrError<dap::SourceResponse> GetSource(const dap::SourceRequest &request);
	dap::ResponseOrError<dap::LoadedSourcesResponse> GetLoadedSources(const dap::LoadedSourcesRequest &request);
	dap::ResponseOrError<dap::DisassembleResponse> Disassemble(const dap::DisassembleRequest &request);
	private:
	bool m_closed = false;

	std::atomic<uint64_t> msg_counter = 0;
	std::shared_ptr<IdProvider> m_idProvider;

	std::shared_ptr<dap::Session> m_session = nullptr;
	std::shared_ptr<PexCache> m_pexCache;
	std::shared_ptr<BreakpointManager> m_breakpointManager;
	std::shared_ptr<RuntimeState> m_runtimeState;
	std::shared_ptr<DebugExecutionManager> m_executionManager;
	std::map<int, dap::Source> m_projectSources;
	std::string m_projectPath;
	std::string m_projectArchive;
	std::mutex m_instructionMutex;
	dap::InitializeRequest m_clientCaps;

	RuntimeEvents::CreateStackEventHandle m_createStackEventHandle;
	RuntimeEvents::CleanupStackEventHandle m_cleanupStackEventHandle;
	RuntimeEvents::InstructionExecutionEventHandle m_instructionExecutionEventHandle;
	RuntimeEvents::LogEventHandle m_logEventHandle;
	RuntimeEvents::BreakpointChangedEventHandle m_breakpointChangedEventHandle;
	bool m_quitting = false;

	void RegisterSessionHandlers();
	dap::Error Error(const std::string &msg);
	void EventLogged(int severity, const char *msg) const;
	void StackCreated(VMFrameStack *stack);
	void StackCleanedUp(uint32_t stackId);
	void InstructionExecution(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc) const;
	void CheckSourceLoaded(const std::string &scriptName) const;
	void BreakpointChanged(const dap::Breakpoint &bpoint, const std::string &reason) const;
};
}
