#ifndef __FMOPL_H_
#define __FMOPL_H_

#include "opl.h"

// Multiplying OPL_SAMPLE_RATE by ADLIB_CLOCK_MUL gives the number
// Adlib clocks per second, as used by the RAWADATA file format.

OPLEmul *YM3812Init(bool stereo);

#endif
