#ifndef __A_HEXENGLOBAL_H__
#define __A_HEXENGLOBAL_H__

#include "a_hereticglobal.h"

class ADarkServant : public AMinotaur
{
	DECLARE_ACTOR (ADarkServant, AMinotaur)
public:
	AActor *master;
};

class ALightning : public AActor
{
	DECLARE_ACTOR (ALightning, AActor)
};

class APoisonCloud : public AActor
{
	DECLARE_ACTOR (APoisonCloud, AActor)
};

#endif //__A_HEXENGLOBAL_H__