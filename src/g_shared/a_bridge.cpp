#include "actor.h"
#include "info.h"
#include "gi.h"
#include "m_random.h"
#include "thingdef/thingdef.h"

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

// Custom bridge --------------------------------------------------------
/*
	args[0]: Bridge radius, in mapunits
	args[1]: Bridge height, in mapunits
	args[2]: Amount of bridge balls (if 0: Doom bridge)
	args[3]: Rotation speed of bridge balls, in byte angle per seconds, sorta:
		Since an arg is only a byte, it can only go from 0 to 255, while ZDoom's
		BAM go from 0 to 65535. Plus, it needs to be able to go either way. So,
		up to 128, it goes counterclockwise; 129-255 is clockwise, substracting
		256 from it to get the angle. A few example values:
		  0: Hexen default
	     11:  15° / seconds
		 21:  30° / seconds
		 32:  45° / seconds
		 64:  90° / seconds
		128: 180° / seconds
		192: -90° / seconds
		223: -45° / seconds
		233: -30° / seconds
		244: -15° / seconds
		This value only matters if args[2] is not zero.
	args[4]: Rotation radius of bridge balls, in bridge radius %.
		If 0, use Hexen default: ORBIT_RADIUS, regardless of bridge radius.
		This value only matters if args[2] is not zero.
*/

class ACustomBridge : public ABridge
{
	DECLARE_STATELESS_ACTOR (ACustomBridge, ABridge)
public:
	void BeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (ACustomBridge, Any, 9991, 0)
	PROP_SpawnState (S_DBRIDGE)
	PROP_SeeState (S_BRIDGE)
	PROP_DeathState (S_FREE_BRIDGE)
	PROP_Flags4 (MF4_ACTLIKEBRIDGE)
	PROP_RenderStyle (STYLE_None)
END_DEFAULTS

void ACustomBridge::BeginPlay ()
{
	if (args[2]) // Hexen bridge if there are balls
	{
		SetState(FindState(FName("See")));
		radius = args[0] ? args[0] << FRACBITS : 32 * FRACUNIT;
		height = args[1] ? args[1] << FRACBITS : 2 * FRACUNIT;
	}
	else // No balls? Then a Doom bridge.
	{
		radius = args[0] ? args[0] << FRACBITS : 36 * FRACUNIT;
		height = args[1] ? args[1] << FRACBITS : 4 * FRACUNIT;
		RenderStyle = STYLE_Normal;
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
	// Set default values
	// Every five tics, Hexen moved the ball 3/256th of a revolution.
	int rotationspeed  = ANGLE_45/32*3/5;
	int rotationradius = ORBIT_RADIUS;
	// If the bridge is custom, set non-default values if any.
	if (self->target->IsKindOf(PClass::FindClass("CustomBridge")))
	{
		// Set angular speed; 1--128: counterclockwise rotation ~=1--180°; 129--255: clockwise rotation ~= 180--1°
		if (self->target->args[3] > 128) rotationspeed = ANGLE_45/32 * (self->target->args[3]-256) / TICRATE;
		else if (self->target->args[3] > 0) rotationspeed = ANGLE_45/32 * (self->target->args[3]) / TICRATE;
		// Set rotation radius
		if (self->target->args[4]) rotationradius = ((self->target->args[4] * self->target->radius) / (100 * FRACUNIT));
	}
	if (self->target->special1)
	{
		self->SetState (NULL);
	}
	self->angle += rotationspeed;
	self->x = self->target->x + rotationradius * finecosine[self->angle >> ANGLETOFINESHIFT];
	self->y = self->target->y + rotationradius * finesine[self->angle >> ANGLETOFINESHIFT];
	self->z = self->target->z;
}


static const PClass *GetBallType()
{
	const PClass *balltype = NULL;
	int index=CheckIndex(1, NULL);
	if (index>=0) 
	{
		balltype = PClass::FindClass((ENamedName)StateParameters[index]);
	}
	if (balltype == NULL) balltype = PClass::FindClass("BridgeBall");
	return balltype;
}



void A_BridgeInit (AActor *self)
{
	angle_t startangle;
	AActor *ball;
	fixed_t cx, cy, cz;

	cx = self->x;
	cy = self->y;
	cz = self->z;
	startangle = pr_orbit() << 24;
	self->special1 = 0;

	// Spawn triad into world -- may be more than a triad now.
	int ballcount = ((self->GetClass()==PClass::FindClass("Bridge") || (self->args[2]==0)) ? 3 : self->args[2]);
	const PClass *balltype = GetBallType();
	for (int i = 0; i < ballcount; i++)
	{
		ball = Spawn(balltype, cx, cy, cz, ALLOW_REPLACE);
		ball->angle = startangle + (ANGLE_45/32) * (256/ballcount) * i;
		ball->target = self;
		A_BridgeOrbit(ball);
	}
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
