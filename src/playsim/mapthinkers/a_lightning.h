#ifndef __A_LIGHTNING_H__
#define __A_LIGHTNING_H__

#ifdef _MSC_VER
#pragma once
#endif

#include "dthinker.h"
#include "s_soundinternal.h"

class DLightningThinker : public DThinker
{
	DECLARE_CLASS (DLightningThinker, DThinker);
public:
	static const int DEFAULT_STAT = STAT_LIGHTNING;
	void Construct(FSoundID tempSound = NO_SOUND);
	~DLightningThinker ();
	void Serialize(FSerializer &arc);
	void Tick ();
	void ForceLightning (int mode, FSoundID tempSound = NO_SOUND);
	void TerminateLightning();

protected:
	void LightningFlash ();

	int NextLightningFlash;
	int LightningFlashCount;
	bool Stopped;
	FSoundID TempLightningSound;
	TArray<short> LightningLightLevels;
};


#endif //__A_LIGHTNING_H__
