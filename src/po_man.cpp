
//**************************************************************************
//**
//** PO_MAN.C : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: po_man.c,v $
//** $Revision: 1.22 $
//** $Date: 95/09/28 18:20:56 $
//** $Author: cjr $
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"
#include "r_local.h"
#include "i_system.h"
#include "w_wad.h"
#include "m_swap.h"
#include "m_bbox.h"
#include "tables.h"
#include "s_sndseq.h"
#include "a_sharedglobal.h"
#include "r_main.h"
#include "p_lnspec.h"
#include "r_interpolate.h"
#include "g_level.h"

// MACROS ------------------------------------------------------------------

#define PO_MAXPOLYSEGS 64

// TYPES -------------------------------------------------------------------

inline vertex_t *side_t::V1() const
{
	return this == linedef->sidedef[0]? linedef->v1 : linedef->v2;
}

inline vertex_t *side_t::V2() const
{
	return this == linedef->sidedef[0]? linedef->v2 : linedef->v1;
}


inline FArchive &operator<< (FArchive &arc, podoortype_t &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (podoortype_t)val;
	return arc;
}

class DPolyAction : public DThinker
{
	DECLARE_CLASS (DPolyAction, DThinker)
	HAS_OBJECT_POINTERS
public:
	DPolyAction (int polyNum);
	void Serialize (FArchive &arc);
	void Destroy();

	void StopInterpolation ();
protected:
	DPolyAction ();
	int m_PolyObj;
	int m_Speed;
	int m_Dist;
	TObjPtr<DInterpolation> m_Interpolation;

	void SetInterpolation ();

	friend void ThrustMobj (AActor *actor, side_t *side, FPolyObj *po);
};

class DRotatePoly : public DPolyAction
{
	DECLARE_CLASS (DRotatePoly, DPolyAction)
public:
	DRotatePoly (int polyNum);
	void Tick ();
private:
	DRotatePoly ();

	friend bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle, int direction, bool overRide);
};


class DMovePoly : public DPolyAction
{
	DECLARE_CLASS (DMovePoly, DPolyAction)
public:
	DMovePoly (int polyNum);
	void Serialize (FArchive &arc);
	void Tick ();
protected:
	DMovePoly ();
	int m_Angle;
	fixed_t m_xSpeed; // for sliding walls
	fixed_t m_ySpeed;

	friend bool EV_MovePoly (line_t *line, int polyNum, int speed, angle_t angle, fixed_t dist, bool overRide);
};


class DPolyDoor : public DMovePoly
{
	DECLARE_CLASS (DPolyDoor, DMovePoly)
public:
	DPolyDoor (int polyNum, podoortype_t type);
	void Serialize (FArchive &arc);
	void Tick ();
protected:
	int m_Direction;
	int m_TotalDist;
	int m_Tics;
	int m_WaitTics;
	podoortype_t m_Type;
	bool m_Close;

	friend bool EV_OpenPolyDoor (line_t *line, int polyNum, int speed, angle_t angle, int delay, int distance, podoortype_t type);
private:
	DPolyDoor ();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

bool PO_RotatePolyobj (int num, angle_t angle);
void PO_Init (void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int GetPolyobjMirror (int poly);
static void UpdateSegBBox (seg_t *seg);
static void RotatePt (int an, fixed_t *x, fixed_t *y, fixed_t startSpotX,
	fixed_t startSpotY);
static void UnLinkPolyobj (FPolyObj *po);
static void LinkPolyobj (FPolyObj *po);
static bool CheckMobjBlocking (side_t *seg, FPolyObj *po);
static void InitBlockMap (void);
static void IterFindPolySegs (vertex_t *v1, vertex_t *v2, seg_t **segList);
static void SpawnPolyobj (int index, int tag, int type);
static void TranslateToStartSpot (int tag, int originX, int originY);
static void DoMovePolyobj (FPolyObj *po, int x, int y);
static void InitSegLists ();
static void KillSegLists ();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern seg_t *segs;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

polyblock_t **PolyBlockMap;
FPolyObj *polyobjs; // list of all poly-objects on the level
int po_NumPolyobjs;
polyspawns_t *polyspawns; // [RH] Let P_SpawnMapThings() find our thingies for us

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static SDWORD *SideListHead;	// contains numvertexes elements
static TArray<SDWORD> KnownPolySides;

// CODE --------------------------------------------------------------------

IMPLEMENT_POINTY_CLASS (DPolyAction)
	DECLARE_POINTER(m_Interpolation)
END_POINTERS

IMPLEMENT_CLASS (DRotatePoly)
IMPLEMENT_CLASS (DMovePoly)
IMPLEMENT_CLASS (DPolyDoor)

DPolyAction::DPolyAction ()
{
}

void DPolyAction::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_PolyObj << m_Speed << m_Dist << m_Interpolation;
}

DPolyAction::DPolyAction (int polyNum)
{
	m_PolyObj = polyNum;
	m_Speed = 0;
	m_Dist = 0;
	SetInterpolation ();
}

void DPolyAction::Destroy()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly->specialdata == NULL || poly->specialdata == this)
	{
		poly->specialdata = NULL;
	}

	StopInterpolation();
	Super::Destroy();
}

void DPolyAction::SetInterpolation ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	m_Interpolation = poly->SetInterpolation();
}

void DPolyAction::StopInterpolation ()
{
	if (m_Interpolation != NULL)
	{
		m_Interpolation->DelRef();
		m_Interpolation = NULL;
	}
}

DRotatePoly::DRotatePoly ()
{
}

DRotatePoly::DRotatePoly (int polyNum)
	: Super (polyNum)
{
}

DMovePoly::DMovePoly ()
{
}

void DMovePoly::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Angle << m_xSpeed << m_ySpeed;
}

DMovePoly::DMovePoly (int polyNum)
	: Super (polyNum)
{
	m_Angle = 0;
	m_xSpeed = 0;
	m_ySpeed = 0;
}

DPolyDoor::DPolyDoor ()
{
}

void DPolyDoor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Direction << m_TotalDist << m_Tics << m_WaitTics << m_Type << m_Close;
}

DPolyDoor::DPolyDoor (int polyNum, podoortype_t type)
	: Super (polyNum), m_Type (type)
{
	m_Direction = 0;
	m_TotalDist = 0;
	m_Tics = 0;
	m_WaitTics = 0;
	m_Close = false;
}

// ===== Polyobj Event Code =====

//==========================================================================
//
// T_RotatePoly
//
//==========================================================================

void DRotatePoly::Tick ()
{
	if (PO_RotatePolyobj (m_PolyObj, m_Speed))
	{
		unsigned int absSpeed = abs (m_Speed);

		if (m_Dist == -1)
		{ // perpetual polyobj
			return;
		}
		m_Dist -= absSpeed;
		if (m_Dist == 0)
		{
			FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
			if (poly->specialdata == this)
			{
				poly->specialdata = NULL;
			}
			SN_StopSequence (poly);
			Destroy ();
		}
		else if ((unsigned int)m_Dist < absSpeed)
		{
			m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
		}
	}
}

//==========================================================================
//
// EV_RotatePoly
//
//==========================================================================


bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle,
					int direction, bool overRide)
{
	int mirror;
	DRotatePoly *pe;
	FPolyObj *poly;

	if ( (poly = PO_GetPolyobj(polyNum)) )
	{
		if (poly->specialdata && !overRide)
		{ // poly is already moving
			return false;
		}
	}
	else
	{
		I_Error("EV_RotatePoly: Invalid polyobj num: %d\n", polyNum);
	}
	pe = new DRotatePoly (polyNum);
	if (byteAngle)
	{
		if (byteAngle == 255)
		{
			pe->m_Dist = ~0;
		}
		else
		{
			pe->m_Dist = byteAngle*(ANGLE_90/64); // Angle
		}
	}
	else
	{
		pe->m_Dist = ANGLE_MAX-1;
	}
	pe->m_Speed = (speed*direction*(ANGLE_90/64))>>3;
	poly->specialdata = pe;
	SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
	
	while ( (mirror = GetPolyobjMirror(polyNum)) )
	{
		poly = PO_GetPolyobj(mirror);
		if (poly == NULL)
		{
			I_Error ("EV_RotatePoly: Invalid polyobj num: %d\n", polyNum);
		}
		if (poly && poly->specialdata && !overRide)
		{ // mirroring poly is already in motion
			break;
		}
		pe = new DRotatePoly (mirror);
		poly->specialdata = pe;
		if (byteAngle)
		{
			if (byteAngle == 255)
			{
				pe->m_Dist = ~0;
			}
			else
			{
				pe->m_Dist = byteAngle*(ANGLE_90/64); // Angle
			}
		}
		else
		{
			pe->m_Dist = ANGLE_MAX-1;
		}
		direction = -direction;
		pe->m_Speed = (speed*direction*(ANGLE_90/64))>>3;
		polyNum = mirror;
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
	}
	return true;
}

//==========================================================================
//
// T_MovePoly
//
//==========================================================================

void DMovePoly::Tick ()
{
	FPolyObj *poly;

	if (PO_MovePolyobj (m_PolyObj, m_xSpeed, m_ySpeed))
	{
		int absSpeed = abs (m_Speed);
		m_Dist -= absSpeed;
		if (m_Dist <= 0)
		{
			poly = PO_GetPolyobj (m_PolyObj);
			if (poly->specialdata == this)
			{
				poly->specialdata = NULL;
			}
			SN_StopSequence (poly);
			Destroy ();
		}
		else if (m_Dist < absSpeed)
		{
			m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
			m_xSpeed = FixedMul (m_Speed, finecosine[m_Angle]);
			m_ySpeed = FixedMul (m_Speed, finesine[m_Angle]);
		}
	}
}

//==========================================================================
//
// EV_MovePoly
//
//==========================================================================

bool EV_MovePoly (line_t *line, int polyNum, int speed, angle_t angle,
				  fixed_t dist, bool overRide)
{
	int mirror;
	DMovePoly *pe;
	FPolyObj *poly;
	angle_t an;

	if ( (poly = PO_GetPolyobj(polyNum)) )
	{
		if (poly->specialdata && !overRide)
		{ // poly is already moving
			return false;
		}
	}
	else
	{
		I_Error("EV_MovePoly: Invalid polyobj num: %d\n", polyNum);
	}
	pe = new DMovePoly (polyNum);
	pe->m_Dist = dist; // Distance
	pe->m_Speed = speed;
	poly->specialdata = pe;

	an = angle;

	pe->m_Angle = an>>ANGLETOFINESHIFT;
	pe->m_xSpeed = FixedMul (pe->m_Speed, finecosine[pe->m_Angle]);
	pe->m_ySpeed = FixedMul (pe->m_Speed, finesine[pe->m_Angle]);
	SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

	// Do not interpolate very fast moving polyobjects. The minimum tic count is
	// 3 instead of 2, because the moving crate effect in Massmouth 2, Hostitality
	// that this fixes isn't quite fast enough to move the crate back to its start
	// in just 1 tic.
	if (dist/speed <= 2)
	{
		pe->StopInterpolation ();
	}

	while ( (mirror = GetPolyobjMirror(polyNum)) )
	{
		poly = PO_GetPolyobj(mirror);
		if (poly && poly->specialdata && !overRide)
		{ // mirroring poly is already in motion
			break;
		}
		pe = new DMovePoly (mirror);
		poly->specialdata = pe;
		pe->m_Dist = dist; // Distance
		pe->m_Speed = speed;
		an = an+ANGLE_180; // reverse the angle
		pe->m_Angle = an>>ANGLETOFINESHIFT;
		pe->m_xSpeed = FixedMul (pe->m_Speed, finecosine[pe->m_Angle]);
		pe->m_ySpeed = FixedMul (pe->m_Speed, finesine[pe->m_Angle]);
		polyNum = mirror;
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
		if (dist/speed <= 2)
		{
			pe->StopInterpolation ();
		}
	}
	return true;
}

//==========================================================================
//
// T_PolyDoor
//
//==========================================================================

void DPolyDoor::Tick ()
{
	int absSpeed;
	FPolyObj *poly;

	if (m_Tics)
	{
		if (!--m_Tics)
		{
			poly = PO_GetPolyobj (m_PolyObj);
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, m_Close);
		}
		return;
	}
	switch (m_Type)
	{
	case PODOOR_SLIDE:
		if (m_Dist <= 0 || PO_MovePolyobj (m_PolyObj, m_xSpeed, m_ySpeed))
		{
			absSpeed = abs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				poly = PO_GetPolyobj (m_PolyObj);
				SN_StopSequence (poly);
				if (!m_Close)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Direction = (ANGLE_MAX>>ANGLETOFINESHIFT)-
						m_Direction;
					m_xSpeed = -m_xSpeed;
					m_ySpeed = -m_ySpeed;					
				}
				else
				{
					if (poly->specialdata == this)
					{
						poly->specialdata = NULL;
					}
					Destroy ();
				}
			}
		}
		else
		{
			poly = PO_GetPolyobj (m_PolyObj);
			if (poly->crush || !m_Close)
			{ // continue moving if the poly is a crusher, or is opening
				return;
			}
			else
			{ // open back up
				m_Dist = m_TotalDist - m_Dist;
				m_Direction = (ANGLE_MAX>>ANGLETOFINESHIFT)-
					m_Direction;
				m_xSpeed = -m_xSpeed;
				m_ySpeed = -m_ySpeed;
				m_Close = false;
				SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
			}
		}
		break;

	case PODOOR_SWING:
		if (PO_RotatePolyobj (m_PolyObj, m_Speed))
		{
			absSpeed = abs (m_Speed);
			if (m_Dist == -1)
			{ // perpetual polyobj
				return;
			}
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				poly = PO_GetPolyobj (m_PolyObj);
				SN_StopSequence (poly);
				if (!m_Close)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Speed = -m_Speed;
				}
				else
				{
					if (poly->specialdata == this)
					{
						poly->specialdata = NULL;
					}
					Destroy ();
				}
			}
		}
		else
		{
			poly = PO_GetPolyobj (m_PolyObj);
			if(poly->crush || !m_Close)
			{ // continue moving if the poly is a crusher, or is opening
				return;
			}
			else
			{ // open back up and rewait
				m_Dist = m_TotalDist - m_Dist;
				m_Speed = -m_Speed;
				m_Close = false;
				SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
			}
		}			
		break;

	default:
		break;
	}
}

//==========================================================================
//
// EV_OpenPolyDoor
//
//==========================================================================

bool EV_OpenPolyDoor (line_t *line, int polyNum, int speed, angle_t angle,
					  int delay, int distance, podoortype_t type)
{
	int mirror;
	DPolyDoor *pd;
	FPolyObj *poly;

	if( (poly = PO_GetPolyobj(polyNum)) )
	{
		if (poly->specialdata)
		{ // poly is already moving
			return false;
		}
	}
	else
	{
		I_Error("EV_OpenPolyDoor: Invalid polyobj num: %d\n", polyNum);
	}
	pd = new DPolyDoor (polyNum, type);
	if (type == PODOOR_SLIDE)
	{
		pd->m_WaitTics = delay;
		pd->m_Speed = speed;
		pd->m_Dist = pd->m_TotalDist = distance; // Distance
		pd->m_Direction = angle >> ANGLETOFINESHIFT;
		pd->m_xSpeed = FixedMul (pd->m_Speed, finecosine[pd->m_Direction]);
		pd->m_ySpeed = FixedMul (pd->m_Speed, finesine[pd->m_Direction]);
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
	}
	else if (type == PODOOR_SWING)
	{
		pd->m_WaitTics = delay;
		pd->m_Direction = 1; // ADD:  PODOOR_SWINGL, PODOOR_SWINGR
		pd->m_Speed = (speed*pd->m_Direction*(ANGLE_90/64))>>3;
		pd->m_Dist = pd->m_TotalDist = angle;
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
	}

	poly->specialdata = pd;

	while ( (mirror = GetPolyobjMirror (polyNum)) )
	{
		poly = PO_GetPolyobj (mirror);
		if (poly && poly->specialdata)
		{ // mirroring poly is already in motion
			break;
		}
		pd = new DPolyDoor (mirror, type);
		poly->specialdata = pd;
		if (type == PODOOR_SLIDE)
		{
			pd->m_WaitTics = delay;
			pd->m_Speed = speed;
			pd->m_Dist = pd->m_TotalDist = distance; // Distance
			pd->m_Direction = (angle + ANGLE_180) >> ANGLETOFINESHIFT; // reverse the angle
			pd->m_xSpeed = FixedMul (pd->m_Speed, finecosine[pd->m_Direction]);
			pd->m_ySpeed = FixedMul (pd->m_Speed, finesine[pd->m_Direction]);
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
		}
		else if (type == PODOOR_SWING)
		{
			pd->m_WaitTics = delay;
			pd->m_Direction = -1; // ADD:  same as above
			pd->m_Speed = (speed*pd->m_Direction*(ANGLE_90/64))>>3;
			pd->m_Dist = pd->m_TotalDist = angle;
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
		}
		polyNum = mirror;
	}
	return true;
}
	
// ===== Higher Level Poly Interface code =====

//==========================================================================
//
// PO_GetPolyobj
//
//==========================================================================

FPolyObj *PO_GetPolyobj (int polyNum)
{
	int i;

	for (i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].tag == polyNum)
		{
			return &polyobjs[i];
		}
	}
	return NULL;
}

//==========================================================================
//
// GetPolyobjMirror
//
//==========================================================================

static int GetPolyobjMirror(int poly)
{
	int i;

	for (i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].tag == poly)
		{
			return polyobjs[i].Linedefs[0]->args[1];
		}
	}
	return 0;
}

//==========================================================================
//
// ThrustMobj
//
//==========================================================================

void ThrustMobj (AActor *actor, side_t *side, FPolyObj *po)
{
	int thrustAngle;
	int thrustX;
	int thrustY;
	DPolyAction *pe;

	int force;

	if (!(actor->flags&MF_SHOOTABLE) && !actor->player)
	{
		return;
	}
	vertex_t *v1 = side->V1();
	vertex_t *v2 = side->V2();
	thrustAngle = (R_PointToAngle2 (v1->x, v1->y, v2->x, v2->y) - ANGLE_90) >> ANGLETOFINESHIFT;

	pe = static_cast<DPolyAction *>(po->specialdata);
	if (pe)
	{
		if (pe->IsKindOf (RUNTIME_CLASS (DRotatePoly)))
		{
			force = pe->m_Speed >> 8;
		}
		else
		{
			force = pe->m_Speed >> 3;
		}
		if (force < FRACUNIT)
		{
			force = FRACUNIT;
		}
		else if (force > 4*FRACUNIT)
		{
			force = 4*FRACUNIT;
		}
	}
	else
	{
		force = FRACUNIT;
	}

	thrustX = FixedMul (force, finecosine[thrustAngle]);
	thrustY = FixedMul (force, finesine[thrustAngle]);
	actor->velx += thrustX;
	actor->vely += thrustY;
	if (po->crush)
	{
		if (po->bHurtOnTouch || !P_CheckMove (actor, actor->x + thrustX, actor->y + thrustY))
		{
			P_DamageMobj (actor, NULL, NULL, po->crush, NAME_Crush);
			P_TraceBleed (po->crush, actor);
		}
	}
	if (level.flags2 & LEVEL2_POLYGRIND) actor->Grind(false); // crush corpses that get caught in a polyobject's way
}

//==========================================================================
//
// UpdateSegBBox
//
//==========================================================================

static void UpdateBBox (FPolyObj *po)
{
	for(unsigned i=0;i<po->Linedefs.Size(); i++)
	{
		line_t *line = po->Linedefs[i];

		if (line->v1->x < line->v2->x)
		{
			line->bbox[BOXLEFT] = line->v1->x;
			line->bbox[BOXRIGHT] = line->v2->x;
		}
		else
		{
			line->bbox[BOXLEFT] = line->v2->x;
			line->bbox[BOXRIGHT] = line->v1->x;
		}
		if (line->v1->y < line->v2->y)
		{
			line->bbox[BOXBOTTOM] = line->v1->y;
			line->bbox[BOXTOP] = line->v2->y;
		}
		else
		{
			line->bbox[BOXBOTTOM] = line->v2->y;
			line->bbox[BOXTOP] = line->v1->y;
		}

		// Update the line's slopetype
		line->dx = line->v2->x - line->v1->x;
		line->dy = line->v2->y - line->v1->y;
		if (!line->dx)
		{
			line->slopetype = ST_VERTICAL;
		}
		else if (!line->dy)
		{
			line->slopetype = ST_HORIZONTAL;
		}
		else
		{
			line->slopetype = ((line->dy ^ line->dx) >= 0) ? ST_POSITIVE : ST_NEGATIVE;
		}
	}
}

//==========================================================================
//
// PO_MovePolyobj
//
//==========================================================================

bool PO_MovePolyobj (int num, int x, int y, bool force)
{
	FPolyObj *po;

	if (!(po = PO_GetPolyobj (num)))
	{
		I_Error ("PO_MovePolyobj: Invalid polyobj number: %d\n", num);
	}

	UnLinkPolyobj (po);
	DoMovePolyobj (po, x, y);

	if (!force)
	{
		bool blocked = false;

		for(unsigned i=0;i < po->Sidedefs.Size(); i++)
		{
			if (CheckMobjBlocking(po->Sidedefs[i], po))
			{
				blocked = true;
			}
		}
		if (blocked)
		{
			DoMovePolyobj (po, -x, -y);
			LinkPolyobj(po);
			return false;
		}
	}
	po->StartSpot.x += x;
	po->StartSpot.y += y;
	LinkPolyobj (po);
	return true;
}

//==========================================================================
//
// DoMovePolyobj
//
//==========================================================================

void DoMovePolyobj (FPolyObj *po, int x, int y)
{
	for(unsigned i=0;i < po->Vertices.Size(); i++)
	{
		po->Vertices[i]->x += x;
		po->Vertices[i]->y += y;
		po->PrevPts[i].x += x;
		po->PrevPts[i].y += y;
	}
}

//==========================================================================
//
// RotatePt
//
//==========================================================================

static void RotatePt (int an, fixed_t *x, fixed_t *y, fixed_t startSpotX, fixed_t startSpotY)
{
	fixed_t tr_x = *x;
	fixed_t tr_y = *y;

	*x = DMulScale16 (tr_x, finecosine[an], -tr_y, finesine[an])+startSpotX;
	*y = DMulScale16 (tr_x, finesine[an], tr_y, finecosine[an])+startSpotY;
}

//==========================================================================
//
// PO_RotatePolyobj
//
//==========================================================================

bool PO_RotatePolyobj (int num, angle_t angle)
{
	int an;
	FPolyObj *po;
	bool blocked;

	if(!(po = PO_GetPolyobj(num)))
	{
		I_Error("PO_RotatePolyobj: Invalid polyobj number: %d\n", num);
	}
	an = (po->angle+angle)>>ANGLETOFINESHIFT;

	UnLinkPolyobj(po);

	for(unsigned i=0;i < po->Vertices.Size(); i++)
	{
		po->PrevPts[i].x = po->Vertices[i]->x;
		po->PrevPts[i].y = po->Vertices[i]->y;
		po->Vertices[i]->x = po->OriginalPts[i].x;
		po->Vertices[i]->y = po->OriginalPts[i].y;
		RotatePt(an, &po->Vertices[i]->x, &po->Vertices[i]->y, po->StartSpot.x,	po->StartSpot.y);
	}
	blocked = false;
	validcount++;
	UpdateBBox(po);

	for(unsigned i=0;i < po->Sidedefs.Size(); i++)
	{
		if (CheckMobjBlocking(po->Sidedefs[i], po))
		{
			blocked = true;
		}
	}
	if (blocked)
	{
		for(unsigned i=0;i < po->Vertices.Size(); i++)
		{
			po->Vertices[i]->x = po->PrevPts[i].x;
			po->Vertices[i]->y = po->PrevPts[i].y;
		}
		UpdateBBox(po);
		LinkPolyobj(po);
		return false;
	}
	po->angle += angle;
	LinkPolyobj(po);
	return true;
}

//==========================================================================
//
// UnLinkPolyobj
//
//==========================================================================

static void UnLinkPolyobj (FPolyObj *po)
{
	polyblock_t *link;
	int i, j;
	int index;

	// remove the polyobj from each blockmap section
	for(j = po->bbox[BOXBOTTOM]; j <= po->bbox[BOXTOP]; j++)
	{
		index = j*bmapwidth;
		for(i = po->bbox[BOXLEFT]; i <= po->bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight)
			{
				link = PolyBlockMap[index+i];
				while(link != NULL && link->polyobj != po)
				{
					link = link->next;
				}
				if(link == NULL)
				{ // polyobj not located in the link cell
					continue;
				}
				link->polyobj = NULL;
			}
		}
	}
}

//==========================================================================
//
// CheckMobjBlocking
//
//==========================================================================

static bool CheckMobjBlocking (side_t *sd, FPolyObj *po)
{
	static TArray<AActor *> checker;
	FBlockNode *block;
	AActor *mobj;
	int i, j, k;
	int left, right, top, bottom;
	line_t *ld;
	bool blocked;

	ld = sd->linedef;
	top = (ld->bbox[BOXTOP]-bmaporgy) >> MAPBLOCKSHIFT;
	bottom = (ld->bbox[BOXBOTTOM]-bmaporgy) >> MAPBLOCKSHIFT;
	left = (ld->bbox[BOXLEFT]-bmaporgx) >> MAPBLOCKSHIFT;
	right = (ld->bbox[BOXRIGHT]-bmaporgx) >> MAPBLOCKSHIFT;

	blocked = false;
	checker.Clear();

	bottom = bottom < 0 ? 0 : bottom;
	bottom = bottom >= bmapheight ? bmapheight-1 : bottom;
	top = top < 0 ? 0 : top;
	top = top >= bmapheight  ? bmapheight-1 : top;
	left = left < 0 ? 0 : left;
	left = left >= bmapwidth ? bmapwidth-1 : left;
	right = right < 0 ? 0 : right;
	right = right >= bmapwidth ?  bmapwidth-1 : right;

	for (j = bottom*bmapwidth; j <= top*bmapwidth; j += bmapwidth)
	{
		for (i = left; i <= right; i++)
		{
			for (block = blocklinks[j+i]; block != NULL; block = block->NextActor)
			{
				mobj = block->Me;
				for (k = (int)checker.Size()-1; k >= 0; --k)
				{
					if (checker[k] == mobj)
					{
						break;
					}
				}
				if (k < 0)
				{
					checker.Push (mobj);
					if ((mobj->flags&MF_SOLID) && !(mobj->flags&MF_NOCLIP))
					{
						FBoundingBox box(mobj->x, mobj->y, mobj->radius);

						if (box.Right() <= ld->bbox[BOXLEFT]
							|| box.Left() >= ld->bbox[BOXRIGHT]
							|| box.Top() <= ld->bbox[BOXBOTTOM]
							|| box.Bottom() >= ld->bbox[BOXTOP])
						{
							continue;
						}
						if (box.BoxOnLineSide(ld) != -1)
						{
							continue;
						}
						ThrustMobj (mobj, sd, po);
						blocked = true;
					}
				}
			}
		}
	}
	return blocked;
}

//==========================================================================
//
// LinkPolyobj
//
//==========================================================================

static void LinkPolyobj (FPolyObj *po)
{
	int leftX, rightX;
	int topY, bottomY;
	polyblock_t **link;
	polyblock_t *tempLink;

	// calculate the polyobj bbox
	vertex_t *vt = po->Sidedefs[0]->V1();
	rightX = leftX = vt->x;
	topY = bottomY = vt->y;

	for(unsigned i = 1; i < po->Sidedefs.Size(); i++)
	{
		vt = po->Sidedefs[i]->V1();
		if(vt->x > rightX)
		{
			rightX = vt->x;
		}
		if(vt->x < leftX)
		{
			leftX = vt->x;
		}
		if(vt->y > topY)
		{
			topY = vt->y;
		}
		if(vt->y < bottomY)
		{
			bottomY = vt->y;
		}
	}
	po->bbox[BOXRIGHT] = (rightX-bmaporgx)>>MAPBLOCKSHIFT;
	po->bbox[BOXLEFT] = (leftX-bmaporgx)>>MAPBLOCKSHIFT;
	po->bbox[BOXTOP] = (topY-bmaporgy)>>MAPBLOCKSHIFT;
	po->bbox[BOXBOTTOM] = (bottomY-bmaporgy)>>MAPBLOCKSHIFT;
	// add the polyobj to each blockmap section
	for(int j = po->bbox[BOXBOTTOM]*bmapwidth; j <= po->bbox[BOXTOP]*bmapwidth;
		j += bmapwidth)
	{
		for(int i = po->bbox[BOXLEFT]; i <= po->bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight*bmapwidth)
			{
				link = &PolyBlockMap[j+i];
				if(!(*link))
				{ // Create a new link at the current block cell
					*link = new polyblock_t;
					(*link)->next = NULL;
					(*link)->prev = NULL;
					(*link)->polyobj = po;
					continue;
				}
				else
				{
					tempLink = *link;
					while(tempLink->next != NULL && tempLink->polyobj != NULL)
					{
						tempLink = tempLink->next;
					}
				}
				if(tempLink->polyobj == NULL)
				{
					tempLink->polyobj = po;
					continue;
				}
				else
				{
					tempLink->next = new polyblock_t;
					tempLink->next->next = NULL;
					tempLink->next->prev = tempLink;
					tempLink->next->polyobj = po;
				}
			}
			// else, don't link the polyobj, since it's off the map
		}
	}
}

//==========================================================================
//
// InitBlockMap
//
//==========================================================================

static void InitBlockMap (void)
{
	int i;

	PolyBlockMap = new polyblock_t *[bmapwidth*bmapheight];
	memset (PolyBlockMap, 0, bmapwidth*bmapheight*sizeof(polyblock_t *));

	for (i = 0; i < po_NumPolyobjs; i++)
	{
		LinkPolyobj(&polyobjs[i]);
	}
}

//==========================================================================
//
// InitSegLists [RH]
//
// Group segs by vertex and collect segs that are known to belong to a
// polyobject so that they can be initialized fast.
//==========================================================================

static void InitSideLists ()
{
	SDWORD i;

	SideListHead = new SDWORD[numvertexes];
	clearbuf (SideListHead, numvertexes, -1);

	for (i = 0; i < numsides; ++i)
	{
		if (segs[i].linedef != NULL)
		{
			SideListHead[sides[i].V1() - vertexes] = i;
			if ((sides[i].linedef->special == Polyobj_StartLine ||
				 sides[i].linedef->special == Polyobj_ExplicitLine))
			{
				KnownPolySides.Push (i);
			}
		}
	}
}

//==========================================================================
//
// KilSegLists [RH]
//
//==========================================================================

static void KillSideLists ()
{
	delete[] SideListHead;
	SideListHead = NULL;
	KnownPolySides.Clear ();
	KnownPolySides.ShrinkToFit ();
}

//==========================================================================
//
// IterFindPolySegs
//
//==========================================================================

static void IterFindPolySegs (FPolyObj *po, side_t *side)
{
	SDWORD j;
	int i;
	vertex_t *v1 = side->V1();

	// This for loop exists solely to avoid infinitely looping on badly
	// formed polyobjects.
	for (i = 0; i < numsides; i++)
	{
		int v2 = int(side->V2() - vertexes);
		j = SideListHead[v2];

		if (j < 0)
		{
			break;
		}
		side = &sides[j];

		if (side->V1() == v1)
		{
			return;
		}

		if (side->linedef != NULL)
		{
			po->Sidedefs.Push(side);
		}
	}
	I_Error ("IterFindPolySegs: Non-closed Polyobj at linedef %d.\n",
		side->linedef-lines);
}


//==========================================================================
//
// SpawnPolyobj
//
//==========================================================================

static void SpawnPolyobj (int index, int tag, int type)
{
	unsigned int ii;
	int i;
	int j;
	FPolyObj *po = &polyobjs[index];

	for (ii = 0; ii < KnownPolySides.Size(); ++ii)
	{
		i = KnownPolySides[ii];
		if (i < 0)
		{
			continue;
		}

		side_t *sd = &sides[i];
		
		if (sd->linedef->special == Polyobj_StartLine &&
			sd->linedef->args[0] == tag)
		{
			if (po->Sidedefs.Size() > 0)
			{
				I_Error ("SpawnPolyobj: Polyobj %d already spawned.\n", tag);
			}
			sd->linedef->special = 0;
			sd->linedef->args[0] = 0;
			IterFindPolySegs(&polyobjs[index], sd);
			po->crush = (type != PO_SPAWN_TYPE) ? 3 : 0;
			po->bHurtOnTouch = (type == PO_SPAWNHURT_TYPE);
			po->tag = tag;
			po->seqType = sd->linedef->args[2];
			if (po->seqType < 0 || po->seqType > 63)
			{
				po->seqType = 0;
			}
			break;
		}
	}
	if (po->Sidedefs.Size() == 0)
	{ 
		// didn't find a polyobj through PO_LINE_START
		TArray<side_t *> polySideList;
		unsigned int psIndexOld;
		for (j = 1; j < PO_MAXPOLYSEGS; j++)
		{
			psIndexOld = po->Sidedefs.Size();
			for (ii = 0; ii < KnownPolySides.Size(); ++ii)
			{
				i = KnownPolySides[ii];

				if (i >= 0 &&
					sides[i].linedef->special == Polyobj_ExplicitLine &&
					sides[i].linedef->args[0] == tag)
				{
					if (!sides[i].linedef->args[1])
					{
						I_Error ("SpawnPolyobj: Explicit line missing order number (probably %d) in poly %d.\n",
							j+1, tag);
					}
					if (sides[i].linedef->args[1] == j)
					{
						po->Sidedefs.Push (&sides[i]);
					}
				}
			}
			// Clear out any specials for these segs...we cannot clear them out
			// 	in the above loop, since we aren't guaranteed one seg per linedef.
			for (ii = 0; ii < KnownPolySides.Size(); ++ii)
			{
				i = KnownPolySides[ii];
				if (i >= 0 &&
					sides[i].linedef->special == Polyobj_ExplicitLine &&
					sides[i].linedef->args[0] == tag && sides[i].linedef->args[1] == j)
				{
					sides[i].linedef->special = 0;
					sides[i].linedef->args[0] = 0;
					KnownPolySides[ii] = -1;
				}
			}
			if (po->Sidedefs.Size() == psIndexOld)
			{ // Check if an explicit line order has been skipped.
			  // A line has been skipped if there are any more explicit
			  // lines with the current tag value. [RH] Can this actually happen?
				for (ii = 0; ii < KnownPolySides.Size(); ++ii)
				{
					i = KnownPolySides[ii];
					if (i >= 0 &&
						sides[i].linedef->special == Polyobj_ExplicitLine &&
						sides[i].linedef->args[0] == tag)
					{
						I_Error ("SpawnPolyobj: Missing explicit line %d for poly %d\n",
							j, tag);
					}
				}
			}
		}
		if (po->Sidedefs.Size() > 0)
		{
			po->crush = (type != PO_SPAWN_TYPE) ? 3 : 0;
			po->bHurtOnTouch = (type == PO_SPAWNHURT_TYPE);
			po->tag = tag;
			po->seqType = po->Sidedefs[0]->linedef->args[3];
			// Next, change the polyobj's first line to point to a mirror
			//		if it exists
			po->Sidedefs[0]->linedef->args[1] =
				po->Sidedefs[0]->linedef->args[2];
		}
		else
			I_Error ("SpawnPolyobj: Poly %d does not exist\n", tag);
	}

	validcount++;	
	for(unsigned int i=0; i<po->Sidedefs.Size(); i++)
	{
		line_t *l = po->Sidedefs[i]->linedef;

		if (l->validcount != validcount)
		{
			l->validcount = validcount;
			po->Linedefs.Push(l);

			vertex_t *v = l->v1;
			int j;
			for(j = po->Vertices.Size() - 1; j >= 0; j--)
			{
				if (po->Vertices[j] == v) break;
			}
			if (j < 0) po->Vertices.Push(v);

			v = l->v2;
			for(j = po->Vertices.Size() - 1; j >= 0; j--)
			{
				if (po->Vertices[j] == v) break;
			}
			if (j < 0) po->Vertices.Push(v);

		}
	}
	po->Sidedefs.ShrinkToFit();
	po->Linedefs.ShrinkToFit();
	po->Vertices.ShrinkToFit();
}

//==========================================================================
//
// TranslateToStartSpot
//
//==========================================================================

static void TranslateToStartSpot (int tag, int originX, int originY)
{
	FPolyObj *po;
	int deltaX;
	int deltaY;

	po = NULL;
	for (int i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].tag == tag)
		{
			po = &polyobjs[i];
			break;
		}
	}
	if (po == NULL)
	{ // didn't match the tag with a polyobj tag
		I_Error("TranslateToStartSpot: Unable to match polyobj tag: %d\n", tag);
	}
	if (po->Sidedefs.Size() == 0)
	{
		I_Error ("TranslateToStartSpot: Anchor point located without a StartSpot point: %d\n", tag);
	}
	po->OriginalPts.Resize(po->Sidedefs.Size());
	po->PrevPts.Resize(po->Sidedefs.Size());
	deltaX = originX - po->StartSpot.x;
	deltaY = originY - po->StartSpot.y;

	for (unsigned i = 0; i < po->Sidedefs.Size(); i++)
	{
		po->Sidedefs[i]->Flags |= WALLF_POLYOBJ;
	}
	for (unsigned i = 0; i < po->Vertices.Size(); i++)
	{
		po->Linedefs[i]->bbox[BOXTOP] -= deltaY;
		po->Linedefs[i]->bbox[BOXBOTTOM] -= deltaY;
		po->Linedefs[i]->bbox[BOXLEFT] -= deltaX;
		po->Linedefs[i]->bbox[BOXRIGHT] -= deltaX;
	}
	for (unsigned i = 0; i < po->Vertices.Size(); i++)
	{
		po->Vertices[i]->x -= deltaX;
		po->Vertices[i]->y -= deltaY;
	}
	// subsector assignment no longer done here.
	// Polyobjects will be sorted into the subsectors each frame before rendering them.
}

//==========================================================================
//
// PO_Init
//
//==========================================================================

void PO_Init (void)
{
	// [RH] Hexen found the polyobject-related things by reloading the map's
	//		THINGS lump here and scanning through it. I have P_SpawnMapThing()
	//		record those things instead, so that in here we simply need to
	//		look at the polyspawns list.
	polyspawns_t *polyspawn, **prev;
	int polyIndex;

	// [RH] Make this faster
	InitSideLists ();

	polyobjs = new FPolyObj[po_NumPolyobjs];

	polyIndex = 0; // index polyobj number
	// Find the startSpot points, and spawn each polyobj
	for (polyspawn = polyspawns, prev = &polyspawns; polyspawn;)
	{
		// 9301 (3001) = no crush, 9302 (3002) = crushing, 9303 = hurting touch
		if (polyspawn->type == PO_SPAWN_TYPE ||
			polyspawn->type == PO_SPAWNCRUSH_TYPE ||
			polyspawn->type == PO_SPAWNHURT_TYPE)
		{ 
			// Polyobj StartSpot Pt.
			polyobjs[polyIndex].StartSpot.x = polyspawn->x;
			polyobjs[polyIndex].StartSpot.y = polyspawn->y;
			SpawnPolyobj(polyIndex, polyspawn->angle, polyspawn->type);
			polyIndex++;
			*prev = polyspawn->next;
			delete polyspawn;
			polyspawn = *prev;
		} 
		else 
		{
			prev = &polyspawn->next;
			polyspawn = polyspawn->next;
		}
	}
	for (polyspawn = polyspawns; polyspawn;)
	{
		polyspawns_t *next = polyspawn->next;
		if (polyspawn->type == PO_ANCHOR_TYPE)
		{ 
			// Polyobj Anchor Pt.
			TranslateToStartSpot (polyspawn->angle, polyspawn->x, polyspawn->y);
		}
		delete polyspawn;
		polyspawn = next;
	}
	polyspawns = NULL;

	// check for a startspot without an anchor point
	for (polyIndex = 0; polyIndex < po_NumPolyobjs; polyIndex++)
	{
		if (polyobjs[polyIndex].OriginalPts.Size() == 0)
		{
			I_Error ("PO_Init: StartSpot located without an Anchor point: %d\n",
				polyobjs[polyIndex].tag);
		}
	}
	InitBlockMap();

	// [RH] Don't need the seg lists anymore
	KillSideLists ();

	// We still need to flag the segs of the polyobj's sidedefs so that they are excluded from rendering.
	for(int i=0;i<numsegs;i++)
	{
		segs[i].bPolySeg = (segs[i].sidedef != NULL && segs[i].sidedef->Flags & WALLF_POLYOBJ);
	}
}

//==========================================================================
//
// PO_Busy
//
//==========================================================================

bool PO_Busy (int polyobj)
{
	FPolyObj *poly;

	poly = PO_GetPolyobj (polyobj);
	return (poly != NULL && poly->specialdata != NULL);
}

//===========================================================================
//
// PO_ClosestPoint
//
// Given a point (x,y), returns the point (ox,oy) on the polyobject's walls
// that is nearest to (x,y). Also returns the seg this point came from.
//
//===========================================================================

void PO_ClosestPoint(const FPolyObj *poly, fixed_t fx, fixed_t fy, fixed_t &ox, fixed_t &oy, side_t **side)
{
	unsigned int i;
	double x = fx, y = fy;
	double bestdist = HUGE_VAL;
	double bestx = 0, besty = 0;
	side_t *bestline = NULL;

	for (i = 0; i < poly->Sidedefs.Size(); ++i)
	{
		vertex_t *v1 = poly->Sidedefs[i]->V1();
		vertex_t *v2 = poly->Sidedefs[i]->V2();
		double a = v2->x - v1->x;
		double b = v2->y - v1->y;
		double den = a*a + b*b;
		double ix, iy, dist;

		if (den == 0)
		{ // Line is actually a point!
			ix = v1->x;
			iy = v1->y;
		}
		else
		{
			double num = (x - v1->x) * a + (y - v1->y) * b;
			double u = num / den;
			if (u <= 0)
			{
				ix = v1->x;
				iy = v1->y;
			}
			else if (u >= 1)
			{
				ix = v2->x;
				iy = v2->y;
			}
			else
			{
				ix = v1->x + u * a;
				iy = v1->y + u * b;
			}
		}
		a = (ix - x);
		b = (iy - y);
		dist = a*a + b*b;
		if (dist < bestdist)
		{
			bestdist = dist;
			bestx = ix;
			besty = iy;
			bestline = poly->Sidedefs[i];
		}
	}
	ox = fixed_t(bestx);
	oy = fixed_t(besty);
	if (side != NULL)
	{
		*side = bestline;
	}
}

FPolyObj::~FPolyObj()
{
}


//=============================================================================
// phares 3/21/98
//
// Maintain a freelist of msecnode_t's to reduce memory allocs and frees.
//=============================================================================

FPolyNode *headpolynode = NULL;

//=============================================================================
//
// P_GetSecnode
//
// Retrieve a node from the freelist. The calling routine
// should make sure it sets all fields properly.
//
//=============================================================================

FPolyNode *PO_GetPolynode()
{
	FPolyNode *node;

	if (headpolynode)
	{
		node = headpolynode;
		headpolynode = headpolynode->pnext;
	}
	else
	{
		node = new FPolyNode;
	}
	return node;
}

//=============================================================================
//
// P_PutSecnode
//
// Returns a node to the freelist.
//
//=============================================================================

void PO_PutPolynode (FPolyNode *node)
{
	node->pnext = headpolynode;
	headpolynode = node;
	node->segs.Clear();
	node->segs.ShrinkToFit();
}

