#include "DebugExecutionManager.h"
#include <thread>
// #include "Window.h"
#include "GameInterfaces.h"
namespace DebugServer
{
// using namespace RE::BSScript::Internal;
static const char *pauseReasonStrings[] = {"none", "step", "breakpoint", "paused", "exception"};

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
		std::lock_guard<std::mutex> lock(m_instructionMutex);
		m_state = DebuggerState::kRunning;
		m_currentStepStackId = 0;
		m_currentStepStackFrame = nullptr;
		m_currentVMFunction = nullptr;
		m_lastLine = -1;
		m_lastInstruction = nullptr;
		if (m_session)
		{
			m_session->send(dap::ContinuedEvent {.allThreadsContinued = true, .threadId = 1});
		}
	}
	break;
	default: // not NONE
	{
		std::lock_guard<std::mutex> lock(m_instructionMutex);
		// `stack` is thread_local, we're currently on that thread,
		// and the debugger will be running in a separate thread, so we need to set it here.
		RuntimeState::m_GlobalVMStack = stack;
		m_state = DebuggerState::kPaused;
		m_currentStepStackId = 0;
		m_currentStepStackFrame = nullptr;
		m_currentVMFunction = nullptr;
		// don't reset the last line or last instruction here

		if (m_session)
		{
			m_session->send(dap::StoppedEvent {.reason = pauseReasonStrings[(int)pauseReason], .threadId = 1});
		}
		// TODO: How to do this in GZDoom?
		// Window::ReleaseFocus();
	}
	break;
	}

	while (m_state == DebuggerState::kPaused)
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms);
	}
	// If we were the thread that paused, regain focus
	if (pauseReason != pauseReason::NONE)
	{
		std::lock_guard<std::mutex> lock(m_instructionMutex);
		// TODO: How to do this in GZDoom?
		// Window::RegainFocus();
		// also reset the state
		m_runtimeState->Reset();
		if (m_state != DebuggerState::kRunning)
		{
			RuntimeState::m_GlobalVMStack = stack;
		}
	}
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
}
