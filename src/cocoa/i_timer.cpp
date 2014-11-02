
#include <assert.h>
#include <sys/time.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>

#include <SDL.h>

#include "basictypes.h"
#include "basicinlines.h"
#include "doomdef.h"
#include "i_system.h"
#include "templates.h"


unsigned int I_MSTime()
{
	return SDL_GetTicks();
}

unsigned int I_FPSTime()
{
	return SDL_GetTicks();
}


bool g_isTicFrozen;


namespace
{

timespec GetNextTickTime()
{
	static const long MILLISECONDS_IN_SECOND = 1000;
	static const long MICROSECONDS_IN_SECOND = 1000 * MILLISECONDS_IN_SECOND;
	static const long NANOSECONDS_IN_SECOND  = 1000 * MICROSECONDS_IN_SECOND;

	static timespec ts = {};

	if (__builtin_expect((0 == ts.tv_sec), 0))
	{
		timeval tv;
		gettimeofday(&tv, NULL);

		ts.tv_sec = tv.tv_sec;
		ts.tv_nsec = (tv.tv_usec + MICROSECONDS_IN_SECOND / TICRATE) * MILLISECONDS_IN_SECOND;
	}
	else
	{
		ts.tv_nsec += (MICROSECONDS_IN_SECOND / TICRATE) * MILLISECONDS_IN_SECOND;
	}

	if (ts.tv_nsec >= NANOSECONDS_IN_SECOND)
	{
		ts.tv_sec++;
		ts.tv_nsec -= NANOSECONDS_IN_SECOND;
	}

	return ts;
}


pthread_cond_t  s_timerEvent;
pthread_mutex_t s_timerMutex;
pthread_t       s_timerThread;

bool s_timerInitialized;
bool s_timerExitRequested;

uint32_t s_ticStart;
uint32_t s_timerStart;

int  s_tics;


void* TimerThreadFunc(void*)
{
	assert(s_timerInitialized);
	assert(!s_timerExitRequested);

	while (true)
	{
		if (s_timerExitRequested)
		{
			break;
		}

		const timespec timeToNextTick = GetNextTickTime();

		pthread_mutex_lock(&s_timerMutex);
		pthread_cond_timedwait(&s_timerEvent, &s_timerMutex, &timeToNextTick);

		if (!g_isTicFrozen)
		{
			// The following GCC/Clang intrinsic can be used instead of OS X specific function:
			// __sync_add_and_fetch(&s_tics, 1);
			// Although it's not supported on all platform/compiler combination,
			// e.g. GCC 4.0.1 with PowerPC target architecture

			OSAtomicIncrement32(&s_tics);
		}

		s_timerStart = SDL_GetTicks();

		pthread_cond_broadcast(&s_timerEvent);
		pthread_mutex_unlock(&s_timerMutex);
	}

	return NULL;
}

int GetTimeThreaded(bool saveMS)
{
	if (saveMS)
	{
		s_ticStart = s_timerStart;
	}

	return s_tics;
}

int WaitForTicThreaded(int prevTic)
{
	assert(!g_isTicFrozen);

	while (s_tics <= prevTic)
	{
		pthread_mutex_lock(&s_timerMutex);
		pthread_cond_wait(&s_timerEvent, &s_timerMutex);
		pthread_mutex_unlock(&s_timerMutex);
	}

	return s_tics;
}

void FreezeTimeThreaded(bool frozen)
{
	g_isTicFrozen = frozen;
}

} // unnamed namespace


fixed_t I_GetTimeFrac(uint32* ms)
{
	const uint32_t now = SDL_GetTicks();

	if (NULL != ms)
	{
		*ms = s_ticStart + 1000 / TICRATE;
	}

	return 0 == s_ticStart
		? FRACUNIT
		: clamp<fixed_t>( (now - s_ticStart) * FRACUNIT * TICRATE / 1000, 0, FRACUNIT);
}


void I_InitTimer ()
{
	assert(!s_timerInitialized);
	s_timerInitialized = true;

	pthread_cond_init (&s_timerEvent,  NULL);
	pthread_mutex_init(&s_timerMutex,  NULL);

	pthread_create(&s_timerThread, NULL, TimerThreadFunc, NULL);

	I_GetTime = GetTimeThreaded;
	I_WaitForTic = WaitForTicThreaded;
	I_FreezeTime = FreezeTimeThreaded;
}

void I_ShutdownTimer ()
{
	if (!s_timerInitialized)
	{
		// This might happen if Cancel button was pressed
		// in the IWAD selector window
		return;
	}

	s_timerExitRequested = true;

	pthread_join(s_timerThread, NULL);

	pthread_mutex_destroy(&s_timerMutex);
	pthread_cond_destroy (&s_timerEvent);
}
