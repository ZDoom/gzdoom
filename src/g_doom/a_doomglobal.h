#ifndef __A_DOOMGLOBAL_H__
#define __A_DOOMGLOBAL_H__

#include "info.h"

class AScriptedMarine : public AActor
{
	DECLARE_CLASS (AScriptedMarine, AActor)
public:

	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
	void BeginPlay ();
	void Tick ();
	void SetWeapon (EMarineWeapon);
	void SetSprite (PClassActor *source);
	
	void Serialize(FSerializer &arc);

	int CurrentWeapon;

protected:
	bool GetWeaponStates(int weap, FState *&melee, FState *&missile);

	int SpriteOverride;
};

#endif //__A_DOOMGLOBAL_H__
