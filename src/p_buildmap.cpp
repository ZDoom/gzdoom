
//**************************************************************************
//**
//** TEMPLATE.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "p_local.h"
#include "m_swap.h"
#include "v_palette.h"
#include "w_wad.h"
#include "templates.h"
#include "r_sky.h"
#include "r_main.h"
#include "r_defs.h"
#include "p_setup.h"
#include "g_level.h"

// MACROS ------------------------------------------------------------------

//#define SHADE2LIGHT(s) (clamp (160-2*(s), 0, 255))
#define SHADE2LIGHT(s) (clamp (255-2*s, 0, 255))

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
struct sectortype
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
};

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
struct walltype
{
	SDWORD x, y;
	SWORD point2, nextwall, nextsector, cstat;
	SWORD picnum, overpicnum;
	SBYTE shade;
	BYTE pal, xrepeat, yrepeat, xpanning, ypanning;
	SWORD lotag, hitag, extra;
};

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
struct spritetype
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
};

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

static bool P_LoadBloodMap (BYTE *data, size_t len, FMapThing **sprites, int *numsprites);
static void LoadSectors (sectortype *bsectors);
static void LoadWalls (walltype *walls, int numwalls, sectortype *bsectors);
static int LoadSprites (spritetype *sprites, int numsprites, sectortype *bsectors, FMapThing *mapthings);
static vertex_t *FindVertex (fixed_t x, fixed_t y);
static void CreateStartSpot (fixed_t *pos, FMapThing *start);
static void CalcPlane (SlopeWork &slope, secplane_t &plane);
static void Decrypt (void *to, const void *from, int len, int key);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

bool P_IsBuildMap(MapData *map)
{
	DWORD len = map->Size(ML_LABEL);
	if (len < 4) return false;

	BYTE *data = new BYTE[len];

	map->Seek(ML_LABEL);
	map->Read(ML_LABEL, data);

	// Check for a Blood map.
	if (*(DWORD *)data == MAKE_ID('B','L','M','\x1a'))
	{
		delete[] data;
		return true;
	}

	numsectors = LittleShort(*(WORD *)(data + 20));
	int numwalls;

	if (len < 26 + numsectors*sizeof(sectortype) ||
		(numwalls = LittleShort(*(WORD *)(data + 22 + numsectors*sizeof(sectortype))),
			len < 24 + numsectors*sizeof(sectortype) + numwalls*sizeof(walltype)) ||
		LittleLong(*(DWORD *)data) != 7 ||
		LittleShort(*(WORD *)(data + 16)) >= 2048)
	{ // Can't possibly be a version 7 BUILD map
		delete[] data;
		return false;
	}
	delete[] data;
	return true;
}

//==========================================================================
//
// P_LoadBuildMap
//
//==========================================================================

bool P_LoadBuildMap (BYTE *data, size_t len, FMapThing **sprites, int *numspr)
{
	if (len < 26)
	{
		return false;
	}

	// Check for a Blood map.
	if (*(DWORD *)data == MAKE_ID('B','L','M','\x1a'))
	{
		return P_LoadBloodMap (data, len, sprites, numspr);
	}

	numsectors = LittleShort(*(WORD *)(data + 20));
	int numwalls;
	int numsprites;

	if (len < 26 + numsectors*sizeof(sectortype) ||
		(numwalls = LittleShort(*(WORD *)(data + 22 + numsectors*sizeof(sectortype))),
			len < 24 + numsectors*sizeof(sectortype) + numwalls*sizeof(walltype)) ||
		LittleLong(*(DWORD *)data) != 7 ||
		LittleShort(*(WORD *)(data + 16)) >= 2048)
	{ // Can't possibly be a version 7 BUILD map
		return false;
	}

	LoadSectors ((sectortype *)(data + 22));
	LoadWalls ((walltype *)(data + 24 + numsectors*sizeof(sectortype)), numwalls,
		(sectortype *)(data + 22));

	numsprites = *(WORD *)(data + 24 + numsectors*sizeof(sectortype) + numwalls*sizeof(walltype));
	*sprites = new FMapThing[numsprites + 1];
	CreateStartSpot ((fixed_t *)(data + 4), *sprites);
	*numspr = 1 + LoadSprites ((spritetype *)(data + 26 + numsectors*sizeof(sectortype) + numwalls*sizeof(walltype)),
		numsprites, (sectortype *)(data + 22), *sprites + 1);

	return true;
}

//==========================================================================
//
// P_LoadBloodMap
//
//==========================================================================

static bool P_LoadBloodMap (BYTE *data, size_t len, FMapThing **mapthings, int *numspr)
{
	BYTE infoBlock[37];
	int mapver = data[5];
	DWORD matt;
	int numRevisions, numWalls, numsprites, skyLen;
	int i;
	int k;

	if (mapver != 6 && mapver != 7)
	{
		return false;
	}

	matt = *(DWORD *)(data + 28);
	if (matt != 0 &&
		matt != MAKE_ID('M','a','t','t') &&
		matt != MAKE_ID('t','t','a','M'))
	{
		Decrypt (infoBlock, data + 6, 37, 0x7474614d);
	}
	else
	{
		memcpy (infoBlock, data + 6, 37);
	}
	numRevisions = *(DWORD *)(infoBlock + 27);
	numsectors = *(WORD *)(infoBlock + 31);
	numWalls = *(WORD *)(infoBlock + 33);
	numsprites = *(WORD *)(infoBlock + 35);
	skyLen = 2 << *(WORD *)(infoBlock + 16);

	if (mapver == 7)
	{
		// Version 7 has some extra stuff after the info block. This
		// includes a copyright, and I have no idea what the rest of
		// it is.
		data += 171;
	}
	else
	{
		data += 43;
	}

	// Skip the sky info.
	data += skyLen;

	sectortype *bsec = new sectortype[numsectors];
	walltype *bwal = new walltype[numWalls];
	spritetype *bspr = new spritetype[numsprites];

	// Read sectors
	k = numRevisions * sizeof(sectortype);
	for (i = 0; i < numsectors; ++i)
	{
		if (mapver == 7)
		{
			Decrypt (&bsec[i], data, sizeof(sectortype), k);
		}
		else
		{
			memcpy (&bsec[i], data, sizeof(sectortype));
		}
		data += sizeof(sectortype);
		if (bsec[i].extra > 0)	// skip Xsector
		{
			data += 60;
		}
	}

	// Read walls
	k |= 0x7474614d;
	for (i = 0; i < numWalls; ++i)
	{
		if (mapver == 7)
		{
			Decrypt (&bwal[i], data, sizeof(walltype), k);
		}
		else
		{
			memcpy (&bwal[i], data, sizeof(walltype));
		}
		data += sizeof(walltype);
		if (bwal[i].extra > 0)	// skip Xwall
		{
			data += 24;
		}
	}

	// Read sprites
	k = (numRevisions * sizeof(spritetype)) | 0x7474614d;
	for (i = 0; i < numsprites; ++i)
	{
		if (mapver == 7)
		{
			Decrypt (&bspr[i], data, sizeof(spritetype), k);
		}
		else
		{
			memcpy (&bspr[i], data, sizeof(spritetype));
		}
		data += sizeof(spritetype);
		if (bspr[i].extra > 0)	// skip Xsprite
		{
			data += 56;
		}
	}

	// Now convert to Doom format, since we've extracted all the standard
	// BUILD info from the map we need. (Sprites are ignored.)
	LoadSectors (bsec);
	LoadWalls (bwal, numWalls, bsec);
	*mapthings = new FMapThing[numsprites + 1];
	CreateStartSpot ((fixed_t *)infoBlock, *mapthings);
	*numspr = 1 + LoadSprites (bspr, numsprites, bsec, *mapthings + 1);

	delete[] bsec;
	delete[] bwal;
	delete[] bspr;

	return true;
}

//==========================================================================
//
// LoadSectors
//
//==========================================================================

static void LoadSectors (sectortype *bsec)
{
	FDynamicColormap *map = GetSpecialLights (PalEntry (255,255,255), level.fadeto, 0);
	sector_t *sec;
	char tnam[9];

	sec = sectors = new sector_t[numsectors];
	memset (sectors, 0, sizeof(sector_t)*numsectors);

	for (int i = 0; i < numsectors; ++i, ++bsec, ++sec)
	{
		bsec->wallptr = WORD(bsec->wallptr);
		bsec->wallnum = WORD(bsec->wallnum);
		bsec->ceilingstat = WORD(bsec->ceilingstat);
		bsec->floorstat = WORD(bsec->floorstat);

		sec->SetPlaneTexZ(sector_t::floor, -(LittleLong(bsec->floorz) << 8));
		sec->floorplane.d = -sec->GetPlaneTexZ(sector_t::floor);
		sec->floorplane.c = FRACUNIT;
		sec->floorplane.ic = FRACUNIT;
		mysnprintf (tnam, countof(tnam), "BTIL%04d", LittleShort(bsec->floorpicnum));
		sec->SetTexture(sector_t::floor, TexMan.GetTexture (tnam, FTexture::TEX_Build));
		sec->SetXScale(sector_t::floor, (bsec->floorstat & 8) ? FRACUNIT*2 : FRACUNIT);
		sec->SetYScale(sector_t::floor, (bsec->floorstat & 8) ? FRACUNIT*2 : FRACUNIT);
		sec->SetXOffset(sector_t::floor, (bsec->floorxpanning << FRACBITS) + (32 << FRACBITS));
		sec->SetYOffset(sector_t::floor, bsec->floorypanning << FRACBITS);
		sec->SetPlaneLight(sector_t::floor, SHADE2LIGHT (bsec->floorshade));
		sec->ChangeFlags(sector_t::floor, 0, SECF_ABSLIGHTING);

		sec->SetPlaneTexZ(sector_t::ceiling, -(LittleLong(bsec->ceilingz) << 8));
		sec->ceilingplane.d = sec->GetPlaneTexZ(sector_t::ceiling);
		sec->ceilingplane.c = -FRACUNIT;
		sec->ceilingplane.ic = -FRACUNIT;
		mysnprintf (tnam, countof(tnam), "BTIL%04d", LittleShort(bsec->ceilingpicnum));
		sec->SetTexture(sector_t::ceiling, TexMan.GetTexture (tnam, FTexture::TEX_Build));
		if (bsec->ceilingstat & 1)
		{
			sky1texture = sky2texture = sec->GetTexture(sector_t::ceiling);
			sec->SetTexture(sector_t::ceiling, skyflatnum);
		}
		sec->SetXScale(sector_t::ceiling, (bsec->ceilingstat & 8) ? FRACUNIT*2 : FRACUNIT);
		sec->SetYScale(sector_t::ceiling, (bsec->ceilingstat & 8) ? FRACUNIT*2 : FRACUNIT);
		sec->SetXOffset(sector_t::ceiling, (bsec->ceilingxpanning << FRACBITS) + (32 << FRACBITS));
		sec->SetYOffset(sector_t::ceiling, bsec->ceilingypanning << FRACBITS);
		sec->SetPlaneLight(sector_t::ceiling, SHADE2LIGHT (bsec->ceilingshade));
		sec->ChangeFlags(sector_t::ceiling, 0, SECF_ABSLIGHTING);

		sec->lightlevel = (sec->GetPlaneLight(sector_t::floor) + sec->GetPlaneLight(sector_t::ceiling)) / 2;

		sec->seqType = -1;
		sec->nextsec = -1;
		sec->prevsec = -1;
		sec->gravity = 1.f;
		sec->friction = ORIG_FRICTION;
		sec->movefactor = ORIG_FRICTION_FACTOR;
		sec->ColorMap = map;
		sec->ZoneNumber = 0xFFFF;

		if (bsec->floorstat & 4)
		{
			sec->SetAngle(sector_t::floor, ANGLE_90);
			sec->SetXScale(sector_t::floor, -sec->GetXScale(sector_t::floor));
		}
		if (bsec->floorstat & 16)
		{
			sec->SetXScale(sector_t::floor, -sec->GetXScale(sector_t::floor));
		}
		if (bsec->floorstat & 32)
		{
			sec->SetYScale(sector_t::floor, -sec->GetYScale(sector_t::floor));
		}

		if (bsec->ceilingstat & 4)
		{
			sec->SetAngle(sector_t::ceiling, ANGLE_90);
			sec->SetYScale(sector_t::ceiling, -sec->GetYScale(sector_t::ceiling));
		}
		if (bsec->ceilingstat & 16)
		{
			sec->SetXScale(sector_t::ceiling, -sec->GetXScale(sector_t::ceiling));
		}
		if (bsec->ceilingstat & 32)
		{
			sec->SetYScale(sector_t::ceiling, -sec->GetYScale(sector_t::ceiling));
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

	sides = new side_t[numsides];
	memset (sides, 0, numsides*sizeof(side_t));

	vertexes = new vertex_t[numvertexes];
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
		char tnam[9];
		FTextureID overpic, pic;

		mysnprintf (tnam, countof(tnam), "BTIL%04d", LittleShort(walls[i].picnum));
		pic = TexMan.GetTexture (tnam, FTexture::TEX_Build);
		mysnprintf (tnam, countof(tnam), "BTIL%04d", LittleShort(walls[i].overpicnum));
		overpic = TexMan.GetTexture (tnam, FTexture::TEX_Build);

		walls[i].x = LittleLong(walls[i].x);
		walls[i].y = LittleLong(walls[i].y);
		walls[i].point2 = LittleShort(walls[i].point2);
		walls[i].cstat = LittleShort(walls[i].cstat);
		walls[i].nextwall = LittleShort(walls[i].nextwall);
		walls[i].nextsector = LittleShort(walls[i].nextsector);

		sides[i].SetTextureXOffset(walls[i].xpanning << FRACBITS);
		sides[i].SetTextureYOffset(walls[i].ypanning << FRACBITS);

		sides[i].SetTexture(side_t::top, pic);
		sides[i].SetTexture(side_t::bottom, pic);
		if (walls[i].nextsector < 0 || (walls[i].cstat & 32))
		{
			sides[i].SetTexture(side_t::mid, pic);
		}
		else if (walls[i].cstat & 16)
		{
			sides[i].SetTexture(side_t::mid, overpic);
		}
		else
		{
			sides[i].SetTexture(side_t::mid, FNullTextureID());
		}

		sides[i].TexelLength = walls[i].xrepeat * 8;
		sides[i].SetLight(SHADE2LIGHT(walls[i].shade));
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
	lines = new line_t[numlines];
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
		lines[j].flags |= ML_WRAP_MIDTEX;
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
				lines[j].Alpha = FRACUNIT/3;
			}
			else
			{
				lines[j].Alpha = FRACUNIT*2/3;
			}
		}
		if (walls[i].cstat & 1)
		{
			lines[j].flags |= ML_BLOCKING;
		}
		if (walls[i].nextwall < 0)
		{
			if (walls[i].cstat & 4)
			{
				lines[j].flags |= ML_DONTPEGBOTTOM;
			}
		}
		else
		{
			if (walls[i].cstat & 4)
			{
				lines[j].flags |= ML_DONTPEGTOP;
			}
			else
			{
				lines[j].flags |= ML_DONTPEGBOTTOM;
			}
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
		slope.i = long (sqrt ((double)(slope.dx*slope.dx+slope.dy*slope.dy))) << 5;
		if (slope.i == 0)
		{
			continue;
		}
		if ((bsec->floorstat & 2) && (bsec->floorheinum != 0))
		{ // floor is sloped
			slope.heinum = -LittleShort(bsec->floorheinum);
			slope.z[0] = slope.z[1] = slope.z[2] = -bsec->floorz;
			CalcPlane (slope, sectors[i].floorplane);
		}
		if ((bsec->ceilingstat & 2) && (bsec->ceilingheinum != 0))
		{ // ceiling is sloped
			slope.heinum = -LittleShort(bsec->ceilingheinum);
			slope.z[0] = slope.z[1] = slope.z[2] = -bsec->ceilingz;
			CalcPlane (slope, sectors[i].ceilingplane);
		}
		if (bsec->floorstat & 64)
		{ // floor is aligned to first wall
			R_AlignFlat (sides[bsec->wallptr].linenum,
				lines[sides[bsec->wallptr].linenum].sidenum[1] == (DWORD)bsec->wallptr, 0);
		}
		if (bsec->ceilingstat & 64)
		{ // ceiling is aligned to first wall
			R_AlignFlat (sides[bsec->wallptr].linenum,
				lines[sides[bsec->wallptr].linenum].sidenum[1] == (DWORD)bsec->wallptr, 0);
		}
	}
}

//==========================================================================
//
// LoadSprites
//
//==========================================================================

static int LoadSprites (spritetype *sprites, int numsprites, sectortype *bsectors,
						 FMapThing *mapthings)
{
	int count = 0;

	for (int i = 0; i < numsprites; ++i)
	{
		if (sprites[i].cstat & (16|32|32768)) continue;
		if (sprites[i].xrepeat == 0 || sprites[i].yrepeat == 0) continue;

		mapthings[count].thingid = 0;
		mapthings[count].x = (sprites[i].x << 12);
		mapthings[count].y = -(sprites[i].y << 12);
		mapthings[count].z = (bsectors[sprites[i].sectnum].floorz - sprites[i].z) << 8;
		mapthings[count].angle = (((2048-sprites[i].ang) & 2047) * 360) >> 11;
		mapthings[count].type = 9988;
		mapthings[count].ClassFilter = 0xffff;
		mapthings[count].SkillFilter = 0xffff;
		mapthings[count].flags = MTF_SINGLE|MTF_COOPERATIVE|MTF_DEATHMATCH;
		mapthings[count].special = 0;

		mapthings[count].args[0] = sprites[i].picnum & 255;
		mapthings[count].args[1] = sprites[i].picnum >> 8;
		mapthings[count].args[2] = sprites[i].xrepeat;
		mapthings[count].args[3] = sprites[i].yrepeat;
		mapthings[count].args[4] = (sprites[i].cstat & 14) | ((sprites[i].cstat >> 9) & 1);
		count++;
	}
	return count;
}

//==========================================================================
//
// FindVertex
//
//==========================================================================

vertex_t *FindVertex (fixed_t x, fixed_t y)
{
	int i;

	x <<= 12;
	y = -(y << 12);

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

static void CreateStartSpot (fixed_t *pos, FMapThing *start)
{
	short angle = LittleShort(*(WORD *)(&pos[3]));
	FMapThing mt =
	{
		0, (LittleLong(pos[0])<<12), ((-LittleLong(pos[1]))<<12), 0,// tid, x, y, z
		short(Scale ((2048-angle)&2047, 360, 2048)), 1,	// angle, type
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
	FVector3 pt[3];
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

	pt[0] = FVector3(slope.dx, -slope.dy, 0);
	pt[1] = FVector3(slope.x[2] - slope.x[0], slope.y[0] - slope.y[2], (slope.z[2] - slope.z[0]) / 16);
	pt[2] = (pt[0] ^ pt[1]).Unit();

	if ((pt[2][2] < 0 && plane.c > 0) || (pt[2][2] > 0 && plane.c < 0))
	{
		pt[2] = -pt[2];
	}

	plane.a = fixed_t(pt[2][0]*65536.f);
	plane.b = fixed_t(pt[2][1]*65536.f);
	plane.c = fixed_t(pt[2][2]*65536.f);
	plane.ic = fixed_t(65536.f/pt[2][2]);
	plane.d = -TMulScale8
		(plane.a, slope.x[0]<<4, plane.b, (-slope.y[0])<<4, plane.c, slope.z[0]);
}

//==========================================================================
//
// Decrypt
//
// Note that this is different from the general RFF encryption.
//
//==========================================================================

static void Decrypt (void *to_, const void *from_, int len, int key)
{
	BYTE *to = (BYTE *)to_;
	const BYTE *from = (const BYTE *)from_;

	for (int i = 0; i < len; ++i, ++key)
	{
		to[i] = from[i] ^ key;
	}
}

//==========================================================================
//
// Just an actor to make the Build sprites show up. It doesn't do anything
// with them other than display them.
//
//==========================================================================

class ACustomSprite : public AActor
{
	DECLARE_CLASS (ACustomSprite, AActor);
public:
	void BeginPlay ();
};

IMPLEMENT_CLASS (ACustomSprite)

void ACustomSprite::BeginPlay ()
{
	char name[9];
	Super::BeginPlay ();

	mysnprintf (name, countof(name), "BTIL%04d", (args[0] + args[1]*256) & 0xffff);
	picnum = TexMan.GetTexture (name, FTexture::TEX_Build);

	scaleX = args[2] * (FRACUNIT/64);
	scaleY = args[3] * (FRACUNIT/64);

	if (args[4] & 2)
	{
		RenderStyle = STYLE_Translucent;
		if (args[4] & 1)
			alpha = TRANSLUC66;
		else
			alpha = TRANSLUC33;
	}
	if (args[4] & 4)
		renderflags |= RF_XFLIP;
	if (args[4] & 8)
		renderflags |= RF_YFLIP;
}
