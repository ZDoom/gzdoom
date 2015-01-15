#ifndef __PO_MAN_H
#define __PO_MAN_H

#include "tarray.h"
#include "r_defs.h"
#include "m_bbox.h"


struct FPolyVertex
{
	fixed_t x, y;

	FPolyVertex &operator=(vertex_t *v)
	{
		x = v->x;
		y = v->y;
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

	angle_t		angle;
	int			tag;			// reference tag assigned in HereticEd
	int			bbox[4];		// bounds in blockmap coordinates
	int			validcount;
	int			crush; 			// should the polyobj attempt to crush mobjs?
	bool		bHurtOnTouch;	// should the polyobj hurt anything it touches?
	int			seqType;
	fixed_t		size;			// polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
	FPolyNode	*subsectorlinks;
	DPolyAction	*specialdata;	// pointer to a thinker, if the poly is moving
	TObjPtr<DInterpolation> interpolation;

	FPolyObj();
	DInterpolation *SetInterpolation();
	void StopInterpolation();

	int GetMirror();
	bool MovePolyobj (int x, int y, bool force = false);
	bool RotatePolyobj (angle_t angle, bool fromsave = false);
	void ClosestPoint(fixed_t fx, fixed_t fy, fixed_t &ox, fixed_t &oy, side_t **side) const;
	void LinkPolyobj ();
	void RecalcActorFloorCeil(FBoundingBox bounds) const;
	void CreateSubsectorLinks();
	void ClearSubsectorLinks();
	void CalcCenter();
	static void ClearAllSubsectorLinks();

private:

	void ThrustMobj (AActor *actor, side_t *side);
	void UpdateBBox ();
	void DoMovePolyobj (int x, int y);
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


#endif