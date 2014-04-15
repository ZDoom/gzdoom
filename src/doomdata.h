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
//	all external data is defined here
//	most of the data is loaded into different structures at run time
//	some internal structures shared by many modules are here
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDATA__
#define __DOOMDATA__

// The most basic types we use, portability.
#include "doomtype.h"

// Some global defines, that configure the game.
#include "doomdef.h"
#include "m_swap.h"

//
// Map level types.
// The following data structures define the persistent format
// used in the lumps of the WAD files.
//

// Lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description.
enum
{
	ML_LABEL, 			// A separator, name, ExMx or MAPxx
	ML_THINGS,			// Monsters, items
	ML_LINEDEFS,		// LineDefs, from editing
	ML_SIDEDEFS,		// SideDefs, from editing
	ML_VERTEXES,		// Vertices, edited and BSP splits generated
	ML_SEGS,			// LineSegs, from LineDefs split by BSP
	ML_SSECTORS,		// SubSectors, list of LineSegs
	ML_NODES, 			// BSP nodes
	ML_SECTORS,			// Sectors, from editing
	ML_REJECT,			// LUT, sector-sector visibility
	ML_BLOCKMAP,		// LUT, motion clipping, walls/grid element
	ML_BEHAVIOR,		// [RH] Hexen-style scripts. If present, THINGS
						//		and LINEDEFS are also Hexen-style.
	ML_CONVERSATION,	// Strife dialog (only for TEXTMAP format)
	ML_MAX,

	// [RH] These are compressed (and extended) nodes. They combine the data from
	// vertexes, segs, ssectors, and nodes into a single lump.
	ML_ZNODES = ML_NODES,
	ML_GLZNODES = ML_SSECTORS,

	// for text map format
	ML_TEXTMAP = ML_THINGS,
};


// A single Vertex.
struct mapvertex_t
{
	short x, y;
};

// A SideDef, defining the visual appearance of a wall,
// by setting textures and offsets.
struct mapsidedef_t
{
	short	textureoffset;
	short	rowoffset;
	char	toptexture[8];
	char	bottomtexture[8];
	char	midtexture[8];
	short	sector;	// Front sector, towards viewer.
};

// A LineDef, as used for editing, and as input to the BSP builder.
struct maplinedef_t
{
	WORD	v1;
	WORD	v2;
	WORD	flags;
	short	special;
	short	tag;
	WORD	sidenum[2];	// sidenum[1] will be -1 if one sided

} ;

// [RH] Hexen-compatible LineDef.
struct maplinedef2_t
{
	WORD	v1;
	WORD	v2;
	WORD	flags;
	BYTE	special;
	BYTE	args[5];
	WORD	sidenum[2];
} ;


//
// LineDef attributes.
//

enum ELineFlags
{
	ML_BLOCKING					=0x00000001,	// solid, is an obstacle
	ML_BLOCKMONSTERS			=0x00000002,	// blocks monsters only
	ML_TWOSIDED					=0x00000004,	// backside will not be present at all if not two sided

	// If a texture is pegged, the texture will have
	// the end exposed to air held constant at the
	// top or bottom of the texture (stairs or pulled
	// down things) and will move with a height change
	// of one of the neighbor sectors.
	// Unpegged textures always have the first row of
	// the texture at the top pixel of the line for both
	// top and bottom textures (use next to windows).

	ML_DONTPEGTOP				= 0x00000008,	// upper texture unpegged
	ML_DONTPEGBOTTOM			= 0x00000010,	// lower texture unpegged
	ML_SECRET					= 0x00000020,	// don't map as two sided: IT'S A SECRET!
	ML_SOUNDBLOCK				= 0x00000040,	// don't let sound cross two of these
	ML_DONTDRAW 				= 0x00000080,	// don't draw on the automap
	ML_MAPPED					= 0x00000100,	// set if already drawn in automap
	ML_REPEAT_SPECIAL			= 0x00000200,	// special is repeatable

	ML_ADDTRANS					= 0x00000400,	// additive translucency (can only be set internally)

	// Extended flags
	ML_MONSTERSCANACTIVATE		= 0x00002000,	// [RH] Monsters (as well as players) can activate the line
	ML_BLOCK_PLAYERS			= 0x00004000,
	ML_BLOCKEVERYTHING			= 0x00008000,	// [RH] Line blocks everything
	ML_ZONEBOUNDARY				= 0x00010000,
	ML_RAILING					= 0x00020000,
	ML_BLOCK_FLOATERS			= 0x00040000,
	ML_CLIP_MIDTEX				= 0x00080000,	// Automatic for every Strife line
	ML_WRAP_MIDTEX				= 0x00100000,
	ML_3DMIDTEX					= 0x00200000,
	ML_CHECKSWITCHRANGE			= 0x00400000,
	ML_FIRSTSIDEONLY			= 0x00800000,	// activated only when crossed from front side
	ML_BLOCKPROJECTILE			= 0x01000000,
	ML_BLOCKUSE					= 0x02000000,	// blocks all use actions through this line
	ML_BLOCKSIGHT				= 0x04000000,	// blocks monster line of sight
	ML_BLOCKHITSCAN				= 0x08000000,	// blocks hitscan attacks
};


// Special activation types
enum SPAC
{
	SPAC_Cross = 1<<0,		// when player crosses line
	SPAC_Use = 1<<1,		// when player uses line
	SPAC_MCross = 1<<2,		// when monster crosses line
	SPAC_Impact = 1<<3,		// when projectile hits line
	SPAC_Push = 1<<4,		// when player pushes line	
	SPAC_PCross = 1<<5,		// when projectile crosses line
	SPAC_UseThrough = 1<<6,	// when player uses line (doesn't block)
	// SPAC_PTOUCH is mapped to SPAC_PCross|SPAC_Impact
	SPAC_AnyCross = 1<<7,	// when anything without the MF2_TELEPORT flag crosses the line
	SPAC_MUse = 1<<8,		// monsters can use
	SPAC_MPush = 1<<9,		// monsters can push
	SPAC_UseBack = 1<<10,	// Can be used from the backside

	SPAC_PlayerActivate = (SPAC_Cross|SPAC_Use|SPAC_Impact|SPAC_Push|SPAC_AnyCross|SPAC_UseThrough|SPAC_UseBack),
};

enum EMapLineFlags	// These are flags that use different values internally
{
	// For Hexen format maps
	ML_SPAC_SHIFT				= 10,
	ML_SPAC_MASK				= 0x1c00,	// Hexen's activator mask.

	// [RH] BOOM's ML_PASSUSE flag (conflicts with ML_REPEATSPECIAL)
	ML_PASSUSE_BOOM				= 0x0200,
	ML_3DMIDTEX_ETERNITY		= 0x0400,

	// If this bit is set, then all non-original-Doom bits are cleared when
	// translating the line. Only applies when playing Doom with Doom-format maps.
	// Hexen format maps and the other games are not affected by this.
	ML_RESERVED_ETERNITY		= 0x0800,

	// [RH] Extra flags for Strife
	ML_RAILING_STRIFE			= 0x0200,
	ML_BLOCK_FLOATERS_STRIFE	= 0x0400,
	ML_TRANSPARENT_STRIFE		= 0x0800,
	ML_TRANSLUCENT_STRIFE		= 0x1000,
};


static inline int GET_SPAC (int flags)
{
	return (flags&ML_SPAC_MASK) >> ML_SPAC_SHIFT;
}



// Sector definition, from editing
struct mapsector_t
{
	short	floorheight;
	short	ceilingheight;
	char	floorpic[8];
	char	ceilingpic[8];
	short	lightlevel;
	short	special;
	short	tag;
};

// SubSector, as generated by BSP
struct mapsubsector_t
{
	WORD	numsegs;
	WORD	firstseg;	// index of first one, segs are stored sequentially
};

#pragma pack(1)
struct mapsubsector4_t
{
	WORD	numsegs;
	DWORD	firstseg;	// index of first one, segs are stored sequentially
};
#pragma pack()

// LineSeg, generated by splitting LineDefs
// using partition lines selected by BSP builder.
struct mapseg_t
{
	WORD	v1;
	WORD	v2;
	SWORD	angle;
	WORD	linedef;
	SWORD	side;
	SWORD	offset;

	int V1() { return LittleShort(v1); }
	int V2() { return LittleShort(v2); }
};

struct mapseg4_t 
{
	SDWORD v1;
	SDWORD v2;
	SWORD angle;
	WORD linedef;
	SWORD side;
	SWORD offset;

	int V1() { return LittleLong(v1); }
	int V2() { return LittleLong(v2); }
};



// BSP node structure.

// Indicate a leaf.

struct mapnode_t
{
	enum
	{
		NF_SUBSECTOR = 0x8000,
		NF_LUMPOFFSET = 0
	};
	SWORD 	x,y,dx,dy;	// partition line
	SWORD 	bbox[2][4];	// bounding box for each child
	// If NF_SUBSECTOR is or'ed in, it's a subsector,
	// else it's a node of another subtree.
	WORD 	children[2];

	DWORD Child(int num) { return LittleShort(children[num]); }
};


struct mapnode4_t
{
	enum
	{
		NF_SUBSECTOR = 0x80000000,
		NF_LUMPOFFSET = 8
	};
	SWORD 	x,y,dx,dy;	// partition line
	SWORD 	bbox[2][4];	// bounding box for each child
	// If NF_SUBSECTOR is or'ed in, it's a subsector,
	// else it's a node of another subtree.
	DWORD 	children[2];

	DWORD Child(int num) { return LittleLong(children[num]); }
};



// Thing definition, position, orientation and type,
// plus skill/visibility flags and attributes.
struct mapthing_t
{
	SWORD		x;
	SWORD		y;
	SWORD		angle;
	SWORD		type;
	SWORD		options;
};

// [RH] Hexen-compatible MapThing.
struct mapthinghexen_t
{
	SWORD 		thingid;
	SWORD		x;
	SWORD		y;
	SWORD		z;
	SWORD		angle;
	SWORD		type;
	SWORD		flags;
	BYTE		special;
	BYTE		args[5];
};

class FArchive;

// Internal representation of a mapthing
struct FMapThing
{
	int			thingid;
	fixed_t		x;
	fixed_t		y;
	fixed_t		z;
	short		angle;
	short		type;
	WORD		SkillFilter;
	WORD		ClassFilter;
	DWORD		flags;
	int			special;
	int			args[5];
	int			Conversation;
	fixed_t		gravity;
	fixed_t		alpha;
	DWORD		fillcolor;
	fixed_t		scaleX;
	fixed_t		scaleY;
	int			health;
	int			score;
	short		pitch;
	short		roll;
	DWORD		RenderStyle;

	void Serialize (FArchive &);
};


// [RH] MapThing flags.

enum EMapThingFlags
{
	/*
	MTF_EASY			= 0x0001,	// The skill flags are no longer used directly.
	MTF_MEDIUM			= 0x0002,
	MTF_HARD			= 0x0004,
	*/

	MTF_SKILLMASK		= 0x0007,
	MTF_SKILLSHIFT		= 1,		// Skill bits will be shifted 1 bit to the left to make room
									// for a bit representing the lowest skill (sk_baby)

	MTF_AMBUSH			= 0x0008,	// Thing is deaf
	MTF_DORMANT			= 0x0010,	// Thing is dormant (use Thing_Activate)
	/*
	MTF_FIGHTER			= 0x0020,	// The class flags are no longer used directly.
	MTF_CLERIC			= 0x0040,
	MTF_MAGE			= 0x0080,
	*/
	MTF_CLASS_MASK		= 0x00e0,
	MTF_CLASS_SHIFT		= 5,

	MTF_SINGLE			= 0x0100,	// Thing appears in single-player games
	MTF_COOPERATIVE		= 0x0200,	// Thing appears in cooperative games
	MTF_DEATHMATCH		= 0x0400,	// Thing appears in deathmatch games

	MTF_SHADOW			= 0x0800,
	MTF_ALTSHADOW		= 0x1000,
	MTF_FRIENDLY		= 0x2000,
	MTF_STANDSTILL		= 0x4000,
	MTF_STRIFESOMETHING	= 0x8000,

	MTF_SECRET			= 0x080000,	// Secret pickup
	MTF_NOINFIGHTING	= 0x100000,

	// BOOM and DOOM compatible versions of some of the above

	BTF_NOTSINGLE		= 0x0010,	// (TF_COOPERATIVE|TF_DEATHMATCH)
	BTF_NOTDEATHMATCH	= 0x0020,	// (TF_SINGLE|TF_COOPERATIVE)
	BTF_NOTCOOPERATIVE	= 0x0040,	// (TF_SINGLE|TF_DEATHMATCH)
	BTF_FRIENDLY		= 0x0080,	// MBF friendly monsters
	BTF_BADEDITORCHECK	= 0x0100,	// for detecting bad (Mac) editors

	// Strife thing flags

	STF_STANDSTILL		= 0x0008,
	STF_AMBUSH			= 0x0020,
	STF_FRIENDLY		= 0x0040,
	STF_SHADOW			= 0x0100,
	STF_ALTSHADOW		= 0x0200,
};

// A simplified mapthing for player starts
struct FPlayerStart
{
	fixed_t x, y, z;
	short angle, type;

	FPlayerStart() { }
	FPlayerStart(const FMapThing *mthing)
	: x(mthing->x), y(mthing->y), z(mthing->z),
	  angle(mthing->angle),
	  type(mthing->type)
	{ }
};
// Player spawn spots for deathmatch.
extern TArray<FPlayerStart> deathmatchstarts;

// Player spawn spots.
extern FPlayerStart playerstarts[MAXPLAYERS];
extern TArray<FPlayerStart> AllPlayerStarts;


#endif					// __DOOMDATA__
