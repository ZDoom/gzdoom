/**************************************************************************************************
"POLYMOST" code written by Ken Silverman
Ken Silverman's official web site: http://www.advsys.net/ken
This file has been modified (severely) from Ken Silverman's original release

Motivation:
When 3D Realms released the Duke Nukem 3D source code, I thought somebody would do a OpenGL or
Direct3D port. Well, after a few months passed, I saw no sign of somebody working on a true
hardware-accelerated port of Build, just people saying it wasn't possible. Eventually, I realized
the only way this was going to happen was for me to do it myself. First, I needed to port Build to
Windows. I could have done it myself, but instead I thought I'd ask my Australian buddy, Jonathon
Fowler, if he would upgrade his Windows port to my favorite compiler (MSVC) - which he did. Once
that was done, I was ready to start the "POLYMOST" project.

About:
This source file is basically a complete rewrite of the entire rendering part of the Build engine.
There are small pieces in ENGINE.C to activate this code, and other minor hacks in other source
files, but most of it is in here. If you're looking for polymost-related code in the other source
files, you should find most of them by searching for either "polymost" or "rendmode". Speaking of
rendmode, there are now 4 rendering modes in Build:

	rendmode 0: The original code I wrote from 1993-1997
	rendmode 1: Solid-color rendering: my debug code before I did texture mapping
	rendmode 2: Software rendering before I started the OpenGL code (Note: this is just a quick
						hack to make testing easier - it's not optimized to my usual standards!)
	rendmode 3: The OpenGL code

The original Build engine did hidden surface removal by using a vertical span buffer on the tops
and bottoms of walls. This worked nice back in the day, but it it's not suitable for a polygon
engine. So I decided to write a brand new hidden surface removal algorithm - using the same idea
as the original Build - but one that worked with vectors instead of already rasterized data.

Brief history:
06/20/2000: I release Build Source code
04/01/2003: 3D Realms releases Duke Nukem 3D source code
10/04/2003: Jonathon Fowler gets his Windows port working in Visual C
10/04/2003: I start writing POLYMOST.BAS, a new hidden surface removal algorithm for Build that
					works on a polygon level instead of spans.
10/16/2003: Ported POLYMOST.BAS to C inside JonoF KenBuild's ENGINE.C; later this code was split
					out of ENGINE.C and put in this file, POLYMOST.C.
12/10/2003: Started OpenGL code for POLYMOST (rendmode 3)
12/23/2003: 1st public release
01/01/2004: 2nd public release: fixed stray lines, status bar, mirrors, sky, and lots of other bugs.

----------------------------------------------------------------------------------------------------

Todo list (in approximate chronological order):

High priority:
	*   BOTH: Do accurate software sorting/chopping for sprites: drawing in wrong order is bad :/
	*   BOTH: Fix hall of mirrors near "zenith". Call polymost_drawrooms twice?
	* OPENGL: drawmapview()

Low priority:
	* SOFT6D: Do back-face culling of sprites during up/down/tilt transformation (top of drawpoly)
	* SOFT6D: Fix depth shading: use saturation&LUT
	* SOFT6D: Optimize using hyperbolic mapping (similar to KUBE algo)
	* SOFT6D: Slab6-style voxel sprites. How to accelerate? :/
	* OPENGL: KENBUILD: Write flipping code for floor mirrors
	*   BOTH: KENBUILD: Parallaxing sky modes 1&2
	*   BOTH: Masked/1-way walls don't clip correctly to sectors of intersecting ceiling/floor slopes
	*   BOTH: Editart x-center is not working correctly with Duke's camera/turret sprites
	*   BOTH: Get rid of horizontal line above Duke full-screen status bar
	*   BOTH: Combine ceilings/floors into a single triangle strip (should lower poly count by 2x)
	*   BOTH: Optimize/clean up texture-map setup equations

**************************************************************************************************/

#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "doomtype.h"
#include "r_polymost.h"

#pragma warning (disable:4244)

PolyClipper::PolyClipper ()
	: vsps (&EmptyList)
{
	UsedList.Next = UsedList.Prev = &UsedList;
}

PolyClipper::~PolyClipper ()
{
	vspgroup *probe = vsps.NextGroup;
	while (probe != NULL)
	{
		vspgroup *next = probe->NextGroup;
		delete probe;
		probe = next;
	}
}

PolyClipper::vspgroup::vspgroup (vsptype *sentinel)
{
	int i;

	NextGroup = NULL;
	vsp[0].Prev = sentinel;
	vsp[0].Next = &vsp[1];
	for (i = 1; i < GROUP_SIZE-1; ++i)
	{
		vsp[i].Next = &vsp[i+1];
		vsp[i].Prev = &vsp[i-1];
	}
	vsp[i].Next = sentinel;
	vsp[i].Prev = &vsp[i-1];
}

	/*Init viewport boundary (must be 4 point convex loop):
	//      (px[0],py[0]).----.(px[1],py[1])
	//                  /      \
	//                /          \
	// (px[3],py[3]).--------------.(px[2],py[2])
	*/
void PolyClipper::InitMosts (double *px, double *py, int n)
{
	int i, j, k, imin;
	int vcnt;
	vsptype *vsp[5];

	EmptyAll ();
	vcnt = 0;

	if (n < 3) return;
	imin = (px[1] < px[0]);
	for(i=n-1;i>=2;i--) if (px[i] < px[imin]) imin = i;


	vsp[0] = GetVsp ();
	vsp[0]->X = px[imin];
	vsp[0]->Cy[0] = vsp[0]->Fy[0] = py[imin];
	vcnt++;
	i = imin+1; if (i >= n) i = 0;
	j = imin-1; if (j < 0) j = n-1;
	do
	{
		if (px[i] < px[j])
		{
			if ((vcnt > 0) && (px[i] <= vsp[vcnt-1]->X)) vcnt--;
			else vsp[vcnt] = GetVsp ();
			vsp[vcnt]->X = px[i];
			vsp[vcnt]->Cy[0] = py[i];
			k = j+1; if (k >= n) k = 0;
				//(px[k],py[k])
				//(px[i],?)
				//(px[j],py[j])
			vsp[vcnt]->Fy[0] = (px[i]-px[k])*(py[j]-py[k])/(px[j]-px[k]) + py[k];
			vcnt++;
			i++; if (i >= n) i = 0;
		}
		else if (px[j] < px[i])
		{
			if ((vcnt > 0) && (px[j] <= vsp[vcnt-1]->X)) vcnt--;
			else vsp[vcnt] = GetVsp ();
			vsp[vcnt]->X = px[j];
			vsp[vcnt]->Fy[0] = py[j];
			k = i-1; if (k < 0) k = n-1;
				//(px[k],py[k])
				//(px[j],?)
				//(px[i],py[i])
			vsp[vcnt]->Cy[0] = (px[j]-px[k])*(py[i]-py[k])/(px[i]-px[k]) + py[k];
			vcnt++;
			j--; if (j < 0) j = n-1;
		}
		else
		{
			if ((vcnt > 0) && (px[i] <= vsp[vcnt-1]->X)) vcnt--;
			else vsp[vcnt] = GetVsp ();
			vsp[vcnt]->X = px[i];
			vsp[vcnt]->Cy[0] = py[i];
			vsp[vcnt]->Cy[0] = py[j];
			vcnt++;
			i++; if (i >= n) i = 0; if (i == j) break;
			j--; if (j < 0) j = n-1;
		}
	} while (i != j);
	if (px[i] > vsp[vcnt-1]->X)
	{
		vsp[vcnt] = GetVsp ();
		vsp[vcnt]->X = px[i];
		vsp[vcnt]->Cy[0] = vsp[vcnt]->Fy[0] = py[i];
		vcnt++;
	}

	assert (vcnt < 5);

	for(i=0;i<vcnt;i++)
	{
		vsp[i]->Cy[1] = vsp[i+1]->Cy[0]; vsp[i]->CTag = i;
		vsp[i]->Fy[1] = vsp[i+1]->Fy[0]; vsp[i]->FTag = i;
		vsp[i]->Next = vsp[i+1]; vsp[i]->Prev = vsp[i-1];
	}
	vsp[vcnt-1]->Next = &UsedList; UsedList.Prev = vsp[vcnt-1];
	UsedList.Next = vsp[0]; vsp[0]->Prev = &UsedList;
	GTag = vcnt;
}

void PolyClipper::EmptyAll ()
{
	if (UsedList.Next != &UsedList)
	{
		if (EmptyList.Next != &EmptyList)
		{
			// Move the used list to the start of the empty list
			UsedList.Prev->Next = EmptyList.Next;
			EmptyList.Next = UsedList.Prev;
		}
		else
		{
			// The empty list is empty, so we can just move the
			// used list to the empty list.
			EmptyList.Next = UsedList.Next;
			EmptyList.Prev = UsedList.Prev;
		}
		UsedList.Next = UsedList.Prev = &UsedList;
	}
}

void PolyClipper::AddGroup ()
{
	vspgroup *group = new vspgroup (&EmptyList);
	group->NextGroup = vsps.NextGroup;
	vsps.NextGroup = group;
}

PolyClipper::vsptype *PolyClipper::GetVsp ()
{
	vsptype *vsp;

	if (EmptyList.Next == &EmptyList)
	{
		AddGroup ();
	}
	vsp = EmptyList.Next;
	EmptyList.Next = vsp->Next;
	vsp->Next->Prev = &EmptyList;
	return vsp;
}

void PolyClipper::FreeVsp (vsptype *vsp)
{
	vsp->Next->Prev = vsp->Prev;
	vsp->Prev->Next = vsp->Next;

	vsp->Next = EmptyList.Next;
	vsp->Next->Prev = vsp;
	vsp->Prev = &EmptyList;
	EmptyList.Next = vsp;
}

PolyClipper::vsptype *PolyClipper::vsinsaft (vsptype *i)
{
	vsptype *r;

	// Get an element from the empty list
	r = GetVsp ();

	*r = *i;	// Copy i to r

	// Insert r after i
	r->Prev = i;
	r->Next = i->Next;
	i->Next->Prev = r;
	i->Next = r;

	return r;
}

bool PolyClipper::TestVisibleMost (float x0, float x1)
{
	vsptype *i, *newi;

	for (i = UsedList.Next; i != &UsedList; i = newi)
	{
		newi = i->Next;
		if ((x0 < newi->X) && (i->X < x1) && (i->CTag >= 0)) return true;
	}
	return false;
}

int PolyClipper::DoMost (float x0, float y0, float x1, float y1, pmostcallbacktype callback)
{
	double dpx[4], dpy[4];
	float d, f, n, t, slop, dx, dx0, dx1, nx, nx0, ny0, nx1, ny1;
	float spx[4], spy[4], cy[2], cv[2];
	int j, k, z, scnt, dir, spt[4];
	vsptype *vsp, *nvsp, *vcnt, *ni;
	int did = 1;
	
	if (x0 < x1)
	{
		dir = 1; //clip dmost (floor)
		y0 -= .01f; y1 -= .01f;
	}
	else
	{
		if (x0 == x1) return 0;
		f = x0; x0 = x1; x1 = f;
		f = y0; y0 = y1; y1 = f;
		dir = 0; //clip umost (ceiling)
		//y0 += .01f; y1 += .01f; //necessary?
	}

	slop = (y1-y0)/(x1-x0);
	for (vsp = UsedList.Next; vsp != &UsedList; vsp = nvsp)
	{
		nvsp = vsp->Next; nx0 = vsp->X; nx1 = nvsp->X;
		if ((x0 >= nx1) || (nx0 >= x1) || (vsp->CTag <= 0)) continue;
		dx = nx1-nx0;
		cy[0] = vsp->Cy[0]; cv[0] = vsp->Cy[1]-cy[0];
		cy[1] = vsp->Fy[0]; cv[1] = vsp->Fy[1]-cy[1];

		scnt = 0;

			//Test if left edge requires split (x0,y0) (nx0,cy(0)),<dx,cv(0)>
		if ((x0 > nx0) && (x0 < nx1))
		{
			t = (x0-nx0)*cv[dir] - (y0-cy[dir])*dx;
			if (((!dir) && (t < 0)) || ((dir) && (t > 0)))
				{ spx[scnt] = x0; spy[scnt] = y0; spt[scnt] = -1; scnt++; }
		}

			//Test for intersection on umost (j == 0) and dmost (j == 1)
		for(j=0;j<2;j++)
		{
			d = (y0-y1)*dx - (x0-x1)*cv[j];
			n = (y0-cy[j])*dx - (x0-nx0)*cv[j];
			if ((fabsf(n) <= fabsf(d)) && (d*n >= 0) && (d != 0))
			{
				t = n/d; nx = (x1-x0)*t + x0;
				if ((nx > nx0) && (nx < nx1))
				{
					spx[scnt] = nx; spy[scnt] = (y1-y0)*t + y0;
					spt[scnt] = j; scnt++;
				}
			}
		}

			//Nice hack to avoid full sort later :)
		if ((scnt >= 2) && (spx[scnt-1] < spx[scnt-2]))
		{
			f = spx[scnt-1]; spx[scnt-1] = spx[scnt-2]; spx[scnt-2] = f;
			f = spy[scnt-1]; spy[scnt-1] = spy[scnt-2]; spy[scnt-2] = f;
			j = spt[scnt-1]; spt[scnt-1] = spt[scnt-2]; spt[scnt-2] = j;
		}

			//Test if right edge requires split
		if ((x1 > nx0) && (x1 < nx1))
		{
			t = (x1-nx0)*cv[dir] - (y1-cy[dir])*dx;
			if (((!dir) && (t < 0)) || ((dir) && (t > 0)))
				{ spx[scnt] = x1; spy[scnt] = y1; spt[scnt] = -1; scnt++; }
		}

		vsp->Tag = nvsp->Tag = -1;
		for(z = 0; z <= scnt; z++, vsp = vcnt)
		{
			if (z < scnt)
			{
				vcnt = vsinsaft(vsp);
				t = (spx[z]-nx0)/dx;
				vsp->Cy[1] = t*cv[0] + cy[0];
				vsp->Fy[1] = t*cv[1] + cy[1];
				vcnt->X = spx[z];
				vcnt->Cy[0] = vsp->Cy[1];
				vcnt->Fy[0] = vsp->Fy[1];
				vcnt->Tag = spt[z];
			}

			ni = vsp->Next; if (ni == &UsedList) continue; //this 'if' fixes many bugs!
			dx0 = vsp->X; if (x0 > dx0) continue;
			dx1 = ni->X; if (x1 < dx1) continue;
			ny0 = (dx0-x0)*slop + y0;
			ny1 = (dx1-x0)*slop + y0;

				//      dx0           dx1
				//       ³             ³
				//----------------------------
				//     t0+=0         t1+=0
				//   vsp[i].cy[0]  vsp[i].cy[1]
				//============================
				//     t0+=1         t1+=3
				//============================
				//   vsp[i].fy[0]    vsp[i].fy[1]
				//     t0+=2         t1+=6
				//
				//     ny0 ?         ny1 ?

			k = 1+3;
			if ((vsp->Tag == 0) || (ny0 <= vsp->Cy[0]+.01)) k--;
			if ((vsp->Tag == 1) || (ny0 >= vsp->Fy[0]-.01)) k++;
			if ((ni->Tag == 0)  || (ny1 <= vsp->Cy[1]+.01)) k -= 3;
			if ((ni->Tag == 1)  || (ny1 >= vsp->Fy[1]-.01)) k += 3;

			if (!dir)
			{
				switch(k)
				{
					case 1: case 2:
						dpx[0] = dx0; dpy[0] = vsp->Cy[0];
						dpx[1] = dx1; dpy[1] = vsp->Cy[1];
						dpx[2] = dx0; dpy[2] = ny0;
						if(callback) callback(dpx,dpy,3);
						vsp->Cy[0] = ny0; vsp->CTag = GTag; break;
					case 3: case 6:
						dpx[0] = dx0; dpy[0] = vsp->Cy[0];
						dpx[1] = dx1; dpy[1] = vsp->Cy[1];
						dpx[2] = dx1; dpy[2] = ny1;
						if(callback) callback(dpx,dpy,3);
						vsp->Cy[1] = ny1; vsp->CTag = GTag; break;
					case 4: case 5: case 7:
						dpx[0] = dx0; dpy[0] = vsp->Cy[0];
						dpx[1] = dx1; dpy[1] = vsp->Cy[1];
						dpx[2] = dx1; dpy[2] = ny1;
						dpx[3] = dx0; dpy[3] = ny0;
						if(callback) callback(dpx,dpy,4);
						vsp->Cy[0] = ny0; vsp->Cy[1] = ny1; vsp->CTag = GTag; break;
					case 8:
						dpx[0] = dx0; dpy[0] = vsp->Cy[0];
						dpx[1] = dx1; dpy[1] = vsp->Cy[1];
						dpx[2] = dx1; dpy[2] = vsp->Fy[1];
						dpx[3] = dx0; dpy[3] = vsp->Fy[0];
						if(callback) callback(dpx,dpy,4);
						vsp->CTag = vsp->FTag = -1; break;
					default: did = 0; break;
				}
			}
			else
			{
				switch(k)
				{
					case 7: case 6:
						dpx[0] = dx0; dpy[0] = ny0;
						dpx[1] = dx1; dpy[1] = vsp->Fy[1];
						dpx[2] = dx0; dpy[2] = vsp->Fy[0];
						if(callback) callback(dpx,dpy,3);
						vsp->Fy[0] = ny0; vsp->FTag = GTag; break;
					case 5: case 2:
						dpx[0] = dx0; dpy[0] = vsp->Fy[0];
						dpx[1] = dx1; dpy[1] = ny1;
						dpx[2] = dx1; dpy[2] = vsp->Fy[1];
						if(callback) callback(dpx,dpy,3);
						vsp->Fy[1] = ny1; vsp->FTag = GTag; break;
					case 4: case 3: case 1:
						dpx[0] = dx0; dpy[0] = ny0;
						dpx[1] = dx1; dpy[1] = ny1;
						dpx[2] = dx1; dpy[2] = vsp->Fy[1];
						dpx[3] = dx0; dpy[3] = vsp->Fy[0];
						if(callback) callback(dpx,dpy,4);
						vsp->Fy[0] = ny0; vsp->Fy[1] = ny1; vsp->FTag = GTag; break;
					case 0:
						dpx[0] = dx0; dpy[0] = vsp->Cy[0];
						dpx[1] = dx1; dpy[1] = vsp->Cy[1];
						dpx[2] = dx1; dpy[2] = vsp->Fy[1];
						dpx[3] = dx0; dpy[3] = vsp->Fy[0];
						if(callback) callback(dpx,dpy,4);
						vsp->CTag = vsp->FTag = -1; break;
					default: did = 0; break;
				}
			}
		}
	}

	GTag++;

		//Combine neighboring vertical strips with matching collinear top&bottom edges
		//This prevents x-splits from propagating through the entire scan
	vsp = UsedList.Next;
	while (vsp != &UsedList)
	{
		ni = vsp->Next;
		if ((vsp->Cy[0] >= vsp->Fy[0]) && (vsp->Cy[1] >= vsp->Fy[1])) { vsp->CTag = vsp->FTag = -1; }
		if ((vsp->CTag == ni->CTag) && (vsp->FTag == ni->FTag))
			{ vsp->Cy[1] = ni->Cy[1]; vsp->Fy[1] = ni->Fy[1]; FreeVsp (ni); }
		else vsp = ni;
	}
	return did;
}
