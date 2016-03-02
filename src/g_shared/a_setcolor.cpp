#include "r_defs.h"
#include "actor.h"
#include "info.h"

class AColorSetter : public AActor
{
	DECLARE_CLASS(AColorSetter, AActor)

	void PostBeginPlay()
	{
		Super::PostBeginPlay();
		Sector->SetColor(args[0], args[1], args[2], args[3]);
		Destroy();
	}

};

IMPLEMENT_CLASS(AColorSetter)

class AFadeSetter : public AActor
{
	DECLARE_CLASS(AFadeSetter, AActor)

	void PostBeginPlay()
	{
		Super::PostBeginPlay();
		Sector->SetFade(args[0], args[1], args[2]);
		Destroy();
	}

};

IMPLEMENT_CLASS(AFadeSetter)
