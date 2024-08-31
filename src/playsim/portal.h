#ifndef _PORTALS_H_
#define _PORTALS_H_

#include "basics.h"
#include "m_bbox.h"

struct linebase_t;
struct line_t;
struct sector_t;

struct FPortalGroupArray;
struct portnode_t;
struct subsector_t;
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
//   Group 0 has no displacement to any other group in the level
//
//============================================================================

struct FDisplacement
{
	DVector2 pos;
	bool isSet;
	uint8_t indirect;	// just for illustration.

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

	void MoveGroup(int grp, DVector2 delta)
	{
		for (int i = 1; i < size; i++)
		{
			data[grp + size*i].pos -= delta;
			data[i + grp*size].pos += delta;
		}
	}
};


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
	PORTF_POLYOBJ = 16,

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
//
//============================================================================
struct FLinePortalSpan;
struct vertex_t;

struct FLinePortal
{
	line_t *mOrigin;
	line_t *mDestination;
	DVector2 mDisplacement;
	uint8_t mType;
	uint8_t mFlags;
	uint8_t mDefFlags;
	uint8_t mAlign;
	DAngle mAngleDiff;
	double mSinRot;
	double mCosRot;
	portnode_t *lineportal_thinglist;
	FLinePortalSpan *mGroup;

	FLinePortal()
	{
		Clear();
	}

	void Clear()
	{
		memset((void*)this, 0, sizeof * this);
	}
};

struct FLinePortalSpan
{
	// defines the complete span of connected colinear line portals, if they are of type PORTT_LINKED.
	vertex_t	*v1 = nullptr, *v2 = nullptr;	// vertices, from v1 to v2
	TArray<FLinePortal *> lines;
	int validcount = 0;
};


//============================================================================
//
// All information about a sector plane portal
//
//============================================================================

enum
{
	PORTS_SKYVIEWPOINT = 0,		// a regular skybox
	PORTS_STACKEDSECTORTHING,	// stacked sectors with the thing method
	PORTS_PORTAL,				// stacked sectors with Sector_SetPortal
	PORTS_LINKEDPORTAL,			// linked portal (interactive)
	PORTS_PLANE,				// EE-style plane portal (not implemented in SW renderer)
	PORTS_HORIZON,				// EE-style horizon portal (not implemented in SW renderer)
};

enum
{
	PORTSF_SKYFLATONLY = 1,				// portal is only active on skyflatnum
	PORTSF_INSKYBOX = 2,				// to avoid recursion
};

struct FSectorPortal
{
	int mType;
	int mFlags;
	unsigned mPartner;
	int mPlane;
	sector_t *mOrigin;
	sector_t *mDestination;
	DVector2 mDisplacement;
	double mPlaneZ;
	TObjPtr<AActor*> mSkybox;

	FSectorPortal()
	{
		Clear();
	}

	void Clear()
	{
		memset((void*)this, 0, sizeof * this);
	}

	bool MergeAllowed() const
	{
		// For thing based stack sectors and regular skies the portal has no relevance for merging visplanes.
		return (mType == PORTS_STACKEDSECTORTHING || (mType == PORTS_SKYVIEWPOINT && (mFlags & PORTSF_SKYFLATONLY)));
	}
};

//============================================================================
//
// This groups all sector portals with identical offset.
//
//============================================================================

struct FSectorPortalGroup
{
	DVector2 mDisplacement;
	int plane;
};



//============================================================================
//
// Functions
//
//============================================================================



/* code ported from prototype */
bool P_ClipLineToPortal(linebase_t* line, linebase_t* portal, DVector2 view, bool partial = true, bool samebehind = true);
void P_TranslatePortalXY(line_t* src, double& vx, double& vy);
void P_TranslatePortalVXVY(line_t* src, double &velx, double &vely);
void P_TranslatePortalAngle(line_t* src, DAngle& angle);
void P_TranslatePortalZ(line_t* src, double& vz);
struct FLevelLocals;
void InitPortalGroups(FLevelLocals *Level);


#endif
