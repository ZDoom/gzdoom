#ifndef A_KEYS_H
#define A_KEYS_H

#include "a_pickups.h"

class AKey : public AInventory
{
	DECLARE_STATELESS_ACTOR (AKey, AInventory)
public:
	virtual bool HandlePickup (AInventory *item);

	BYTE KeyNumber;

protected:
	virtual bool ShouldStay ();
};

class ADoomKey : public AKey
{
	DECLARE_STATELESS_ACTOR (ADoomKey, AKey)
};

class AHereticKey : public AKey
{
	DECLARE_STATELESS_ACTOR (AHereticKey, AKey)
};

class AHexenKey : public AKey
{
	DECLARE_STATELESS_ACTOR (AHexenKey, AKey)
};

class AStrifeKey : public AKey
{
	DECLARE_STATELESS_ACTOR (AStrifeKey, AKey)
};

bool P_CheckKeys (AActor *owner, int keynum, bool remote);
void P_InitKeyMessages ();
int P_GetMapColorForLock (int lock);
int P_GetMapColorForKey (AInventory *key);

#endif
