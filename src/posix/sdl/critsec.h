// Wraps an SDL mutex object. (A critical section is a Windows synchronization
// object similar to a mutex but optimized for access by threads belonging to
// only one process, hence the class name.)

#ifndef CRITSEC_H
#define CRITSEC_H

#include "SDL.h"
#include "SDL_thread.h"
#include "i_system.h"

class FCriticalSection
{
public:
	FCriticalSection()
	{
		CritSec = SDL_CreateMutex();
		if (CritSec == NULL)
		{
			I_FatalError("Failed to create a critical section mutex.");
		}
	}
	~FCriticalSection()
	{
		if (CritSec != NULL)
		{
			SDL_DestroyMutex(CritSec);
		}
	}
	void Enter()
	{
		if (SDL_mutexP(CritSec) != 0)
		{
			I_FatalError("Failed entering a critical section.");
		}
	}
	void Leave()
	{
		if (SDL_mutexV(CritSec) != 0)
		{
			I_FatalError("Failed to leave a critical section.");
		}
	}
private:
	SDL_mutex *CritSec;
};

#endif
