#ifndef __A_HEXENGLOBAL_H__
#define __A_HEXENGLOBAL_H__

#include "d_player.h"

class AArtiPoisonBag : public AInventory
{
	DECLARE_CLASS (AArtiPoisonBag, AInventory)
public:
	bool HandlePickup (AInventory *item);
	AInventory *CreateCopy (AActor *other);
	void BeginPlay ();
};

#endif //__A_HEXENGLOBAL_H__
