#pragma once

#include <stdint.h>

extern int GameTicRate;
extern double TimeScale;

// Called by D_DoomLoop, sets the time for the current frame
void I_SetFrameTime();

// Called by D_DoomLoop, returns current time in tics.
int I_GetTime();
// same, but using nanoseconds
uint64_t I_GetTimeNS();

// Called by Build games in lieu of totalclock, returns current time in tics at ticrate of 120.
int I_GetBuildTime();

double I_GetTimeFrac();
double I_GetBuildTimeFrac();

// like I_GetTime, except it waits for a new tic before returning
int I_WaitForTic(int);

// Freezes tic counting temporarily. While frozen, calls to I_GetTime()
// will always return the same value.
// You must also not call I_WaitForTic() while freezing time, since the
// tic will never arrive (unless it's the current one).
void I_FreezeTime(bool frozen);

// [RH] Returns millisecond-accurate time
uint64_t I_msTime();

// [RH] Returns nanosecond-accurate time in milliseconds
double I_msTimeF(void);

// [SP] Returns millisecond-accurate time from start
uint64_t I_msTimeFS();

// Nanosecond-accurate time
uint64_t I_nsTime();
