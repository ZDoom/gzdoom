#ifndef _PORTALS_H_
#define _PORTALS_H_

#include "basictypes.h"
#include "v_video.h"
#include "m_bbox.h"

struct FPortalGroupArray;
//============================================================================
//
// This table holds the offsets for the different parts of a map
// that are connected by portals.
// The idea here is basically the same as implemented in Eternity Engine:
//
// - each portal creates two sector groups in the map 
//   which are offset by the displacement of the portal anchors
//
// - for two or multiple groups the displacement is calculated by
//   adding the displacements between intermediate groups which
//   have to be traversed to connect the two
//
// - any sector not connected to any portal is assigned to group 0
//   Group 0 has no displacement to any other group in the level.
//
//============================================================================

struct FDisplacement
{
	DVector2 pos;
	bool isSet;
	BYTE indirect;	// just for illustration.

};

struct FDisplacementTable
{
	TArray<FDisplacement> data;
	int size;

	FDisplacementTable()
	{
		Create(1);
	}

	void Create(int numgroups)
	{
		data.Resize(numgroups*numgroups);
		memset(&data[0], 0, numgroups*numgroups*sizeof(data[0]));
		size = numgroups;
	}

	FDisplacement &operator()(int x, int y)
	{
		return data[x + size*y];
	}

	DVector2 getOffset(int x, int y) const
	{
		if (x == y)
		{
			DVector2 nulvec = { 0,0 };
			return nulvec;	// shortcut for the most common case
		}
		return data[x + size*y].pos;
	}

};

extern FDisplacementTable Displacements;


//============================================================================
//
// A blockmap that only contains crossable portals
// This is used for quick checks if a vector crosses through one.
//
//============================================================================

struct FPortalBlock
{
	bool neighborContainsLines;	// this is for skipping the traverser and exiting early if we can quickly decide that there's no portals nearby.
	bool containsLinkedPortals;	// this is for sight check optimization. We can't early-out on an impenetrable line if there may be portals being found in the same block later on.
	TArray<line_t*> portallines;

	FPortalBlock()
	{
		neighborContainsLines = false;
		containsLinkedPortals = false;
	}
};

struct FPortalBlockmap
{
	TArray<FPortalBlock> data;
	int dx, dy;
	bool containsLines;
	bool hasLinkedSectorPortals;	// global flag to shortcut portal checks if the map has none.
	bool hasLinkedPolyPortals;	// this means that any early-outs in P_CheckSight need to be disabled if a block contains polyobjects.

	void Create(int blockx, int blocky)
	{
		data.Resize(blockx*blocky);
		dx = blockx;
		dy = blocky;
	}

	void Clear()
	{
		data.Clear();
		data.ShrinkToFit();
		dx = dy = 0;
		containsLines = false;
		hasLinkedPolyPortals = false;
		hasLinkedSectorPortals = false;
	}

	FPortalBlock &operator()(int x, int y)
	{
		return data[x + dx*y];
	}
};

extern FPortalBlockmap PortalBlockmap;


//============================================================================
//
// Flags and types for linedef portals
//
//============================================================================

enum
{
	PORTF_VISIBLE = 1,
	PORTF_PASSABLE = 2,
	PORTF_SOUNDTRAVERSE = 4,
	PORTF_INTERACTIVE = 8,

	PORTF_TYPETELEPORT = PORTF_VISIBLE | PORTF_PASSABLE | PORTF_SOUNDTRAVERSE,
	PORTF_TYPEINTERACTIVE = PORTF_VISIBLE | PORTF_PASSABLE | PORTF_SOUNDTRAVERSE | PORTF_INTERACTIVE,
};

enum
{
	PORTT_VISUAL,
	PORTT_TELEPORT,
	PORTT_INTERACTIVE,
	PORTT_LINKED,
	PORTT_LINKEDEE	// Eternity compatible definition which uses only one line ID and a different anchor type to link to.
};

enum
{
	PORG_ABSOLUTE,	// does not align at all. z-ccoordinates must match.
	PORG_FLOOR,
	PORG_CEILING,
};

enum
{
	PCOLL_NOTLINKED = 1,
	PCOLL_LINKED = 2
};

//============================================================================
//
// All information about a line-to-line portal (all types)
// There is no structure for sector plane portals because for historic
// reasons those use actors to connect.
//
//============================================================================

struct FLinePortal
{
	line_t *mOrigin;
	line_t *mDestination;
	DVector2 mDisplacement;
	BYTE mType;
	BYTE mFlags;
	BYTE mDefFlags;
	BYTE mAlign;
	DAngle mAngleDiff;
	double mSinRot;
	double mCosRot;
};

extern TArray<FLinePortal> linePortals;

void P_ClearPortals();
void P_SpawnLinePortal(line_t* line);
void P_FinalizePortals();
bool P_ChangePortal(line_t *ln, int thisid, int destid);
void P_CreateLinkedPortals();
bool P_CollectConnectedGroups(int startgroup, const DVector3 &position, double upperz, double checkradius, FPortalGroupArray &out);
void P_CollectLinkedPortals();
inline int P_NumPortalGroups()
{
	return Displacements.size;
}


/* code ported from prototype */
bool P_ClipLineToPortal(line_t* line, line_t* portal, DVector2 view, bool partial = true, bool samebehind = true);
void P_TranslatePortalXY(line_t* src, double& vx, double& vy);
void P_TranslatePortalVXVY(line_t* src, double &velx, double &vely);
void P_TranslatePortalAngle(line_t* src, DAngle& angle);
void P_TranslatePortalZ(line_t* src, double& vz);
DVector2 P_GetOffsetPosition(double x, double y, double dx, double dy);


#endif