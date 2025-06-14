
#include "core/timer.h"
#include "core/widget.h"
#include "window/window.h"

Timer::Timer(Widget* owner) : OwnerObj(owner)
{
	PrevTimerObj = owner->FirstTimerObj;
	if (PrevTimerObj)
		PrevTimerObj->PrevTimerObj = this;
	owner->FirstTimerObj = this;
}

Timer::~Timer()
{
	Stop();

	if (PrevTimerObj)
		PrevTimerObj->NextTimerObj = NextTimerObj;
	if (NextTimerObj)
		NextTimerObj->PrevTimerObj = PrevTimerObj;
	if (OwnerObj->FirstTimerObj == this)
		OwnerObj->FirstTimerObj = NextTimerObj;
}

void Timer::Start(int timeoutMilliseconds, bool repeat)
{
	Stop();

	TimerId = DisplayWindow::StartTimer(timeoutMilliseconds, [=]() {
		if (!repeat)
			Stop();
		if (FuncExpired)
			FuncExpired();
	});
}

void Timer::Stop()
{
	if (TimerId != 0)
	{
		DisplayWindow::StopTimer(TimerId);
		TimerId = 0;
	}
}
