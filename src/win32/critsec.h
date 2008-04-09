// Wraps a Windows critical section object.

#ifndef CRITSEC_H
#define CRITSEC_H

#ifndef _WINNT_
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define USE_WINDOWS_DWORD
#endif

class FCriticalSection
{
public:
	FCriticalSection()
	{
		InitializeCriticalSection(&CritSec);
	}
	~FCriticalSection()
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

#endif
