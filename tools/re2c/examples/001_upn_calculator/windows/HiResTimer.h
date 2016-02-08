/** 
 * @file HiResTimer.h
 * @brief 
 * @note 
 */

#ifndef _HI_RES_TIMER_H_
#define _HI_RES_TIMER_H_

#ifdef WIN32
#include <windows.h>  // probably already done in stdafx.h
static LARGE_INTEGER start;
static LARGE_INTEGER stop;
static LARGE_INTEGER freq;
static _int64 elapsedCounts;
static double elapsedMillis;
static double elapsedMicros;
static HANDLE processHandle;
static DWORD  prevPriorityClass;

void HrtInit()
{
   processHandle = GetCurrentProcess();
   prevPriorityClass = GetPriorityClass(processHandle);
   QueryPerformanceFrequency(&freq);
}

void HrtStart()
{
   QueryPerformanceCounter(&start);
}

void HrtSetPriority(DWORD priority)
{
   int flag;
   prevPriorityClass = GetPriorityClass(processHandle);
   flag = SetPriorityClass(processHandle, priority);
}

void HrtResetPriority(void)
{
   int flag = SetPriorityClass(processHandle, prevPriorityClass);
}

double HrtElapsedMillis()
{
   QueryPerformanceCounter(&stop);
   elapsedCounts = (stop.QuadPart - start.QuadPart);
   elapsedMillis = ((elapsedCounts * 1000.0) / freq.QuadPart);
   return elapsedMillis;
}

#endif
#endif