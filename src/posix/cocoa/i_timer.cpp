/*
 ** i_timer.cpp
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2015 Alexey Lysiuk
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 **
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **---------------------------------------------------------------------------
 **
 */

#include <assert.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>

#include "basictypes.h"
#include "basicinlines.h"
#include "doomdef.h"
#include "i_system.h"
#include "templates.h"


namespace
{

timeval s_gameStartTicks;
timeval s_systemBootTicks;

unsigned int GetMillisecondsSince(const timeval& time)
{
	timeval now;
	gettimeofday(&now, NULL);

	return static_cast<unsigned int>(
		  (now.tv_sec  - time.tv_sec ) * 1000
		+ (now.tv_usec - time.tv_usec) / 1000);
}


bool s_isTicFrozen;

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

		if (!s_isTicFrozen)
		{
			// The following GCC/Clang intrinsic can be used instead of OS X specific function:
			// __sync_add_and_fetch(&s_tics, 1);
			// Although it's not supported on all platform/compiler combination,
			// e.g. GCC 4.0.1 with PowerPC target architecture

			OSAtomicIncrement32(&s_tics);
		}

		s_timerStart = I_MSTime();

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
	assert(!s_isTicFrozen);

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
	s_isTicFrozen = frozen;
}

} // unnamed namespace


unsigned int I_MSTime()
{
	return GetMillisecondsSince(s_gameStartTicks);
}

unsigned int I_FPSTime()
{
	return GetMillisecondsSince(s_systemBootTicks);
}


double I_GetTimeFrac(uint32* ms)
{
	const uint32_t now = I_MSTime();

	if (NULL != ms)
	{
		*ms = s_ticStart + 1000 / TICRATE;
	}

	return 0 == s_ticStart
		? 1.
		: clamp<double>( (now - s_ticStart) * TICRATE / 1000., 0, 1);
}


void I_InitTimer()
{
	assert(!s_timerInitialized);
	s_timerInitialized = true;

	gettimeofday(&s_gameStartTicks, NULL);

	int mib[2] = { CTL_KERN, KERN_BOOTTIME };
	size_t len = sizeof s_systemBootTicks;

	sysctl(mib, 2, &s_systemBootTicks, &len, NULL, 0);

	pthread_cond_init (&s_timerEvent,  NULL);
	pthread_mutex_init(&s_timerMutex,  NULL);

	pthread_create(&s_timerThread, NULL, TimerThreadFunc, NULL);

	I_GetTime = GetTimeThreaded;
	I_WaitForTic = WaitForTicThreaded;
	I_FreezeTime = FreezeTimeThreaded;
}

void I_ShutdownTimer()
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
