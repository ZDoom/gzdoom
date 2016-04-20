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

DEFINE_ACTION_FUNCTION(AActor, A_Bang4Cloud)
{
	PARAM_ACTION_PROLOGUE;

	double xo = (pr_bang4cloud.Random2() & 3) * (10. / 64);
	double yo = (pr_bang4cloud.Random2() & 3) * (10. / 64);
	Spawn("Bang4Cloud", self->Vec3Offset(xo, yo, 0.), ALLOW_REPLACE);
	return 0;
}

// -------------------------------------------------------------------

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_GiveQuestItem)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(questitem);

	// Give one of these quest items to every player in the game
	if (questitem >= 0 && questitem < (int)countof(QuestItemClasses))
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				AInventory *item = static_cast<AInventory *>(Spawn (QuestItemClasses[questitem - 1]));
				if (!item->CallTryPickup (players[i].mo))
				{
					item->Destroy ();
				}
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
	return 0;
}

// PowerCrystal -------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_ExtraLightOff)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target != NULL && self->target->player != NULL)
	{
		self->target->player->extralight = 0;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_Explode512)
{
	PARAM_ACTION_PROLOGUE;

	P_RadiusAttack (self, self->target, 512, 512, NAME_None, RADF_HURTSOURCE);
	if (self->target != NULL && self->target->player != NULL)
	{
		self->target->player->extralight = 5;
	}
	P_CheckSplash(self, 512);

	// Strife didn't do this next part, but it looks good
	self->RenderStyle = STYLE_Add;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_LightGoesOut)
{
	PARAM_ACTION_PROLOGUE;

	AActor *foo;
	sector_t *sec = self->Sector;
	vertex_t *spot;
	double newheight;

	sec->SetLightLevel(0);

	double oldtheight = sec->floorplane.fD();
	newheight = sec->FindLowestFloorSurrounding(&spot);
	sec->floorplane.setD(sec->floorplane.PointToDist (spot, newheight));
	double newtheight = sec->floorplane.fD();
	sec->ChangePlaneTexZ(sector_t::floor, newtheight - oldtheight);
	sec->CheckPortalPlane(sector_t::floor);

	for (int i = 0; i < 8; ++i)
	{
		foo = Spawn("Rubble1", self->Pos(), ALLOW_REPLACE);
		if (foo != NULL)
		{
			int t = pr_lightout() & 15;
			foo->Vel.X = t - (pr_lightout() & 7);
			foo->Vel.Y = pr_lightout.Random2() & 7;
			foo->Vel.Z = 7 + (pr_lightout() & 3);
		}
	}
	return 0;
}
