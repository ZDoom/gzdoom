#include <functional>
#include <string>

#include <dap/protocol.h>
#include <dap/session.h>

#include "GameInterfaces.h"
#include "Nodes/LocalScopeStateNode.h"
#include "Nodes/StackFrameStateNode.h"
#include "Nodes/StateNodeBase.h"
#include "RuntimeState.h"
#include "Utilities.h"
#include "ZScriptDebugger.h"

// This is the main class that handles the debug session and the debug requests/responses and events

namespace DebugServer
{
ZScriptDebugger::ZScriptDebugger()
{
	m_pexCache = std::make_shared<PexCache>();

	m_breakpointManager = std::make_shared<BreakpointManager>(m_pexCache.get());

	m_idProvider = std::make_shared<IdProvider>();
	m_runtimeState = std::make_shared<RuntimeState>(m_idProvider);

	m_executionManager = std::make_shared<DebugExecutionManager>(m_runtimeState.get(), m_breakpointManager.get());
}

void ZScriptDebugger::StartSession(std::shared_ptr<dap::Session> session)
{
	if (m_session)
	{
		if (m_initialized)
		{
			LogInternalError("Session is already active, ending it first!");
		}
		EndSession();
	}
	m_initialized = false;
	m_quitting = false;
	m_session = session;
	m_executionManager->Open(session);
	m_createStackEventHandle = RuntimeEvents::SubscribeToCreateStack(std::bind(&ZScriptDebugger::StackCreated, this, std::placeholders::_1));

	m_cleanupStackEventHandle = RuntimeEvents::SubscribeToCleanupStack(std::bind(&ZScriptDebugger::StackCleanedUp, this, std::placeholders::_1));

	m_instructionExecutionEventHandle = RuntimeEvents::SubscribeToInstructionExecution(
		std::bind(&ZScriptDebugger::InstructionExecution, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	// m_initScriptEventHandle = RuntimeEvents::SubscribeToInitScript(std::bind(&ZScriptDebugger::InitScriptEvent, this, std::placeholders::_1));
	m_logEventHandle = RuntimeEvents::SubscribeToLog(std::bind(&ZScriptDebugger::EventLogged, this, std::placeholders::_1, std::placeholders::_2));

	m_breakpointChangedEventHandle
		= RuntimeEvents::SubscribeToBreakpointChanged(std::bind(&ZScriptDebugger::BreakpointChanged, this, std::placeholders::_1, std::placeholders::_2));

	m_exceptionThrownEventHandle = RuntimeEvents::SubscribeToExceptionThrown(
		std::bind(&ZScriptDebugger::ExceptionThrown, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	RegisterSessionHandlers();
}

bool ZScriptDebugger::IsEndingSession()
{
	return m_endingSession;
}

bool ZScriptDebugger::EndSession(bool closed)
{
	// This is just to prevent the closedHandler from ending the session again
	if (m_endingSession)
	{
		return m_quitting;
	}
	m_endingSession = true;
	m_executionManager->Close();
	if (m_session)
	{
		if (!closed && m_initialized)
		{
			LogInternal("Ending DAP debugging session.");
			SendEvent(dap::TerminatedEvent());
		}
	}
	m_initialized = false;
	m_session = nullptr;

	RuntimeEvents::UnsubscribeFromLog(m_logEventHandle);
	// RuntimeEvents::UnsubscribeFromInitScript(m_initScriptEventHandle);
	RuntimeEvents::UnsubscribeFromInstructionExecution(m_instructionExecutionEventHandle);
	RuntimeEvents::UnsubscribeFromCreateStack(m_createStackEventHandle);
	RuntimeEvents::UnsubscribeFromCleanupStack(m_cleanupStackEventHandle);
	RuntimeEvents::UnsubscribeFromBreakpointChanged(m_breakpointChangedEventHandle);
	RuntimeEvents::UnsubscribeFromExceptionThrown(m_exceptionThrownEventHandle);
	m_logEventHandle = nullptr;
	m_instructionExecutionEventHandle = nullptr;
	m_createStackEventHandle = nullptr;
	m_cleanupStackEventHandle = nullptr;
	m_breakpointChangedEventHandle = nullptr;
	m_exceptionThrownEventHandle = nullptr;
	// clear session data
	m_projectArchive.clear();
	m_projectPath.clear();
	m_projectSources.clear();
	m_breakpointManager->ClearBreakpoints();
	m_endingSession = false;
	return m_quitting;
}

dap::ResponseOrError<dap::SetInstructionBreakpointsResponse> ZScriptDebugger::SetInstructionBreakpoints(const dap::SetInstructionBreakpointsRequest &request)
{
	return m_breakpointManager->SetInstructionBreakpoints(request);
}

void ZScriptDebugger::RegisterSessionHandlers()
{
	// The Initialize request is the first message sent from the client and the response reports debugger capabilities.
	// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
	m_session->registerHandler([this](const dap::InitializeRequest &request) { return Initialize(request); });
	m_session->onError([this](const char *msg) { LogInternalError("%s", msg); });
	m_session->registerSentHandler(
		// After an intialize response is sent, we send an initialized event to indicate that the client can now send requests.
		[this](const dap::ResponseOrError<dap::InitializeResponse> &)
		{
			LogInternal("DAP debugging session started.");
			// enable event sending
			m_initialized = true;
			SendEvent(dap::InitializedEvent());
		});

	// Client is done configuring.
	m_session->registerHandler([this](const dap::ConfigurationDoneRequest &) { return dap::ConfigurationDoneResponse {}; });

	// The Disconnect request is sent by the client before it disconnects from the server.
	// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Disconnect
	m_session->registerHandler(
		[this](const dap::DisconnectRequest &request)
		{
			// Client wants to disconnect.
			if (request.terminateDebuggee.value(false))
			{
				m_quitting = true;
			}
			return dap::DisconnectResponse {};
		});
	m_session->registerHandler([this](const dap::PDSLaunchRequest &request) { return Launch(request); });
	m_session->registerHandler([this](const dap::PDSAttachRequest &request) { return Attach(request); });
	m_session->registerHandler([this](const dap::PauseRequest &request) { return Pause(request); });
	m_session->registerHandler([this](const dap::ContinueRequest &request) { return Continue(request); });
	m_session->registerHandler([this](const dap::ThreadsRequest &request) { return GetThreads(request); });
	m_session->registerHandler([this](const dap::SetBreakpointsRequest &request) { return SetBreakpoints(request); });
	m_session->registerHandler([this](const dap::SetExceptionBreakpointsRequest &request) { return SetExceptionBreakpoints(request); });
	m_session->registerHandler([this](const dap::SetFunctionBreakpointsRequest &request) { return SetFunctionBreakpoints(request); });
	m_session->registerHandler([this](const dap::SetInstructionBreakpointsRequest &request) { return SetInstructionBreakpoints(request); });
	m_session->registerHandler([this](const dap::StackTraceRequest &request) { return GetStackTrace(request); });
	m_session->registerHandler([this](const dap::StepInRequest &request) { return StepIn(request); });
	m_session->registerHandler([this](const dap::StepOutRequest &request) { return StepOut(request); });
	m_session->registerHandler([this](const dap::NextRequest &request) { return Next(request); });
	m_session->registerHandler([this](const dap::ScopesRequest &request) { return GetScopes(request); });
	m_session->registerHandler([this](const dap::VariablesRequest &request) { return GetVariables(request); });
	m_session->registerHandler([this](const dap::SourceRequest &request) { return GetSource(request); });
	m_session->registerHandler([this](const dap::LoadedSourcesRequest &request) { return GetLoadedSources(request); });
	m_session->registerHandler([this](const dap::DisassembleRequest &request) { return Disassemble(request); });
	m_session->registerHandler([this](const dap::EvaluateRequest &request) { return Evaluate(request); });
	m_session->registerHandler([this](const dap::ModulesRequest &request) { return Modules(request); });
}

dap::Error ZScriptDebugger::Error(const std::string &msg)
{
	Printf("%s", msg.c_str());
	return dap::Error(msg);
}

template <typename T, typename> void ZScriptDebugger::SendEvent(const T &event)
{
	if (m_session && m_initialized)
	{
		try
		{
			m_session->send(event);
			// catch signal 13
		}
		catch (...)
		{
			LogInternalError("Error sending event");
			EndSession(true);
		}
	}
}

void ZScriptDebugger::EventLogged(int severity, const char *msg)
{
	if (severity & PRINT_NODAPEVENT)
	{
		return;
	}
	dap::OutputEvent output;
	output.category = "console";
	output.output = std::string(msg) + "\r\n";
	// LogGameOutput(logEvent->severity, output.output);
	SendEvent(output);
}

void ZScriptDebugger::StackCreated(VMFrameStack *stack)
{
#if 0
		const auto stackId = 0; // only one stack
		SendEvent(dap::ThreadEvent{
				.reason = "started",
				.threadId = stackId
				});

#endif
}

void ZScriptDebugger::StackCleanedUp(uint32_t stackId)
{
#if 0
		SendEvent(dap::ThreadEvent{
			.reason = "exited",
			.threadId = stackId
		});
#endif
}

void ZScriptDebugger::InstructionExecution(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc)
{
	m_executionManager->HandleInstruction(stack, ret, numret, pc);
}

// For source loaded events
void ZScriptDebugger::CheckSourceLoaded(const std::string &scriptName)
{
	auto binary = m_pexCache->GetScript(scriptName);
	if (binary && m_session)
	{
		dap::LoadedSourceEvent event;
		event.reason = "new";
		event.source = binary->GetDapSource();
		SendEvent(event);
	}
}

void ZScriptDebugger::BreakpointChanged(const dap::Breakpoint &bpoint, const std::string &reason)
{
	dap::BreakpointEvent event;
	event.breakpoint = bpoint;
	event.reason = reason;
	SendEvent(event);
}

void ZScriptDebugger::ExceptionThrown(EVMAbortException reason, const std::string &message, const std::string &stackTrace)
{
	m_executionManager->HandleException(reason, message, stackTrace);
}

ZScriptDebugger::~ZScriptDebugger()
{
	m_initialized = false;
	EndSession();
	m_runtimeState->Reset();
	m_pexCache->Clear();
}

dap::ResponseOrError<dap::InitializeResponse> ZScriptDebugger::Initialize(const dap::InitializeRequest &request)
{
	m_clientCaps = request;
	dap::InitializeResponse response;
	LogInternal("Initializing DAP session...");
	response.supportsConfigurationDoneRequest = true;
	response.supportsLoadedSourcesRequest = true;
	response.supportedChecksumAlgorithms = {"CRC32"};
	response.supportsFunctionBreakpoints = true;
#if !defined(_WIN32) && !defined(_WIN64)
	// TODO: remove this when disassemble is supported on windows
	response.supportsDisassembleRequest = true;
	response.supportsSteppingGranularity = true;
#endif
	response.supportTerminateDebuggee = true;
	response.supportsInstructionBreakpoints = true;
	response.supportsEvaluateForHovers = true;
	response.supportsModulesRequest = true;
	response.exceptionBreakpointFilters = DebugExecutionManager::GetAllExceptionFilters();
	return response;
}

dap::ResponseOrError<dap::LaunchResponse> ZScriptDebugger::Launch(const dap::PDSLaunchRequest &request)
{
	dap::PDSAttachRequest attach_request;
	attach_request.name = request.name;
	attach_request.type = request.type;
	attach_request.request = request.request;
	attach_request.projectSources = request.projectSources;

	auto resp = Attach(attach_request);
	if (resp.error)
	{
		RETURN_DAP_ERROR(resp.error.message.c_str());
	}
	return dap::ResponseOrError<dap::LaunchResponse>();
}

dap::ResponseOrError<dap::AttachResponse> ZScriptDebugger::Attach(const dap::PDSAttachRequest &request)
{
	m_projectSources.clear();
	if (!request.restart.has_value())
	{
		m_pexCache->Clear();
	}
	m_pexCache->ScanAllScripts();
	for (auto src : request.projectSources.value(std::vector<dap::Source>()))
	{
		auto binary = m_pexCache->GetScript(src);
		if (!binary)
		{ // no source ref or name, we'll ignore it
			continue;
		}
		auto source = binary->GetDapSource();
		m_projectSources[source.sourceReference.value()] = source;
	}

	return dap::AttachResponse();
}

dap::ResponseOrError<dap::ContinueResponse> ZScriptDebugger::Continue(const dap::ContinueRequest &request)
{
	if (m_executionManager->Continue()) return dap::ContinueResponse();
	RETURN_DAP_ERROR("Could not Continue");
}

dap::ResponseOrError<dap::PauseResponse> ZScriptDebugger::Pause(const dap::PauseRequest &request)
{
	if (m_executionManager->Pause()) return dap::PauseResponse();
	RETURN_DAP_ERROR("Already paused!");
}

dap::ResponseOrError<dap::ThreadsResponse> ZScriptDebugger::GetThreads(const dap::ThreadsRequest &request)
{
	dap::ThreadsResponse response;

	// just one thread!
	dap::Thread thread;
	thread.id = 1;
	thread.name = "Main Thread";
	response.threads.push_back(thread);
	return response;
}

dap::ResponseOrError<dap::SetBreakpointsResponse> ZScriptDebugger::SetBreakpoints(const dap::SetBreakpointsRequest &request)
{
	dap::Source source = request.source;
	auto ref = GetSourceReference(source);
	if (m_projectSources.find(ref) != m_projectSources.end())
	{
		source = m_projectSources[ref];
	}
	else if (ref > 0)
	{
		// It's not part of the project's imported sources, they have to get the decompiled source from us,
		// So we set sourceReference to make the debugger request the source from us
		source.sourceReference = ref;
	}
	return m_breakpointManager->SetBreakpoints(source, request);
	;
}

dap::ResponseOrError<dap::SetFunctionBreakpointsResponse> ZScriptDebugger::SetFunctionBreakpoints(const dap::SetFunctionBreakpointsRequest &request) { return m_breakpointManager->SetFunctionBreakpoints(request); }
dap::ResponseOrError<dap::StackTraceResponse> ZScriptDebugger::GetStackTrace(const dap::StackTraceRequest &request)
{
	dap::StackTraceResponse response;

	if (request.threadId <= -1)
	{
		response.totalFrames = 0;
		RETURN_DAP_ERROR("No threadId specified");
	}
	auto frameVal = request.startFrame.value(0);
	auto levelVal = request.levels.value(0);
	std::vector<std::shared_ptr<StateNodeBase>> frameNodes;
	if (!m_runtimeState->ResolveChildrenByParentPath(std::to_string(request.threadId), frameNodes))
	{
		RETURN_DAP_ERROR("Could not find ThreadId");
	}
	uint32_t startFrame = static_cast<uint32_t>(frameVal > 0 ? frameVal : dap::integer(0));
	uint32_t levels = static_cast<uint32_t>(levelVal > 0 ? levelVal : dap::integer(0));

	for (uint32_t frameIndex = startFrame; frameIndex < frameNodes.size() && frameIndex < startFrame + levels; frameIndex++)
	{
		const auto node = dynamic_cast<StackFrameStateNode *>(frameNodes.at(frameIndex).get());

		dap::StackFrame frame;
		if (!node->SerializeToProtocol(frame, m_pexCache.get()))
		{
			RETURN_DAP_ERROR("Serialization error");
		}

		response.stackFrames.push_back(frame);
	}
	return response;
}

inline StepGranularity granularityStringToEnum(const std::string &granularity)
{
	if (granularity == "instruction")
	{
		return StepGranularity::kInstruction;
	}
	else if (granularity == "statement")
	{
		return StepGranularity::kStatement;
	}
	return StepGranularity::kLine;
}

dap::ResponseOrError<dap::StepInResponse> ZScriptDebugger::StepIn(const dap::StepInRequest &request)
{
	if (m_executionManager->Step(static_cast<uint32_t>(request.threadId), STEP_IN, granularityStringToEnum(request.granularity.value("line"))))
	{
		return dap::StepInResponse();
	}
	RETURN_DAP_ERROR("Could not StepIn");
}
dap::ResponseOrError<dap::StepOutResponse> ZScriptDebugger::StepOut(const dap::StepOutRequest &request)
{
	if (m_executionManager->Step(static_cast<uint32_t>(request.threadId), STEP_OUT, granularityStringToEnum(request.granularity.value("line"))))
	{
		return dap::StepOutResponse();
	}
	RETURN_DAP_ERROR("Could not StepOut");
}
dap::ResponseOrError<dap::NextResponse> ZScriptDebugger::Next(const dap::NextRequest &request)
{
	if (m_executionManager->Step(static_cast<uint32_t>(request.threadId), STEP_OVER, granularityStringToEnum(request.granularity.value("line"))))
	{
		return dap::NextResponse();
	}
	RETURN_DAP_ERROR("Could not Next");
}
dap::ResponseOrError<dap::ScopesResponse> ZScriptDebugger::GetScopes(const dap::ScopesRequest &request)
{
	dap::ScopesResponse response;

	std::vector<std::shared_ptr<StateNodeBase>> frameScopes;
	if (request.frameId < 0)
	{
		RETURN_DAP_ERROR(StringFormat("Invalid frameId %ld", static_cast<int64_t>(request.frameId)).c_str());
	}
	auto frameId = static_cast<uint32_t>(request.frameId);
	if (!m_runtimeState->ResolveChildrenByParentId(frameId, frameScopes))
	{
		// Don't log, this happens as a result of a scopes request being sent after a step request that invalidates the state
		return dap::Error(StringFormat("No such frameId %d", frameId).c_str());
	}

	for (const auto &frameScope : frameScopes)
	{
		auto asScopeSerializable = dynamic_cast<IProtocolScopeSerializable *>(frameScope.get());
		if (!asScopeSerializable)
		{
			continue;
		}

		dap::Scope scope;
		if (!asScopeSerializable->SerializeToProtocol(scope))
		{
			continue;
		}

		response.scopes.push_back(scope);
	}

	return response;
}

dap::ResponseOrError<dap::VariablesResponse> ZScriptDebugger::GetVariables(const dap::VariablesRequest &request)
{
	dap::VariablesResponse response;

	std::vector<std::shared_ptr<StateNodeBase>> variableNodes;
	int64_t maxCount = request.count.value(INT_MAX);
	int64_t start = request.start.value(0);

	if (!m_runtimeState->ResolveChildrenByParentId(static_cast<uint32_t>(request.variablesReference), variableNodes, start, maxCount))
	{
		// Don't log, this happens as a result of a variables request being sent after a step request that invalidates the state
		return dap::Error(StringFormat("No such variablesReference %ld", static_cast<int64_t>(request.variablesReference)).c_str());
	}
	bool only_indexed = request.filter.value("") == "indexed";
	bool only_named = request.filter.value("") == "named";

	for (int64_t i = 0; i < (int64_t)variableNodes.size(); i++)
	{
		auto asVariableSerializable = dynamic_cast<IProtocolVariableSerializable *>(variableNodes.at(i).get());
		if (!asVariableSerializable)
		{
			continue;
		}

		dap::Variable variable;
		if (!asVariableSerializable->SerializeToProtocol(variable))
		{
			continue;
		}
		// if it's a number (i.e. the index of an array), we only want to show indexed variables
		if (only_indexed || only_named)
		{
			bool is_indexed = false;
			if (variable.name.size() > 0)
			{
				size_t offset = variable.name[0] == '[' && variable.name[variable.name.size() - 1] == ']' ? 1 : 0;
				is_indexed = std::all_of(variable.name.begin() + offset, variable.name.begin() + variable.name.size() - 1 - offset, ::isdigit);
			}
			if ((is_indexed && only_named) || (!is_indexed && only_indexed))
			{
				continue;
			}
		}
		response.variables.push_back(variable);
	}

	return response;
}
dap::ResponseOrError<dap::SourceResponse> ZScriptDebugger::GetSource(const dap::SourceRequest &request)
{
	if (!request.source.has_value() || !request.source.value().path.has_value() || !request.source.value().sourceReference.has_value())
	{
		RETURN_DAP_ERROR("No source path or reference");
	}
	auto source = request.source.value();
	dap::SourceResponse response;
	std::string sourceContent;
	if (m_pexCache->GetDecompiledSource(source, sourceContent))
	{
		response.content = sourceContent;
		return response;
	}
	RETURN_DAP_ERROR(StringFormat("No source found for %s", source.path.value("").c_str()).c_str());
}
dap::ResponseOrError<dap::LoadedSourcesResponse> ZScriptDebugger::GetLoadedSources(const dap::LoadedSourcesRequest &request) { return m_pexCache->GetLoadedSources(request); }

dap::ResponseOrError<dap::DisassembleResponse> ZScriptDebugger::Disassemble(const dap::DisassembleRequest &request)
{

#if defined(_WIN32) || defined(_WIN64)
	RETURN_DAP_ERROR("Disassemble not supported on Windows");
#else
	auto ref = request.memoryReference;
	// ref is in the format "0x12345678", we need to convert it to a number
	if (ref.size() < 3 || ref[0] != '0' || ref[1] != 'x')
	{
		RETURN_DAP_ERROR("Invalid memoryReference");
	}
	const uint64_t req_address = std::stoull(ref.substr(2), nullptr, 16);
	const int64_t offset = request.instructionOffset.value(0);
	const VMOP *currCodePointer = (VMOP *)req_address;
	auto response = dap::DisassembleResponse();
	std::vector<std::shared_ptr<DisassemblyLine>> lines;
	m_pexCache->GetDisassemblyLines(currCodePointer, offset, request.instructionCount, lines);
	std::shared_ptr<Binary> bin;
	std::vector<std::string> instruction_addrs;
	for (auto &line : lines)
	{
		auto instruction = dap::DisassembledInstruction();
		instruction.instruction = line->instruction;
		instruction.address = StringFormat("%p", line->address);
		instruction_addrs.push_back(instruction.address);
		instruction.line = line->line;
		if (line->line != line->endLine && line->endLine > 0)
		{
			instruction.endLine = line->endLine;
		}

		// TODO: turn this back on, vscode doesn't like it currently
		// only map the source for the first instruction, or if the source location has changed
		//		if (!bin || bin->sourceData.sourceReference.value(-1) != line->ref){
		bin = m_pexCache->GetCachedScript(line->ref);
		if (bin)
		{
			instruction.location = bin->GetDapSource();
		}
		//		}
		instruction.instructionBytes = line->bytes;
		response.instructions.push_back(instruction);
	}
	return response;
#endif
}

dap::ResponseOrError<dap::SetExceptionBreakpointsResponse> ZScriptDebugger::SetExceptionBreakpoints(const dap::SetExceptionBreakpointsRequest &request)
{
	auto response = dap::SetExceptionBreakpointsResponse();
	response.breakpoints = m_executionManager->SetExceptionBreakpointFilters(request.filters);
	return response;
}

dap::ResponseOrError<dap::ModulesResponse> ZScriptDebugger::Modules(const dap::ModulesRequest &request)
{
	auto response = dap::ModulesResponse();
	response.modules = m_pexCache->GetModules();
	if (request.startModule.has_value()){
		response.modules = {response.modules.begin() + request.startModule.value(), response.modules.end()};
	}
	if (request.moduleCount.has_value()){
		response.modules = {response.modules.begin(), response.modules.begin() + request.moduleCount.value()};
	}
	return response;
}

dap::ResponseOrError<dap::EvaluateResponse> ZScriptDebugger::Evaluate(const dap::EvaluateRequest &request)
{
	// get the type of evaluate
	auto context = request.context.value("repl");
	auto response = dap::EvaluateResponse();
	dap::Variable variable;
	bool found = false;
	auto isNonVariableContext = context != "variables";

	auto TryPath = [&](const std::string &path){
		std::shared_ptr<StateNodeBase> node;
		if(m_runtimeState->ResolveStateByPath(path, node, isNonVariableContext)){
			auto asVariableSerializable = std::dynamic_pointer_cast<IProtocolVariableSerializable>(node);
			if(asVariableSerializable && asVariableSerializable->SerializeToProtocol(variable)){
				found = true;
			} else {
				return false;
			}
		}
		return true;
	};
	if (context == "variables"){
		isNonVariableContext = false;
	} else {
		isNonVariableContext = true;
	}
	if (context == "variables" || context == "hover" || context == "watch" || (context == "repl" && m_executionManager->IsPaused()))
	{
		int64_t frameId = request.frameId.value(0);
		int64_t evalLineNumber = request.line.value(-1);
		std::shared_ptr<StateNodeBase> _frameNode;
		if( m_runtimeState->ResolveStateById(frameId, _frameNode)){
			auto frameNode = std::dynamic_pointer_cast<StackFrameStateNode>(_frameNode);
			if (!frameNode){
				RETURN_DAP_ERROR(StringFormat("Could not find frameId %ld", frameId).c_str());
			}
			auto frameNodePath = m_runtimeState->GetPathById(frameNode->GetId());
			if(frameNodePath.empty()){
				RETURN_DAP_ERROR(StringFormat("Could not find frameId %ld", frameId).c_str());
			}
			// try locals first
			std::string localsPath = StringFormat("%s.%s", frameNodePath.c_str(), StackFrameStateNode::LOCAL_SCOPE_NAME);
			std::string path;
			int64_t funcStartingLineNumber = -1;

			auto stackFrame = frameNode->GetStackFrame();
			if (stackFrame && !IsFunctionNative(stackFrame->Func)){
				auto vmscriptfunc = GetVMScriptFunction(stackFrame->Func);
				if (vmscriptfunc){
					funcStartingLineNumber = vmscriptfunc->PCToLine(vmscriptfunc->Code);
					if (evalLineNumber == -1){
						evalLineNumber = vmscriptfunc->PCToLine(stackFrame->PC);
					}
				}
			}

			std::shared_ptr<LocalScopeStateNode> localScope;
			{
				std::shared_ptr<StateNodeBase> _localScopeNodeBase;
				if (m_runtimeState->ResolveStateByPath(localsPath, _localScopeNodeBase, true)){
					localScope = std::dynamic_pointer_cast<LocalScopeStateNode>(_localScopeNodeBase);
				}
			}
			std::vector<std::string> localChildrenNames;
			if (localScope){
				localScope->GetChildNames(localChildrenNames);
				caseless_path_set localChildrenNamesSet(localChildrenNames.begin(), localChildrenNames.end());
				
				path = StringFormat("%s.%s", localsPath.c_str(), request.expression.c_str());
				if(!TryPath(path)){
					RETURN_DAP_ERROR(StringFormat("Could not serialize variable %s", request.expression.c_str()).c_str());
				}

				if (!found && evalLineNumber > -1 && funcStartingLineNumber <= evalLineNumber && request.expression.find(" @") == std::string::npos){
					// try @ line number`
					std::string qualName;
					for (auto &childName : localChildrenNames) {
						// check if the child name contains an @ and starts with the expression
						if (childName.find(" @") != std::string::npos && CaseInsensitiveFind(childName, request.expression) == 0){
							// get the line number from the child name
							auto lineNum = LocalScopeStateNode::GetLineFromLineQualifiedName(childName);
							if (lineNum <= evalLineNumber){
								qualName = childName;
							}
						}
					}
					if (!qualName.empty()){
						path = StringFormat("%s.%s", localsPath.c_str(), qualName.c_str());
						if(!TryPath(path)){
							RETURN_DAP_ERROR(StringFormat("Could not serialize variable %s", request.expression.c_str()).c_str());
						}
					}
				}
				if (!found && request.expression.find("self.") != 0){
					// if the first part isn't self, and the current function on the stack is an action or method, try "locals.self"
					if (stackFrame && (IsFunctionHasSelf(stackFrame->Func))){
						path = StringFormat("%s.self.%s", localsPath.c_str(), request.expression.c_str());
						if(!TryPath(path)){
							RETURN_DAP_ERROR(StringFormat("Could not serialize variable %s", request.expression.c_str()).c_str());
						}
					}
				}
			}
			// try globals next
			if (!found && !TryPath(StringFormat("%s.%s.%s", frameNodePath.c_str(), StackFrameStateNode::GLOBALS_SCOPE_NAME, request.expression.c_str()))){
				RETURN_DAP_ERROR(StringFormat("Could not serialize variable %s", request.expression.c_str()).c_str());
			}
			// cvars?
			if (!found && context != "variables" && !TryPath(StringFormat("%s.%s.%s", frameNodePath.c_str(), StackFrameStateNode::CVAR_SCOPE_NAME, request.expression.c_str()))){
				RETURN_DAP_ERROR(StringFormat("Could not serialize variable %s", request.expression.c_str()).c_str());
			}
		}
		if (found){
			response.result = variable.value;
			response.variablesReference = variable.variablesReference;
			response.type = variable.type;
			response.namedVariables = variable.namedVariables;
			response.indexedVariables = variable.indexedVariables;
			response.memoryReference = variable.memoryReference;
			return response;
		}
	}

	if (context == "repl" && !m_executionManager->IsPaused())
	{
		// TODO: This isn't safe to do from the debugger thread, figure out a way to safely dispatch to the main thread later
#if 0
		// we don't support repl commands when not paused
		auto args = Split(request.expression, " ");
		if (args.size() == 0){
			return dap::Error(StringFormat("No command provided").c_str());
		}
		auto cmdstr = args.front();
		FConsoleCommand *cmd = FConsoleCommand::FindByName(args.front().c_str());
		if (cmd)
		{
			bool unsafe = false;
			if (cmd->IsAlias())
			{
				FUnsafeConsoleAlias *alias = dynamic_cast<FUnsafeConsoleAlias *>(cmd);
				unsafe = alias != nullptr;
			}
			else
			{
				FUnsafeConsoleCommand *unsafeCmd = dynamic_cast<FUnsafeConsoleCommand *>(cmd);
				unsafe = unsafeCmd != nullptr;
			}
			if (unsafe)
			{
				return dap::Error(StringFormat("Cannot run command %s from debugger (unsafe context)!", cmdstr.c_str()).c_str());
			}

			FCommandLine cmdline(request.expression.c_str());
			cmd->Run(cmdline, 0);
			// there's no response for this; the output will be in the debug console buffer
			return response;
		}

		// try a c_var?
		auto cvar = FindConsoleVariable(cmdstr);
		if (cvar){
			
			if (args.size() > 1){
				if (cmdstr == "vm_debug" || cmdstr == "vm_debug_port"){
					return dap::Error(StringFormat("Refusing change %s while debugging!", cmdstr.c_str()).c_str());
				}
				if (cvar->GetFlags() & CVAR_UNSAFECONTEXT) {
					return dap::Error(StringFormat("Cannot change cvar %s from debugger!", cmdstr.c_str()).c_str());
				}
				auto val = request.expression.substr(request.expression.find_first_of(" ") + 1);
				// trim whitespace
				while (val[0] == ' '){
					val = val.substr(1);
				}
				while (val[val.size() - 1] == ' '){
					val = val.substr(0, val.size() - 1);
				}
				cvar->SetGenericRep(val.c_str(), CVAR_String);
			} else {
				auto var = CVarStateNode::ToVariable(cvar);
				response.result = var.value;
				response.type = var.type;
				response.namedVariables = var.namedVariables;
				response.indexedVariables = var.indexedVariables;
				response.presentationHint = var.presentationHint;
			}
			
			return response;
		}
		return dap::Error(StringFormat("Command %s not found!", request.expression.c_str()).c_str());
#endif
		return dap::Error("Cannot run evaluate while running!");
	}
	return dap::Error(StringFormat("Could not evaluate expression %s", request.expression.c_str()).c_str());

}

} // namespace DebugServer
