#pragma once

#include <stdint.h>
#include "d_event.h"

class ShiftState
{
	bool ShiftStatus = false;

public:

	bool ShiftPressed()
	{
		return ShiftStatus;
	}

	void AddEvent(const event_t *ev)
	{
		if ((ev->type == EV_KeyDown || ev->type == EV_KeyUp) && (ev->data1 == KEY_LSHIFT || ev->data1 == KEY_RSHIFT))
		{
			ShiftStatus = ev->type == EV_KeyDown;
		}
	}

	void Clear()
	{
		ShiftStatus = false;
	}


};

inline ShiftState shiftState;
