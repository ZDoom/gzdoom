#include "DebugExecutionManager.h"
#include <thread>
// #include "Window.h"
#include "GameInterfaces.h"
#ifdef _WIN32
extern void I_GetWindowEvent();
extern bool win32EnableInput;
#endif

namespace DebugServer
{
// using namespace RE::BSScript::Internal;
static constexpr const char *pauseReasonStrings[] = {"none", "step", "breakpoint", "paused", "exception"};
static constexpr size_t pauseReasonStringsSize = sizeof(pauseReasonStrings) / sizeof(pauseReasonStrings[0]);
static_assert(pauseReasonStringsSize == static_cast<int>(DebugExecutionManager::pauseReason::exception) + 1, "pauseReasonStrings size mismatch");
static constexpr const char *exceptionStrings[]
	= {"Other", "ReadNil", "WriteNil", "TooManyTries", "ArrayOutOfBounds", "DivisionByZero", "BadSelf", "StringFormat"};
static constexpr size_t exceptionStringsSize = sizeof(exceptionStrings) / sizeof(exceptionStrings[0]);
static_assert(exceptionStringsSize == X_FORMAT_ERROR + 1, "exceptionStrings size mismatch");

static constexpr const char *exceptionFilters[] = {"VM"};
static constexpr const char *exceptionFilterDescriptions[] = {"VM exceptions"};
static_assert(sizeof(exceptionFilters) / sizeof(exceptionFilters[0]) == (size_t)DebugExecutionManager::ExceptionFilter::kMAX, "exceptionFilters size mismatch");
static_assert(
	sizeof(exceptionFilterDescriptions) / sizeof(exceptionFilterDescriptions[0]) == (size_t)DebugExecutionManager::ExceptionFilter::kMAX,
	"exceptionFilterDescriptions size mismatch");


DebugExecutionManager::pauseReason DebugExecutionManager::CheckState(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc)
{
	switch (m_state)
	{
	case DebuggerState::kPaused:
	{
		return pauseReason::paused;
	}
	case DebuggerState::kRunning:
	{
		if (m_breakpointManager->GetExecutionIsAtValidBreakpoint(stack, ret, numret, pc))
		{
			return pauseReason::breakpoint;
		}
	}
	break;
	case DebuggerState::kStepping:
	{
		std::lock_guard<std::mutex> lock(m_instructionMutex);
		if (m_breakpointManager->GetExecutionIsAtValidBreakpoint(stack, ret, numret, pc))
		{
			return pauseReason::breakpoint;
		}
		else if (!RuntimeState::GetStack(m_currentStepStackId))
		{
			return pauseReason::CONTINUING;
		}
		else if (m_currentStepStackFrame)
		{
			VMScriptFunction *func = nullptr;
			auto lastInst = m_lastInstruction;
			m_lastInstruction = pc;
			std::vector<VMFrame *> currentFrames;
			RuntimeState::GetStackFrames(stack, currentFrames);
			if (!currentFrames.empty())
			{
				ptrdiff_t stepFrameIndex = -1;
				const auto stepFrameIter = std::find(currentFrames.begin(), currentFrames.end(), m_currentStepStackFrame);
				if (stepFrameIter != currentFrames.end() && m_currentVMFunction && m_currentStepStackFrame->Func == m_currentVMFunction)
				{
					stepFrameIndex = std::distance(currentFrames.begin(), stepFrameIter);
				}
				// Only get the function if we're not stepping by instruction and the frame exists
				if (m_granularity != kInstruction && stepFrameIndex != -1)
				{
					func = !IsFunctionNative(m_currentStepStackFrame->Func) ? dynamic_cast<VMScriptFunction *>(m_currentStepStackFrame->Func) : nullptr;
					// if we're in the same frame, the last instruction was at the previous address, and the line is the same, we should continue
					if (func && stepFrameIndex == 0 && lastInst == pc - 1 && m_lastLine == func->PCToLine(pc))
					{
						// NONE will cause the function to continue execution without resetting the step state
						return pauseReason::NONE;
					}
				}

				switch (m_currentStepType)
				{
				case StepType::STEP_IN:
					return pauseReason::step;
					break;
				case StepType::STEP_OUT:
					// If the stack exists, but the original frame is gone, we know we're in a previous frame now.
					if (stepFrameIndex == -1)
					{
						return pauseReason::step;
					}
					break;
				case StepType::STEP_OVER:
					if (stepFrameIndex <= 0)
					{
						return pauseReason::step;
					}
					break;
				}
			}
			if (m_granularity != kInstruction && func)
			{
				m_lastLine = func->PCToLine(pc);
			}
			else
			{
				m_lastLine = -1;
			}
			// we deliberately don't set shouldContinue here in an else here, as we want to continue until we hit the next step point
		}
		else
		{
			// no more frames on stack, should continue
			if (!stack->HasFrames())
			{
				return pauseReason::CONTINUING;
			}
		}
	}
	break;
	}
	return pauseReason::NONE;
}

void DebugExecutionManager::ResetStepState(DebuggerState state, VMFrameStack *stack)
{
	std::lock_guard<std::mutex> lock(m_instructionMutex);
	// `stack` is thread_local, we're currently on that thread,
	// and the debugger will be running in a separate thread, so we need to set it here.
	m_state = state;

	m_currentStepStackId = 0;
	m_currentStepStackFrame = nullptr;
	m_currentVMFunction = nullptr;
	if (state == DebuggerState::kPaused && stack)
	{
		RuntimeState::m_GlobalVMStack = stack;
	}
	else if (state == DebuggerState::kRunning)
	{
		m_lastLine = -1;
		m_lastInstruction = nullptr;
	}
}

void DebugExecutionManager::WaitWhilePaused(pauseReason pauseReason, VMFrameStack *stack)
{
	if (pauseReason != pauseReason::NONE)
	{
		if (pauseReason > pauseReason::NONE)
		{
#ifdef _WIN32
			win32EnableInput = false;
#endif

			while (m_state == DebuggerState::kPaused)
			{
#ifdef _WIN32
				I_GetWindowEvent();
#endif
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(100ms);
			}
#ifdef _WIN32
			win32EnableInput = true;
#endif
		}
		std::lock_guard<std::mutex> lock(m_instructionMutex);
		// reset the state
		m_runtimeState->Reset();
		if (m_state != DebuggerState::kRunning && stack)
		{
			RuntimeState::m_GlobalVMStack = stack;
		}
	}
}

void DebugExecutionManager::HandleInstruction(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc)
{
	if (m_closed)
	{
		return;
	}


	pauseReason pauseReason = CheckState(stack, ret, numret, pc);
	switch (pauseReason)
	{
	case pauseReason::NONE:
		break;
	case pauseReason::CONTINUING:
	{
		ResetStepState(DebuggerState::kRunning, stack);
		if (m_session)
		{
			dap::ContinuedEvent event;
			event.allThreadsContinued = true;
			event.threadId = 1;
			m_session->send(event);
		}
	}
	break;
	default: // not NONE
	{
		// don't reset the last line or last instruction here
		ResetStepState(DebuggerState::kPaused, stack);
		if (m_session)
		{
			dap::StoppedEvent event;
			event.allThreadsStopped = true;
			event.reason = pauseReasonStrings[(int)pauseReason];
			if (pauseReason == pauseReason::breakpoint)
			{
				m_breakpointManager->SetBPStoppedEventInfo(stack, event);
			}
			event.threadId = 1;
			m_session->send(event);
		}
		// TODO: How to do this in GZDoom?
		// Window::ReleaseFocus();
	}
	break;
	}
	WaitWhilePaused(pauseReason, stack);
}

void DebugExecutionManager::HandleException(EVMAbortException reason, const std::string &message, const std::string &stackTrace)
{
	if (m_exceptionFilters.empty())
	{
		return;
	}
	// If we're in a VM exception being thrown, we're on the main thread
	ResetStepState(DebuggerState::kPaused, &GlobalVMStack);
	if (m_session)
	{
		auto event = dap::StoppedEvent();
		event.allThreadsStopped = true;
		if (!stackTrace.empty())
		{
			event.text = message + "\n" + stackTrace;
		}
		else
		{
			event.text = message;
		}
		event.reason = "exception";
		event.description = "Paused on exception: " + (reason < exceptionStringsSize ? std::string(exceptionStrings[(int)reason]) : "Unknown");
		event.threadId = 1;
		m_session->send(event);
	};
	WaitWhilePaused(pauseReason::exception, nullptr);
}


void DebugExecutionManager::Open(std::shared_ptr<dap::Session> ses)
{
	std::lock_guard<std::mutex> lock(m_instructionMutex);
	m_closed = false;
	m_session = ses;
}

void DebugExecutionManager::Close()
{
	std::lock_guard<std::mutex> lock(m_instructionMutex);
	m_state = DebuggerState::kRunning;
	m_closed = true;
	m_session = nullptr;
}

bool DebugExecutionManager::Continue()
{
	std::lock_guard<std::mutex> lock(m_instructionMutex);
	m_state = DebuggerState::kRunning;
	if (m_session){
		m_session->send(dap::ContinuedEvent());
	}
	return true;
}

bool DebugExecutionManager::Pause()
{
	if (m_state == DebuggerState::kPaused)
	{
		return false;
	}
	std::lock_guard<std::mutex> lock(m_instructionMutex);
	m_state = DebuggerState::kPaused;
	return true;
}

bool DebugExecutionManager::Step(uint32_t stackId, const StepType stepType, StepGranularity stepGranularity)
{
	if (m_state != DebuggerState::kPaused)
	{
		return false;
	}
	std::lock_guard<std::mutex> lock(m_instructionMutex);
	const auto stack = RuntimeState::GetStack(stackId);
	if (stack)
	{
		if (stack->HasFrames())
		{
			m_currentStepStackFrame = stack->TopFrame();
			if (m_currentStepStackFrame)
			{
				m_currentVMFunction = m_currentStepStackFrame->Func;
			}
		}
	}
	else
	{
		return false;
	}

	m_currentStepStackId = stackId;
	m_currentStepType = stepType;
	m_state = DebuggerState::kStepping;
	m_granularity = stepGranularity;
	m_lastInstruction = nullptr;
	if (m_granularity != kInstruction)
	{
		VMScriptFunction *func = !IsFunctionNative(m_currentStepStackFrame->Func) ? dynamic_cast<VMScriptFunction *>(m_currentStepStackFrame->Func) : nullptr;
		if (func)
		{
			m_lastInstruction = m_currentStepStackFrame->PC;
			m_lastLine = func->PCToLine(m_currentStepStackFrame->PC);
		}
	}

	return true;
}

dap::array<dap::Breakpoint> DebugExecutionManager::SetExceptionBreakpointFilters(const std::vector<std::string> &filterIds)
{
	m_exceptionFilters.clear();
	dap::array<dap::Breakpoint> breakpoints;
	for (unsigned int i = 0; i < filterIds.size(); i++)
	{
		auto breakpoint = dap::Breakpoint();
		int64_t id = (int64_t)DebugExecutionManager::GetFilterID(filterIds[i]);
		breakpoint.id = id;
		breakpoint.verified = false;
		if (id != -1)
		{
			m_exceptionFilters.insert((ExceptionFilter)id);
			breakpoint.verified = true;
		}
		else
		{
			breakpoint.reason = "failed";
			breakpoint.message = "No such exception filter";
		}
		breakpoints.push_back(breakpoint);
	}
	return breakpoints;
}

DebugExecutionManager::ExceptionFilter DebugExecutionManager::GetFilterID(const std::string &filter_string)
{
	for (int i = 0; i < (int)ExceptionFilter::kMAX; i++)
	{
		if (filter_string == exceptionFilters[i])
		{
			return (ExceptionFilter)i;
		}
	}
	return (ExceptionFilter)-1;
}

dap::array<dap::ExceptionBreakpointsFilter> DebugExecutionManager::GetAllExceptionFilters()
{
	dap::array<dap::ExceptionBreakpointsFilter> filters;
	for (int i = 0; i < (int)ExceptionFilter::kMAX; i++)
	{
		dap::ExceptionBreakpointsFilter filter;
		filter.filter = exceptionFilters[i];
		filter.label = exceptionFilterDescriptions[i];
		filter.description = exceptionFilterDescriptions[i];
		filter.conditionDescription = exceptionFilterDescriptions[i];
		filter.def = true;
		filters.push_back(filter);
	}
	return filters;
}
}
