#include "actor.h"
#include "info.h"
#include "p_enemy.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "a_sharedglobal.h"
#include "m_random.h"
#include "gi.h"
#include "doomstat.h"
#include "gstrings.h"

// The barrel of green goop ------------------------------------------------

void A_BarrelDestroy (AActor *actor)
{
	if ((dmflags2 & DF2_BARRELS_RESPAWN) &&
		(deathmatch || alwaysapplydmflags))
	{
		actor->height = actor->GetDefault()->height;
		actor->renderflags |= RF_INVISIBLE;
		actor->flags &= ~MF_SOLID;
	}
	else
	{
		actor->Destroy ();
	}
}

