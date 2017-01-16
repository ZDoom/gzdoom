#ifndef A_KEYS_H
#define A_KEYS_H

#include "a_pickups.h"

class AKey : public AInventory
{
	DECLARE_CLASS (AKey, AInventory)
public:
	BYTE KeyNumber;
};

bool P_CheckKeys (AActor *owner, int keynum, bool remote);
void P_InitKeyMessages ();
void P_DeinitKeyMessages ();
int P_GetMapColorForLock (int lock);
int P_GetMapColorForKey (AInventory *key);

#endif
