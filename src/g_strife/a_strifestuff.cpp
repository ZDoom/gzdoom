#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "p_enemy.h"

static FRandom pr_gibtosser ("GibTosser");

void A_LoopActiveSound (AActor *);
void A_FLoopActiveSound (AActor *);

// Gibs for things that bleed -----------------------------------------------

class AMeat : public AActor
{
	DECLARE_ACTOR (AMeat, AActor)
public:
	void BeginPlay ()
	{
		// Strife used mod 19, but there are 20 states. Hmm.
		SetState (SpawnState + pr_gibtosser() % 20);
	}
};

FState AMeat::States[] =
{
	S_NORMAL (MEAT, 'A', 700, NULL, NULL),
	S_NORMAL (MEAT, 'B', 700, NULL, NULL),
	S_NORMAL (MEAT, 'C', 700, NULL, NULL),
	S_NORMAL (MEAT, 'D', 700, NULL, NULL),
	S_NORMAL (MEAT, 'E', 700, NULL, NULL),
	S_NORMAL (MEAT, 'F', 700, NULL, NULL),
	S_NORMAL (MEAT, 'G', 700, NULL, NULL),
	S_NORMAL (MEAT, 'H', 700, NULL, NULL),
	S_NORMAL (MEAT, 'I', 700, NULL, NULL),
	S_NORMAL (MEAT, 'J', 700, NULL, NULL),
	S_NORMAL (MEAT, 'K', 700, NULL, NULL),
	S_NORMAL (MEAT, 'L', 700, NULL, NULL),
	S_NORMAL (MEAT, 'M', 700, NULL, NULL),
	S_NORMAL (MEAT, 'N', 700, NULL, NULL),
	S_NORMAL (MEAT, 'O', 700, NULL, NULL),
	S_NORMAL (MEAT, 'P', 700, NULL, NULL),
	S_NORMAL (MEAT, 'Q', 700, NULL, NULL),
	S_NORMAL (MEAT, 'R', 700, NULL, NULL),
	S_NORMAL (MEAT, 'S', 700, NULL, NULL),
	S_NORMAL (MEAT, 'T', 700, NULL, NULL)
};

IMPLEMENT_ACTOR (AMeat, Any, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOCLIP|MF_NOBLOCKMAP)
END_DEFAULTS

// Gibs for things that don't bleed -----------------------------------------

class AJunk : public AMeat
{
	DECLARE_ACTOR (AJunk, AMeat)
};

FState AJunk::States[] =
{
	S_NORMAL (JUNK, 'A', 700, NULL, NULL),
	S_NORMAL (JUNK, 'B', 700, NULL, NULL),
	S_NORMAL (JUNK, 'C', 700, NULL, NULL),
	S_NORMAL (JUNK, 'D', 700, NULL, NULL),
	S_NORMAL (JUNK, 'E', 700, NULL, NULL),
	S_NORMAL (JUNK, 'F', 700, NULL, NULL),
	S_NORMAL (JUNK, 'G', 700, NULL, NULL),
	S_NORMAL (JUNK, 'H', 700, NULL, NULL),
	S_NORMAL (JUNK, 'I', 700, NULL, NULL),
	S_NORMAL (JUNK, 'J', 700, NULL, NULL),
	S_NORMAL (JUNK, 'K', 700, NULL, NULL),
	S_NORMAL (JUNK, 'L', 700, NULL, NULL),
	S_NORMAL (JUNK, 'M', 700, NULL, NULL),
	S_NORMAL (JUNK, 'N', 700, NULL, NULL),
	S_NORMAL (JUNK, 'O', 700, NULL, NULL),
	S_NORMAL (JUNK, 'P', 700, NULL, NULL),
	S_NORMAL (JUNK, 'Q', 700, NULL, NULL),
	S_NORMAL (JUNK, 'R', 700, NULL, NULL),
	S_NORMAL (JUNK, 'S', 700, NULL, NULL),
	S_NORMAL (JUNK, 'T', 700, NULL, NULL)
};

IMPLEMENT_ACTOR (AJunk, Any, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOCLIP|MF_NOBLOCKMAP)
END_DEFAULTS

//==========================================================================
//
// A_TossGib
//
//==========================================================================

void A_TossGib (AActor *self)
{
	const TypeInfo *gibtype = (self->flags & MF_NOBLOOD) ? RUNTIME_CLASS(AJunk) : RUNTIME_CLASS(AMeat);
	AActor *gib = Spawn (gibtype, self->x, self->y, self->z + 24*FRACUNIT);
	angle_t an;

	if (gib == NULL)
	{
		return;
	}

	an = pr_gibtosser() << 24;
	gib->angle = an;
	gib->momx = (pr_gibtosser() & 15) * finecosine[an >> ANGLETOFINESHIFT];
	gib->momy = (pr_gibtosser() & 15) * finesine[an >> ANGLETOFINESHIFT];
	gib->momz = (pr_gibtosser() & 15) << FRACBITS;
}

// Strife's explosive barrel ------------------------------------------------

class AExplosiveBarrel2 : public AExplosiveBarrel
{
	DECLARE_ACTOR (AExplosiveBarrel2, AExplosiveBarrel)
};

FState AExplosiveBarrel2::States[] =
{
	S_NORMAL (BART, 'A', -1, NULL, NULL),

	S_BRIGHT (BART, 'B',  2, A_Scream,		&States[2]),
	S_BRIGHT (BART, 'C',  2, NULL,			&States[3]),
	S_BRIGHT (BART, 'D',  2, NULL,			&States[4]),
	S_BRIGHT (BART, 'E',  2, A_NoBlocking,	&States[5]),
	S_BRIGHT (BART, 'F',  2, A_Explode,		&States[6]),
	S_BRIGHT (BART, 'G',  2, NULL,			&States[7]),
	S_BRIGHT (BART, 'H',  2, NULL,			&States[8]),
	S_BRIGHT (BART, 'I',  2, NULL,			&States[9]),
	S_BRIGHT (BART, 'J',  2, NULL,			&States[10]),
	S_BRIGHT (BART, 'K',  3, NULL,			&States[11]),
	S_BRIGHT (BART, 'L', -1, NULL,			NULL),
	
};

IMPLEMENT_ACTOR (AExplosiveBarrel2, Strife, 94, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (30)
	PROP_DeathState (1)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (32)
END_DEFAULTS

// Short Bush ---------------------------------------------------------------

class AShortBush : public AActor
{
	DECLARE_ACTOR (AShortBush, AActor)
};

FState AShortBush::States[] =
{
	S_NORMAL (BUSH, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AShortBush, Strife, 60, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (40)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Tall Bush ---------------------------------------------------------------

class ATallBush : public AActor
{
	DECLARE_ACTOR (ATallBush, AActor)
};

FState ATallBush::States[] =
{
	S_NORMAL (SHRB, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATallBush, Strife, 62, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Medium Torch -------------------------------------------------------------

class AMediumTorch : public AActor
{
	DECLARE_ACTOR (AMediumTorch, AActor)
};

FState AMediumTorch::States[] =
{
	S_BRIGHT (LTRH, 'A', 4, NULL, &States[1]),
	S_BRIGHT (LTRH, 'B', 4, NULL, &States[2]),
	S_BRIGHT (LTRH, 'C', 4, NULL, &States[3]),
	S_BRIGHT (LTRH, 'D', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (AMediumTorch, Strife, 111, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (72)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Pole Lantern -------------------------------------------------------------

class APoleLantern : public AActor
{
	DECLARE_ACTOR (APoleLantern, AActor)
};

FState APoleLantern::States[] =
{
	S_NORMAL (LANT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APoleLantern, Strife, 46, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (80)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Silver Flourescent ---------------------------------------------------------

class ASilverFlourescent : public AActor
{
	DECLARE_ACTOR (ASilverFlourescent, AActor)
};

FState ASilverFlourescent::States[] =
{
	S_BRIGHT (LITS, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASilverFlourescent, Strife, 95, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Brown Flourescent ---------------------------------------------------------

class ABrownFlourescent : public AActor
{
	DECLARE_ACTOR (ABrownFlourescent, AActor)
};

FState ABrownFlourescent::States[] =
{
	S_BRIGHT (LITB, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABrownFlourescent, Strife, 96, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Gold Flourescent ---------------------------------------------------------

class AGoldFlourescent : public AActor
{
	DECLARE_ACTOR (AGoldFlourescent, AActor)
};

FState AGoldFlourescent::States[] =
{
	S_BRIGHT (LITG, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AGoldFlourescent, Strife, 97, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Claxon Light -------------------------------------------------------------

class AClaxonWarningLight : public AActor
{
	DECLARE_ACTOR (AClaxonWarningLight, AActor)
};

FState AClaxonWarningLight::States[] =
{
	S_NORMAL (KLAX, 'A',  5, NULL/*f608*/,	&States[0]),

	S_NORMAL (KLAX, 'B',  6, NULL/*11c0c*/, &States[2]),
	S_NORMAL (KLAX, 'C', 60, NULL,			&States[1])
};

IMPLEMENT_ACTOR (AClaxonWarningLight, Strife, 24, 0)
	PROP_SpawnState (0)
	PROP_SeeState (1)
	PROP_ReactionTime (60)
	PROP_Flags (MF_NOBLOCKMAP|MF_AMBUSH|MF_SPAWNCEILING|MF_NOGRAVITY)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS)
END_DEFAULTS

// Small Torch (Lit) --------------------------------------------------------

class ASmallTorchLit : public AActor
{
	DECLARE_ACTOR (ASmallTorchLit, AActor)
};

FState ASmallTorchLit::States[] =
{
	S_BRIGHT (TRCH, 'A', 4, NULL, &States[1]),
	S_BRIGHT (TRCH, 'B', 4, NULL, &States[2]),
	S_BRIGHT (TRCH, 'C', 4, NULL, &States[3]),
	S_BRIGHT (TRCH, 'D', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (ASmallTorchLit, Strife, 107, 0)
	PROP_SpawnState (0)
	PROP_Radius (4)			// This is intentionally not PROP_RadiusFixed
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)

	// It doesn't have any action functions, so how does it use this sound?
	PROP_ActiveSound ("world/smallfire")
END_DEFAULTS

// Candle -------------------------------------------------------------------

class ACandle : public AActor
{
	DECLARE_ACTOR (ACandle, AActor)
};

FState ACandle::States[] =
{
	S_BRIGHT (CNDL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACandle, Strife, 34, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Mug ----------------------------------------------------------------------

class AMug : public AActor
{
	DECLARE_ACTOR (AMug, AActor)
};

FState AMug::States[] =
{
	S_NORMAL (MUGG, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMug, Strife, 164, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Pitcher ------------------------------------------------------------------

class APitcher : public AActor
{
	DECLARE_ACTOR (APitcher, AActor)
};

FState APitcher::States[] =
{
	S_NORMAL (VASE, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APitcher, Strife, 188, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (32)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Stick in Water -----------------------------------------------------------

class AStickInWater : public AActor
{
	DECLARE_ACTOR (AStickInWater, AActor)
};

FState AStickInWater::States[] =
{
	S_NORMAL (LOGG, 'A', 5, A_LoopActiveSound, &States[1]),
	S_NORMAL (LOGG, 'B', 5, A_LoopActiveSound, &States[2]),
	S_NORMAL (LOGG, 'C', 5, A_LoopActiveSound, &States[3]),
	S_NORMAL (LOGG, 'D', 5, A_LoopActiveSound, &States[0])
};

IMPLEMENT_ACTOR (AStickInWater, Strife, 215, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_ActiveSound ("world/river")
END_DEFAULTS

void A_LoopActiveSound (AActor *self)
{
	if (self->ActiveSound != 0 && !S_IsActorPlayingSomething (self, 6))
	{
		S_LoopedSoundID (self, 6, self->ActiveSound, 1, ATTN_NORM);
	}
}

// Waterfall Splash ---------------------------------------------------------
// Why is this not visible in Strife?

class AWaterfallSplash : public AActor
{
	DECLARE_ACTOR (AWaterfallSplash, AActor)
};

FState AWaterfallSplash::States[] =
{
	S_NORMAL (SPLH, 'A', 4, NULL,				&States[1]),
	S_NORMAL (SPLH, 'B', 4, NULL,				&States[2]),
	S_NORMAL (SPLH, 'C', 4, NULL,				&States[3]),
	S_NORMAL (SPLH, 'D', 8, NULL,				&States[4]),
	S_NORMAL (SPLH, 'E', 4, NULL,				&States[5]),
	S_NORMAL (SPLH, 'F', 4, NULL,				&States[6]),
	S_NORMAL (SPLH, 'G', 4, NULL,				&States[7]),
	S_NORMAL (SPLH, 'H', 4, A_LoopActiveSound,	&States[0]),
};

IMPLEMENT_ACTOR (AWaterfallSplash, Strife, 104, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_ActiveSound ("world/waterfall")
END_DEFAULTS

// Brass Tech Lamp ----------------------------------------------------------

class ABrassTechLamp : public AActor
{
	DECLARE_ACTOR (ABrassTechLamp, AActor)
};

FState ABrassTechLamp::States[] =
{
	S_NORMAL (TLMP, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABrassTechLamp, Strife, 197, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Water Bottle -------------------------------------------------------------

class AWaterBottle : public AActor
{
	DECLARE_ACTOR (AWaterBottle, AActor)
};

FState AWaterBottle::States[] =
{
	S_NORMAL (WATR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AWaterBottle, Strife, 2014, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Barricade Column ---------------------------------------------------------

class ABarricadeColumn : public AActor
{
	DECLARE_ACTOR (ABarricadeColumn, AActor)
};

FState ABarricadeColumn::States[] =
{
	S_NORMAL (BAR1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABarricadeColumn, Strife, 69, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (128)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Stool --------------------------------------------------------------------

class AStool : public AActor
{
	DECLARE_ACTOR (AStool, AActor)
};

FState AStool::States[] =
{
	S_NORMAL (STOL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AStool, Strife, 189, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (24)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Burning Bowl -------------------------------------------------------------

class ABurningBowl : public AActor
{
	DECLARE_ACTOR (ABurningBowl, AActor)
};

FState ABurningBowl::States[] =
{
	S_BRIGHT (BOWL, 'A', 4, NULL, &States[1]),
	S_BRIGHT (BOWL, 'B', 4, NULL, &States[2]),
	S_BRIGHT (BOWL, 'C', 4, NULL, &States[3]),
	S_BRIGHT (BOWL, 'D', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (ABurningBowl, Strife, 105, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("world/smallfire")
END_DEFAULTS

// Hearts in Tank -----------------------------------------------------------

class AHeartsInTank : public AActor
{
	DECLARE_ACTOR (AHeartsInTank, AActor)
};

FState AHeartsInTank::States[] =
{
	S_BRIGHT (HERT, 'A', 4, NULL, &States[1]),
	S_BRIGHT (HERT, 'B', 4, NULL, &States[2]),
	S_BRIGHT (HERT, 'C', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (AHeartsInTank, Strife, 113, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Teleport Swirl -----------------------------------------------------------

class ATeleportSwirl : public AActor
{
	DECLARE_ACTOR (ATeleportSwirl, AActor)
};

FState ATeleportSwirl::States[] =
{
	S_BRIGHT (TELP, 'A', 3, NULL, &States[1]),
	S_BRIGHT (TELP, 'B', 3, NULL, &States[2]),
	S_BRIGHT (TELP, 'C', 3, NULL, &States[3]),
	S_BRIGHT (TELP, 'D', 3, NULL, &States[0])
};

IMPLEMENT_ACTOR (ATeleportSwirl, Strife, 23, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (HX_SHADOW)
END_DEFAULTS

// Surgery Crab -------------------------------------------------------------

class ASurgeryCrab : public AActor
{
	DECLARE_ACTOR (ASurgeryCrab, AActor)
};

FState ASurgeryCrab::States[] =
{
	S_NORMAL (CRAB, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASurgeryCrab, Strife, 117, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
END_DEFAULTS

// Archery Target -----------------------------------------------------------

class AArcheryTarget : public AActor
{
	DECLARE_ACTOR (AArcheryTarget, AActor)
};

void A_20e10 (AActor *) {}

FState AArcheryTarget::States[] =
{
	S_NORMAL (HOGN, 'A', 2, A_20e10, &States[0]),

	S_NORMAL (HOGN, 'B', 1, A_20e10, &States[2]),
	S_NORMAL (HOGN, 'C', 1, A_Pain, &States[0])
};

IMPLEMENT_ACTOR (AArcheryTarget, Strife, 208, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealthLong (99999999)
	PROP_PainState (1)
	PROP_PainChance (255)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (72)
	PROP_MassLong (9999999)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)	// 0x88006
	PROP_PainSound ("misc/metalhit")
END_DEFAULTS

// RebelBoots ---------------------------------------------------------------

class ARebelBoots : public AActor
{
	DECLARE_ACTOR (ARebelBoots, AActor)
};

FState ARebelBoots::States[] =
{
	S_NORMAL (BOTR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARebelBoots, Strife, 217, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// RebelHelmet --------------------------------------------------------------

class ARebelHelmet : public AActor
{
	DECLARE_ACTOR (ARebelHelmet, AActor)
};

FState ARebelHelmet::States[] =
{
	S_NORMAL (BOTR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARebelHelmet, Strife, 218, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// RebelShirt ---------------------------------------------------------------

class ARebelShirt : public AActor
{
	DECLARE_ACTOR (ARebelShirt, AActor)
};

FState ARebelShirt::States[] =
{
	S_NORMAL (TOPR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARebelShirt, Strife, 219, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Wooden Barrel ------------------------------------------------------------

class AWoodenBarrel : public AActor
{
	DECLARE_ACTOR (AWoodenBarrel, AActor)
};

FState AWoodenBarrel::States[] =
{
	S_NORMAL (BARW, 'A', -1, NULL, NULL),

	S_NORMAL (BARW, 'B', 2, A_Scream,			&States[2]),
	S_NORMAL (BARW, 'C', 2, NULL,				&States[3]),
	S_NORMAL (BARW, 'D', 2, A_NoBlocking,		&States[4]),
	S_NORMAL (BARW, 'E', 2, NULL,				&States[5]),
	S_NORMAL (BARW, 'F', 2, NULL,				&States[6]),
	S_NORMAL (BARW, 'G', 2, NULL,				&States[7]),
	S_NORMAL (BARW, 'H', -1, NULL,				NULL)
};

IMPLEMENT_ACTOR (AWoodenBarrel, Strife, 82, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (10)
	PROP_DeathState (1)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (32)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)	// 0x88006
	PROP_DeathSound ("woodenbarrel/death")
END_DEFAULTS

// Ceiling Water Drip -------------------------------------------------------

class AWaterDrip : public AActor
{
	DECLARE_ACTOR (AWaterDrip, AActor)
};

FState AWaterDrip::States[] =
{
	S_NORMAL (CDRP, 'A', 10, NULL, &States[1]),
	S_NORMAL (CDRP, 'B',  8, NULL, &States[2]),
	S_NORMAL (CDRP, 'C',  8, NULL, &States[3]),
	S_NORMAL (CDRP, 'D',  8, NULL, &States[0])
};

IMPLEMENT_ACTOR (AWaterDrip, Strife, 53, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_SPAWNCEILING|MF_NOGRAVITY)
END_DEFAULTS

// Floor Water Drop ---------------------------------------------------------

class AWaterDropOnFloor : public AActor
{
	DECLARE_ACTOR (AWaterDropOnFloor, AActor)
};

FState AWaterDropOnFloor::States[] =
{
	S_NORMAL (DRIP, 'A', 6, A_FLoopActiveSound,	&States[1]),
	S_NORMAL (DRIP, 'B', 4, NULL,				&States[2]),
	S_NORMAL (DRIP, 'C', 4, NULL,				&States[3]),
	S_NORMAL (DRIP, 'D', 4, A_FLoopActiveSound,	&States[4]),
	S_NORMAL (DRIP, 'E', 4, NULL,				&States[5]),
	S_NORMAL (DRIP, 'F', 4, NULL,				&States[6]),
	S_NORMAL (DRIP, 'G', 4, A_FLoopActiveSound,	&States[7]),
	S_NORMAL (DRIP, 'H', 4, NULL,				&States[0])
};

IMPLEMENT_ACTOR (AWaterDropOnFloor, Strife, 103, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_ActiveSound ("world/waterdrip")
END_DEFAULTS

void A_FLoopActiveSound (AActor *self)
{
	if (self->ActiveSound != 0 && !(level.time & 7))
	{
		S_LoopedSoundID (self, 6, self->ActiveSound, 1, ATTN_NORM);
	}
}
