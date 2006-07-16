#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"
#include "a_keys.h"


// --- Key gizmos -----------------------------------------------------------

void A_InitKeyGizmo (AActor *);

class AKeyGizmo : public AActor
{
	DECLARE_ACTOR (AKeyGizmo, AActor)
public:
	virtual int GetFloatState () { return 0; }
};

FState AKeyGizmo::States[] =
{
	S_NORMAL (KGZ1, 'A',	1, NULL 				, &States[1]),
	S_NORMAL (KGZ1, 'A',	1, A_InitKeyGizmo		, &States[2]),
	S_NORMAL (KGZ1, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AKeyGizmo, Heretic, -1, 0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (50)
	PROP_Flags (MF_SOLID)

	PROP_SpawnState (0)
END_DEFAULTS

class AKeyGizmoFloat : public AActor
{
	DECLARE_ACTOR (AKeyGizmoFloat, AActor)
};

FState AKeyGizmoFloat::States[] =
{
#define S_KGZ_BLUEFLOAT 0
	S_BRIGHT (KGZB, 'A',   -1, NULL 				, NULL),

#define S_KGZ_GREENFLOAT (S_KGZ_BLUEFLOAT+1)
	S_BRIGHT (KGZG, 'A',   -1, NULL 				, NULL),

#define S_KGZ_YELLOWFLOAT (S_KGZ_GREENFLOAT+1)
	S_BRIGHT (KGZY, 'A',   -1, NULL 				, NULL)
};

IMPLEMENT_ACTOR (AKeyGizmoFloat, Heretic, -1, 0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SOLID|MF_NOGRAVITY)

	PROP_SpawnState (0)
END_DEFAULTS

// Blue gizmo ---------------------------------------------------------------

class AKeyGizmoBlue : public AKeyGizmo
{
	DECLARE_STATELESS_ACTOR (AKeyGizmoBlue, AKeyGizmo)
public:
	int GetFloatState () { return S_KGZ_BLUEFLOAT; }
};

IMPLEMENT_STATELESS_ACTOR (AKeyGizmoBlue, Heretic, 94, 0)
END_DEFAULTS

// Green gizmo --------------------------------------------------------------

class AKeyGizmoGreen : public AKeyGizmo
{
	DECLARE_STATELESS_ACTOR (AKeyGizmoGreen, AKeyGizmo)
public:
	int GetFloatState () { return S_KGZ_GREENFLOAT; }
};

IMPLEMENT_STATELESS_ACTOR (AKeyGizmoGreen, Heretic, 95, 0)
END_DEFAULTS

// Yellow gizmo -------------------------------------------------------------

class AKeyGizmoYellow : public AKeyGizmo
{
	DECLARE_STATELESS_ACTOR (AKeyGizmoYellow, AKeyGizmo)
public:
	int GetFloatState () { return S_KGZ_YELLOWFLOAT; }
};

IMPLEMENT_STATELESS_ACTOR (AKeyGizmoYellow, Heretic, 96, 0)
END_DEFAULTS

//----------------------------------------------------------------------------
//
// PROC A_InitKeyGizmo
//
//----------------------------------------------------------------------------

void A_InitKeyGizmo (AActor *gizmo)
{
	AActor *floater;

	floater = Spawn<AKeyGizmoFloat> (gizmo->x, gizmo->y, gizmo->z+60*FRACUNIT, NO_REPLACE);
	floater->SetState (&AKeyGizmoFloat::
		States[static_cast<AKeyGizmo *>(gizmo)->GetFloatState ()]);
}
