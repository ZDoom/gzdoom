
#include "core/timer.h"
#include "core/widget.h"

Timer::Timer(Widget* owner) : OwnerObj(owner)
{
	PrevTimerObj = owner->FirstTimerObj;
	if (PrevTimerObj)
		PrevTimerObj->PrevTimerObj = this;
	owner->FirstTimerObj = this;
}

Timer::~Timer()
{
	if (PrevTimerObj)
		PrevTimerObj->NextTimerObj = NextTimerObj;
	if (NextTimerObj)
		NextTimerObj->PrevTimerObj = PrevTimerObj;
	if (OwnerObj->FirstTimerObj == this)
		OwnerObj->FirstTimerObj = NextTimerObj;
}

void Timer::Start(int timeoutMilliseconds, bool repeat)
{
}

void Timer::Stop()
{
}
