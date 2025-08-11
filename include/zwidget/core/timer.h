#pragma once

#include <functional>

class Widget;

class Timer
{
public:
	Timer(Widget* owner);
	~Timer();

	void Start(int timeoutMilliseconds, bool repeat = true);
	void Stop();

	std::function<void()> FuncExpired;

private:
	Widget* OwnerObj = nullptr;
	Timer* PrevTimerObj = nullptr;
	Timer* NextTimerObj = nullptr;

	void* TimerId = nullptr;

	friend class Widget;
};
