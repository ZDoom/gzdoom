#pragma once

#include <memory>

#include <dap/protocol.h>
#include <dap/session.h>

namespace dap
{
class Debugger
{
	public:
	enum StepType
	{
		STEP_IN = 0,
		STEP_OVER,
		STEP_OUT
	};
	enum VariablesFilter
	{
		VariablesNamed,
		VariablesIndexed,
		VariablesBoth
	};
	enum DisconnectAction
	{
		DisconnectDefault, // Attach -> Detach, Launch -> Terminate
		DisconnectTerminate,
		DisconnectDetach
	};

	virtual ~Debugger() { }

	virtual ResponseOrError<AttachResponse> Attach(const AttachRequest &request) = 0;
	virtual ResponseOrError<BreakpointLocationsResponse> BreakpointLocations(const BreakpointLocationsRequest &request) = 0;
	virtual ResponseOrError<CompletionsResponse> Completions(const CompletionsRequest &request) = 0;
	virtual ResponseOrError<ConfigurationDoneResponse> ConfigurationDone(const ConfigurationDoneRequest &request) = 0;
	virtual ResponseOrError<ContinueResponse> Continue(const ContinueRequest &request) = 0;
	virtual ResponseOrError<DataBreakpointInfoResponse> DataBreakpointInfo(const DataBreakpointInfoRequest &request) = 0;
	virtual ResponseOrError<DisassembleResponse> Disassemble(const DisassembleRequest &request) = 0;
	virtual ResponseOrError<DisconnectResponse> Disconnect(const DisconnectRequest &request) = 0;
	virtual ResponseOrError<EvaluateResponse> Evaluate(const EvaluateRequest &request) = 0;
	virtual ResponseOrError<ExceptionInfoResponse> ExceptionInfo(const ExceptionInfoRequest &request) = 0;
	virtual ResponseOrError<GotoResponse> Goto(const GotoRequest &request) = 0;
	virtual ResponseOrError<GotoTargetsResponse> GotoTargets(const GotoTargetsRequest &request) = 0;
	virtual ResponseOrError<InitializeResponse> Initialize(const InitializeRequest &request) = 0;
	virtual ResponseOrError<LaunchResponse> Launch(const LaunchRequest &request) = 0;
	virtual ResponseOrError<LoadedSourcesResponse> LoadedSources(const LoadedSourcesRequest &request) = 0;
	virtual ResponseOrError<ModulesResponse> Modules(const ModulesRequest &request) = 0;
	virtual ResponseOrError<NextResponse> Next(const NextRequest &request) = 0;
	virtual ResponseOrError<PauseResponse> Pause(const PauseRequest &request) = 0;
	virtual ResponseOrError<ReadMemoryResponse> ReadMemory(const ReadMemoryRequest &request) = 0;
	virtual ResponseOrError<RestartResponse> Restart(const RestartRequest &request) = 0;
	virtual ResponseOrError<RestartFrameResponse> RestartFrame(const RestartFrameRequest &request) = 0;
	virtual ResponseOrError<ReverseContinueResponse> ReverseContinue(const ReverseContinueRequest &request) = 0;
	virtual ResponseOrError<ScopesResponse> Scopes(const ScopesRequest &request) = 0;
	virtual ResponseOrError<SetBreakpointsResponse> SetBreakpoints(const SetBreakpointsRequest &request) = 0;
	virtual ResponseOrError<SetDataBreakpointsResponse> SetDataBreakpoints(const SetDataBreakpointsRequest &request) = 0;
	virtual ResponseOrError<SetExceptionBreakpointsResponse> SetExceptionBreakpoints(const SetExceptionBreakpointsRequest &request) = 0;
	virtual ResponseOrError<SetExpressionResponse> SetExpression(const SetExpressionRequest &request) = 0;
	virtual ResponseOrError<SetFunctionBreakpointsResponse> SetFunctionBreakpoints(const SetFunctionBreakpointsRequest &request) = 0;
	virtual ResponseOrError<SetInstructionBreakpointsResponse> SetInstructionBreakpoints(const SetInstructionBreakpointsRequest &request) = 0;
	virtual ResponseOrError<SetVariableResponse> SetVariable(const SetVariableRequest &request) = 0;
	virtual ResponseOrError<SourceResponse> Source(const SourceRequest &request) = 0;
	virtual ResponseOrError<StackTraceResponse> StackTrace(const StackTraceRequest &request) = 0;
	virtual ResponseOrError<StepBackResponse> StepBack(const StepBackRequest &request) = 0;
	virtual ResponseOrError<StepInResponse> StepIn(const StepInRequest &request) = 0;
	virtual ResponseOrError<StepInTargetsResponse> StepInTargets(const StepInTargetsRequest &request) = 0;
	virtual ResponseOrError<StepOutResponse> StepOut(const StepOutRequest &request) = 0;
	virtual ResponseOrError<TerminateResponse> Terminate(const TerminateRequest &request) = 0;
	virtual ResponseOrError<TerminateThreadsResponse> TerminateThreads(const TerminateThreadsRequest &request) = 0;
	virtual ResponseOrError<ThreadsResponse> Threads(const ThreadsRequest &request) = 0;
	virtual ResponseOrError<VariablesResponse> Variables(const VariablesRequest &request) = 0;
	protected:
	std::shared_ptr<dap::Session> m_Session;
};
}