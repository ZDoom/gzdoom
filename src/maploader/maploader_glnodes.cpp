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
#include <zlib.h>

#include "maploader.h"
#include "maploader_internal.h"
#include "files.h"
#include "version.h"
#include "p_setup.h"
#include "w_wad.h"
#include "resourcefiles/resourcefile.h"
#include "doomerrors.h"
#include "g_levellocals.h"


#define gNd2        MAKE_ID('g','N','d','2')
#define gNd4        MAKE_ID('g','N','d','4')
#define gNd5        MAKE_ID('g','N','d','5')
#define GL_VERT_OFFSET  4

//==========================================================================
//
// LoadGLVertexes
//
// loads GL vertices
//
//==========================================================================

bool MapLoader::LoadGLVertexes(FileReader &lump)
{
	uint8_t *gldata;
	int                 i;
	
	firstglvertex = vertexes.Size();
	
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
	
	auto oldvertexes = &vertexes[0];
	vertexes.Resize(numvertexes);
	
	for(auto &line : lines)
	{
		// Remap vertex pointers in linedefs
		line.v1 = &vertexes[line.v1 - oldvertexes];
		line.v2 = &vertexes[line.v2 - oldvertexes];
	}
	
	for (i = firstglvertex; i < (int)numvertexes; i++)
	{
		vertexes[i].set(LittleLong(mgl->x)/65536., LittleLong(mgl->y)/65536.);
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
	char		*data;
	int			i;
	line_t		*ldef=nullptr;
	
	int numsegs = (int)lump.GetLength();
	data= new char[numsegs];
	lump.Seek(0, FileReader::SeekSet);
	lump.Read(data, numsegs);
	
	if (!format5 && memcmp(data, "gNd3", 4))
	{
		numsegs/=sizeof(glseg_t);
		segs.Alloc(numsegs);
		memset(&segs[0],0,sizeof(seg_t)*numsegs);
		
		glseg_t * ml = (glseg_t*)data;
		for(i = 0; i < numsegs; i++)
		{
			// check for gl-vertices
			segs[i].v1 = &vertexes[checkGLVertex(LittleShort(ml->v1))];
			segs[i].v2 = &vertexes[checkGLVertex(LittleShort(ml->v2))];
			segs[i].PartnerSeg = ml->partner == 0xFFFF ? nullptr : &segs[LittleShort(ml->partner)];
			if(ml->linedef != 0xffff)
			{
				ldef = &lines[LittleShort(ml->linedef)];
				segs[i].linedef = ldef;
				
				
				ml->side=LittleShort(ml->side);
				segs[i].sidedef = ldef->sidedef[ml->side];
				if (ldef->sidedef[ml->side] != nullptr)
				{
					segs[i].frontsector = ldef->sidedef[ml->side]->sector;
				}
				else
				{
					segs[i].frontsector = nullptr;
				}
				if (ldef->flags & ML_TWOSIDED && ldef->sidedef[ml->side^1] != nullptr)
				{
					segs[i].backsector = ldef->sidedef[ml->side^1]->sector;
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
		
		glseg3_t * ml = (glseg3_t*)(data+ (format5? 0:4));
		for(i = 0; i < numsegs; i++)
		{							// check for gl-vertices
			segs[i].v1 = &vertexes[checkGLVertex3(LittleLong(ml->v1))];
			segs[i].v2 = &vertexes[checkGLVertex3(LittleLong(ml->v2))];
			
			const uint32_t partner = LittleLong(ml->partner);
			segs[i].PartnerSeg = DWORD_MAX == partner ? nullptr : &segs[partner];
			
			if(ml->linedef != 0xffff) // skip minisegs
			{
				ldef = &lines[LittleLong(ml->linedef)];
				segs[i].linedef = ldef;
				
				
				ml->side=LittleShort(ml->side);
				segs[i].sidedef = ldef->sidedef[ml->side];
				if (ldef->sidedef[ml->side] != nullptr)
				{
					segs[i].frontsector = ldef->sidedef[ml->side]->sector;
				}
				else
				{
					segs[i].frontsector = nullptr;
				}
				if (ldef->flags & ML_TWOSIDED && ldef->sidedef[ml->side^1] != nullptr)
				{
					segs[i].backsector = ldef->sidedef[ml->side^1]->sector;
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
	delete [] data;
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
	TArray<int8_t> datab(numsubsectors);
	
	lump.Seek(0, FileReader::SeekSet);
	lump.Read(datab.Data(), numsubsectors);
	
	if (numsubsectors == 0)
	{
		return false;
	}
	
	if (!format5 && memcmp(datab.Data(), "gNd3", 4))
	{
		mapsubsector_t * data = (mapsubsector_t*) datab.Data();
		numsubsectors /= sizeof(mapsubsector_t);
		subsectors.Alloc(numsubsectors);
		memset(&subsectors[0],0,numsubsectors * sizeof(subsector_t));
		
		for (i=0; i<numsubsectors; i++)
		{
			subsectors[i].numlines  = LittleShort(data[i].numsegs );
			subsectors[i].firstline = &segs[LittleShort(data[i].firstseg)];
			
			if (subsectors[i].numlines == 0)
			{
				return false;
			}
		}
	}
	else
	{
		gl3_mapsubsector_t * data = (gl3_mapsubsector_t*) (datab.Data()+(format5? 0:4));
		numsubsectors /= sizeof(gl3_mapsubsector_t);
		subsectors.Alloc(numsubsectors);
		memset(&subsectors[0],0,numsubsectors * sizeof(subsector_t));
		
		for (i=0; i<numsubsectors; i++)
		{
			subsectors[i].numlines  = LittleLong(data[i].numsegs );
			subsectors[i].firstline = &segs[LittleLong(data[i].firstseg)];
			
			if (subsectors[i].numlines == 0)
			{
				return false;
			}
		}
	}
	
	for (auto &sub : subsectors)
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

bool MapLoader::LoadGLNodeLump (FileReader &lump)
{
	const int NF_SUBSECTOR = 0x8000;
	const int GL5_NF_SUBSECTOR = (1 << 31);
	
	int 		j;
	int 		k;
	node_t* 	no;
	
	if (!format5)
	{
		unsigned numnodes = unsigned(lump.GetLength() / sizeof(mapnode_t));
		
		if (numnodes == 0) return false;
		TArray<mapnode_t> basemn(numnodes, true);
		auto mn = basemn.Data();
		
		lump.Seek(0, FileReader::SeekSet);
		lump.Read(basemn.Data(), lump.GetLength());
		
		TArray<uint16_t> used(numnodes, true);
		memset (used.Data(), 0, sizeof(uint16_t)*numnodes);
		
		nodes.Alloc(numnodes);
		no = &nodes[0];
		
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
					if (child >= subsectors.Size())
					{
						return false;
					}
					no->children[j] = (uint8_t *)&subsectors[child] + 1;
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
					no->children[j] = &nodes[child];
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
		auto numnodes = unsigned(lump.GetLength() / sizeof(gl5_mapnode_t));
		
		if (numnodes == 0) return false;
		
		if (numnodes == 0) return false;
		TArray<gl5_mapnode_t> basemn(numnodes, true);
		auto mn = basemn.Data();
		
		lump.Seek(0, FileReader::SeekSet);
		lump.Read(basemn.Data(), lump.GetLength());
		
		TArray<uint16_t> used(numnodes, true);
		memset (used.Data(), 0, sizeof(uint16_t)*numnodes);
		
		nodes.Alloc(numnodes);
		no = &nodes[0];
		
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
					if ((unsigned)child >= subsectors.Size())
					{
						return false;
					}
					no->children[j] = (uint8_t *)&subsectors[child] + 1;
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
					no->children[j] = &nodes[child];
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
		!LoadGLNodeLump(lumps[3]))
	{
		goto fail;
	}
	
	// Quick check for the validity of the nodes
	// For invalid nodes there is a high chance that this test will fail
	
	for (auto &sub : subsectors)
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
	nodes.Clear();
	subsectors.Clear();
	segs.Clear();
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
		int id;
		
		auto &file = map->Reader(ML_GLZNODES);
		file.Read (&id, 4);
		if (id == idcheck1a || id == idcheck2a || id == idcheck3a ||
			id == idcheck1b || id == idcheck2b || id == idcheck3b)
		{
			try
			{
				subsectors.Clear();
				segs.Clear();
				nodes.Clear();
				LoadZNodes (file, id);
				return true;
			}
			catch (CRecoverableError &)
			{
				subsectors.Clear();
				segs.Clear();
				nodes.Clear();
			}
		}
	}
	
	if (!CheckCachedNodes(map))
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
					if (f_gwa==nullptr) return false;
					
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

