#include "actor.h"
#include "info.h"
#include "p_effect.h"

class AParticleFountain : public AActor
{
	DECLARE_STATELESS_ACTOR (AParticleFountain, AActor)
public:
	void PostBeginPlay ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

IMPLEMENT_STATELESS_ACTOR (AParticleFountain, Any, -1, 0)
	PROP_HeightFixed (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderFlags (RF_INVISIBLE)
END_DEFAULTS

#define FOUNTAIN(color,ednum) \
	class A##color##ParticleFountain : public AParticleFountain { \
		DECLARE_STATELESS_ACTOR (A##color##ParticleFountain, AParticleFountain) }; \
	IMPLEMENT_STATELESS_ACTOR (A##color##ParticleFountain, Any, ednum, 0) \
		PROP_SpawnHealth (ednum-9026) \
	END_DEFAULTS

FOUNTAIN (Red, 9027);
FOUNTAIN (Green, 9028);
FOUNTAIN (Blue, 9029);
FOUNTAIN (Yellow, 9030);
FOUNTAIN (Purple, 9031);
FOUNTAIN (Black, 9032);
FOUNTAIN (White, 9033);

void AParticleFountain::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	if (!(mapflags & MTF_DORMANT))
		Activate (NULL);
}

void AParticleFountain::Activate (AActor *activator)
{
	Super::Activate (activator);
	effects &= ~FX_FOUNTAINMASK;
	effects |= health << FX_FOUNTAINSHIFT;
}

void AParticleFountain::Deactivate (AActor *activator)
{
	Super::Deactivate (activator);
	effects &= ~FX_FOUNTAINMASK;
}

