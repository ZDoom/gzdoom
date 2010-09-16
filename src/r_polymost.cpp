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
#include "c_cvars.h"
#include "c_dispatch.h"
#include "r_main.h"
#include "r_draw.h"
#include "templates.h"
#include "r_sky.h"
#include "g_level.h"
#include "r_bsp.h"
#include "v_palette.h"
#include "v_font.h"

EXTERN_CVAR (Int, r_polymost)

#define SCISDIST 1.0 //1.0: Close plane clipping distance

static double gyxscale, gxyaspect, gviewxrange, ghalfx, grhalfxdown10, grhalfxdown10x, ghoriz;
static double gcosang, gsinang, gcosang2, gsinang2;
static double gchang, gshang, gctang, gstang;
//static float gtang = 0.0;
CVAR (Float, gtang, 0, 0);
double guo, gux, guy; //Screen-based texture mapping parameters
double gvo, gvx, gvy;
double gdo, gdx, gdy;

#ifdef _MSC_VER
#pragma warning (disable:4244)
#endif

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
	sentinel->Next = &vsp[0];
	sentinel->Prev = &vsp[i];
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
	vsptype *vsp[8];

	EmptyAll ();
	vcnt = 1; // 0 is dummy solid node

	if (n < 3) return;
	imin = (px[1] < px[0]);
	for(i=n-1;i>=2;i--) if (px[i] < px[imin]) imin = i;

	vsp[0] = &UsedList;
	vsp[vcnt] = GetVsp ();
	vsp[vcnt]->X = px[imin];
	vsp[vcnt]->Cy[0] = vsp[vcnt]->Fy[0] = py[imin];
	vsp[vcnt]->CTag = vsp[vcnt]->FTag = 1;
	vcnt++;
	i = imin+1; if (i >= n) i = 0;
	j = imin-1; if (j < 0) j = n-1;
	do
	{
		if (px[i] < px[j])
		{
			if ((vcnt > 1) && (px[i] - vsp[vcnt-1]->X < 0.00001)) vcnt--;
			else vsp[vcnt] = GetVsp ();
			vsp[vcnt]->X = px[i];
			vsp[vcnt]->Cy[0] = py[i];
			k = j+1; if (k >= n) k = 0;
				//(px[k],py[k])
				//(px[i],?)
				//(px[j],py[j])
			vsp[vcnt]->Fy[0] = (px[i]-px[k])*(py[j]-py[k])/(px[j]-px[k]) + py[k];
			if (vcnt > 1)
			{
				vsp[vcnt]->CTag = vsp[vcnt-1]->CTag + 1;
				vsp[vcnt]->FTag = vsp[vcnt-1]->FTag;
			}
			vcnt++;
			i++; if (i >= n) i = 0;
		}
		else if (px[j] < px[i])
		{
			if ((vcnt > 1) && (px[j] - vsp[vcnt-1]->X < 0.00001)) vcnt--;
			else vsp[vcnt] = GetVsp ();
			vsp[vcnt]->X = px[j];
			vsp[vcnt]->Fy[0] = py[j];
			k = i-1; if (k < 0) k = n-1;
				//(px[k],py[k])
				//(px[j],?)
				//(px[i],py[i])
			vsp[vcnt]->Cy[0] = (px[j]-px[k])*(py[i]-py[k])/(px[i]-px[k]) + py[k];
			if (vcnt > 1)
			{
				vsp[vcnt]->FTag = vsp[vcnt-1]->FTag + 1;
				vsp[vcnt]->CTag = vsp[vcnt-1]->CTag;
			}
			vcnt++;
			j--; if (j < 0) j = n-1;
		}
		else
		{
			if ((vcnt > 1) && (px[i] - vsp[vcnt-1]->X < 0.00001)) vcnt--;
			else vsp[vcnt] = GetVsp ();
			vsp[vcnt]->X = px[i];
			vsp[vcnt]->Cy[0] = py[i];
			vsp[vcnt]->Fy[0] = py[j];
			if (vcnt > 1)
			{
				vsp[vcnt]->CTag = vsp[vcnt-1]->CTag + 1;
				vsp[vcnt]->FTag = vsp[vcnt-1]->FTag + 1;
			}
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
		vsp[vcnt]->CTag = vsp[vcnt-1]->CTag + 1;
		vsp[vcnt]->FTag = vsp[vcnt-1]->FTag + 1;
		vcnt++;
	}

	assert (vcnt < 8);

	vsp[vcnt-1]->CTag = vsp[vcnt-1]->FTag = vcnt-1;
	for(i=0;i<vcnt-1;i++)
	{
		vsp[i]->Cy[1] = vsp[i+1]->Cy[0]; //vsp[i]->CTag = i;
		vsp[i]->Fy[1] = vsp[i+1]->Fy[0]; //vsp[i]->FTag = i;
		vsp[i]->Next = vsp[i+1]; vsp[i]->Prev = vsp[i-1];
	}
	vsp[vcnt-1]->Next = &UsedList; UsedList.Prev = vsp[vcnt-1];
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
			EmptyList.Next = UsedList.Next;
			UsedList.Next->Prev = &EmptyList;
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

int PolyClipper::DoMost (float x0, float y0, float x1, float y1, pmostcallbacktype callback, void *callbackdata)
{
	double dpx[4], dpy[4];
	float f, slop, dx0, dx1, nx, nx0, ny0, nx1, ny1;
	double dx, d, n, t;
	float spx[4], spy[4], cy[2], cv[2];
	int j, k, z, scnt, dir, spt[4];
	vsptype *vsp, *nvsp, *vcnt = NULL, *ni;
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
						if(callback) callback(dpx,dpy,3,callbackdata);
						vsp->Cy[0] = ny0; vsp->CTag = GTag; break;
					case 3: case 6:
						dpx[0] = dx0; dpy[0] = vsp->Cy[0];
						dpx[1] = dx1; dpy[1] = vsp->Cy[1];
						dpx[2] = dx1; dpy[2] = ny1;
						if(callback) callback(dpx,dpy,3,callbackdata);
						vsp->Cy[1] = ny1; vsp->CTag = GTag; break;
					case 4: case 5: case 7:
						dpx[0] = dx0; dpy[0] = vsp->Cy[0];
						dpx[1] = dx1; dpy[1] = vsp->Cy[1];
						dpx[2] = dx1; dpy[2] = ny1;
						dpx[3] = dx0; dpy[3] = ny0;
						if(callback) callback(dpx,dpy,4,callbackdata);
						vsp->Cy[0] = ny0; vsp->Cy[1] = ny1; vsp->CTag = GTag; break;
					case 8:
						dpx[0] = dx0; dpy[0] = vsp->Cy[0];
						dpx[1] = dx1; dpy[1] = vsp->Cy[1];
						dpx[2] = dx1; dpy[2] = vsp->Fy[1];
						dpx[3] = dx0; dpy[3] = vsp->Fy[0];
						if(callback) callback(dpx,dpy,4,callbackdata);
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
						if(callback) callback(dpx,dpy,3,callbackdata);
						vsp->Fy[0] = ny0; vsp->FTag = GTag; break;
					case 5: case 2:
						dpx[0] = dx0; dpy[0] = vsp->Fy[0];
						dpx[1] = dx1; dpy[1] = ny1;
						dpx[2] = dx1; dpy[2] = vsp->Fy[1];
						if(callback) callback(dpx,dpy,3,callbackdata);
						vsp->Fy[1] = ny1; vsp->FTag = GTag; break;
					case 4: case 3: case 1:
						dpx[0] = dx0; dpy[0] = ny0;
						dpx[1] = dx1; dpy[1] = ny1;
						dpx[2] = dx1; dpy[2] = vsp->Fy[1];
						dpx[3] = dx0; dpy[3] = vsp->Fy[0];
						if(callback) callback(dpx,dpy,4,callbackdata);
						vsp->Fy[0] = ny0; vsp->Fy[1] = ny1; vsp->FTag = GTag; break;
					case 0:
						dpx[0] = dx0; dpy[0] = vsp->Cy[0];
						dpx[1] = dx1; dpy[1] = vsp->Cy[1];
						dpx[2] = dx1; dpy[2] = vsp->Fy[1];
						dpx[3] = dx0; dpy[3] = vsp->Fy[0];
						if(callback) callback(dpx,dpy,4,callbackdata);
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
	while (vsp->Next != &UsedList)
	{
		ni = vsp->Next;
		if ((vsp->Cy[0] >= vsp->Fy[0]) && (vsp->Cy[1] >= vsp->Fy[1]))
			{ vsp->CTag = vsp->FTag = -1; }
		if ((vsp->CTag == ni->CTag) && (vsp->FTag == ni->FTag))
			{ vsp->Cy[1] = ni->Cy[1]; vsp->Fy[1] = ni->Fy[1]; FreeVsp (ni); }
		else vsp = ni;
	}
	return did;
}

#include "d_event.h"
CVAR(Bool, testpolymost, false, 0)
static int pmx, pmy;
static int pt, px0, py0, px1, py1;
static struct polypt { float x, y; } polypts[80];
static BYTE polysize[32];
static int numpoly, polypt;
PolyClipper TestPoly;

void drawline2d (float x1, float y1, float x2, float y2, BYTE col)
{
	float dx, dy, fxresm1, fyresm1, f;
	long i, x, y, xi, yi, xup16, yup16;

		//Always draw lines in same direction
	if ((y2 > y1) || ((y2 == y1) && (x2 > x1))) { f = x1; x1 = x2; x2 = f; f = y1; y1 = y2; y2 = f; }

	dx = x2-x1; dy = y2-y1; if ((dx == 0) && (dy == 0)) return;
	fxresm1 = (float)RenderTarget->GetWidth()-.5; fyresm1 = (float)RenderTarget->GetHeight()-.5;
		 if (x1 >= fxresm1) { if (x2 >= fxresm1) return; y1 += (fxresm1-x1)*dy/dx; x1 = fxresm1; }
	else if (x1 <        0) { if (x2 <        0) return; y1 += (      0-x1)*dy/dx; x1 =       0; }
		 if (x2 >= fxresm1) {                            y2 += (fxresm1-x2)*dy/dx; x2 = fxresm1; }
	else if (x2 <        0) {                            y2 += (      0-x2)*dy/dx; x2 =       0; }
		 if (y1 >= fyresm1) { if (y2 >= fyresm1) return; x1 += (fyresm1-y1)*dx/dy; y1 = fyresm1; }
	else if (y1 <        0) { if (y2 <        0) return; x1 += (      0-y1)*dx/dy; y1 =       0; }
		 if (y2 >= fyresm1) {                            x2 += (fyresm1-y2)*dx/dy; y2 = fyresm1; }
	else if (y2 <        0) {                            x2 += (      0-y2)*dx/dy; y2 =       0; }

	dx = x2-x1; dy = y2-y1;
	i = (long)(MAX(fabsf(dx)+1,fabsf(dy)+1)); f = 65536.f/((float)i);
	x = (long)(x1*65536.f)+32768; xi = (long)(dx*f); xup16 = (RenderTarget->GetWidth()<<16);
	y = (long)(y1*65536.f)+32768; yi = (long)(dy*f); yup16 = (RenderTarget->GetHeight()<<16);
	do
	{
		if (((unsigned long)x < (unsigned long)xup16) && ((unsigned long)y < (unsigned long)yup16))
			*(ylookup[y>>16]+(x>>16)+dc_destorg) = col;
		x += xi; y += yi; i--;
	} while (i >= 0);
}

static int maskhack;

void fillconvpoly (float x[], float y[], int n, int col, int bcol)
{
	int mini = y[0] >= y[1], maxi = 1 - mini;
	int i, j, y2, oz, z, yy, zz, ncol;
	float area, xi, xx;
	static int lastx[MAXHEIGHT+2];

	for (z = 2; z < n; ++z)
	{
		if (y[z] < y[mini]) mini = z;
		if (y[z] > y[maxi]) maxi = z;
	}

	area = 0; zz = n - 1;
	for (z = 0; z < n; ++z)
	{
		area += (x[zz] - x[z]) * (y[z] + y[zz]); zz = z;
	}
	if (area <= 0) return;

	i = maxi; y2 = int(y[i]);
	do
	{
		j = i + 1; if (j == n) j = 0;
		yy = int(ceilf(y[j]));
		if (yy < 0) yy = 0;
		if (yy < y2)
		{
			xi = (x[j] - x[i]) / (y[j] - y[i]);
			xx = (y2 - y[j]) * xi + x[j];
			if (y2 >= RenderTarget->GetHeight()) { xx = xx - (y2 - RenderTarget->GetHeight() + 1)*xi; y2 = RenderTarget->GetHeight()-1; }
			for (; y2 >= yy; --y2)
			{
				lastx[y2] = MAX (0, int(ceilf(xx))); xx = xx - xi;
			}
		}
		i = j;
	} while (i != mini);
	if (y2 == yy) lastx[yy] = lastx[yy+1];
	do
	{
		j = i + 1; if (j == n) j = 0;
		y2 = int(y[j]);
		if (y2 >= RenderTarget->GetHeight()) y2 = RenderTarget->GetHeight()-1;
		if (y2 > yy)
		{
			xi = (x[j] - x[i]) / (y[j] - y[i]);
			xx = (yy - y[i]) * xi + x[i];
			if (yy < 0) { xx = xx - xi*yy; yy = 0; }
			ncol = col; if (yy & 1) ncol = ncol ^ maskhack;
			for (; yy <= y2; ++yy)
			{
				//drawline2d(lastx[yy], yy, int(ceilf(xx)), yy, ncol); xx = xx + xi;
				int xxx = MIN(RenderTarget->GetWidth(), int(ceilf(xx)));
				if (yy < RenderTarget->GetHeight() && lastx[yy] < xxx) memset(RenderTarget->GetBuffer()+yy*RenderTarget->GetPitch()+lastx[yy], ncol, xxx-lastx[yy]);
				xx = xx + xi;
				ncol = ncol ^ maskhack;
			}
		}
		i = j;
	} while (i != maxi);

	if (col != bcol)
	{
		oz = n - 1;
		for (z = 0; z < n; ++z)
		{
			drawline2d(x[oz], y[oz], x[z], y[z], bcol);
			oz = z;
		}
	}
}

void drawtri(float x0, float y0, float x1, float y1, float x2, float y2, int col, int bcol)
{
	float x[3], y[3];
	x[0] = x0; y[0] = y0;
	x[1] = x1; y[1] = y1;
	x[2] = x2; y[2] = y2;
	fillconvpoly(x, y, 3, col, bcol);
}

void drawquad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, int col, int bcol)
{
	float x[4], y[4];
	x[0] = x0; y[0] = y0;
	x[1] = x1; y[1] = y1;
	x[2] = x2; y[2] = y2;
	x[3] = x3; y[3] = y3;
	fillconvpoly(x, y, 4, col, bcol); // 2 triangles
	if (col != bcol)
	{
		if (fabsf(y0-y2) < fabsf(y1-y3))
			drawline2d(x0,y0,x2,y2,bcol);
		else
			drawline2d(x1,y1,x3,y3,bcol);
	}
}

void printnum(int x, int y, int num)
{
	char foo[16]; mysnprintf (foo, countof(foo), "%d", num);
	RenderTarget->DrawText (SmallFont, CR_WHITE, x, y, foo);
}

void drawpolymosttest()
{
	float cx0 = 0, cy0 = 0, fx0 = 0, fy0 = 0;
	int ccol, fcol;
	PolyClipper::vsptype *vsp, *ovsp = &TestPoly.UsedList, *nvsp;

	fcol = 0; ccol = 0;

	RenderTarget->Clear(0, 0, RenderTarget->GetWidth(), RenderTarget->GetHeight(), 0, 0);
	for (vsp = ovsp->Next; vsp->Next != &TestPoly.UsedList; ovsp = vsp, vsp = nvsp)
	{
		nvsp = vsp->Next;
		if (vsp->CTag == -1 && vsp->FTag == -1)
		{ // Hide spans that have been clipped away
			vsp->Cy[0] = vsp->Cy[1] = vsp->Fy[0] = vsp->Fy[1] = RenderTarget->GetHeight()/2;
		}

		if (vsp->CTag != ovsp->CTag) cx0 = vsp->X, cy0 = vsp->Cy[0];
		if (vsp->CTag != nvsp->CTag)
		{ // fill the ceiling region
			maskhack = 0x18;
			drawquad(cx0, 0, nvsp->X, 0, nvsp->X, vsp->Cy[1], cx0, cy0, ccol, ccol);
			maskhack = 0; ccol ^= 0x18;
			printnum(int(cx0 + nvsp->X) / 2, 2, vsp->CTag);
		}

		if(vsp->FTag != ovsp->FTag) fx0 = vsp->X, fy0 = vsp->Fy[0];
		if(vsp->FTag != nvsp->FTag)
		{ // fill the floor region
			maskhack = 0x78;
			drawquad(fx0, fy0+1, nvsp->X, vsp->Fy[1]+1, nvsp->X, RenderTarget->GetHeight(), fx0, RenderTarget->GetHeight(), fcol, fcol);
			maskhack = 0; fcol ^= 0x78;
			printnum(int(fx0 + nvsp->X) / 2, RenderTarget->GetHeight()-10, vsp->FTag);
		}

		// fill the unclipped middle region
		drawquad(vsp->X, vsp->Cy[0], nvsp->X, vsp->Cy[1], nvsp->X, vsp->Fy[1], vsp->X, vsp->Fy[0], 0xC4, 0xE6);
	}

	int x = (pmx + 3) & ~7, y = (pmy + 3) & ~7;

	drawline2d (x - 3, y, x + 3, y, 30);
	drawline2d (x, y - 3, x, y + 3, 30);
	printnum ( 0, 20, x);
	printnum (50, 20, y);

	if (pt > 0 && px0 != px1)
	{
		if (px0 < px1)
		{
			drawline2d (px0, py0, px0, RenderTarget->GetHeight()-1, 47);
			drawline2d (px1, py1, px1, RenderTarget->GetHeight()-1, 47);
		}
		else
		{
			drawline2d (px0, py0, px0, 0, 47);
			drawline2d (px1, py1, px1, 0, 47);
		}
		drawline2d (px0, py0, px1, py1, 47);
	}
	if (pt == 2)
	{
		int i = 0;
		for (x = 0; x < numpoly; ++x)
		{
			if (polysize[x] == 3)
			{
				drawtri (polypts[i  ].x, polypts[i  ].y,
						 polypts[i+1].x, polypts[i+1].y,
						 polypts[i+2].x, polypts[i+2].y, 0x7f, 0x9f);
				i += 3;
			}
			else
			{
				drawquad (polypts[i  ].x, polypts[i  ].y,
						  polypts[i+1].x, polypts[i+1].y,
						  polypts[i+2].x, polypts[i+2].y,
						  polypts[i+3].x, polypts[i+3].y, 0x7f, 0x9f);
				i += 4;
			}
		}
	}
}

CCMD(initpolymosttest)
{
	double px[4], py[4];
	int test = 0;

	if (argv.argc() > 1)
		test = atoi(argv[1]);

	// Box
	px[0] = px[3] = 0;
	px[1] = px[2] = screen->GetWidth();
	py[0] = py[1] = screen->GetHeight()/4;
	py[2] = py[3] = screen->GetHeight()*3/4;

	switch (test)
	{
	case 1: // Shorter top edge
		px[0] = px[1]/6;
		px[1] = px[1]*5/6;
		break;

	case 2: // Shorter bottom edge
		px[3] = px[2]/6;
		px[2] = px[2]*5/6;
		break;

	case 3: // Shorter left edge
		py[0] = screen->GetHeight()*3/8;
		py[3] = screen->GetHeight()*5/8;
		break;

	case 4: // Shorter right edge
		py[1] = screen->GetHeight()*3/8;
		py[2] = screen->GetHeight()*5/8;
		break;

	case 5:
		px[0] = -1.0048981460288360/2+50;	py[0] = -1.0/2+50;
		px[1] = 643.00492866407262/2+50;		py[1] = -1.0/2+50;
		px[2] = 643.00492866407262/2+50;		py[2] = 483/2+50;
		px[3] = -1.0048981460288360/2+50;	py[3] = 483/2+50;
		break;
	}
	TestPoly.InitMosts (px, py, 4);
	pmx = screen->GetWidth()/2;
	pmy = screen->GetHeight()/2;
	pt = 0;
}

static void testpolycallback (double *dpx, double *dpy, int n, void *foo)
{
	if (numpoly == sizeof(polysize)) return;
	if (size_t(polypt + n) > countof(polypts)) return;
	polysize[numpoly++] = n;
	for (int i = 0; i < n; ++i)
	{
		polypts[polypt + i].x = dpx[i];
		polypts[polypt + i].y = dpy[i];
	}
	polypt += n;
}

void Polymost_Responder (event_t *ev)
{
	if (ev->type == EV_Mouse && pt < 2)
	{
		pmx = clamp (pmx + ev->x, 0, screen->GetWidth()-1);
		pmy = clamp (pmy - ev->y, 0, screen->GetHeight()-1);
		int x = (pmx + 3) & ~7, y = (pmy + 3) & ~7;
		if (pt == 0) px0 = x, py0 = y;
		if (pt <= 1) px1 = x, py1 = y;
	}
	else if (ev->type == EV_KeyDown && ev->data1 == KEY_MOUSE1)
	{
		if (pt == 0) pt = 1; else pt = 0;
	}
	else if (ev->type == EV_KeyUp && ev->data1 == KEY_MOUSE1)
	{
		if (pt == 1) { if (px0 != px1) pt++; else pt--; }
		if (pt == 2)
		{
			numpoly = polypt = 0;
			TestPoly.DoMost (px0, py0, px1, py1, testpolycallback, NULL);
		}
	}
}








extern fixed_t WallSZ1, WallSZ2, WallTX1, WallTX2, WallTY1, WallTY2, WallCX1, WallCX2, WallCY1, WallCY2;
extern int WallSX1, WallSX2;
extern float WallUoverZorg, WallUoverZstep, WallInvZorg, WallInvZstep, WallDepthScale, WallDepthOrg;
extern fixed_t	rw_backcz1, rw_backcz2;
extern fixed_t	rw_backfz1, rw_backfz2;
extern fixed_t	rw_frontcz1, rw_frontcz2;
extern fixed_t	rw_frontfz1, rw_frontfz2;
extern fixed_t	rw_offset;
extern bool rw_markmirror;
extern bool			rw_havehigh;
extern bool			rw_havelow;
extern bool		markfloor;
extern bool		markceiling;
extern FTexture *toptexture;
extern FTexture *bottomtexture;
extern FTexture *midtexture;
extern bool			rw_mustmarkfloor, rw_mustmarkceiling;
extern void R_NewWall(bool);
extern void R_GetExtraLight (int *light, const secplane_t &plane, FExtraLight *el);
extern int doorclosed;
extern int viewpitch;
#include "p_lnspec.h"

PolyClipper Mosts;
static bool drawback;

bool RP_SetupFrame (bool backside)
{
	double ox, oy, oz, ox2, oy2, oz2, r, px[6], py[6], pz[6], px2[6], py2[6], pz2[6], sx[6], sy[6];
	int i, j, n, n2;

	drawback = backside;
	if (backside)
	{
		viewangle += ANGLE_180;
		viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
		viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];
		viewtansin = FixedMul (FocalTangent, viewsin);
		viewtancos = FixedMul (FocalTangent, viewcos);
	}
		//Polymost supports true look up/down :) Here, we convert horizon to angle.
		//gchang&gshang are cos&sin of this angle (respectively)
//	gyxscale = ((double)xdimenscale)/131072.0;
	gyxscale = double(InvZtoScale)/65536.0;///131072.0/320.0;
//	gxyaspect = ((double)xyaspect*(double)viewingrange)*(5.0/(65536.0*262144.0));
//	gviewxrange = ((double)viewingrange)*((double)xdimen)/(32768.0*128.0);
	gcosang = double(viewcos)/65536.0;
	gsinang = double(viewsin)/65536.0;
	gcosang2 = gcosang*double(FocalTangent)/65536.0;
	gsinang2 = gsinang*double(FocalTangent)/65536.0;
	ghalfx = (double)viewwidth*0.5; grhalfxdown10 = 1.0/(((double)ghalfx)*1024);

		//global cos/sin height angle
	angle_t pitch = (angle_t)viewpitch;
	if (backside) pitch = ANGLE_180 - pitch;

	gshang = double(finesine[pitch>>ANGLETOFINESHIFT])/65536.0;
	gchang = double(finecosine[pitch>>ANGLETOFINESHIFT])/65536.0;
	ghoriz = double(viewheight)*0.5;

		//global cos/sin tilt angle
	gctang = cos(gtang);
	gstang = sin(gtang);
	if (fabs(gstang) < .001) //This hack avoids nasty precision bugs in domost()
		{ gstang = 0; if (gctang > 0) gctang = 1.0; else gctang = -1.0; }

		// Generate viewport trapezoid
	px[0] = px[3] = 0-1; px[1] = px[2] = viewwidth+3;
	py[0] = py[1] = 0-1; py[2] = py[3] = viewheight+3; n = 4;
	for(i=0;i<n;i++)
	{
		ox = px[i]-ghalfx; oy = py[i]-ghoriz; oz = ghalfx;

			//Tilt rotation (backwards)
		ox2 = ox*gctang + oy*gstang;
		oy2 = oy*gctang - ox*gstang;
		oz2 = oz;

			//Up/down rotation (backwards)
		px[i] = ox2;
		py[i] = oy2*gchang + oz2*gshang;
		pz[i] = oz2*gchang - oy2*gshang;
	}

		//Clip to SCISDIST plane
	n2 = 0;
	bool clipped = false;
	for(i=0;i<n;i++)
	{
		j = i+1; if (j >= n) j = 0;
		if (pz[i] >= SCISDIST/16) { px2[n2] = px[i]; py2[n2] = py[i]; pz2[n2] = pz[i]; n2++; }
		if ((pz[i] >= SCISDIST/16) != (pz[j] >= SCISDIST/16))
		{
			clipped = true;
			r = (SCISDIST/16-pz[i])/(pz[j]-pz[i]);
			px2[n2] = (px[j]-px[i])*r + px[i];
			py2[n2] = (py[j]-py[i])*r + py[i];
			pz2[n2] = SCISDIST/16;
			if (backside) py2[n2] -= r;
			n2++;
		}
	}
	if (n2 < 3) { return true; }
	for(i=0;i<n2;i++)
	{
		r = ghalfx / pz2[i];
		sx[i] = px2[i]*r + ghalfx;
		sy[i] = py2[i]*r + ghoriz;
	}
	Mosts.InitMosts (sx, sy, n2);
	return clipped;
}

CVAR (Bool, r_nopolytilt, 0, 0)

static void wireframe (double *dpx, double *dpy, int n, void *data)
{
	int wfc = (int)(size_t)data;
	double f, r, ox, oy, oz, ox2, oy2, oz2;
	float px[16], py[16];
	int i, j, k;

	if (n == 3)
	{
		if ((dpx[0]-dpx[1])*(dpy[2]-dpy[1]) >= (dpx[2]-dpx[1])*(dpy[0]-dpy[1])) return; //for triangle
	}
	else
	{
		f = 0; //f is area of polygon / 2
		for(i=n-2,j=n-1,k=0;k<n;i=j,j=k,k++) f += (dpx[i]-dpx[k])*dpy[j];
		if (f <= 0) return;
	}

	j = 0;
	for(i=0;i<n;i++)
	{
		if (!r_nopolytilt)
		{
			ox = dpx[i]-ghalfx;
			oy = dpy[i]-ghoriz;
			oz = ghalfx;

				//Up/down rotation
			ox2 = ox;
			oy2 = oy*gchang - oz*gshang;
			oz2 = oy*gshang + oz*gchang;

				//Tilt rotation
			ox = ox2*gctang - oy2*gstang;
			oy = ox2*gstang + oy2*gctang;
			oz = oz2;

			r = ghalfx / oz;

			if (drawback)
			{
				ox = -ox;
				oy = -oy;
			}
			px[j] = ox*r + ghalfx;
			py[j] = oy*r + ghoriz;
		}
		else
		{
			px[j] = (dpx[i]-ghalfx)*0.5+ghalfx;
			py[j] = (dpy[i]-ghoriz)*0.5+ghoriz;
		}
		if ((!j) || (px[j] != px[j-1]) || (py[j] != py[j-1])) j++;
	}
	if (r_polymost == 2 || r_polymost == 3)
	{
		fillconvpoly (px, py, j, wfc+4, r_polymost == 2 ? wfc : wfc+4);
	}
	if (r_polymost == 1)
	{
		for (i=0, k=j-1; i < j; k=i, ++i)
		{
			drawline2d (px[k], py[k], px[i], py[i], wfc);
		}
	}
}

void RP_AddLine (seg_t *line)
{
	static sector_t tempsec;	// killough 3/8/98: ceiling/water hack
	bool			solid;
	fixed_t			tx1, tx2, ty1, ty2;
	fixed_t			fcz0, ffz0, fcz1, ffz1, bcz0, bfz0, bcz1, bfz1;
	double x0, x1, xp0, yp0, xp1, yp1, oxp0, oyp0, nx0, ny0, nx1, ny1, ryp0, ryp1;
	double cy0, fy0, cy1, fy1, ocy0 = 0, ofy0 = 0, ocy1 = 0, ofy1 = 0;
	double t0, t1;
	double x, y;

	curline = line;

	if (line->linedef == NULL) return;

	//Offset&Rotate 3D coordinates to screen 3D space
	x = double(line->v1->x - viewx); y = double(line->v1->y - viewy);
	xp0 = x*gsinang  - y*gcosang;
	yp0 = x*gcosang2 + y*gsinang2;
	x = double(line->v2->x - viewx); y = double(line->v2->y - viewy);
	xp1 = x*gsinang  - y*gcosang;
	yp1 = x*gcosang2 + y*gsinang2;

	oxp0 = xp0; oyp0 = yp0;

		//Clip to close parallel-screen plane
	// [RH] Why oh why does clipping the left side of the wall against
	// a small SCISDIST not work for me? Strictly speaking, it's not
	// the clipping of the left side that's the problem, because if I
	// rotate the view 180 degrees so the right side of the wall is on
	// the left of the screen, then clipping the right side becomes
	// problematic.
#define WCLIPDIST (SCISDIST*256.0)
	if (yp0 < WCLIPDIST)
	{
		if (yp1 < WCLIPDIST) return;
		t0 = (WCLIPDIST-yp0)/(yp1-yp0);
		xp0 = (xp1-xp0)*t0+xp0;
		yp0 = WCLIPDIST;
		nx0 = (line->v2->x - line->v1->x)*t0 + line->v1->x;
		ny0 = (line->v2->y - line->v1->y)*t0 + line->v1->y;
	}
	else { t0 = 0.f; nx0 = line->v1->x; ny0 = line->v1->y; }
	if (yp1 < WCLIPDIST)
	{
		t1 = (WCLIPDIST-oyp0)/(yp1-oyp0);
		xp1 = (xp1-oxp0)*t1+oxp0;
		yp1 = WCLIPDIST;
		nx1 = (line->v2->x - line->v1->x)*t1 + line->v1->x;
		ny1 = (line->v2->y - line->v1->y)*t1 + line->v1->y;
	}
	else { t1 = 1.f; nx1 = line->v2->x; ny1 = line->v2->y; }

	ryp0 = 1.0/yp0; ryp1 = 1.0/yp1;

		//Generate screen coordinates for front side of wall
	x0 = ghalfx*xp0*ryp0 + ghalfx;
	x1 = ghalfx*xp1*ryp1 + ghalfx;
	if (x1 <= x0) return;

	ryp0 *= gyxscale; ryp1 *= gyxscale;
	fixed_t fnx0 = fixed_t(nx0), fny0 = fixed_t(ny0);
	fixed_t fnx1 = fixed_t(nx1), fny1 = fixed_t(ny1);

	fcz0 = frontsector->ceilingplane.ZatPoint (fnx0, fny0);
	ffz0 = frontsector->floorplane.ZatPoint (fnx0, fny0);
	fcz1 = frontsector->ceilingplane.ZatPoint (fnx1, fny1);
	ffz1 = frontsector->floorplane.ZatPoint (fnx1, fny1);
	bool cc = (t0>0 && x1 > 0);
	cy0 = ghoriz - double(fcz0 - viewz) * ryp0;
	fy0 = ghoriz - double(ffz0 - viewz) * ryp0;
	cy1 = ghoriz - double(fcz1 - viewz) * ryp1;
	fy1 = ghoriz - double(ffz1 - viewz) * ryp1;

/*
	tx1 = line->v1->x - viewx;
	tx2 = line->v2->x - viewx;
	ty1 = line->v1->y - viewy;
	ty2 = line->v2->y - viewy;
	// Reject lines not facing viewer
	if (DMulScale32 (ty1, tx1-tx2, tx1, ty2-ty1) >= 0)
		return;

	WallTX1 = DMulScale20 (tx1, viewsin, -ty1, viewcos);
	WallTX2 = DMulScale20 (tx2, viewsin, -ty2, viewcos);

	WallTY1 = DMulScale20 (tx1, viewtancos, ty1, viewtansin);
	WallTY2 = DMulScale20 (tx2, viewtancos, ty2, viewtansin);

	if (MirrorFlags & RF_XFLIP)
	{
		int t = 256-WallTX1;
		WallTX1 = 256-WallTX2;
		WallTX2 = t;
		swap (WallTY1, WallTY2);
	}

	if (WallTX1 >= -WallTY1)
	{
		if (WallTX1 > WallTY1) return;	// left edge is off the right side
		if (WallTY1 == 0) return;
		WallSX1 = (centerxfrac + Scale (WallTX1, centerxfrac, WallTY1)) >> FRACBITS;
		if (WallTX1 >= 0) WallSX1 = MIN (viewwidth, WallSX1+1); // fix for signed divide
		WallSZ1 = WallTY1;
	}
	else
	{
		if (WallTX2 < -WallTY2) return;	// wall is off the left side
		fixed_t den = WallTX1 - WallTX2 - WallTY2 + WallTY1;	
		if (den == 0) return;
		WallSX1 = 0;
		WallSZ1 = WallTY1 + Scale (WallTY2 - WallTY1, WallTX1 + WallTY1, den);
	}

	if (WallSZ1 < 32)
		return;

	if (WallTX2 <= WallTY2)
	{
		if (WallTX2 < -WallTY2) return;	// right edge is off the left side
		if (WallTY2 == 0) return;
		WallSX2 = (centerxfrac + Scale (WallTX2, centerxfrac, WallTY2)) >> FRACBITS;
		if (WallTX2 >= 0) WallSX2 = MIN (viewwidth, WallSX2+1);	// fix for signed divide
		WallSZ2 = WallTY2;
	}
	else
	{
		if (WallTX1 > WallTY1) return;	// wall is off the right side
		fixed_t den = WallTY2 - WallTY1 - WallTX2 + WallTX1;
		if (den == 0) return;
		WallSX2 = viewwidth;
		WallSZ2 = WallTY1 + Scale (WallTY2 - WallTY1, WallTX1 - WallTY1, den);
	}

	if (WallSZ2 < 32 || WallSX2 <= WallSX1)
		return;

	if (WallSX1 > WindowRight || WallSX2 < WindowLeft)
		return;
	if (line->linedef == NULL)
	{
		return;
	}
*/

	vertex_t *v1, *v2;

	v1 = line->linedef->v1;
	v2 = line->linedef->v2;

	if ((v1 == line->v1 && v2 == line->v2) || (v2 == line->v1 && v1 == line->v2))
	{ // The seg is the entire wall.
		if (MirrorFlags & RF_XFLIP)
		{
			WallUoverZorg = (float)WallTX2 * WallTMapScale;
			WallUoverZstep = (float)(-WallTY2) * 32.f;
			WallInvZorg = (float)(WallTX2 - WallTX1) * WallTMapScale;
			WallInvZstep = (float)(WallTY1 - WallTY2) * 32.f;
		}
		else
		{
			WallUoverZorg = (float)WallTX1 * WallTMapScale;
			WallUoverZstep = (float)(-WallTY1) * 32.f;
			WallInvZorg = (float)(WallTX1 - WallTX2) * WallTMapScale;
			WallInvZstep = (float)(WallTY2 - WallTY1) * 32.f;
		}
	}
	else
	{ // The seg is only part of the wall.
		if (line->linedef->sidedef[0] != line->sidedef)
		{
			swapvalues (v1, v2);
		}
		tx1 = v1->x - viewx;
		tx2 = v2->x - viewx;
		ty1 = v1->y - viewy;
		ty2 = v2->y - viewy;

		fixed_t fullx1 = DMulScale20 (tx1, viewsin, -ty1, viewcos);
		fixed_t fullx2 = DMulScale20 (tx2, viewsin, -ty2, viewcos);
		fixed_t fully1 = DMulScale20 (tx1, viewtancos, ty1, viewtansin);
		fixed_t fully2 = DMulScale20 (tx2, viewtancos, ty2, viewtansin);

		if (MirrorFlags & RF_XFLIP)
		{
			fullx1 = -fullx1;
			fullx2 = -fullx2;
		}

		WallUoverZorg = (float)fullx1 * WallTMapScale;
		WallUoverZstep = (float)(-fully1) * 32.f;
		WallInvZorg = (float)(fullx1 - fullx2) * WallTMapScale;
		WallInvZstep = (float)(fully2 - fully1) * 32.f;
	}
	WallDepthScale = WallInvZstep * WallTMapScale2;
	WallDepthOrg = -WallUoverZstep * WallTMapScale2;

	backsector = line->backsector;

	rw_mustmarkfloor = rw_mustmarkceiling = false;
	rw_havehigh = rw_havelow = false;

	// Single sided line?
	if (backsector == NULL)
	{
		solid = true;
	}
	else
	{
		// killough 3/8/98, 4/4/98: hack for invisible ceilings / deep water
		backsector = R_FakeFlat (backsector, &tempsec, NULL, NULL, true);

		doorclosed = 0;		// killough 4/16/98

		bcz0 = backsector->ceilingplane.ZatPoint (fnx0, fny0);
		bfz0 = backsector->floorplane.ZatPoint (fnx0, fny0);
		bcz1 = backsector->ceilingplane.ZatPoint (fnx1, fny1);
		bfz1 = backsector->floorplane.ZatPoint (fnx1, fny1);
		ocy0 = ghoriz - double(bcz0 - viewz) * ryp0;
		ofy0 = ghoriz - double(bfz0 - viewz) * ryp0;
		ocy1 = ghoriz - double(bcz1 - viewz) * ryp1;
		ofy1 = ghoriz - double(bfz1 - viewz) * ryp1;

		if (fcz0 > bcz0 || fcz1 > bcz1)
		{
			rw_havehigh = true;
		}
		if (ffz0 < bfz0 ||ffz1 < bfz1)
		{
			rw_havelow = true;
		}

		// Closed door.
		if ((bcz0 <= ffz0 && bcz1 <= ffz1) || (bfz0 >= fcz0 && bfz1 >= fcz1))
		{
			solid = true;
		}
		else if (
			(backsector->GetTexture(sector_t::ceiling) != skyflatnum ||
			frontsector->GetTexture(sector_t::ceiling) != skyflatnum)

		// if door is closed because back is shut:
		&& bcz0 <= bfz0 && bcz1 <= bfz1

		// preserve a kind of transparent door/lift special effect:
		&& bcz0 >= fcz0 && bcz1 >= fcz1

		&& ((bfz0 <= ffz0 && bfz1 <= ffz1) || line->sidedef->GetTexture(side_t::bottom).isValid()))
		{
		// killough 1/18/98 -- This function is used to fix the automap bug which
		// showed lines behind closed doors simply because the door had a dropoff.
		//
		// It assumes that Doom has already ruled out a door being closed because
		// of front-back closure (e.g. front floor is taller than back ceiling).

		// This fixes the automap floor height bug -- killough 1/18/98:
		// killough 4/7/98: optimize: save result in doorclosed for use in r_segs.c
			doorclosed = true;
			solid = true;
		}
		else if (frontsector->ceilingplane != backsector->ceilingplane ||
			frontsector->floorplane != backsector->floorplane)
		{
		// Window.
			solid = false;
		}
		else if (backsector->lightlevel != frontsector->lightlevel
			|| backsector->GetTexture(sector_t::floor) != frontsector->GetTexture(sector_t::floor)
			|| backsector->GetTexture(sector_t::ceiling) != frontsector->GetTexture(sector_t::ceiling)
			|| curline->sidedef->GetTexture(side_t::mid).isValid()

			// killough 3/7/98: Take flats offsets into account:
			|| backsector->GetXOffset(sector_t::floor) != frontsector->GetXOffset(sector_t::floor)
			|| backsector->GetYOffset(sector_t::floor) != frontsector->GetYOffset(sector_t::floor)
			|| backsector->GetXOffset(sector_t::ceiling) != frontsector->GetXOffset(sector_t::ceiling)
			|| backsector->GetYOffset(sector_t::ceiling) != frontsector->GetYOffset(sector_t::ceiling)
	
			|| backsector->GetPlaneLight(sector_t::floor) != frontsector->GetPlaneLight(sector_t::floor)
			|| backsector->GetPlaneLight(sector_t::ceiling) != frontsector->GetPlaneLight(sector_t::ceiling)
			|| backsector->GetFlags(sector_t::floor) != frontsector->GetFlags(sector_t::floor)
			|| backsector->GetFlags(sector_t::ceiling) != frontsector->GetFlags(sector_t::ceiling)

			// [RH] Also consider colormaps
			|| backsector->ColorMap != frontsector->ColorMap

			// [RH] and scaling
			|| backsector->GetXScale(sector_t::floor) != frontsector->GetXScale(sector_t::floor)
			|| backsector->GetYScale(sector_t::floor) != frontsector->GetYScale(sector_t::floor)
			|| backsector->GetXScale(sector_t::ceiling) != frontsector->GetXScale(sector_t::ceiling)
			|| backsector->GetYScale(sector_t::ceiling) != frontsector->GetYScale(sector_t::ceiling)

			// [RH] and rotation
			|| backsector->GetAngle(sector_t::floor) != frontsector->GetAngle(sector_t::floor)
			|| backsector->GetAngle(sector_t::ceiling) != frontsector->GetAngle(sector_t::ceiling)
			)
		{
			solid = false;
		}
		else
		{
			// Reject empty lines used for triggers and special events.
			// Identical floor and ceiling on both sides, identical light levels
			// on both sides, and no middle texture.
			return;
		}
	}

	if (line->linedef->special == Line_Horizon)
	{
		// Be aware: Line_Horizon does not work properly with sloped planes
		fcz1 = fcz0 = ffz1 = ffz0 = viewz;
		markceiling = markfloor = true;
	}

	// must be fixed in case the polymost renderer ever gets developed further!
	rw_offset = line->sidedef->GetTextureXOffset(side_t::mid);

	R_NewWall (false);
	if (rw_markmirror)
	{
		WallMirrors.Push (ds_p - drawsegs);
	}

	// render it
	if (markceiling)
	{
		Mosts.DoMost (x1, cy1, x0, cy0, wireframe, !cc||1?(void *)0xc3:(void*)0xca);
	}
	if (markfloor)
	{
		Mosts.DoMost (x0, fy0, x1, fy1, wireframe, (void *)0xd3);
	}
	if (midtexture)
	{ // one sided line
		//if(line->linedef-lines==1)Printf ("%g %g %g -> %g %g %g : %g %g -> %g %g\n", yp0, x0, fy0, yp1, x1, fy1, nx0, ny0, nx1, ny1);
		if (viewpitch > 0)
			Mosts.DoMost (x0, -10000, x1, -10000, wireframe, !cc||1?(void *)0x83:(void*)0x93);
		else
			Mosts.DoMost (x1, 10000, x0, 10000, wireframe,  !cc||1?(void *)0x83:(void*)0x93);
	}
	else
	{ // two sided line
		if (toptexture != NULL && toptexture->UseType != FTexture::TEX_Null)
		{ // top wall
			Mosts.DoMost (x1, ocy1, x0, ocy0, wireframe, (void *)0xa3);
		}
		if (bottomtexture != NULL && bottomtexture->UseType != FTexture::TEX_Null)
		{ // bottom wall
			Mosts.DoMost (x0, ofy0, x1, ofy1, wireframe, (void *)0x93);
/*			float bfz2 = float(-(rw_backfz2 - viewz)) / 65536.f;
			float bfz1 = float(-(rw_backfz1 - viewz)) / 65536.f;
			Mosts.DoMost (WallSX1, bfz1 * izs / sz1 + ghoriz,
						  WallSX2, bfz2 * izs / sz2 + ghoriz,
						  wireframe, (void *)0x93);*/
		}
	}
}

void RP_Subsector (subsector_t *sub)
{
	int 		 count;
	seg_t*		 line;
	sector_t     tempsec;				// killough 3/7/98: deep water hack
	int          floorlightlevel;		// killough 3/16/98: set floor lightlevel
	int          ceilinglightlevel;		// killough 4/11/98

	frontsector = sub->sector;
	count = sub->numlines;
	line = sub->firstline;

	// killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
	frontsector = R_FakeFlat(frontsector, &tempsec, &floorlightlevel,
						   &ceilinglightlevel, false);	// killough 4/11/98

	basecolormap = frontsector->ColorMap;
	R_GetExtraLight (&ceilinglightlevel, frontsector->ceilingplane, frontsector->ExtraLights);

	// [RH] set foggy flag
	foggy = level.fadeto || frontsector->ColorMap->Fade || (level.flags & LEVEL_HASFADETABLE);
	r_actualextralight = foggy ? 0 : extralight << 4;
	basecolormap = frontsector->ColorMap;
/*	ceilingplane = frontsector->ceilingplane.ZatPoint (viewx, viewy) > viewz ||
		frontsector->ceilingpic == skyflatnum ||
		(frontsector->CeilingSkyBox != NULL && frontsector->CeilingSkyBox->bAlways) ||
		(frontsector->heightsec && 
		 !(frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
		 frontsector->heightsec->floorpic == skyflatnum) ?
		R_FindPlane(frontsector->ceilingplane,		// killough 3/8/98
					frontsector->ceilingpic,
					ceilinglightlevel + r_actualextralight,				// killough 4/11/98
					frontsector->ceiling_xoffs,		// killough 3/7/98
					frontsector->ceiling_yoffs + frontsector->base_ceiling_yoffs,
					frontsector->ceiling_xscale,
					frontsector->ceiling_yscale,
					frontsector->ceiling_angle + frontsector->base_ceiling_angle,
					frontsector->sky,
					frontsector->CeilingSkyBox
					) : NULL;*/

	basecolormap = frontsector->ColorMap;
	R_GetExtraLight (&floorlightlevel, frontsector->floorplane, frontsector->ExtraLights);

	// killough 3/7/98: Add (x,y) offsets to flats, add deep water check
	// killough 3/16/98: add floorlightlevel
	// killough 10/98: add support for skies transferred from sidedefs
/*	floorplane = frontsector->floorplane.ZatPoint (viewx, viewy) < viewz || // killough 3/7/98
		frontsector->floorpic == skyflatnum ||
		(frontsector->FloorSkyBox != NULL && frontsector->FloorSkyBox->bAlways) ||
		(frontsector->heightsec &&
		 !(frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
		 frontsector->heightsec->ceilingpic == skyflatnum) ?
		R_FindPlane(frontsector->floorplane,
					frontsector->floorpic,
					floorlightlevel + r_actualextralight,				// killough 3/16/98
					frontsector->floor_xoffs,		// killough 3/7/98
					frontsector->floor_yoffs + frontsector->base_floor_yoffs,
					frontsector->floor_xscale,
					frontsector->floor_yscale,
					frontsector->floor_angle + frontsector->base_floor_angle,
					frontsector->sky,
					frontsector->FloorSkyBox
					) : NULL;*/

	// killough 9/18/98: Fix underwater slowdown, by passing real sector 
	// instead of fake one. Improve sprite lighting by basing sprite
	// lightlevels on floor & ceiling lightlevels in the surrounding area.
	// [RH] Handle sprite lighting like Duke 3D: If the ceiling is a sky, sprites are lit by
	// it, otherwise they are lit by the floor.
//	R_AddSprites (sub->sector, frontsector->ceilingpic == skyflatnum ?
//		ceilinglightlevel : floorlightlevel, FakeSide);

	// [RH] Add particles
//	int shade = LIGHT2SHADE((floorlightlevel + ceilinglightlevel)/2 + r_actualextralight);
//	for (WORD i = ParticlesInSubsec[sub-subsectors]; i != NO_PARTICLE; i = Particles[i].snext)
//	{
//		R_ProjectParticle (Particles + i, subsectors[sub-subsectors].sector, shade, FakeSide);
//	}

#if 0
	if (sub->poly)
	{ // Render the polyobj in the subsector first
		int polyCount = sub->poly->numsegs;
		seg_t **polySeg = sub->poly->segs;
		while (polyCount--)
		{
			RP_AddLine (*polySeg++);
		}
	}
#endif

	while (count--)
	{
		if (line->sidedef == NULL || !(line->sidedef->Flags & WALLF_POLYOBJ))
		{
			RP_AddLine (line);
		}
		line++;
	}
}

extern "C" const int checkcoord[12][4];

static bool RP_CheckBBox (fixed_t *bspcoord)
{
	int 				boxx;
	int 				boxy;
	int 				boxpos;

	fixed_t 			x1, y1, x2, y2;
	double				x, y, xp0, yp0, xp1, yp1, t, sx0, sx1;

	// Find the corners of the box
	// that define the edges from current viewpoint.
	if (viewx <= bspcoord[BOXLEFT])
		boxx = 0;
	else if (viewx < bspcoord[BOXRIGHT])
		boxx = 1;
	else
		boxx = 2;

	if (viewy >= bspcoord[BOXTOP])
		boxy = 0;
	else if (viewy > bspcoord[BOXBOTTOM])
		boxy = 1;
	else
		boxy = 2;

	boxpos = (boxy<<2)+boxx;
	if (boxpos == 5)
		return true;

	x1 = bspcoord[checkcoord[boxpos][0]] - viewx;
	y1 = bspcoord[checkcoord[boxpos][1]] - viewy;
	x2 = bspcoord[checkcoord[boxpos][2]] - viewx;
	y2 = bspcoord[checkcoord[boxpos][3]] - viewy;

	// check clip list for an open space

	// Sitting on a line?
	if (DMulScale32 (y1, x1-x2, x1, y2-y1) >= 0)
		return true;

	//Offset&Rotate 3D coordinates to screen 3D space
	x = double(x1); y = double(y1);
	xp0 = x*gsinang  - y*gcosang;
	yp0 = x*gcosang2 + y*gsinang2;
	x = double(x2); y = double(y2);
	xp1 = x*gsinang  - y*gcosang;
	yp1 = x*gcosang2 + y*gsinang2;

		//Clip to close parallel-screen plane
	if (yp0 < SCISDIST)
	{
		if (yp1 < SCISDIST) return false;
		t = (SCISDIST-yp0)/(yp1-yp0);
		xp0 = (xp1-xp0)*t+xp0;
		yp0 = SCISDIST;
	}
	if (yp1 < SCISDIST)
	{
		t = (SCISDIST-yp0)/(yp1-yp0);
		xp1 = (xp1-xp0)*t+xp0;
		yp1 = SCISDIST;
	}

		//Generate screen coordinates for front side of wall
	sx0 = ghalfx*xp0/yp0 + ghalfx;
	sx1 = ghalfx*xp1/yp1 + ghalfx;

	// Does not cross a pixel.
	if (sx1 <= sx0)
		return false;

	return Mosts.TestVisibleMost (sx0, sx1);
}

void RP_RenderBSPNode (void *node)
{
	if (numnodes == 0)
	{
		RP_Subsector (subsectors);
		return;
	}
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = R_PointOnSide (viewx, viewy, bsp);

		// Recursively divide front space (toward the viewer).
		RP_RenderBSPNode (bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;
		if (!RP_CheckBBox (bsp->bbox[side]))
			return;

		node = bsp->children[side];
	}
	RP_Subsector ((subsector_t *)((BYTE *)node - 1));
}

