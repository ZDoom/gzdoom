/*
** gl_nodes.cpp
**
**---------------------------------------------------------------------------
** Copyright 2005-2010 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
#include <math.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <miniz.h>

#include "m_argv.h"
#include "c_dispatch.h"
#include "m_swap.h"
#include "filesystem.h"
#include "p_local.h"
#include "nodebuild.h"
#include "doomstat.h"
#include "engineerrors.h"
#include "p_setup.h"
#include "version.h"
#include "md5.h"
#include "m_misc.h"
#include "cmdlib.h"
#include "g_levellocals.h"
#include "i_time.h"
#include "maploader.h"
#include "fs_findfile.h"

EXTERN_CVAR(Bool, gl_cachenodes)
EXTERN_CVAR(Float, gl_cachetime)

// fixed 32 bit gl_vert format v2.0+ (glBsp 1.91)
struct mapglvertex_t
{
	int32_t x,y;
};

struct gl3_mapsubsector_t
{
	int32_t numsegs;
	int32_t firstseg;    // Index of first one; segs are stored sequentially.
};

struct glseg_t
{
	uint16_t	v1;		 // start vertex		(16 bit)
	uint16_t	v2;		 // end vertex			(16 bit)
	uint16_t	linedef; // linedef, or -1 for minisegs
	uint16_t	side;	 // side on linedef: 0 for right, 1 for left
	uint16_t	partner; // corresponding partner seg, or 0xffff on one-sided walls
};

struct glseg3_t
{
	int32_t			v1;
	int32_t			v2;
	uint16_t			linedef;
	uint16_t			side;
	int32_t			partner;
};

struct gl5_mapnode_t
{
	int16_t 	x,y,dx,dy;	// partition line
	int16_t 	bbox[2][4];	// bounding box for each child
	// If NF_SUBSECTOR is or'ed in, it's a subsector,
	// else it's a node of another subtree.
	uint32_t children[2];
};



//==========================================================================
//
// Collect all sidedefs which are not entirely covered by segs
// Old ZDBSPs could create such maps. If such a BSP is discovered
// a node rebuild must be done to ensure proper rendering
//
//==========================================================================

int MapLoader::CheckForMissingSegs()
{
	auto numsides = Level->sides.Size();
	double *added_seglen = new double[numsides];
	int missing = 0;

	memset(added_seglen, 0, sizeof(double)*numsides);
	for (auto &seg : Level->segs)
	{
		if (seg.sidedef != nullptr)
		{
			// check all the segs and calculate the length they occupy on their sidedef
			DVector2 vec1(seg.v2->fX() - seg.v1->fX(), seg.v2->fY() - seg.v1->fY());
			added_seglen[Index(seg.sidedef)] += vec1.Length();
		}
	}

	for (unsigned i = 0; i < numsides; i++)
	{
		double linelen = Level->sides[i].linedef->Delta().Length();
		missing += (added_seglen[i] < linelen - 1.);
	}

	delete[] added_seglen;
	return missing;
}

//==========================================================================
//
// Checks whether the nodes are suitable for GL rendering
//
//==========================================================================

bool MapLoader::CheckForGLNodes()
{
	for(auto &sub : Level->subsectors)
	{
		seg_t * firstseg = sub.firstline;
		seg_t * lastseg = sub.firstline + sub.numlines - 1;

		if (firstseg->v1 != lastseg->v2)
		{
			// This subsector is incomplete which means that these
			// are normal nodes
			return false;
		}
		else
		{
			for(uint32_t j=0;j<sub.numlines;j++)
			{
				if (Level->segs[j].linedef==nullptr)	// miniseg
				{
					// We already have GL nodes. Great!
					return true;
				}
			}
		}
	}
	// all subsectors were closed but there are no minisegs
	// Although unlikely this can happen. Such nodes are not a problem.
	// all that is left is to check whether the BSP covers all sidedefs completely.
	int missing = CheckForMissingSegs();
	if (missing > 0)
	{
		Printf("%d missing segs counted\nThe BSP needs to be rebuilt.\n", missing);
	}
	return missing == 0;
}


//==========================================================================
//
// LoadGLVertexes
//
// loads GL vertices
//
//==========================================================================

#define gNd2		MAKE_ID('g','N','d','2')
#define gNd4		MAKE_ID('g','N','d','4')
#define gNd5		MAKE_ID('g','N','d','5')

#define GL_VERT_OFFSET  4

bool MapLoader::LoadGLVertexes(FileReader &lump)
{
	int                 i;

	firstglvertex = Level->vertexes.Size();
	
	lump.Seek(0, FileReader::SeekSet);
	auto glbuf = lump.Read();
	auto gllen=lump.GetLength();
	if (gllen < 4)
		return false;
	auto gldata = glbuf.bytes();

	if (*(int *)gldata == gNd5) 
	{
		format5=true;
	}
	else if (*(int *)gldata != gNd2) 
	{
		// GLNodes V1 and V4 are unsupported.
		// V1 because the precision is insufficient and
		// V4 due to the missing partner segs
		Printf("GL nodes v%d found. This format is not supported by " GAMENAME "\n",
			(*(int *)gldata == gNd4)? 4:1);

		return false;
	}
	else format5=false;

	mapglvertex_t*	mgl = (mapglvertex_t *)(gldata + GL_VERT_OFFSET);
	unsigned numvertexes = (unsigned)(firstglvertex +  (gllen - GL_VERT_OFFSET)/sizeof(mapglvertex_t));

	auto oldvertexes = &Level->vertexes[0];
	Level->vertexes.Resize(numvertexes);

	for(auto &line : Level->lines)
	{
		// Remap vertex pointers in linedefs
		line.v1 = &Level->vertexes[line.v1 - oldvertexes];
		line.v2 = &Level->vertexes[line.v2 - oldvertexes];
	}

	for (i = firstglvertex; i < (int)numvertexes; i++)
	{
		Level->vertexes[i].set(LittleLong(mgl->x)/65536., LittleLong(mgl->y)/65536.);
		mgl++;
	}
	return true;
}

//==========================================================================
//
// GL Nodes utilities
//
//==========================================================================

int MapLoader::checkGLVertex(int num)
{
	if (num & 0x8000)
		num = (num&0x7FFF)+firstglvertex;
	return num;
}

int MapLoader::checkGLVertex3(int num)
{
	if (num & 0xc0000000)
		num = (num&0x3FFFFFFF)+firstglvertex;
	return num;
}

//==========================================================================
//
// LoadGLSegs
//
//==========================================================================

bool MapLoader::LoadGLSegs(FileReader &lump)
{
	int			i;
	line_t		*ldef=nullptr;

	lump.Seek(0, FileReader::SeekSet);
	auto data = lump.Read();
	int numsegs = (int)lump.GetLength();
	if (numsegs < 4)
		return false;
	auto &segs = Level->segs;
	const unsigned numverts = Level->vertexes.Size();
	const unsigned numlines = Level->lines.Size(); 

	if (!format5 && memcmp(data.data(), "gNd3", 4))
	{
		numsegs/=sizeof(glseg_t);
		segs.Alloc(numsegs);
		memset(&segs[0],0,sizeof(seg_t)*numsegs);
			
		auto ml = (const glseg_t*)data.data();
		for(i = 0; i < numsegs; i++)
		{		
			// check for gl-vertices
			const unsigned v1idx = checkGLVertex(LittleShort(ml->v1));
			if (v1idx >= numverts) 
				return false;
			segs[i].v1 = &Level->vertexes[v1idx];

			const unsigned v2idx = checkGLVertex(LittleShort(ml->v2));
			if (v2idx >= numverts) 
				return false;
			segs[i].v2 = &Level->vertexes[v2idx];

			const int partner = LittleShort(ml->partner);
			if (partner == 0xFFFF)
				segs[i].PartnerSeg = nullptr;
			else if (partner >= numsegs) 
				return false;
			else 
				segs[i].PartnerSeg = &segs[partner];

			if(ml->linedef != 0xffff)
			{
				const unsigned lineidx = LittleShort(ml->linedef);
				if (lineidx >= numlines) 
					return false;
				ldef = &Level->lines[lineidx];
				segs[i].linedef = ldef;	
					
				auto side=LittleShort(ml->side);
				if (side > 1) 
					return false;
				segs[i].sidedef = ldef->sidedef[side];
				if (ldef->sidedef[side] != nullptr)
				{
					segs[i].frontsector = ldef->sidedef[side]->sector;
				}
				else
				{
					segs[i].frontsector = nullptr;
				}
				if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != nullptr)
				{
					segs[i].backsector = ldef->sidedef[side^1]->sector;
				}
				else
				{
					ldef->flags &= ~ML_TWOSIDED;
					segs[i].backsector = nullptr;
				}
	
			}
			else
			{
				segs[i].linedef = nullptr;
				segs[i].sidedef = nullptr;
	
				segs[i].frontsector = nullptr;
				segs[i].backsector  = nullptr;
			}
			ml++;		
		}
	}
	else
	{
		if (!format5) numsegs-=4;
		numsegs/=sizeof(glseg3_t);
		segs.Alloc(numsegs);
		memset(&segs[0],0,sizeof(seg_t)*numsegs);
			
		const glseg3_t * ml = (const glseg3_t*)(data.bytes() + (format5? 0:4));
		for(i = 0; i < numsegs; i++)
		{							// check for gl-vertices
			const unsigned v1idx = checkGLVertex3(LittleLong(ml->v1));
			if (v1idx >= numverts)
				return false;
			segs[i].v1 = &Level->vertexes[v1idx];

			const unsigned v2idx = checkGLVertex3(LittleLong(ml->v2));
			if (v2idx >= numverts)
				return false;
			segs[i].v2 = &Level->vertexes[v2idx];

			const uint32_t partner = LittleLong(ml->partner);
			if (partner == UINT_MAX)
				segs[i].PartnerSeg = nullptr;
			else if (partner >= (uint32_t)numsegs) 
				return false;
			else 
				segs[i].PartnerSeg = &segs[partner];
	
			if(ml->linedef != 0xffff) // skip minisegs 
			{
				const unsigned lineidx = LittleShort(ml->linedef);
				if (lineidx >= numlines)
					return false;
				ldef = &Level->lines[lineidx];
				segs[i].linedef = ldef;
	
					
				auto side=LittleShort(ml->side);
				if (side > 1)
					return false;
				segs[i].sidedef = ldef->sidedef[side];
				if (ldef->sidedef[side] != nullptr)
				{
					segs[i].frontsector = ldef->sidedef[side]->sector;
				}
				else
				{
					segs[i].frontsector = nullptr;
				}
				if (ldef->flags & ML_TWOSIDED && ldef->sidedef[side^1] != nullptr)
				{
					segs[i].backsector = ldef->sidedef[side^1]->sector;
				}
				else
				{
					ldef->flags &= ~ML_TWOSIDED;
					segs[i].backsector = nullptr;
				}
	
			}
			else
			{
				segs[i].linedef = nullptr;
				segs[i].sidedef = nullptr;
				segs[i].frontsector = nullptr;
				segs[i].backsector  = nullptr;
			}
			ml++;		
		}
	}
	return true;
}


//==========================================================================
//
// LoadGLSubsectors
//
//==========================================================================

bool MapLoader::LoadGLSubsectors(FileReader &lump)
{
	int  i;

	int numsubsectors = (int)lump.GetLength();
	if (numsubsectors < 4)
		return false;
	lump.Seek(0, FileReader::SeekSet);
	auto datab = lump.Read();
	const unsigned numsegs = Level->segs.Size();
	
	if (numsubsectors == 0)
	{
		return false;
	}
	
	if (!format5 && memcmp(datab.data(), "gNd3", 4))
	{
		auto data = (const mapsubsector_t*) datab.data();
		numsubsectors /= sizeof(mapsubsector_t);
		Level->subsectors.Alloc(numsubsectors);
		auto &subsectors = Level->subsectors;
		memset(&subsectors[0],0,numsubsectors * sizeof(subsector_t));
	
		for (i=0; i<numsubsectors; i++)
		{
			subsectors[i].numlines  = LittleShort(data[i].numsegs );
			if (subsectors[i].numlines == 0)
			{
				return false;
			}

			const unsigned firstseg = LittleShort(data[i].firstseg);
			if (firstseg >= numsegs)
				return false;
			subsectors[i].firstline = &Level->segs[firstseg];
		}
	}
	else
	{
		auto data = (const gl3_mapsubsector_t*) (datab.bytes()+(format5? 0:4));
		numsubsectors /= sizeof(gl3_mapsubsector_t);
		Level->subsectors.Alloc(numsubsectors);
		auto &subsectors = Level->subsectors;
		memset(&subsectors[0],0,numsubsectors * sizeof(subsector_t));
	
		for (i=0; i<numsubsectors; i++)
		{
			subsectors[i].numlines  = LittleLong(data[i].numsegs );
			if (subsectors[i].numlines == 0)
			{
				return false;
			}

			const unsigned firstseg = LittleLong(data[i].firstseg);
			if (firstseg >= numsegs)
				return false;
			subsectors[i].firstline = &Level->segs[firstseg];
		}
	}

	for (auto &sub : Level->subsectors)
	{
		for(unsigned j=0;j<sub.numlines;j++)
		{
			seg_t * seg = sub.firstline + j;
			if (seg->linedef==nullptr) seg->frontsector = seg->backsector = sub.firstline->frontsector;
		}
		seg_t *firstseg = sub.firstline;
		seg_t *lastseg = sub.firstline + sub.numlines - 1;
		// The subsector must be closed. If it isn't we can't use these nodes and have to do a rebuild.
		if (lastseg->v2 != firstseg->v1)
		{
			return false;
		}

	}
	return true;
}

//==========================================================================
//
// P_LoadNodes
//
//==========================================================================

bool MapLoader::LoadNodes (FileReader &lump)
{
	const int NF_SUBSECTOR = 0x8000;
	const int GL5_NF_SUBSECTOR = (1 << 31);

	int 		j;
	int 		k;
	node_t* 	no;
	TArray<uint16_t> used;

	if (!format5)
	{
		mapnode_t*	mn, * basemn;
		unsigned numnodes = unsigned(lump.GetLength() / sizeof(mapnode_t));

		if (numnodes == 0) return false;

		Level->nodes.Alloc(numnodes);
		lump.Seek(0, FileReader::SeekSet);

		auto buf = lump.Read();
		basemn = mn = (mapnode_t*)buf.data();

		used.Resize(numnodes);
		memset (used.Data(), 0, sizeof(uint16_t)*numnodes);

		no = &Level->nodes[0];

		for (unsigned i = 0; i < numnodes; i++, no++, mn++)
		{
			no->x = LittleShort(mn->x)<<FRACBITS;
			no->y = LittleShort(mn->y)<<FRACBITS;
			no->dx = LittleShort(mn->dx)<<FRACBITS;
			no->dy = LittleShort(mn->dy)<<FRACBITS;
			for (j = 0; j < 2; j++)
			{
				uint16_t child = LittleShort(mn->children[j]);
				if (child & NF_SUBSECTOR)
				{
					child &= ~NF_SUBSECTOR;
					if (child >= Level->subsectors.Size())
					{
						return false;
					}
					no->children[j] = (uint8_t *)&Level->subsectors[child] + 1;
				}
				else if (child >= numnodes)
				{
					return false;
				}
				else if (used[child])
				{
					return false;
				}
				else
				{
					no->children[j] = &Level->nodes[child];
					used[child] = j + 1;
				}
				for (k = 0; k < 4; k++)
				{
					no->bbox[j][k] = (float)LittleShort(mn->bbox[j][k]);
				}
			}
		}
	}
	else
	{
		gl5_mapnode_t*	mn, * basemn;
		auto numnodes = unsigned(lump.GetLength() / sizeof(gl5_mapnode_t));

		if (numnodes == 0) return false;

		Level->nodes.Alloc(numnodes);
		lump.Seek(0, FileReader::SeekSet);

		auto buf = lump.Read();
		basemn = mn = (gl5_mapnode_t*)buf.data();

		used.Resize(numnodes);
		memset(used.Data(), 0, sizeof(uint16_t)*numnodes);

		no = &Level->nodes[0];

		for (unsigned i = 0; i < numnodes; i++, no++, mn++)
		{
			no->x = LittleShort(mn->x)<<FRACBITS;
			no->y = LittleShort(mn->y)<<FRACBITS;
			no->dx = LittleShort(mn->dx)<<FRACBITS;
			no->dy = LittleShort(mn->dy)<<FRACBITS;
			for (j = 0; j < 2; j++)
			{
				int32_t child = LittleLong(mn->children[j]);
				if (child & GL5_NF_SUBSECTOR)
				{
					child &= ~GL5_NF_SUBSECTOR;
					if ((unsigned)child >= Level->subsectors.Size())
					{
						return false;
					}
					no->children[j] = (uint8_t *)&Level->subsectors[child] + 1;
				}
				else if ((unsigned)child >= numnodes)
				{
					return false;
				}
				else if (used[child])
				{
					return false;
				}
				else
				{
					no->children[j] = &Level->nodes[child];
					used[child] = j + 1;
				}
				for (k = 0; k < 4; k++)
				{
					no->bbox[j][k] = (float)LittleShort(mn->bbox[j][k]);
				}
			}
		}
	}
	return true;
}

//==========================================================================
//
// loads the GL node data
//
//==========================================================================

bool MapLoader::DoLoadGLNodes(FileReader * lumps)
{
	int missing = 0;

	if (!LoadGLVertexes(lumps[0]) ||
		!LoadGLSegs(lumps[1]) ||
		!LoadGLSubsectors(lumps[2]) ||
		!LoadNodes(lumps[3]))
	{
		goto fail;
	}

	// Quick check for the validity of the nodes
	// For invalid nodes there is a high chance that this test will fail

	for (auto &sub : Level->subsectors)
	{
		seg_t * seg = sub.firstline;
		if (!seg->sidedef) 
		{
			Printf("GL nodes contain invalid data. The BSP has to be rebuilt.\n");
			goto fail;
		}
	}

	// check whether the BSP covers all sidedefs completely.
	missing = CheckForMissingSegs();
	if (missing > 0)
	{
		Printf("%d missing segs counted in GL nodes.\nThe BSP has to be rebuilt.\n", missing);
	}
	return missing == 0;

fail:
	Level->nodes.Clear();
	Level->subsectors.Clear();
	Level->segs.Clear();
	return false;
}


//===========================================================================
//
// MatchHeader
//
// Checks whether a GL_LEVEL header belongs to this level
//
//===========================================================================

static bool MatchHeader(const char * label, const char * hdata)
{
	if (memcmp(hdata, "LEVEL=", 6) == 0)
	{
		size_t labellen = strlen(label);
		labellen = min(size_t(8), labellen);

		if (strnicmp(hdata+6, label, labellen)==0 && 
			(hdata[6+labellen]==0xa || hdata[6+labellen]==0xd))
		{
			return true;
		}
	}
	return false;
}

//===========================================================================
//
// FindGLNodesInWAD
//
// Looks for GL nodes in the same WAD as the level itself
//
//===========================================================================

static int FindGLNodesInWAD(int labellump)
{
	int wadfile = fileSystem.GetFileContainer(labellump);
	FString glheader;

	glheader.Format("GL_%s", fileSystem.GetFileName(labellump));
	if (glheader.Len()<=8)
	{
		int gllabel = fileSystem.CheckNumForName(glheader.GetChars(), ns_global, wadfile);
		if (gllabel >= 0) return gllabel;
	}
	else
	{
		// Before scanning the entire WAD directory let's check first whether
		// it is necessary.
		int gllabel = fileSystem.CheckNumForName("GL_LEVEL", ns_global, wadfile);

		if (gllabel >= 0)
		{
			int lastlump=0;
			int lump;
			while ((lump=fileSystem.FindLump("GL_LEVEL", &lastlump))>=0)
			{
				if (fileSystem.GetFileContainer(lump)==wadfile)
				{
					auto mem = fileSystem.ReadFile(lump);
					if (MatchHeader(fileSystem.GetFileName(labellump), GetStringFromLump(lump).GetChars())) return lump;
				}
			}
		}
	}
	return -1;
}

//===========================================================================
//
// FindGLNodesInFile
//
// Looks for GL nodes in the same WAD as the level itself
// Function returns the lump number within the file. Returns -1 if the input
// resource file is nullptr.
//
//===========================================================================

static int FindGLNodesInFile(FResourceFile * f, const char * label)
{
	// No file open?  Probably shouldn't happen but assume no GL nodes
	if(!f)
		return -1;

	FString glheader;
	bool mustcheck=false;
	uint32_t numentries = f->EntryCount();

	glheader.Format("GL_%.8s", label);
	if (glheader.Len()>8)
	{
		glheader="GL_LEVEL";
		mustcheck=true;
	}

	if (numentries > 4)
	{
		for(uint32_t i=0;i<numentries-4;i++)
		{
			if (!strnicmp(f->getName(i), glheader.GetChars(), 8))
			{
				if (mustcheck)
				{
					char check[16]={0};
					auto fr = f->GetEntryReader(i, FileSys::READER_SHARED);
					fr.Read(check, 16);
					if (MatchHeader(label, check)) return i;
				}
				else return i;
			}
		}
	}
	return -1;
}

//==========================================================================
//
// Checks for the presence of GL nodes in the loaded WADs or a .GWA file
// returns true if successful
//
//==========================================================================

bool MapLoader::LoadGLNodes(MapData * map)
{
	if (map->Size(ML_GLZNODES) != 0)
	{
		const int idcheck1a = MAKE_ID('Z','G','L','N');
		const int idcheck2a = MAKE_ID('Z','G','L','2');
		const int idcheck3a = MAKE_ID('Z','G','L','3');
		const int idcheck1b = MAKE_ID('X','G','L','N');
		const int idcheck2b = MAKE_ID('X','G','L','2');
		const int idcheck3b = MAKE_ID('X','G','L','3');
		int id = 0;

		auto &file = map->Reader(ML_GLZNODES);
		file.Read (&id, 4);
		if (id == idcheck1a || id == idcheck2a || id == idcheck3a ||
			id == idcheck1b || id == idcheck2b || id == idcheck3b)
		{
			try
			{
				Level->subsectors.Clear();
				Level->segs.Clear();
				Level->nodes.Clear();
				LoadExtendedNodes (file, id);
				return true;
			}
			catch (CRecoverableError &)
			{
				Level->subsectors.Clear();
				Level->segs.Clear();
				Level->nodes.Clear();
			}
		}
	}

	if (!CheckCachedNodes(map))
	{
		FileReader gwalumps[4];
		char path[256];
		int li;
		int lumpfile = fileSystem.GetFileContainer(map->lumpnum);
		bool mapinwad = map->InWad;
		FResourceFile * f_gwa = map->resource;

		const char * name = fileSystem.GetContainerFullName(lumpfile);

		if (mapinwad)
		{
			li = FindGLNodesInWAD(map->lumpnum);

			if (li>=0)
			{
				// GL nodes are loaded with a WAD
				for(int i=0;i<4;i++)
				{
					gwalumps[i]=fileSystem.ReopenFileReader(li+i+1);
				}
				return DoLoadGLNodes(gwalumps);
			}
			else
			{
				strcpy(path, name);

				char * ext = strrchr(path, '.');
				if (ext)
				{
					strcpy(ext, ".gwa");
					// Todo: Compare file dates

					f_gwa = FResourceFile::OpenResourceFile(path);
					if (f_gwa==nullptr) return false;

					strncpy(map->MapLumps[0].Name, fileSystem.GetFileName(map->lumpnum), 8);
				}
			}
		}

		bool result = false;
		li = FindGLNodesInFile(f_gwa, map->MapLumps[0].Name);
		if (li!=-1)
		{
			static const char check[][9]={"GL_VERT","GL_SEGS","GL_SSECT","GL_NODES"};
			result=true;
			for(unsigned i=0; i<4;i++)
			{
				if (strnicmp(f_gwa->getName(li + i + 1), check[i], 8))
				{
					result=false;
					break;
				}
				else
					// This is a special case. The container for the map WAD is not set up to create new file handles for itself so this needs to cache the content here.
					gwalumps[i] = f_gwa->GetEntryReader(li + i + 1, FileSys::READER_CACHED, FileSys::READERFLAG_SEEKABLE);
			}
			if (result) result = DoLoadGLNodes(gwalumps);
		}

		if (f_gwa != map->resource)
			delete f_gwa;
		return result;
	}
	else return true;
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
		ret = true;	// we are not using the level's original nodes if we get here.
		for (auto &sub : Level->subsectors)
		{
			sub.sector = sub.firstline->sidedef->sector;
		}

		// The nodes and subsectors need to be preserved for gameplay related purposes.
		Level->gamenodes = std::move(Level->nodes);
		Level->gamesubsectors = std::move(Level->subsectors);
		Level->segs.Clear();

		// Try to load GL nodes (cached or GWA)
		loaded = LoadGLNodes(map);
		if (!loaded)
		{
			// none found - we have to build new ones!
			uint64_t startTime, endTime;

			startTime = I_msTime ();
			TArray<FNodeBuilder::FPolyStart> polyspots, anchors;
			GetPolySpots (map, polyspots, anchors);
			FNodeBuilder::FLevel leveldata =
			{
				&Level->vertexes[0], (int)Level->vertexes.Size(),
				&Level->sides[0], (int)Level->sides.Size(),
				&Level->lines[0], (int)Level->lines.Size(),
				0, 0, 0, 0
			};
			leveldata.FindMapBounds ();
			FNodeBuilder builder (leveldata, polyspots, anchors, true);
			
			builder.Extract (*Level);
			endTime = I_msTime ();
			DPrintf (DMSG_NOTIFY, "BSP generation took %.3f sec (%u segs)\n", (endTime - startTime) * 0.001, Level->segs.Size());
			buildtime = (int32_t)(endTime - startTime);
		}
	}

	if (!loaded)
	{
#ifdef DEBUG
		// Building nodes in debug is much slower so let's cache them only if cachetime is 0
		buildtime = 0;
#endif
		if (Level->maptype != MAPTYPE_BUILD && gl_cachenodes && buildtime/1000.f >= gl_cachetime)
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
	FString lumpname = fileSystem.GetFileFullPath(map->lumpnum).c_str();
	auto separator = lumpname.IndexOf(':');
	path << '/' << lumpname.Left(separator);
	if (create) CreatePath(path.GetChars());

	lumpname.ReplaceChars('/', '%');
	lumpname.ReplaceChars(':', '$');
	path << '/' << lumpname.Right((ptrdiff_t)lumpname.Len() - separator - 1) << ".gzc";
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

void MapLoader::CreateCachedNodes(MapData *map)
{
	MemFile ZNodes;

	WriteLong(ZNodes, 0);
	WriteLong(ZNodes, Level->vertexes.Size());
	for(auto &vert : Level->vertexes)
	{
		WriteLong(ZNodes, vert.fixX());
		WriteLong(ZNodes, vert.fixY());
	}

	WriteLong(ZNodes, Level->subsectors.Size());
	for (auto &sub : Level->subsectors)
	{
		WriteLong(ZNodes, sub.numlines);
	}

	WriteLong(ZNodes, Level->segs.Size());
	for(auto &seg : Level->segs)
	{
		WriteLong(ZNodes, Index(seg.v1));
		WriteLong(ZNodes, seg.PartnerSeg == nullptr? 0xffffffffu : uint32_t(Index(seg.PartnerSeg)));
		if (seg.linedef)
		{
			WriteLong(ZNodes, uint32_t(Index(seg.linedef)));
			WriteByte(ZNodes, seg.sidedef == seg.linedef->sidedef[0]? 0:1);
		}
		else
		{
			WriteLong(ZNodes, 0xffffffffu);
			WriteByte(ZNodes, 0);
		}
	}

	WriteLong(ZNodes, Level->nodes.Size());
	for(auto &node : Level->nodes)
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
				child = 0x80000000 | uint32_t(Index(((subsector_t *)((uint8_t *)node.children[j] - 1))));
			}
			else
			{
				child = Index(((node_t *)node.children[j]));
			}
			WriteLong(ZNodes, child);
		}
	}

	uLongf outlen = ZNodes.Size();
	TArray<Bytef> compressed;
	int offset = Level->lines.Size() * 8 + 12 + 16;
	int r;
	do
	{
		compressed.Resize(outlen + offset);
		r = compress (compressed.Data() + offset, &outlen, &ZNodes[0], ZNodes.Size());
		if (r == Z_BUF_ERROR)
		{
			outlen += 1024;
		}
	} 
	while (r == Z_BUF_ERROR);

	memcpy(compressed.Data(), "CACH", 4);
	uint32_t len = LittleLong(Level->lines.Size());
	memcpy(&compressed[4], &len, 4);
	map->GetChecksum(&compressed[8]);
	for (unsigned i = 0; i < Level->lines.Size(); i++)
	{
		uint32_t ndx[2] = { LittleLong(uint32_t(Index(Level->lines[i].v1))), LittleLong(uint32_t(Index(Level->lines[i].v2))) };
		memcpy(&compressed[8 + 16 + 8 * i], ndx, 8);
	}
	memcpy(&compressed[offset - 4], "ZGL3", 4);

	FString path = CreateCacheName(map, true);
	FileWriter *fw = FileWriter::Open(path.GetChars());

	if (fw != nullptr)
	{
		const size_t length = outlen + offset;
		if (fw->Write(compressed.Data(), length) != length)
		{
			Printf("Error saving nodes to file %s\n", path.GetChars());
		}
		delete fw;
	}
	else
	{
		Printf("Cannot open nodes file %s for writing\n", path.GetChars());
	}
}


bool MapLoader::CheckCachedNodes(MapData *map)
{
	char magic[4] = {0,0,0,0};
	uint8_t md5[16];
	uint8_t md5map[16];
	uint32_t numlin;
	TArray<uint32_t> verts;

	FString path = CreateCacheName(map, false);
	FileReader fr;

	if (!fr.OpenFile(path.GetChars())) return false;

	if (fr.Read(magic, 4) != 4) return false;
	if (memcmp(magic, "CACH", 4))  return false;

	if (fr.Read(&numlin, 4) != 4) return false; 
	numlin = LittleLong(numlin);
	if (numlin != Level->lines.Size()) return false;

	if (fr.Read(md5, 16) != 16) return false;
	map->GetChecksum(md5map);
	if (memcmp(md5, md5map, 16)) return false;

	verts.Resize(numlin * 2);
	if (fr.Read(verts.Data(), 8 * numlin) != 8 * numlin) return false;

	if (fr.Read(magic, 4) != 4) return false;
	if (memcmp(magic, "ZGL2", 4) && memcmp(magic, "ZGL3", 4))  return false;


	try
	{
		LoadExtendedNodes (fr, MAKE_ID(magic[0],magic[1],magic[2],magic[3]));
	}
	catch (CRecoverableError &error)
	{
		Printf ("Error loading nodes: %s\n", error.GetMessage());

		Level->subsectors.Clear();
		Level->segs.Clear();
		Level->nodes.Clear();
		return false;
	}

	for(auto &line : Level->lines)
	{
		int i = Index(&line);
		line.v1 = &Level->vertexes[LittleLong(verts[i*2])];
		line.v2 = &Level->vertexes[LittleLong(verts[i*2+1])];
	}
	return true;
}

UNSAFE_CCMD(clearnodecache)
{
	FileSys::FileList list;
	FString path = M_GetCachePath(false);
	path += "/";

	if (!FileSys::ScanDirectory(list, path.GetChars(), "*", false))
	{
		Printf("Unable to scan node cache directory %s\n", path.GetChars());
		return;
	}

	// Scan list backwards so that when we reach a directory
	// all files within are already deleted.
	for(int i = (int)list.size()-1; i >= 0; i--)
	{
		if (list[i].isDirectory)
		{
			RemoveDir(list[i].FilePath.c_str());
		}
		else
		{
			RemoveFile(list[i].FilePath.c_str());
		}
	}

		
}

//==========================================================================
//
// Keep both the original nodes from the WAD and the GL nodes created here.
// The original set is only being used to get the sector for in-game 
// positioning of actors but not for rendering.
//
// This is necessary because ZDBSP is much more sensitive
// to sloppy mapping practices that produce overlapping sectors.
// The crane in P:AR E1M3 is a good example that would be broken if
// this wasn't done.
//
//==========================================================================


//==========================================================================
//
// PointOnLine
//
// Same as the one im the node builder, but not part of a specific class
//
//==========================================================================

static bool PointOnLine (int x, int y, int x1, int y1, int dx, int dy)
{
	const double SIDE_EPSILON = 6.5536;

	// For most cases, a simple dot product is enough.
	double d_dx = double(dx);
	double d_dy = double(dy);
	double d_x = double(x);
	double d_y = double(y);
	double d_x1 = double(x1);
	double d_y1 = double(y1);

	double s_num = (d_y1-d_y)*d_dx - (d_x1-d_x)*d_dy;

	if (fabs(s_num) < 17179869184.0)	// 4<<32
	{
		// Either the point is very near the line, or the segment defining
		// the line is very short: Do a more expensive test to determine
		// just how far from the line the point is.
		double l = g_sqrt(d_dx*d_dx+d_dy*d_dy);
		double dist = fabs(s_num)/l;
		if (dist < SIDE_EPSILON)
		{
			return true;
		}
	}
	return false;
}


//==========================================================================
//
// SetRenderSector
//
// Sets the render sector for each GL subsector so that the proper flat 
// information can be retrieved
//
//==========================================================================

void MapLoader::SetRenderSector()
{
	int 				i;
	uint32_t 				j;
	TArray<subsector_t *> undetermined;
	subsector_t *		ss;

	// Check for incorrect partner seg info so that the following code does not crash.

	for (auto &seg : Level->segs)
	{
		auto p = seg.PartnerSeg;
		if (p != nullptr)
		{
			int partner = Index(p);

			if (partner < 0 || partner >= (int)Level->segs.Size() || &Level->segs[partner] != p)
			{
				seg.PartnerSeg = nullptr;
			}

			// glbsp creates such incorrect references for Strife.
			if (seg.linedef && seg.PartnerSeg != nullptr && !seg.PartnerSeg->linedef)
			{
				seg.PartnerSeg = seg.PartnerSeg->PartnerSeg = nullptr;
			}
		}
	}
	for (auto &seg : Level->segs)
	{
		if (seg.PartnerSeg != nullptr && seg.PartnerSeg->PartnerSeg != &seg)
		{
			seg.PartnerSeg = nullptr;
		}
	}

	// look up sector number for each subsector
	for (auto &ss : Level->subsectors)
	{
		// For rendering pick the sector from the first seg that is a sector boundary
		// this takes care of self-referencing sectors
		seg_t *seg = ss.firstline;

		// Check for one-dimensional subsectors. These should be ignored when
		// being processed for automap drawing etc.
		ss.flags |= SSECF_DEGENERATE;
		for(j=2; j<ss.numlines; j++)
		{
			if (!PointOnLine(seg[j].v1->fixX(), seg[j].v1->fixY(), seg->v1->fixX(), seg->v1->fixY(), seg->v2->fixX() -seg->v1->fixX(), seg->v2->fixY() -seg->v1->fixY()))
			{
				// Not on the same line
				ss.flags &= ~SSECF_DEGENERATE;
				break;
			}
		}

		seg = ss.firstline;
		for(j=0; j<ss.numlines; j++)
		{
			if(seg->sidedef && (seg->PartnerSeg == nullptr || (seg->PartnerSeg->sidedef != nullptr && seg->sidedef->sector!=seg->PartnerSeg->sidedef->sector)))
			{
				ss.render_sector = seg->sidedef->sector;
				break;
			}
			seg++;
		}
		if(ss.render_sector == nullptr) 
		{
			undetermined.Push(&ss);
		}
	}

	// assign a vaild render sector to all subsectors which haven't been processed yet.
	while (undetermined.Size())
	{
		bool deleted=false;
		for(i=undetermined.Size()-1;i>=0;i--)
		{
			ss=undetermined[i];
			seg_t * seg = ss->firstline;
			
			for(j=0; j<ss->numlines; j++)
			{
				if (seg->PartnerSeg != nullptr && seg->PartnerSeg->Subsector)
				{
					sector_t * backsec = seg->PartnerSeg->Subsector->render_sector;
					if (backsec)
					{
						ss->render_sector = backsec;
						undetermined.Delete(i);
						deleted = 1;
						break;
					}
				}
				seg++;
			}
		}
		// We still got some left but the loop above was unable to assign them.
		// This only happens when a subsector is off the map.
		// Don't bother and just assign the real sector for rendering
		if (!deleted && undetermined.Size()) 
		{
			for(i=undetermined.Size()-1;i>=0;i--)
			{
				ss=undetermined[i];
				ss->render_sector=ss->sector;
			}
			break;
		}
	}
}
