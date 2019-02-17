#ifndef __A_PICKUPS_H__
#define __A_PICKUPS_H__

#include "actor.h"
#include "info.h"
#include "s_sound.h"

#define NUM_WEAPON_SLOTS		10

class player_t;
class FConfigFile;

// This encapsulates the fields of vissprite_t that can be altered by AlterWeaponSprite
struct visstyle_t
{
	bool			Invert;
	float			Alpha;
	ERenderStyle	RenderStyle;
};



/************************************************************************/
/* Class definitions													*/
/************************************************************************/

// A pickup is anything the player can pickup (i.e. weapons, ammo, powerups, etc)

bool CallTryPickup(AActor *item, AActor *toucher, AActor **toucher_return = nullptr);
void DepleteOrDestroy(AActor *item);			// virtual on the script side. 

#endif //__A_PICKUPS_H__
