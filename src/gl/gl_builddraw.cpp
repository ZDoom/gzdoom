/*
** gl_builddraw.cpp
** a build-like rendering algorithm
** Uses the sections created in gl_sections.cpp
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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

#include "i_system.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_clipper.h"
#include "gl/utility/gl_clock.h"
#include "gl/data/gl_sections.h"
#include "gl/scene/gl_wall.h"

#ifdef BUILD_TEST
#define D(x) x
#else
#define D(x) do{}while(0)
#endif

EXTERN_CVAR (Bool, dumpsections)

struct FBunch
{
	int startline;
	int endline;
	angle_t startangle;
	angle_t endangle;
	fixed_t minviewdist;
	fixed_t maxviewdist;
};

void DoSubsector(subsector_t * sub, bool handlelines);

EXTERN_CVAR(Bool, gl_render_walls)

//==========================================================================
//
// From Build but changed to use doubles to prevent overflows
//
//==========================================================================

static int WallInFront(FGLSectionLine *wal1, FGLSectionLine *wal2)
{
	double x11, y11, x21, y21, x12, y12, x22, y22, dx, dy, t1, t2;

	x11 = wal1->start->x;
	y11 = wal1->start->y;
	x21 = wal1->end->x;
	y21 = wal1->end->y;
	x12 = wal2->start->x;
	y12 = wal2->start->y;
	x22 = wal2->end->x;
	y22 = wal2->end->y;

	dx = x21-x11; dy = y21-y11;

	t1 = (x12-x11)*dy - (y12-y11)*dx;
	t2 = (x22-x11)*dy - (y22-y11)*dx;
	if (t1 == 0) 
	{ 
		t1 = t2; 
		if (t1 == 0) return(-1); 
	}
	if (t2 == 0) t2 = t1;

	if ((t1*t2) >= 0)
	{
		t2 = (double(viewx)-x11) * dy - (double(viewy)-y11)*dx;
		return((t2*t1) < 0);
	}

	dx = x22-x12; dy = y22-y12;
	t1 = (x11-x12)*dy - (y11-y12)*dx;
	t2 = (x21-x12)*dy - (y21-y12)*dx;
	if (t1 == 0) 
	{ 
		t1 = t2; 
		if (t1 == 0) return(-1); 
	}
	if (t2 == 0) t2 = t1;
	if ((t1*t2) >= 0)
	{
		t2 = (double(viewx)-x12) * dy - (double(viewy)-y12)*dx;
		return((t2*t1) >= 0);
	}
	return(-2);
}

//==========================================================================
//
// This is a bit more complicated than it looks because angles can wrap
// around so we can only compare angle differences.
//
// Rules:
// 1. Any bunch can span at most 180°.
// 2. 2 bunches can never overlap at both ends
// 3. if there is an overlap one of the 2 starting points must be in the
//    overlapping area.
//
//==========================================================================

static int BunchInFront(FBunch *b1, FBunch *b2)
{
	angle_t anglecheck, endang;

	if (b2->startangle - b1->startangle < b1->endangle - b1->startangle)
	{
		// we have an overlap at b2->startangle
		anglecheck = b2->startangle - b1->startangle;

		// Find the wall in b1 that overlaps b2->startangle
		for(int i = b1->startline; i <= b1->endline; i++)
		{
			#ifdef _DEBUG
				angle_t startang = SectionLines[i].start->GetClipAngleInverse() - b1->startangle;
			#endif
			endang = SectionLines[i].end->GetClipAngleInverse() - b1->startangle;
			if (endang > anglecheck)
			{
				assert (startang <= anglecheck);

				// found a line
				int ret = WallInFront(&SectionLines[b2->startline], &SectionLines[i]);

				D(Printf (PRINT_LOG, "Line %d <-> line %d: Result = %d.\n",
					SectionLines[b2->startline].linedef-lines,
					SectionLines[i].linedef-lines, ret));

				return ret;
			}
		}
	}
	else if (b1->startangle - b2->startangle < b2->endangle - b2->startangle)
	{
		// we have an overlap at b1->startangle
		anglecheck = b1->startangle - b2->startangle;

		// Find the wall in b2 that overlaps b1->startangle
		for(int i = b2->startline; i <= b2->endline; i++)
		{
			#ifdef _DEBUG
				angle_t startang = SectionLines[i].start->GetClipAngleInverse() - b2->startangle;
			#endif
			endang = SectionLines[i].end->GetClipAngleInverse() - b2->startangle;
			if (endang > anglecheck)
			{
				assert (startang <= anglecheck);
				
				// found a line
				int ret = WallInFront(&SectionLines[i], &SectionLines[b1->startline]);

				D(Printf (PRINT_LOG, "Line %d <-> line %d: Result = %d,\n",
					SectionLines[i].linedef-lines,
					SectionLines[b1->endline].linedef-lines, ret));

				return ret;
			}
		}
	}
	// we have no overlap
	return -1;
}


// ----------------------------------------------------------------------------
//
// Bunches are groups of continuous lines
// This array stores the amount of points per bunch,
// the view angles for each point and the line index for the starting line
//
// ----------------------------------------------------------------------------

class BunchDrawer
{
	int LastBunch;
	int StartTime;
	TArray<FBunch> Bunches;
	TArray<int> CompareData;
	sector_t fakebacksec;

	//==========================================================================
	//
	//
	//
	//==========================================================================
public:
	BunchDrawer()
	{
		StartScene();
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================
private:
	void StartScene()
	{
		LastBunch = 0;
		StartTime = I_MSTime();
		Bunches.Clear();
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void StartBunch(int linenum, angle_t startan, angle_t endan, vertex_t *startpt, vertex_t *endpt)
	{
		FBunch *bunch = &Bunches[LastBunch = Bunches.Reserve(1)];

		bunch->startline = bunch->endline = linenum;
		bunch->startangle = startan;
		bunch->endangle = endan;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void AddLineToBunch(int newan)
	{
		Bunches[LastBunch].endline++;
		Bunches[LastBunch].endangle = newan;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void DeleteBunch(int index)
	{
		Bunches.Delete(index);
	}

	//==========================================================================
	//
	// ClipLine
	// Clips the given segment
	//
	//==========================================================================

	enum
	{
		CL_Skip = 0,
		CL_Draw = 1,
		CL_Pass = 2,
	};


	int ClipLine (FGLSectionLine *line, sector_t * sector, sector_t **pbacksector)
	{
		angle_t startAngle, endAngle;
		sector_t * backsector = NULL;
		bool blocking;

		startAngle = line->end->GetClipAngle();
		endAngle = line->start->GetClipAngle();
		*pbacksector = NULL;

		// Back side, i.e. backface culling	- read: endAngle >= startAngle!
		if (startAngle-endAngle<ANGLE_180)  
		{
			return CL_Skip;
		}

		if (!clipper.SafeCheckRange(startAngle, endAngle)) 
		{
			return CL_Skip;
		}

		if (line->otherside == -1)
		{
			// one-sided
			clipper.SafeAddClipRange(startAngle, endAngle);
			return CL_Draw;
		}
		else if (line->polysub == NULL)
		{
			// two sided and not a polyobject
			if (line->linedef == NULL)
			{
				// Miniseg
				return CL_Pass;
			}
			if (sector->sectornum == line->refseg->backsector->sectornum)
			{
				FTexture *tex = TexMan(line->sidedef->GetTexture(side_t::mid));
				if (!tex || tex->UseType==FTexture::TEX_Null) 
				{
					// no mid texture: nothing to do here
					return CL_Pass;
				}
				*pbacksector = sector;
				return CL_Draw|CL_Pass;
			}
			else
			{
				// clipping checks are only needed when the backsector is not the same as the front sector
				gl_CheckViewArea(line->start, line->end, line->refseg->frontsector, line->refseg->backsector);

				*pbacksector = backsector = gl_FakeFlat(line->refseg->backsector, &fakebacksec, true);

				blocking = gl_CheckClip(line->sidedef, sector, backsector);
				if (blocking)
				{
					clipper.SafeAddClipRange(startAngle, endAngle);
					return CL_Draw;
				}
				return CL_Draw|CL_Pass;
			}
		}
		else
		{
			*pbacksector = sector;
			return CL_Draw;
		}
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void ProcessBunch(int bnch)
	{
		FBunch *bunch = &Bunches[bnch];

		sector_t fake;
		sector_t *sec;
		sector_t *backsector;

		D(Printf(PRINT_LOG, "------------------------------\nProcessing bunch %d (Startline %d)\n",bnch,SectionLines[bunch->startline].linedef-lines));
		ClipWall.Clock();
		for(int i=bunch->startline; i <= bunch->endline; i++)
		{
			FGLSectionLine *ln = &SectionLines[i];


			// Draw this line. todo: optimize
			sec = gl_FakeFlat(ln->refseg->frontsector, &fake, false);

			int clipped = ClipLine(ln, sec, &backsector);

			D(Printf(PRINT_LOG, "line %d clip result is %d\n", ln->linedef - lines, clipped));

			if (clipped & CL_Draw)
			{
				ln->linedef->flags |= ML_MAPPED;

				if (ln->linedef->validcount!=validcount) 
				{
					ln->linedef->validcount=validcount;

					#ifndef BUILD_TEST
						if (gl_render_walls)
						{
							SetupWall.Clock();

							GLWall wall;
							wall.Process(ln->refseg, sec, backsector, ln->polysub);
							rendered_lines++;

							SetupWall.Unclock();
						}
					#endif
				}
			}

			if (clipped & CL_Pass)
			{
				ClipWall.Unclock();
				ProcessSection(ln->otherside);
				ClipWall.Clock();
			}
		}
		D(Printf(PRINT_LOG, "Bunch %d done\n------------------------------\n",bnch));
		ClipWall.Unclock();
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	int FindClosestBunch()
	{
		int closest = 0;              //Almost works, but not quite :(

		CompareData.Clear();
		for(unsigned i = 1; i < Bunches.Size(); i++)
		{
			switch (BunchInFront(&Bunches[i], &Bunches[closest]))
			{
			case 0:		// i is in front
				closest = i;
				continue;

			case 1:	// i is behind
				continue;

			default:		// can't determine
				CompareData.Push(i);	// mark for later comparison
				continue;
			}
		}

		// we need to do a second pass to see how the marked bunches relate to the currently closest one.
		for(unsigned i = 0; i < CompareData.Size(); i++)
		{
			switch (BunchInFront(&Bunches[CompareData[i]], &Bunches[closest]))
			{
			case 0:		// is in front
				closest = i;
				CompareData.Delete(i);
				i = 0;	// we need to recheck everything that's still marked.
				continue;

			case 1:	// is behind
				CompareData.Delete(i);
				i--;
				continue;

			default:
				continue;

			}
		}
		return closest;
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

	void ProcessSection(int sectnum)
	{
		FGLSection *sect = &Sections[sectnum];
		bool inbunch;
		angle_t startangle;

		if (sect->validcount == StartTime) return;
		sect->validcount = StartTime;
		D(Printf(PRINT_LOG, "------------------------------\nProcessing section %d (sector %d)\n",sectnum, sect->sector->sectornum));

		#ifndef BUILD_TEST
			for(unsigned i = 0; i < sect->subsectors.Size(); i++)
			{
				DoSubsector(sect->subsectors[i], false);
				if (sect->subsectors[i]->poly != NULL)
				{
					// ProcessPolyobject()
				}
			}
		#endif
		
		//Todo: process subsectors
		for(int i=0; i<sect->numloops; i++)
		{
			FGLSectionLoop *loop = sect->GetLoop(i);
			inbunch = false;

			for(int j=0; j<loop->numlines; j++)
			{
				FGLSectionLine *ln = loop->GetLine(j);

				angle_t ang1 = ln->start->GetClipAngle();
				angle_t ang2 = ln->end->GetClipAngle();

				if (ang2 - ang1 < ANGLE_180) 
				{
					// Backside
					D(Printf(PRINT_LOG, "line %d facing backwards\n", ln->linedef - lines));
					inbunch = false;
				}
				else if (!clipper.SafeCheckRange(ang2, ang1)) 
				{
					// is it visible?
					D(Printf(PRINT_LOG, "line %d not in view\n", ln->linedef - lines));
					inbunch = false;
				}
				else if (!inbunch || startangle - ang2 >= ANGLE_180)
				{
					// don't let a bunch span more than 180° to avoid problems.
					// This limitation ensures that the combined range of 2
					// bunches will always be less than 360° which simplifies
					// the distance comparison code because it prevents a 
					// situation where 2 bunches may overlap at both ends.
					D(Printf(PRINT_LOG, "Starting bunch %d at line %d\n",Bunches.Size(), ln->linedef - lines));

					startangle = ang2;
					// Clipping angles are backward which makes this code very hard to read so let's use the inverse
					StartBunch(loop->startline + j, 0 - ang1, 0 - ang2);
					inbunch = true;
				}
				else
				{
					D(Printf(PRINT_LOG, "    Adding line %d\n", ln->linedef - lines));
					AddLineToBunch(0 - ang2); 
				}
			}
		}
		D(Printf(PRINT_LOG, "Section %d done\n------------------------------\n",sectnum));
	}

	//==========================================================================
	//
	//
	//
	//==========================================================================

public:
	void RenderScene(int viewsection)
	{
		ProcessSection(viewsection);
		while (Bunches.Size() > 0)
		{
			int closest = FindClosestBunch();
			ProcessBunch(closest);
			DeleteBunch(closest);
		}
	}
};


void gl_RenderBuild()
{
	subsector_t *sub = R_PointInSubsector(viewx, viewy);

	clipper.Clear();
	angle_t a1 = GLRenderer->FrustumAngle();
	clipper.SafeAddClipRangeRealAngles(viewangle+a1, viewangle-a1);
	if (Sections.Size() == 0) gl_CreateSections();

	int startsection = SectionForSubsector[sub-subsectors];

	BunchDrawer bd;
	bd.RenderScene(startsection);
}

#ifdef BUILD_TEST
CCMD(testrender)
{
	gl_RenderBuild();
}
#endif
