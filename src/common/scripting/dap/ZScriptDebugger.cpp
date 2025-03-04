#include "ZScriptDebugger.h"

#include <functional>
#include <string>
#include <dap/protocol.h>
#include <dap/session.h>

#include "Utilities.h"
#include "GameInterfaces.h"
#include "Nodes/StackStateNode.h"
#include "Nodes/StackFrameStateNode.h"

#include "Nodes/StateNodeBase.h"

#include <common/engine/filesystem.h>

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
		LogInternalError("Session is already active, ending it first!");
		EndSession();
	}
	m_initialized = false;
	m_quitting = false;
	m_session = session;
	m_executionManager->Open(session);
	m_createStackEventHandle = RuntimeEvents::SubscribeToCreateStack(std::bind(&ZScriptDebugger::StackCreated, this, std::placeholders::_1));

	m_cleanupStackEventHandle = RuntimeEvents::SubscribeToCleanupStack(std::bind(&ZScriptDebugger::StackCleanedUp, this, std::placeholders::_1));

	m_instructionExecutionEventHandle
		= RuntimeEvents::SubscribeToInstructionExecution(std::bind(&ZScriptDebugger::InstructionExecution, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

	// m_initScriptEventHandle = RuntimeEvents::SubscribeToInitScript(std::bind(&ZScriptDebugger::InitScriptEvent, this, std::placeholders::_1));
	m_logEventHandle = RuntimeEvents::SubscribeToLog(std::bind(&ZScriptDebugger::EventLogged, this, std::placeholders::_1, std::placeholders::_2));

	m_breakpointChangedEventHandle = RuntimeEvents::SubscribeToBreakpointChanged(std::bind(&ZScriptDebugger::BreakpointChanged, this, std::placeholders::_1, std::placeholders::_2));

	RegisterSessionHandlers();
}

bool ZScriptDebugger::EndSession()
{
	m_executionManager->Close();
	if (m_session)
	{
		SendEvent(dap::TerminatedEvent());
	}
	m_initialized = false;
	m_session = nullptr;

	RuntimeEvents::UnsubscribeFromLog(m_logEventHandle);
	// RuntimeEvents::UnsubscribeFromInitScript(m_initScriptEventHandle);
	RuntimeEvents::UnsubscribeFromInstructionExecution(m_instructionExecutionEventHandle);
	RuntimeEvents::UnsubscribeFromCreateStack(m_createStackEventHandle);
	RuntimeEvents::UnsubscribeFromCleanupStack(m_cleanupStackEventHandle);
	RuntimeEvents::UnsubscribeFromBreakpointChanged(m_breakpointChangedEventHandle);

	// clear session data
	m_projectArchive.clear();
	m_projectPath.clear();
	m_projectSources.clear();
	m_breakpointManager->ClearBreakpoints();
	return m_quitting;
}

void ZScriptDebugger::RegisterSessionHandlers()
{
	// The Initialize request is the first message sent from the client and the response reports debugger capabilities.
	// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
	m_session->registerHandler([this](const dap::InitializeRequest &request) { return Initialize(request); });
	m_session->onError([this](const char *msg) { Printf("%s", msg); });
	m_session->registerSentHandler(
		// After an intialize response is sent, we send an initialized event to indicate that the client can now send requests.
		[this](const dap::ResponseOrError<dap::InitializeResponse> &)
		{
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
	m_session->registerHandler([this](const dap::SetFunctionBreakpointsRequest &request) { return SetFunctionBreakpoints(request); });
	m_session->registerHandler([this](const dap::StackTraceRequest &request) { return GetStackTrace(request); });
	m_session->registerHandler([this](const dap::StepInRequest &request) { return StepIn(request); });
	m_session->registerHandler([this](const dap::StepOutRequest &request) { return StepOut(request); });
	m_session->registerHandler([this](const dap::NextRequest &request) { return Next(request); });
	m_session->registerHandler([this](const dap::ScopesRequest &request) { return GetScopes(request); });
	m_session->registerHandler([this](const dap::VariablesRequest &request) { return GetVariables(request); });
	m_session->registerHandler([this](const dap::SourceRequest &request) { return GetSource(request); });
	m_session->registerHandler([this](const dap::LoadedSourcesRequest &request) { return GetLoadedSources(request); });
	m_session->registerHandler([this](const dap::DisassembleRequest &request) { return Disassemble(request); });
}

dap::Error ZScriptDebugger::Error(const std::string &msg)
{
	Printf("%s", msg.c_str());
	return dap::Error(msg);
}

template <typename T, typename> void ZScriptDebugger::SendEvent(const T &event) const
{
	if (m_session && m_initialized) m_session->send(event);
}

void ZScriptDebugger::EventLogged(int severity, const char *msg) const
{
	if (severity & PrintLevel_NoEmit)
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

void ZScriptDebugger::InstructionExecution(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc) const { m_executionManager->HandleInstruction(stack, ret, numret, pc); }

// For source loaded events
void ZScriptDebugger::CheckSourceLoaded(const std::string &scriptName) const
{
	auto binary = m_pexCache->GetScript(scriptName);
	if (binary && m_session)
	{
		SendEvent(dap::LoadedSourceEvent {.reason = "new", .source = binary->GetDapSource()});
	}
}

void ZScriptDebugger::BreakpointChanged(const dap::Breakpoint &bpoint, const std::string &reason) const { SendEvent(dap::BreakpointEvent {.breakpoint = bpoint, .reason = reason}); }

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
	return response;
}

dap::ResponseOrError<dap::LaunchResponse> ZScriptDebugger::Launch(const dap::PDSLaunchRequest &request)
{
	auto resp = Attach(
		dap::PDSAttachRequest {.name = request.name, .type = request.type, .request = request.request, .projectPath = request.projectPath, .projectArchive = request.projectArchive, .projectSources = request.projectSources});
	if (resp.error)
	{
		RETURN_DAP_ERROR(resp.error.message.c_str());
	}
	return dap::ResponseOrError<dap::LaunchResponse>();
}

dap::ResponseOrError<dap::AttachResponse> ZScriptDebugger::Attach(const dap::PDSAttachRequest &request)
{
	// get basename of projectArchive
	auto basename = request.projectArchive.value("");
	auto pos = basename.find_last_of("/\\");
	if (pos != std::string::npos)
	{
		basename = basename.substr(pos + 1);
	}

	m_projectPath = request.projectPath.value("");
	m_projectArchive = basename;
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
	RETURN_DAP_ERROR("Could not Pause");
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
		RETURN_DAP_ERROR(StringFormat("Invalid frameId %d", request.frameId).c_str());
	}
	auto frameId = static_cast<uint32_t>(request.frameId);
	if (!m_runtimeState->ResolveChildrenByParentId(frameId, frameScopes))
	{
		RETURN_DAP_ERROR(StringFormat("No such frameId %d", frameId).c_str());
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
	if (!m_runtimeState->ResolveChildrenByParentId(static_cast<uint32_t>(request.variablesReference), variableNodes))
	{
		RETURN_DAP_ERROR(StringFormat("No such variablesReference %d", request.variablesReference).c_str());
	}

	// TODO DAP: support `filter`, parameter
	int64_t count = 0;
	int64_t maxCount = std::min<int64_t>(request.count.value(variableNodes.size()), variableNodes.size());
	int64_t start = request.start.value(0);
	for (int64_t i = start; i < start + maxCount; i++)
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

		response.variables.push_back(variable);
		count++;
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
		if (line->line == 29 && line->function.find("doSomeStupidShit") != -1)
		{
			int i = 0;
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
} // namespace DebugServer
