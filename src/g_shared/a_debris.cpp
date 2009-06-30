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
	fixed_t x,y,z;
	const PClass *dtype = NULL;
	AActor *mo;
	angle_t angle;

	angle = pr_dirt()<<5;		// <<24 >>19
	x = actor->x + FixedMul(radius,finecosine[angle]);
	y = actor->y + FixedMul(radius,finesine[angle]);
	z = actor->z + (pr_dirt()<<9) + FRACUNIT;

	char fmt[8];
	mysnprintf(fmt, countof(fmt), "Dirt%d", 1 + pr_dirt()%6);
	dtype = PClass::FindClass(fmt);
	if (dtype)
	{
		mo = Spawn (dtype, x, y, z, ALLOW_REPLACE);
		if (mo)
		{
			mo->velz = pr_dirt()<<10;
		}
	}
}
