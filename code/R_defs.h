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
	fixed_t 	x;
	fixed_t 	y;
	
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
	struct dyncolormap_s *colormap;	// [RH] Per-sector colormap
};
typedef struct sector_s sector_t;



//
// The SideDef.
//

struct side_s
{
	// add this to the calculated texture column
	fixed_t 	textureoffset;
	
	// add this to the calculated texture top
	fixed_t 	rowoffset;

	// Texture indices.
	// We do not maintain names here. 
	short		toptexture;
	short		bottomtexture;
	short		midtexture;

	// [RH] Bah. Had to add this for BOOM
	byte		special;

	// Sector the SideDef is facing.
	sector_t*	sector;
	
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
	// Vertices, from v1 to v2.
	vertex_t*	v1;
	vertex_t*	v2;

	// Precalculated v2 - v1 for side checking.
	fixed_t 	dx;
	fixed_t 	dy;

	// Animation related.
	short		flags;
	byte		special;	// [RH] Only one byte per special.
	byte		lucency;	// <--- Translucency (0-255/255=opaque)
	short		id;			// <--- Same as tag or set with Line_SetIdentification.
	short		args[5];	// <--- Hexen-style arguments
							//		Note that these are shorts in order to support
							//		the tag parameter from DOOM.
	int			firstid, nextid;

	// Visual appearance: SideDefs.
	//	sidenum[1] will be -1 if one sided
	short		sidenum[2]; 					

	// Neat. Another bounding box, for the extent
	//	of the LineDef.
	fixed_t 	bbox[4];

	// To aid move clipping.
	slopetype_t slopetype;

	// Front and back sector.
	// Note: redundant? Can be retrieved from SideDefs.
	sector_t*	frontsector;
	sector_t*	backsector;

	// if == validcount, already checked
	int 		validcount;
};
typedef struct line_s line_t;



//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs,
//	indicating the visible walls that define
//	(all or some) sides of a convex BSP leaf.
//
struct subsector_s
{
	sector_t*	sector;
	short		numlines;
	short		firstline;
	
};
typedef struct subsector_s subsector_t;

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
	// backsector is NULL for one sided lines
	sector_t*	frontsector;
	sector_t*	backsector;
	
};
typedef struct seg_s seg_t;


//
// BSP node.
//
struct node_s
{
	// Partition line.
	fixed_t 	x;
	fixed_t 	y;
	fixed_t 	dx;
	fixed_t 	dy;

	// Bounding box for each child.
	fixed_t 	bbox[2][4];

	// If NF_SUBSECTOR its a subsector.
	unsigned short children[2];
	
};
typedef struct node_s node_t;



// posts are runs of non masked source pixels
struct post_s
{
	byte				topdelta;		// -1 is the last post in a column
	byte				length; 		// length data bytes follows
};
typedef struct post_s post_t;

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t	column_t;



// PC direct to screen pointers
//B UNUSED - keep till detailshift in r_draw.c resolved
//extern byte*	destview;
//extern byte*	destscreen;





//
// OTHER TYPES
//

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
//	precalculating 24bpp lightmap/colormap LUT.
//	from darkening PLAYPAL to all black.
// Could even us emore than 32 levels.
typedef byte	lighttable_t;	




//
// ?
//
struct drawseg_s
{
	seg_t*				curline;
	int 				x1;
	int 				x2;

	fixed_t 			scale1;
	fixed_t 			scale2;
	fixed_t 			scalestep;

	// 0=none, 1=bottom, 2=top, 3=both
	int 				silhouette;

	// do not clip sprites above this
	fixed_t 			bsilheight;

	// do not clip sprites below this
	fixed_t 			tsilheight;
	
	// Pointers to lists for sprite clipping,
	//	all three adjusted so [x1] is first value.
	short*				sprtopclip; 			
	short*				sprbottomclip;	
	short*				maskedtexturecol;
};
typedef struct drawseg_s drawseg_t;


// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.
struct patch_s
{ 
	short				width;			// bounding box size 
	short				height; 
	short				leftoffset; 	// pixels to the left of origin 
	short				topoffset;		// pixels below the origin 
	int 				columnofs[8];	// only [width] used
	// the [0] is &columnofs[width] 
};
typedef struct patch_s patch_t;






// A vissprite_t is a thing
//	that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.
struct vissprite_s
{
	int 				x1;
	int 				x2;

	// for line side calculation
	fixed_t 			gx;
	fixed_t 			gy; 			

	// global bottom / top for silhouette clipping
	fixed_t 			gz;
	fixed_t 			gzt;

	// horizontal position of x1
	fixed_t 			startfrac;
	
	fixed_t 			scale;
	
	// negative if flipped
	fixed_t 			xiscale;		

	fixed_t 			texturemid;
	int 				patch;

	// for color translation and maxbright frames as well
	lighttable_t*		colormap;
   
	int 				mobjflags;

	// [RH] Sprite's palette (NULL for default)
	//		Currently this is just a translation table and not a palette.
	struct palette_s	*palette;

	// killough 3/27/98: height sector for underwater/fake ceiling support
	int heightsec;

};
typedef struct vissprite_s vissprite_t;

//		
// Sprites are patches with a special naming convention
//	so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with
//	x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//	is range checked at run time.
// A sprite is a patch_t that is assumed to represent
//	a three dimensional object and may have multiple
//	rotations pre drawn.
// Horizontal flipping is used to save space,
//	thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//
struct spriteframe_s
{
	// If false use 0 for any position.
	// Note: as eight entries are available,
	//	we might as well insert the same name eight times.
	BOOL	 	rotate;

	// Lump to use for view angles 0-7.
	short		lump[8];

	// [RH] Move some data out of spritewidth, spriteoffset,
	//		and spritetopoffset arrays.
	fixed_t		width[8];
	fixed_t		topoffset[8];
	fixed_t		offset[8];

	// Flip bit (1 = flip) to use for view angles 0-7.
	byte		flip[8];
	
};
typedef struct spriteframe_s spriteframe_t;

//
// A sprite definition:
//	a number of animation frames.
//
struct spritedef_s
{
	int 				numframes;
	spriteframe_t*		spriteframes;

};
typedef struct spritedef_s spritedef_t;

//
// [RH] Internal "skin" definition.
//
struct playerskin_s
{
	char				name[17];	// 16 chars + NULL
	char				face[3];
	int					sprite;
	int					namespc;	// namespace for this skin
	int					gender;		// This skin's gender (not used)
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

	unsigned short *bottom;			// [RH] bottom and top arrays are dynamically
	unsigned short pad;				//		allocated immediately after the
	unsigned short top[3];			//		visplane.
};
typedef struct visplane_s visplane_t;



#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
