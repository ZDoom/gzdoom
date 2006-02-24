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
	void ForceLightning ();

protected:
	void LightningFlash ();

	int NextLightningFlash;
	int LightningFlashCount;
	BYTE *LightningLightLevels;
};

void P_StartLightning ();
void P_StopLightning ();
void P_ForceLightning ();

#endif //__A_LIGHTNING_H__
