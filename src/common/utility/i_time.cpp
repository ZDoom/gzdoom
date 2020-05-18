/*
** i_time.cpp
** Implements the timer
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2017 Magnus Norddahl
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
#include "i_time.h"

//==========================================================================
//
// Tick time functions
//
//==========================================================================

static uint64_t FirstFrameStartTime;
static uint64_t CurrentFrameStartTime;
static uint64_t FreezeTime;
int GameTicRate;

double TimeScale = 1.0;

static uint64_t GetClockTimeNS()
{
	using namespace std::chrono;
	return (uint64_t)((duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) * (uint64_t)(TimeScale * 1000));
}

static uint64_t MSToNS(unsigned int ms)
{
	return static_cast<uint64_t>(ms) * 1'000'000;
}

static uint64_t NSToMS(uint64_t ns)
{
	return static_cast<uint64_t>(ns / 1'000'000);
}

static int NSToTic(uint64_t ns)
{
	return static_cast<int>(ns * GameTicRate / 1'000'000'000);
}

static uint64_t TicToNS(int tic)
{
	return static_cast<uint64_t>(tic) * 1'000'000'000 / GameTicRate;
}

void I_SetFrameTime()
{
	// Must only be called once per frame/swapbuffers.
	//
	// Caches all timing information for the current rendered frame so that any
	// calls to I_GetTime or I_GetTimeFrac will return
	// the same time.

	if (FreezeTime == 0)
	{
		CurrentFrameStartTime = GetClockTimeNS();
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
		// Windows-specific note:
		// The minimum amount of time a thread can sleep is controlled by timeBeginPeriod.
		// We set this to 1 ms in DoMain.

		const uint64_t next = FirstFrameStartTime + TicToNS(prevtic + 1);
		const uint64_t now = I_nsTime();

		if (next > now)
		{
			const uint64_t sleepTime = NSToMS(next - now);

			if (sleepTime > 2)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime - 2));
			}
		}

		I_SetFrameTime();
	}

	return time;
}

uint64_t I_nsTime()
{
	return GetClockTimeNS();
}

uint64_t I_msTime()
{
	return NSToMS(I_nsTime());
}

uint64_t I_msTimeFS() // from "start"
{
	return (FirstFrameStartTime == 0) ? 0 : NSToMS(I_nsTime() - FirstFrameStartTime);
}

int I_GetTime()
{
	return NSToTic(CurrentFrameStartTime - FirstFrameStartTime);
}

double I_GetTimeFrac()
{
	int currentTic = NSToTic(CurrentFrameStartTime - FirstFrameStartTime);
	uint64_t ticStartTime = FirstFrameStartTime + TicToNS(currentTic);
	uint64_t ticNextTime = FirstFrameStartTime + TicToNS(currentTic + 1);

	return (CurrentFrameStartTime - ticStartTime) / (double)(ticNextTime - ticStartTime);
}

void I_FreezeTime(bool frozen)
{
	if (frozen)
	{
		assert(FreezeTime == 0);
		FreezeTime = GetClockTimeNS();
	}
	else
	{
		assert(FreezeTime != 0);
		FirstFrameStartTime += GetClockTimeNS() - FreezeTime;
		FreezeTime = 0;
		I_SetFrameTime();
	}
}

