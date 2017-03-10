// Wraps a Windows critical section object.

#ifndef _WINNT_
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

class FInternalCriticalSection
{
public:
	FInternalCriticalSection()
	{
		InitializeCriticalSection(&CritSec);
	}
	~FInternalCriticalSection()
	{
		DeleteCriticalSection(&CritSec);
	}
	void Enter()
	{
		EnterCriticalSection(&CritSec);
	}
	void Leave()
	{
		LeaveCriticalSection(&CritSec);
	}
#if 0
	// SDL has no equivalent functionality, so better not use it on Windows.
	bool TryEnter()
	{
		return TryEnterCriticalSection(&CritSec) != 0;
	}
#endif
private:
	CRITICAL_SECTION CritSec;
};


FInternalCriticalSection *CreateCriticalSection()
{
	return new FInternalCriticalSection();
}

void DeleteCriticalSection(FInternalCriticalSection *c)
{
	delete c;
}

void EnterCriticalSection(FInternalCriticalSection *c)
{
	c->Enter();
}

void LeaveCriticalSection(FInternalCriticalSection *c)
{
	c->Leave();
}
