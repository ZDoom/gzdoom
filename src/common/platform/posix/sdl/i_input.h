#ifndef I_INPUT_H
#define I_INPUT_H

#include "d_eventbase.h"

extern int WaitingForKey;

static void I_CheckGUICapture ();
static void I_CheckNativeMouse ();

void I_JoyConsumeEvent(int instanceID, event_t * event);

#endif
