#ifndef M_HAPTICS_H
#define M_HAPTICS_H

#include "name.h"

enum EHapticCompat {
	HAPTCOMPAT_WARN,
	HAPTCOMPAT_NONE,
	HAPTCOMPAT_MATCH,
	HAPTCOMPAT_ALL,

	NUM_HAPTCOMPAT
};

struct Haptics {
	int ticks;
	double high_frequency;
	double low_frequency;
	double left_trigger;
	double right_trigger;
};

void I_Rumble(double high_freq, double low_freq, double left_trig, double right_trig);

void Joy_AddRumbleType(const FName idenifier, const struct Haptics data);
void Joy_AddRumbleAlias(const FName alias, const FName actual);
void Joy_MapRumbleType(const FName sound, const FName idenifier);
void Joy_ResetRumbleMapping();
void Joy_ReadyRumbleMapping();
void Joy_RumbleTick();
void Joy_Rumble(const FName source, const struct Haptics data, double attenuation = 0);
void Joy_Rumble(const FName identifier, double attenuation = 0);

#endif
