#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"

static FRandom pr_atk ("DemonAttack1");
static FRandom pr_demonchunks ("DemonChunks");

//============================================================================
// Demon AI
//============================================================================

void A_DemonAttack1 (AActor *);
void A_DemonAttack2_1 (AActor *);
void A_DemonAttack2_2 (AActor *);
void A_DemonDeath (AActor *);
void A_Demon2Death (AActor *);

static void TossChunks (AActor *, const TypeInfo *const chunks[]);

// Demon, type 1 (green, like D'Sparil's) -----------------------------------

class ADemon1 : public AActor
{
	DECLARE_ACTOR (ADemon1, AActor)
};

FState ADemon1::States[] =
{
#define S_DEMN_LOOK1 0
	S_NORMAL (DEMN, 'A',   10, A_Look				    , &States[S_DEMN_LOOK1+1]),
	S_NORMAL (DEMN, 'A',   10, A_Look				    , &States[S_DEMN_LOOK1]),

#define S_DEMN_CHASE1 (S_DEMN_LOOK1+2)
	S_NORMAL (DEMN, 'A',	4, A_Chase				    , &States[S_DEMN_CHASE1+1]),
	S_NORMAL (DEMN, 'B',	4, A_Chase				    , &States[S_DEMN_CHASE1+2]),
	S_NORMAL (DEMN, 'C',	4, A_Chase				    , &States[S_DEMN_CHASE1+3]),
	S_NORMAL (DEMN, 'D',	4, A_Chase				    , &States[S_DEMN_CHASE1]),

#define S_DEMN_PAIN1 (S_DEMN_CHASE1+4)
	S_NORMAL (DEMN, 'E',	4, NULL					    , &States[S_DEMN_PAIN1+1]),
	S_NORMAL (DEMN, 'E',	4, A_Pain				    , &States[S_DEMN_CHASE1]),

#define S_DEMN_ATK1_1 (S_DEMN_PAIN1+2)
	S_NORMAL (DEMN, 'E',	6, A_FaceTarget			    , &States[S_DEMN_ATK1_1+1]),
	S_NORMAL (DEMN, 'F',	8, A_FaceTarget			    , &States[S_DEMN_ATK1_1+2]),
	S_NORMAL (DEMN, 'G',	6, A_DemonAttack1		    , &States[S_DEMN_CHASE1]),

#define S_DEMN_ATK2_1 (S_DEMN_ATK1_1+3)
	S_NORMAL (DEMN, 'E',	5, A_FaceTarget			    , &States[S_DEMN_ATK2_1+1]),
	S_NORMAL (DEMN, 'F',	6, A_FaceTarget			    , &States[S_DEMN_ATK2_1+2]),
	S_NORMAL (DEMN, 'G',	5, A_DemonAttack2_1		    , &States[S_DEMN_CHASE1]),

#define S_DEMN_DEATH1 (S_DEMN_ATK2_1+3)
	S_NORMAL (DEMN, 'H',	6, NULL					    , &States[S_DEMN_DEATH1+1]),
	S_NORMAL (DEMN, 'I',	6, NULL					    , &States[S_DEMN_DEATH1+2]),
	S_NORMAL (DEMN, 'J',	6, A_Scream				    , &States[S_DEMN_DEATH1+3]),
	S_NORMAL (DEMN, 'K',	6, A_NoBlocking			    , &States[S_DEMN_DEATH1+4]),
	S_NORMAL (DEMN, 'L',	6, A_QueueCorpse		    , &States[S_DEMN_DEATH1+5]),
	S_NORMAL (DEMN, 'M',	6, NULL					    , &States[S_DEMN_DEATH1+6]),
	S_NORMAL (DEMN, 'N',	6, NULL					    , &States[S_DEMN_DEATH1+7]),
	S_NORMAL (DEMN, 'O',	6, NULL					    , &States[S_DEMN_DEATH1+8]),
	S_NORMAL (DEMN, 'P',   -1, NULL					    , NULL),

#define S_DEMN_XDEATH1 (S_DEMN_DEATH1+9)
	S_NORMAL (DEMN, 'H',	6, NULL					    , &States[S_DEMN_XDEATH1+1]),
	S_NORMAL (DEMN, 'I',	6, A_DemonDeath			    , &States[S_DEMN_XDEATH1+2]),
	S_NORMAL (DEMN, 'J',	6, A_Scream				    , &States[S_DEMN_XDEATH1+3]),
	S_NORMAL (DEMN, 'K',	6, A_NoBlocking			    , &States[S_DEMN_XDEATH1+4]),
	S_NORMAL (DEMN, 'L',	6, A_QueueCorpse		    , &States[S_DEMN_XDEATH1+5]),
	S_NORMAL (DEMN, 'M',	6, NULL					    , &States[S_DEMN_XDEATH1+6]),
	S_NORMAL (DEMN, 'N',	6, NULL					    , &States[S_DEMN_XDEATH1+7]),
	S_NORMAL (DEMN, 'O',	6, NULL					    , &States[S_DEMN_XDEATH1+8]),
	S_NORMAL (DEMN, 'P',   -1, NULL					    , NULL),

#define S_DEMON_ICE1 (S_DEMN_XDEATH1+9)
	S_NORMAL (DEMN, 'Q',	5, A_FreezeDeath		    , &States[S_DEMON_ICE1+1]),
	S_NORMAL (DEMN, 'Q',	1, A_FreezeDeathChunks	    , &States[S_DEMON_ICE1+1]),
};

IMPLEMENT_ACTOR (ADemon1, Hexen, 31, 3)
	PROP_SpawnHealth (250)
	PROP_PainChance (50)
	PROP_SpeedFixed (13)
	PROP_RadiusFixed (32)
	PROP_HeightFixed (64)
	PROP_Mass (220)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_MCROSS)

	PROP_SpawnState (S_DEMN_LOOK1)
	PROP_SeeState (S_DEMN_CHASE1)
	PROP_PainState (S_DEMN_PAIN1)
	PROP_MeleeState (S_DEMN_ATK1_1)
	PROP_MissileState (S_DEMN_ATK2_1)
	PROP_DeathState (S_DEMN_DEATH1)
	PROP_XDeathState (S_DEMN_XDEATH1)
	PROP_IDeathState (S_DEMON_ICE1)

	PROP_SeeSound ("DemonSight")
	PROP_AttackSound ("DemonAttack")
	PROP_PainSound ("DemonPain")
	PROP_DeathSound ("DemonDeath")
	PROP_ActiveSound ("DemonActive")
END_DEFAULTS

// Demon, type 1, mashed ----------------------------------------------------

class ADemon1Mash : public ADemon1
{
	DECLARE_STATELESS_ACTOR (ADemon1Mash, ADemon1)
};

IMPLEMENT_STATELESS_ACTOR (ADemon1Mash, Hexen, -1, 100)
	PROP_FlagsSet (MF_NOBLOOD)
	PROP_Flags2Set (MF2_BLASTED)
	PROP_Flags2Clear (MF2_TELESTOMP)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_DeathState (~0)
	PROP_XDeathState (~0)
	PROP_IDeathState (~0)
END_DEFAULTS

// Demon chunk, base class --------------------------------------------------

class ADemonChunk : public AActor
{
	DECLARE_STATELESS_ACTOR (ADemonChunk, AActor);
};

IMPLEMENT_STATELESS_ACTOR (ADemonChunk, Hexen, -1, 0)
	PROP_RadiusFixed (5)
	PROP_HeightFixed (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_DROPOFF|MF_MISSILE|MF_CORPSE)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_NOTELEPORT)
END_DEFAULTS

// Demon, type 1, chunk 1 ---------------------------------------------------

class ADemon1Chunk1 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon1Chunk1, ADemonChunk)
};

FState ADemon1Chunk1::States[] =
{
#define S_DEMONCHUNK1_1 0
	S_NORMAL (DEMA, 'A',	4, NULL					    , &States[S_DEMONCHUNK1_1+1]),
	S_NORMAL (DEMA, 'A',   10, A_QueueCorpse		    , &States[S_DEMONCHUNK1_1+2]),
	S_NORMAL (DEMA, 'A',   20, NULL					    , &States[S_DEMONCHUNK1_1+2]),

#define S_DEMONCHUNK1_4 (S_DEMONCHUNK1_1+3)
	S_NORMAL (DEMA, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon1Chunk1, Hexen, -1, 0)
	PROP_SpawnState (S_DEMONCHUNK1_1)
	PROP_DeathState (S_DEMONCHUNK1_4)
END_DEFAULTS

// Demon, type 1, chunk 2 ---------------------------------------------------

class ADemon1Chunk2 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon1Chunk2, ADemonChunk)
};

FState ADemon1Chunk2::States[] =
{
#define S_DEMONCHUNK2_1 0
	S_NORMAL (DEMB, 'A',	4, NULL					    , &States[S_DEMONCHUNK2_1+1]),
	S_NORMAL (DEMB, 'A',   10, A_QueueCorpse		    , &States[S_DEMONCHUNK2_1+2]),
	S_NORMAL (DEMB, 'A',   20, NULL					    , &States[S_DEMONCHUNK2_1+2]),

#define S_DEMONCHUNK2_4 (S_DEMONCHUNK2_1+3)
	S_NORMAL (DEMB, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon1Chunk2, Hexen, -1, 0)
	PROP_SpawnState (S_DEMONCHUNK2_1)
	PROP_DeathState (S_DEMONCHUNK2_4)
END_DEFAULTS

// Demon, type 1, chunk 3 ---------------------------------------------------

class ADemon1Chunk3 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon1Chunk3, ADemonChunk)
};

FState ADemon1Chunk3::States[] =
{
#define S_DEMONCHUNK3_1 0
	S_NORMAL (DEMC, 'A',	4, NULL					    , &States[S_DEMONCHUNK3_1+1]),
	S_NORMAL (DEMC, 'A',   10, A_QueueCorpse		    , &States[S_DEMONCHUNK3_1+2]),
	S_NORMAL (DEMC, 'A',   20, NULL					    , &States[S_DEMONCHUNK3_1+2]),

#define S_DEMONCHUNK3_4 (S_DEMONCHUNK3_1+3)
	S_NORMAL (DEMC, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon1Chunk3, Hexen, -1, 0)
	PROP_SpawnState (S_DEMONCHUNK3_1)
	PROP_DeathState (S_DEMONCHUNK3_4)
END_DEFAULTS

// Demon, type 1, chunk 4 ---------------------------------------------------

class ADemon1Chunk4 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon1Chunk4, ADemonChunk)
};

FState ADemon1Chunk4::States[] =
{
#define S_DEMONCHUNK4_1 0
	S_NORMAL (DEMD, 'A',	4, NULL					    , &States[S_DEMONCHUNK4_1+1]),
	S_NORMAL (DEMD, 'A',   10, A_QueueCorpse		    , &States[S_DEMONCHUNK4_1+2]),
	S_NORMAL (DEMD, 'A',   20, NULL					    , &States[S_DEMONCHUNK4_1+2]),

#define S_DEMONCHUNK4_4 (S_DEMONCHUNK4_1+3)
	S_NORMAL (DEMD, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon1Chunk4, Hexen, -1, 0)
	PROP_SpawnState (S_DEMONCHUNK4_1)
	PROP_DeathState (S_DEMONCHUNK4_4)
END_DEFAULTS

// Demon, type 1, chunk 5 ---------------------------------------------------

class ADemon1Chunk5 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon1Chunk5, ADemonChunk)
};

FState ADemon1Chunk5::States[] =
{
#define S_DEMONCHUNK5_1 0
	S_NORMAL (DEME, 'A',	4, NULL					    , &States[S_DEMONCHUNK5_1+1]),
	S_NORMAL (DEME, 'A',   10, A_QueueCorpse		    , &States[S_DEMONCHUNK5_1+2]),
	S_NORMAL (DEME, 'A',   20, NULL					    , &States[S_DEMONCHUNK5_1+2]),

#define S_DEMONCHUNK5_4 (S_DEMONCHUNK5_1+3)
	S_NORMAL (DEME, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon1Chunk5, Hexen, -1, 0)
	PROP_SpawnState (S_DEMONCHUNK5_1)
	PROP_DeathState (S_DEMONCHUNK5_4)
END_DEFAULTS

// Demon, type 1, projectile ------------------------------------------------

class ADemon1FX1 : public AActor
{
	DECLARE_ACTOR (ADemon1FX1, AActor)
};

FState ADemon1FX1::States[] =
{
#define S_DEMONFX_MOVE1 0
	S_BRIGHT (DMFX, 'A',	4, NULL					    , &States[S_DEMONFX_MOVE1+1]),
	S_BRIGHT (DMFX, 'B',	4, NULL					    , &States[S_DEMONFX_MOVE1+2]),
	S_BRIGHT (DMFX, 'C',	4, NULL					    , &States[S_DEMONFX_MOVE1]),

#define S_DEMONFX_BOOM1 (S_DEMONFX_MOVE1+3)
	S_BRIGHT (DMFX, 'D',	4, NULL					    , &States[S_DEMONFX_BOOM1+1]),
	S_BRIGHT (DMFX, 'E',	4, NULL					    , &States[S_DEMONFX_BOOM1+2]),
	S_BRIGHT (DMFX, 'F',	3, NULL					    , &States[S_DEMONFX_BOOM1+3]),
	S_BRIGHT (DMFX, 'G',	3, NULL					    , &States[S_DEMONFX_BOOM1+4]),
	S_BRIGHT (DMFX, 'H',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon1FX1, Hexen, -1, 0)
	PROP_SpeedFixed (15)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_Damage (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_FIREDAMAGE|MF2_IMPACT|MF2_PCROSS)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_DEMONFX_MOVE1)
	PROP_DeathState (S_DEMONFX_BOOM1)

	PROP_DeathSound ("DemonMissileExplode")
END_DEFAULTS


// Demon, type 2 (brown) ----------------------------------------------------

class ADemon2 : public ADemon1
{
	DECLARE_ACTOR (ADemon2, ADemon1)
};

FState ADemon2::States[] =
{
#define S_DEMN2_LOOK1 0
	S_NORMAL (DEM2, 'A',   10, A_Look				    , &States[S_DEMN2_LOOK1+1]),
	S_NORMAL (DEM2, 'A',   10, A_Look				    , &States[S_DEMN2_LOOK1]),

#define S_DEMN2_CHASE1 (S_DEMN2_LOOK1+2)
	S_NORMAL (DEM2, 'A',	4, A_Chase				    , &States[S_DEMN2_CHASE1+1]),
	S_NORMAL (DEM2, 'B',	4, A_Chase				    , &States[S_DEMN2_CHASE1+2]),
	S_NORMAL (DEM2, 'C',	4, A_Chase				    , &States[S_DEMN2_CHASE1+3]),
	S_NORMAL (DEM2, 'D',	4, A_Chase				    , &States[S_DEMN2_CHASE1]),

#define S_DEMN2_PAIN1 (S_DEMN2_CHASE1+4)
	S_NORMAL (DEM2, 'E',	4, NULL					    , &States[S_DEMN2_PAIN1+1]),
	S_NORMAL (DEM2, 'E',	4, A_Pain				    , &States[S_DEMN2_CHASE1]),

#define S_DEMN2_ATK1_1 (S_DEMN2_PAIN1+2)
	S_NORMAL (DEM2, 'E',	6, A_FaceTarget			    , &States[S_DEMN2_ATK1_1+1]),
	S_NORMAL (DEM2, 'F',	8, A_FaceTarget			    , &States[S_DEMN2_ATK1_1+2]),
	S_NORMAL (DEM2, 'G',	6, A_DemonAttack1		    , &States[S_DEMN2_CHASE1]),

#define S_DEMN2_ATK2_1 (S_DEMN2_ATK1_1+3)
	S_NORMAL (DEM2, 'E',	5, A_FaceTarget			    , &States[S_DEMN2_ATK2_1+1]),
	S_NORMAL (DEM2, 'F',	6, A_FaceTarget			    , &States[S_DEMN2_ATK2_1+2]),
	S_NORMAL (DEM2, 'G',	5, A_DemonAttack2_2		    , &States[S_DEMN2_CHASE1]),

#define S_DEMN2_DEATH1 (S_DEMN2_ATK2_1+3)
	S_NORMAL (DEM2, 'H',	6, NULL					    , &States[S_DEMN2_DEATH1+1]),
	S_NORMAL (DEM2, 'I',	6, NULL					    , &States[S_DEMN2_DEATH1+2]),
	S_NORMAL (DEM2, 'J',	6, A_Scream				    , &States[S_DEMN2_DEATH1+3]),
	S_NORMAL (DEM2, 'K',	6, A_NoBlocking			    , &States[S_DEMN2_DEATH1+4]),
	S_NORMAL (DEM2, 'L',	6, A_QueueCorpse		    , &States[S_DEMN2_DEATH1+5]),
	S_NORMAL (DEM2, 'M',	6, NULL					    , &States[S_DEMN2_DEATH1+6]),
	S_NORMAL (DEM2, 'N',	6, NULL					    , &States[S_DEMN2_DEATH1+7]),
	S_NORMAL (DEM2, 'O',	6, NULL					    , &States[S_DEMN2_DEATH1+8]),
	S_NORMAL (DEM2, 'P',   -1, NULL					    , NULL),

#define S_DEMN2_XDEATH1 (S_DEMN2_DEATH1+9)
	S_NORMAL (DEM2, 'H',	6, NULL					    , &States[S_DEMN2_XDEATH1+1]),
	S_NORMAL (DEM2, 'I',	6, A_Demon2Death		    , &States[S_DEMN2_XDEATH1+2]),
	S_NORMAL (DEM2, 'J',	6, A_Scream				    , &States[S_DEMN2_XDEATH1+3]),
	S_NORMAL (DEM2, 'K',	6, A_NoBlocking			    , &States[S_DEMN2_XDEATH1+4]),
	S_NORMAL (DEM2, 'L',	6, A_QueueCorpse		    , &States[S_DEMN2_XDEATH1+5]),
	S_NORMAL (DEM2, 'M',	6, NULL					    , &States[S_DEMN2_XDEATH1+6]),
	S_NORMAL (DEM2, 'N',	6, NULL					    , &States[S_DEMN2_XDEATH1+7]),
	S_NORMAL (DEM2, 'O',	6, NULL					    , &States[S_DEMN2_XDEATH1+8]),
	S_NORMAL (DEM2, 'P',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon2, Hexen, 8080, 0)
	PROP_SpawnState (S_DEMN2_LOOK1)
	PROP_SeeState (S_DEMN2_CHASE1)
	PROP_PainState (S_DEMN2_PAIN1)
	PROP_MeleeState (S_DEMN2_ATK1_1)
	PROP_MissileState (S_DEMN2_ATK2_1)
	PROP_DeathState (S_DEMN2_DEATH1)
	PROP_XDeathState (S_DEMN2_XDEATH1)
END_DEFAULTS

// Demon, type 2, mashed ----------------------------------------------------



class ADemon2Mash : public ADemon2
{
	DECLARE_STATELESS_ACTOR (ADemon2Mash, ADemon2)
};

IMPLEMENT_STATELESS_ACTOR (ADemon2Mash, Hexen, -1, 101)
	PROP_FlagsSet (MF_NOBLOOD)
	PROP_Flags2Set (MF2_BLASTED)
	PROP_Flags2Clear (MF2_TELESTOMP)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)

	PROP_DeathState (~0)
	PROP_XDeathState (~0)
	PROP_IDeathState (~0)
END_DEFAULTS

// Demon, type 2, chunk 1 ---------------------------------------------------

class ADemon2Chunk1 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon2Chunk1, ADemonChunk)
};

FState ADemon2Chunk1::States[] =
{
#define S_DEMON2CHUNK1_1 0
	S_NORMAL (DMBA, 'A',	4, NULL					    , &States[S_DEMON2CHUNK1_1+1]),
	S_NORMAL (DMBA, 'A',   10, A_QueueCorpse		    , &States[S_DEMON2CHUNK1_1+2]),
	S_NORMAL (DMBA, 'A',   20, NULL					    , &States[S_DEMON2CHUNK1_1+2]),

#define S_DEMON2CHUNK1_4 (S_DEMON2CHUNK1_1+3)
	S_NORMAL (DMBA, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon2Chunk1, Hexen, -1, 0)
	PROP_SpawnState (S_DEMON2CHUNK1_1)
	PROP_DeathState (S_DEMON2CHUNK1_4)
END_DEFAULTS

// Demon, type 2, chunk 2 ---------------------------------------------------

class ADemon2Chunk2 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon2Chunk2, ADemonChunk)
};

FState ADemon2Chunk2::States[] =
{
#define S_DEMON2CHUNK2_1 0
	S_NORMAL (DMBB, 'A',	4, NULL					    , &States[S_DEMON2CHUNK2_1+1]),
	S_NORMAL (DMBB, 'A',   10, A_QueueCorpse		    , &States[S_DEMON2CHUNK2_1+2]),
	S_NORMAL (DMBB, 'A',   20, NULL					    , &States[S_DEMON2CHUNK2_1+2]),

#define S_DEMON2CHUNK2_4 (S_DEMON2CHUNK2_1+3)
	S_NORMAL (DMBB, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon2Chunk2, Hexen, -1, 0)
	PROP_SpawnState (S_DEMON2CHUNK2_1)
	PROP_DeathState (S_DEMON2CHUNK2_4)
END_DEFAULTS

// Demon, type 2, chunk 3 ---------------------------------------------------

class ADemon2Chunk3 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon2Chunk3, ADemonChunk)
};

FState ADemon2Chunk3::States[] =
{
#define S_DEMON2CHUNK3_1 0
	S_NORMAL (DMBC, 'A',	4, NULL					    , &States[S_DEMON2CHUNK3_1+1]),
	S_NORMAL (DMBC, 'A',   10, A_QueueCorpse		    , &States[S_DEMON2CHUNK3_1+2]),
	S_NORMAL (DMBC, 'A',   20, NULL					    , &States[S_DEMON2CHUNK3_1+2]),

#define S_DEMON2CHUNK3_4 (S_DEMON2CHUNK3_1+3)
	S_NORMAL (DMBC, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon2Chunk3, Hexen, -1, 0)
	PROP_SpawnState (S_DEMON2CHUNK3_1)
	PROP_DeathState (S_DEMON2CHUNK3_4)
END_DEFAULTS

// Demon, type 2, chunk 4 ---------------------------------------------------

class ADemon2Chunk4 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon2Chunk4, ADemonChunk)
};

FState ADemon2Chunk4::States[] =
{
#define S_DEMON2CHUNK4_1 0
	S_NORMAL (DMBD, 'A',	4, NULL					    , &States[S_DEMON2CHUNK4_1+1]),
	S_NORMAL (DMBD, 'A',   10, A_QueueCorpse		    , &States[S_DEMON2CHUNK4_1+2]),
	S_NORMAL (DMBD, 'A',   20, NULL					    , &States[S_DEMON2CHUNK4_1+2]),

#define S_DEMON2CHUNK4_4 (S_DEMON2CHUNK4_1+3)
	S_NORMAL (DMBD, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon2Chunk4, Hexen, -1, 0)
	PROP_SpawnState (S_DEMON2CHUNK4_1)
	PROP_DeathState (S_DEMON2CHUNK4_4)
END_DEFAULTS

// Demon, type 2, chunk 5 ---------------------------------------------------

class ADemon2Chunk5 : public ADemonChunk
{
	DECLARE_ACTOR (ADemon2Chunk5, ADemonChunk)
};

FState ADemon2Chunk5::States[] =
{
#define S_DEMON2CHUNK5_1 0
	S_NORMAL (DMBE, 'A',	4, NULL					    , &States[S_DEMON2CHUNK5_1+1]),
	S_NORMAL (DMBE, 'A',   10, NULL					    , &States[S_DEMON2CHUNK5_1+2]),
	S_NORMAL (DMBE, 'A',   20, NULL					    , &States[S_DEMON2CHUNK5_1+2]),

#define S_DEMON2CHUNK5_4 (S_DEMON2CHUNK5_1+3)
	S_NORMAL (DMBE, 'A',   -1, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon2Chunk5, Hexen, -1, 0)
	PROP_SpawnState (S_DEMON2CHUNK5_1)
	PROP_DeathState (S_DEMON2CHUNK5_4)
END_DEFAULTS

// Demon, type 2, projectile ------------------------------------------------

class ADemon2FX1 : public AActor
{
	DECLARE_ACTOR (ADemon2FX1, AActor)
};

FState ADemon2FX1::States[] =
{
#define S_DEMON2FX_MOVE1 0
	S_BRIGHT (D2FX, 'A',	4, NULL					    , &States[S_DEMON2FX_MOVE1+1]),
	S_BRIGHT (D2FX, 'B',	4, NULL					    , &States[S_DEMON2FX_MOVE1+2]),
	S_BRIGHT (D2FX, 'C',	4, NULL					    , &States[S_DEMON2FX_MOVE1+3]),
	S_BRIGHT (D2FX, 'D',	4, NULL					    , &States[S_DEMON2FX_MOVE1+4]),
	S_BRIGHT (D2FX, 'E',	4, NULL					    , &States[S_DEMON2FX_MOVE1+5]),
	S_BRIGHT (D2FX, 'F',	4, NULL					    , &States[S_DEMON2FX_MOVE1]),

#define S_DEMON2FX_BOOM1 (S_DEMON2FX_MOVE1+6)
	S_BRIGHT (D2FX, 'G',	4, NULL					    , &States[S_DEMON2FX_BOOM1+1]),
	S_BRIGHT (D2FX, 'H',	4, NULL					    , &States[S_DEMON2FX_BOOM1+2]),
	S_BRIGHT (D2FX, 'I',	4, NULL					    , &States[S_DEMON2FX_BOOM1+3]),
	S_BRIGHT (D2FX, 'J',	4, NULL					    , &States[S_DEMON2FX_BOOM1+4]),
	S_BRIGHT (D2FX, 'K',	3, NULL					    , &States[S_DEMON2FX_BOOM1+5]),
	S_BRIGHT (D2FX, 'L',	3, NULL					    , NULL),
};

IMPLEMENT_ACTOR (ADemon2FX1, Hexen, -1, 0)
	PROP_SpeedFixed (15)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (6)
	PROP_Damage (5)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_FIREDAMAGE|MF2_IMPACT|MF2_PCROSS)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_DEMON2FX_MOVE1)
	PROP_DeathState (S_DEMON2FX_BOOM1)

	PROP_DeathSound ("DemonMissileExplode")
END_DEFAULTS

//============================================================================
//
// A_DemonAttack1 (melee)
//
//============================================================================

void A_DemonAttack1 (AActor *actor)
{
	if (P_CheckMeleeRange (actor))
	{
		int damage = pr_atk.HitDice (2);
		P_DamageMobj (actor->target, actor, actor, damage);
		P_TraceBleed (damage, actor->target, actor);
	}
}


//============================================================================
//
// A_DemonAttack2_1 (missile)
//
//============================================================================

void A_DemonAttack2_1 (AActor *actor)
{
	AActor *mo;

	mo = P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(ADemon1FX1));
	if (mo)
	{
		mo->z += 30*FRACUNIT;
		S_Sound (actor, CHAN_BODY, "DemonMissileFire", 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_DemonAttack2_2 (missile)
//
//============================================================================

void A_DemonAttack2_2 (AActor *actor)
{
	AActor *mo;

	mo = P_SpawnMissile (actor, actor->target, RUNTIME_CLASS(ADemon2FX1));
	if (mo)
	{
		mo->z += 30*FRACUNIT;
		S_Sound (actor, CHAN_BODY, "DemonMissileFire", 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_DemonDeath
//
//============================================================================

void A_DemonDeath (AActor *actor)
{
	const TypeInfo *const ChunkTypes[] =
	{
		RUNTIME_CLASS(ADemon1Chunk5),
		RUNTIME_CLASS(ADemon1Chunk4),
		RUNTIME_CLASS(ADemon1Chunk3),
		RUNTIME_CLASS(ADemon1Chunk2),
		RUNTIME_CLASS(ADemon1Chunk1)
	};

	TossChunks (actor, ChunkTypes);
}

//===========================================================================
//
// A_Demon2Death
//
//===========================================================================

void A_Demon2Death (AActor *actor)
{
	const TypeInfo *const ChunkTypes[] =
	{
		RUNTIME_CLASS(ADemon2Chunk5),
		RUNTIME_CLASS(ADemon2Chunk4),
		RUNTIME_CLASS(ADemon2Chunk3),
		RUNTIME_CLASS(ADemon2Chunk2),
		RUNTIME_CLASS(ADemon2Chunk1)
	};

	TossChunks (actor, ChunkTypes);
}

static void TossChunks (AActor *actor, const TypeInfo *const chunks[])
{
	AActor *mo;
	angle_t angle;
	int i;

	for (i = 4; i >= 0; --i)
	{
		mo = Spawn (chunks[i], actor->x, actor->y, actor->z+45*FRACUNIT);
		if (mo)
		{
			angle = actor->angle + (i<4 ? ANGLE_270 : ANGLE_90);
			mo->momz = 8*FRACUNIT;
			mo->momx = FixedMul((pr_demonchunks()<<10)+FRACUNIT,
				finecosine[angle>>ANGLETOFINESHIFT]);
			mo->momy = FixedMul((pr_demonchunks()<<10)+FRACUNIT, 
				finesine[angle>>ANGLETOFINESHIFT]);
			mo->target = actor;
		}
	}
}
