#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"

// Green key ------------------------------------------------------------

class AKeyGreen : public AKey
{
	DECLARE_ACTOR (AKeyGreen, AKey)
protected:
	virtual keytype_t GetKeyType ()
	{
		return key_green;
	}
	virtual const char *PickupMessage ()
	{
		return GStrings(TXT_GOTGREENKEY);
	}
};

FState AKeyGreen::States[] =
{
	S_BRIGHT (AKYY, 'A',	3, NULL 				, &States[1]),
	S_BRIGHT (AKYY, 'B',	3, NULL 				, &States[2]),
	S_BRIGHT (AKYY, 'C',	3, NULL 				, &States[3]),
	S_BRIGHT (AKYY, 'D',	3, NULL 				, &States[4]),
	S_BRIGHT (AKYY, 'E',	3, NULL 				, &States[5]),
	S_BRIGHT (AKYY, 'F',	3, NULL 				, &States[6]),
	S_BRIGHT (AKYY, 'G',	3, NULL 				, &States[7]),
	S_BRIGHT (AKYY, 'H',	3, NULL 				, &States[8]),
	S_BRIGHT (AKYY, 'I',	3, NULL 				, &States[9]),
	S_BRIGHT (AKYY, 'J',	3, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AKeyGreen, Heretic, 73, 86)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)
	PROP_SpawnState (0)
END_DEFAULTS

// Blue key -----------------------------------------------------------------

class AKeyBlue : public AKey
{
	DECLARE_ACTOR (AKeyBlue, AKey)
protected:
	virtual keytype_t GetKeyType ()
	{
		return key_blue;
	}
	virtual const char *PickupMessage ()
	{
		return GStrings(TXT_GOTBLUEKEY);
	}
};

FState AKeyBlue::States[] =
{
	S_BRIGHT (BKYY, 'A',	3, NULL 				, &States[1]),
	S_BRIGHT (BKYY, 'B',	3, NULL 				, &States[2]),
	S_BRIGHT (BKYY, 'C',	3, NULL 				, &States[3]),
	S_BRIGHT (BKYY, 'D',	3, NULL 				, &States[4]),
	S_BRIGHT (BKYY, 'E',	3, NULL 				, &States[5]),
	S_BRIGHT (BKYY, 'F',	3, NULL 				, &States[6]),
	S_BRIGHT (BKYY, 'G',	3, NULL 				, &States[7]),
	S_BRIGHT (BKYY, 'H',	3, NULL 				, &States[8]),
	S_BRIGHT (BKYY, 'I',	3, NULL 				, &States[9]),
	S_BRIGHT (BKYY, 'J',	3, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AKeyBlue, Heretic, 79, 85)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)
	PROP_SpawnState (0)
END_DEFAULTS

// Yellow key ---------------------------------------------------------------

class AKeyYellow : public AKey
{
	DECLARE_ACTOR (AKeyYellow, AKey)
protected:
	virtual keytype_t GetKeyType ()
	{
		return key_yellow;
	}
	virtual const char *PickupMessage ()
	{
		return GStrings(TXT_GOTYELLOWKEY);
	}
};

FState AKeyYellow::States[] =
{
	S_BRIGHT (CKYY, 'A',	3, NULL 				, &States[1]),
	S_BRIGHT (CKYY, 'B',	3, NULL 				, &States[2]),
	S_BRIGHT (CKYY, 'C',	3, NULL 				, &States[3]),
	S_BRIGHT (CKYY, 'D',	3, NULL 				, &States[4]),
	S_BRIGHT (CKYY, 'E',	3, NULL 				, &States[5]),
	S_BRIGHT (CKYY, 'F',	3, NULL 				, &States[6]),
	S_BRIGHT (CKYY, 'G',	3, NULL 				, &States[7]),
	S_BRIGHT (CKYY, 'H',	3, NULL 				, &States[8]),
	S_BRIGHT (CKYY, 'I',	3, NULL 				, &States[0])
};

IMPLEMENT_ACTOR (AKeyYellow, Heretic, 80, 87)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)
	PROP_SpawnState (0)
END_DEFAULTS

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

	floater = Spawn<AKeyGizmoFloat> (gizmo->x, gizmo->y, gizmo->z+60*FRACUNIT);
	floater->SetState (&AKeyGizmoFloat::
		States[static_cast<AKeyGizmo *>(gizmo)->GetFloatState ()]);
}
