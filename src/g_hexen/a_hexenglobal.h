#ifndef __A_HEXENGLOBAL_H__
#define __A_HEXENGLOBAL_H__

#include "d_player.h"

EXTERN_CVAR (Int, paletteflash)
enum PaletteFlashFlags
{
	PF_HEXENWEAPONS		= 1,
	PF_SPECIALDAMAGE	= 2,
};

class AHolySpirit : public AActor
{
	DECLARE_CLASS (AHolySpirit, AActor)
public:
	bool Slam (AActor *thing);
	bool SpecialBlastHandling (AActor *source, fixed_t strength);
};

class AFighterWeapon : public AWeapon
{
	DECLARE_CLASS (AFighterWeapon, AWeapon);
public:
};

class AClericWeapon : public AWeapon
{
	DECLARE_CLASS (AClericWeapon, AWeapon);
public:
};

class AMageWeapon : public AWeapon
{
	DECLARE_CLASS (AMageWeapon, AWeapon);
public:
};

#endif //__A_HEXENGLOBAL_H__
