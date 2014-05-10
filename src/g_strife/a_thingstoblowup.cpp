/*
#include "actor.h"
#include "m_random.h"
#include "p_local.h"
#include "c_console.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

static FRandom pr_bang4cloud ("Bang4Cloud");
static FRandom pr_lightout ("LightOut");

extern const PClass *QuestItemClasses[31];

DEFINE_ACTION_FUNCTION(AActor, A_Bang4Cloud)
{
	fixed_t spawnx, spawny;

	spawnx = self->x + (pr_bang4cloud.Random2() & 3) * 10240;
	spawny = self->y + (pr_bang4cloud.Random2() & 3) * 10240;

	Spawn("Bang4Cloud", spawnx, spawny, self->z, ALLOW_REPLACE);
}

// -------------------------------------------------------------------

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveQuestItem)
{
	ACTION_PARAM_START(1);
	ACTION_PARAM_INT(questitem, 0);

	// Give one of these quest items to every player in the game
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			AInventory *item = static_cast<AInventory *>(Spawn (QuestItemClasses[questitem-1], 0,0,0, NO_REPLACE));
			if (!item->CallTryPickup (players[i].mo))
			{
				item->Destroy ();
			}
		}
	}

	char messageid[64];

	mysnprintf(messageid, countof(messageid), "TXT_QUEST_%d", questitem);
	const char * name = GStrings[messageid];

	if (name != NULL)
	{
		C_MidPrint (SmallFont, name);
	}
}

// PowerCrystal -------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_ExtraLightOff)
{
	if (self->target != NULL && self->target->player != NULL)
	{
		self->target->player->extralight = 0;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_Explode512)
{
	P_RadiusAttack (self, self->target, 512, 512, NAME_None, RADF_HURTSOURCE);
	if (self->target != NULL && self->target->player != NULL)
	{
		self->target->player->extralight = 5;
	}
	P_CheckSplash(self, 512<<FRACBITS);

	// Strife didn't do this next part, but it looks good
	self->RenderStyle = STYLE_Add;
}

DEFINE_ACTION_FUNCTION(AActor, A_LightGoesOut)
{
	AActor *foo;
	sector_t *sec = self->Sector;
	vertex_t *spot;
	fixed_t newheight;

	sec->SetLightLevel(0);

	fixed_t oldtheight = sec->floorplane.Zat0();
	newheight = sec->FindLowestFloorSurrounding(&spot);
	sec->floorplane.d = sec->floorplane.PointToDist (spot, newheight);
	fixed_t newtheight = sec->floorplane.Zat0();
	sec->ChangePlaneTexZ(sector_t::floor, newtheight - oldtheight);

	for (int i = 0; i < 8; ++i)
	{
		foo = Spawn("Rubble1", self->x, self->y, self->z, ALLOW_REPLACE);
		if (foo != NULL)
		{
			int t = pr_lightout() & 15;
			foo->velx = (t - (pr_lightout() & 7)) << FRACBITS;
			foo->vely = (pr_lightout.Random2() & 7) << FRACBITS;
			foo->velz = (7 + (pr_lightout() & 3)) << FRACBITS;
		}
	}
}
