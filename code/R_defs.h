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


#ifndef __R_DEFS__
#define __R_DEFS__


// Screenwidth.
#include "doomdef.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"
// SECTORS do store MObjs anyway.
#include "p_mobj.h"






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

// Each sector has a degenmobj_t in its center
//	for sound origin purposes.
// I suppose this does not handle sound from
//	moving objects (doppler), because
//	position is prolly just buffered, not
//	updated.
struct player_s;
struct degenmobj_s
{
	thinker_t			thinker;		// not used for anything
	fixed_t 			x;
	fixed_t 			y;
	fixed_t 			z;
};
typedef struct degenmobj_s degenmobj_t;

//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
struct dyncolormap_s;

struct sector_s
{
	fixed_t 	floorheight;
	fixed_t 	ceilingheight;
	short		floorpic;
	short		ceilingpic;
	short		lightlevel;
	short		special;
	short		tag;
	int			nexttag,firsttag;	// killough 1/30/98: improves searches for tags.

	int 		soundtraversed;	// 0 = untraversed, 1,2 = sndlines -1
	mobj_t* 	soundtarget;	// thing that made a sound (or null)
	int 		blockbox[4];	// mapblock bounding box for height changes
	degenmobj_t soundorg;		// origin for any sounds played by the sector
	int 		validcount;		// if == validcount, already checked
	mobj_t* 	thinglist;		// list of mobjs in sector
	int			seqType;		// this sector's sound sequence

	int sky;

	// killough 8/28/98: friction is a sector property, not an mobj property.
	// these fields used to be in mobj_t, but presented performance problems
	// when processed as mobj properties. Fix is to make them sector properties.
	int friction, movefactor;

	// thinker_t for reversable actions
	void *floordata;			// jff 2/22/98 make thinkers on
	void *ceilingdata;			// floors, ceilings, lighting,
	void *lightingdata;			// independent of one another

	// jff 2/26/98 lockout machinery for stairbuilding
	int stairlock;		// -2 on first locked -1 after thinker done 0 normally
	int prevsec;		// -1 or number of sector for previous step
	int nextsec;		// -1 or number of next step sector

	// killough 3/7/98: floor and ceiling texture offsets
	fixed_t   floor_xoffs,   floor_yoffs;
	fixed_t ceiling_xoffs, ceiling_yoffs;

	// killough 3/7/98: support flat heights drawn at another sector's heights
	int heightsec;		// other sector, or -1 if no other sector

	// killough 4/11/98: support for lightlevels coming from another sector
	int floorlightsec, ceilinglightsec;

	unsigned int bottommap, midmap, topmap; // killough 4/4/98: dynamic colormaps
											// [RH] these can also be blend values if
											//		the alpha mask is non-zero

	// list of mobjs that are at least partially in the sector
	// thinglist is a subset of touching_thinglist
	struct msecnode_s *touching_thinglist;				// phares 3/14/98

	int linecount;
	struct line_s **lines;		// [linecount] size
	
	float gravity;		// [RH] Sector gravity (1.0 is normal)
	short damage;		// [RH] Damage to do while standing on floor
	short mod;			// [RH] Means-of-death for applied damage
	struct dyncolormap_s *floorcolormap;	// [RH] Per-sector colormap
	struct dyncolormap_s *ceilingcolormap;
};
typedef struct sector_s sector_t;



//
// The SideDef.
//

struct side_s
{
	fixed_t 	textureoffset;	// add this to the calculated texture column
	fixed_t 	rowoffset;		// add this to the calculated texture top
	short		toptexture, bottomtexture, midtexture;	// texture indices
	sector_t*	sector;			// Sector the SideDef is facing.
	short		special;		// [RH] Bah. Had to add these for BOOM stuff
	short		tag;
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
	byte		lucency;	// <--- translucency (0-255/255=opaque)
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
// The second thread starts at touching_sectorlist in an mobj_t and flows
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
	struct mobj_s		*m_thing;	// this object
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
	
	fixed_t 	offset;

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
typedef struct
{
	int			numsegs;
	seg_t		**segs;
	degenmobj_t	startSpot;
	vertex_t	*originalPts;	// used as the base for the rotations
	vertex_t	*prevPts; 		// use to restore the old point values
	angle_t		angle;
	int			tag;			// reference tag assigned in HereticEd
	int			bbox[4];
	int			validcount;
	BOOL		crush; 			// should the polyobj attempt to crush mobjs?
	int			seqType;
	fixed_t		size;			// polyobj size (area of POLY_AREAUNIT == size of FRACUNIT)
	void		*specialdata;	// pointer a thinker, if the poly is moving
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

struct drawseg_s
{
	seg_t*		curline;
	int 		x1, x2;
	fixed_t 	scale1, scale2, scalestep;
	int 		silhouette;		// 0=none, 1=bottom, 2=top, 3=both
	fixed_t 	bsilheight;		// don't clip sprites above this
	fixed_t 	tsilheight;		// don't clip sprites below this
// Pointers to lists for sprite clipping,
// all three adjusted so [x1] is first value.
	short*		sprtopclip; 			
	short*		sprbottomclip;	
	short*		maskedtexturecol;
};
typedef struct drawseg_s drawseg_t;


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
	fixed_t			scale;
	fixed_t			xiscale;		// negative if flipped
	fixed_t			texturemid;
	int				patch;
	lighttable_t	*colormap;
	int 			mobjflags;		// for shadow draw
	byte			*translation;	// [RH] for translation;
	int				heightsec;		// killough 3/27/98: height sector for underwater/fake ceiling
	fixed_t			translucency;
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
	// Note: as eight entries are available,
	//	we might as well insert the same name eight times.
	BOOL	 	rotate;		// if false, use 0 for any position.
	short		lump[8];	// lump to use for view angles 0-7
	byte		flip[8];	// flip (1 = flip) to use for view angles 0-7.

	// [RH] Move some data out of spritewidth, spriteoffset,
	//		and spritetopoffset arrays.
	fixed_t		width[8];
	fixed_t		topoffset[8];
	fixed_t		offset[8];
};
typedef struct spriteframe_s spriteframe_t;

//
// A sprite definition:
//	a number of animation frames.
//
struct spritedef_s
{
	int 			numframes;
	spriteframe_t	*spriteframes;
};
typedef struct spritedef_s spritedef_t;

//
// [RH] Internal "skin" definition.
//
struct playerskin_s
{
	char		name[17];	// 16 chars + NULL
	char		face[3];
	int			sprite;
	int			namespc;	// namespace for this skin
	int			gender;		// This skin's gender (not used)
};
typedef struct playerskin_s playerskin_t;

//
// The infamous visplane
// 
struct visplane_s
{
	struct visplane_s *next;		// Next visplane in hash chain -- killough

	fixed_t		height;
	int			picnum;
	int			lightlevel;
	fixed_t		xoffs, yoffs;		// killough 2/28/98: Support scrolling flats
	int			minx, maxx;
	byte		*colormap;			// [RH] Support multiple colormaps
	int			color;				// [RH] Color to render if r_drawflat is 1

	unsigned short *bottom;			// [RH] bottom and top arrays are dynamically
	unsigned short pad;				//		allocated immediately after the
	unsigned short top[3];			//		visplane.
};
typedef struct visplane_s visplane_t;



#endif
