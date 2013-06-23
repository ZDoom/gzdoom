
#ifndef __GLC_CONVERT
#define __GLC_CONVERT

#include "m_fixed.h"
#define ANGLE_TO_FLOAT(ang) ((float)((ang) * 180. / ANGLE_180))
#define FLOAT_TO_ANGLE(ang) xs_RoundToUInt((ang) / 180. * ANGLE_180)

#endif