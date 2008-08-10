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
#include "thingdef/thingdef.h"

// The barrel of green goop ------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_BarrelDestroy)
{
	if ((dmflags2 & DF2_BARRELS_RESPAWN) &&
		(deathmatch || alwaysapplydmflags))
	{
		self->height = self->GetDefault()->height;
		self->renderflags |= RF_INVISIBLE;
		self->flags &= ~MF_SOLID;
	}
	else
	{
		self->Destroy ();
	}
}

