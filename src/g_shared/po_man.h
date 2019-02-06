#ifndef __PO_MAN_H
#define __PO_MAN_H

#include "tarray.h"
#include "r_defs.h"
#include "m_bbox.h"
#include "dthinker.h"

struct FPolyObj;

class DPolyAction : public DThinker
{
	DECLARE_CLASS(DPolyAction, DThinker)
	HAS_OBJECT_POINTERS
public:
	void Construct(FPolyObj *polyNum);
	void Serialize(FSerializer &arc);
	void OnDestroy() override;
	void Stop();
	double GetSpeed() const { return m_Speed; }

	void StopInterpolation();
protected:
	FPolyObj *m_PolyObj;
	double m_Speed;
	double m_Dist;
	TObjPtr<DInterpolation*> m_Interpolation;

	void SetInterpolation();
};

struct FPolyVertex
{
	DVector2 pos;

	FPolyVertex &operator=(vertex_t *v)
	{
		pos = v->fPos();
		return *this;
	}
};

struct FPolySeg
{
	FPolyVertex v1;
	FPolyVertex v2;
	side_t *wall;
};

//
// Linked lists of polyobjects
//
struct FPolyObj;
struct FPolyNode
{
	FPolyObj *poly;				// owning polyobject
	FPolyNode *pnext;			// next polyobj in list
	FPolyNode *pprev;			// previous polyobj

	subsector_t *subsector;		// containimg subsector
	FPolyNode *snext;			// next subsector

	TArray<FPolySeg> segs;		// segs for this node
	int state;
};

// ===== Polyobj data =====
struct FPolyObj
{
	FLevelLocals			*Level;
	TArray<side_t *>		Sidedefs;
	TArray<line_t *>		Linedefs;
	TArray<vertex_t *>		Vertices;
	TArray<FPolyVertex>		OriginalPts;
	TArray<FPolyVertex>		PrevPts;
	FPolyVertex				StartSpot;
	FPolyVertex				CenterSpot;
	FBoundingBox			Bounds;	// Bounds in map coordinates 
	subsector_t				*CenterSubsector;
	int						MirrorNum;

	DAngle		Angle;
	int			tag;			// reference tag assigned in HereticEd
	int			bbox[4];		// bounds in blockmap coordinates
	int			validcount;
	int			crush; 			// should the polyobj attempt to crush mobjs?
	bool		bHurtOnTouch;	// should the polyobj hurt anything it touches?
	bool		bBlocked;
	uint8_t		bHasPortals;	// 1 for any portal, 2 for a linked portal (2 must block rotations.)
	int			seqType;
	double		Size;			// polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
	FPolyNode	*subsectorlinks;
	TObjPtr<DPolyAction*> specialdata;	// pointer to a thinker, if the poly is moving
	TObjPtr<DInterpolation*> interpolation;

	FPolyObj();
	DInterpolation *SetInterpolation();
	void StopInterpolation();

	int GetMirror();
	bool MovePolyobj (const DVector2 &pos, bool force = false);
	bool RotatePolyobj (DAngle angle, bool fromsave = false);
	void ClosestPoint(const DVector2 &fpos, DVector2 &out, side_t **side) const;
	void LinkPolyobj ();
	void RecalcActorFloorCeil(FBoundingBox bounds) const;
	void CreateSubsectorLinks();
	void ClearSubsectorLinks();
	void CalcCenter();
	void UpdateLinks();
	static void ClearAllSubsectorLinks();

private:

	void ThrustMobj (AActor *actor, side_t *side);
	void UpdateBBox ();
	void DoMovePolyobj (const DVector2 &pos);
	void UnLinkPolyobj ();
	bool CheckMobjBlocking (side_t *sd);

};

struct polyblock_t
{
	FPolyObj *polyobj;
	struct polyblock_t *prev;
	struct polyblock_t *next;
};


void PO_LinkToSubsectors(FLevelLocals *Level);


// ===== PO_MAN =====

typedef enum
{
	PODOOR_NONE,
	PODOOR_SLIDE,
	PODOOR_SWING,
} podoortype_t;

bool EV_RotatePoly (FLevelLocals *Level, line_t *line, int polyNum, int speed, int byteAngle, int direction, bool overRide);
bool EV_MovePoly (FLevelLocals *Level, line_t *line, int polyNum, double speed, DAngle angle, double dist, bool overRide);
bool EV_MovePolyTo (FLevelLocals *Level, line_t *line, int polyNum, double speed, const DVector2 &pos, bool overRide);
bool EV_OpenPolyDoor (FLevelLocals *Level, line_t *line, int polyNum, double speed, DAngle angle, int delay, double distance, podoortype_t type);
bool EV_StopPoly (FLevelLocals *Level, int polyNum);

bool PO_Busy (FLevelLocals *Level, int polyobj);


#endif
