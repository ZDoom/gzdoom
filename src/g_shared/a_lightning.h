#ifndef __A_LIGHTNING_H__
#define __A_LIGHTNING_H__

#ifdef _MSC_VER
#pragma once
#endif

#include "dthinker.h"

class DLightningThinker : public DThinker
{
	DECLARE_CLASS (DLightningThinker, DThinker);
public:
	static const int DEFAULT_STAT = STAT_LIGHTNING;
	void Construct();
	~DLightningThinker ();
	void Serialize(FSerializer &arc);
	void Tick ();
	void ForceLightning (int mode);
	void TerminateLightning();

protected:
	void LightningFlash ();

	int NextLightningFlash;
	int LightningFlashCount;
	bool Stopped;
	TArray<short> LightningLightLevels;
};


#endif //__A_LIGHTNING_H__
