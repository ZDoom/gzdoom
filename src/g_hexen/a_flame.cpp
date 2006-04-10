#include "actor.h"
#include "info.h"
#include "a_action.h"
#include "s_sound.h"

void A_FlameCheck (AActor *);

// Temp Small Flame --------------------------------------------------------

class AFlameSmallTemp : public AActor
{
	DECLARE_ACTOR (AFlameSmallTemp, AActor)
};

FState AFlameSmallTemp::States[] =
{
	S_BRIGHT (FFSM, 'A',	3, NULL					    , &States[1]),
	S_BRIGHT (FFSM, 'B',	3, NULL					    , &States[2]),
	S_BRIGHT (FFSM, 'C',	2, A_FlameCheck			    , &States[3]),
	S_BRIGHT (FFSM, 'C',	2, NULL					    , &States[4]),
	S_BRIGHT (FFSM, 'D',	3, NULL					    , &States[5]),
	S_BRIGHT (FFSM, 'E',	3, A_FlameCheck			    , &States[0]),
};

IMPLEMENT_ACTOR (AFlameSmallTemp, Hexen, 10500, 96)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)
	PROP_SpawnState (0)
END_DEFAULTS

// Temp Large Flame ---------------------------------------------------------

class AFlameLargeTemp : public AActor
{
	DECLARE_ACTOR (AFlameLargeTemp, AActor)
};

FState AFlameLargeTemp::States[] =
{
	S_BRIGHT (FFLG, 'A',	4, NULL					    , &States[1]),
	S_BRIGHT (FFLG, 'B',	4, A_FlameCheck			    , &States[2]),
	S_BRIGHT (FFLG, 'C',	4, NULL					    , &States[3]),
	S_BRIGHT (FFLG, 'D',	4, A_FlameCheck			    , &States[4]),
	S_BRIGHT (FFLG, 'E',	4, NULL					    , &States[5]),
	S_BRIGHT (FFLG, 'F',	4, A_FlameCheck			    , &States[6]),
	S_BRIGHT (FFLG, 'G',	4, NULL					    , &States[7]),
	S_BRIGHT (FFLG, 'H',	4, A_FlameCheck			    , &States[8]),
	S_BRIGHT (FFLG, 'I',	4, NULL					    , &States[9]),
	S_BRIGHT (FFLG, 'J',	4, A_FlameCheck			    , &States[10]),
	S_BRIGHT (FFLG, 'K',	4, NULL					    , &States[11]),
	S_BRIGHT (FFLG, 'L',	4, A_FlameCheck			    , &States[12]),
	S_BRIGHT (FFLG, 'M',	4, NULL					    , &States[13]),
	S_BRIGHT (FFLG, 'N',	4, A_FlameCheck			    , &States[14]),
	S_BRIGHT (FFLG, 'O',	4, NULL					    , &States[15]),
	S_BRIGHT (FFLG, 'P',	4, A_FlameCheck			    , &States[4]),
};

IMPLEMENT_ACTOR (AFlameLargeTemp, Hexen, 10502, 98)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)
	PROP_SpawnState (0)
END_DEFAULTS

// Small Flame --------------------------------------------------------------

class AFlameSmall : public AActor
{
	DECLARE_ACTOR (AFlameSmall, AActor)
public:
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

FState AFlameSmall::States[] =
{
#define S_FLAME_SMALL1 0
	S_BRIGHT (FFSM, 'A',	3, NULL					    , &States[S_FLAME_SMALL1+1]),
	S_BRIGHT (FFSM, 'A',	3, A_UnHideThing		    , &States[S_FLAME_SMALL1+2]),
	S_BRIGHT (FFSM, 'A',	3, NULL					    , &States[S_FLAME_SMALL1+3]),
	S_BRIGHT (FFSM, 'B',	3, NULL					    , &States[S_FLAME_SMALL1+4]),
	S_BRIGHT (FFSM, 'C',	3, NULL					    , &States[S_FLAME_SMALL1+5]),
	S_BRIGHT (FFSM, 'D',	3, NULL					    , &States[S_FLAME_SMALL1+6]),
	S_BRIGHT (FFSM, 'E',	3, NULL					    , &States[S_FLAME_SMALL1+2]),

#define S_FLAME_SDORM1 (S_FLAME_SMALL1+7)
	S_NORMAL (FFSM, 'A',	2, NULL					    , &States[S_FLAME_SDORM1+1]),
	S_NORMAL (FFSM, 'B',	2, A_HideThing			    , &States[S_FLAME_SDORM1+2]),
	S_NORMAL (FFSM, 'C',  200, NULL					    , &States[S_FLAME_SDORM1+2]),

};

IMPLEMENT_ACTOR (AFlameSmall, Hexen, 10501, 97)
	PROP_Flags2 (MF2_NOTELEPORT|RF_INVISIBLE)
	PROP_RenderStyle (STYLE_Add)
	PROP_SpawnState (S_FLAME_SMALL1)
END_DEFAULTS

void AFlameSmall::Activate (AActor *activator)
{
	Super::Activate (activator);
	S_Sound (this, CHAN_BODY, "Ignite", 1, ATTN_NORM);
	SetState (&States[S_FLAME_SMALL1]);
}

void AFlameSmall::Deactivate (AActor *activator)
{
	Super::Deactivate (activator);
	SetState (&States[S_FLAME_SDORM1]);
}

// Large Flame --------------------------------------------------------------

class AFlameLarge : public AActor
{
	DECLARE_ACTOR (AFlameLarge, AActor)
public:
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

FState AFlameLarge::States[] =
{
#define S_FLAME_LARGE1 0
	S_BRIGHT (FFLG, 'A',	2, NULL					    , &States[S_FLAME_LARGE1+1]),
	S_BRIGHT (FFLG, 'A',	2, A_UnHideThing		    , &States[S_FLAME_LARGE1+2]),
	S_BRIGHT (FFLG, 'A',	4, NULL					    , &States[S_FLAME_LARGE1+3]),
	S_BRIGHT (FFLG, 'B',	4, NULL					    , &States[S_FLAME_LARGE1+4]),
	S_BRIGHT (FFLG, 'C',	4, NULL					    , &States[S_FLAME_LARGE1+5]),
	S_BRIGHT (FFLG, 'D',	4, NULL					    , &States[S_FLAME_LARGE1+6]),
	S_BRIGHT (FFLG, 'E',	4, NULL					    , &States[S_FLAME_LARGE1+7]),
	S_BRIGHT (FFLG, 'F',	4, NULL					    , &States[S_FLAME_LARGE1+8]),
	S_BRIGHT (FFLG, 'G',	4, NULL					    , &States[S_FLAME_LARGE1+9]),
	S_BRIGHT (FFLG, 'H',	4, NULL					    , &States[S_FLAME_LARGE1+10]),
	S_BRIGHT (FFLG, 'I',	4, NULL					    , &States[S_FLAME_LARGE1+11]),
	S_BRIGHT (FFLG, 'J',	4, NULL					    , &States[S_FLAME_LARGE1+12]),
	S_BRIGHT (FFLG, 'K',	4, NULL					    , &States[S_FLAME_LARGE1+13]),
	S_BRIGHT (FFLG, 'L',	4, NULL					    , &States[S_FLAME_LARGE1+14]),
	S_BRIGHT (FFLG, 'M',	4, NULL					    , &States[S_FLAME_LARGE1+15]),
	S_BRIGHT (FFLG, 'N',	4, NULL					    , &States[S_FLAME_LARGE1+16]),
	S_BRIGHT (FFLG, 'O',	4, NULL					    , &States[S_FLAME_LARGE1+17]),
	S_BRIGHT (FFLG, 'P',	4, NULL					    , &States[S_FLAME_LARGE1+6]),

#define S_FLAME_LDORM1 (S_FLAME_LARGE1+18)
	S_NORMAL (FFLG, 'D',	2, NULL					    , &States[S_FLAME_LDORM1+1]),
	S_NORMAL (FFLG, 'C',	2, NULL					    , &States[S_FLAME_LDORM1+2]),
	S_NORMAL (FFLG, 'B',	2, NULL					    , &States[S_FLAME_LDORM1+3]),
	S_NORMAL (FFLG, 'A',	2, A_HideThing			    , &States[S_FLAME_LDORM1+4]),
	S_NORMAL (FFLG, 'A',  200, NULL					    , &States[S_FLAME_LDORM1+4]),
};

IMPLEMENT_ACTOR (AFlameLarge, Hexen, 10503, 99)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderFlags (RF_INVISIBLE)
	PROP_RenderStyle (STYLE_Add)
	PROP_SpawnState (S_FLAME_LARGE1)
END_DEFAULTS

void AFlameLarge::Activate (AActor *activator)
{
	Super::Activate (activator);
	S_Sound (this, CHAN_BODY, "Ignite", 1, ATTN_NORM);
	SetState (&States[S_FLAME_LARGE1]);
}

void AFlameLarge::Deactivate (AActor *activator)
{
	Super::Deactivate (activator);
	SetState (&States[S_FLAME_LDORM1]);
}

//===========================================================================
//
// A_FlameCheck
//
//===========================================================================

void A_FlameCheck (AActor *actor)
{
	if (!actor->args[0]--)		// Called every 8 tics
	{
		actor->Destroy ();
	}
}



//===========================================================================
//
// Hexen uses 2 different spawn IDs for these actors
//
//===========================================================================

class AFlameSmall2 : public AFlameSmall
{
	DECLARE_ACTOR (AFlameSmall2, AFlameSmall)
};

IMPLEMENT_STATELESS_ACTOR (AFlameSmall2, Hexen, -1, 66)
END_DEFAULTS

class AFlameLarge2 : public AFlameLarge
{
	DECLARE_ACTOR (AFlameLarge2, AFlameLarge)
};

IMPLEMENT_STATELESS_ACTOR (AFlameLarge2, Hexen, -1, 67)
END_DEFAULTS
