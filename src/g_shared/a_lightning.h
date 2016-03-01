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
	DLightningThinker ();
	~DLightningThinker ();
	void Serialize (FArchive &arc);
	void Tick ();
	void ForceLightning (int mode);
	void TerminateLightning();

protected:
	void LightningFlash ();

	int NextLightningFlash;
	int LightningFlashCount;
	bool Stopped;
	short *LightningLightLevels;
};

void P_StartLightning ();
void P_ForceLightning (int mode);

#endif //__A_LIGHTNING_H__
