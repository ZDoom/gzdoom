// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//		Refresh/rendering module, shared data struct definitions.
//
//-----------------------------------------------------------------------------


#ifndef __R_DEFS_H__
#define __R_DEFS_H__

#include "doomdef.h"
#include "templates.h"
#include "memarena.h"
#include "m_bbox.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
// SECTORS do store MObjs anyway.
#include "actor.h"
struct FLightNode;
struct FGLSection;
struct FPortal;
struct seg_t;

#include "dthinker.h"

#define MAXWIDTH 5760
#define MAXHEIGHT 3600

const WORD NO_INDEX = 0xffffu;
const DWORD NO_SIDE = 0xffffffffu;

// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
enum
{
	SIL_NONE,
	SIL_BOTTOM,
	SIL_TOP,
	SIL_BOTH
};

extern size_t MaxDrawSegs;
struct FDisplacement;

//
// INTERNAL MAP TYPES
//	used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
//	like some DOOM-alikes ("wt", "WebView") did.
//
enum
{
	VERTEXFLAG_ZCeilingEnabled = 0x01,
	VERTEXFLAG_ZFloorEnabled   = 0x02
};
struct vertexdata_t
{
	double zCeiling, zFloor;
	DWORD flags;
};

#ifdef USE_FLOAT
typedef float vtype;
#elif !defined USE_FIXED
typedef double vtype;
#endif


struct vertex_t
{
private:
	DVector2 p;

public:

	void set(fixed_t x, fixed_t y)
	{
		p.X = x / 65536.;
		p.Y = y / 65536.;
	}

	void set(double x, double y)
	{
		p.X = x;
		p.Y = y;
	}

	void set(const DVector2 &pos)
	{
		p = pos;
	}

	double fX() const
	{
		return p.X;
	}

	double fY() const
	{
		return p.Y;
	}

	fixed_t fixX() const
	{
		return FLOAT2FIXED(p.X);
	}

	fixed_t fixY() const
	{
		return FLOAT2FIXED(p.Y);
	}

	DVector2 fPos()
	{
		return { p.X, p.Y };
	}

	angle_t viewangle;	// precalculated angle for clipping
	int angletime;		// recalculation time for view angle
	bool dirty;			// something has changed and needs to be recalculated
	int numheights;
	int numsectors;
	sector_t ** sectors;
	float * heightlist;

	vertex_t()
	{
		p = { 0,0 };
		angletime = 0;
		viewangle = 0;
		dirty = true;
		numheights = numsectors = 0;
		sectors = NULL;
		heightlist = NULL;
	}

	bool operator== (const vertex_t &other)
	{
		return p == other.p;
	}

	bool operator!= (const vertex_t &other)
	{
		return p != other.p;
	}

	void clear()
	{
		p.Zero();
	}

	angle_t GetClipAngle();
};

// Forward of LineDefs, for Sectors.
struct line_t;

class player_t;
class FScanner;
class FBitmap;
struct FCopyInfo;
class DInterpolation;
class FArchive;

enum
{
	UDMF_Line,
	UDMF_Side,
	UDMF_Sector,
	UDMF_Thing
};


struct FUDMFKey
{
	enum
	{
		UDMF_Int,
		UDMF_Float,
		UDMF_String
	};

	FName Key;
	int Type;
	int IntVal;
	double FloatVal;
	FString StringVal;

	FUDMFKey()
	{
	}

	FUDMFKey& operator =(int val)
	{
		Type = UDMF_Int;
		IntVal = val;
		FloatVal = val;
		StringVal = "";
		return *this;
	}

	FUDMFKey& operator =(double val)
	{
		Type = UDMF_Float;
		IntVal = int(val);
		FloatVal = val;
		StringVal = "";
		return *this;
	}

	FUDMFKey& operator =(const FString &val)
	{
		Type = UDMF_String;
		IntVal = strtol(val.GetChars(), NULL, 0);
		FloatVal = strtod(val.GetChars(), NULL);
		StringVal = val;
		return *this;
	}

};

class FUDMFKeys : public TArray<FUDMFKey>
{
public:
	void Sort();
	FUDMFKey *Find(FName key);
};

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
class DSectorEffect;
struct sector_t;
struct FRemapTable;

enum
{
	SECSPAC_Enter		= 1,	// Trigger when player enters
	SECSPAC_Exit		= 2,	// Trigger when player exits
	SECSPAC_HitFloor	= 4,	// Trigger when player hits floor
	SECSPAC_HitCeiling	= 8,	// Trigger when player hits ceiling
	SECSPAC_Use			= 16,	// Trigger when player uses
	SECSPAC_UseWall		= 32,	// Trigger when player uses a wall
	SECSPAC_EyesDive	= 64,	// Trigger when player eyes go below fake floor
	SECSPAC_EyesSurface = 128,	// Trigger when player eyes go above fake floor
	SECSPAC_EyesBelowC	= 256,	// Trigger when player eyes go below fake ceiling
	SECSPAC_EyesAboveC	= 512,	// Trigger when player eyes go above fake ceiling
	SECSPAC_HitFakeFloor= 1024,	// Trigger when player hits fake floor
};

class ASectorAction : public AActor
{
	DECLARE_CLASS (ASectorAction, AActor)
public:
	ASectorAction (bool activatedByUse = false);
	void Destroy ();
	void BeginPlay ();
	void Activate (AActor *source);
	void Deactivate (AActor *source);
	bool TriggerAction(AActor *triggerer, int activationType);
	bool CanTrigger (AActor *triggerer) const;
	bool IsActivatedByUse() const;
protected:
	virtual bool DoTriggerAction(AActor *triggerer, int activationType);
	bool CheckTrigger(AActor *triggerer) const;
private:
	bool ActivatedByUse;
};

class ASkyViewpoint;

struct secplane_t
{
	friend FArchive &operator<< (FArchive &arc, secplane_t &plane);
	// the plane is defined as a*x + b*y + c*z + d = 0
	// ic is 1/c, for faster Z calculations

//private:
	DVector3 normal;
	double  D, negiC;	// negative iC because that also saves a negation in all methods using this.
public:

	void set(double aa, double bb, double cc, double dd)
	{
		normal.X = aa;
		normal.Y = bb;
		normal.Z = cc;
		D = dd;
		negiC = -1 / cc;
	}

	void setD(double dd)
	{
		D = dd;
	}

	double fC() const
	{
		return normal.Z;
	}
	double fD() const
	{
		return D;
	}
	
	bool isSlope() const
	{
		return !normal.XY().isZero();
	}

	DVector3 Normal() const
	{
		return normal;
	}

	// Returns < 0 : behind; == 0 : on; > 0 : in front
	int PointOnSide (fixed_t x, fixed_t y, fixed_t z) const
	{
		return PointOnSide(DVector3(FIXED2DBL(x), FIXED2DBL(y), FIXED2DBL(z)));
	}

	int PointOnSide(const DVector3 &pos) const
	{
		double v = (normal | pos) + D;
		return v < -EQUAL_EPSILON ? -1 : v > EQUAL_EPSILON ? 1 : 0;
	}

	// Returns the value of z at (0,0) This is used by the 3D floor code which does not handle slopes
	fixed_t Zat0 () const
	{
		return FLOAT2FIXED(negiC*D);
	}

	// Returns the value of z at (x,y)
	fixed_t ZatPoint(fixed_t x, fixed_t y) const = delete;	// it is not allowed to call this.

	fixed_t ZatPointFixed(fixed_t x, fixed_t y) const
	{
		return FLOAT2FIXED(ZatPoint(FIXED2DBL(x), FIXED2DBL(y)));
	}

	// This is for the software renderer
	fixed_t ZatPointFixed(const DVector2 &pos) const
	{
		return FLOAT2FIXED(ZatPoint(pos));
	}

	fixed_t ZatPointFixed(const vertex_t *v) const
	{
		return FLOAT2FIXED(ZatPoint(v));
	}


	// Returns the value of z at (x,y) as a double
	double ZatPoint (double x, double y) const
	{
		return (D + normal.X*x + normal.Y*y) * negiC;
	}

	double ZatPoint(const DVector2 &pos) const
	{
		return (D + normal.X*pos.X + normal.Y*pos.Y) * negiC;
	}


	double ZatPoint(const vertex_t *v) const
	{
		return (D + normal.X*v->fX() + normal.Y*v->fY()) * negiC;
	}

	double ZatPoint(const AActor *ac) const
	{
		return (D + normal.X*ac->X() + normal.Y*ac->Y()) * negiC;
	}

	// Returns the value of z at vertex v if d is equal to dist
	double ZatPointDist(const vertex_t *v, double dist)
	{
		return (dist + normal.X*v->fX() + normal.Y*v->fY()) * negiC;
	}
	// Flips the plane's vertical orientiation, so that if it pointed up,
	// it will point down, and vice versa.
	void FlipVert ()
	{
		normal = -normal;
		D = -D;
		negiC = -negiC;
	}

	// Returns true if 2 planes are the same
	bool operator== (const secplane_t &other) const
	{
		return normal == other.normal && D == other.D;
	}

	// Returns true if 2 planes are different
	bool operator!= (const secplane_t &other) const
	{
		return normal != other.normal || D != other.D;
	}

	// Moves a plane up/down by hdiff units
	void ChangeHeight(double hdiff)
	{
		D = D - hdiff * normal.Z;
	}

	// Moves a plane up/down by hdiff units
	double GetChangedHeight(double hdiff)
	{
		return D - hdiff * normal.Z;
	}

	// Returns how much this plane's height would change if d were set to oldd
	double HeightDiff(double oldd) const
	{
		return (D - oldd) * negiC;
	}

	// Returns how much this plane's height would change if d were set to oldd
	double HeightDiff(double oldd, double newd) const
	{
		return (newd - oldd) * negiC;
	}

	double PointToDist(const DVector2 &xy, double z) const
	{
		return -(normal.X * xy.X + normal.Y * xy.Y + normal.Z * z);
	}

	double PointToDist(const vertex_t *v, double z) const
	{
		return -(normal.X * v->fX() + normal.Y * v->fY() + normal.Z * z);
	}

	void SetAtHeight(double height, int ceiling)
	{
		normal.X = normal.Y = 0;
		if (ceiling)
		{
			normal.Z = -1;
			negiC = 1;
			D = height;
		}
		else
		{
			normal.Z = 1;
			negiC = -1;
			D = -height;
		}
	}

	bool CopyPlaneIfValid (secplane_t *dest, const secplane_t *opp) const;

};

FArchive &operator<< (FArchive &arc, secplane_t &plane);


#include "p_3dfloors.h"
struct subsector_t;
struct sector_t;
struct side_t;
extern bool gl_plane_reflection_i;

// Ceiling/floor flags
enum
{
	PLANEF_ABSLIGHTING	= 1,	// floor/ceiling light is absolute, not relative
	PLANEF_BLOCKED		= 2,	// can not be moved anymore.
	PLANEF_ADDITIVE		= 4,	// rendered additive

	// linked portal stuff
	PLANEF_NORENDER		= 8,
	PLANEF_NOPASS		= 16,
	PLANEF_BLOCKSOUND	= 32,
	PLANEF_DISABLED		= 64,
	PLANEF_OBSTRUCTED	= 128,	// if the portal plane is beyond the sector's floor or ceiling.
	PLANEF_LINKED		= 256	// plane is flagged as a linked portal
};

// Internal sector flags
enum
{
	SECF_FAKEFLOORONLY	= 2,	// when used as heightsec in R_FakeFlat, only copies floor
	SECF_CLIPFAKEPLANES = 4,	// as a heightsec, clip planes to target sector's planes
	SECF_NOFAKELIGHT	= 8,	// heightsec does not change lighting
	SECF_IGNOREHEIGHTSEC= 16,	// heightsec is only for triggering sector actions
	SECF_UNDERWATER		= 32,	// sector is underwater
	SECF_FORCEDUNDERWATER= 64,	// sector is forced to be underwater
	SECF_UNDERWATERMASK	= 32+64,
	SECF_DRAWN			= 128,	// sector has been drawn at least once
	SECF_HIDDEN			= 256,	// Do not draw on textured automap
};

enum
{
	SECF_SILENT			= 1,	// actors in sector make no noise
	SECF_NOFALLINGDAMAGE= 2,	// No falling damage in this sector
	SECF_FLOORDROP		= 4,	// all actors standing on this floor will remain on it when it lowers very fast.
	SECF_NORESPAWN		= 8,	// players can not respawn in this sector
	SECF_FRICTION		= 16,	// sector has friction enabled
	SECF_PUSH			= 32,	// pushers enabled
	SECF_SILENTMOVE		= 64,	// Sector movement makes mo sound (Eternity got this so this may be useful for an extended cross-port standard.) 
	SECF_DMGTERRAINFX	= 128,	// spawns terrain splash when inflicting damage
	SECF_ENDGODMODE		= 256,	// getting damaged by this sector ends god mode
	SECF_ENDLEVEL		= 512,	// ends level when health goes below 10
	SECF_HAZARD			= 1024,	// Change to Strife's delayed damage handling.

	SECF_WASSECRET		= 1 << 30,	// a secret that was discovered
	SECF_SECRET			= 1 << 31,	// a secret sector

	SECF_DAMAGEFLAGS = SECF_ENDGODMODE|SECF_ENDLEVEL|SECF_DMGTERRAINFX|SECF_HAZARD,
	SECF_NOMODIFY = SECF_SECRET|SECF_WASSECRET,	// not modifiable by Sector_ChangeFlags
	SECF_SPECIALFLAGS = SECF_DAMAGEFLAGS|SECF_FRICTION|SECF_PUSH,	// these flags originate from 'special and must be transferrable by floor thinkers
};

enum
{
	PL_SKYFLAT = 0x40000000
};

struct FDynamicColormap;


struct FLinkedSector
{
	sector_t *Sector;
	int Type;
};


// this substructure contains a few sector properties that are stored in dynamic arrays
// These must not be copied by R_FakeFlat etc. or bad things will happen.
struct extsector_t
{
	// Boom sector transfer information
	struct fakefloor
	{
		TArray<sector_t *> Sectors;
	} FakeFloor;

	// 3DMIDTEX information
	struct midtex
	{
		struct plane
		{
			TArray<sector_t *> AttachedSectors;		// all sectors containing 3dMidtex lines attached to this sector
			TArray<line_t *> AttachedLines;			// all 3dMidtex lines attached to this sector
		} Floor, Ceiling;
	} Midtex;

	// Linked sector information
	struct linked
	{
		struct plane
		{
			TArray<FLinkedSector> Sectors;
		} Floor, Ceiling;
	} Linked;

	// 3D floors
	struct xfloor
	{
		TDeletingArray<F3DFloor *>		ffloors;		// 3D floors in this sector
		TArray<lightlist_t>				lightlist;		// 3D light list
		TArray<sector_t*>				attached;		// 3D floors attached to this sector
	} XFloor;

	TArray<vertex_t *> vertices;
	
	void Serialize(FArchive &arc);
};

struct FTransform
{
	// killough 3/7/98: floor and ceiling texture offsets
	fixed_t xoffs, yoffs;

	// [RH] floor and ceiling texture scales
	fixed_t xscale, yscale;

	// [RH] floor and ceiling texture rotation
	angle_t angle;

	// base values
	fixed_t base_angle, base_yoffs;
};

struct secspecial_t
{
	FNameNoInit damagetype;		// [RH] Means-of-death for applied damage
	int damageamount;			// [RH] Damage to do while standing on floor
	short special;
	short damageinterval;	// Interval for damage application
	short leakydamage;		// chance of leaking through radiation suit
	int Flags;

	secspecial_t()
	{
		Clear();
	}

	void Clear()
	{
		memset(this, 0, sizeof(*this));
	}
};

FArchive &operator<< (FArchive &arc, secspecial_t &p);

enum class EMoveResult { ok, crushed, pastdest };

struct sector_t
{
	// Member functions

private:
	bool MoveAttached(int crush, double move, int floorOrCeiling, bool resetfailed);
public:
	EMoveResult MoveFloor(double speed, double dest, int crush, int direction, bool hexencrush);
	EMoveResult MoveCeiling(double speed, double dest, int crush, int direction, bool hexencrush);

	inline EMoveResult MoveFloor(double speed, double dest, int direction)
	{
		return MoveFloor(speed, dest, -1, direction, false);
	}

	inline EMoveResult MoveCeiling(double speed, double dest, int direction)
	{
		return MoveCeiling(speed, dest, -1, direction, false);
	}

	bool IsLinked(sector_t *other, bool ceiling) const;
	double FindLowestFloorSurrounding(vertex_t **v) const;
	double FindHighestFloorSurrounding(vertex_t **v) const;
	double FindNextHighestFloor(vertex_t **v) const;
	double FindNextLowestFloor(vertex_t **v) const;
	double FindLowestCeilingSurrounding(vertex_t **v) const;			// jff 2/04/98
	double FindHighestCeilingSurrounding(vertex_t **v) const;			// jff 2/04/98
	double FindNextLowestCeiling(vertex_t **v) const;					// jff 2/04/98
	double FindNextHighestCeiling(vertex_t **v) const;					// jff 2/04/98
	double FindShortestTextureAround() const;							// jff 2/04/98
	double FindShortestUpperAround() const;								// jff 2/04/98
	sector_t *FindModelFloorSector(double floordestheight) const;		// jff 2/04/98
	sector_t *FindModelCeilingSector(double floordestheight) const;		// jff 2/04/98
	int FindMinSurroundingLight (int max) const;
	sector_t *NextSpecialSector (int type, sector_t *prev) const;		// [RH]
	double FindLowestCeilingPoint(vertex_t **v) const;
	double FindHighestFloorPoint(vertex_t **v) const;

	void AdjustFloorClip () const;
	void SetColor(int r, int g, int b, int desat);
	void SetFade(int r, int g, int b);
	void ClosestPoint(const DVector2 &pos, DVector2 &out) const;
	int GetFloorLight () const;
	int GetCeilingLight () const;
	sector_t *GetHeightSec() const;
	double GetFriction(int plane = sector_t::floor, double *movefac = NULL) const;

	DInterpolation *SetInterpolation(int position, bool attach);

	FSectorPortal *ValidatePortal(int which);
	void CheckPortalPlane(int plane);

	enum
	{
		floor,
		ceiling
	};

	struct splane
	{
		FTransform xform;
		int Flags;
		int Light;
		fixed_t alpha;
		FTextureID Texture;
		fixed_t TexZ;
	};


	splane planes[2];

	void SetXOffset(int pos, double o)
	{
		planes[pos].xform.xoffs = FLOAT2FIXED(o);
	}

	void AddXOffset(int pos, double o)
	{
		planes[pos].xform.xoffs += FLOAT2FIXED(o);
	}

	fixed_t GetXOffset(int pos) const
	{
		return planes[pos].xform.xoffs;
	}

	double GetXOffsetF(int pos) const
	{
		return FIXED2DBL(planes[pos].xform.xoffs);
	}

	void SetYOffset(int pos, double o)
	{
		planes[pos].xform.yoffs = FLOAT2FIXED(o);
	}

	void AddYOffset(int pos, double o)
	{
		planes[pos].xform.yoffs += FLOAT2FIXED(o);
	}

	fixed_t GetYOffset(int pos, bool addbase = true) const
	{
		if (!addbase)
		{
			return planes[pos].xform.yoffs;
		}
		else
		{
			return planes[pos].xform.yoffs + planes[pos].xform.base_yoffs;
		}
	}

	double GetYOffsetF(int pos, bool addbase = true) const
	{
		if (!addbase)
		{
			return FIXED2DBL(planes[pos].xform.yoffs);
		}
		else
		{
			return FIXED2DBL(planes[pos].xform.yoffs + planes[pos].xform.base_yoffs);
		}
	}

	void SetXScale(int pos, double o)
	{
		planes[pos].xform.xscale = FLOAT2FIXED(o);
	}

	fixed_t GetXScale(int pos) const
	{
		return planes[pos].xform.xscale;
	}

	double GetXScaleF(int pos) const
	{
		return FIXED2DBL(planes[pos].xform.xscale);
	}

	void SetYScale(int pos, double o)
	{
		planes[pos].xform.yscale = FLOAT2FIXED(o);
	}

	fixed_t GetYScale(int pos) const
	{
		return planes[pos].xform.yscale;
	}

	double GetYScaleF(int pos) const
	{
		return FIXED2DBL(planes[pos].xform.yscale);
	}

	void SetAngle(int pos, DAngle o)
	{
		planes[pos].xform.angle = o.BAMs();
	}

	angle_t GetAngle(int pos, bool addbase = true) const
	{
		if (!addbase)
		{
			return planes[pos].xform.angle;
		}
		else
		{
			return planes[pos].xform.angle + planes[pos].xform.base_angle;
		}
	}

	DAngle GetAngleF(int pos, bool addbase = true) const
	{
		if (!addbase)
		{
			return ANGLE2DBL(planes[pos].xform.angle);
		}
		else
		{
			return ANGLE2DBL(planes[pos].xform.angle + planes[pos].xform.base_angle);
		}
	}

	void SetBase(int pos, double y, DAngle o)
	{
		planes[pos].xform.base_yoffs = FLOAT2FIXED(y);
		planes[pos].xform.base_angle = o.BAMs();
	}

	void SetAlpha(int pos, double o)
	{
		planes[pos].alpha = FLOAT2FIXED(o);
	}

	fixed_t GetAlpha(int pos) const
	{
		return planes[pos].alpha;
	}

	double GetAlphaF(int pos) const
	{
		return FIXED2DBL(planes[pos].alpha);
	}

	int GetFlags(int pos) const
	{
		return planes[pos].Flags;
	}

	void ChangeFlags(int pos, int And, int Or)
	{
		planes[pos].Flags &= ~And;
		planes[pos].Flags |= Or;
	}

	int GetPlaneLight(int pos) const 
	{
		return planes[pos].Light;
	}

	void SetPlaneLight(int pos, int level)
	{
		planes[pos].Light = level;
	}

	FTextureID GetTexture(int pos) const
	{
		return planes[pos].Texture;
	}

	void SetTexture(int pos, FTextureID tex, bool floorclip = true)
	{
		FTextureID old = planes[pos].Texture;
		planes[pos].Texture = tex;
		if (floorclip && pos == floor && tex != old) AdjustFloorClip();
	}

	fixed_t GetPlaneTexZ(int pos) const
	{
		return planes[pos].TexZ;
	}

	double GetPlaneTexZF(int pos) const
	{
		return FIXED2DBL(planes[pos].TexZ);
	}

	void SetPlaneTexZ(int pos, double val, bool dirtify = false)	// This mainly gets used by init code. The only place where it must set the vertex to dirty is the interpolation code.
	{
		planes[pos].TexZ = FLOAT2FIXED(val);
		if (dirtify) SetAllVerticesDirty();
	}

	void ChangePlaneTexZ(int pos, double val)
	{
		planes[pos].TexZ += FLOAT2FIXED(val);
	}

	static inline short ClampLight(int level)
	{
		return (short)clamp(level, SHRT_MIN, SHRT_MAX);
	}

	void ChangeLightLevel(int newval)
	{
		lightlevel = ClampLight(lightlevel + newval);
	}

	void SetLightLevel(int newval)
	{
		lightlevel = ClampLight(newval);
	}

	int GetLightLevel() const
	{
		return lightlevel;
	}

	secplane_t &GetSecPlane(int pos)
	{
		return pos == floor? floorplane:ceilingplane;
	}

	bool isSecret() const
	{
		return !!(Flags & SECF_SECRET);
	}

	bool wasSecret() const
	{
		return !!(Flags & SECF_WASSECRET);
	}

	void ClearSecret()
	{
		Flags &= ~SECF_SECRET;
	}

	void ClearSpecial()
	{
		// clears all variables that originate from 'special'. Used for sector type transferring thinkers
		special = 0;
		damageamount = 0;
		damageinterval = 0;
		damagetype = NAME_None;
		leakydamage = 0;
		Flags &= ~SECF_SPECIALFLAGS;
	}

	bool PortalBlocksView(int plane)
	{
		if (GetPortalType(plane) != PORTS_LINKEDPORTAL) return false;
		return !!(planes[plane].Flags & (PLANEF_NORENDER | PLANEF_DISABLED | PLANEF_OBSTRUCTED));
	}

	bool PortalBlocksSight(int plane)
	{
		return PLANEF_LINKED != (planes[plane].Flags & (PLANEF_NORENDER | PLANEF_NOPASS | PLANEF_DISABLED | PLANEF_OBSTRUCTED | PLANEF_LINKED));
	}

	bool PortalBlocksMovement(int plane)
	{
		return PLANEF_LINKED != (planes[plane].Flags & (PLANEF_NOPASS | PLANEF_DISABLED | PLANEF_OBSTRUCTED | PLANEF_LINKED));
	}

	bool PortalBlocksSound(int plane)
	{
		return PLANEF_LINKED != (planes[plane].Flags & (PLANEF_BLOCKSOUND | PLANEF_DISABLED | PLANEF_OBSTRUCTED | PLANEF_LINKED));
	}

	bool PortalIsLinked(int plane)
	{
		return (GetPortalType(plane) == PORTS_LINKEDPORTAL);
	}

	void ClearPortal(int plane)
	{
		Portals[plane] = 0;
		portals[plane] = nullptr;
	}

	FSectorPortal *GetPortal(int plane)
	{
		return &sectorPortals[Portals[plane]];
	}

	double GetPortalPlaneZ(int plane)
	{
		return sectorPortals[Portals[plane]].mPlaneZ;
	}

	DVector2 GetPortalDisplacement(int plane)
	{
		return sectorPortals[Portals[plane]].mDisplacement;
	}

	int GetPortalType(int plane)
	{
		return sectorPortals[Portals[plane]].mType;
	}

	int GetOppositePortalGroup(int plane)
	{
		return sectorPortals[Portals[plane]].mDestination->PortalGroup;
	}

	void SetVerticesDirty()	
	{
		for (unsigned i = 0; i < e->vertices.Size(); i++) e->vertices[i]->dirty = true;
	}

	void SetAllVerticesDirty()
	{
		SetVerticesDirty();
		for (unsigned i = 0; i < e->FakeFloor.Sectors.Size(); i++) e->FakeFloor.Sectors[i]->SetVerticesDirty();
		for (unsigned i = 0; i < e->XFloor.attached.Size(); i++) e->XFloor.attached[i]->SetVerticesDirty();
	}

	int GetTerrain(int pos) const;

	void TransferSpecial(sector_t *model);
	void GetSpecial(secspecial_t *spec);
	void SetSpecial(const secspecial_t *spec);
	bool PlaneMoving(int pos);

	// Portal-aware height calculation
	double HighestCeilingAt(const DVector2 &a, sector_t **resultsec = NULL);
	double LowestFloorAt(const DVector2 &a, sector_t **resultsec = NULL);


	double HighestCeilingAt(AActor *a, sector_t **resultsec = NULL)
	{
		return HighestCeilingAt(a->Pos(), resultsec);
	}

	double LowestFloorAt(AActor *a, sector_t **resultsec = NULL)
	{
		return LowestFloorAt(a->Pos(), resultsec);
	}

	double NextHighestCeilingAt(double x, double y, double bottomz, double topz, int flags = 0, sector_t **resultsec = NULL, F3DFloor **resultffloor = NULL);
	double NextLowestFloorAt(double x, double y, double z, int flags = 0, double steph = 0, sector_t **resultsec = NULL, F3DFloor **resultffloor = NULL);

	// Member variables
	double		CenterFloor() const { return floorplane.ZatPoint(centerspot); }
	double		CenterCeiling() const { return ceilingplane.ZatPoint(centerspot); }

	// [RH] store floor and ceiling planes instead of heights
	secplane_t	floorplane, ceilingplane;

	// [RH] give floor and ceiling even more properties
	FDynamicColormap *ColorMap;	// [RH] Per-sector colormap


	TObjPtr<AActor> SoundTarget;

	short		special;
	short		lightlevel;
	short		seqType;		// this sector's sound sequence

	int			sky;
	FNameNoInit	SeqName;		// Sound sequence name. Setting seqType non-negative will override this.

	DVector2	centerspot;		// origin for any sounds played by the sector
	int 		validcount;		// if == validcount, already checked
	AActor* 	thinglist;		// list of mobjs in sector

	// killough 8/28/98: friction is a sector property, not an mobj property.
	// these fields used to be in AActor, but presented performance problems
	// when processed as mobj properties. Fix is to make them sector properties.
	double		friction, movefactor;

	int			terrainnum[2];

	// thinker_t for reversable actions
	TObjPtr<DSectorEffect> floordata;			// jff 2/22/98 make thinkers on
	TObjPtr<DSectorEffect> ceilingdata;			// floors, ceilings, lighting,
	TObjPtr<DSectorEffect> lightingdata;		// independent of one another

	enum
	{
		CeilingMove,
		FloorMove,
		CeilingScroll,
		FloorScroll
	};
	TObjPtr<DInterpolation> interpolations[4];

	BYTE 		soundtraversed;	// 0 = untraversed, 1,2 = sndlines -1
	// jff 2/26/98 lockout machinery for stairbuilding
	SBYTE stairlock;	// -2 on first locked -1 after thinker done 0 normally
	SWORD prevsec;		// -1 or number of sector for previous step
	SWORD nextsec;		// -1 or number of next step sector

	short linecount;
	struct line_t **lines;		// [linecount] size

	// killough 3/7/98: support flat heights drawn at another sector's heights
	sector_t *heightsec;		// other sector, or NULL if no other sector

	DWORD bottommap, midmap, topmap;	// killough 4/4/98: dynamic colormaps
										// [RH] these can also be blend values if
										//		the alpha mask is non-zero

	// list of mobjs that are at least partially in the sector
	// thinglist is a subset of touching_thinglist
	struct msecnode_t *touching_thinglist;				// phares 3/14/98
	struct msecnode_t *render_thinglist;				// for cross-portal rendering.

	double gravity;			// [RH] Sector gravity (1.0 is normal)
	FNameNoInit damagetype;		// [RH] Means-of-death for applied damage
	int damageamount;			// [RH] Damage to do while standing on floor
	short damageinterval;	// Interval for damage application
	short leakydamage;		// chance of leaking through radiation suit

	WORD ZoneNumber;	// [RH] Zone this sector belongs to
	WORD MoreFlags;		// [RH] Internal sector flags
	DWORD Flags;		// Sector flags

	// [RH] Action specials for sectors. Like Skull Tag, but more
	// flexible in a Bloody way. SecActTarget forms a list of actors
	// joined by their tracer fields. When a potential sector action
	// occurs, SecActTarget's TriggerAction method is called.
	TObjPtr<ASectorAction> SecActTarget;

	// [RH] The portal or skybox to render for this sector.
	unsigned Portals[2];
	int PortalGroup;

	int							sectornum;			// for comparing sector copies

	extsector_t	*				e;		// This stores data that requires construction/destruction. Such data must not be copied by R_FakeFlat.

	// GL only stuff starts here
	float						reflect[2];

	bool						transdoor;			// For transparent door hacks
	fixed_t						transdoorheight;	// for transparent door hacks
	int							subsectorcount;		// list of subsectors
	subsector_t **				subsectors;
	FPortal *					portals[2];			// floor and ceiling portals

	enum
	{
		vbo_fakefloor = floor+2,
		vbo_fakeceiling = ceiling+2,
	};

	int				vboindex[4];	// VBO indices of the 4 planes this sector uses during rendering
	fixed_t			vboheight[2];	// Last calculated height for the 2 planes of this actual sector
	int				vbocount[2];	// Total count of vertices belonging to this sector's planes

	float GetReflect(int pos) { return gl_plane_reflection_i? reflect[pos] : 0; }
	bool VBOHeightcheck(int pos) const { return vboheight[pos] == GetPlaneTexZ(pos); }
	FPortal *GetGLPortal(int plane) { return portals[plane]; }

	enum
	{
		INVALIDATE_PLANES = 1,
		INVALIDATE_OTHER = 2
	};

};

FArchive &operator<< (FArchive &arc, sector_t::splane &p);


struct ReverbContainer;
struct zone_t
{
	ReverbContainer *Environment;
};


//
// The SideDef.
//

class DBaseDecal;

enum
{
	WALLF_ABSLIGHTING	 = 1,	// Light is absolute instead of relative
	WALLF_NOAUTODECALS	 = 2,	// Do not attach impact decals to this wall
	WALLF_NOFAKECONTRAST = 4,	// Don't do fake contrast for this wall in side_t::GetLightLevel
	WALLF_SMOOTHLIGHTING = 8,   // Similar to autocontrast but applies to all angles.
	WALLF_CLIP_MIDTEX	 = 16,	// Like the line counterpart, but only for this side.
	WALLF_WRAP_MIDTEX	 = 32,	// Like the line counterpart, but only for this side.
	WALLF_POLYOBJ		 = 64,	// This wall belongs to a polyobject.
	WALLF_LIGHT_FOG      = 128,	// This wall's Light is used even in fog.
};

struct side_t
{
	enum ETexpart
	{
		top=0,
		mid=1,
		bottom=2
	};
	struct part
	{
		fixed_t xoffset;
		fixed_t yoffset;
		fixed_t xscale;
		fixed_t yscale;
		FTextureID texture;
		TObjPtr<DInterpolation> interpolation;
		//int Light;
	};

	sector_t*	sector;			// Sector the SideDef is facing.
	DBaseDecal*	AttachedDecals;	// [RH] Decals bound to the wall
	part		textures[3];
	line_t		*linedef;
	//DWORD		linenum;
	DWORD		LeftSide, RightSide;	// [RH] Group walls into loops
	WORD		TexelLength;
	SWORD		Light;
	BYTE		Flags;
	int			Index;		// needed to access custom UDMF fields which are stored in loading order.

	int GetLightLevel (bool foggy, int baselight, bool is3dlight=false, int *pfakecontrast_usedbygzdoom=NULL) const;

	void SetLight(SWORD l)
	{
		Light = l;
	}

	FTextureID GetTexture(int which) const
	{
		return textures[which].texture;
	}
	void SetTexture(int which, FTextureID tex)
	{
		textures[which].texture = tex;
	}

	void SetTextureXOffset(int which, fixed_t offset)
	{
		textures[which].xoffset = offset;
	}
	void SetTextureXOffset(fixed_t offset)
	{
		textures[top].xoffset =
		textures[mid].xoffset =
		textures[bottom].xoffset = offset;
	}
	void SetTextureXOffset(int which, double offset)
	{
		textures[which].xoffset = FLOAT2FIXED(offset);
	}
	void SetTextureXOffset(double offset)
	{
		textures[top].xoffset =
		textures[mid].xoffset =
		textures[bottom].xoffset = FLOAT2FIXED(offset);
	}
	fixed_t GetTextureXOffset(int which) const
	{
		return textures[which].xoffset;
	}
	double GetTextureXOffsetF(int which) const
	{
		return FIXED2DBL(textures[which].xoffset);
	}
	void AddTextureXOffset(int which, fixed_t delta)
	{
		textures[which].xoffset += delta;
	}
	void AddTextureXOffset(int which, double delta)
	{
		textures[which].xoffset += FLOAT2FIXED(delta);
	}

	void SetTextureYOffset(int which, fixed_t offset)
	{
		textures[which].yoffset = offset;
	}
	void SetTextureYOffset(fixed_t offset)
	{
		textures[top].yoffset =
		textures[mid].yoffset =
		textures[bottom].yoffset = offset;
	}
	void SetTextureYOffset(int which, double offset)
	{
		textures[which].yoffset = FLOAT2FIXED(offset);
	}
	void SetTextureYOffset(double offset)
	{
		textures[top].yoffset =
		textures[mid].yoffset =
		textures[bottom].yoffset = FLOAT2FIXED(offset);
	}
	fixed_t GetTextureYOffset(int which) const
	{
		return textures[which].yoffset;
	}
	double GetTextureYOffsetF(int which) const
	{
		return FIXED2DBL(textures[which].yoffset);
	}
	void AddTextureYOffset(int which, fixed_t delta)
	{
		textures[which].yoffset += delta;
	}
	void AddTextureYOffset(int which, double delta)
	{
		textures[which].yoffset += FLOAT2FIXED(delta);
	}

	void SetTextureXScale(int which, fixed_t scale)
	{
		textures[which].xscale = scale == 0 ? FRACUNIT : scale;
	}
	void SetTextureXScale(int which, double scale)
	{
		textures[which].xscale = scale == 0 ? FRACUNIT : FLOAT2FIXED(scale);
	}
	void SetTextureXScale(fixed_t scale)
	{
		textures[top].xscale = textures[mid].xscale = textures[bottom].xscale = scale == 0 ? FRACUNIT : scale;
	}
	void SetTextureXScale(double scale)
	{
		textures[top].xscale = textures[mid].xscale = textures[bottom].xscale = scale == 0 ? FRACUNIT : FLOAT2FIXED(scale);
	}
	fixed_t GetTextureXScale(int which) const
	{
		return textures[which].xscale;
	}
	double GetTextureXScaleF(int which) const
	{
		return FIXED2DBL(textures[which].xscale);
	}
	void MultiplyTextureXScale(int which, double delta)
	{
		textures[which].xscale = fixed_t(textures[which].xscale * delta);
	}


	void SetTextureYScale(int which, fixed_t scale)
	{
		textures[which].yscale = scale == 0 ? FRACUNIT : scale;
	}

	void SetTextureYScale(int which, double scale)
	{
		textures[which].yscale = scale == 0 ? FRACUNIT : FLOAT2FIXED(scale);
	}

	void SetTextureYScale(fixed_t scale)
	{
		textures[top].yscale = textures[mid].yscale = textures[bottom].yscale = scale == 0 ? FRACUNIT : scale;
	}
	void SetTextureYScale(double scale)
	{
		textures[top].yscale = textures[mid].yscale = textures[bottom].yscale = scale == 0 ? FRACUNIT : FLOAT2FIXED(scale);
	}
	fixed_t GetTextureYScale(int which) const
	{
		return textures[which].yscale;
	}
	double GetTextureYScaleF(int which) const
	{
		return FIXED2DBL(textures[which].yscale);
	}
	void MultiplyTextureYScale(int which, double delta)
	{
		textures[which].yscale = fixed_t(textures[which].yscale * delta);
	}

	DInterpolation *SetInterpolation(int position);
	void StopInterpolation(int position);

	vertex_t *V1() const;
	vertex_t *V2() const;

	//For GL
	FLightNode * lighthead[2];				// all blended lights that may affect this wall

	seg_t **segs;	// all segs belonging to this sidedef in ascending order. Used for precise rendering
	int numsegs;

};

FArchive &operator<< (FArchive &arc, side_t::part &p);

struct line_t
{
	vertex_t	*v1, *v2;	// vertices, from v1 to v2
private:
	DVector2	delta;		// precalculated v2 - v1 for side checking
public:
	DWORD		flags;
	DWORD		activation;	// activation type
	int			special;
	fixed_t		Alpha;		// <--- translucency (0=invisibile, FRACUNIT=opaque)
	int			args[5];	// <--- hexen-style arguments (expanded to ZDoom's full width)
	side_t		*sidedef[2];
	double		bbox[4];	// bounding box, for the extent of the LineDef.
	sector_t	*frontsector, *backsector;
	int 		validcount;	// if == validcount, already checked
	int			locknumber;	// [Dusk] lock number for special
	unsigned	portalindex;
	unsigned	portaltransferred;

	DVector2 Delta() const
	{
		return delta;
	}

	void setDelta(double x, double y)
	{
		delta = { x, y };
	}

	void setAlpha(double a)
	{
		Alpha = FLOAT2FIXED(a);
	}

	FSectorPortal *GetTransferredPortal()
	{
		return portaltransferred >= sectorPortals.Size() ? (FSectorPortal*)NULL : &sectorPortals[portaltransferred];
	}

	FLinePortal *getPortal() const
	{
		return portalindex >= linePortals.Size() ? (FLinePortal*)NULL : &linePortals[portalindex];
	}

	// returns true if the portal is crossable by actors
	bool isLinePortal() const
	{
		return portalindex >= linePortals.Size() ? false : !!(linePortals[portalindex].mFlags & PORTF_PASSABLE);
	}

	// returns true if the portal needs to be handled by the renderer
	bool isVisualPortal() const
	{
		return portalindex >= linePortals.Size() ? false : !!(linePortals[portalindex].mFlags & PORTF_VISIBLE);
	}

	line_t *getPortalDestination() const
	{
		return portalindex >= linePortals.Size() ? (line_t*)NULL : linePortals[portalindex].mDestination;
	}

	int getPortalAlignment() const
	{
		return portalindex >= linePortals.Size() ? 0 : linePortals[portalindex].mAlign;
	}
};

// phares 3/14/98
//
// Sector list node showing all sectors an object appears in.
//
// There are two threads that flow through these nodes. The first thread
// starts at touching_thinglist in a sector_t and flows through the m_snext
// links to find all mobjs that are entirely or partially in the sector.
// The second thread starts at touching_sectorlist in a AActor and flows
// through the m_tnext links to find all sectors a thing touches. This is
// useful when applying friction or push effects to sectors. These effects
// can be done as thinkers that act upon all objects touching their sectors.
// As an mobj moves through the world, these nodes are created and
// destroyed, with the links changed appropriately.
//
// For the links, NULL means top or end of list.

struct msecnode_t
{
	sector_t			*m_sector;	// a sector containing this object
	AActor				*m_thing;	// this object
	struct msecnode_t	*m_tprev;	// prev msecnode_t for this thing
	struct msecnode_t	*m_tnext;	// next msecnode_t for this thing
	struct msecnode_t	*m_sprev;	// prev msecnode_t for this sector
	struct msecnode_t	*m_snext;	// next msecnode_t for this sector
	bool visited;	// killough 4/4/98, 4/7/98: used in search algorithms
};

// use the same memory layout as msecnode_t so both can be used from the same freelist.
struct portnode_t
{
	FLinePortal			*m_portal;	// a portal containing this object
	AActor				*m_thing;	// this object
	struct portnode_t	*m_tprev;	// prev msecnode_t for this thing
	struct portnode_t	*m_tnext;	// next msecnode_t for this thing
	struct portnode_t	*m_sprev;	// prev msecnode_t for this portal
	struct portnode_t	*m_snext;	// next msecnode_t for this portal
	bool visited;
};

struct FPolyNode;
struct FMiniBSP;

//
// The LineSeg.
//
struct seg_t
{
	vertex_t*	v1;
	vertex_t*	v2;
	
	side_t* 	sidedef;
	line_t* 	linedef;

	// Sector references. Could be retrieved from linedef, too.
	sector_t*		frontsector;
	sector_t*		backsector;		// NULL for one-sided lines

	seg_t*			PartnerSeg;
	subsector_t*	Subsector;

	float			sidefrac;		// relative position of seg's ending vertex on owning sidedef
};

struct glsegextra_t
{
	DWORD		 PartnerSeg;
	subsector_t *Subsector;
};

extern seg_t *segs;


//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs indicating the visible walls that
// define (all or some) sides of a convex BSP leaf.
//

enum
{
	SSECF_DEGENERATE = 1,
	SSECF_DRAWN = 2,
	SSECF_POLYORG = 4,
};

struct FPortalCoverage
{
	DWORD *		subsectors;
	int			sscount;
};

struct subsector_t
{
	sector_t	*sector;
	FPolyNode	*polys;
	FMiniBSP	*BSP;
	seg_t		*firstline;
	sector_t	*render_sector;
	DWORD		numlines;
	int			flags;

	void BuildPolyBSP();
	// subsector related GL data
	FLightNode *	lighthead[2];	// Light nodes (blended and additive)
	int				validcount;
	short			mapsection;
	char			hacked;			// 1: is part of a render hack
									// 2: has one-sided walls
	FPortalCoverage	portalcoverage[2];
};


	

//
// BSP node.
//
struct node_t
{
	// Partition line.
	fixed_t		x;
	fixed_t		y;
	fixed_t		dx;
	fixed_t		dy;
	fixed_t		bbox[2][4];		// Bounding box for each child.
	float		len;
	union
	{
		void	*children[2];	// If bit 0 is set, it's a subsector.
		int		intchildren[2];	// Used by nodebuilder.
	};
};


// An entire BSP tree.

struct FMiniBSP
{
	bool bDirty;

	TArray<node_t> Nodes;
	TArray<seg_t> Segs;
	TArray<subsector_t> Subsectors;
	TArray<vertex_t> Verts;
};



//
// OTHER TYPES
//

typedef BYTE lighttable_t;	// This could be wider for >8 bit display.

// This encapsulates the fields of vissprite_t that can be altered by AlterWeaponSprite
struct visstyle_t
{
	lighttable_t	*colormap;
	float			Alpha;
	FRenderStyle	RenderStyle;
};


//----------------------------------------------------------------------------------
//
// The playsim can use different nodes than the renderer so this is
// not the same as R_PointInSubsector
//
//----------------------------------------------------------------------------------
subsector_t *P_PointInSubsector(double x, double y);

inline sector_t *P_PointInSector(const DVector2 &pos)
{
	return P_PointInSubsector(pos.X, pos.Y)->sector;
}

inline sector_t *P_PointInSector(double X, double Y)
{
	return P_PointInSubsector(X, Y)->sector;
}

inline DVector3 AActor::PosRelative(int portalgroup) const
{
	return Pos() + Displacements.getOffset(Sector->PortalGroup, portalgroup);
}

inline DVector3 AActor::PosRelative(const AActor *other) const
{
	return Pos() + Displacements.getOffset(Sector->PortalGroup, other->Sector->PortalGroup);
}

inline DVector3 AActor::PosRelative(sector_t *sec) const
{
	return Pos() + Displacements.getOffset(Sector->PortalGroup, sec->PortalGroup);
}

inline DVector3 AActor::PosRelative(line_t *line) const
{
	return Pos() + Displacements.getOffset(Sector->PortalGroup, line->frontsector->PortalGroup);
}

inline DVector3 PosRelative(const DVector3 &pos, line_t *line, sector_t *refsec = NULL)
{
	return pos + Displacements.getOffset(refsec->PortalGroup, line->frontsector->PortalGroup);
}


inline void AActor::ClearInterpolation()
{
	Prev = Pos();
	PrevAngles = Angles;
	if (Sector) PrevPortalGroup = Sector->PortalGroup;
	else PrevPortalGroup = 0;
}

inline bool FBoundingBox::inRange(const line_t *ld) const
{
	return Left() < ld->bbox[BOXRIGHT] &&
		Right() > ld->bbox[BOXLEFT] &&
		Top() > ld->bbox[BOXBOTTOM] &&
		Bottom() < ld->bbox[BOXTOP];
}



#endif
