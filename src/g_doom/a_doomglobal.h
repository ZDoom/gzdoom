#ifndef __A_DOOMGLOBAL_H__
#define __A_DOOMGLOBAL_H__

#include "info.h"

class AScriptedMarine : public AActor
{
	DECLARE_CLASS (AScriptedMarine, AActor)
public:
	enum EMarineWeapon
	{
		WEAPON_Dummy,
		WEAPON_Fist,
		WEAPON_BerserkFist,
		WEAPON_Chainsaw,
		WEAPON_Pistol,
		WEAPON_Shotgun,
		WEAPON_SuperShotgun,
		WEAPON_Chaingun,
		WEAPON_RocketLauncher,
		WEAPON_PlasmaRifle,
		WEAPON_Railgun,
		WEAPON_BFG
	};

	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
	void BeginPlay ();
	void Tick ();
	void SetWeapon (EMarineWeapon);
	void SetSprite (const PClass *source);
	void Serialize (FArchive &arc);

	int CurrentWeapon;

protected:
	bool GetWeaponStates(int weap, FState *&melee, FState *&missile);

	int SpriteOverride;
};

#endif //__A_DOOMGLOBAL_H__
