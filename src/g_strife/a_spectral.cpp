#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_strifeglobal.h"

void A_SpectralLightning (AActor *);

void A_AlertMonsters (AActor *);
void A_Countdown (AActor *);
void A_Tracer2 (AActor *);
AActor *P_SpawnSubMissile (AActor *source, PClass *type, AActor *target);

// Container for all spectral lightning deaths ------------------------------

FState ASpectralLightningBase::States[] =
{
	S_NORMAL (ZAP1, 'B', 3, A_Explode,			&States[1]),
	S_NORMAL (ZAP1, 'A', 3, A_AlertMonsters,	&States[2]),
	S_NORMAL (ZAP1, 'B', 3, NULL,				&States[3]),
	S_NORMAL (ZAP1, 'C', 3, NULL,				&States[4]),
	S_NORMAL (ZAP1, 'D', 3, NULL,				&States[5]),
	S_NORMAL (ZAP1, 'E', 3, NULL,				&States[6]),
	S_NORMAL (ZAP1, 'F', 3, NULL,				&States[7]),
	S_NORMAL (ZAP1, 'E', 3, NULL,				&States[8]),
	S_NORMAL (ZAP1, 'D', 2, NULL,				&States[9]),
	S_NORMAL (ZAP1, 'C', 2, NULL,				&States[10]),
	S_NORMAL (ZAP1, 'B', 2, NULL,				&States[11]),
	S_NORMAL (ZAP1, 'A', 1, NULL,				NULL),
};

IMPLEMENT_ACTOR (ASpectralLightningBase, Strife, -1, 0)
	PROP_DeathState (0)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_IMPACT|MF2_PCROSS)
	PROP_Flags4 (MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
	PROP_RenderStyle (STYLE_Add)
	PROP_SeeSound ("weapons/sigil")
	PROP_DeathSound ("weapons/sigilhit")
END_DEFAULTS

void ASpectralLightningBase::GetExplodeParms (int &damage, int &dist, bool &hurtSource)
{
	damage = dist = 32;
}

// Spectral Lightning death that does not explode ---------------------------

IMPLEMENT_STATELESS_ACTOR (ASpectralLightningDeath1, Strife, -1, 0)
	PROP_DeathState (1)
END_DEFAULTS

// Spectral Lightning death that does not alert monsters --------------------

IMPLEMENT_STATELESS_ACTOR (ASpectralLightningDeath2, Strife, -1, 0)
	PROP_DeathState (2)
END_DEFAULTS

// Spectral Lightning death that is shorter than the rest -------------------

IMPLEMENT_STATELESS_ACTOR (ASpectralLightningDeathShort, Strife, -1, 0)
	PROP_DeathState (6)
END_DEFAULTS

// Spectral Lightning (Ball Shaped #1) --------------------------------------

FState ASpectralLightningBall1::States[] =
{
	S_BRIGHT (ZOT3, 'A', 4, NULL,				&States[1]),
	S_BRIGHT (ZOT3, 'B', 4, NULL,				&States[2]),
	S_BRIGHT (ZOT3, 'C', 4, NULL,				&States[3]),
	S_BRIGHT (ZOT3, 'D', 4, NULL,				&States[4]),
	S_BRIGHT (ZOT3, 'E', 4, NULL,				&States[0]),
};

IMPLEMENT_ACTOR (ASpectralLightningBall1, Strife, -1, 0)
	PROP_StrifeType (80)
	PROP_SpawnState (0)
	PROP_SpeedFixed (30)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (16)
	PROP_Damage (70)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags4 (MF4_SPECTRAL|MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
END_DEFAULTS

// Spectral Lightning (Ball Shaped #2) --------------------------------------

IMPLEMENT_STATELESS_ACTOR (ASpectralLightningBall2, Strife, -1, 0)
	PROP_StrifeType (81)
	PROP_Damage (20)
END_DEFAULTS

// Spectral Lightning (Horizontal #1) ---------------------------------------

void A_SpectralLightningTail (AActor *);

FState ASpectralLightningH1::States[] =
{
	S_BRIGHT (ZAP6, 'A', 4, NULL,				&States[1]),
	S_BRIGHT (ZAP6, 'B', 4, A_SpectralLightningTail,			&States[2]),
	S_BRIGHT (ZAP6, 'C', 4, A_SpectralLightningTail,			&States[0])
};

IMPLEMENT_ACTOR (ASpectralLightningH1, Strife, -1, 0)
	PROP_StrifeType (78)
	PROP_SpawnState (0)
	PROP_SpeedFixed (30)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (16)
	PROP_Damage (70)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags4 (MF4_SPECTRAL|MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
END_DEFAULTS

// Spectral Lightning (Horizontal #2) -------------------------------------

IMPLEMENT_STATELESS_ACTOR (ASpectralLightningH2, Strife, -1, 0)
	PROP_StrifeType (79)
	PROP_Damage (20)
END_DEFAULTS

// Spectral Lightning (Horizontal #3) -------------------------------------

IMPLEMENT_STATELESS_ACTOR (ASpectralLightningH3, Strife, -1, 0)
	PROP_StrifeType (82)
	PROP_Damage (10)
END_DEFAULTS

// ASpectralLightningHTail --------------------------------------------------

FState ASpectralLightningHTail::States[] =
{
	S_BRIGHT (ZAP6, 'A', 5, NULL, &States[1]),
	S_BRIGHT (ZAP6, 'B', 5, NULL, &States[2]),
	S_BRIGHT (ZAP6, 'C', 5, NULL, NULL)
};

IMPLEMENT_ACTOR (ASpectralLightningHTail, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF)
	PROP_Flags4Clear (MF4_SPECTRAL)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

void A_SpectralLightningTail (AActor *self)
{
	AActor *foo = Spawn<ASpectralLightningHTail> (self->x - self->momx, self->y - self->momy, self->z, ALLOW_REPLACE);

	foo->angle = self->angle;
	foo->health = self->health;
}

// Spectral Lightning (Big Ball #1) -----------------------------------------

void A_SpectralBigBallLightning (AActor *);

FState ASpectralLightningBigBall1::States[] =
{
	S_BRIGHT (ZAP7, 'A', 4, A_SpectralBigBallLightning,		&States[1]),
	S_BRIGHT (ZAP7, 'B', 4, A_SpectralBigBallLightning,		&States[2]),
	S_BRIGHT (ZAP7, 'C', 6, A_SpectralBigBallLightning,		&States[3]),
	S_BRIGHT (ZAP7, 'D', 6, A_SpectralBigBallLightning,		&States[4]),
	S_BRIGHT (ZAP7, 'E', 6, A_SpectralBigBallLightning,		&States[0]),
};

IMPLEMENT_ACTOR (ASpectralLightningBigBall1, Strife, -1, 0)
	PROP_StrifeType (84)
	PROP_SpawnState (0)
	PROP_SpeedFixed (18)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (40)
	PROP_Damage (130)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags4 (MF4_SPECTRAL|MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
END_DEFAULTS


void A_SpectralBigBallLightning (AActor *self)
{
	self->angle += ANGLE_90;
	P_SpawnSubMissile (self, RUNTIME_CLASS(ASpectralLightningH3), self->target);
	self->angle += ANGLE_180;
	P_SpawnSubMissile (self, RUNTIME_CLASS(ASpectralLightningH3), self->target);
	self->angle += ANGLE_90;
	P_SpawnSubMissile (self, RUNTIME_CLASS(ASpectralLightningH3), self->target);
}


// Spectral Lightning (Big Ball #2 - less damaging) -------------------------

IMPLEMENT_STATELESS_ACTOR (ASpectralLightningBigBall2, Strife, -1, 0)
	PROP_StrifeType (85)
	PROP_Damage (30)
END_DEFAULTS

// Sigil Lightning (Vertical #1) --------------------------------------------

FState ASpectralLightningV1::States[] =
{
	S_BRIGHT (ZOT1, 'A', 4, NULL, &States[1]),
	S_BRIGHT (ZOT1, 'B', 4, NULL, &States[2]),
	S_BRIGHT (ZOT1, 'C', 6, NULL, &States[3]),
	S_BRIGHT (ZOT1, 'D', 6, NULL, &States[4]),
	S_BRIGHT (ZOT1, 'E', 6, NULL, &States[0]),
	// Apparent Strife bug: The last state for this sprite used frame D
	// instead of E, even though E is in the wad.
};

IMPLEMENT_ACTOR (ASpectralLightningV1, Strife, -1, 0)
	PROP_StrifeType (86)
	PROP_SpawnState (0)
	PROP_SpeedFixed (22)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (24)
	PROP_Damage (100)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags4 (MF4_SPECTRAL|MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
END_DEFAULTS

// Sigil Lightning (Vertical #2 - less damaging) ----------------------------

IMPLEMENT_STATELESS_ACTOR (ASpectralLightningV2, Strife, -1, 0)
	PROP_StrifeType (87)
	PROP_Damage (50)
END_DEFAULTS

// Sigil Lightning Spot (roams around dropping lightning from above) --------

FState ASpectralLightningSpot::States[] =
{
	S_BRIGHT (ZAP5, 'A', 4, A_Countdown,	&States[1]),
	S_BRIGHT (ZAP5, 'B', 4, A_SpectralLightning,		&States[2]),
	S_BRIGHT (ZAP5, 'C', 4, A_Countdown,	&States[3]),
	S_BRIGHT (ZAP5, 'D', 4, A_Countdown,	&States[0]),
};

IMPLEMENT_ACTOR (ASpectralLightningSpot, Strife, -1, 0)
	PROP_StrifeType (88)
	PROP_SpawnState (0)
	PROP_SpeedFixed (18)
	PROP_ReactionTime (70)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags3 (MF3_NOBLOCKMONST)
	PROP_Flags5 (MF5_NODROPOFF)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HR_SHADOW)
END_DEFAULTS

// Sigil Lightning (Big Vertical #1) ----------------------------------------

FState ASpectralLightningBigV1::States[] =
{
	S_BRIGHT (ZOT2, 'A', 4, A_Tracer2, &States[1]),
	S_BRIGHT (ZOT2, 'B', 4, A_Tracer2, &States[2]),
	S_BRIGHT (ZOT2, 'C', 4, A_Tracer2, &States[3]),
	S_BRIGHT (ZOT2, 'D', 4, A_Tracer2, &States[4]),
	S_BRIGHT (ZOT2, 'E', 4, A_Tracer2, &States[0]),
};

IMPLEMENT_ACTOR (ASpectralLightningBigV1, Strife, -1, 0)
	PROP_StrifeType (89)
	PROP_SpawnState (0)
	PROP_SpeedFixed (28)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (16)
	PROP_Damage (120)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags4 (MF4_SPECTRAL|MF4_STRIFEDAMAGE)
	PROP_MaxStepHeight (4)
END_DEFAULTS

// Actor 90 -----------------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (ASpectralLightningBigV2, Strife, -1, 0)
	PROP_StrifeType (90)
	PROP_Damage (60)
END_DEFAULTS

// "Zap 5" ------------------------------------------------------------------

static FRandom pr_zap5 ("Zap5");

void A_SpectralLightning (AActor *self)
{
	AActor *flash;
	fixed_t x, y;

	if (self->threshold != 0)
		--self->threshold;

	self->momx += pr_zap5.Random2(3) << FRACBITS;
	self->momy += pr_zap5.Random2(3) << FRACBITS;

	x = self->x + pr_zap5.Random2(3) * FRACUNIT * 50;
	y = self->y + pr_zap5.Random2(3) * FRACUNIT * 50;

	flash = Spawn (self->threshold > 25 ? RUNTIME_CLASS(ASpectralLightningV2) :
		RUNTIME_CLASS(ASpectralLightningV1), x, y, ONCEILINGZ, ALLOW_REPLACE);

	flash->target = self->target;
	flash->momz = -18*FRACUNIT;
	flash->health = self->health;

	flash = Spawn<ASpectralLightningV2> (self->x, self->y, ONCEILINGZ, ALLOW_REPLACE);

	flash->target = self->target;
	flash->momz = -18*FRACUNIT;
	flash->health = self->health;
}

// In Strife, this number is stored in the data segment, but it doesn't seem to be
// altered anywhere.
#define TRACEANGLE (0xe000000)

void A_Tracer2 (AActor *self)
{
	AActor *dest;
	angle_t exact;
	fixed_t dist;
	fixed_t slope;

	dest = self->tracer;

	if (dest == NULL || dest->health <= 0 || self->Speed == 0)
		return;

	// change angle
	exact = R_PointToAngle2 (self->x, self->y, dest->x, dest->y);

	if (exact != self->angle)
	{
		if (exact - self->angle > 0x80000000)
		{
			self->angle -= TRACEANGLE;
			if (exact - self->angle < 0x80000000)
				self->angle = exact;
		}
		else
		{
			self->angle += TRACEANGLE;
			if (exact - self->angle > 0x80000000)
				self->angle = exact;
		}
	}

	exact = self->angle >> ANGLETOFINESHIFT;
	self->momx = FixedMul (self->Speed, finecosine[exact]);
	self->momy = FixedMul (self->Speed, finesine[exact]);

	// change slope
	dist = P_AproxDistance (dest->x - self->x, dest->y - self->y);
	dist /= self->Speed;

	if (dist < 1)
	{
		dist = 1;
	}
	if (dest->height >= 56*FRACUNIT)
	{
		slope = (dest->z+40*FRACUNIT - self->z) / dist;
	}
	else
	{
		slope = (dest->z + self->height*2/3 - self->z) / dist;
	}
	if (slope < self->momz)
	{
		self->momz -= FRACUNIT/8;
	}
	else
	{
		self->momz += FRACUNIT/8;
	}
}
