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

// Screenwidth.
#include "doomdef.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
// SECTORS do store MObjs anyway.
#include "actor.h"

#include "dthinker.h"



#define MAXWIDTH 1600
#define MAXHEIGHT 1200

// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE				0
#define SIL_BOTTOM				1
#define SIL_TOP 				2
#define SIL_BOTH				3

extern int MaxDrawSegs;





//
// INTERNAL MAP TYPES
//	used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
//	like some DOOM-alikes ("wt", "WebView") did.
//
struct vertex_s
{
	fixed_t x, y;
};
typedef struct vertex_s vertex_t;

// Forward of LineDefs, for Sectors.
struct line_s;

class player_s;

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
struct dyncolormap_s;

class DSectorEffect;
struct sector_t;

enum
{
	SECSPAC_Enter		= 1,	// Trigger when player enters
	SECSPAC_Exit		= 2,	// Trigger when player exits
	SECSPAC_HitFloor	= 4,	// Trigger when player hits floor
	SECSPAC_HitCeiling	= 8,	// Trigger when player hits ceiling
	SECSPAC_Use			= 16,	// Trigger when player uses
	SECSPAC_UseWall		= 32,	// Trigger when player uses a wall
};

class ASectorAction : public AActor
{
	DECLARE_ACTOR (ASectorAction, AActor)
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

	fixed_t ZatPoint (fixed_t x, fixed_t y) const
	{
		return FixedMul (ic, -d - DMulScale16 (a, x, b, y));
	}

	fixed_t ZatPoint (const vertex_t *v) const
	{
		return FixedMul (ic, -d - DMulScale16 (a, v->x, b, v->y));
	}

	fixed_t ZatPointDist (fixed_t x, fixed_t y, fixed_t dist) const
	{
		return FixedMul (ic, -dist - DMulScale16 (a, x, b, y));
	}

	fixed_t ZatPointDist (const vertex_t *v, fixed_t dist)
	{
		return FixedMul (ic, -dist - DMulScale16 (a, v->x, b, v->y));
	}

	bool operator== (const secplane_t &other) const
	{
		return a == other.a && b == other.b && c == other.c && d == other.d;
	}

	bool operator!= (const secplane_t &other) const
	{
		return a != other.a || b != other.b || c != other.c || d != other.d;
	}

	void ChangeHeight (fixed_t hdiff)
	{ // Move the plane up or down by hdiff units
		d = d - FixedMul (hdiff, c);
	}

	fixed_t HeightDiff (fixed_t oldd) const
	{
		return FixedMul (oldd - d, ic);
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
	if (plane.c != 0)
	{
		plane.ic = DivScale32 (1, plane.c);
	}
	return arc;
}

// Ceiling/floor flags
enum
{
	SECF_ABSLIGHTING	= 1		// floor/ceiling light is absolute, not relative
};

// Misc sector flags
enum
{
	SECF_SILENT			= 1		// actors in sector make no noise
};

struct FDynamicColormap;

struct sector_t
{
	// Member functions
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

	// Member variables
	fixed_t		CenterFloor () const { return floorplane.ZatPoint (soundorg[0], soundorg[1]); }
	fixed_t		CenterCeiling () const { return ceilingplane.ZatPoint (soundorg[0], soundorg[1]); }

	// [RH] store floor and ceiling planes instead of heights
	secplane_t	floorplane, ceilingplane;
	fixed_t		floortexz, ceilingtexz;	// [RH] used for wall texture mapping

	// [RH] give floor and ceiling even more properties
	FDynamicColormap *floorcolormap;	// [RH] Per-sector colormap
	FDynamicColormap *ceilingcolormap;

	// killough 3/7/98: floor and ceiling texture offsets
	fixed_t		  floor_xoffs,   floor_yoffs;
	fixed_t		ceiling_xoffs, ceiling_yoffs;

	// [RH] floor and ceiling texture scales
	fixed_t		  floor_xscale,   floor_yscale;
	fixed_t		ceiling_xscale, ceiling_yscale;

	// [RH]		floor and ceiling texture rotation
	angle_t		floor_angle, ceiling_angle;

	fixed_t		base_ceiling_angle, base_ceiling_yoffs;
	fixed_t		base_floor_angle, base_floor_yoffs;

	BYTE		FloorLight, CeilingLight;
	BYTE		FloorFlags, CeilingFlags;
	short		floorpic, ceilingpic;
	BYTE		lightlevel;

	byte 		soundtraversed;	// 0 = untraversed, 1,2 = sndlines -1

	short		special;
	short		tag;
	int			nexttag,firsttag;	// killough 1/30/98: improves searches for tags.

	WORD		sky;
	short		seqType;		// this sector's sound sequence

	AActor* 	soundtarget;	// thing that made a sound (or null)
//	int 		blockbox[4];	// mapblock bounding box for height changes
	fixed_t		soundorg[3];	// origin for any sounds played by the sector
	int 		validcount;		// if == validcount, already checked
	AActor* 	thinglist;		// list of mobjs in sector

	// killough 8/28/98: friction is a sector property, not an mobj property.
	// these fields used to be in AActor, but presented performance problems
	// when processed as mobj properties. Fix is to make them sector properties.
	fixed_t		friction, movefactor;

	// thinker_t for reversable actions
	DSectorEffect *floordata;			// jff 2/22/98 make thinkers on
	DSectorEffect *ceilingdata;			// floors, ceilings, lighting,
	DSectorEffect *lightingdata;		// independent of one another

	// jff 2/26/98 lockout machinery for stairbuilding
	SBYTE stairlock;	// -2 on first locked -1 after thinker done 0 normally
	SWORD prevsec;		// -1 or number of sector for previous step
	SWORD nextsec;		// -1 or number of next step sector

	short linecount;
	struct line_s **lines;		// [linecount] size

	// killough 3/7/98: support flat heights drawn at another sector's heights
	sector_t *heightsec;		// other sector, or NULL if no other sector

	DWORD bottommap, midmap, topmap;	// killough 4/4/98: dynamic colormaps
										// [RH] these can also be blend values if
										//		the alpha mask is non-zero

	// list of mobjs that are at least partially in the sector
	// thinglist is a subset of touching_thinglist
	struct msecnode_s *touching_thinglist;				// phares 3/14/98

	float gravity;		// [RH] Sector gravity (1.0 is normal)
	short damage;		// [RH] Damage to do while standing on floor
	short mod;			// [RH] Means-of-death for applied damage

	byte waterzone;		// [RH] Sector is underwater?
	WORD MoreFlags;		// [RH] Misc sector flags

	// [RH] Action specials for sectors. Like Skull Tag, but more
	// flexible in a Bloody way. SecActTarget forms a list of actors
	// joined by their tracer fields. When a potential sector action
	// occurs, SecActTarget's TriggerAction method is called.
	ASectorAction *SecActTarget;

	// [RH] The sky box to render for this sector. NULL means use a
	// regular sky.
	ASkyViewpoint *SkyBox;
};



//
// The SideDef.
//

enum
{
	WALLF_ABSLIGHTING	= 1,	// Light is absolute instead of relative
	WALLF_NOAUTODECALS	= 2,	// Do not attach impact decals to this wall
};

struct side_s
{
	fixed_t 	textureoffset;	// add this to the calculated texture column
	fixed_t 	rowoffset;		// add this to the calculated texture top
	sector_t*	sector;			// Sector the SideDef is facing.
	AActor*		BoundActors;	// [RH] Wall sprites bound to the wall (aka decals)
	short		toptexture, bottomtexture, midtexture;	// texture indices
	short		linenum;
	short		LeftSide, RightSide;	// [RH] Group walls into loops
	WORD		TexelLength;
	SBYTE		Light;
	BYTE		Flags;
};
typedef struct side_s side_t;


//
// Move clipping aid for LineDefs.
//
typedef enum
{
	ST_HORIZONTAL,
	ST_VERTICAL,
	ST_POSITIVE,
	ST_NEGATIVE
} slopetype_t;

struct line_s
{
	vertex_t	*v1, *v2;	// vertices, from v1 to v2
	fixed_t 	dx, dy;		// precalculated v2 - v1 for side checking
	short		flags;
	byte		special;	// [RH] specials are only one byte (like Hexen)
	byte		alpha;		// <--- translucency (0-255/255=opaque)
	short		id;			// <--- same as tag or set with Line_SetIdentification
	short		args[5];	// <--- hexen-style arguments
							//		note that these are shorts in order to support
							//		the tag parameter from DOOM.
	int			firstid, nextid;
	short		sidenum[2];	// sidenum[1] will be -1 if one sided
	fixed_t		bbox[4];	// bounding box, for the extent of the LineDef.
	slopetype_t	slopetype;	// To aid move clipping.
	sector_t	*frontsector, *backsector;
	int 		validcount;	// if == validcount, already checked

};
typedef struct line_s line_t;

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

typedef struct msecnode_s
{
	sector_t			*m_sector;	// a sector containing this object
	AActor				*m_thing;	// this object
	struct msecnode_s	*m_tprev;	// prev msecnode_t for this thing
	struct msecnode_s	*m_tnext;	// next msecnode_t for this thing
	struct msecnode_s	*m_sprev;	// prev msecnode_t for this sector
	struct msecnode_s	*m_snext;	// next msecnode_t for this sector
	BOOL visited;	// killough 4/4/98, 4/7/98: used in search algorithms
} msecnode_t;

//
// The LineSeg.
//
struct seg_s
{
	vertex_t*	v1;
	vertex_t*	v2;
	
	angle_t 	angle;

	side_t* 	sidedef;
	line_t* 	linedef;

	// Sector references.
	// Could be retrieved from linedef, too.
	sector_t*	frontsector;
	sector_t*	backsector;		// NULL for one-sided lines
	
};
typedef struct seg_s seg_t;


// ===== Polyobj data =====
typedef struct FPolyObj
{
	int			numsegs;
	seg_t		**segs;
	fixed_t		startSpot[3];
	vertex_t	*originalPts;	// used as the base for the rotations
	vertex_t	*prevPts; 		// use to restore the old point values
	angle_t		angle;
	int			tag;			// reference tag assigned in HereticEd
	int			bbox[4];
	int			validcount;
	BOOL		crush; 			// should the polyobj attempt to crush mobjs?
	int			seqType;
	fixed_t		size;			// polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
	DThinker	*specialdata;	// pointer to a thinker, if the poly is moving
} polyobj_t;

typedef struct polyblock_s
{
	polyobj_t *polyobj;
	struct polyblock_s *prev;
	struct polyblock_s *next;
} polyblock_t;

//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs,
//	indicating the visible walls that define
//	(all or some) sides of a convex BSP leaf.
//
typedef struct subsector_s
{
	sector_t	*sector;
	short		numlines;
	short		firstline;
	polyobj_t	*poly;
} subsector_t;

//
// BSP node.
//
struct node_s
{
	// Partition line.
	fixed_t		x;
	fixed_t		y;
	fixed_t		dx;
	fixed_t		dy;
	fixed_t		bbox[2][4];		// Bounding box for each child.
	unsigned short children[2];	// If NF_SUBSECTOR its a subsector.
};
typedef struct node_s node_t;



// posts are runs of non masked source pixels
struct post_s
{
	byte		topdelta;		// -1 is the last post in a column
	byte		length; 		// length data bytes follows
};
typedef struct post_s post_t;

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t	column_t;


//
// OTHER TYPES
//

typedef byte lighttable_t;	// This could be wider for >8 bit display.

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures, and we compose
// textures from the TEXTURE1/2 lists of patches.
struct patch_s
{ 
	short			width;			// bounding box size 
	short			height; 
	short			leftoffset; 	// pixels to the left of origin 
	short			topoffset;		// pixels below the origin 
	int 			columnofs[8];	// only [width] used
	// the [0] is &columnofs[width] 
};
typedef struct patch_s patch_t;


// A vissprite_t is a thing
//	that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.
struct vissprite_s
{
	int				x1, x2;
	fixed_t			gx, gy;			// for line side calculation
	fixed_t			gz, gzt;		// global bottom / top for silhouette clipping
	fixed_t			startfrac;		// horizontal position of x1
	fixed_t			xscale, yscale;
	fixed_t			xiscale;		// negative if flipped
	fixed_t			depth;
	fixed_t			texturemid;
	short			picnum;
	short 			renderflags;
	BYTE			RenderStyle;
	DWORD			AlphaColor;
	lighttable_t	*colormap;
	byte			*translation;	// [RH] for translation;
	sector_t		*heightsec;		// killough 3/27/98: height sector for underwater/fake ceiling
	fixed_t			alpha;
	fixed_t			floorclip;
};
typedef struct vissprite_s vissprite_t;

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
struct spriteframe_s
{
	byte	 	rotate;		// if false, use 0 for any position.
	short		lump[16];	// lump to use for view angles 0-15
	WORD		flip;		// flip (1 = flip) to use for view angles 0-15.
};
typedef struct spriteframe_s spriteframe_t;

//
// A sprite definition:
//	a number of animation frames.
//
struct spritedef_s
{
	char			name[5];
	short 			numframes;
	spriteframe_t	*spriteframes;
};
typedef struct spritedef_s spritedef_t;

//
// [RH] Internal "skin" definition.
//
class FPlayerSkin
{
public:
	char		name[17];	// 16 chars + NULL
	char		face[3];
	byte		gender;		// This skin's gender (not really used)
	byte		range0start;
	byte		range0end;
	int			sprite;
	int			namespc;	// namespace for this skin
};

#endif
