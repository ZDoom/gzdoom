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
typedef struct
{
	short x, y;
} mapvertex_t;

// A SideDef, defining the visual appearance of a wall,
// by setting textures and offsets.
typedef struct
{
	short	textureoffset;
	short	rowoffset;
	char	toptexture[8];
	char	bottomtexture[8];
	char	midtexture[8];
	short	sector;	// Front sector, towards viewer.
} mapsidedef_t;

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

#define ML_BLOCKING			0x0001	// solid, is an obstacle
#define ML_BLOCKMONSTERS	0x0002	// blocks monsters only
#define ML_TWOSIDED			0x0004	// backside will not be present at all if not two sided

// If a texture is pegged, the texture will have
// the end exposed to air held constant at the
// top or bottom of the texture (stairs or pulled
// down things) and will move with a height change
// of one of the neighbor sectors.
// Unpegged textures always have the first row of
// the texture at the top pixel of the line for both
// top and bottom textures (use next to windows).

#define ML_DONTPEGTOP		0x0008	// upper texture unpegged
#define ML_DONTPEGBOTTOM	0x0010	// lower texture unpegged
#define ML_SECRET			0x0020	// don't map as two sided: IT'S A SECRET!
#define ML_SOUNDBLOCK		0x0040	// don't let sound cross two of these
#define ML_DONTDRAW 		0x0080	// don't draw on the automap
#define ML_MAPPED			0x0100	// set if already drawn in automap
#define ML_REPEAT_SPECIAL	0x0200	// special is repeatable

#define ML_SPAC_SHIFT		10
#define ML_SPAC_MASK		0x1c00	// Hexen's activator mask. These flags are no longer used.

// Extended flags
#define ML_MONSTERSCANACTIVATE		0x00002000	// [RH] Monsters (as well as players) can active the line
#define ML_BLOCK_PLAYERS			0x00004000
#define ML_BLOCKEVERYTHING			0x00008000	// [RH] Line blocks everything
#define ML_ZONEBOUNDARY				0x00010000
#define ML_RAILING					0x00020000
#define ML_BLOCK_FLOATERS			0x00040000
#define ML_CLIP_MIDTEX				0x00080000	// Automatic for every Strife line
#define ML_WRAP_MIDTEX				0x00100000
#define ML_3DMIDTEX					0x00200000
#define ML_CHECKSWITCHRANGE			0x00400000
#define ML_FIRSTSIDEONLY			0x00800000	// activated only when crossed from front side


static inline int GET_SPAC (int flags)
{
	return (flags&ML_SPAC_MASK) >> ML_SPAC_SHIFT;
}

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

	SPAC_PlayerActivate = (SPAC_Cross|SPAC_Use|SPAC_Impact|SPAC_Push|SPAC_AnyCross|SPAC_UseThrough),
};

// [RH] BOOM's ML_PASSUSE flag (conflicts with ML_REPEATSPECIAL)
#define ML_PASSUSE_BOOM				0x0200

#define ML_3DMIDTEX_ETERNITY		0x0400

// If this bit is set, then all non-original-Doom bits are cleared when
// translating the line. Only applies when playing Doom with Doom-format maps.
// Hexen format maps and the other games are not affected by this.
#define ML_RESERVED_ETERNITY		0x0800

// [RH] Extra flags for Strife compatibility
#define ML_TRANSLUCENT_STRIFE		0x1000
#define ML_RAILING_STRIFE			0x0200
#define ML_BLOCK_FLOATERS_STRIFE	0x0400


// Sector definition, from editing
typedef struct
{
	short	floorheight;
	short	ceilingheight;
	char	floorpic[8];
	char	ceilingpic[8];
	short	lightlevel;
	short	special;
	short	tag;
} mapsector_t;

// SubSector, as generated by BSP
typedef struct
{
	WORD	numsegs;
	WORD	firstseg;	// index of first one, segs are stored sequentially
} mapsubsector_t;


// LineSeg, generated by splitting LineDefs
// using partition lines selected by BSP builder.
typedef struct
{
	WORD	v1;
	WORD	v2;
	short	angle;
	WORD	linedef;
	short	side;
	short	offset;
} mapseg_t;



// BSP node structure.

// Indicate a leaf.
#define NF_SUBSECTOR	0x8000

typedef struct
{
	short 	x,y,dx,dy;	// partition line
	short 	bbox[2][4];	// bounding box for each child
	// If NF_SUBSECTOR is or'ed in, it's a subsector,
	// else it's a node of another subtree.
	unsigned short children[2];
} mapnode_t;




// Thing definition, position, orientation and type,
// plus skill/visibility flags and attributes.
typedef struct
{
	short		x;
	short		y;
	short		angle;
	short		type;
	short		options;
} mapthing_t;

// [RH] Hexen-compatible MapThing.
typedef struct
{
	unsigned short thingid;
	short		x;
	short		y;
	short		z;
	short		angle;
	short		type;
	short		flags;
	BYTE		special;
	BYTE		args[5];
} mapthinghexen_t;

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

	void Serialize (FArchive &);
};


// [RH] MapThing flags.

/*
#define MTF_EASY			0x0001	// Thing will appear on easy skill setting
#define MTF_MEDIUM			0x0002	// Thing will appear on medium skill setting
#define MTF_HARD			0x0004	// Thing will appear on hard skill setting
#define MTF_AMBUSH			0x0008	// Thing is deaf
*/
#define MTF_DORMANT			0x0010	// Thing is dormant (use Thing_Activate)
/*
#define MTF_FIGHTER			0x0020
#define MTF_CLERIC			0x0040
#define MTF_MAGE			0x0080
*/
#define MTF_CLASS_MASK		0x00e0
#define MTF_CLASS_SHIFT		5

#define MTF_SINGLE			0x0100	// Thing appears in single-player games
#define MTF_COOPERATIVE		0x0200	// Thing appears in cooperative games
#define MTF_DEATHMATCH		0x0400	// Thing appears in deathmatch games

#define MTF_SHADOW			0x0800
#define MTF_ALTSHADOW		0x1000
#define MTF_FRIENDLY		0x2000
#define MTF_STANDSTILL		0x4000
#define MTF_STRIFESOMETHING	0x8000

// BOOM and DOOM compatible versions of some of the above

#define BTF_NOTSINGLE		0x0010	// (TF_COOPERATIVE|TF_DEATHMATCH)
#define BTF_NOTDEATHMATCH	0x0020	// (TF_SINGLE|TF_COOPERATIVE)
#define BTF_NOTCOOPERATIVE	0x0040	// (TF_SINGLE|TF_DEATHMATCH)
#define BTF_FRIENDLY		0x0080	// MBF friendly monsters
#define BTF_BADEDITORCHECK	0x0100	// for detecting bad (Mac) editors

// Strife thing flags

#define STF_STANDSTILL		0x0008
#define STF_AMBUSH			0x0020
#define STF_FRIENDLY		0x0040
#define STF_SHADOW			0x0100
#define STF_ALTSHADOW		0x0200

//--------------------------------------------------------------------------
//
// Texture definition
//
//--------------------------------------------------------------------------

//
// Each texture is composed of one or more patches, with patches being lumps
// stored in the WAD. The lumps are referenced by number, and patched into
// the rectangular texture space using origin and possibly other attributes.
//
typedef struct
{
	SWORD	originx;
	SWORD	originy;
	SWORD	patch;
	SWORD	stepdir;
	SWORD	colormap;
} mappatch_t;

//
// A wall texture is a list of patches which are to be combined in a
// predefined order.
//
typedef struct
{
	BYTE		name[8];
	WORD		Flags;				// [RH] Was unused
	BYTE		ScaleX;				// [RH] Scaling (8 is normal)
	BYTE		ScaleY;				// [RH] Same as above
	SWORD		width;
	SWORD		height;
	BYTE		columndirectory[4];	// OBSOLETE
	SWORD		patchcount;
	mappatch_t	patches[1];
} maptexture_t;

#define MAPTEXF_WORLDPANNING	0x8000

// [RH] Just for documentation purposes, here's what I think the
// Strife versions of the above two structures are:

typedef struct
{
	SWORD	originx;
	SWORD	originy;
	SWORD	patch;
} strifemappatch_t;

//
// A wall texture is a list of patches which are to be combined in a
// predefined order.
//
typedef struct
{
	BYTE		name[8];
	WORD		Flags;				// [RH] Was unused
	BYTE		ScaleX;				// [RH] Scaling (8 is normal)
	BYTE		ScaleY;				// [RH] Same as above
	SWORD		width;
	SWORD		height;
	SWORD		patchcount;
	strifemappatch_t	patches[1];
} strifemaptexture_t;



#endif					// __DOOMDATA__
