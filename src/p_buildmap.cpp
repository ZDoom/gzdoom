
//**************************************************************************
//**
//** TEMPLATE.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "p_local.h"
#include "z_zone.h"
#include "m_swap.h"
#include "v_palette.h"
#include "w_wad.h"
#include "templates.h"
#include "vectors.h"

// MACROS ------------------------------------------------------------------

#define SHADE2LIGHT(s) (clamp (160-2*(s), 0, 255))

// TYPES -------------------------------------------------------------------

//ceilingstat/floorstat:
//   bit 0: 1 = parallaxing, 0 = not                                 "P"
//   bit 1: 1 = groudraw, 0 = not
//   bit 2: 1 = swap x&y, 0 = not                                    "F"
//   bit 3: 1 = double smooshiness                                   "E"
//   bit 4: 1 = x-flip                                               "F"
//   bit 5: 1 = y-flip                                               "F"
//   bit 6: 1 = Align texture to first wall of sector                "R"
//   bits 7-8:                                                       "T"
//          00 = normal floors
//          01 = masked floors
//          10 = transluscent masked floors
//          11 = reverse transluscent masked floors
//   bits 9-15: reserved

	//40 bytes
typedef struct
{
	SWORD wallptr, wallnum;
	SDWORD ceilingz, floorz;
	SWORD ceilingstat, floorstat;
	SWORD ceilingpicnum, ceilingheinum;
	SBYTE ceilingshade;
	BYTE ceilingpal, ceilingxpanning, ceilingypanning;
	SWORD floorpicnum, floorheinum;
	SBYTE floorshade;
	BYTE floorpal, floorxpanning, floorypanning;
	BYTE visibility, filler;
	SWORD lotag, hitag, extra;
} sectortype;

//cstat:
//   bit 0: 1 = Blocking wall (use with clipmove, getzrange)         "B"
//   bit 1: 1 = bottoms of invisible walls swapped, 0 = not          "2"
//   bit 2: 1 = align picture on bottom (for doors), 0 = top         "O"
//   bit 3: 1 = x-flipped, 0 = normal                                "F"
//   bit 4: 1 = masking wall, 0 = not                                "M"
//   bit 5: 1 = 1-way wall, 0 = not                                  "1"
//   bit 6: 1 = Blocking wall (use with hitscan / cliptype 1)        "H"
//   bit 7: 1 = Transluscence, 0 = not                               "T"
//   bit 8: 1 = y-flipped, 0 = normal                                "F"
//   bit 9: 1 = Transluscence reversing, 0 = normal                  "T"
//   bits 10-15: reserved

	//32 bytes
typedef struct
{
	SDWORD x, y;
	SWORD point2, nextwall, nextsector, cstat;
	SWORD picnum, overpicnum;
	SBYTE shade;
	BYTE pal, xrepeat, yrepeat, xpanning, ypanning;
	SWORD lotag, hitag, extra;
} walltype;

//cstat:
//   bit 0: 1 = Blocking sprite (use with clipmove, getzrange)       "B"
//   bit 1: 1 = transluscence, 0 = normal                            "T"
//   bit 2: 1 = x-flipped, 0 = normal                                "F"
//   bit 3: 1 = y-flipped, 0 = normal                                "F"
//   bits 5-4: 00 = FACE sprite (default)                            "R"
//             01 = WALL sprite (like masked walls)
//             10 = FLOOR sprite (parallel to ceilings&floors)
//   bit 6: 1 = 1-sided sprite, 0 = normal                           "1"
//   bit 7: 1 = Real centered centering, 0 = foot center             "C"
//   bit 8: 1 = Blocking sprite (use with hitscan / cliptype 1)      "H"
//   bit 9: 1 = Transluscence reversing, 0 = normal                  "T"
//   bits 10-14: reserved
//   bit 15: 1 = Invisible sprite, 0 = not invisible

	//44 bytes
typedef struct
{
	SDWORD x, y, z;
	SWORD cstat, picnum;
	SBYTE shade;
	BYTE pal, clipdist, filler;
	BYTE xrepeat, yrepeat;
	SBYTE xoffset, yoffset;
	SWORD sectnum, statnum;
	SWORD ang, owner, xvel, yvel, zvel;
	SWORD lotag, hitag, extra;
} spritetype;

struct SlopeWork
{
	walltype *wal;
	walltype *wal2;
	long dx, dy, i, x[3], y[3], z[3];
	long heinum;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void P_AdjustLine (line_t *line);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void LoadSectors (sectortype *bsectors);
static void LoadWalls (walltype *walls, int numwalls, sectortype *bsectors);
static vertex_t *FindVertex (fixed_t x, fixed_t y);
static void CreateStartSpot (fixed_t *pos, mapthing2_t *start);
static void CalcPlane (SlopeWork &slope, secplane_t &plane);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// P_LoadBuildMap
//
//==========================================================================

bool P_LoadBuildMap (BYTE *data, size_t len, mapthing2_t *buildstart)
{
	if (len < 26)
	{
		return false;
	}
	numsectors = SHORT(*(WORD *)(data + 20));
	int numwalls;

	if (len < 26 + numsectors*sizeof(sectortype) ||
		(numwalls = SHORT(*(WORD *)(data + 22 + numsectors*sizeof(sectortype))),
			len < 24 + numsectors*sizeof(sectortype) + numwalls*sizeof(walltype)) ||
		LONG(*(DWORD *)data) != 7 ||
		SHORT(*(WORD *)(data + 16)) >= 2048)
	{ // Can't possibly be a version 7 BUILD map
		return false;
	}

	LoadSectors ((sectortype *)(data + 22));
	LoadWalls ((walltype *)(data + 24 + numsectors*sizeof(sectortype)), numwalls,
		(sectortype *)(data + 22));
	CreateStartSpot ((fixed_t *)(data + 4), buildstart);

	return true;
}

//==========================================================================
//
// LoadSectors
//
//==========================================================================

static void LoadSectors (sectortype *bsec)
{
	FDynamicColormap *map = GetSpecialLights (PalEntry (255,255,255), level.fadeto);
	sector_t *sec;

	sec = sectors = (sector_t *)Z_Malloc (sizeof(sector_t)*numsectors, PU_LEVEL, 0);
	memset (sectors, 0, sizeof(sector_t)*numsectors);

	for (int i = 0; i < numsectors; ++i, ++bsec, ++sec)
	{
		bsec->wallptr = WORD(bsec->wallptr);
		bsec->wallnum = WORD(bsec->wallnum);
		bsec->ceilingstat = WORD(bsec->ceilingstat);
		bsec->floorstat = WORD(bsec->floorstat);

		sec->floortexz = -(LONG(bsec->floorz) << 8);
		sec->floorplane.d = -sec->floortexz;
		sec->floorplane.c = FRACUNIT;
		sec->floorplane.ic = FRACUNIT;
		sec->floorpic = (bsec->floorstat & 1) ? skyflatnum :
			(WORD(bsec->floorpicnum) % numflats);
		sec->floor_xscale = (bsec->floorstat & 8) ? FRACUNIT/2 : FRACUNIT;
		sec->floor_yscale = (bsec->floorstat & 8) ? FRACUNIT/2 : FRACUNIT;
		sec->floor_xoffs = bsec->floorxpanning << FRACBITS;
		sec->floor_yoffs = bsec->floorypanning << FRACBITS;
		sec->FloorLight = SHADE2LIGHT (bsec->floorshade);
		sec->FloorFlags = SECF_ABSLIGHTING;

		sec->ceilingtexz = -(LONG(bsec->ceilingz) << 8);
		sec->ceilingplane.d = sec->ceilingtexz;
		sec->ceilingplane.c = -FRACUNIT;
		sec->ceilingplane.ic = -FRACUNIT;
		sec->ceilingpic = (bsec->ceilingstat & 1) ? skyflatnum :
			(WORD(bsec->ceilingpicnum) % numflats);
		sec->ceiling_xscale = (bsec->ceilingstat & 8) ? FRACUNIT/2 : FRACUNIT;
		sec->ceiling_yscale = (bsec->ceilingstat & 8) ? FRACUNIT/2 : FRACUNIT;
		sec->ceiling_xoffs = bsec->ceilingxpanning << FRACBITS;
		sec->ceiling_yoffs = bsec->ceilingypanning << FRACBITS;
		sec->CeilingLight = SHADE2LIGHT (bsec->ceilingshade);
		sec->CeilingFlags = SECF_ABSLIGHTING;

		sec->lightlevel = (sec->FloorLight + sec->CeilingLight) / 2;

		sec->seqType = -1;
		sec->nextsec = -1;
		sec->prevsec = -1;
		sec->gravity = 1.f;
		sec->friction = ORIG_FRICTION;
		sec->movefactor = ORIG_FRICTION_FACTOR;
		sec->ColorMap = map;

		if (bsec->floorstat & 4)
		{
			sec->floor_angle = ANGLE_90;
		}
		if (bsec->floorstat & 16)
		{
			sec->floor_xscale = -sec->floor_xscale;
		}
		if (bsec->floorstat & 32)
		{
			sec->floor_yscale = -sec->floor_yscale;
		}

		if (bsec->ceilingstat & 4)
		{
			sec->ceiling_angle = ANGLE_90;
		}
		if (bsec->ceilingstat & 16)
		{
			sec->ceiling_xscale = -sec->ceiling_xscale;
		}
		if (bsec->ceilingstat & 32)
		{
			sec->ceiling_yscale = -sec->ceiling_yscale;
		}

		while (sec->floorpic == 0 || W_LumpLength (firstflat + sec->floorpic) != 4096)
		{
			sec->floorpic++;
			if (sec->floorpic >= numflats)
			{
				sec->floorpic = 1;
			}
		}
		while (sec->ceilingpic == 0 || W_LumpLength (firstflat + sec->ceilingpic) != 4096)
		{
			sec->ceilingpic++;
			if (sec->ceilingpic >= numflats)
			{
				sec->ceilingpic = 1;
			}
		}
	}
}

//==========================================================================
//
// LoadWalls
//
//==========================================================================

static void LoadWalls (walltype *walls, int numwalls, sectortype *bsec)
{
	int i, j;

	// Setting numvertexes to the same as numwalls is overly conservative,
	// but the extra vertices will be removed during the BSP building pass.
	numsides = numvertexes = numwalls;
	numlines = 0;

	sides = (side_t *)Z_Malloc (numsides*sizeof(side_t), PU_LEVEL, 0);
	memset (sides, 0, numsides*sizeof(side_t));

	vertexes = (vertex_t *)Z_Malloc (numvertexes*sizeof(vertex_t), PU_LEVEL, 0);
	numvertexes = 0;

	// First mark each sidedef with the sector it belongs to
	for (i = 0; i < numsectors; ++i)
	{
		if (bsec[i].wallptr >= 0)
		{
			for (j = 0; j < bsec[i].wallnum; ++j)
			{
				sides[j + bsec[i].wallptr].sector = sectors + i;
			}
		}
	}

	// Now copy wall properties to their matching sidedefs
	for (i = 0; i < numwalls; ++i)
	{
		walls[i].x = LONG(walls[i].x);
		walls[i].y = LONG(walls[i].y);
		walls[i].point2 = SHORT(walls[i].point2);
		walls[i].cstat = SHORT(walls[i].cstat);
		walls[i].nextwall = SHORT(walls[i].nextwall);
		walls[i].nextsector = SHORT(walls[i].nextsector);

		sides[i].textureoffset = walls[i].xpanning << FRACBITS;
		sides[i].rowoffset = walls[i].ypanning << FRACBITS;
		sides[i].toptexture = sides[i].bottomtexture = /*SHORT(walls[i].overpicnum)+*/1;
		if ((walls[i].cstat & (16|32)) || walls[i].nextsector < 0)
		{
			sides[i].midtexture = /*SHORT(walls[i].picnum)+*/1;
		}
		else
		{
			sides[i].midtexture = 0;
		}
		sides[i].TexelLength = walls[i].xrepeat * 8;
		sides[i].Light = SHADE2LIGHT(walls[i].shade);
		sides[i].Flags = WALLF_ABSLIGHTING;
		sides[i].RightSide = walls[i].point2;
		sides[walls[i].point2].LeftSide = i;

		if (walls[i].nextwall >= 0 && walls[i].nextwall <= i)
		{
			sides[i].linenum = sides[walls[i].nextwall].linenum;
		}
		else
		{
			sides[i].linenum = numlines++;
		}
	}

	// Set line properties that Doom doesn't store per-sidedef
	lines = (line_t *)Z_Malloc (numlines*sizeof(line_t), PU_LEVEL, 0);
	memset (lines, 0, numlines*sizeof(line_t));

	for (i = 0, j = -1; i < numwalls; ++i)
	{
		if (walls[i].nextwall >= 0 && walls[i].nextwall <= i)
		{
			continue;
		}

		j = sides[i].linenum;
		lines[j].sidenum[0] = i;
		lines[j].sidenum[1] = walls[i].nextwall;
		lines[j].v1 = FindVertex (walls[i].x, walls[i].y);
		lines[j].v2 = FindVertex (walls[walls[i].point2].x, walls[walls[i].point2].y);
		lines[j].frontsector = sides[i].sector;
		if (walls[i].nextsector >= 0)
		{
			lines[j].backsector = sectors + walls[i].nextsector;
			lines[j].flags |= ML_TWOSIDED;
		}
		else
		{
			lines[j].backsector = NULL;
		}
		P_AdjustLine (&lines[j]);
		if (walls[i].cstat & 128)
		{
			if (walls[i].cstat & 512)
			{
				lines[j].alpha = 255/3;
			}
			else
			{
				lines[j].alpha = 255*2/3;
			}
		}
		if (walls[i].cstat & 1)
		{
			lines[j].flags |= ML_BLOCKING;
		}
		if (walls[i].cstat & 4)
		{
			lines[j].flags |= ML_DONTPEGTOP;
		}
		if (walls[i].cstat & 64)
		{
			lines[j].flags |= ML_BLOCKEVERYTHING;
		}
	}

	// Finish setting sector properties that depend on walls
	for (i = 0; i < numsectors; ++i, ++bsec)
	{
		SlopeWork slope;

		slope.wal = &walls[bsec->wallptr];
		slope.wal2 = &walls[slope.wal->point2];
		slope.dx = slope.wal2->x - slope.wal->x;
		slope.dy = slope.wal2->y - slope.wal->y;
		slope.i = long (sqrt (slope.dx*slope.dx+slope.dy*slope.dy)) << 5;
		if (slope.i == 0)
		{
			continue;
		}
		if ((bsec->floorstat & 2) && (bsec->floorheinum != 0))
		{ // floor is sloped
			slope.heinum = -SHORT(bsec->floorheinum);
			slope.z[0] = slope.z[1] = slope.z[2] = -bsec->floorz;
			CalcPlane (slope, sectors[i].floorplane);
		}
		if ((bsec->ceilingstat & 2) && (bsec->ceilingheinum != 0))
		{ // ceiling is sloped
			slope.heinum = -SHORT(bsec->ceilingheinum);
			slope.z[0] = slope.z[1] = slope.z[2] = -bsec->ceilingz;
			CalcPlane (slope, sectors[i].ceilingplane);
		}
		if (bsec->floorstat & 64)
		{ // floor is aligned to first wall
			R_AlignFlat (sides[bsec->wallptr].linenum,
				lines[sides[bsec->wallptr].linenum].sidenum[1] == bsec->wallptr, 0);
		}
		if (bsec->ceilingstat & 64)
		{ // ceiling is aligned to first wall
			R_AlignFlat (sides[bsec->wallptr].linenum,
				lines[sides[bsec->wallptr].linenum].sidenum[1] == bsec->wallptr, 0);
		}
	}
}

//==========================================================================
//
// FindVertex
//
//==========================================================================

vertex_t *FindVertex (fixed_t x, fixed_t y)
{
	int i;

	x <<= 13;
	y = -(y << 13);

	for (i = 0; i < numvertexes; ++i)
	{
		if (vertexes[i].x == x && vertexes[i].y == y)
		{
			return &vertexes[i];
		}
	}
	vertexes[i].x = x;
	vertexes[i].y = y;
	numvertexes++;
	return &vertexes[i];
}

//==========================================================================
//
// CreateStartSpot
//
//==========================================================================

static void CreateStartSpot (fixed_t *pos, mapthing2_t *start)
{
	mapthing2_t mt =
	{
		0, short(LONG(pos[0])>>3), short((-LONG(pos[1]))>>3), 0,// tid, x, y, z
		short(Scale (SHORT(*(WORD *)(&pos[3])), 360, 2048)), 1,	// angle, type
		7|MTF_SINGLE|224,										// flags
		// special and args are 0
	};

	*start = mt;
}

//==========================================================================
//
// CalcPlane
//
//==========================================================================

static void CalcPlane (SlopeWork &slope, secplane_t &plane)
{
	vec3_t pt[3];
	long j;

	slope.x[0] = slope.wal->x;  slope.y[0] = slope.wal->y;
	slope.x[1] = slope.wal2->x; slope.y[1] = slope.wal2->y;
	if (slope.dx == 0)
	{
		slope.x[2] = slope.x[0] + 64;
		slope.y[2] = slope.y[0];
	}
	else
	{
		slope.x[2] = slope.x[0];
		slope.y[2] = slope.y[0] + 64;
	}
	j = DMulScale3 (slope.dx, slope.y[2]-slope.wal->y,
		-slope.dy, slope.x[2]-slope.wal->x);
	slope.z[2] += Scale (slope.heinum, j, slope.i);

	pt[0][0] = float(slope.dx);
	pt[0][1] = float(-slope.dy);
	pt[0][2] = 0.f;

	pt[1][0] = float(slope.x[2] - slope.x[0]);
	pt[1][1] = float(slope.y[0] - slope.y[2]);
	pt[1][2] = float(slope.z[2] - slope.z[0]) / 32.f;

	CrossProduct (pt[0], pt[1], pt[2]);
	VectorNormalize (pt[2]);

	if ((pt[2][2] < 0 && plane.c > 0) ||
		(pt[2][2] > 0 && plane.c < 0))
	{
		pt[2][0] = -pt[2][0];
		pt[2][1] = -pt[2][1];
		pt[2][2] = -pt[2][2];
	}

	plane.a = fixed_t(pt[2][0]*65536.f);
	plane.b = fixed_t(pt[2][1]*65536.f);
	plane.c = fixed_t(pt[2][2]*65536.f);
	plane.ic = fixed_t(65536.f/pt[2][2]);
	plane.d = -TMulScale8
		(plane.a, slope.x[0]<<5, plane.b, (-slope.y[0])<<5, plane.c, slope.z[0]);
}
