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

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
// SECTORS do store MObjs anyway.
#include "actor.h"

#include "dthinker.h"
#include "farchive.h"

#define MAXWIDTH 2560
#define MAXHEIGHT 1600

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


//
// INTERNAL MAP TYPES
//	used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
//	like some DOOM-alikes ("wt", "WebView") did.
//
struct vertex_t
{
	fixed_t x, y;

	bool operator== (const vertex_t &other)
	{
		return x == other.x && y == other.y;
	}
};

// Forward of LineDefs, for Sectors.
struct line_t;

class player_t;
class FScanner;
class FBitmap;
struct FCopyInfo;
class DInterpolation;

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
		IntVal = strtol(val, NULL, 0);
		FloatVal = strtod(val, NULL);
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
struct line_t;
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
	void Destroy ();
	void BeginPlay ();
	void Activate (AActor *source);
	void Deactivate (AActor *source);
	virtual bool TriggerAction (AActor *triggerer, int activationType);
protected:
	bool CheckTrigger (AActor *triggerer) const;
};

class ASkyViewpoint;

struct secplane_t
{
	// the plane is defined as a*x + b*y + c*z + d = 0
	// ic is 1/c, for faster Z calculations

	fixed_t a, b, c, d, ic;

	// Returns the value of z at (x,y)
	fixed_t ZatPoint (fixed_t x, fixed_t y) const
	{
		return FixedMul (ic, -d - DMulScale16 (a, x, b, y));
	}

	// Returns the value of z at (x,y) as a double
	double ZatPoint (double x, double y) const
	{
		return (d + a*x + b*y) * ic / (-65536.0 * 65536.0);
	}

	// Returns the value of z at vertex v
	fixed_t ZatPoint (const vertex_t *v) const
	{
		return FixedMul (ic, -d - DMulScale16 (a, v->x, b, v->y));
	}

	// Returns the value of z at (x,y) if d is equal to dist
	fixed_t ZatPointDist (fixed_t x, fixed_t y, fixed_t dist) const
	{
		return FixedMul (ic, -dist - DMulScale16 (a, x, b, y));
	}

	// Returns the value of z at vertex v if d is equal to dist
	fixed_t ZatPointDist (const vertex_t *v, fixed_t dist)
	{
		return FixedMul (ic, -dist - DMulScale16 (a, v->x, b, v->y));
	}

	// Flips the plane's vertical orientiation, so that if it pointed up,
	// it will point down, and vice versa.
	void FlipVert ()
	{
		a = -a;
		b = -b;
		c = -c;
		d = -d;
		ic = -ic;
	}

	// Returns true if 2 planes are the same
	bool operator== (const secplane_t &other) const
	{
		return a == other.a && b == other.b && c == other.c && d == other.d;
	}

	// Returns true if 2 planes are different
	bool operator!= (const secplane_t &other) const
	{
		return a != other.a || b != other.b || c != other.c || d != other.d;
	}

	// Moves a plane up/down by hdiff units
	void ChangeHeight (fixed_t hdiff)
	{
		d = d - FixedMul (hdiff, c);
	}

	// Moves a plane up/down by hdiff units
	fixed_t GetChangedHeight (fixed_t hdiff)
	{
		return d - FixedMul (hdiff, c);
	}

	// Returns how much this plane's height would change if d were set to oldd
	fixed_t HeightDiff (fixed_t oldd) const
	{
		return FixedMul (oldd - d, ic);
	}

	// Returns how much this plane's height would change if d were set to oldd
	fixed_t HeightDiff (fixed_t oldd, fixed_t newd) const
	{
		return FixedMul (oldd - newd, ic);
	}

	fixed_t PointToDist (fixed_t x, fixed_t y, fixed_t z) const
	{
		return -TMulScale16 (a, x, y, b, z, c);
	}

	fixed_t PointToDist (const vertex_t *v, fixed_t z) const
	{
		return -TMulScale16 (a, v->x, b, v->y, z, c);
	}
};

inline FArchive &operator<< (FArchive &arc, secplane_t &plane)
{
	arc << plane.a << plane.b << plane.c << plane.d;
	//if (plane.c != 0)
	{	// plane.c should always be non-0. Otherwise, the plane
		// would be perfectly vertical.
		plane.ic = DivScale32 (1, plane.c);
	}
	return arc;
}

#include "p_3dfloors.h"
// Ceiling/floor flags
enum
{
	PLANEF_ABSLIGHTING	= 1,	// floor/ceiling light is absolute, not relative
	PLANEF_BLOCKED		= 2		// can not be moved anymore.
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
};

enum
{
	SECF_SILENT			= 1,	// actors in sector make no noise
	SECF_NOFALLINGDAMAGE= 2,	// No falling damage in this sector
	SECF_FLOORDROP		= 4,	// all actors standing on this floor will remain on it when it lowers very fast.
	SECF_NORESPAWN		= 8,	// players can not respawn in this sector
};

enum
{
	PL_SKYFLAT = 0x40000000
};

struct FDynamicColormap;

struct FLightStack
{
	secplane_t Plane;		// Plane above this light (points up)
	sector_t *Master;		// Sector to get light from (NULL for owner)
	BITFIELD bBottom:1;		// Light is from the bottom of a block?
	BITFIELD bFlooder:1;	// Light floods lower lights until another flooder is reached?
	BITFIELD bOverlaps:1;	// Plane overlaps the next one
};

struct FExtraLight
{
	short Tag;
	WORD NumLights;
	WORD NumUsedLights;
	FLightStack *Lights;	// Lights arranged from top to bottom

	void InsertLight (const secplane_t &plane, line_t *line, int type);
};

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

struct sector_t
{
	// Member functions
	bool IsLinked(sector_t *other, bool ceiling) const;
	fixed_t FindLowestFloorSurrounding (vertex_t **v) const;
	fixed_t FindHighestFloorSurrounding (vertex_t **v) const;
	fixed_t FindNextHighestFloor (vertex_t **v) const;
	fixed_t FindNextLowestFloor (vertex_t **v) const;
	fixed_t FindLowestCeilingSurrounding (vertex_t **v) const;			// jff 2/04/98
	fixed_t FindHighestCeilingSurrounding (vertex_t **v) const;			// jff 2/04/98
	fixed_t FindNextLowestCeiling (vertex_t **v) const;					// jff 2/04/98
	fixed_t FindNextHighestCeiling (vertex_t **v) const;				// jff 2/04/98
	fixed_t FindShortestTextureAround () const;							// jff 2/04/98
	fixed_t FindShortestUpperAround () const;							// jff 2/04/98
	sector_t *FindModelFloorSector (fixed_t floordestheight) const;		// jff 2/04/98
	sector_t *FindModelCeilingSector (fixed_t floordestheight) const;	// jff 2/04/98
	int FindMinSurroundingLight (int max) const;
	sector_t *NextSpecialSector (int type, sector_t *prev) const;		// [RH]
	fixed_t FindLowestCeilingPoint (vertex_t **v) const;
	fixed_t FindHighestFloorPoint (vertex_t **v) const;
	void AdjustFloorClip () const;
	void SetColor(int r, int g, int b, int desat);
	void SetFade(int r, int g, int b);
	void ClosestPoint(fixed_t x, fixed_t y, fixed_t &ox, fixed_t &oy) const;

	DInterpolation *SetInterpolation(int position, bool attach);
	void StopInterpolation(int position);

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
		FTextureID Texture;
		fixed_t TexZ;
	};


	splane planes[2];

	void SetXOffset(int pos, fixed_t o)
	{
		planes[pos].xform.xoffs = o;
	}

	void AddXOffset(int pos, fixed_t o)
	{
		planes[pos].xform.xoffs += o;
	}

	fixed_t GetXOffset(int pos) const
	{
		return planes[pos].xform.xoffs;
	}

	void SetYOffset(int pos, fixed_t o)
	{
		planes[pos].xform.yoffs = o;
	}

	void AddYOffset(int pos, fixed_t o)
	{
		planes[pos].xform.yoffs += o;
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

	void SetXScale(int pos, fixed_t o)
	{
		planes[pos].xform.xscale = o;
	}

	fixed_t GetXScale(int pos) const
	{
		return planes[pos].xform.xscale;
	}

	void SetYScale(int pos, fixed_t o)
	{
		planes[pos].xform.yscale = o;
	}

	fixed_t GetYScale(int pos) const
	{
		return planes[pos].xform.yscale;
	}

	void SetAngle(int pos, angle_t o)
	{
		planes[pos].xform.angle = o;
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

	void SetBase(int pos, fixed_t y, angle_t o)
	{
		planes[pos].xform.base_yoffs = y;
		planes[pos].xform.base_angle = o;
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

	void SetPlaneTexZ(int pos, fixed_t val)
	{
		planes[pos].TexZ = val;
	}

	void ChangePlaneTexZ(int pos, fixed_t val)
	{
		planes[pos].TexZ += val;
	}

	sector_t *GetHeightSec() const 
	{
		return (heightsec && !(heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))? heightsec : NULL;
	}

	void ChangeLightLevel(int newval)
	{
		lightlevel = (BYTE)clamp(lightlevel + newval, 0, 255);
	}

	void SetLightLevel(int newval)
	{
		lightlevel = (BYTE)clamp(newval, 0, 255);
	}

	int GetLightLevel() const
	{
		return lightlevel;
	}

	bool PlaneMoving(int pos);


	// Member variables
	fixed_t		CenterFloor () const { return floorplane.ZatPoint (soundorg[0], soundorg[1]); }
	fixed_t		CenterCeiling () const { return ceilingplane.ZatPoint (soundorg[0], soundorg[1]); }

	// [RH] store floor and ceiling planes instead of heights
	secplane_t	floorplane, ceilingplane;

	// [RH] give floor and ceiling even more properties
	FDynamicColormap *ColorMap;	// [RH] Per-sector colormap

	BYTE		lightlevel;

	TObjPtr<AActor> SoundTarget;
	BYTE 		soundtraversed;	// 0 = untraversed, 1,2 = sndlines -1

	short		special;
	short		tag;
	int			nexttag,firsttag;	// killough 1/30/98: improves searches for tags.

	int			sky;
	short		seqType;		// this sector's sound sequence

	fixed_t		soundorg[2];	// origin for any sounds played by the sector
	int 		validcount;		// if == validcount, already checked
	AActor* 	thinglist;		// list of mobjs in sector

	// killough 8/28/98: friction is a sector property, not an mobj property.
	// these fields used to be in AActor, but presented performance problems
	// when processed as mobj properties. Fix is to make them sector properties.
	fixed_t		friction, movefactor;

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

	float gravity;		// [RH] Sector gravity (1.0 is normal)
	short damage;		// [RH] Damage to do while standing on floor
	short mod;			// [RH] Means-of-death for applied damage

	WORD ZoneNumber;	// [RH] Zone this sector belongs to
	WORD MoreFlags;		// [RH] Internal sector flags
	DWORD Flags;		// Sector flags

	// [RH] Action specials for sectors. Like Skull Tag, but more
	// flexible in a Bloody way. SecActTarget forms a list of actors
	// joined by their tracer fields. When a potential sector action
	// occurs, SecActTarget's TriggerAction method is called.
	TObjPtr<ASectorAction> SecActTarget;

	// [RH] The sky box to render for this sector. NULL means use a
	// regular sky.
	TObjPtr<ASkyViewpoint> FloorSkyBox, CeilingSkyBox;

	// Planes that partition this sector into different light zones.
	FExtraLight *ExtraLights;

	vertex_t *Triangle[3];	// Three points that can define a plane
	short						secretsector;		//jff 2/16/98 remembers if sector WAS secret (automap)
	int							sectornum;			// for comparing sector copies

	extsector_t	*				e;		// This stores data that requires construction/destruction. Such data must not be copied by R_FakeFlat.
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

	int GetLightLevel (bool foggy, int baselight) const;

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
	fixed_t GetTextureXOffset(int which) const
	{
		return textures[which].xoffset;
	}
	void AddTextureXOffset(int which, fixed_t delta)
	{
		textures[which].xoffset += delta;
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
	fixed_t GetTextureYOffset(int which) const
	{
		return textures[which].yoffset;
	}
	void AddTextureYOffset(int which, fixed_t delta)
	{
		textures[which].yoffset += delta;
	}

	void SetTextureXScale(int which, fixed_t scale)
	{
		textures[which].xscale = scale <= 0? FRACUNIT : scale;
	}
	void SetTextureXScale(fixed_t scale)
	{
		textures[top].xscale = textures[mid].xscale = textures[bottom].xscale = scale <= 0? FRACUNIT : scale;
	}
	fixed_t GetTextureXScale(int which) const
	{
		return textures[which].xscale;
	}
	void MultiplyTextureXScale(int which, fixed_t delta)
	{
		textures[which].xscale = FixedMul(textures[which].xscale, delta);
	}


	void SetTextureYScale(int which, fixed_t scale)
	{
		textures[which].yscale = scale <= 0? FRACUNIT : scale;
	}
	void SetTextureYScale(fixed_t scale)
	{
		textures[top].yscale = textures[mid].yscale = textures[bottom].yscale = scale <= 0? FRACUNIT : scale;
	}
	fixed_t GetTextureYScale(int which) const
	{
		return textures[which].yscale;
	}
	void MultiplyTextureYScale(int which, fixed_t delta)
	{
		textures[which].yscale = FixedMul(textures[which].yscale, delta);
	}

	DInterpolation *SetInterpolation(int position);
	void StopInterpolation(int position);
};

FArchive &operator<< (FArchive &arc, side_t::part &p);

//
// Move clipping aid for LineDefs.
//
enum slopetype_t
{
	ST_HORIZONTAL,
	ST_VERTICAL,
	ST_POSITIVE,
	ST_NEGATIVE
};


struct line_t
{
	vertex_t	*v1, *v2;	// vertices, from v1 to v2
	fixed_t 	dx, dy;		// precalculated v2 - v1 for side checking
	DWORD		flags;
	DWORD		activation;	// activation type
	int			special;
	fixed_t		Alpha;		// <--- translucency (0-255/255=opaque)
	int			id;			// <--- same as tag or set with Line_SetIdentification
	int			args[5];	// <--- hexen-style arguments (expanded to ZDoom's full width)
	int			firstid, nextid;
	side_t		*sidedef[2];
	//DWORD		sidenum[2];	// sidenum[1] will be NO_SIDE if one sided
	fixed_t		bbox[4];	// bounding box, for the extent of the LineDef.
	slopetype_t	slopetype;	// To aid move clipping.
	sector_t	*frontsector, *backsector;
	int 		validcount;	// if == validcount, already checked
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

//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs indicating the visible walls that
// define (all or some) sides of a convex BSP leaf.
//
struct FPolyObj;
struct subsector_t
{
	sector_t	*sector;
	DWORD		numlines;
	DWORD		firstline;
	FPolyObj	*poly;
	int			validcount;
	fixed_t		CenterX, CenterY;
};

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

	subsector_t*	Subsector;
	seg_t*			PartnerSeg;

	BITFIELD		bPolySeg:1;
};

// ===== Polyobj data =====
struct FPolyObj
{
	int			numsegs;
	seg_t		**segs;
	int			numlines;
	line_t		**lines;
	int			numvertices;
	vertex_t	**vertices;
	fixed_t		startSpot[2];
	vertex_t	*originalPts;	// used as the base for the rotations
	vertex_t	*prevPts; 		// use to restore the old point values
	angle_t		angle;
	int			tag;			// reference tag assigned in HereticEd
	int			bbox[4];
	int			validcount;
	int			crush; 			// should the polyobj attempt to crush mobjs?
	bool		bHurtOnTouch;	// should the polyobj hurt anything it touches?
	int			seqType;
	fixed_t		size;			// polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
	DThinker	*specialdata;	// pointer to a thinker, if the poly is moving
	TObjPtr<DInterpolation> interpolation;

	~FPolyObj();
	DInterpolation *SetInterpolation();
	void StopInterpolation();
};
extern FPolyObj *polyobjs;		// list of all poly-objects on the level

inline FArchive &operator<< (FArchive &arc, FPolyObj *&poly)
{
	return arc.SerializePointer (polyobjs, (BYTE **)&poly, sizeof(FPolyObj));
}

inline FArchive &operator<< (FArchive &arc, const FPolyObj *&poly)
{
	return arc.SerializePointer (polyobjs, (BYTE **)&poly, sizeof(FPolyObj));
}


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
	union
	{
		void	*children[2];	// If bit 0 is set, it's a subsector.
		int		intchildren[2];	// Used by nodebuilder.
	};
};


struct polyblock_t
{
	FPolyObj *polyobj;
	struct polyblock_t *prev;
	struct polyblock_t *next;
};



// posts are runs of non masked source pixels
struct column_t
{
	BYTE		topdelta;		// -1 is the last post in a column
	BYTE		length; 		// length data bytes follows
};



//
// OTHER TYPES
//

typedef BYTE lighttable_t;	// This could be wider for >8 bit display.


// A vissprite_t is a thing
//	that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.
struct vissprite_t
{
	short			x1, x2;
	fixed_t			cx;				// for line side calculation
	fixed_t			gx, gy;			// for fake floor clipping
	fixed_t			gz, gzt;		// global bottom / top for silhouette clipping
	fixed_t			startfrac;		// horizontal position of x1
	fixed_t			xscale, yscale;
	fixed_t			xiscale;		// negative if flipped
	fixed_t			depth;
	fixed_t			idepth;			// 1/z
	fixed_t			texturemid;
	DWORD			FillColor;
	lighttable_t	*colormap;
	sector_t		*heightsec;		// killough 3/27/98: height sector for underwater/fake ceiling
	sector_t		*sector;		// [RH] sector this sprite is in
	fixed_t			alpha;
	fixed_t			floorclip;
	FTexture		*pic;
	short 			renderflags;
	DWORD			Translation;	// [RH] for color translation
	FRenderStyle	RenderStyle;
	BYTE			FakeFlatStat;	// [RH] which side of fake/floor ceiling sprite is on
	BYTE			bSplitSprite;	// [RH] Sprite was split by a drawseg
};

enum
{
	FAKED_Center,
	FAKED_BelowFloor,
	FAKED_AboveCeiling
};

//
// Sprites are patches with a special naming convention so they can be
// recognized by R_InitSprites. The base name is NNNNFx or NNNNFxFx, with
// x indicating the rotation, x = 0, 1-7. The sprite and frame specified
// by a thing_t is range checked at run time.
// A sprite is a patch_t that is assumed to represent a three dimensional
// object and may have multiple rotations pre drawn. Horizontal flipping
// is used to save space, thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used for all views: NNNNF0
//
struct spriteframe_t
{
	FTextureID Texture[16];	// texture to use for view angles 0-15
	WORD Flip;			// flip (1 = flip) to use for view angles 0-15.
};

//
// A sprite definition:
//	a number of animation frames.
//
struct spritedef_t
{
	union
	{
		char name[5];
		DWORD dwName;
	};
	BYTE numframes;
	WORD spriteframes;
};

extern TArray<spriteframe_t> SpriteFrames;

//
// [RH] Internal "skin" definition.
//
class FPlayerSkin
{
public:
	char		name[17];	// 16 chars + NULL
	char		face[4];	// 3 chars ([MH] + NULL so can use as a C string)
	BYTE		gender;		// This skin's gender (not really used)
	BYTE		range0start;
	BYTE		range0end;
	bool		othergame;	// [GRB]
	fixed_t		ScaleX;
	fixed_t		ScaleY;
	int			sprite;
	int			crouchsprite;
	int			namespc;	// namespace for this skin
};

#endif
