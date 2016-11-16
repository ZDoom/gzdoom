#include "actor.h"
#include "info.h"
#include "m_random.h"
#include "m_fixed.h"

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

IMPLEMENT_CLASS(AGlassShard, false, false, false, false)

