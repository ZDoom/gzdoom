#pragma once

#include "GameInterfaces.h"

#include "BreakpointManager.h"
#include "RuntimeState.h"
#include <dap/session.h>
#include <mutex>
#include <atomic>

namespace DebugServer
{
enum StepType
{
	STEP_IN = 0,
	STEP_OVER,
	STEP_OUT
};

enum StepGranularity
{
	kInstruction = 0,
	kLine,
	kStatement
};

class DebugExecutionManager
{

public:
	enum class DebuggerState
	{
		kRunning = 0,
		kPaused,
		kStepping
	};
	enum class pauseReason
	{
		CONTINUING = -1,
		NONE = 0,
		step,
		breakpoint,
		paused,
		exception
	};
	enum class ExceptionFilter
	{
		kScript,
		kMAX
	};
	private:
	std::mutex m_instructionMutex;
	bool m_closed;

	std::shared_ptr<dap::Session> m_session;
	RuntimeState *m_runtimeState;
	BreakpointManager *m_breakpointManager;

	std::atomic<DebuggerState> m_state = DebuggerState::kRunning;
	std::atomic<uint32_t> m_currentStepStackId = 0;
	StepType m_currentStepType = StepType::STEP_IN;
	StepGranularity m_granularity;
	int m_lastLine = -1;
	const VMOP *m_lastInstruction = nullptr;
	VMFrame *m_currentStepStackFrame;
	VMFunction *m_currentVMFunction;
	std::set<ExceptionFilter> m_exceptionFilters = {ExceptionFilter::kScript};
	public:
	explicit DebugExecutionManager(RuntimeState *runtimeState, BreakpointManager *breakpointManager) : m_closed(true), m_runtimeState(runtimeState), m_breakpointManager(breakpointManager), m_currentStepStackFrame(nullptr)
	{
	}
	static dap::array<dap::ExceptionBreakpointsFilter> GetAllExceptionFilters();


	void Close();
	void HandleInstruction(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc);
	void HandleException(EVMAbortException reason, const std::string &message, const std::string &stackTrace);
	void Open(std::shared_ptr<dap::Session> ses);
	bool Continue();
	bool Pause();
	bool Step(uint32_t stackId, StepType stepType, StepGranularity stepGranularity);
	dap::array<dap::Breakpoint> SetExceptionBreakpointFilters(const std::vector<std::string> &filterIds);
	static ExceptionFilter GetFilterID(const std::string &filter_string);
	bool IsPaused() const { return m_state == DebuggerState::kPaused; }
	private:
	inline pauseReason CheckState(VMFrameStack *stack, VMReturn *ret, int numret, const VMOP *pc);
			void ResetStepState(DebuggerState state, VMFrameStack *stack);
			void WaitWhilePaused(pauseReason pauseReason, VMFrameStack *stack);
};
}
