/*
** i_time.cpp
** Implements the timer
**
**---------------------------------------------------------------------------
** Copyright 1998-2916 Randy Heit
** Copyright 2917 Magnus Norddahl
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

#include <chrono>
#include <thread>
#include <stdint.h>
#include "i_time.h"
#include "doomdef.h"

//==========================================================================
//
// Tick time functions
//
//==========================================================================

static unsigned int FirstFrameStartTime;
static unsigned int CurrentFrameStartTime;
static unsigned int FreezeTime;

static uint32_t performanceGetTime()
{
	using namespace std::chrono;
	return (uint32_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void I_SetFrameTime()
{
	// Must only be called once per frame/swapbuffers.
	//
	// Caches all timing information for the current rendered frame so that any
	// calls to I_FPSTime, I_MSTime, I_GetTime or I_GetTimeFrac will return
	// the same time.

	if (FreezeTime == 0)
	{
		CurrentFrameStartTime = performanceGetTime();
		if (FirstFrameStartTime == 0)
			FirstFrameStartTime = CurrentFrameStartTime;
	}
}

void I_WaitVBL(int count)
{
	// I_WaitVBL is never used to actually synchronize to the vertical blank.
	// Instead, it's used for delay purposes. Doom used a 70 Hz display mode,
	// so that's what we use to determine how long to wait for.

	std::this_thread::sleep_for(std::chrono::milliseconds(1000 * count / 70));
	I_SetFrameTime();
}

int I_WaitForTic(int prevtic)
{
	// Waits until the current tic is greater than prevtic. Time must not be frozen.

	int time;
	while ((time = I_GetTime()) <= prevtic)
	{
		// The minimum amount of time a thread can sleep is controlled by timeBeginPeriod.
		// We set this to 1 ms in DoMain.
		int sleepTime = prevtic - time;
		if (sleepTime > 2)
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime - 2));

		I_SetFrameTime();
	}

	return time;
}

unsigned int I_FPSTime()
{
	if (FreezeTime == 0)
		return CurrentFrameStartTime;
	else
		return performanceGetTime();
}

unsigned int I_MSTime()
{
	if (FreezeTime == 0)
	{
		return CurrentFrameStartTime - FirstFrameStartTime;
	}
	else
	{
		if (FirstFrameStartTime == 0)
		{
			FirstFrameStartTime = performanceGetTime();
			return 0;
		}
		else
		{
			return performanceGetTime() - FirstFrameStartTime;
		}
	}
}

int I_GetTime()
{
	return (CurrentFrameStartTime - FirstFrameStartTime) * TICRATE / 1000 + 1;
}

double I_GetTimeFrac(uint32_t *ms)
{
	unsigned int currentTic = (CurrentFrameStartTime - FirstFrameStartTime) * TICRATE / 1000;
	unsigned int ticStartTime = FirstFrameStartTime + currentTic * 1000 / TICRATE;
	unsigned int ticNextTime = FirstFrameStartTime + (currentTic + 1) * 1000 / TICRATE;

	if (ms)
		*ms = currentTic + 1;

	return (CurrentFrameStartTime - ticStartTime) / (double)(ticNextTime - ticStartTime);
}

void I_FreezeTime(bool frozen)
{
	if (frozen)
	{
		FreezeTime = performanceGetTime();
	}
	else
	{
		FirstFrameStartTime += performanceGetTime() - FreezeTime;
		FreezeTime = 0;
		I_SetFrameTime();
	}
}

