#include "actor.h"
#include "info.h"
#include "gi.h"
#include "m_random.h"

static FRandom pr_orbit ("Orbit");

void A_BridgeOrbit (AActor *);
void A_BridgeInit (AActor *);

// The Hexen (and Heretic) version of the bridge spawns extra floating
// "balls" orbiting the bridge. The Doom version only shows the bridge
// itself.

// Bridge ball -------------------------------------------------------------

class ABridgeBall : public AActor
{
	DECLARE_ACTOR (ABridgeBall, AActor)
};

FState ABridgeBall::States[] =
{
	S_NORMAL (TLGL, 'A',    2, NULL                         , &States[1]),
	S_NORMAL (TLGL, 'A',    1, A_BridgeOrbit                , &States[1])
};

IMPLEMENT_ACTOR (ABridgeBall, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_SpawnState (0)
END_DEFAULTS

// The bridge itself -------------------------------------------------------

class ABridge : public AActor
{
	DECLARE_ACTOR (ABridge, AActor)
};

FState ABridge::States[] =
{
#define S_DBRIDGE 0
	S_BRIGHT (TLGL, 'A',	4, NULL 						, &States[S_DBRIDGE+1]),
	S_BRIGHT (TLGL, 'B',	4, NULL 						, &States[S_DBRIDGE+2]),
	S_BRIGHT (TLGL, 'C',	4, NULL 						, &States[S_DBRIDGE+3]),
	S_BRIGHT (TLGL, 'D',	4, NULL 						, &States[S_DBRIDGE+4]),
	S_BRIGHT (TLGL, 'E',	4, NULL 						, &States[S_DBRIDGE+0]),

#define S_BRIDGE (S_DBRIDGE+5)
	S_NORMAL (TLGL, 'A',    2, NULL                         , &States[S_BRIDGE+1]),
	S_NORMAL (TLGL, 'A',    2, A_BridgeInit                 , &States[S_BRIDGE+2]),
	S_NORMAL (TLGL, 'A',   -1, NULL                         , NULL),

#define S_FREE_BRIDGE (S_BRIDGE+3)
	S_NORMAL (TLGL, 'A',    2, NULL                         , &States[S_FREE_BRIDGE+1]),
	S_NORMAL (TLGL, 'A',  300, NULL                         , NULL)
};

IMPLEMENT_ACTOR (ABridge, Any, 118, 21)
	PROP_Flags (MF_SOLID|MF_NOGRAVITY|MF_NOLIFTDROP)
END_DEFAULTS

AT_GAME_SET (Bridge)
{
	ABridge *def = GetDefault<ABridge> ();

	if (gameinfo.gametype == GAME_Doom)
	{
		def->SpawnState = &ABridge::States[S_DBRIDGE];
		def->radius = 36 * FRACUNIT;
		def->height = 4 * FRACUNIT;
		def->RenderStyle = STYLE_Normal;
	}
	else
	{
		def->SpawnState = &ABridge::States[S_BRIDGE];
		def->radius = 32 * FRACUNIT;
		def->height = 2 * FRACUNIT;
		def->RenderStyle = STYLE_None;
	}
}

// Action functions for the non-Doom bridge --------------------------------

#define ORBIT_RADIUS	15

// New bridge stuff
//	Parent
//		special1	true == removing from world
//
//	Child
//		target		pointer to center mobj
//		angle		angle of ball

void A_BridgeOrbit (AActor *self)
{
	if (self->target == NULL)
	{ // Don't crash if somebody spawned this into the world
	  // independantly of a Bridge actor.
		return;
	}
	if (self->target->special1)
	{
		self->SetState (NULL);
	}
	// Every five tics, Hexen moved the ball 3/256th of a revolution.
	self->angle += ANGLE_45/32*3/5;
	self->x = self->target->x + ORBIT_RADIUS * finecosine[self->angle >> ANGLETOFINESHIFT];
	self->y = self->target->y + ORBIT_RADIUS * finesine[self->angle >> ANGLETOFINESHIFT];
	self->z = self->target->z;
}

void A_BridgeInit (AActor *self)
{
	angle_t startangle;
	AActor *ball1, *ball2, *ball3;
	fixed_t cx, cy, cz;

	cx = self->x;
	cy = self->y;
	cz = self->z;
	startangle = pr_orbit() << 24;
	self->special1 = 0;

	// Spawn triad into world
	ball1 = Spawn<ABridgeBall> (cx, cy, cz, ALLOW_REPLACE);
	ball1->angle = startangle;
	ball1->target = self;

	ball2 = Spawn<ABridgeBall> (cx, cy, cz, ALLOW_REPLACE);
	ball2->angle = startangle + ANGLE_45/32*85;
	ball2->target = self;

	ball3 = Spawn<ABridgeBall> (cx, cy, cz, ALLOW_REPLACE);
	ball3->angle = startangle + (angle_t)ANGLE_45/32*170;
	ball3->target = self;

	A_BridgeOrbit (ball1);
	A_BridgeOrbit (ball2);
	A_BridgeOrbit (ball3);
}

void A_BridgeRemove (AActor *self)
{
	self->special1 = true;		// Removing the bridge
	self->flags &= ~MF_SOLID;
	self->SetState (&ABridge::States[S_FREE_BRIDGE]);
}

// Invisible bridge --------------------------------------------------------

class AInvisibleBridge : public ABridge
{
	DECLARE_ACTOR (AInvisibleBridge, ABridge)
public:
	void BeginPlay ();
	void Tick ()
	{
		Super::Tick ();
	}
};

FState AInvisibleBridge::States[] =
{
	S_NORMAL (TNT1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AInvisibleBridge, Any, 9990, 0)
	PROP_RenderStyle (STYLE_None)
	PROP_SpawnState (0)
	PROP_RadiusFixed (32)
	PROP_HeightFixed (4)
	PROP_Flags4 (MF4_ACTLIKEBRIDGE)
END_DEFAULTS

void AInvisibleBridge::BeginPlay ()
{
	Super::BeginPlay ();
	if (args[0])
		radius = args[0] << FRACBITS;
	if (args[1])
		height = args[1] << FRACBITS;
}

// And some invisible bridges from Skull Tag -------------------------------

class AInvisibleBridge32 : public AInvisibleBridge
{
	DECLARE_STATELESS_ACTOR (AInvisibleBridge32, AInvisibleBridge)
};

IMPLEMENT_STATELESS_ACTOR (AInvisibleBridge32, Any, 5061, 0)
	PROP_RadiusFixed (32)
	PROP_HeightFixed (8)
END_DEFAULTS


class AInvisibleBridge16 : public AInvisibleBridge
{
	DECLARE_STATELESS_ACTOR (AInvisibleBridge16, AInvisibleBridge)
};

IMPLEMENT_STATELESS_ACTOR (AInvisibleBridge16, Any, 5064, 0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (8)
END_DEFAULTS


class AInvisibleBridge8 : public AInvisibleBridge
{
	DECLARE_STATELESS_ACTOR (AInvisibleBridge8, AInvisibleBridge)
};

IMPLEMENT_STATELESS_ACTOR (AInvisibleBridge8, Any, 5065, 0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
END_DEFAULTS
