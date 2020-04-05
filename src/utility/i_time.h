#pragma once

#include <stdint.h>

extern int GameTicRate;
extern double TimeScale;

// Called by D_DoomLoop, sets the time for the current frame
void I_SetFrameTime();

// Called by D_DoomLoop, returns current time in tics.
int I_GetTime();

double I_GetTimeFrac();

// like I_GetTime, except it waits for a new tic before returning
int I_WaitForTic(int);

// Freezes tic counting temporarily. While frozen, calls to I_GetTime()
// will always return the same value.
// You must also not call I_WaitForTic() while freezing time, since the
// tic will never arrive (unless it's the current one).
void I_FreezeTime(bool frozen);

// [RH] Returns millisecond-accurate time
uint64_t I_msTime();

// [SP] Returns millisecond-accurate time from start
uint64_t I_msTimeFS();

// Nanosecond-accurate time
uint64_t I_nsTime();
