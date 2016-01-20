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
			if (abs (velz) < (FRACUNIT/2))
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

void P_SpawnDirt (AActor *actor, fixed_t radius)
{
	const PClass *dtype = NULL;
	AActor *mo;

	fixed_t zo = (pr_dirt() << 9) + FRACUNIT;
	fixedvec3 pos = actor->Vec3Angle(radius, pr_dirt() << 24, zo);

	char fmt[8];
	mysnprintf(fmt, countof(fmt), "Dirt%d", 1 + pr_dirt()%6);
	dtype = PClass::FindClass(fmt);
	if (dtype)
	{
		mo = Spawn (dtype, pos, ALLOW_REPLACE);
		if (mo)
		{
			mo->velz = pr_dirt()<<10;
		}
	}
}
