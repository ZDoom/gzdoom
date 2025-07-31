// ---------------------------------------------------------------------------
//	FM sound generator common timer module
//	Copyright (C) cisc 1998, 2000.
// ---------------------------------------------------------------------------
//	$Id: fmtimer.h,v 1.2 2003/04/22 13:12:53 cisc Exp $

#ifndef FM_TIMER_H
#define FM_TIMER_H

#include "fmgen_types.h"

// ---------------------------------------------------------------------------

namespace FM
{
	struct TimerData {
		uint8	status;
		uint8	regtc;
		uint8	regta[2];
		int32	timera, timera_count;
		int32	timerb, timerb_count;
		int32	timer_step;
	};

	class Timer
	{
	public:
		void	Reset();
		bool	Count(int32 us);
		int32	GetNextEvent();

		void    DataSave(struct TimerData* data);
		void    DataLoad(struct TimerData* data);
	
	protected:
		virtual void SetStatus(uint bit) = 0;
		virtual void ResetStatus(uint bit) = 0;

		void	SetTimerBase(uint clock);
		void	SetTimerA(uint addr, uint data);
		void	SetTimerB(uint data);
		void	SetTimerControl(uint data);
		
		uint8	status;
		uint8	regtc;
	
	private:
		virtual void TimerA() {}
		uint8	regta[2];
		
		int32	timera, timera_count;
		int32	timerb, timerb_count;
		int32	timer_step;
	};

// ---------------------------------------------------------------------------
//	èâä˙âª
//
inline void Timer::Reset()
{
	timera_count = 0;
	timerb_count = 0;
}

} // namespace FM

#endif // FM_TIMER_H
