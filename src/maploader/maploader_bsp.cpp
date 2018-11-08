//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Map loader
//

#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#define rmdir _rmdir
#endif

#include <zlib.h>
#include "p_setup.h"
#include "maploader.h"
#include "files.h"
#include "i_system.h"
#include "i_time.h"
#include "nodebuild.h"
#include "doomerrors.h"
#include "info.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "m_misc.h"

// should be moved into the map loader later but for now it is referenced from too many parts elsewhere.
extern TArray<FMapThing> MapThingsConverted;


CVAR(Bool, gl_cachenodes, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Float, gl_cachetime, 0.6f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)


//===========================================================================
//
// P_LoadZSegs
//
//===========================================================================

void MapLoader::LoadZSegs (FileReader &data)
{
	for (auto &seg : segs)
	{
		line_t *ldef;
		uint32_t v1 = data.ReadUInt32();
		uint32_t v2 = data.ReadUInt32();
		uint16_t line = data.ReadUInt16();
		uint8_t side = data.ReadUInt8();

		seg.v1 = &vertexes[v1];
		seg.v2 = &vertexes[v2];
		seg.linedef = ldef = &lines[line];
		seg.sidedef = ldef->sidedef[side];
		seg.frontsector = ldef->sidedef[side]->sector;
		if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != NULL)
		{
			seg.backsector = ldef->sidedef[side^1]->sector;
		}
		else
		{
			seg.backsector = 0;
			ldef->flags &= ~ML_TWOSIDED;
		}
	}
}

//===========================================================================
//
// P_LoadGLZSegs
//
// This is the GL nodes version of the above function.
//
//===========================================================================

void MapLoader::LoadGLZSegs (FileReader &data, int type)
{
	for (unsigned i = 0; i < subsectors.Size(); ++i)
	{

		for (size_t j = 0; j < subsectors[i].numlines; ++j)
		{
			seg_t *seg;
			uint32_t v1 = data.ReadUInt32();
			uint32_t partner = data.ReadUInt32();
			uint32_t line;

			if (type >= 2)
			{
				line = data.ReadUInt32();
			}
			else
			{
				line = data.ReadUInt16();
				if (line == 0xffff) line = 0xffffffff;
			}
			uint8_t side = data.ReadUInt8();

			seg = subsectors[i].firstline + j;
			seg->v1 = &vertexes[v1];
			if (j == 0)
			{
				seg[subsectors[i].numlines - 1].v2 = seg->v1;
			}
			else
			{
				seg[-1].v2 = seg->v1;
			}
			
			seg->PartnerSeg = partner == 0xffffffffu? nullptr : &segs[partner];
			if (line != 0xFFFFFFFF)
			{
				line_t *ldef;

				seg->linedef = ldef = &lines[line];
				seg->sidedef = ldef->sidedef[side];
				seg->frontsector = ldef->sidedef[side]->sector;
				if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != NULL)
				{
					seg->backsector = ldef->sidedef[side^1]->sector;
				}
				else
				{
					seg->backsector = 0;
					ldef->flags &= ~ML_TWOSIDED;
				}
			}
			else
			{
				seg->linedef = NULL;
				seg->sidedef = NULL;
				seg->frontsector = seg->backsector = subsectors[i].firstline->frontsector;
			}
		}
	}
}

//===========================================================================
//
// P_LoadZNodes
//
//===========================================================================

void MapLoader::LoadZNodes(FileReader &data, int glnodes)
{
	// Read extra vertices added during node building
	unsigned int i;

	uint32_t orgVerts = data.ReadUInt32();
	uint32_t newVerts = data.ReadUInt32();
	if (orgVerts > vertexes.Size())
	{ // These nodes are based on a map with more vertex data than we have.
	  // We can't use them.
		I_Error("Incorrect number of vertexes in nodes.\n");
	}
	auto oldvertexes = &vertexes[0];
	if (orgVerts + newVerts != vertexes.Size())
	{
		vertexes.Reserve(newVerts);
	}
	for (i = 0; i < newVerts; ++i)
	{
		fixed_t x = data.ReadInt32();
		fixed_t y = data.ReadInt32();
		vertexes[i + orgVerts].set(x, y);
	}
	if (oldvertexes != &vertexes[0])
	{
		for (auto &line : lines)
		{
			line.v1 = line.v1 - oldvertexes + &vertexes[0];
			line.v2 = line.v2 - oldvertexes + &vertexes[0];
		}
	}

	// Read the subsectors
	uint32_t currSeg;
	uint32_t numSubs = data.ReadUInt32();
	subsectors.Alloc(numSubs);
	memset (&subsectors[0], 0, subsectors.Size()*sizeof(subsector_t));

	for (i = currSeg = 0; i < numSubs; ++i)
	{
		uint32_t numsegs = data.ReadUInt32();
		subsectors[i].firstline = (seg_t *)(size_t)currSeg;		// Oh damn. I should have stored the seg count sooner.
		subsectors[i].numlines = numsegs;
		currSeg += numsegs;
	}

	// Read the segs
	uint32_t numSegs = data.ReadUInt32();

	// The number of segs stored should match the number of
	// segs used by subsectors.
	if (numSegs != currSeg)
	{
		I_Error("Incorrect number of segs in nodes.\n");
	}

	segs.Alloc(numSegs);
	memset (&segs[0], 0, numSegs*sizeof(seg_t));

	for (auto &sub : subsectors)
	{
		sub.firstline = &segs[(size_t)sub.firstline];
	}

	if (glnodes == 0)
	{
		LoadZSegs (data);
	}
	else
	{
		LoadGLZSegs (data, glnodes);
	}

	// Read nodes
	uint32_t numNodes = data.ReadUInt32();

	nodes.Alloc(numNodes);
	memset (&nodes[0], 0, sizeof(node_t)*numNodes);

	for (i = 0; i < numNodes; ++i)
	{
		if (glnodes < 3)
		{
			nodes[i].x = data.ReadInt16() * FRACUNIT;
			nodes[i].y = data.ReadInt16() * FRACUNIT;
			nodes[i].dx = data.ReadInt16() * FRACUNIT;
			nodes[i].dy = data.ReadInt16() * FRACUNIT;
		}
		else
		{
			nodes[i].x = data.ReadInt32();
			nodes[i].y = data.ReadInt32();
			nodes[i].dx = data.ReadInt32();
			nodes[i].dy = data.ReadInt32();
		}
		for (int j = 0; j < 2; ++j)
		{
			for (int k = 0; k < 4; ++k)
			{
				nodes[i].bbox[j][k] = data.ReadInt16();
			}
		}
		for (int m = 0; m < 2; ++m)
		{
			uint32_t child = data.ReadUInt32();
			if (child & 0x80000000)
			{
				nodes[i].children[m] = (uint8_t *)&subsectors[child & 0x7FFFFFFF] + 1;
			}
			else
			{
				nodes[i].children[m] = &nodes[child];
			}
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================

void MapLoader::LoadZNodes (FileReader &dalump, uint32_t id)
{
	int type;
	bool compressed;

	switch (id)
	{
	case MAKE_ID('Z','N','O','D'):
		type = 0;
		compressed = true;
		break;

	case MAKE_ID('Z','G','L','N'):
		type = 1;
		compressed = true;
		break;

	case MAKE_ID('Z','G','L','2'):
		type = 2;
		compressed = true;
		break;

	case MAKE_ID('Z','G','L','3'):
		type = 3;
		compressed = true;
		break;

	case MAKE_ID('X','N','O','D'):
		type = 0;
		compressed = false;
		break;

	case MAKE_ID('X','G','L','N'):
		type = 1;
		compressed = false;
		break;

	case MAKE_ID('X','G','L','2'):
		type = 2;
		compressed = false;
		break;

	case MAKE_ID('X','G','L','3'):
		type = 3;
		compressed = false;
		break;

	default:
		return;
	}
	
	if (compressed)
	{
		FileReader zip;
		if (zip.OpenDecompressor(dalump, -1, METHOD_ZLIB, false))
		{
			LoadZNodes(zip, type);
		}
	}
	else
	{
		LoadZNodes(dalump, type);
	}
}



//===========================================================================
//
// P_CheckV4Nodes
// http://www.sbsoftware.com/files/DeePBSPV4specs.txt
//
//===========================================================================

bool MapLoader::CheckV4Nodes(MapData *map)
{
	char header[8];

	map->Read(ML_NODES, header, 8);
	return !memcmp(header, "xNd4\0\0\0\0", 8);
}


//===========================================================================
//
// P_LoadSegs
//
// killough 5/3/98: reformatted, cleaned up
//
//===========================================================================

struct badseg
{
	badseg(int t, int s, int d) : badtype(t), badsegnum(s), baddata(d) {}
	int badtype;
	int badsegnum;
	int baddata;
};

template<class segtype>
void MapLoader::LoadSegs (MapData * map)
{
	uint8_t *data;
	int numvertexes = vertexes.Size();
	uint8_t *vertchanged = new uint8_t[numvertexes];	// phares 10/4/98
	uint32_t segangle;
	//int ptp_angle;		// phares 10/4/98
	//int delta_angle;	// phares 10/4/98
	int vnum1,vnum2;	// phares 10/4/98
	int lumplen = map->Size(ML_SEGS);

	memset (vertchanged,0,numvertexes); // phares 10/4/98

	unsigned numsegs = lumplen / sizeof(segtype);

	if (numsegs == 0)
	{
		Printf ("This map has no segs.\n");
		subsectors.Clear();
		nodes.Clear();
		delete[] vertchanged;
		ForceNodeBuild = true;
		return;
	}

	segs.Alloc(numsegs);
	memset (&segs[0], 0, numsegs*sizeof(seg_t));

	data = new uint8_t[lumplen];
	map->Read(ML_SEGS, data);

	for (auto &sub : subsectors)
	{
		sub.firstline = &segs[(size_t)sub.firstline];
	}

	// phares: 10/4/98: Vertchanged is an array that represents the vertices.
	// Mark those used by linedefs. A marked vertex is one that is not a
	// candidate for movement further down.

	for (auto &line : lines)
	{
		vertchanged[line.v1->Index()] = vertchanged[line.v2->Index()] = 1;
	}

	try
	{
		for (unsigned i = 0; i < numsegs; i++)
		{
			seg_t *li = &segs[i];
			segtype *ml = ((segtype *) data) + i;

			int side, linedef;
			line_t *ldef;

			vnum1 = ml->V1();
			vnum2 = ml->V2();

			if (vnum1 >= numvertexes || vnum2 >= numvertexes)
			{
				throw badseg(0, i, MAX(vnum1, vnum2));
			}

			li->v1 = &vertexes[vnum1];
			li->v2 = &vertexes[vnum2];

			segangle = (uint16_t)LittleShort(ml->angle);

			// phares 10/4/98: In the case of a lineseg that was created by splitting
			// another line, it appears that the line angle is inherited from the
			// father line. Due to roundoff, the new vertex may have been placed 'off
			// the line'. When you get close to such a line, and it is very short,
			// it's possible that the roundoff error causes 'firelines', the thin
			// lines that can draw from screen top to screen bottom occasionally. This
			// is due to all the angle calculations that are done based on the line
			// angle, the angles from the viewer to the vertices, and the viewer's
			// angle in the world. In the case of firelines, the rounded-off position
			// of one of the vertices determines one of these angles, and introduces
			// an error in the scaling factor for mapping textures and determining
			// where on the screen the ceiling and floor spans should be shown. For a
			// fireline, the engine thinks the ceiling bottom and floor top are at the
			// midpoint of the screen. So you get ceilings drawn all the way down to the
			// screen midpoint, and floors drawn all the way up. Thus 'firelines'. The
			// name comes from the original sighting, which involved a fire texture.
			//
			// To correct this, reset the vertex that was added so that it sits ON the
			// split line.
			//
			// To know which of the two vertices was added, its number is greater than
			// that of the last of the author-created vertices. If both vertices of the
			// line were added by splitting, pick the higher-numbered one. Once you've
			// changed a vertex, don't change it again if it shows up in another seg.
			//
			// To determine if there's an error in the first place, find the
			// angle of the line between the two seg vertices. If it's one degree or more
			// off, then move one vertex. This may seem insignificant, but one degree
			// errors _can_ cause firelines.

			DAngle ptp_angle = (li->v2->fPos() - li->v1->fPos()).Angle();
			DAngle seg_angle = AngleToFloat(segangle << 16);
			DAngle delta_angle = absangle(ptp_angle, seg_angle);

			if (delta_angle >= 1.)
			{
				double dis = (li->v2->fPos() - li->v1->fPos()).Length();
				DVector2 delta = seg_angle.ToVector(dis);
				if ((vnum2 > vnum1) && (vertchanged[vnum2] == 0))
				{
					li->v2->set(li->v1->fPos() + delta);
					vertchanged[vnum2] = 1; // this was changed
				}
				else if (vertchanged[vnum1] == 0)
				{
					li->v1->set(li->v2->fPos() - delta);
					vertchanged[vnum1] = 1; // this was changed
				}
			}

			linedef = LittleShort(ml->linedef);
			if ((unsigned)linedef >= lines.Size())
			{
				throw badseg(1, i, linedef);
			}
			ldef = &lines[linedef];
			li->linedef = ldef;
			side = LittleShort(ml->side);
			if ((unsigned)(ldef->sidedef[side]->Index()) >= sides.Size())
			{
				throw badseg(2, i, ldef->sidedef[side]->Index());
			}
			li->sidedef = ldef->sidedef[side];
			li->frontsector = ldef->sidedef[side]->sector;

			// killough 5/3/98: ignore 2s flag if second sidedef missing:
			if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != NULL)
			{
				li->backsector = ldef->sidedef[side^1]->sector;
			}
			else
			{
				li->backsector = 0;
				ldef->flags &= ~ML_TWOSIDED;
			}
		}
	}
	catch (const badseg &bad) // the preferred way is to catch by (const) reference.
	{
		switch (bad.badtype)
		{
		case 0:
			Printf ("Seg %d references a nonexistant vertex %d (max %d).\n", bad.badsegnum, bad.baddata, numvertexes);
			break;

		case 1:
			Printf ("Seg %d references a nonexistant linedef %d (max %u).\n", bad.badsegnum, bad.baddata, lines.Size());
			break;

		case 2:
			Printf ("The linedef for seg %d references a nonexistant sidedef %d (max %d).\n", bad.badsegnum, bad.baddata, sides.Size());
			break;
		}
		Printf ("The BSP will be rebuilt.\n");
		segs.Clear();
		subsectors.Clear();
		nodes.Clear();
		ForceNodeBuild = true;
	}

	delete[] vertchanged; // phares 10/4/98
	delete[] data;
}


//===========================================================================
//
// P_LoadSubsectors
//
//===========================================================================

template<class subsectortype, class segtype>
void MapLoader::LoadSubsectors (MapData * map)
{
	uint32_t maxseg = map->Size(ML_SEGS) / sizeof(segtype);

	unsigned numsubsectors = map->Size(ML_SSECTORS) / sizeof(subsectortype);

	if (numsubsectors == 0 || maxseg == 0 )
	{
		Printf ("This map has an incomplete BSP tree.\n");
		nodes.Clear();
		ForceNodeBuild = true;
		return;
	}

	subsectors.Alloc(numsubsectors);
	auto &fr = map->Reader(ML_SSECTORS);
		
	memset (&subsectors[0], 0, numsubsectors*sizeof(subsector_t));
	
	for (unsigned i = 0; i < numsubsectors; i++)
	{
		subsectortype subd;

		subd.numsegs = sizeof(subd.numsegs) == 2 ? fr.ReadUInt16() : fr.ReadUInt32();
		subd.firstseg = sizeof(subd.firstseg) == 2 ? fr.ReadUInt16() : fr.ReadUInt32();

		if (subd.numsegs == 0)
		{
			Printf ("Subsector %i is empty.\n", i);
			subsectors.Clear();
			nodes.Clear();
			ForceNodeBuild = true;
			return;
		}

		subsectors[i].numlines = subd.numsegs;
		subsectors[i].firstline = (seg_t *)(size_t)subd.firstseg;

		if ((size_t)subsectors[i].firstline >= maxseg)
		{
			Printf ("Subsector %d contains invalid segs %u-%u\n"
				"The BSP will be rebuilt.\n", i, (unsigned)((size_t)subsectors[i].firstline),
				(unsigned)((size_t)subsectors[i].firstline) + subsectors[i].numlines - 1);
			ForceNodeBuild = true;
			nodes.Clear();
			subsectors.Clear();
			break;
		}
		else if ((size_t)subsectors[i].firstline + subsectors[i].numlines > maxseg)
		{
			Printf ("Subsector %d contains invalid segs %u-%u\n"
				"The BSP will be rebuilt.\n", i, maxseg,
				(unsigned)((size_t)subsectors[i].firstline) + subsectors[i].numlines - 1);
			ForceNodeBuild = true;
			nodes.Clear();
			subsectors.Clear();
			break;
		}
	}
}


//===========================================================================
//
// P_LoadNodes
//
//===========================================================================

template<class nodetype, class subsectortype>
void MapLoader::LoadNodes (MapData * map)
{
	int 		j;
	int 		k;
	char		*mnp;
	nodetype	*mn;
	node_t* 	no;
	uint16_t*		used;
	int			lumplen = map->Size(ML_NODES);
	int			maxss = map->Size(ML_SSECTORS) / sizeof(subsectortype);

	unsigned numnodes = (lumplen - nodetype::NF_LUMPOFFSET) / sizeof(nodetype);

	if ((numnodes == 0 && maxss != 1) || maxss == 0)
	{
		ForceNodeBuild = true;
		return;
	}
	
	nodes.Alloc(numnodes);		
	used = (uint16_t *)alloca (sizeof(uint16_t)*numnodes);
	memset (used, 0, sizeof(uint16_t)*numnodes);

	
	mnp = new char[lumplen];
	mn = (nodetype*)(mnp + nodetype::NF_LUMPOFFSET);
	map->Read(ML_NODES, mnp);
	no = &nodes[0];
	
	for (unsigned i = 0; i < numnodes; i++, no++, mn++)
	{
		no->x = LittleShort(mn->x)<<FRACBITS;
		no->y = LittleShort(mn->y)<<FRACBITS;
		no->dx = LittleShort(mn->dx)<<FRACBITS;
		no->dy = LittleShort(mn->dy)<<FRACBITS;
		for (j = 0; j < 2; j++)
		{
			int child = mn->Child(j);
			if (child & nodetype::NF_SUBSECTOR)
			{
				child &= ~nodetype::NF_SUBSECTOR;
				if (child >= maxss)
				{
					Printf ("BSP node %d references invalid subsector %d.\n"
						"The BSP will be rebuilt.\n", i, child);
					ForceNodeBuild = true;
					nodes.Clear();
					delete[] mnp;
					return;
				}
				no->children[j] = (uint8_t *)&subsectors[child] + 1;
			}
			else if ((unsigned)child >= numnodes)
			{
				Printf ("BSP node %d references invalid node %d.\n"
					"The BSP will be rebuilt.\n", i, ((node_t *)no->children[j])->Index());
				ForceNodeBuild = true;
				nodes.Clear();
				delete[] mnp;
				return;
			}
			else if (used[child])
			{
				Printf ("BSP node %d references node %d,\n"
					"which is already used by node %d.\n"
					"The BSP will be rebuilt.\n", i, child, used[child]-1);
				ForceNodeBuild = true;
				nodes.Clear();
				delete[] mnp;
				return;
			}
			else
			{
				no->children[j] = &nodes[child];
				used[child] = j + 1;
			}
			for (k = 0; k < 4; k++)
			{
				no->bbox[j][k] = (float)LittleShort(mn->bbox[j][k]);
			}
		}
	}
	delete[] mnp;
}


//===========================================================================
//
//
//
//===========================================================================

void MapLoader::GetPolySpots (MapData * map, TArray<FNodeBuilder::FPolyStart> &spots, TArray<FNodeBuilder::FPolyStart> &anchors)
{
    //if (map->HasBehavior)
    {
        for (unsigned int i = 0; i < MapThingsConverted.Size(); ++i)
        {
            FDoomEdEntry *mentry = MapThingsConverted[i].info;
            if (mentry != NULL && mentry->Type == NULL && mentry->Special >= SMT_PolyAnchor && mentry->Special <= SMT_PolySpawnHurt)
            {
                FNodeBuilder::FPolyStart newvert;
                newvert.x = FLOAT2FIXED(MapThingsConverted[i].pos.X);
                newvert.y = FLOAT2FIXED(MapThingsConverted[i].pos.Y);
                newvert.polynum = MapThingsConverted[i].angle;
                if (mentry->Special == SMT_PolyAnchor)
                {
                    anchors.Push (newvert);
                }
                else
                {
                    spots.Push (newvert);
                }
            }
        }
    }
}

//==========================================================================
//
// Checks whether nodes are GL friendly or not
//
//==========================================================================

bool MapLoader::CheckNodes(MapData * map, bool rebuilt, int buildtime)
{
    bool ret = false;
    bool loaded = false;
    
    // If the map loading code has performed a node rebuild we don't need to check for it again.
    if (!rebuilt && !CheckForGLNodes())
    {
        ret = true;    // we are not using the level's original nodes if we get here.
        for (auto &sub : subsectors)
        {
            sub.sector = sub.firstline->sidedef->sector;
        }
        
        // The nodes and subsectors need to be preserved for gameplay related purposes.
        gamenodes = std::move(nodes);
        gamesubsectors = std::move(subsectors);
        segs.Clear();
        
        // Try to load GL nodes (cached or GWA)
        loaded = P_LoadGLNodes(map);
        if (!loaded)
        {
            // none found - we have to build new ones!
            uint64_t startTime, endTime;
            
            startTime = I_msTime ();
            TArray<FNodeBuilder::FPolyStart> polyspots, anchors;
            GetPolySpots (map, polyspots, anchors);
            FNodeBuilder::FLevel leveldata =
            {
                &vertexes[0], (int)vertexes.Size(),
                &sides[0], (int)sides.Size(),
                &lines[0], (int)lines.Size(),
                0, 0, 0, 0
            };
            leveldata.FindMapBounds ();
            FNodeBuilder builder (leveldata, polyspots, anchors, true);
            
            builder.Extract (*this);
            endTime = I_msTime ();
            DPrintf (DMSG_NOTIFY, "BSP generation took %.3f sec (%u segs)\n", (endTime - startTime) * 0.001, segs.Size());
            buildtime = (int32_t)(endTime - startTime);
        }
    }
    
    if (!loaded)
    {
#ifdef DEBUG
        // Building nodes in debug is much slower so let's cache them only if cachetime is 0
        buildtime = 0;
#endif
        if (gl_cachenodes && buildtime/1000.f >= gl_cachetime)
        {
            DPrintf(DMSG_NOTIFY, "Caching nodes\n");
            CreateCachedNodes(map);
        }
        else
        {
            DPrintf(DMSG_NOTIFY, "Not caching nodes (time = %f)\n", buildtime/1000.f);
        }
    }
    return ret;
}

//==========================================================================
//
// Node caching
//
//==========================================================================

typedef TArray<uint8_t> MemFile;

static FString CreateCacheName(MapData *map, bool create)
{
    FString path = M_GetCachePath(create);
    FString lumpname = Wads.GetLumpFullPath(map->lumpnum);
    int separator = lumpname.IndexOf(':');
    path << '/' << lumpname.Left(separator);
    if (create) CreatePath(path);
    
    lumpname.ReplaceChars('/', '%');
    lumpname.ReplaceChars(':', '$');
    path << '/' << lumpname.Right(lumpname.Len() - separator - 1) << ".gzc";
    return path;
}

static void WriteByte(MemFile &f, uint8_t b)
{
    f.Push(b);
}

static void WriteWord(MemFile &f, uint16_t b)
{
    int v = f.Reserve(2);
    f[v] = (uint8_t)b;
    f[v+1] = (uint8_t)(b>>8);
}

static void WriteLong(MemFile &f, uint32_t b)
{
    int v = f.Reserve(4);
    f[v] = (uint8_t)b;
    f[v+1] = (uint8_t)(b>>8);
    f[v+2] = (uint8_t)(b>>16);
    f[v+3] = (uint8_t)(b>>24);
}

//==========================================================================
//
//
//
//==========================================================================

void MapLoader::CreateCachedNodes(MapData *map)
{
    MemFile ZNodes;
    
    WriteLong(ZNodes, 0);
    WriteLong(ZNodes, vertexes.Size());
    for(auto &vert : vertexes)
    {
        WriteLong(ZNodes, vert.fixX());
        WriteLong(ZNodes, vert.fixY());
    }
    
    WriteLong(ZNodes, subsectors.Size());
    for (auto &sub : subsectors)
    {
        WriteLong(ZNodes, sub.numlines);
    }
    
    WriteLong(ZNodes, segs.Size());
    for(auto &seg : segs)
    {
        WriteLong(ZNodes, seg.v1->Index());
        WriteLong(ZNodes, seg.PartnerSeg == nullptr? 0xffffffffu : uint32_t(seg.PartnerSeg->Index()));
        if (seg.linedef)
        {
            WriteLong(ZNodes, uint32_t(seg.linedef->Index()));
            WriteByte(ZNodes, seg.sidedef == seg.linedef->sidedef[0]? 0:1);
        }
        else
        {
            WriteLong(ZNodes, 0xffffffffu);
            WriteByte(ZNodes, 0);
        }
    }
    
    WriteLong(ZNodes, nodes.Size());
    for(auto &node : nodes)
    {
        WriteLong(ZNodes, node.x);
        WriteLong(ZNodes, node.y);
        WriteLong(ZNodes, node.dx);
        WriteLong(ZNodes, node.dy);
        for (int j = 0; j < 2; ++j)
        {
            for (int k = 0; k < 4; ++k)
            {
                WriteWord(ZNodes, (short)node.bbox[j][k]);
            }
        }
        
        for (int j = 0; j < 2; ++j)
        {
            uint32_t child;
            if ((size_t)node.children[j] & 1)
            {
                child = 0x80000000 | uint32_t(((subsector_t *)((uint8_t *)node.children[j] - 1))->Index());
            }
            else
            {
                child = ((node_t *)node.children[j])->Index();
            }
            WriteLong(ZNodes, child);
        }
    }
    
    uLongf outlen = ZNodes.Size();
    uint8_t *compressed;
    int offset = lines.Size() * 8 + 12 + 16;
    int r;
    do
    {
        compressed = new Bytef[outlen + offset];
        r = compress (compressed + offset, &outlen, &ZNodes[0], ZNodes.Size());
        if (r == Z_BUF_ERROR)
        {
            delete[] compressed;
            outlen += 1024;
        }
    }
    while (r == Z_BUF_ERROR);
    
    memcpy(compressed, "CACH", 4);
    uint32_t len = LittleLong(lines.Size());
    memcpy(compressed+4, &len, 4);
    map->GetChecksum(compressed+8);
    for (unsigned i = 0; i < lines.Size(); i++)
    {
        uint32_t ndx[2] = { LittleLong(uint32_t(lines[i].v1->Index())), LittleLong(uint32_t(lines[i].v2->Index())) };
        memcpy(compressed + 8 + 16 + 8 * i, ndx, 8);
    }
    memcpy(compressed + offset - 4, "ZGL3", 4);
    
    FString path = CreateCacheName(map, true);
    FileWriter *fw = FileWriter::Open(path);
    
    if (fw != nullptr)
    {
        const size_t length = outlen + offset;
        if (fw->Write(compressed, length) != length)
        {
            Printf("Error saving nodes to file %s\n", path.GetChars());
        }
        delete fw;
    }
    else
    {
        Printf("Cannot open nodes file %s for writing\n", path.GetChars());
    }
    
    delete [] compressed;
}

//==========================================================================
//
//
//
//==========================================================================

bool MapLoader::CheckCachedNodes(MapData *map)
{
    char magic[4] = {0,0,0,0};
    uint8_t md5[16];
    uint8_t md5map[16];
    uint32_t numlin;
    TArray<uint32_t> Verts;
    
    FString path = CreateCacheName(map, false);
    FileReader fr;
    
    if (!fr.OpenFile(path)) return false;
    
    if (fr.Read(magic, 4) != 4) return false;
    if (memcmp(magic, "CACH", 4))  return false;
    
    if (fr.Read(&numlin, 4) != 4) return false;
    numlin = LittleLong(numlin);
    if (numlin != lines.Size()) return false;
    
    if (fr.Read(md5, 16) != 16) return false;
    map->GetChecksum(md5map);
    if (memcmp(md5, md5map, 16)) return false;
    
    Verts.Resize(numlin * 8);
    if (fr.Read(Verts.Data(), 8 * numlin) != 8 * numlin) return false;
    
    if (fr.Read(magic, 4) != 4) return false;
    if (memcmp(magic, "ZGL2", 4) && memcmp(magic, "ZGL3", 4))  return false;
    
    
    try
    {
        LoadZNodes (fr, MAKE_ID(magic[0],magic[1],magic[2],magic[3]));
    }
    catch (CRecoverableError &error)
    {
        Printf ("Error loading nodes: %s\n", error.GetMessage());
        
        subsectors.Clear();
        segs.Clear();
        nodes.Clear();
        return false;
    }
    
    for(auto &line : lines)
    {
        int i = line.Index();
        line.v1 = &vertexes[LittleLong(Verts[i*2])];
        line.v2 = &vertexes[LittleLong(Verts[i*2+1])];
    }
    return true;
}

//===========================================================================
//
// P_LoadBsp
//
//===========================================================================

bool MapLoader::LoadBsp(MapData *map)
{
    bool reloop = false;
    if (!ForceNodeBuild)
    {
        // Check for compressed nodes first, then uncompressed nodes
        FileReader *fr = nullptr;
        uint32_t id = MAKE_ID('X','x','X','x'), idcheck = 0, idcheck2 = 0, idcheck3 = 0, idcheck4 = 0, idcheck5 = 0, idcheck6 = 0;
        
        if (map->Size(ML_ZNODES) != 0)
        {
            // Test normal nodes first
            fr = &map->Reader(ML_ZNODES);
            idcheck = MAKE_ID('Z','N','O','D');
            idcheck2 = MAKE_ID('X','N','O','D');
        }
        else if (map->Size(ML_GLZNODES) != 0)
        {
            fr = &map->Reader(ML_GLZNODES);
            idcheck = MAKE_ID('Z','G','L','N');
            idcheck2 = MAKE_ID('Z','G','L','2');
            idcheck3 = MAKE_ID('Z','G','L','3');
            idcheck4 = MAKE_ID('X','G','L','N');
            idcheck5 = MAKE_ID('X','G','L','2');
            idcheck6 = MAKE_ID('X','G','L','3');
        }
        
        if (fr != nullptr && fr->isOpen()) fr->Read (&id, 4);
        if (id != 0 && (id == idcheck || id == idcheck2 || id == idcheck3 || id == idcheck4 || id == idcheck5 || id == idcheck6))
        {
            try
            {
                LoadZNodes (*fr, id);
            }
            catch (CRecoverableError &error)
            {
                Printf ("Error loading nodes: %s\n", error.GetMessage());
                
                ForceNodeBuild = true;
                subsectors.Clear();
                segs.Clear();
                nodes.Clear();
            }
        }
        else if (!map->isText)    // regular nodes are not supported for text maps
        {
            // If all 3 node related lumps are empty there's no need to output a message.
            // This just means that the map has no nodes and the engine is supposed to build them.
            if (map->Size(ML_SEGS) != 0 || map->Size(ML_SSECTORS) != 0 || map->Size(ML_NODES) != 0)
            {
                if (!CheckV4Nodes(map))
                {
                    LoadSubsectors<mapsubsector_t, mapseg_t> (map);
                    if (!ForceNodeBuild) LoadNodes<mapnode_t, mapsubsector_t> (map);
                    if (!ForceNodeBuild) LoadSegs<mapseg_t> (map);
                }
                else
                {
                    LoadSubsectors<mapsubsector4_t, mapseg4_t> (map);
                    if (!ForceNodeBuild) LoadNodes<mapnode4_t, mapsubsector4_t> (map);
                    if (!ForceNodeBuild) LoadSegs<mapseg4_t> (map);
                }
            }
            else ForceNodeBuild = true;
        }
        else ForceNodeBuild = true;
        
        // If loading the regular nodes failed try GL nodes before considering a rebuild
        if (ForceNodeBuild)
        {
            if (P_LoadGLNodes(map))
            {
                ForceNodeBuild = false;
                reloop = true;
            }
        }
    }
    else reloop = true;
    
    uint64_t startTime=0, endTime=0;
    if (ForceNodeBuild)
    {
        startTime = I_msTime ();
        TArray<FNodeBuilder::FPolyStart> polyspots, anchors;
        GetPolySpots (map, polyspots, anchors);
        FNodeBuilder::FLevel leveldata =
        {
            &vertexes[0], (int)vertexes.Size(),
            &sides[0], (int)sides.Size(),
            &lines[0], (int)lines.Size(),
            0, 0, 0, 0
        };
        leveldata.FindMapBounds ();
        // We need GL nodes if am_textured is on.
        // In case a sync critical game mode is started, also build GL nodes to avoid problems
        // if the different machines' am_textured setting differs.
        FNodeBuilder builder (leveldata, polyspots, anchors, true);
        builder.Extract (*this);
        endTime = I_msTime ();
        DPrintf (DMSG_NOTIFY, "BSP generation took %.3f sec (%d segs)\n", (endTime - startTime) * 0.001, segs.Size());
        oldvertextable = builder.GetOldVertexTable();
        reloop = true;
    }
    else
    {
        // Older ZDBSPs had problems with compressed sidedefs and assigned wrong sides to the segs if both sides were the same sidedef.
        for(auto &seg : segs)
        {
            if (seg.backsector == seg.frontsector && seg.linedef)
            {
                double d1 = (seg.v1->fPos() - seg.linedef->v1->fPos()).LengthSquared();
                double d2 = (seg.v2->fPos() - seg.linedef->v1->fPos()).LengthSquared();
                
                if (d2<d1)    // backside
                {
                    seg.sidedef = seg.linedef->sidedef[1];
                }
                else    // front side
                {
                    seg.sidedef = seg.linedef->sidedef[0];
                }
            }
        }
    }
    

    // Build GL nodes if we want a textured automap or GL nodes are forced to be built.
    // If the original nodes being loaded are not GL nodes they will be kept around for
    // use in P_PointInSubsector to avoid problems with maps that depend on the specific
    // nodes they were built with (P:AR E1M3 is a good example for a map where this is the case.)
    reloop |= CheckNodes(map, true, (uint32_t)(endTime - startTime));
 
    return reloop;
}

//==========================================================================
//
//
//
//==========================================================================

UNSAFE_CCMD(clearnodecache)
{
    TArray<FFileList> list;
    FString path = M_GetCachePath(false);
    path += "/";
    
    try
    {
        ScanDirectory(list, path);
    }
    catch (CRecoverableError &err)
    {
        Printf("%s\n", err.GetMessage());
        return;
    }
    
    // Scan list backwards so that when we reach a directory
    // all files within are already deleted.
    for(int i = list.Size()-1; i >= 0; i--)
    {
        if (list[i].isDirectory)
        {
            rmdir(list[i].Filename);
        }
        else
        {
            remove(list[i].Filename);
        }
    }
}
