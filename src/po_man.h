#ifndef __PO_MAN_H
#define __PO_MAN_H

#include "tarray.h"
#include "r_defs.h"
#include "m_bbox.h"

class DPolyAction : public DThinker
{
	DECLARE_CLASS(DPolyAction, DThinker)
	HAS_OBJECT_POINTERS
public:
	DPolyAction(int polyNum);
	void Serialize(FArchive &arc);
	void Destroy();
	void Stop();
	double GetSpeed() const { return m_Speed; }

	void StopInterpolation();
protected:
	DPolyAction();
	int m_PolyObj;
	double m_Speed;
	double m_Dist;
	TObjPtr<DInterpolation> m_Interpolation;

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
	BYTE		bHasPortals;	// 1 for any portal, 2 for a linked portal (2 must block rotations.)
	int			seqType;
	double		Size;			// polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
	FPolyNode	*subsectorlinks;
	TObjPtr<DPolyAction> specialdata;	// pointer to a thinker, if the poly is moving
	TObjPtr<DInterpolation> interpolation;

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
extern FPolyObj *polyobjs;		// list of all poly-objects on the level

struct polyblock_t
{
	FPolyObj *polyobj;
	struct polyblock_t *prev;
	struct polyblock_t *next;
};


void PO_LinkToSubsectors();
FArchive &operator<< (FArchive &arc, FPolyObj *&poly);
FArchive &operator<< (FArchive &arc, const FPolyObj *&poly);


// ===== PO_MAN =====

typedef enum
{
	PODOOR_NONE,
	PODOOR_SLIDE,
	PODOOR_SWING,
} podoortype_t;

bool EV_RotatePoly (line_t *line, int polyNum, int speed, int byteAngle, int direction, bool overRide);
bool EV_MovePoly (line_t *line, int polyNum, double speed, DAngle angle, double dist, bool overRide);
bool EV_MovePolyTo (line_t *line, int polyNum, double speed, const DVector2 &pos, bool overRide);
bool EV_OpenPolyDoor (line_t *line, int polyNum, double speed, DAngle angle, int delay, double distance, podoortype_t type);
bool EV_StopPoly (int polyNum);


// [RH] Data structure for P_SpawnMapThing() to keep track
//		of polyobject-related things.
struct polyspawns_t
{
	polyspawns_t *next;
	DVector2 pos;
	short angle;
	short type;
};

extern int po_NumPolyobjs;
extern polyspawns_t *polyspawns;	// [RH] list of polyobject things to spawn


void PO_Init ();
bool PO_Busy (int polyobj);
FPolyObj *PO_GetPolyobj(int polyNum);


#endif