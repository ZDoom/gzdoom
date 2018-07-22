//-----------------------------------------------------------------------------
//
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"
#include "m_bbox.h"
#include "s_sndseq.h"
#include "a_sharedglobal.h"
#include "p_3dmidtex.h"
#include "p_lnspec.h"
#include "r_data/r_interpolate.h"
#include "po_man.h"
#include "p_setup.h"
#include "serializer.h"
#include "p_blockmap.h"
#include "p_maputl.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "v_text.h"

// MACROS ------------------------------------------------------------------

#define PO_MAXPOLYSEGS 64

// TYPES -------------------------------------------------------------------

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
	void Serialize(FSerializer &arc);
	void Tick ();
protected:
	DMovePoly ();
	DAngle m_Angle;
	DVector2 m_Speedv;

	friend bool EV_MovePoly(line_t *line, int polyNum, double speed, DAngle angle, double dist, bool overRide);
};

class DMovePolyTo : public DPolyAction
{
	DECLARE_CLASS(DMovePolyTo, DPolyAction)
public:
	DMovePolyTo(int polyNum);
	void Serialize(FSerializer &arc);
	void Tick();
protected:
	DMovePolyTo();
	DVector2 m_Speedv;
	DVector2 m_Target;

	friend bool EV_MovePolyTo(line_t *line, int polyNum, double speed, const DVector2 &pos, bool overRide);
};


class DPolyDoor : public DMovePoly
{
	DECLARE_CLASS (DPolyDoor, DMovePoly)
public:
	DPolyDoor (int polyNum, podoortype_t type);
	void Serialize(FSerializer &arc);
	void Tick ();
protected:
	DAngle m_Direction;
	double m_TotalDist;
	int m_Tics;
	int m_WaitTics;
	podoortype_t m_Type;
	bool m_Close;

	friend bool EV_OpenPolyDoor(line_t *line, int polyNum, double speed, DAngle angle, int delay, double distance, podoortype_t type);
private:
	DPolyDoor ();
};

class FPolyMirrorIterator
{
	FPolyObj *CurPoly;
	int UsedPolys[100];	// tracks mirrored polyobjects we've seen
	int NumUsedPolys;

public:
	FPolyMirrorIterator(FPolyObj *poly);
	FPolyObj *NextMirror();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void PO_Init (void);
void P_AdjustLine(line_t *ld);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void UnLinkPolyobj (FPolyObj *po);
static void LinkPolyobj (FPolyObj *po);
static bool CheckMobjBlocking (side_t *seg, FPolyObj *po);
static void InitBlockMap (void);
static void IterFindPolySides (FPolyObj *po, side_t *side);
static void SpawnPolyobj (int index, int tag, int type);
static void TranslateToStartSpot (int tag, const DVector2 &origin);
static void DoMovePolyobj (FPolyObj *po, const DVector2 & move);
static void InitSegLists ();
static void KillSegLists ();
static FPolyNode *NewPolyNode();
static void FreePolyNode();
static void ReleaseAllPolyNodes();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

polyblock_t **PolyBlockMap;
FPolyObj *polyobjs; // list of all poly-objects on the level
int po_NumPolyobjs;
polyspawns_t *polyspawns; // [RH] Let P_SpawnMapThings() find our thingies for us

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TArray<int32_t> KnownPolySides;
static FPolyNode *FreePolyNodes;

// CODE --------------------------------------------------------------------



//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DPolyAction, false, true)

IMPLEMENT_POINTERS_START(DPolyAction)
	IMPLEMENT_POINTER(m_Interpolation)
IMPLEMENT_POINTERS_END

DPolyAction::DPolyAction ()
{
}

void DPolyAction::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("polyobj", m_PolyObj)
		("speed", m_Speed)
		("dist", m_Dist)
		("interpolation", m_Interpolation);
}

DPolyAction::DPolyAction (int polyNum)
{
	m_PolyObj = polyNum;
	m_Speed = 0;
	m_Dist = 0;
	SetInterpolation ();
}

void DPolyAction::OnDestroy()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly->specialdata == this)
	{
		poly->specialdata = NULL;
	}

	StopInterpolation();
	Super::OnDestroy();
}

void DPolyAction::Stop()
{
	FPolyObj *poly = PO_GetPolyobj(m_PolyObj);
	SN_StopSequence(poly);
	Destroy();
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

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DRotatePoly, false, false)

DRotatePoly::DRotatePoly ()
{
}

DRotatePoly::DRotatePoly (int polyNum)
	: Super (polyNum)
{
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DMovePoly, false, false)

DMovePoly::DMovePoly ()
{
}

void DMovePoly::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("angle", m_Angle)
		("speedv", m_Speedv);
}

DMovePoly::DMovePoly (int polyNum)
	: Super (polyNum)
{
	m_Angle = 0.;
	m_Speedv = { 0,0 };
}

//==========================================================================
//
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DMovePolyTo, false, false)

DMovePolyTo::DMovePolyTo()
{
}

void DMovePolyTo::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("speedv", m_Speedv)
		("target", m_Target);
}

DMovePolyTo::DMovePolyTo(int polyNum)
	: Super(polyNum)
{
	m_Speedv = m_Target = { 0,0 };
}

//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DPolyDoor, false, false)

DPolyDoor::DPolyDoor ()
{
}

void DPolyDoor::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc.Enum("type", m_Type)
		("direction", m_Direction)
		("totaldist", m_TotalDist)
		("tics", m_Tics)
		("waittics", m_WaitTics)
		("close", m_Close);
}

DPolyDoor::DPolyDoor (int polyNum, podoortype_t type)
	: Super (polyNum), m_Type (type)
{
	m_Direction = 0.;
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
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);
	if (poly == NULL) return;

	// Don't let non-perpetual polyobjs overshoot their targets.
	if (m_Dist != -1 && m_Dist < fabs(m_Speed))
	{
		m_Speed = m_Speed < 0 ? -m_Dist : m_Dist;
	}

	if (poly->RotatePolyobj (m_Speed))
	{
		if (m_Dist == -1)
		{ // perpetual polyobj
			return;
		}
		m_Dist -= fabs(m_Speed);
		if (m_Dist == 0)
		{
			SN_StopSequence (poly);
			Destroy ();
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
	DRotatePoly *pe = NULL;
	FPolyObj *poly;

	if ((poly = PO_GetPolyobj(polyNum)) == NULL)
	{
		Printf("EV_RotatePoly: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	while ((poly = it.NextMirror()) != NULL)
	{
		if ((poly->specialdata != NULL || poly->bBlocked) && !overRide)
		{ // poly is already in motion
			break;
		}
		if (poly->bHasPortals == 2)
		{
			// cannot do rotations on linked polyportals.
			break;
		}
		pe = Create<DRotatePoly>(poly->tag);
		poly->specialdata = pe;
		poly->bBlocked = false;
		if (byteAngle != 0)
		{
			if (byteAngle == 255)
			{
				pe->m_Dist = -1.;
			}
			else
			{
				pe->m_Dist =  byteAngle*(90./64); // Angle
			}
		}
		else
		{
			pe->m_Dist = 360.;
		}
		pe->m_Speed = speed*direction*(90./(64<<3));
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
		direction = -direction;	// Reverse the direction
	}
	return pe != NULL;	// Return true if something started moving.
}

//==========================================================================
//
// T_MovePoly
//
//==========================================================================

void DMovePoly::Tick ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly != NULL)
	{
		if (poly->MovePolyobj (m_Speedv))
		{
			double absSpeed = fabs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (poly);
				Destroy ();
			}
			else if (m_Dist < absSpeed)
			{
				m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
				m_Speedv = m_Angle.ToVector(m_Speed);
			}
			poly->UpdateLinks();
		}
	}
}

//==========================================================================
//
// EV_MovePoly
//
//==========================================================================

bool EV_MovePoly (line_t *line, int polyNum, double speed, DAngle angle,
				  double dist, bool overRide)
{
	DMovePoly *pe = NULL;
	FPolyObj *poly;
	DAngle an = angle;

	if ((poly = PO_GetPolyobj(polyNum)) == NULL)
	{
		Printf("EV_MovePoly: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	while ((poly = it.NextMirror()) != NULL)
	{
		if ((poly->specialdata != NULL || poly->bBlocked) && !overRide)
		{ // poly is already in motion
			break;
		}
		pe = Create<DMovePoly>(poly->tag);
		poly->specialdata = pe;
		poly->bBlocked = false;
		pe->m_Dist = dist; // Distance
		pe->m_Speed = speed;
		pe->m_Angle = angle;
		pe->m_Speedv = angle.ToVector(speed);
		SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);

		// Do not interpolate very fast moving polyobjects. The minimum tic count is
		// 3 instead of 2, because the moving crate effect in Massmouth 2, Hostitality
		// that this fixes isn't quite fast enough to move the crate back to its start
		// in just 1 tic.
		if (dist/speed <= 2)
		{
			pe->StopInterpolation ();
		}

		angle += 180.;	// Reverse the angle.
	}
	return pe != NULL;	// Return true if something started moving.
}

//==========================================================================
//
// DMovePolyTo :: Tick
//
//==========================================================================

void DMovePolyTo::Tick ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly != NULL)
	{
		if (poly->MovePolyobj (m_Speedv))
		{
			double absSpeed = fabs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (poly);
				Destroy ();
			}
			else if (m_Dist < absSpeed)
			{
				m_Speed = m_Dist * (m_Speed < 0 ? -1 : 1);
				m_Speedv = m_Target - poly->StartSpot.pos;
			}
			poly->UpdateLinks();
		}
	}
}

//==========================================================================
//
// EV_MovePolyTo
//
//==========================================================================

bool EV_MovePolyTo(line_t *line, int polyNum, double speed, const DVector2 &targ, bool overRide)
{
	DMovePolyTo *pe = NULL;
	FPolyObj *poly;
	DVector2 dist;
	double distlen;

	if ((poly = PO_GetPolyobj(polyNum)) == NULL)
	{
		Printf("EV_MovePolyTo: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	dist = targ - poly->StartSpot.pos;
	distlen = dist.MakeUnit();
	while ((poly = it.NextMirror()) != NULL)
	{
		if ((poly->specialdata != NULL || poly->bBlocked) && !overRide)
		{ // poly is already in motion
			break;
		}
		pe = Create<DMovePolyTo>(poly->tag);
		poly->specialdata = pe;
		poly->bBlocked = false;
		pe->m_Dist = distlen;
		pe->m_Speed = speed;
		pe->m_Speedv = dist * speed;
		pe->m_Target = poly->StartSpot.pos + dist * distlen;
		if ((pe->m_Dist / pe->m_Speed) <= 2)
		{
			pe->StopInterpolation();
		}
		dist = -dist;	// reverse the direction
	}
	return pe != NULL; // Return true if something started moving.
}

//==========================================================================
//
// T_PolyDoor
//
//==========================================================================

void DPolyDoor::Tick ()
{
	FPolyObj *poly = PO_GetPolyobj (m_PolyObj);

	if (poly == NULL) return;

	if (m_Tics)
	{
		if (!--m_Tics)
		{
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, m_Close);
		}
		return;
	}
	switch (m_Type)
	{
	case PODOOR_SLIDE:
		if (m_Dist <= 0 || poly->MovePolyobj (m_Speedv))
		{
			double absSpeed = fabs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (poly);
				if (!m_Close && m_WaitTics >= 0)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Direction = -m_Direction;
					m_Speedv = -m_Speedv;
				}
				else
				{
					// if set to wait infinitely, Hexen kept the dead thinker to block the polyobject from getting activated again but that causes some problems
					// with the subsectorlinks and the interpolation. Better delete the thinker and use a different means to block it.
					if (!m_Close) poly->bBlocked = true;
					Destroy ();
				}
			}
			poly->UpdateLinks();
		}
		else
		{
			if (poly->crush || !m_Close)
			{ // continue moving if the poly is a crusher, or is opening
				return;
			}
			else
			{ // open back up
				m_Dist = m_TotalDist - m_Dist;
				m_Direction = -m_Direction;
				m_Speedv = -m_Speedv;
				m_Close = false;
				SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
			}
		}
		break;

	case PODOOR_SWING:
		if (m_Dist <= 0 || poly->RotatePolyobj (m_Speed))
		{
			double absSpeed = fabs (m_Speed);
			m_Dist -= absSpeed;
			if (m_Dist <= 0)
			{
				SN_StopSequence (poly);
				if (!m_Close && m_WaitTics >= 0)
				{
					m_Dist = m_TotalDist;
					m_Close = true;
					m_Tics = m_WaitTics;
					m_Speed = -m_Speed;
				}
				else
				{
					if (!m_Close) poly->bBlocked = true;
					Destroy ();
				}
			}
		}
		else
		{
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

bool EV_OpenPolyDoor(line_t *line, int polyNum, double speed, DAngle angle, int delay, double distance, podoortype_t type)
{
	DPolyDoor *pd = NULL;
	FPolyObj *poly;
	int swingdir = 1;	// ADD:  PODOOR_SWINGL, PODOOR_SWINGR

	if ((poly = PO_GetPolyobj(polyNum)) == NULL)
	{
		Printf("EV_OpenPolyDoor: Invalid polyobj num: %d\n", polyNum);
		return false;
	}
	FPolyMirrorIterator it(poly);

	while ((poly = it.NextMirror()) != NULL)
	{
		if ((poly->specialdata != NULL || poly->bBlocked))
		{ // poly is already moving
			break;
		}
		if (poly->bHasPortals == 2 && type == PODOOR_SWING)
		{
			// cannot do rotations on linked polyportals.
			break;
		}

		pd = Create<DPolyDoor>(poly->tag, type);
		poly->specialdata = pd;
		if (type == PODOOR_SLIDE)
		{
			pd->m_WaitTics = delay;
			pd->m_Speed = speed;
			pd->m_Dist = pd->m_TotalDist = distance; // Distance
			pd->m_Direction = angle;
			pd->m_Speedv = angle.ToVector(speed);
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
			angle += 180.;	// reverse the angle
		}
		else if (type == PODOOR_SWING)
		{
			pd->m_WaitTics = delay;
			pd->m_Direction.Degrees = swingdir; 
			pd->m_Speed = (speed*swingdir*(90. / 64)) / 8;
			pd->m_Dist = pd->m_TotalDist = angle.Degrees;
			SN_StartSequence (poly, poly->seqType, SEQ_DOOR, 0);
			swingdir = -swingdir;	// reverse the direction
		}

	}
	return pd != NULL;	// Return true if something started moving.
}

//==========================================================================
//
// EV_StopPoly
//
//==========================================================================

bool EV_StopPoly(int polynum)
{
	FPolyObj *poly;

	if (NULL != (poly = PO_GetPolyobj(polynum)))
	{
		if (poly->specialdata != NULL)
		{
			poly->specialdata->Stop();
		}
		return true;
	}
	return false;
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
// 
//
//==========================================================================

FPolyObj::FPolyObj()
{
	StartSpot.pos = { 0,0 };
	Angle = 0.;
	tag = 0;
	memset(bbox, 0, sizeof(bbox));
	validcount = 0;
	crush = 0;
	bHurtOnTouch = false;
	seqType = 0;
	Size = 0;
	bBlocked = false;
	subsectorlinks = NULL;
	specialdata = NULL;
	interpolation = NULL;
}

//==========================================================================
//
// GetPolyobjMirror
//
//==========================================================================

int FPolyObj::GetMirror()
{
	return MirrorNum;
}

//==========================================================================
//
// ThrustMobj
//
//==========================================================================

void FPolyObj::ThrustMobj (AActor *actor, side_t *side)
{
	DAngle thrustAngle;
	DPolyAction *pe;

	double force;

	if (!(actor->flags&MF_SHOOTABLE) && !actor->player)
	{
		return;
	}
	vertex_t *v1 = side->V1();
	vertex_t *v2 = side->V2();
	thrustAngle = (v2->fPos() - v1->fPos()).Angle() - 90.;

	pe = static_cast<DPolyAction *>(specialdata);
	if (pe)
	{
		if (pe->IsKindOf (RUNTIME_CLASS (DRotatePoly)))
		{
			force = pe->GetSpeed() * (90. / 2048);	// For DRotatePoly m_Speed stores an angle which needs to be converted differently
		}
		else
		{
			force = pe->GetSpeed() / 8;
		}
		force = clamp(force, 1., 4.);
	}
	else
	{
		force = 1;
	}

	DVector2 thrust = thrustAngle.ToVector(force);
	actor->Vel += thrust;

	if (crush)
	{
		DVector2 pos = actor->Vec2Offset(thrust.X, thrust.Y);
		if (bHurtOnTouch || !P_CheckMove (actor, pos))
		{
			int newdam = P_DamageMobj (actor, NULL, NULL, crush, NAME_Crush);
			P_TraceBleed (newdam > 0 ? newdam : crush, actor);
		}
	}
	if (level.flags2 & LEVEL2_POLYGRIND) actor->Grind(false); // crush corpses that get caught in a polyobject's way
}

//==========================================================================
//
// UpdateSegBBox
//
//==========================================================================

void FPolyObj::UpdateLinks()
{
	if (bHasPortals == 2)
	{
		TMap<int, bool> processed;
		for (unsigned i = 0; i < Linedefs.Size(); i++)
		{
			if (Linedefs[i]->isLinePortal())
			{
				FLinePortal *port = Linedefs[i]->getPortal();
				if (port->mType == PORTT_LINKED)
				{
					DVector2 old = port->mDisplacement;
					port->mDisplacement = port->mDestination->v2->fPos() - port->mOrigin->v1->fPos();
					FLinePortal *port2 = port->mDestination->getPortal();
					if (port2) port2->mDisplacement = -port->mDisplacement;
					int destgroup = port->mDestination->frontsector->PortalGroup;
					bool *done = processed.CheckKey(destgroup);
					if (!done || !*done)
					{
						processed[destgroup] = true;
						DVector2 delta = port->mDisplacement - old;
						level.Displacements.MoveGroup(destgroup, delta);
					}
				}
			}
		}
	}
}

void FPolyObj::UpdateBBox ()
{
	for(unsigned i=0;i<Linedefs.Size(); i++)
	{
		P_AdjustLine(Linedefs[i]);
	}
	CalcCenter();
}

void FPolyObj::CalcCenter()
{
	DVector2 c = { 0, 0 };
	for(unsigned i=0;i<Vertices.Size(); i++)
	{
		c += Vertices[i]->fPos();
	}
	CenterSpot.pos = c / Vertices.Size();
}

//==========================================================================
//
// PO_MovePolyobj
//
//==========================================================================

bool FPolyObj::MovePolyobj (const DVector2 &pos, bool force)
{
	FBoundingBox oldbounds = Bounds;
	UnLinkPolyobj ();
	DoMovePolyobj (pos);

	if (!force)
	{
		bool blocked = false;

		for(unsigned i=0;i < Sidedefs.Size(); i++)
		{
			if (CheckMobjBlocking(Sidedefs[i]))
			{
				blocked = true;
			}
		}
		if (blocked)
		{
			DoMovePolyobj (-pos);
			LinkPolyobj();
			return false;
		}
	}
	StartSpot.pos += pos;
	CenterSpot.pos += pos;
	LinkPolyobj ();
	ClearSubsectorLinks();
	RecalcActorFloorCeil(Bounds | oldbounds);
	return true;
}

//==========================================================================
//
// DoMovePolyobj
//
//==========================================================================

void FPolyObj::DoMovePolyobj (const DVector2 &pos)
{
	for(unsigned i=0;i < Vertices.Size(); i++)
	{
		Vertices[i]->set(Vertices[i]->fX() + pos.X, Vertices[i]->fY() + pos.Y);
		PrevPts[i].pos += pos;
	}
	for (unsigned i = 0; i < Linedefs.Size(); i++)
	{
		Linedefs[i]->bbox[BOXTOP] += pos.Y;
		Linedefs[i]->bbox[BOXBOTTOM] += pos.Y;
		Linedefs[i]->bbox[BOXLEFT] += pos.X;
		Linedefs[i]->bbox[BOXRIGHT] += pos.X;
	}
}

//==========================================================================
//
// RotatePt
//
//==========================================================================

static void RotatePt (DAngle an, DVector2 &out, const DVector2 &start)
{
	DVector2 tr = out;

	double s = an.Sin();
	double c = an.Cos();

	out.X = tr.X * c - tr.Y * s + start.X;
	out.Y = tr.X * s + tr.Y * c + start.Y;
}

//==========================================================================
//
// PO_RotatePolyobj
//
//==========================================================================

bool FPolyObj::RotatePolyobj (DAngle angle, bool fromsave)
{
	DAngle an;
	bool blocked;
	FBoundingBox oldbounds = Bounds;

	an = Angle + angle;

	UnLinkPolyobj();

	for(unsigned i=0;i < Vertices.Size(); i++)
	{
		PrevPts[i].pos = Vertices[i]->fPos();
		FPolyVertex torot = OriginalPts[i];
		RotatePt(an, torot.pos, StartSpot.pos);
		Vertices[i]->set(torot.pos.X, torot.pos.Y);
	}
	blocked = false;
	validcount++;
	UpdateBBox();

	// If we are loading a savegame we do not really want to damage actors and be blocked by them. This can also cause crashes when trying to damage incompletely deserialized player pawns.
	if (!fromsave)
	{
		for (unsigned i = 0; i < Sidedefs.Size(); i++)
		{
			if (CheckMobjBlocking(Sidedefs[i]))
			{
				blocked = true;
			}
		}
		if (blocked)
		{
			for(unsigned i=0;i < Vertices.Size(); i++)
			{
				Vertices[i]->set(PrevPts[i].pos.X, PrevPts[i].pos.Y);
			}
			UpdateBBox();
			LinkPolyobj();
			return false;
		}
	}
	Angle += angle;
	LinkPolyobj();
	ClearSubsectorLinks();
	RecalcActorFloorCeil(Bounds | oldbounds);
	return true;
}

//==========================================================================
//
// UnLinkPolyobj
//
//==========================================================================

void FPolyObj::UnLinkPolyobj ()
{
	polyblock_t *link;
	int i, j;
	int index;

	// remove the polyobj from each blockmap section
	for(j = bbox[BOXBOTTOM]; j <= bbox[BOXTOP]; j++)
	{
		index = j*level.blockmap.bmapwidth;
		for(i = bbox[BOXLEFT]; i <= bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < level.blockmap.bmapwidth && j >= 0 && j < level.blockmap.bmapheight)
			{
				link = PolyBlockMap[index+i];
				while(link != NULL && link->polyobj != this)
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

bool FPolyObj::CheckMobjBlocking (side_t *sd)
{
	static TArray<AActor *> checker;
	FBlockNode *block;
	AActor *mobj;
	int i, j, k;
	int left, right, top, bottom;
	line_t *ld;
	bool blocked;
	bool performBlockingThrust;
	int bmapwidth = level.blockmap.bmapwidth;
	int bmapheight = level.blockmap.bmapheight;

	ld = sd->linedef;

	top = level.blockmap.GetBlockY(ld->bbox[BOXTOP]);
	bottom = level.blockmap.GetBlockY(ld->bbox[BOXBOTTOM]);
	left = level.blockmap.GetBlockX(ld->bbox[BOXLEFT]);
	right = level.blockmap.GetBlockX(ld->bbox[BOXRIGHT]);

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
			for (block = level.blockmap.blocklinks[j+i]; block != NULL; block = block->NextActor)
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
						FLineOpening open;
						open.top = LINEOPEN_MAX;
						open.bottom = LINEOPEN_MIN;
						// [TN] Check wether this actor gets blocked by the line.
						if (ld->backsector != NULL &&
							!(ld->flags & (ML_BLOCKING|ML_BLOCKEVERYTHING))
							&& !(ld->flags & ML_BLOCK_PLAYERS && (mobj->player || (mobj->flags8 & MF8_BLOCKASPLAYER))) 
							&& !(ld->flags & ML_BLOCKMONSTERS && mobj->flags3 & MF3_ISMONSTER)
							&& !((mobj->flags & MF_FLOAT) && (ld->flags & ML_BLOCK_FLOATERS))
							&& (!(ld->flags & ML_3DMIDTEX) ||
								(!P_LineOpening_3dMidtex(mobj, ld, open) &&
									(mobj->Top() < open.top)
								) || (open.abovemidtex && mobj->Z() > mobj->floorz))
							)
						{
							// [BL] We can't just continue here since we must
							// determine if the line's backsector is going to
							// be blocked.
							performBlockingThrust = false;
						}
						else
						{
							performBlockingThrust = true;
						}

						DVector2 pos = mobj->PosRelative(ld);
						FBoundingBox box(pos.X, pos.Y, mobj->radius);

						if (!box.inRange(ld) || box.BoxOnLineSide(ld) != -1)
						{
							continue;
						}

						if (ld->isLinePortal())
						{
							// Fixme: this still needs to figure out if the polyobject move made the player cross the portal line.
							if (P_TryMove(mobj, mobj->Pos(), false))
							{
								continue;
							}
						}
						// We have a two-sided linedef so we should only check one side
						// so that the thrust from both sides doesn't cancel each other out.
						// Best use the one facing the player and ignore the back side.
						if (ld->sidedef[1] != NULL)
						{
							int side = P_PointOnLineSidePrecise(mobj->Pos(), ld);
							if (ld->sidedef[side] != sd)
							{
								continue;
							}
							// [BL] See if we hit below the floor/ceiling of the poly.
							else if(!performBlockingThrust && (
									mobj->Z() < ld->sidedef[!side]->sector->GetSecPlane(sector_t::floor).ZatPoint(mobj) ||
									mobj->Top() > ld->sidedef[!side]->sector->GetSecPlane(sector_t::ceiling).ZatPoint(mobj)
								))
							{
								performBlockingThrust = true;
							}
						}

						if(performBlockingThrust)
						{
							ThrustMobj (mobj, sd);
							blocked = true;
						}
						else
							continue;
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

void FPolyObj::LinkPolyobj ()
{
	polyblock_t **link;
	polyblock_t *tempLink;
	int bmapwidth = level.blockmap.bmapwidth;
	int bmapheight = level.blockmap.bmapheight;

	// calculate the polyobj bbox
	Bounds.ClearBox();
	for(unsigned i = 0; i < Sidedefs.Size(); i++)
	{
		vertex_t *vt;
		
		vt = Sidedefs[i]->linedef->v1;
		Bounds.AddToBox(vt->fPos());
		vt = Sidedefs[i]->linedef->v2;
		Bounds.AddToBox(vt->fPos());
	}
	bbox[BOXRIGHT] = level.blockmap.GetBlockX(Bounds.Right());
	bbox[BOXLEFT] = level.blockmap.GetBlockX(Bounds.Left());
	bbox[BOXTOP] = level.blockmap.GetBlockY(Bounds.Top());
	bbox[BOXBOTTOM] = level.blockmap.GetBlockY(Bounds.Bottom());
	// add the polyobj to each blockmap section
	for(int j = bbox[BOXBOTTOM]*bmapwidth; j <= bbox[BOXTOP]*bmapwidth;
		j += bmapwidth)
	{
		for(int i = bbox[BOXLEFT]; i <= bbox[BOXRIGHT]; i++)
		{
			if(i >= 0 && i < bmapwidth && j >= 0 && j < bmapheight*bmapwidth)
			{
				link = &PolyBlockMap[j+i];
				if(!(*link))
				{ // Create a new link at the current block cell
					*link = new polyblock_t;
					(*link)->next = NULL;
					(*link)->prev = NULL;
					(*link)->polyobj = this;
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
					tempLink->polyobj = this;
					continue;
				}
				else
				{
					tempLink->next = new polyblock_t;
					tempLink->next->next = NULL;
					tempLink->next->prev = tempLink;
					tempLink->next->polyobj = this;
				}
			}
			// else, don't link the polyobj, since it's off the map
		}
	}
}

//===========================================================================
//
// FPolyObj :: RecalcActorFloorCeil
//
// For each actor within the bounding box, recalculate its floorz, ceilingz,
// and related values.
//
//===========================================================================

void FPolyObj::RecalcActorFloorCeil(FBoundingBox bounds) const
{
	FBlockThingsIterator it(bounds);
	AActor *actor;

	while ((actor = it.Next()) != NULL)
	{
		// skip everything outside the bounding box.
		if (actor->X() + actor->radius <= bounds.Left() ||
			actor->X() - actor->radius >= bounds.Right() ||
			actor->Y() + actor->radius <= bounds.Bottom() ||
			actor->Y() - actor->radius >= bounds.Top())
		{
			continue;
		}
		// Todo: Be a little more thorough with what gets altered here
		// because this can dislocate a lot of items that were spawned on 
		// the lower side of a sector boundary.
		P_FindFloorCeiling(actor);
	}
}

//===========================================================================
//
// PO_ClosestPoint
//
// Given a point (x,y), returns the point (ox,oy) on the polyobject's walls
// that is nearest to (x,y). Also returns the seg this point came from.
//
//===========================================================================

void FPolyObj::ClosestPoint(const DVector2 &fpos, DVector2 &out, side_t **side) const
{
	unsigned int i;
	double x = fpos.X, y = fpos.Y;
	double bestdist = HUGE_VAL;
	double bestx = 0, besty = 0;
	side_t *bestline = NULL;

	for (i = 0; i < Sidedefs.Size(); ++i)
	{
		vertex_t *v1 = Sidedefs[i]->V1();
		vertex_t *v2 = Sidedefs[i]->V2();
		double a = v2->fX() - v1->fX();
		double b = v2->fY() - v1->fY();
		double den = a*a + b*b;
		double ix, iy, dist;

		if (den == 0)
		{ // Line is actually a point!
			ix = v1->fX();
			iy = v1->fY();
		}
		else
		{
			double num = (x - v1->fX()) * a + (y - v1->fY()) * b;
			double u = num / den;
			if (u <= 0)
			{
				ix = v1->fX();
				iy = v1->fY();
			}
			else if (u >= 1)
			{
				ix = v2->fX();
				iy = v2->fY();
			}
			else
			{
				ix = v1->fX() + u * a;
				iy = v1->fY() + u * b;
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
			bestline = Sidedefs[i];
		}
	}
	out = { bestx, besty };
	if (side != NULL)
	{
		*side = bestline;
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
	int bmapwidth = level.blockmap.bmapwidth;
	int bmapheight = level.blockmap.bmapheight;

	PolyBlockMap = new polyblock_t *[bmapwidth*bmapheight];
	memset (PolyBlockMap, 0, bmapwidth*bmapheight*sizeof(polyblock_t *));

	for (i = 0; i < po_NumPolyobjs; i++)
	{
		polyobjs[i].LinkPolyobj();
	}
}

//==========================================================================
//
// InitSideLists [RH]
//
// Group sides by vertex and collect side that are known to belong to a
// polyobject so that they can be initialized fast.
//==========================================================================

static void InitSideLists ()
{
	for (unsigned i = 0; i < level.sides.Size(); ++i)
	{
		if (level.sides[i].linedef != NULL &&
			(level.sides[i].linedef->special == Polyobj_StartLine ||
				level.sides[i].linedef->special == Polyobj_ExplicitLine))
		{
			KnownPolySides.Push (i);
		}
	}
}

//==========================================================================
//
// KillSideLists [RH]
//
//==========================================================================

static void KillSideLists ()
{
	KnownPolySides.Clear ();
	KnownPolySides.ShrinkToFit ();
}

//==========================================================================
//
// AddPolyVert
//
// Helper function for IterFindPolySides()
//
//==========================================================================

static void AddPolyVert(TArray<uint32_t> &vnum, uint32_t vert)
{
	for (unsigned int i = vnum.Size() - 1; i-- != 0; )
	{
		if (vnum[i] == vert)
		{ // Already in the set. No need to add it.
			return;
		}
	}
	vnum.Push(vert);
}

//==========================================================================
//
// IterFindPolySides
//
// Beginning with the first vertex of the starting side, for each vertex
// in vnum, add all the sides that use it as a first vertex to the polyobj,
// and add all their second vertices to vnum. This continues until there
// are no new vertices in vnum.
//
//==========================================================================

static void IterFindPolySides (FPolyObj *po, side_t *side)
{
	static TArray<uint32_t> vnum;
	unsigned int vnumat;

	assert(sidetemp != NULL);

	vnum.Clear();
	vnum.Push(uint32_t(side->V1()->Index()));
	vnumat = 0;

	while (vnum.Size() != vnumat)
	{
		uint32_t sidenum = sidetemp[vnum[vnumat++]].b.first;
		while (sidenum != NO_SIDE)
		{
			po->Sidedefs.Push(&level.sides[sidenum]);
			AddPolyVert(vnum, uint32_t(level.sides[sidenum].V2()->Index()));
			sidenum = sidetemp[sidenum].b.next;
		}
	}
}


//==========================================================================
//
// SpawnPolyobj
//
//==========================================================================

static int posicmp(const void *a, const void *b)
{
	return (*(const side_t **)a)->linedef->args[1] - (*(const side_t **)b)->linedef->args[1];
}

static void SpawnPolyobj (int index, int tag, int type)
{
	unsigned int ii;
	int i;
	FPolyObj *po = &polyobjs[index];

	for (ii = 0; ii < KnownPolySides.Size(); ++ii)
	{
		i = KnownPolySides[ii];
		if (i < 0)
		{
			continue;
		}
		po->bBlocked = false;
		po->bHasPortals = 0;

		side_t *sd = &level.sides[i];
		
		if (sd->linedef->special == Polyobj_StartLine &&
			sd->linedef->args[0] == tag)
		{
			if (po->Sidedefs.Size() > 0)
			{
				Printf (TEXTCOLOR_RED "SpawnPolyobj: Polyobj %d already spawned.\n", tag);
				return;
			}
			else
			{
				sd->linedef->special = 0;
				sd->linedef->args[0] = 0;
				IterFindPolySides(&polyobjs[index], sd);
				po->MirrorNum = sd->linedef->args[1];
				po->crush = (type != SMT_PolySpawn) ? 3 : 0;
				po->bHurtOnTouch = (type == SMT_PolySpawnHurt);
				po->tag = tag;
				po->seqType = sd->linedef->args[2];
				if (po->seqType < 0 || po->seqType > 63)
				{
					po->seqType = 0;
				}
			}
			break;
		}
	}
	if (po->Sidedefs.Size() == 0)
	{ 
		// didn't find a polyobj through PO_LINE_START
		TArray<side_t *> polySideList;
		unsigned int psIndexOld;
		psIndexOld = po->Sidedefs.Size();

		for (ii = 0; ii < KnownPolySides.Size(); ++ii)
		{
			i = KnownPolySides[ii];

			if (i >= 0 &&
				level.sides[i].linedef->special == Polyobj_ExplicitLine &&
				level.sides[i].linedef->args[0] == tag)
			{
				if (!level.sides[i].linedef->args[1])
				{
					Printf(TEXTCOLOR_RED "SpawnPolyobj: Explicit line missing order number in poly %d, linedef %d.\n", tag, level.sides[i].linedef->Index());
					return;
				}
				else
				{
					po->Sidedefs.Push(&level.sides[i]);
				}
			}
		}
		qsort(&po->Sidedefs[0], po->Sidedefs.Size(), sizeof(po->Sidedefs[0]), posicmp);
		if (po->Sidedefs.Size() > 0)
		{
			po->crush = (type != SMT_PolySpawn) ? 3 : 0;
			po->bHurtOnTouch = (type == SMT_PolySpawnHurt);
			po->tag = tag;
			po->seqType = po->Sidedefs[0]->linedef->args[3];
			po->MirrorNum = po->Sidedefs[0]->linedef->args[2];
		}
		else
		{
			Printf(TEXTCOLOR_RED "SpawnPolyobj: Poly %d does not exist\n", tag);
			return;
		}
	}

	validcount++;	
	for(unsigned int i=0; i<po->Sidedefs.Size(); i++)
	{
		line_t *l = po->Sidedefs[i]->linedef;

		if (l->validcount != validcount)
		{
			FLinePortal *port = l->getPortal();
			if (port && (port->mDefFlags & PORTF_PASSABLE))
			{
				int type = port->mType == PORTT_LINKED ? 2 : 1;
				if (po->bHasPortals < type) po->bHasPortals = (uint8_t)type;
			}
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

static void TranslateToStartSpot (int tag, const DVector2 &origin)
{
	FPolyObj *po;
	DVector2 delta;

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
		Printf(TEXTCOLOR_RED "TranslateToStartSpot: Unable to match polyobj tag: %d\n", tag);
		return;
	}
	if (po->Sidedefs.Size() == 0)
	{
		Printf(TEXTCOLOR_RED "TranslateToStartSpot: Anchor point located without a StartSpot point: %d\n", tag);
		return;
	}
	po->OriginalPts.Resize(po->Sidedefs.Size());
	po->PrevPts.Resize(po->Sidedefs.Size());
	delta = origin - po->StartSpot.pos;

	for (unsigned i = 0; i < po->Sidedefs.Size(); i++)
	{
		po->Sidedefs[i]->Flags |= WALLF_POLYOBJ;
	}
	for (unsigned i = 0; i < po->Linedefs.Size(); i++)
	{
		po->Linedefs[i]->bbox[BOXTOP] -= delta.Y;
		po->Linedefs[i]->bbox[BOXBOTTOM] -= delta.Y;
		po->Linedefs[i]->bbox[BOXLEFT] -= delta.X;
		po->Linedefs[i]->bbox[BOXRIGHT] -= delta.X;
	}
	for (unsigned i = 0; i < po->Vertices.Size(); i++)
	{
		po->Vertices[i]->set(po->Vertices[i]->fX() - delta.X, po->Vertices[i]->fY() - delta.Y);
		po->OriginalPts[i].pos = po->Vertices[i]->fPos() - po->StartSpot.pos;
	}
	po->CalcCenter();
	// For compatibility purposes
	po->CenterSubsector = R_PointInSubsector(po->CenterSpot.pos);
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
		if (polyspawn->type >= SMT_PolySpawn &&	polyspawn->type <= SMT_PolySpawnHurt)
		{ 
			// Polyobj StartSpot Pt.
			polyobjs[polyIndex].StartSpot.pos = polyspawn->pos;
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
		if (polyspawn->type == SMT_PolyAnchor)
		{ 
			// Polyobj Anchor Pt.
			TranslateToStartSpot (polyspawn->angle, polyspawn->pos);
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
			Printf (TEXTCOLOR_RED "PO_Init: StartSpot located without an Anchor point: %d\n", polyobjs[polyIndex].tag);
		}
	}
	InitBlockMap();

	// [RH] Don't need the side lists anymore
	KillSideLists ();

	// mark all subsectors which have a seg belonging to a polyobj
	// These ones should not be rendered on the textured automap.
	for (auto &ss : level.subsectors)
	{
		for(uint32_t j=0;j<ss.numlines; j++)
		{
			if (ss.firstline[j].sidedef != NULL &&
				ss.firstline[j].sidedef->Flags & WALLF_POLYOBJ)
			{
				ss.flags |= SSECF_POLYORG;
				break;
			}
		}
	}
	// clear all polyobj specials so that they do not obstruct using other lines.
	for (auto &line : level.lines)
	{
		if (line.special == Polyobj_ExplicitLine || line.special == Polyobj_StartLine)
		{
			line.special = 0;
		}
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



//==========================================================================
//
//
//
//==========================================================================

void FPolyObj::ClearSubsectorLinks()
{
	while (subsectorlinks != NULL)
	{
		assert(subsectorlinks->state == 1337);

		FPolyNode *next = subsectorlinks->snext;

		if (subsectorlinks->pnext != NULL)
		{
			assert(subsectorlinks->pnext->state == 1337);
			subsectorlinks->pnext->pprev = subsectorlinks->pprev;
		}

		if (subsectorlinks->pprev != NULL)
		{
			assert(subsectorlinks->pprev->state == 1337);
			subsectorlinks->pprev->pnext = subsectorlinks->pnext;
		}
		else
		{
			subsectorlinks->subsector->polys = subsectorlinks->pnext;
		}

		if (subsectorlinks->subsector->BSP != NULL)
		{
			subsectorlinks->subsector->BSP->bDirty = true;
		}

		subsectorlinks->state = -1;
		delete subsectorlinks;
		subsectorlinks = next;
	}
	subsectorlinks = NULL;
}

void FPolyObj::ClearAllSubsectorLinks()
{
	for (int i = 0; i < po_NumPolyobjs; i++)
	{
		polyobjs[i].ClearSubsectorLinks();
	}
	ReleaseAllPolyNodes();
}

//==========================================================================
//
// GetIntersection
//
// adapted from P_InterceptVector
//
//==========================================================================

static bool GetIntersection(FPolySeg *seg, node_t *bsp, FPolyVertex *v)
{
	double frac;
	double num;
	double den;

	double v2x = seg->v1.pos.X;
	double v2y = seg->v1.pos.Y;
	double v2dx = seg->v2.pos.X - v2x;
	double v2dy = seg->v2.pos.Y - v2y;
	double v1x =  FIXED2DBL(bsp->x);
	double v1y =  FIXED2DBL(bsp->y);
	double v1dx = FIXED2DBL(bsp->dx);
	double v1dy = FIXED2DBL(bsp->dy);
		
	den = v1dy*v2dx - v1dx*v2dy;

	if (den == 0)
		return false;		// parallel
	
	num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
	frac = num / den;

	if (frac < 0. || frac > 1.) return false;

	v->pos.X = v2x + frac * v2dx;
	v->pos.Y = v2y + frac * v2dy;
	return true;
}

//==========================================================================
//
// PartitionDistance
//
// Determine the distance of a vertex to a node's partition line.
//
//==========================================================================

static double PartitionDistance(FPolyVertex *vt, node_t *node)
{	
	return fabs(FIXED2DBL(-node->dy) * (vt->pos.X - FIXED2DBL(node->x)) + FIXED2DBL(node->dx) * (vt->pos.Y - FIXED2DBL(node->y))) / node->len;
}

//==========================================================================
//
// AddToBBox
//
//==========================================================================

static void AddToBBox(float child[4], float parent[4])
{
	if (child[BOXTOP] > parent[BOXTOP])
	{
		parent[BOXTOP] = child[BOXTOP];
	}
	if (child[BOXBOTTOM] < parent[BOXBOTTOM])
	{
		parent[BOXBOTTOM] = child[BOXBOTTOM];
	}
	if (child[BOXLEFT] < parent[BOXLEFT])
	{
		parent[BOXLEFT] = child[BOXLEFT];
	}
	if (child[BOXRIGHT] > parent[BOXRIGHT])
	{
		parent[BOXRIGHT] = child[BOXRIGHT];
	}
}

//==========================================================================
//
// AddToBBox
//
//==========================================================================

static void AddToBBox(FPolyVertex *v, float bbox[4])
{
	float x = float(v->pos.X);
	float y = float(v->pos.Y);
	if (x < bbox[BOXLEFT])
	{
		bbox[BOXLEFT] = x;
	}
	if (x > bbox[BOXRIGHT])
	{
		bbox[BOXRIGHT] = x;
	}
	if (y < bbox[BOXBOTTOM])
	{
		bbox[BOXBOTTOM] = y;
	}
	if (y > bbox[BOXTOP])
	{
		bbox[BOXTOP] = y;
	}
}

//==========================================================================
//
// SplitPoly
//
//==========================================================================

static void SplitPoly(FPolyNode *pnode, void *node, float bbox[4])
{
	static TArray<FPolySeg> lists[2];
	static const double POLY_EPSILON = 0.3125;

	if (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		int centerside = R_PointOnSide(pnode->poly->CenterSpot.pos, bsp);

		lists[0].Clear();
		lists[1].Clear();
		for(unsigned i=0;i<pnode->segs.Size(); i++)
		{
			FPolySeg *seg = &pnode->segs[i];

			// Parts of the following code were taken from Eternity and are
			// being used with permission.

			// get distance of vertices from partition line
			// If the distance is too small, we may decide to
			// change our idea of sidedness.
			double dist_v1 = PartitionDistance(&seg->v1, bsp);
			double dist_v2 = PartitionDistance(&seg->v2, bsp);

			// If the distances are less than epsilon, consider the points as being
			// on the same side as the polyobj origin. Why? People like to build
			// polyobject doors flush with their door tracks. This breaks using the
			// usual assumptions.
			

			// Addition to Eternity code: We must also check any seg with only one
			// vertex inside the epsilon threshold. If not, these lines will get split but
			// adjoining ones with both vertices inside the threshold won't thus messing up
			// the order in which they get drawn.

			if(dist_v1 <= POLY_EPSILON)
			{
				if (dist_v2 <= POLY_EPSILON)
				{
					lists[centerside].Push(*seg);
				}
				else
				{
					int side = R_PointOnSide(seg->v2.pos, bsp);
					lists[side].Push(*seg);
				}
			}
			else if (dist_v2 <= POLY_EPSILON)
			{
				int side = R_PointOnSide(seg->v1.pos, bsp);
				lists[side].Push(*seg);
			}
			else 
			{
				int side1 = R_PointOnSide(seg->v1.pos, bsp);
				int side2 = R_PointOnSide(seg->v2.pos, bsp);

				if(side1 != side2)
				{
					// if the partition line crosses this seg, we must split it.

					FPolyVertex vert;

					if (GetIntersection(seg, bsp, &vert))
					{
						lists[0].Push(*seg);
						lists[1].Push(*seg);
						lists[side1].Last().v2 = vert;
						lists[side2].Last().v1 = vert;
					}
					else
					{
						// should never happen
						lists[side1].Push(*seg);
					}
				}
				else 
				{
					// both points on the same side.
					lists[side1].Push(*seg);
				}
			}
		}
		if (lists[1].Size() == 0)
		{
			SplitPoly(pnode, bsp->children[0], bsp->bbox[0]);
			AddToBBox(bsp->bbox[0], bbox);
		}
		else if (lists[0].Size() == 0)
		{
			SplitPoly(pnode, bsp->children[1], bsp->bbox[1]);
			AddToBBox(bsp->bbox[1], bbox);
		}
		else
		{
			// create the new node 
			FPolyNode *newnode = NewPolyNode();
			newnode->poly = pnode->poly;
			newnode->segs = lists[1];

			// set segs for original node
			pnode->segs = lists[0];
		
			// recurse back side
			SplitPoly(newnode, bsp->children[1], bsp->bbox[1]);
			
			// recurse front side
			SplitPoly(pnode, bsp->children[0], bsp->bbox[0]);

			AddToBBox(bsp->bbox[0], bbox);
			AddToBBox(bsp->bbox[1], bbox);
		}
	}
	else
	{
		// we reached a subsector so we can link the node with this subsector
		subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);

		// Link node to subsector
		pnode->pnext = sub->polys;
		if (pnode->pnext != NULL) 
		{
			assert(pnode->pnext->state == 1337);
			pnode->pnext->pprev = pnode;
		}
		pnode->pprev = NULL;
		sub->polys = pnode;

		// link node to polyobject
		pnode->snext = pnode->poly->subsectorlinks;
		pnode->poly->subsectorlinks = pnode;
		pnode->subsector = sub;

		// calculate bounding box for this polynode
		assert(pnode->segs.Size() != 0);
		float subbbox[4] = { FLT_MIN, FLT_MAX, FLT_MAX, FLT_MIN };

		for (unsigned i = 0; i < pnode->segs.Size(); ++i)
		{
			AddToBBox(&pnode->segs[i].v1, subbbox);
			AddToBBox(&pnode->segs[i].v2, subbbox);
		}
		// Potentially expand the parent node's bounding box to contain these bits of polyobject.
		AddToBBox(subbbox, bbox);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FPolyObj::CreateSubsectorLinks()
{
	FPolyNode *node = NewPolyNode();
	// Even though we don't care about it, we need to initialize this
	// bounding box to something so that Valgrind won't complain about it
	// when SplitPoly modifies it.
	float dummybbox[4] = { 0 };

	node->poly = this;
	node->segs.Resize(Sidedefs.Size());

	for(unsigned i=0; i<Sidedefs.Size(); i++)
	{
		FPolySeg *seg = &node->segs[i];
		side_t *side = Sidedefs[i];

		seg->v1 = side->V1();
		seg->v2 = side->V2();
		seg->wall = side;
	}
	if (!(i_compatflags & COMPATF_POLYOBJ))
	{
		SplitPoly(node, level.HeadNode(), dummybbox);
	}
	else
	{
		subsector_t *sub = CenterSubsector;

		// Link node to subsector
		node->pnext = sub->polys;
		if (node->pnext != NULL) 
		{
			assert(node->pnext->state == 1337);
			node->pnext->pprev = node;
		}
		node->pprev = NULL;
		sub->polys = node;

		// link node to polyobject
		node->snext = node->poly->subsectorlinks;
		node->poly->subsectorlinks = node;
		node->subsector = sub;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void PO_LinkToSubsectors()
{
	for (int i = 0; i < po_NumPolyobjs; i++)
	{
		if (polyobjs[i].subsectorlinks == NULL)
		{
			polyobjs[i].CreateSubsectorLinks();
		}
	}
}

//==========================================================================
//
// NewPolyNode
//
//==========================================================================

static FPolyNode *NewPolyNode()
{
	FPolyNode *node;

	if (FreePolyNodes != NULL)
	{
		node = FreePolyNodes;
		FreePolyNodes = node->pnext;
	}
	else
	{
		node = new FPolyNode;
	}
	node->state = 1337;
	node->poly = NULL;
	node->pnext = NULL;
	node->pprev = NULL;
	node->subsector = NULL;
	node->snext = NULL;
	return node;
}

//==========================================================================
//
// FreePolyNode
//
//==========================================================================

void FreePolyNode(FPolyNode *node)
{
	node->segs.Clear();
	node->pnext = FreePolyNodes;
	FreePolyNodes = node;
}

//==========================================================================
//
// ReleaseAllPolyNodes
//
//==========================================================================

void ReleaseAllPolyNodes()
{
	FPolyNode *node, *next;

	for (node = FreePolyNodes; node != NULL; node = next)
	{
		next = node->pnext;
		delete node;
	}
}

//==========================================================================
//
// FPolyMirrorIterator Constructor
//
// This class is used to avoid infinitely looping on cyclical chains of
// mirrored polyobjects.
//
//==========================================================================

FPolyMirrorIterator::FPolyMirrorIterator(FPolyObj *poly)
{
	CurPoly = poly;
	if (poly != NULL)
	{
		UsedPolys[0] = poly->tag;
		NumUsedPolys = 1;
	}
	else
	{
		NumUsedPolys = 0;
	}
}

//==========================================================================
//
// FPolyMirrorIterator :: NextMirror
//
// Returns the polyobject that mirrors the current one, or NULL if there
// is no mirroring polyobject, or there is a mirroring polyobject but it was
// already returned.
//
//==========================================================================

FPolyObj *FPolyMirrorIterator::NextMirror()
{
	FPolyObj *poly = CurPoly, *nextpoly;

	if (poly == NULL)
	{
		return NULL;
	}

	// Do the work to decide which polyobject to return the next time this
	// function is called.
	int mirror = poly->GetMirror(), i;
	nextpoly = NULL;

	// Is there a mirror and we have room to remember it?
	if (mirror != 0 && NumUsedPolys != countof(UsedPolys))
	{
		// Has this polyobject been returned already?
		for (i = 0; i < NumUsedPolys; ++i)
		{
			if (UsedPolys[i] == mirror)
			{
				break;	// Yes, it has been returned.
			}
		}
		if (i == NumUsedPolys)
		{ // No, it has not been returned.
			UsedPolys[NumUsedPolys++] = mirror;
			nextpoly = PO_GetPolyobj(mirror);
			if (nextpoly == NULL)
			{
				Printf("Invalid mirror polyobj num %d for polyobj num %d\n", mirror, UsedPolys[i - 1]);
			}
		}
	}
	CurPoly = nextpoly;
	return poly;
}
