#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_action.h"
#include "a_hexenglobal.h"

static FRandom pr_poisonbag ("PoisonBag");
static FRandom pr_poisoncloud ("PoisonCloud");
static FRandom pr_poisoncloudd ("PoisonCloudDamage");

void A_PoisonBagInit (AActor *);
void A_PoisonBagDamage (AActor *);
void A_PoisonBagCheck (AActor *);
void A_CheckThrowBomb (AActor *);
void A_CheckThrowBomb2 (AActor *);

// Poison Bag (Flechette used by Cleric) ------------------------------------

class APoisonBag : public AActor
{
	DECLARE_ACTOR (APoisonBag, AActor)
};

FState APoisonBag::States[] =
{
#define S_POISONBAG1 0
	S_BRIGHT (PSBG, 'A',   18, NULL					    , &States[S_POISONBAG1+1]),
	S_BRIGHT (PSBG, 'B',	4, NULL					    , &States[S_POISONBAG1+2]),
	S_NORMAL (PSBG, 'C',	3, NULL					    , &States[S_POISONBAG1+3]),
	S_NORMAL (PSBG, 'C',	1, A_PoisonBagInit		    , NULL),
};

IMPLEMENT_ACTOR (APoisonBag, Hexen, -1, 0)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)

	PROP_SpawnState (S_POISONBAG1)
END_DEFAULTS

// Fire Bomb (Flechette used by Mage) ---------------------------------------

class AFireBomb : public AActor
{
	DECLARE_ACTOR (AFireBomb, AActor)
public:
	void PreExplode ()
	{
		z += 32*FRACUNIT;
		RenderStyle = STYLE_Normal;
	}
};

FState AFireBomb::States[] =
{
#define S_FIREBOMB1 0
	S_NORMAL (PSBG, 'A',   20, NULL					    , &States[S_FIREBOMB1+1]),
	S_NORMAL (PSBG, 'A',   10, NULL					    , &States[S_FIREBOMB1+2]),
	S_NORMAL (PSBG, 'A',   10, NULL					    , &States[S_FIREBOMB1+3]),
	S_NORMAL (PSBG, 'B',	4, NULL					    , &States[S_FIREBOMB1+4]),
	S_NORMAL (PSBG, 'C',	4, A_Scream				    , &States[S_FIREBOMB1+5]),
	S_BRIGHT (XPL1, 'A',	4, A_Explode			    , &States[S_FIREBOMB1+6]),
	S_BRIGHT (XPL1, 'B',	4, NULL					    , &States[S_FIREBOMB1+7]),
	S_BRIGHT (XPL1, 'C',	4, NULL					    , &States[S_FIREBOMB1+8]),
	S_BRIGHT (XPL1, 'D',	4, NULL					    , &States[S_FIREBOMB1+9]),
	S_BRIGHT (XPL1, 'E',	4, NULL					    , &States[S_FIREBOMB1+10]),
	S_BRIGHT (XPL1, 'F',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AFireBomb, Hexen, -1, 0)
	PROP_Flags (MF_NOGRAVITY)
	PROP_Flags2 (MF2_FIREDAMAGE)
	PROP_Flags3 (MF3_FOILINVUL)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_SpawnState (S_FIREBOMB1)

	PROP_DeathSound ("FlechetteExplode")
END_DEFAULTS

// Throwing Bomb (Flechette used by Fighter) --------------------------------

class AThrowingBomb : public AActor
{
	DECLARE_ACTOR (AThrowingBomb, AActor)
};

FState AThrowingBomb::States[] =
{
#define S_THROWINGBOMB1 0
	S_NORMAL (THRW, 'A',	4, A_CheckThrowBomb		    , &States[S_THROWINGBOMB1+1]),
	S_NORMAL (THRW, 'B',	3, A_CheckThrowBomb		    , &States[S_THROWINGBOMB1+2]),
	S_NORMAL (THRW, 'C',	3, A_CheckThrowBomb		    , &States[S_THROWINGBOMB1+3]),
	S_NORMAL (THRW, 'D',	3, A_CheckThrowBomb		    , &States[S_THROWINGBOMB1+4]),
	S_NORMAL (THRW, 'E',	3, A_CheckThrowBomb		    , &States[S_THROWINGBOMB1+5]),
	S_NORMAL (THRW, 'F',	3, A_CheckThrowBomb2	    , &States[S_THROWINGBOMB1]),

#define S_THROWINGBOMB7 (S_THROWINGBOMB1+6)
	S_NORMAL (THRW, 'G',	6, A_CheckThrowBomb		    , &States[S_THROWINGBOMB7+1]),
	S_NORMAL (THRW, 'F',	4, A_CheckThrowBomb		    , &States[S_THROWINGBOMB7+2]),
	S_NORMAL (THRW, 'H',	6, A_CheckThrowBomb		    , &States[S_THROWINGBOMB7+3]),
	S_NORMAL (THRW, 'F',	4, A_CheckThrowBomb		    , &States[S_THROWINGBOMB7+4]),
	S_NORMAL (THRW, 'G',	6, A_CheckThrowBomb		    , &States[S_THROWINGBOMB7+5]),
	S_NORMAL (THRW, 'F',	3, A_CheckThrowBomb		    , &States[S_THROWINGBOMB7+5]),

#define S_THROWINGBOMB_X1 (S_THROWINGBOMB7+6)
	S_BRIGHT (CFCF, 'Q',	4, A_NoGravity			    , &States[S_THROWINGBOMB_X1+1]),
	S_BRIGHT (CFCF, 'R',	3, A_Scream				    , &States[S_THROWINGBOMB_X1+2]),
	S_BRIGHT (CFCF, 'S',	4, A_Explode			    , &States[S_THROWINGBOMB_X1+3]),
	S_BRIGHT (CFCF, 'T',	3, NULL					    , &States[S_THROWINGBOMB_X1+4]),
	S_BRIGHT (CFCF, 'U',	4, NULL					    , &States[S_THROWINGBOMB_X1+5]),
	S_BRIGHT (CFCF, 'W',	3, NULL					    , &States[S_THROWINGBOMB_X1+6]),
	S_BRIGHT (CFCF, 'X',	4, NULL					    , &States[S_THROWINGBOMB_X1+7]),
	S_BRIGHT (CFCF, 'Z',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AThrowingBomb, Hexen, -1, 0)
	PROP_SpawnHealth (48)
	PROP_SpeedFixed (12)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (10)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_HEXENBOUNCE|MF2_FIREDAMAGE)

	PROP_SpawnState (S_THROWINGBOMB1)
	PROP_DeathState (S_THROWINGBOMB_X1)

	PROP_SeeSound ("FlechetteBounce")
	PROP_DeathSound ("FlechetteExplode")
END_DEFAULTS

// Poison Bag Artifact (Flechette) ------------------------------------------

class AArtiPoisonBag : public AArtifact
{
	DECLARE_ACTOR (AArtiPoisonBag, AArtifact)
	AT_GAME_SET_FRIEND (PoisonBag)
public:
	bool TryPickup (AActor *toucher)
	{/*
		if (toucher->IsKindOf (RUNTIME_CLASS(AClericPlayer))
		{
			return P_GiveArtifact (toucher->player, arti_poisonbag1);
		}
		else if (toucher->IsKindOf (RUNTIME_CLASS(AMagePlayer))
		{
			return P_GiveArtifact (toucher->player, arti_poisonbag2);
		}
		else
	 */
		{
			return P_GiveArtifact (toucher->player, arti_poisonbag3);
		}
	}
protected:
	const char *PickupMessage ()
	{
		return GStrings(TXT_ARTIPOISONBAG);
	}
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		angle_t angle = player->mo->angle >> ANGLETOFINESHIFT;
		AActor *mo;

		if (arti == arti_poisonbag1 || arti == arti_poisonbag2)	// cleric (1) or mage (2)
		{
			mo = Spawn (
				arti == arti_poisonbag1 ? RUNTIME_CLASS(APoisonBag) : RUNTIME_CLASS (AFireBomb),
				player->mo->x+16*finecosine[angle],
				player->mo->y+24*finesine[angle], player->mo->z-
				player->mo->floorclip+8*FRACUNIT);
			if (mo)
			{
				mo->target = player->mo;
			}
		}
		else // fighter
		{
			mo = Spawn<AThrowingBomb> (player->mo->x, player->mo->y, 
				player->mo->z-player->mo->floorclip+35*FRACUNIT);
			if (mo)
			{
				angle_t pitch = (angle_t)player->mo->pitch >> ANGLETOFINESHIFT;

				mo->angle = player->mo->angle+(((pr_poisonbag()&7)-4)<<24);
				mo->momz = 4*FRACUNIT + 2*finesine[pitch];
				mo->z += 2*finesine[pitch];
				P_ThrustMobj (mo, mo->angle, mo->Speed);
				mo->momx += player->mo->momx>>1;
				mo->momy += player->mo->momy>>1;
				mo->target = player->mo;
				mo->tics -= pr_poisonbag()&3;
				P_CheckMissileSpawn (mo);											
			} 
		}
		return mo != NULL;
	}
};

FState AArtiPoisonBag::States[] =
{
	S_NORMAL (PSBG, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AArtiPoisonBag, Hexen, 8000, 72)
	PROP_Flags (MF_SPECIAL)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (PoisonBag)
{
	ArtiDispatch[arti_poisonbag1] = &AArtiPoisonBag::ActivateArti;
	ArtiDispatch[arti_poisonbag2] = &AArtiPoisonBag::ActivateArti;
	ArtiDispatch[arti_poisonbag3] = &AArtiPoisonBag::ActivateArti;
	ArtiPics[arti_poisonbag1] = "ARTIPSBG";
	ArtiPics[arti_poisonbag2] = "ARTIPSBG";
	ArtiPics[arti_poisonbag3] = "ARTIPSBG";
}

// Poison Cloud -------------------------------------------------------------

FState APoisonCloud::States[] =
{
#define S_POISONCLOUD1 0
	S_NORMAL (PSBG, 'D',	1, NULL					    , &States[S_POISONCLOUD1+1]),
	S_NORMAL (PSBG, 'D',	1, A_Scream				    , &States[S_POISONCLOUD1+2]),
	S_NORMAL (PSBG, 'D',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+3]),
	S_NORMAL (PSBG, 'E',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+4]),
	S_NORMAL (PSBG, 'E',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+5]),
	S_NORMAL (PSBG, 'E',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+6]),
	S_NORMAL (PSBG, 'F',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+7]),
	S_NORMAL (PSBG, 'F',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+8]),
	S_NORMAL (PSBG, 'F',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+9]),
	S_NORMAL (PSBG, 'G',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+10]),
	S_NORMAL (PSBG, 'G',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+11]),
	S_NORMAL (PSBG, 'G',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+12]),
	S_NORMAL (PSBG, 'H',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+13]),
	S_NORMAL (PSBG, 'H',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+14]),
	S_NORMAL (PSBG, 'H',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+15]),
	S_NORMAL (PSBG, 'I',	2, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+16]),
	S_NORMAL (PSBG, 'I',	1, A_PoisonBagDamage	    , &States[S_POISONCLOUD1+17]),
	S_NORMAL (PSBG, 'I',	1, A_PoisonBagCheck		    , &States[S_POISONCLOUD1+3]),

#define S_POISONCLOUD_X1 (S_POISONCLOUD1+18)
	S_NORMAL (PSBG, 'H',	7, NULL					    , &States[S_POISONCLOUD_X1+1]),
	S_NORMAL (PSBG, 'G',	7, NULL					    , &States[S_POISONCLOUD_X1+2]),
	S_NORMAL (PSBG, 'F',	6, NULL					    , &States[S_POISONCLOUD_X1+3]),
	S_NORMAL (PSBG, 'D',	6, NULL					    , NULL)
};

IMPLEMENT_ACTOR (APoisonCloud, Hexen, -1, 0)
	PROP_Radius (20)
	PROP_Height (30)
	PROP_MassLong (0x7fffffff)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF)
	PROP_Flags2 (MF2_NODMGTHRUST)
	PROP_Flags3 (MF3_DONTSPLASH|MF3_FOILINVUL|MF3_CANBLAST)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

	PROP_SpawnState (S_POISONCLOUD1)

	PROP_DeathSound ("PoisonShroomDeath")
END_DEFAULTS

void APoisonCloud::BeginPlay ()
{
	momx = 1; // missile objects must move to impact other objects
	special1 = 24+(pr_poisoncloud()&7);
	special2 = 0;
}

void APoisonCloud::GetExplodeParms (int &damage, int &distance, bool &hurtSource)
{
	damage = 4;
	distance = 40;
}

int APoisonCloud::DoSpecialDamage (AActor *target, int damage)
{
	if (target->player)
	{
		if (target->player->poisoncount < 4)
		{
			P_PoisonDamage (target->player, this,
				15+(pr_poisoncloudd()&15), false); // Don't play painsound
			P_PoisonPlayer (target->player, this, 50);

			// [RH] Don't cough if already making a mouth noise
			if (!S_IsActorPlayingSomething (target, CHAN_VOICE))
			{
				S_Sound (target, CHAN_VOICE, "*poison", 1, ATTN_NORM);
			}
		}	
		return -1;
	}
	else if (!(target->flags & MF_COUNTKILL))
	{ // only damage monsters/players with the poison cloud
		return -1;
	}
	return damage;
}

//===========================================================================
//
// A_PoisonBagInit
//
//===========================================================================

void A_PoisonBagInit (AActor *actor)
{
	AActor *mo;
	
	mo = Spawn<APoisonCloud> (actor->x, actor->y, actor->z+28*FRACUNIT);
	if (mo)
	{
		mo->target = actor->target;
	}
}

//===========================================================================
//
// A_PoisonBagCheck
//
//===========================================================================

void A_PoisonBagCheck (AActor *actor)
{
	if (--actor->special1 <= 0)
	{
		actor->SetState (&APoisonCloud::States[S_POISONCLOUD_X1]);
	}
	else
	{
		return;
	}
}

//===========================================================================
//
// A_PoisonBagDamage
//
//===========================================================================

void A_PoisonBagDamage (AActor *actor)
{
	int bobIndex;
	
	A_Explode (actor);	
	bobIndex = actor->special2;
	actor->z += FloatBobOffsets[bobIndex]>>4;
	actor->special2 = (bobIndex+1)&63;
}

//===========================================================================
//
// A_CheckThrowBomb
//
//===========================================================================

void A_CheckThrowBomb (AActor *actor)
{
	if (--actor->health <= 0)
	{
		actor->SetState (actor->DeathState);
	}
}

//===========================================================================
//
// A_CheckThrowBomb2
//
//===========================================================================

void A_CheckThrowBomb2 (AActor *actor)
{
	// [RH] Check using actual velocity, although the momz < 2 check still stands
	//if (abs(actor->momx) < FRACUNIT*3/2 && abs(actor->momy) < FRACUNIT*3/2
	//	&& actor->momz < 2*FRACUNIT)
	if (actor->momz < 2*FRACUNIT &&
		TMulScale32 (actor->momx, actor->momx, actor->momy, actor->momy, actor->momz, actor->momz)
		< (3*3)/(2*2))
	{
		actor->SetState (&AThrowingBomb::States[S_THROWINGBOMB7]);
		actor->z = actor->floorz;
		actor->momz = 0;
		actor->flags2 &= ~MF2_BOUNCETYPE;
		actor->flags &= ~MF_MISSILE;
	}
	A_CheckThrowBomb (actor);
}
