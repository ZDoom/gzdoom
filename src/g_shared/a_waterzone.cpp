#include "info.h"
#include "r_defs.h"

class AWaterZone : public AActor
{
	DECLARE_STATELESS_ACTOR (AWaterZone, AActor)
public:
	void PostBeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (AWaterZone, Any, 9045, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
END_DEFAULTS

void AWaterZone::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	subsector->sector->waterzone = 1;
	Destroy ();
}

