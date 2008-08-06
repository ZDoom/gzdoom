#include "actor.h"
#include "m_random.h"
#include "p_local.h"
#include "c_console.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"
#include "thingdef/thingdef.h"

static FRandom pr_bang4cloud ("Bang4Cloud");
static FRandom pr_lightout ("LightOut");

extern const PClass *QuestItemClasses[31];

void A_Bang4Cloud (AActor *self)
{
	fixed_t spawnx, spawny;

	spawnx = self->x + (pr_bang4cloud.Random2() & 3) * 10240;
	spawny = self->y + (pr_bang4cloud.Random2() & 3) * 10240;

	Spawn("Bang4Cloud", spawnx, spawny, self->z, ALLOW_REPLACE);
}

// -------------------------------------------------------------------

void A_GiveQuestItem (AActor *self)
{
	int index=CheckIndex(1);
	if (index<0) return;

	int questitem = EvalExpressionI (StateParameters[index], self);
	
	// Give one of these quest items to every player in the game
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			AInventory *item = static_cast<AInventory *>(Spawn (QuestItemClasses[questitem-1], 0,0,0, NO_REPLACE));
			if (!item->TryPickup (players[i].mo))
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
		C_MidPrint (name);
	}
}

// PowerCrystal -------------------------------------------------------------------

void A_ExtraLightOff (AActor *self)
{
	if (self->target != NULL && self->target->player != NULL)
	{
		self->target->player->extralight = 0;
	}
}

void A_Explode512 (AActor *self)
{
	P_RadiusAttack (self, self->target, 512, 512, NAME_None, true);
	if (self->target != NULL && self->target->player != NULL)
	{
		self->target->player->extralight = 5;
	}
	if (self->z <= self->floorz + (512<<FRACBITS))
	{
		P_HitFloor (self);
	}

	// Strife didn't do this next part, but it looks good
	self->RenderStyle = STYLE_Add;
}

void A_LightGoesOut (AActor *self)
{
	AActor *foo;
	sector_t *sec = self->Sector;
	vertex_t *spot;
	fixed_t newheight;

	sec->lightlevel = 0;

	newheight = sec->FindLowestFloorSurrounding (&spot);
	sec->floorplane.d = sec->floorplane.PointToDist (spot, newheight);

	for (int i = 0; i < 8; ++i)
	{
		foo = Spawn("Rubble1", self->x, self->y, self->z, ALLOW_REPLACE);
		if (foo != NULL)
		{
			int t = pr_lightout() & 15;
			foo->momx = (t - (pr_lightout() & 7)) << FRACBITS;
			foo->momy = (pr_lightout.Random2() & 7) << FRACBITS;
			foo->momz = (7 + (pr_lightout() & 3)) << FRACBITS;
		}
	}
}
