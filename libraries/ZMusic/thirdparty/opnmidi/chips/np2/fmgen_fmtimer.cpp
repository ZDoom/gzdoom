// ---------------------------------------------------------------------------
//	FM sound generator common timer module
//	Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//	$Id: fmtimer.cpp,v 1.1 2000/09/08 13:45:56 cisc Exp $

#include "fmgen_headers.h"
#include "fmgen_fmtimer.h"

using namespace FM;

// ---------------------------------------------------------------------------
//	タイマー制御
//
void Timer::SetTimerControl(uint data)
{
	uint tmp = regtc ^ data;
	regtc = uint8(data);
	
	if (data & 0x10) 
		ResetStatus(1);
	if (data & 0x20) 
		ResetStatus(2);

	if (tmp & 0x01)
		timera_count = (data & 1) ? timera : 0;
	if (tmp & 0x02)
		timerb_count = (data & 2) ? timerb : 0;
}

#if 1

// ---------------------------------------------------------------------------
//	タイマーA 周期設定
//
void Timer::SetTimerA(uint addr, uint data)
{
	uint tmp;
	regta[addr & 1] = uint8(data);
	tmp = (regta[0] << 2) + (regta[1] & 3);
	timera = (1024-tmp) * timer_step;
//	LOG2("Timer A = %d   %d us\n", tmp, timera >> 16);
}

// ---------------------------------------------------------------------------
//	タイマーB 周期設定
//
void Timer::SetTimerB(uint data)
{
	timerb = (256-data) * timer_step;
//	LOG2("Timer B = %d   %d us\n", data, timerb >> 12);
}

// ---------------------------------------------------------------------------
//	タイマー時間処理
//
bool Timer::Count(int32 us)
{
	bool event = false;

	if (timera_count)
	{
		timera_count -= us << 16;
		if (timera_count <= 0)
		{
			event = true;
			TimerA();

			while (timera_count <= 0)
				timera_count += timera;
			
			if (regtc & 4)
				SetStatus(1);
		}
	}
	if (timerb_count)
	{
		timerb_count -= us << 12;
		if (timerb_count <= 0)
		{
			event = true;
			while (timerb_count <= 0)
				timerb_count += timerb;
			
			if (regtc & 8)
				SetStatus(2);
		}
	}
	return event;
}

// ---------------------------------------------------------------------------
//	次にタイマーが発生するまでの時間を求める
//
int32 Timer::GetNextEvent()
{
	uint32 ta = ((timera_count + 0xffff) >> 16) - 1;
	uint32 tb = ((timerb_count + 0xfff) >> 12) - 1;
	return (ta < tb ? ta : tb) + 1;
}

// ---------------------------------------------------------------------------
void Timer::DataSave(struct TimerData* data)
{
	data->status = status;
	data->regtc = regtc;
	data->regta[0] = regta[0];
	data->regta[1] = regta[1];
	data->timera = timera;
	data->timera_count = timera_count;
	data->timerb = timerb;
	data->timerb_count = timerb_count;
	data->timer_step = timer_step;
}

// ---------------------------------------------------------------------------
void Timer::DataLoad(struct TimerData* data)
{
	status = data->status;
	regtc = data->regtc;
	regta[0] = data->regta[0];
	regta[1] = data->regta[1];
	timera = data->timera;
	timera_count = data->timera_count;
	timerb = data->timerb;
	timerb_count = data->timerb_count;
	timer_step = data->timer_step;
}

// ---------------------------------------------------------------------------
//	タイマー基準値設定
//
void Timer::SetTimerBase(uint clock)
{
	timer_step = int32(1000000. * 65536 / clock);
}

#else

// ---------------------------------------------------------------------------
//	タイマーA 周期設定
//
void Timer::SetTimerA(uint addr, uint data)
{
	regta[addr & 1] = uint8(data);
	timera = (1024 - ((regta[0] << 2) + (regta[1] & 3))) << 16;
}

// ---------------------------------------------------------------------------
//	タイマーB 周期設定
//
void Timer::SetTimerB(uint data)
{
	timerb = (256-data) << (16 + 4);
}

// ---------------------------------------------------------------------------
//	タイマー時間処理
//
bool Timer::Count(int32 us)
{
	bool event = false;

	int tick = us * timer_step;

	if (timera_count)
	{
		timera_count -= tick;
		if (timera_count <= 0)
		{
			event = true;
			TimerA();

			while (timera_count <= 0)
				timera_count += timera;
			
			if (regtc & 4)
				SetStatus(1);
		}
	}
	if (timerb_count)
	{
		timerb_count -= tick;
		if (timerb_count <= 0)
		{
			event = true;
			while (timerb_count <= 0)
				timerb_count += timerb;
			
			if (regtc & 8)
				SetStatus(2);
		}
	}
	return event;
}

// ---------------------------------------------------------------------------
//	次にタイマーが発生するまでの時間を求める
//
int32 Timer::GetNextEvent()
{
	uint32 ta = timera_count - 1;
	uint32 tb = timerb_count - 1;
	uint32 t = (ta < tb ? ta : tb) + 1;

	return (t+timer_step-1) / timer_step;
}

// ---------------------------------------------------------------------------
//	タイマー基準値設定
//
void Timer::SetTimerBase(uint clock)
{
	timer_step = clock * 1024 / 15625;
}

#endif
