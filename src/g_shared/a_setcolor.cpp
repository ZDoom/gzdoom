#include "r_defs.h"
#include "actor.h"
#include "info.h"

class AColorSetter : public AActor
{
	DECLARE_STATELESS_ACTOR(AColorSetter, AActor)

	void PostBeginPlay()
	{
		Super::PostBeginPlay();
		Sector->SetColor(args[0], args[1], args[2], args[3]);
		Destroy();
	}

};

IMPLEMENT_STATELESS_ACTOR(AColorSetter, Any, 9038, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_DONTSPLASH)
	PROP_RenderStyle (STYLE_None)
END_DEFAULTS


class AFadeSetter : public AActor
{
	DECLARE_STATELESS_ACTOR(AFadeSetter, AActor)

	void PostBeginPlay()
	{
		Super::PostBeginPlay();
		Sector->SetFade(args[0], args[1], args[2]);
		Destroy();
	}

};

IMPLEMENT_STATELESS_ACTOR(AFadeSetter, Any, 9039, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags3 (MF3_DONTSPLASH)
	PROP_RenderStyle (STYLE_None)
END_DEFAULTS
