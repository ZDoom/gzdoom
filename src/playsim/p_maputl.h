#ifndef __P_MAPUTL_H
#define __P_MAPUTL_H

#include <float.h>
#include "r_defs.h"
#include "doomstat.h"
#include "doomdata.h"
#include "m_bbox.h"
#include "cmdlib.h"

extern int validcount;
struct FBlockNode;

struct divline_t
{
	double 	x;
	double 	y;
	double 	dx;
	double 	dy;
};

struct intercept_t
{
	double		frac;
	bool	 	isaline;
	bool		done;
	union {
		AActor *thing;
		line_t *line;
	} d;
};

//==========================================================================
//
// P_PointOnLineSide
//
// Returns 0 (front/on) or 1 (back)
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int P_PointOnLineSidePrecise(double x, double y, const line_t *line)
{
	return (y - line->v1->fY()) * line->Delta().X + (line->v1->fX() - x) * line->Delta().Y > EQUAL_EPSILON;
}

inline int P_PointOnLineSidePrecise(const DVector2 &pt, const line_t *line)
{
	return (pt.Y - line->v1->fY()) * line->Delta().X + (line->v1->fX() - pt.X) * line->Delta().Y > EQUAL_EPSILON;
}

inline int P_PointOnLineSide (double x, double y, const line_t *line)
{
	extern int P_VanillaPointOnLineSide(double x, double y, const line_t* line);
	return (line->flags & ML_COMPATSIDE)
		? P_VanillaPointOnLineSide(x, y, line) : P_PointOnLineSidePrecise(x, y, line);
}

inline int P_PointOnLineSide(const DVector2 & p, const line_t *line)
{
	return P_PointOnLineSide(p.X, p.Y, line);
}




//==========================================================================
//
// P_PointOnDivlineSideCompat
//
// Same as P_PointOnLineSide except it uses divlines
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int P_PointOnDivlineSide(double x, double y, const divline_t *line)
{
	return (y - line->y) * line->dx + (line->x - x) * line->dy > EQUAL_EPSILON;
}

inline int P_PointOnDivlineSide(const DVector2 &pos, const divline_t *line)
{
	return (pos.Y - line->y) * line->dx + (line->x - pos.X) * line->dy > EQUAL_EPSILON;
}

//==========================================================================
//
// P_MakeDivline
//
//==========================================================================

inline void P_MakeDivline(const line_t *li, divline_t *dl)
{
	dl->x = li->v1->fX();
	dl->y = li->v1->fY();
	dl->dx = li->Delta().X;
	dl->dy = li->Delta().Y;
}

struct FLineOpening
{
	double			top;
	double			bottom;
	double			range;
	double			lowfloor;
	sector_t		*bottomsec;
	sector_t		*topsec;
	FTextureID		ceilingpic;
	FTextureID		floorpic;
	secplane_t		frontfloorplane;
	secplane_t		backfloorplane;
	int				floorterrain;
	bool			touchmidtex;
	bool			abovemidtex;
	uint8_t			lowfloorthroughportal;
	F3DFloor		*topffloor;
	F3DFloor		*bottomffloor;
};

static const double LINEOPEN_MIN = -FLT_MAX;
static const double LINEOPEN_MAX = FLT_MAX;

void P_LineOpening(FLineOpening &open, AActor *thing, const line_t *linedef, const DVector2 &xy, const DVector2 *ref = nullptr, int flags = 0);
inline void P_LineOpening(FLineOpening &open, AActor *thing, const line_t *linedef, const DVector2 &xy, const DVector3 *ref, int flags = 0)
{
	P_LineOpening(open, thing, linedef, xy, reinterpret_cast<const DVector2*>(ref), flags);
}

class FBoundingBox;
struct polyblock_t;

//============================================================================
//
// This is a dynamic array which holds its first MAX_STATIC entries in normal
// variables to avoid constant allocations which this would otherwise
// require.
// 
// When collecting touched portal groups the normal cases are either
// no portals == one group or
// two portals = two groups
// 
// Anything with more can happen but far less infrequently, so this
// organization helps avoiding the overhead from heap allocations
// in the vast majority of situations.
//
//============================================================================

struct FPortalGroupArray
{
	// Controls how groups are connected
	enum
	{
		PGA_NoSectorPortals,// only collect line portals
		PGA_CheckPosition,	// only collects sector portals at the actual position
		PGA_Full3d,			// Goes up and down sector portals at any linedef within the bounding box (this is a lot slower and should only be done if really needed.)
	};

	enum
	{
		LOWER = 0x4000,
		UPPER = 0x8000,
		FLAT = 0xc000,
	};

	enum
	{
		MAX_STATIC = 4
	};

	FPortalGroupArray(int collectionmethod = PGA_CheckPosition)
	{
		method = collectionmethod;
		varused = 0;
		inited = false;
	}

	void Clear()
	{
		data.Clear();
		varused = 0;
		inited = false;
	}

	void Add(uint32_t num)
	{
		if (varused < MAX_STATIC) entry[varused++] = (uint16_t)num;
		else data.Push((uint16_t)num);
	}

	unsigned Size()
	{
		return varused + data.Size();
	}

	uint32_t operator[](unsigned index)
	{
		return index < MAX_STATIC ? entry[index] : data[index - MAX_STATIC];
	}

	bool inited;
	int method;

private:
	uint16_t entry[MAX_STATIC];
	uint8_t varused;
	TArray<uint16_t> data;
};

class FBlockLinesIterator
{
	friend class FMultiBlockLinesIterator;
	FLevelLocals *Level;
	int minx, maxx;
	int miny, maxy;

	int curx, cury;
	polyblock_t *polyLink;
	int polyIndex;
	int *list;

	void StartBlock(int x, int y);

	FBlockLinesIterator(FLevelLocals *l)  { Level = l; }
	void init(const FBoundingBox &box);
public:
	FBlockLinesIterator(FLevelLocals *Level, int minx, int miny, int maxx, int maxy, bool keepvalidcount = false);
	FBlockLinesIterator(FLevelLocals *Level, const FBoundingBox &box);
	line_t *Next();
	void Reset() { StartBlock(minx, miny); }
};

class FMultiBlockLinesIterator
{
	FPortalGroupArray &checklist;
	DVector3 checkpoint;
	DVector2 offset;
	sector_t *startsector;
	sector_t *cursector;
	short basegroup;
	short portalflags;
	short index;
	bool continueup;
	bool continuedown;
	FBlockLinesIterator blockIterator;
	FBoundingBox bbox;

	bool GoUp(double x, double y);
	bool GoDown(double x, double y);
	bool startIteratorForGroup(int group);

public:

	struct CheckResult
	{
		line_t *line;
		DVector3 Position;
		int portalflags;
	};

	FMultiBlockLinesIterator(FPortalGroupArray &check, AActor *origin, double checkradius = -1);
	FMultiBlockLinesIterator(FPortalGroupArray &check, FLevelLocals *Level, double checkx, double checky, double checkz, double checkh, double checkradius, sector_t *newsec);

	bool Next(CheckResult *item);
	void Reset();
	// for stopping group traversal through portals. Only the calling code can decide whether this is needed so this needs to be set from the outside.
	void StopUp()
	{
		continueup = false;
	}
	void StopDown()
	{
		continuedown = false;
	}
	const FBoundingBox &Box() const
	{
		return bbox;
	}
};


class FBlockThingsIterator
{
	FLevelLocals *Level;
	int minx, maxx;
	int miny, maxy;

	int curx, cury;

	FBlockNode *block;

	int Buckets[32];

	struct HashEntry
	{
		AActor *Actor;
		int Next;
	};
	HashEntry FixedHash[10];
	int NumFixedHash;
	TArray<HashEntry> DynHash;

	HashEntry *GetHashEntry(int i) { return i < (int)countof(FixedHash) ? &FixedHash[i] : &DynHash[i - countof(FixedHash)]; }

	void StartBlock(int x, int y);
	void SwitchBlock(int x, int y);
	void ClearHash();

	// The following is only for use in the path traverser 
	// and therefore declared private.
	FBlockThingsIterator(FLevelLocals *);

	friend class FPathTraverse;
	friend class FMultiBlockThingsIterator;

public:
	FBlockThingsIterator(FLevelLocals *Level, int minx, int miny, int maxx, int maxy);
	FBlockThingsIterator(FLevelLocals *l, const FBoundingBox &box)
	{
		Level = l;
		init(box);
	}
	void init(const FBoundingBox &box, bool clearhash = true);
	AActor *Next(bool centeronly = false);
	void Reset() { StartBlock(minx, miny); }
};

class FMultiBlockThingsIterator
{
	FPortalGroupArray &checklist;
	DVector3 checkpoint;
	short basegroup;
	short portalflags;
	short index;
	FBlockThingsIterator blockIterator;
	FBoundingBox bbox;

	void startIteratorForGroup(int group);

protected:
	FMultiBlockThingsIterator(FPortalGroupArray &check, FLevelLocals *Level) : checklist(check), blockIterator(Level) {}
public:

	struct CheckResult
	{
		AActor *thing;
		DVector3 Position;
		int portalflags;
	};

	FMultiBlockThingsIterator(FPortalGroupArray &check, AActor *origin, double checkradius = -1, bool ignorerestricted = false);
	FMultiBlockThingsIterator(FPortalGroupArray &check, FLevelLocals *Level, double checkx, double checky, double checkz, double checkh, double checkradius, bool ignorerestricted, sector_t *newsec);
	bool Next(CheckResult *item);
	void Reset();
	const FBoundingBox &Box() const
	{
		return bbox;
	}
};



class FPathTraverse
{
protected:
	static TArray<intercept_t> intercepts;

	FLevelLocals *Level;
	divline_t trace;
	double Startfrac;
	unsigned int intercept_index;
	unsigned int intercept_count;
	unsigned int count;

	virtual void AddLineIntercepts(int bx, int by);
	virtual void AddThingIntercepts(int bx, int by, FBlockThingsIterator &it, bool compatible);
	FPathTraverse(FLevelLocals *l) 
	{
		Level = l;
	}
public:

	intercept_t *Next();

	FPathTraverse(FLevelLocals *l, double x1, double y1, double x2, double y2, int flags, double startfrac = 0)
	{
		Level = l;
		init(x1, y1, x2, y2, flags, startfrac);
	}
	void init(double x1, double y1, double x2, double y2, int flags, double startfrac = 0);
	int PortalRelocate(intercept_t *in, int flags, DVector3 *optpos = nullptr);
	void PortalRelocate(const DVector2 &disp, int flags, double hitfrac);
	virtual ~FPathTraverse();
	const divline_t &Trace() const { return trace; }

	inline DVector2 InterceptPoint(const intercept_t *in)
	{
		return
		{
			trace.x + trace.dx * in->frac,
			trace.y + trace.dy * in->frac
		};
	}

};

//
// P_MAPUTL
//

typedef bool(*traverser_t) (intercept_t *in);

int P_AproxDistance (int dx, int dy);
double P_InterceptVector(const divline_t *v2, const divline_t *v1);

#define PT_ADDLINES 	1
#define PT_ADDTHINGS	2
#define PT_COMPATIBLE	4
#define PT_DELTA		8		// x2,y2 is passed as a delta, not as an endpoint

int BoxOnLineSide(const FBoundingBox& box, const line_t* ld);

#endif
