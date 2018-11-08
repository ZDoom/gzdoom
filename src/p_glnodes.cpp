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
#ifdef _MSC_VER
#include <malloc.h>		// for alloca()
#endif

#ifndef _WIN32
#include <unistd.h>

#else
#include <direct.h>

#define rmdir _rmdir

#endif

#include <zlib.h>
#include "templates.h"
#include "m_argv.h"
#include "c_dispatch.h"
#include "m_swap.h"
#include "w_wad.h"
#include "p_local.h"
#include "nodebuild.h"
#include "doomstat.h"
#include "doomerrors.h"
#include "p_setup.h"
#include "version.h"
#include "md5.h"
#include "m_misc.h"
#include "cmdlib.h"
#include "g_levellocals.h"
#include "i_time.h"

void P_LoadZNodes (FileReader &dalump, uint32_t id);


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

static int CheckForMissingSegs()
{
	auto numsides = level.sides.Size();
	double *added_seglen = new double[numsides];
	int missing = 0;

	memset(added_seglen, 0, sizeof(double)*numsides);
	for (auto &seg : level.segs)
	{
		if (seg.sidedef != nullptr)
		{
			// check all the segs and calculate the length they occupy on their sidedef
			DVector2 vec1(seg.v2->fX() - seg.v1->fX(), seg.v2->fY() - seg.v1->fY());
			added_seglen[seg.sidedef->Index()] += vec1.Length();
		}
	}

	for (unsigned i = 0; i < numsides; i++)
	{
		double linelen = level.sides[i].linedef->Delta().Length();
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

bool P_CheckForGLNodes()
{
	for(auto &sub : level.subsectors)
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
				if (level.segs[j].linedef==NULL)	// miniseg
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
static int firstglvertex;
static bool format5;

static bool LoadGLVertexes(FileReader &lump)
{
	uint8_t *gldata;
	int                 i;

	firstglvertex = level.vertexes.Size();
	
	auto gllen=lump.GetLength();

	gldata = new uint8_t[gllen];
	lump.Seek(0, FileReader::SeekSet);
	lump.Read(gldata, gllen);

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

		delete [] gldata;
		return false;
	}
	else format5=false;

	mapglvertex_t*	mgl = (mapglvertex_t *)(gldata + GL_VERT_OFFSET);
	unsigned numvertexes = (unsigned)(firstglvertex +  (gllen - GL_VERT_OFFSET)/sizeof(mapglvertex_t));

	auto oldvertexes = &level.vertexes[0];
	level.vertexes.Resize(numvertexes);

	for(auto &line : level.lines)
	{
		// Remap vertex pointers in linedefs
		line.v1 = &level.vertexes[line.v1 - oldvertexes];
		line.v2 = &level.vertexes[line.v2 - oldvertexes];
	}

	for (i = firstglvertex; i < (int)numvertexes; i++)
	{
		level.vertexes[i].set(LittleLong(mgl->x)/65536., LittleLong(mgl->y)/65536.);
		mgl++;
	}
	delete[] gldata;
	return true;
}

//==========================================================================
//
// GL Nodes utilities
//
//==========================================================================

static inline int checkGLVertex(int num)
{
	if (num & 0x8000)
		num = (num&0x7FFF)+firstglvertex;
	return num;
}

static inline int checkGLVertex3(int num)
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

static bool LoadGLSegs(FileReader &lump)
{
	char		*data;
	int			i;
	line_t		*ldef=NULL;
	
	int numsegs = (int)lump.GetLength();
	data= new char[numsegs];
	lump.Seek(0, FileReader::SeekSet);
	lump.Read(data, numsegs);
	auto &segs = level.segs;

#ifdef _MSC_VER
	__try
#endif
	{
		if (!format5 && memcmp(data, "gNd3", 4))
		{
			numsegs/=sizeof(glseg_t);
			level.segs.Alloc(numsegs);
			memset(&segs[0],0,sizeof(seg_t)*numsegs);
			
			glseg_t * ml = (glseg_t*)data;
			for(i = 0; i < numsegs; i++)
			{		
				// check for gl-vertices
				segs[i].v1 = &level.vertexes[checkGLVertex(LittleShort(ml->v1))];
				segs[i].v2 = &level.vertexes[checkGLVertex(LittleShort(ml->v2))];
				segs[i].PartnerSeg = ml->partner == 0xFFFF ? nullptr : &segs[LittleShort(ml->partner)];
				if(ml->linedef != 0xffff)
				{
					ldef = &level.lines[LittleShort(ml->linedef)];
					segs[i].linedef = ldef;
	
					
					ml->side=LittleShort(ml->side);
					segs[i].sidedef = ldef->sidedef[ml->side];
					if (ldef->sidedef[ml->side] != NULL)
					{
						segs[i].frontsector = ldef->sidedef[ml->side]->sector;
					}
					else
					{
						segs[i].frontsector = NULL;
					}
					if (ldef->flags & ML_TWOSIDED && ldef->sidedef[ml->side^1] != NULL)
					{
						segs[i].backsector = ldef->sidedef[ml->side^1]->sector;
					}
					else
					{
						ldef->flags &= ~ML_TWOSIDED;
						segs[i].backsector = NULL;
					}
	
				}
				else
				{
					segs[i].linedef = NULL;
					segs[i].sidedef = NULL;
	
					segs[i].frontsector = NULL;
					segs[i].backsector  = NULL;
				}
				ml++;		
			}
		}
		else
		{
			if (!format5) numsegs-=4;
			numsegs/=sizeof(glseg3_t);
			level.segs.Alloc(numsegs);
			memset(&segs[0],0,sizeof(seg_t)*numsegs);
			
			glseg3_t * ml = (glseg3_t*)(data+ (format5? 0:4));
			for(i = 0; i < numsegs; i++)
			{							// check for gl-vertices
				segs[i].v1 = &level.vertexes[checkGLVertex3(LittleLong(ml->v1))];
				segs[i].v2 = &level.vertexes[checkGLVertex3(LittleLong(ml->v2))];

				const uint32_t partner = LittleLong(ml->partner);
				segs[i].PartnerSeg = DWORD_MAX == partner ? nullptr : &segs[partner];
	
				if(ml->linedef != 0xffff) // skip minisegs 
				{
					ldef = &level.lines[LittleLong(ml->linedef)];
					segs[i].linedef = ldef;
	
					
					ml->side=LittleShort(ml->side);
					segs[i].sidedef = ldef->sidedef[ml->side];
					if (ldef->sidedef[ml->side] != NULL)
					{
						segs[i].frontsector = ldef->sidedef[ml->side]->sector;
					}
					else
					{
						segs[i].frontsector = NULL;
					}
					if (ldef->flags & ML_TWOSIDED && ldef->sidedef[ml->side^1] != NULL)
					{
						segs[i].backsector = ldef->sidedef[ml->side^1]->sector;
					}
					else
					{
						ldef->flags &= ~ML_TWOSIDED;
						segs[i].backsector = NULL;
					}
	
				}
				else
				{
					segs[i].linedef = NULL;
					segs[i].sidedef = NULL;
					segs[i].frontsector = NULL;
					segs[i].backsector  = NULL;
				}
				ml++;		
			}
		}
		delete [] data;
		return true;
	}
#ifdef _MSC_VER
	__except(1)
	{
		// Invalid data has the bad habit of requiring extensive checks here
		// so let's just catch anything invalid and output a message.
		// (at least under MSVC. GCC can't do SEH even for Windows... :( )
		Printf("Invalid GL segs. The BSP will have to be rebuilt.\n");
		delete [] data;
		level.segs.Clear();
		return false;
	}
#endif
}


//==========================================================================
//
// LoadGLSubsectors
//
//==========================================================================

static bool LoadGLSubsectors(FileReader &lump)
{
	char * datab;
	int  i;
	
	int numsubsectors = (int)lump.GetLength();
	datab = new char[numsubsectors];
	lump.Seek(0, FileReader::SeekSet);
	lump.Read(datab, numsubsectors);
	
	if (numsubsectors == 0)
	{
		delete [] datab;
		return false;
	}
	
	if (!format5 && memcmp(datab, "gNd3", 4))
	{
		mapsubsector_t * data = (mapsubsector_t*) datab;
		numsubsectors /= sizeof(mapsubsector_t);
		level.subsectors.Alloc(numsubsectors);
		auto &subsectors = level.subsectors;
		memset(&subsectors[0],0,numsubsectors * sizeof(subsector_t));
	
		for (i=0; i<numsubsectors; i++)
		{
			subsectors[i].numlines  = LittleShort(data[i].numsegs );
			subsectors[i].firstline = &level.segs[LittleShort(data[i].firstseg)];

			if (subsectors[i].numlines == 0)
			{
				delete [] datab;
				return false;
			}
		}
	}
	else
	{
		gl3_mapsubsector_t * data = (gl3_mapsubsector_t*) (datab+(format5? 0:4));
		numsubsectors /= sizeof(gl3_mapsubsector_t);
		level.subsectors.Alloc(numsubsectors);
		auto &subsectors = level.subsectors;
		memset(&subsectors[0],0,numsubsectors * sizeof(subsector_t));
	
		for (i=0; i<numsubsectors; i++)
		{
			subsectors[i].numlines  = LittleLong(data[i].numsegs );
			subsectors[i].firstline = &level.segs[LittleLong(data[i].firstseg)];

			if (subsectors[i].numlines == 0)
			{
				delete [] datab;
				return false;
			}
		}
	}

	for (auto &sub : level.subsectors)
	{
		for(unsigned j=0;j<sub.numlines;j++)
		{
			seg_t * seg = sub.firstline + j;
			if (seg->linedef==NULL) seg->frontsector = seg->backsector = sub.firstline->frontsector;
		}
		seg_t *firstseg = sub.firstline;
		seg_t *lastseg = sub.firstline + sub.numlines - 1;
		// The subsector must be closed. If it isn't we can't use these nodes and have to do a rebuild.
		if (lastseg->v2 != firstseg->v1)
		{
			delete [] datab;
			return false;
		}

	}
	delete [] datab;
	return true;
}

//==========================================================================
//
// P_LoadNodes
//
//==========================================================================

static bool LoadNodes (FileReader &lump)
{
	const int NF_SUBSECTOR = 0x8000;
	const int GL5_NF_SUBSECTOR = (1 << 31);

	int 		j;
	int 		k;
	node_t* 	no;
	uint16_t*		used;

	if (!format5)
	{
		mapnode_t*	mn, * basemn;
		unsigned numnodes = unsigned(lump.GetLength() / sizeof(mapnode_t));

		if (numnodes == 0) return false;

		level.nodes.Alloc(numnodes);
		lump.Seek(0, FileReader::SeekSet);

		basemn = mn = new mapnode_t[numnodes];
		lump.Read(mn, lump.GetLength());

		used = (uint16_t *)alloca (sizeof(uint16_t)*numnodes);
		memset (used, 0, sizeof(uint16_t)*numnodes);

		no = &level.nodes[0];

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
					if (child >= level.subsectors.Size())
					{
						delete [] basemn;
						return false;
					}
					no->children[j] = (uint8_t *)&level.subsectors[child] + 1;
				}
				else if (child >= numnodes)
				{
					delete [] basemn;
					return false;
				}
				else if (used[child])
				{
					delete [] basemn;
					return false;
				}
				else
				{
					no->children[j] = &level.nodes[child];
					used[child] = j + 1;
				}
				for (k = 0; k < 4; k++)
				{
					no->bbox[j][k] = (float)LittleShort(mn->bbox[j][k]);
				}
			}
		}
		delete [] basemn;
	}
	else
	{
		gl5_mapnode_t*	mn, * basemn;
		auto numnodes = unsigned(lump.GetLength() / sizeof(gl5_mapnode_t));

		if (numnodes == 0) return false;

		level.nodes.Alloc(numnodes);
		lump.Seek(0, FileReader::SeekSet);

		basemn = mn = new gl5_mapnode_t[numnodes];
		lump.Read(mn, lump.GetLength());

		used = (uint16_t *)alloca (sizeof(uint16_t)*numnodes);
		memset (used, 0, sizeof(uint16_t)*numnodes);

		no = &level.nodes[0];

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
					if ((unsigned)child >= level.subsectors.Size())
					{
						delete [] basemn;
						return false;
					}
					no->children[j] = (uint8_t *)&level.subsectors[child] + 1;
				}
				else if ((unsigned)child >= numnodes)
				{
					delete [] basemn;
					return false;
				}
				else if (used[child])
				{
					delete [] basemn;
					return false;
				}
				else
				{
					no->children[j] = &level.nodes[child];
					used[child] = j + 1;
				}
				for (k = 0; k < 4; k++)
				{
					no->bbox[j][k] = (float)LittleShort(mn->bbox[j][k]);
				}
			}
		}
		delete [] basemn;
	}
	return true;
}

//==========================================================================
//
// loads the GL node data
//
//==========================================================================

static bool DoLoadGLNodes(FileReader * lumps)
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

	for (auto &sub : level.subsectors)
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
	level.nodes.Clear();
	level.subsectors.Clear();
	level.segs.Clear();
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
		labellen = MIN(size_t(8), labellen);

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
	int wadfile = Wads.GetLumpFile(labellump);
	FString glheader;

	glheader.Format("GL_%s", Wads.GetLumpFullName(labellump));
	if (glheader.Len()<=8)
	{
		int gllabel = Wads.CheckNumForName(glheader, ns_global, wadfile);
		if (gllabel >= 0) return gllabel;
	}
	else
	{
		// Before scanning the entire WAD directory let's check first whether
		// it is necessary.
		int gllabel = Wads.CheckNumForName("GL_LEVEL", ns_global, wadfile);

		if (gllabel >= 0)
		{
			int lastlump=0;
			int lump;
			while ((lump=Wads.FindLump("GL_LEVEL", &lastlump))>=0)
			{
				if (Wads.GetLumpFile(lump)==wadfile)
				{
					FMemLump mem = Wads.ReadLump(lump);
					if (MatchHeader(Wads.GetLumpFullName(labellump), (const char *)mem.GetMem())) return lump;
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
// resource file is NULL.
//
//===========================================================================

static int FindGLNodesInFile(FResourceFile * f, const char * label)
{
	// No file open?  Probably shouldn't happen but assume no GL nodes
	if(!f)
		return -1;

	FString glheader;
	bool mustcheck=false;
	uint32_t numentries = f->LumpCount();

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
			if (!strnicmp(f->GetLump(i)->Name, glheader, 8))
			{
				if (mustcheck)
				{
					char check[16]={0};
					auto fr = f->GetLump(i)->GetReader();
					fr->Read(check, 16);
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

bool P_LoadGLNodes(MapData * map)
{
	if (map->Size(ML_GLZNODES) != 0)
	{
		const int idcheck1a = MAKE_ID('Z','G','L','N');
		const int idcheck2a = MAKE_ID('Z','G','L','2');
		const int idcheck3a = MAKE_ID('Z','G','L','3');
		const int idcheck1b = MAKE_ID('X','G','L','N');
		const int idcheck2b = MAKE_ID('X','G','L','2');
		const int idcheck3b = MAKE_ID('X','G','L','3');
		int id;

		auto &file = map->Reader(ML_GLZNODES);
		file.Read (&id, 4);
		if (id == idcheck1a || id == idcheck2a || id == idcheck3a ||
			id == idcheck1b || id == idcheck2b || id == idcheck3b)
		{
			try
			{
				level.subsectors.Clear();
				level.segs.Clear();
				level.nodes.Clear();
				P_LoadZNodes (file, id);
				return true;
			}
			catch (CRecoverableError &)
			{
				level.subsectors.Clear();
				level.segs.Clear();
				level.nodes.Clear();
			}
		}
	}

	//if (!CheckCachedNodes(map))
	{
		FileReader gwalumps[4];
		char path[256];
		int li;
		int lumpfile = Wads.GetLumpFile(map->lumpnum);
		bool mapinwad = map->InWad;
		FResourceFile * f_gwa = map->resource;

		const char * name = Wads.GetWadFullName(lumpfile);

		if (mapinwad)
		{
			li = FindGLNodesInWAD(map->lumpnum);

			if (li>=0)
			{
				// GL nodes are loaded with a WAD
				for(int i=0;i<4;i++)
				{
					gwalumps[i]=Wads.ReopenLumpReader(li+i+1);
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

					f_gwa = FResourceFile::OpenResourceFile(path, true);
					if (f_gwa==NULL) return false;

					strncpy(map->MapLumps[0].Name, Wads.GetLumpFullName(map->lumpnum), 8);
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
				if (strnicmp(f_gwa->GetLump(li+i+1)->Name, check[i], 8))
				{
					result=false;
					break;
				}
				else
					gwalumps[i] = f_gwa->GetLump(li+i+1)->NewReader();
			}
			if (result) result = DoLoadGLNodes(gwalumps);
		}

		if (f_gwa != map->resource)
			delete f_gwa;
		return result;
	}
	return true;
}

