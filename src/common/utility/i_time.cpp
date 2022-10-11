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
#include <assert.h>
#include "i_time.h"

//==========================================================================
//
// Tick time functions
//
//==========================================================================

static uint64_t StartupTimeNS;
static uint64_t FirstFrameStartTime;
static uint64_t CurrentFrameStartTime;
static uint64_t FreezeTime;
static double lastinputtime;
int GameTicRate = 35;	// make sure it is not 0, even if the client doesn't set it.

double TimeScale = 1.0;

static uint64_t GetTimePoint()
{
	using namespace std::chrono;
	return (uint64_t)(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

void I_InitTime()
{
	StartupTimeNS = GetTimePoint();
}

static uint64_t GetClockTimeNS()
{
	auto tp = GetTimePoint() - StartupTimeNS;
	if (TimeScale == 1.0) return tp;
	else return tp / 1000 * TimeScale * 1000;
}

static uint64_t MSToNS(unsigned int ms)
{
	return static_cast<uint64_t>(ms) * 1'000'000;
}

static uint64_t NSToMS(uint64_t ns)
{
	return static_cast<uint64_t>(ns / 1'000'000);
}

static int NSToTic(uint64_t ns, double const ticrate)
{
	return static_cast<int>(ns * ticrate / 1'000'000'000);
}

static uint64_t TicToNS(double tic, double const ticrate)
{
	return static_cast<uint64_t>(tic * 1'000'000'000 / ticrate);
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

int I_WaitForTic(int prevtic, double const ticrate)
{
	// Waits until the current tic is greater than prevtic. Time must not be frozen.

	int time;
	while ((time = I_GetTime(ticrate)) <= prevtic)
	{
		// Windows-specific note:
		// The minimum amount of time a thread can sleep is controlled by timeBeginPeriod.
		// We set this to 1 ms in DoMain.

		const uint64_t next = FirstFrameStartTime + TicToNS(prevtic + 1, ticrate);
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

double I_msTimeF(void)
{
	return I_nsTime() / 1'000'000.;
}

uint64_t I_msTimeFS() // from "start"
{
	return (FirstFrameStartTime == 0) ? 0 : NSToMS(I_nsTime() - FirstFrameStartTime);
}

uint64_t I_GetTimeNS()
{
	return CurrentFrameStartTime - FirstFrameStartTime;
}

int I_GetTime(double const ticrate)
{
	return NSToTic(CurrentFrameStartTime - FirstFrameStartTime, ticrate);
}

double I_GetTimeFrac(double const ticrate)
{
	int currentTic = NSToTic(CurrentFrameStartTime - FirstFrameStartTime, ticrate);
	uint64_t ticStartTime = FirstFrameStartTime + TicToNS(currentTic, ticrate);
	uint64_t ticNextTime = FirstFrameStartTime + TicToNS(currentTic + 1, ticrate);

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
		if (FirstFrameStartTime != 0) FirstFrameStartTime += GetClockTimeNS() - FreezeTime;
		FreezeTime = 0;
		I_SetFrameTime();
	}
}

void I_ResetFrameTime()
{
	// Reset the starting point of the current frame to now. For use after lengthy operations that should not result in tic accumulation.
	auto ft = CurrentFrameStartTime;
	I_SetFrameTime();
	FirstFrameStartTime += (CurrentFrameStartTime - ft);
}

double I_GetInputFrac(bool const synchronised)
{
	if (!synchronised)
	{
		const double now = I_msTimeF();
		const double result = (now - lastinputtime) * GameTicRate * (1. / 1000.);
		lastinputtime = now;

		if (result < 1)
		{
			// Calculate an amplification to apply to the result before returning,
			// factoring in the game's ticrate and the value of the result.
			// This rectifies a deviation of 100+ ms or more depending on the length
			// of the operation to be within 1-2 ms of synchronised input
			// from 60 fps to at least 1000 fps at ticrates of 30 and 40 Hz.
			return result * (1. + 0.35 * (1. - GameTicRate * (1. / 50.)) * (1. - result));
		}
	}

	return 1;
}

void I_ResetInputTime()
{
	// Reset lastinputtime to current time.
	lastinputtime = I_msTimeF();
}
