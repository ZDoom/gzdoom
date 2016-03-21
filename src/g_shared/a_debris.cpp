#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "m_fixed.h"

static FRandom pr_dirt ("SpawnDirt");

// Stained glass ------------------------------------------------------------

class AGlassShard : public AActor
{
	DECLARE_CLASS (AGlassShard, AActor)
public:
	bool FloorBounceMissile (secplane_t &plane)
	{
		if (!Super::FloorBounceMissile (plane))
		{
			if (fabs (Vel.Z) < 0.5)
			{
				Destroy ();
			}
			return false;
		}
		return true;
	}
};

IMPLEMENT_CLASS(AGlassShard)

// Dirt stuff

void P_SpawnDirt (AActor *actor, double radius)
{
	PClassActor *dtype = NULL;
	AActor *mo;

	double zo = pr_dirt() / 128. + 1;
	DVector3 pos = actor->Vec3Angle(radius, pr_dirt() * (360./256), zo);

	char fmt[8];
	mysnprintf(fmt, countof(fmt), "Dirt%d", 1 + pr_dirt()%6);
	dtype = PClass::FindActor(fmt);
	if (dtype)
	{
		mo = Spawn (dtype, pos, ALLOW_REPLACE);
		if (mo)
		{
			mo->Vel.Z = pr_dirt() / 64.;
		}
	}
}
