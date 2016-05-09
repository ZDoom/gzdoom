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
// $Log:$
//
// DESCRIPTION:
//		Rendering main loop and setup functions,
//		 utility functions (BSP, geometry, trigonometry).
//		See tables.c, too.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "templates.h"
#include "doomdef.h"
#include "d_net.h"
#include "doomstat.h"
#include "m_random.h"
#include "m_bbox.h"
#include "r_local.h"
#include "r_plane.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "r_3dfloors.h"
#include "r_sky.h"
#include "st_stuff.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "stats.h"
#include "i_video.h"
#include "i_system.h"
#include "a_sharedglobal.h"
#include "r_data/r_translate.h"
#include "p_3dmidtex.h"
#include "r_data/r_interpolate.h"
#include "v_palette.h"
#include "po_man.h"
#include "p_effect.h"
#include "st_start.h"
#include "v_font.h"
#include "r_data/colormaps.h"
#include "farchive.h"
#include "p_maputl.h"

// MACROS ------------------------------------------------------------------

#if 0
#define TEST_X 32343794 
#define TEST_Y 111387517 
#define TEST_Z 2164524 
#define TEST_ANGLE 2468347904 
#endif


// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_SpanInitData ();
void R_DeinitSprites();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void R_ShutdownRenderer();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern short *openings;
extern bool r_fakingunderwater;
extern "C" int fuzzviewheight;
extern subsector_t *InSubsector;
extern bool r_showviewer;


// PRIVATE DATA DECLARATIONS -----------------------------------------------

static double CurrentVisibility = 8.f;
static double MaxVisForWall;
static double MaxVisForFloor;
bool r_dontmaplines;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (String, r_viewsize, "", CVAR_NOSET)
CVAR (Bool, r_shadercolormaps, true, CVAR_ARCHIVE)

double			r_BaseVisibility;
double			r_WallVisibility;
double			r_FloorVisibility;
float			r_TiltVisibility;
double			r_SpriteVisibility;
double			r_ParticleVisibility;

double			GlobVis;
fixed_t			viewingrangerecip;
double			FocalLengthX;
double			FocalLengthY;
FDynamicColormap*basecolormap;		// [RH] colormap currently drawing with
int				fixedlightlev;
lighttable_t	*fixedcolormap;
FSpecialColormap *realfixedcolormap;
double			WallTMapScale2;


bool			bRenderingToCanvas;	// [RH] True if rendering to a special canvas
double			globaluclip, globaldclip;
double			CenterX, CenterY;
double			YaspectMul;
double			BaseYaspectMul;		// yaspectmul without a forced aspect ratio
double			IYaspectMul;
double			InvZtoScale;

// just for profiling purposes
int 			linecount;
int 			loopcount;


//
// precalculated math tables
//

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t 		xtoviewangle[MAXWIDTH+1];

bool			foggy;			// [RH] ignore extralight and fullbright?
int				r_actualextralight;

void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*transcolfunc) (void);
void (*spanfunc) (void);

void (*hcolfunc_pre) (void);
void (*hcolfunc_post1) (int hx, int sx, int yl, int yh);
void (*hcolfunc_post2) (int hx, int sx, int yl, int yh);
void (*hcolfunc_post4) (int sx, int yl, int yh);

cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int lastcenteryfrac;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// R_InitTextureMapping
//
//==========================================================================

void R_InitTextureMapping ()
{
	int i;

	// Calc focallength so FieldOfView angles cover viewwidth.
	FocalLengthX = CenterX / FocalTangent;
	FocalLengthY = FocalLengthX * YaspectMul;

	// This is 1/FocalTangent before the widescreen extension of FOV.
	viewingrangerecip = FLOAT2FIXED(1. / tan(FieldOfView.Radians() / 2));


	// Now generate xtoviewangle for sky texture mapping.
	// [RH] Do not generate viewangletox, because texture mapping is no
	// longer done with trig, so it's not needed.
	const double slopestep = FocalTangent / centerx;
	double slope;

	for (i = centerx, slope = 0; i <= viewwidth; i++, slope += slopestep)
	{
		xtoviewangle[i] = angle_t((2 * M_PI - atan(slope)) * (ANGLE_180 / M_PI));
	}
	for (i = 0; i < centerx; i++)
	{
		xtoviewangle[i] = 0 - xtoviewangle[viewwidth - i - 1];
	}
}

//==========================================================================
//
// R_SetVisibility
//
// Changes how rapidly things get dark with distance
//
//==========================================================================

void R_SetVisibility(double vis)
{
	// Allow negative visibilities, just for novelty's sake
	vis = clamp(vis, -204.7, 204.7);	// (205 and larger do not work in 5:4 aspect ratio)

	CurrentVisibility = vis;

	if (FocalTangent == 0 || FocalLengthY == 0)
	{ // If r_visibility is called before the renderer is all set up, don't
	  // divide by zero. This will be called again later, and the proper
	  // values can be initialized then.
		return;
	}

	r_BaseVisibility = vis;

	// Prevent overflow on walls
	if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForWall)
		r_WallVisibility = -MaxVisForWall;
	else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForWall)
		r_WallVisibility = MaxVisForWall;
	else
		r_WallVisibility = r_BaseVisibility;

	r_WallVisibility = (InvZtoScale * SCREENWIDTH*BaseRatioSizes[WidescreenRatio][1] /
		(viewwidth*SCREENHEIGHT*3)) * (r_WallVisibility * FocalTangent);

	// Prevent overflow on floors/ceilings. Note that the calculation of
	// MaxVisForFloor means that planes less than two units from the player's
	// view could still overflow, but there is no way to totally eliminate
	// that while still using fixed point math.
	if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForFloor)
		r_FloorVisibility = -MaxVisForFloor;
	else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForFloor)
		r_FloorVisibility = MaxVisForFloor;
	else
		r_FloorVisibility = r_BaseVisibility;

	r_FloorVisibility = 160.0 * r_FloorVisibility / FocalLengthY;

	r_TiltVisibility = float(vis * FocalTangent * (16.f * 320.f) / viewwidth);
	r_SpriteVisibility = r_WallVisibility;
}

//==========================================================================
//
// R_GetVisibility
//
//==========================================================================

double R_GetVisibility()
{
	return CurrentVisibility;
}

//==========================================================================
//
// CCMD r_visibility
//
// Controls how quickly light ramps across a 1/z range. Set this, and it
// sets all the r_*Visibility variables (except r_SkyVisibilily, which is
// currently unused).
//
//==========================================================================

CCMD (r_visibility)
{
	if (argv.argc() < 2)
	{
		Printf ("Visibility is %g\n", R_GetVisibility());
	}
	else if (!netgame)
	{
		R_SetVisibility(atof(argv[1]));
	}
	else
	{
		Printf ("Visibility cannot be changed in net games.\n");
	}
}

//==========================================================================
//
// R_SetWindow
//
//==========================================================================

void R_SWRSetWindow(int windowSize, int fullWidth, int fullHeight, int stHeight, int trueratio)
{
	int virtheight, virtwidth, virtwidth2, virtheight2;

	if (!bRenderingToCanvas)
	{ // Set r_viewsize cvar to reflect the current view size
		UCVarValue value;
		char temp[16];

		mysnprintf (temp, countof(temp), "%d x %d", viewwidth, viewheight);
		value.String = temp;
		r_viewsize.ForceSet (value, CVAR_String);
	}

	fuzzviewheight = viewheight - 2;	// Maximum row the fuzzer can draw to

	lastcenteryfrac = 1<<30;
	CenterX = centerx;
	CenterY = centery;

	virtwidth = virtwidth2 = fullWidth;
	virtheight = virtheight2 = fullHeight;

	if (Is54Aspect(trueratio))
	{
		virtheight2 = virtheight2 * BaseRatioSizes[trueratio][3] / 48;
	}
	else
	{
		virtwidth2 = virtwidth2 * BaseRatioSizes[trueratio][3] / 48;
	}

	if (Is54Aspect(WidescreenRatio))
	{
		virtheight = virtheight * BaseRatioSizes[WidescreenRatio][3] / 48;
	}
	else
	{
		virtwidth = virtwidth * BaseRatioSizes[WidescreenRatio][3] / 48;
	}

	BaseYaspectMul = 320.0 * virtheight2 / (r_Yaspect * virtwidth2);
	YaspectMul = 320.0 * virtheight / (r_Yaspect * virtwidth);
	IYaspectMul = (double)virtwidth * r_Yaspect / 320.0 / virtheight;
	InvZtoScale = YaspectMul * CenterX;

	WallTMapScale2 = IYaspectMul / CenterX;

	// psprite scales
	pspritexscale = centerxwide / 160.0;
	pspriteyscale = pspritexscale * YaspectMul;
	pspritexiscale = 1 / pspritexscale;

	// thing clipping
	clearbufshort (screenheightarray, viewwidth, (short)viewheight);

	R_InitTextureMapping ();

	MaxVisForWall = (InvZtoScale * (SCREENWIDTH*r_Yaspect) /
		(viewwidth*SCREENHEIGHT * FocalTangent));
	MaxVisForWall = 32767.0 / MaxVisForWall;
	MaxVisForFloor = 32767.0 / (viewheight * FocalLengthY / 160);

	// Reset r_*Visibility vars
	R_SetVisibility(R_GetVisibility());
}

//==========================================================================
//
// CVAR r_columnmethod
//
// Selects which version of the seg renderers to use.
//
//==========================================================================

CUSTOM_CVAR (Int, r_columnmethod, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self != 0 && self != 1)
	{
		self = 1;
	}
	else
	{ // Trigger the change
		setsizeneeded = true;
	}
}

//==========================================================================
//
// R_Init
//
//==========================================================================

void R_InitRenderer()
{
	atterm(R_ShutdownRenderer);
	// viewwidth / viewheight are set by the defaults
	clearbufshort (zeroarray, MAXWIDTH, 0);

	R_InitPlanes ();
	R_InitShadeMaps();
	R_InitColumnDrawers ();

	colfunc = basecolfunc = R_DrawColumn;
	fuzzcolfunc = R_DrawFuzzColumn;
	transcolfunc = R_DrawTranslatedColumn;
	spanfunc = R_DrawSpan;

	// [RH] Horizontal column drawers
	hcolfunc_pre = R_DrawColumnHoriz;
	hcolfunc_post1 = rt_map1col;
	hcolfunc_post4 = rt_map4cols;
}

//==========================================================================
//
// R_ShutdownRenderer
//
//==========================================================================

static void R_ShutdownRenderer()
{
	R_DeinitSprites();
	R_DeinitPlanes();
	// Free openings
	if (openings != NULL)
	{
		M_Free (openings);
		openings = NULL;
	}

	// Free drawsegs
	if (drawsegs != NULL)
	{
		M_Free (drawsegs);
		drawsegs = NULL;
	}
}

//==========================================================================
//
// R_CopyStackedViewParameters
//
//==========================================================================

void R_CopyStackedViewParameters()
{
	stacked_viewpos = ViewPos;
	stacked_angle = ViewAngle;
	stacked_extralight = extralight;
	stacked_visibility = R_GetVisibility();
}

//==========================================================================
//
// R_SetupColormap
//
// Sets up special fixed colormaps
//
//==========================================================================

void R_SetupColormap(player_t *player)
{
	realfixedcolormap = NULL;
	fixedcolormap = NULL;
	fixedlightlev = -1;

	if (player != NULL && camera == player->mo)
	{
		if (player->fixedcolormap >= 0 && player->fixedcolormap < (int)SpecialColormaps.Size())
		{
			realfixedcolormap = &SpecialColormaps[player->fixedcolormap];
			if (RenderTarget == screen && (DFrameBuffer *)screen->Accel2D && r_shadercolormaps)
			{
				// Render everything fullbright. The copy to video memory will
				// apply the special colormap, so it won't be restricted to the
				// palette.
				fixedcolormap = realcolormaps;
			}
			else
			{
				fixedcolormap = SpecialColormaps[player->fixedcolormap].Colormap;
			}
		}
		else if (player->fixedlightlevel >= 0 && player->fixedlightlevel < NUMCOLORMAPS)
		{
			fixedlightlev = player->fixedlightlevel * 256;
		}
	}
	// [RH] Inverse light for shooting the Sigil
	if (fixedcolormap == NULL && extralight == INT_MIN)
	{
		fixedcolormap = SpecialColormaps[INVERSECOLORMAP].Colormap;
		extralight = 0;
	}
}

//==========================================================================
//
// R_SetupFreelook
//
// [RH] freelook stuff
//
//==========================================================================

void R_SetupFreelook()
{
	double dy;
		
	if (camera != NULL)
	{
		dy = FocalLengthY * (-ViewPitch).Tan();
	}
	else
	{
		dy = 0;
	}

	CenterY = (viewheight / 2.0) + dy;
	centery = xs_ToInt(CenterY);
	globaluclip = -CenterY / InvZtoScale;
	globaldclip = (viewheight - CenterY) / InvZtoScale;

	//centeryfrac &= 0xffff0000;
	int e, i;

	i = 0;
	e = viewheight;
	float focus = float(FocalLengthY);
	float den;
	float cy = float(CenterY);
	if (i < centery)
	{
		den = cy - i - 0.5f;
		if (e <= centery)
		{
			do {
				yslope[i] = focus / den;
				den -= 1;
			} while (++i < e);
		}
		else
		{
			do {
				yslope[i] = focus / den;
				den -= 1;
			} while (++i < centery);
			den = i - cy + 0.5f;
			do {
				yslope[i] = focus / den;
				den += 1;
			} while (++i < e);
		}
	}
	else
	{
		den = i - cy + 0.5f;
		do {
			yslope[i] = focus / den;
			den += 1;
		} while (++i < e);
	}
}

//==========================================================================
//
// R_EnterPortal
//
// [RH] Draw the reflection inside a mirror
// [ZZ] Merged with portal code, originally called R_EnterMirror
//
//==========================================================================

CVAR(Int, r_portal_recursions, 4, CVAR_ARCHIVE)
CVAR(Bool, r_highlight_portals, false, CVAR_ARCHIVE)

void R_HighlightPortal (PortalDrawseg* pds)
{
	// [ZZ] NO OVERFLOW CHECKS HERE
	//      I believe it won't break. if it does, blame me. :(

	BYTE color = (BYTE)BestColor((DWORD *)GPalette.BaseColors, 255, 0, 0, 0, 255);

	BYTE* pixels = RenderTarget->GetBuffer();
	// top edge
	for (int x = pds->x1; x < pds->x2; x++)
	{
		if (x < 0 || x >= RenderTarget->GetWidth())
			continue;

		int p = x - pds->x1;
		int Ytop = pds->ceilingclip[p];
		int Ybottom = pds->floorclip[p];

		if (x == pds->x1 || x == pds->x2-1)
		{
			RenderTarget->DrawLine(x, Ytop, x, Ybottom+1, color, 0);
			continue;
		}

		int YtopPrev = pds->ceilingclip[p-1];
		int YbottomPrev = pds->floorclip[p-1];

		if (abs(Ytop-YtopPrev) > 1)
			RenderTarget->DrawLine(x, YtopPrev, x, Ytop, color, 0);
		else *(pixels + Ytop * RenderTarget->GetPitch() + x) = color;

		if (abs(Ybottom-YbottomPrev) > 1)
			RenderTarget->DrawLine(x, YbottomPrev, x, Ybottom, color, 0);
		else *(pixels + Ybottom * RenderTarget->GetPitch() + x) = color;
	}
}

void R_EnterPortal (PortalDrawseg* pds, int depth)
{
	// [ZZ] check depth. fill portal with black if it's exceeding the visual recursion limit, and continue like nothing happened.
	if (depth >= r_portal_recursions)
	{
		BYTE color = (BYTE)BestColor((DWORD *)GPalette.BaseColors, 0, 0, 0, 0, 255);
		int spacing = RenderTarget->GetPitch();
		for (int x = pds->x1; x < pds->x2; x++)
		{
			if (x < 0 || x >= RenderTarget->GetWidth())
				continue;

			int Ytop = pds->ceilingclip[x-pds->x1];
			int Ybottom = pds->floorclip[x-pds->x1];

			BYTE *dest = RenderTarget->GetBuffer() + x + Ytop * spacing;

			for (int y = Ytop; y <= Ybottom; y++)
			{
				*dest = color;
				dest += spacing;
			}
		}

		if (r_highlight_portals)
			R_HighlightPortal(pds);

		return;
	}

	DAngle startang = ViewAngle;
	DVector3 startpos = ViewPos;
	DVector3 savedpath[2] = { ViewPath[0], ViewPath[1] };
	ActorRenderFlags savedvisibility = camera? camera->renderflags & RF_INVISIBLE : ActorRenderFlags::FromInt(0);

	CurrentPortalUniq++;

	unsigned int portalsAtStart = WallPortals.Size ();

	if (pds->mirror)
	{
		//vertex_t *v1 = ds->curline->v1;
		vertex_t *v1 = pds->src->v1;

		// Reflect the current view behind the mirror.
		if (pds->src->Delta().X == 0)
		{ // vertical mirror
			ViewPos.X = v1->fX() - startpos.X + v1->fX();
		}
		else if (pds->src->Delta().Y == 0)
		{ // horizontal mirror
			ViewPos.Y = v1->fY() - startpos.Y + v1->fY();
		}
		else
		{ // any mirror
			vertex_t *v2 = pds->src->v2;

			double dx = v2->fX() - v1->fX();
			double dy = v2->fY() - v1->fY();
			double x1 = v1->fX();
			double y1 = v1->fY();
			double x = startpos.X;
			double y = startpos.Y;

			// the above two cases catch len == 0
			double r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

			ViewPos.X = (x1 + r * dx)*2 - x;
			ViewPos.Y = (y1 + r * dy)*2 - y;
		}
		ViewAngle = pds->src->Delta().Angle() - startang;
	}
	else
	{
		P_TranslatePortalXY(pds->src, ViewPos.X, ViewPos.Y);
		P_TranslatePortalZ(pds->src, ViewPos.Z);
		P_TranslatePortalAngle(pds->src, ViewAngle);
		P_TranslatePortalXY(pds->src, ViewPath[0].X, ViewPath[0].Y);
		P_TranslatePortalXY(pds->src, ViewPath[1].X, ViewPath[1].Y);

		if (!r_showviewer && camera && P_PointOnLineSidePrecise(ViewPath[0], pds->dst) != P_PointOnLineSidePrecise(ViewPath[1], pds->dst))
		{
			double distp = (ViewPath[0] - ViewPath[1]).Length();
			if (distp > EQUAL_EPSILON)
			{
				double dist1 = (ViewPos - ViewPath[0]).Length();
				double dist2 = (ViewPos - ViewPath[1]).Length();

				if (dist1 + dist2 < distp + 1)
				{
					camera->renderflags |= RF_INVISIBLE;
				}
			}
		}
	}

	ViewSin = ViewAngle.Sin();
	ViewCos = ViewAngle.Cos();

	ViewTanSin = FocalTangent * ViewSin;
	ViewTanCos = FocalTangent * ViewCos;

	R_CopyStackedViewParameters();

	validcount++;
	PortalDrawseg* prevpds = CurrentPortal;
	CurrentPortal = pds;

	R_ClearPlanes (false);
	R_ClearClipSegs (pds->x1, pds->x2);

	WindowLeft = pds->x1;
	WindowRight = pds->x2;
	
	// RF_XFLIP should be removed before calling the root function
	int prevmf = MirrorFlags;
	if (pds->mirror)
	{
		if (MirrorFlags & RF_XFLIP)
			MirrorFlags &= ~RF_XFLIP;
		else MirrorFlags |= RF_XFLIP;
	}

	// some portals have height differences, account for this here
	R_3D_EnterSkybox(); // push 3D floor height map
	CurrentPortalInSkybox = false; // first portal in a skybox should set this variable to false for proper clipping in skyboxes.

	// first pass, set clipping
	memcpy (ceilingclip + pds->x1, &pds->ceilingclip[0], pds->len*sizeof(*ceilingclip));
	memcpy (floorclip + pds->x1, &pds->floorclip[0], pds->len*sizeof(*floorclip));

	InSubsector = NULL;
	R_RenderBSPNode (nodes + numnodes - 1);
	R_3D_ResetClip(); // reset clips (floor/ceiling)
	if (!savedvisibility && camera) camera->renderflags &= ~RF_INVISIBLE;

	PlaneCycles.Clock();
	R_DrawPlanes ();
	R_DrawPortals ();
	PlaneCycles.Unclock();

	double vzp = ViewPos.Z;

	int prevuniq = CurrentPortalUniq;
	// depth check is in another place right now
	unsigned int portalsAtEnd = WallPortals.Size ();
	for (; portalsAtStart < portalsAtEnd; portalsAtStart++)
	{
		R_EnterPortal (&WallPortals[portalsAtStart], depth + 1);
	}
	int prevuniq2 = CurrentPortalUniq;
	CurrentPortalUniq = prevuniq;

	NetUpdate();

	MaskedCycles.Clock(); // [ZZ] count sprites in portals/mirrors along with normal ones.
	R_DrawMasked ();	  //      this is required since with portals there often will be cases when more than 80% of the view is inside a portal.
	MaskedCycles.Unclock();

	NetUpdate();

	R_3D_LeaveSkybox(); // pop 3D floor height map
	CurrentPortalUniq = prevuniq2;

	// draw a red line around a portal if it's being highlighted
	if (r_highlight_portals)
		R_HighlightPortal(pds);

	CurrentPortal = prevpds;
	MirrorFlags = prevmf;
	ViewAngle = startang;
	ViewPos = startpos;
	ViewPath[0] = savedpath[0];
	ViewPath[1] = savedpath[1];
}

//==========================================================================
//
// R_SetupBuffer
//
// Precalculate all row offsets and fuzz table.
//
//==========================================================================

void R_SetupBuffer ()
{
	static BYTE *lastbuff = NULL;

	int pitch = RenderTarget->GetPitch();
	BYTE *lineptr = RenderTarget->GetBuffer() + viewwindowy*pitch + viewwindowx;

	if (dc_pitch != pitch || lineptr != lastbuff)
	{
		if (dc_pitch != pitch)
		{
			dc_pitch = pitch;
			R_InitFuzzTable (pitch);
#if defined(X86_ASM) || defined(X64_ASM)
			ASM_PatchPitch ();
#endif
		}
		dc_destorg = lineptr;
		for (int i = 0; i < RenderTarget->GetHeight(); i++)
		{
			ylookup[i] = i * pitch;
		}
	}
}

//==========================================================================
//
// R_RenderActorView
//
//==========================================================================

void R_RenderActorView (AActor *actor, bool dontmaplines)
{
	WallCycles.Reset();
	PlaneCycles.Reset();
	MaskedCycles.Reset();
	WallScanCycles.Reset();

	fakeActive = 0; // kg3D - reset fake floor indicator
	R_3D_ResetClip(); // reset clips (floor/ceiling)

	R_SetupBuffer ();
	R_SetupFrame (actor);

	// Clear buffers.
	R_ClearClipSegs (0, viewwidth);
	R_ClearDrawSegs ();
	R_ClearPlanes (true);
	R_ClearSprites ();

	NetUpdate ();

	// [RH] Show off segs if r_drawflat is 1
	if (r_drawflat)
	{
		hcolfunc_pre = R_FillColumnHorizP;
		hcolfunc_post1 = rt_copy1col;
		hcolfunc_post4 = rt_copy4cols;
		colfunc = R_FillColumnP;
		spanfunc = R_FillSpan;
	}
	else
	{
		hcolfunc_pre = R_DrawColumnHoriz;
		hcolfunc_post1 = rt_map1col;
		hcolfunc_post4 = rt_map4cols;
		colfunc = basecolfunc;
		spanfunc = R_DrawSpan;
	}

	WindowLeft = 0;
	WindowRight = viewwidth;
	MirrorFlags = 0;
	CurrentPortal = NULL;
	CurrentPortalUniq = 0;

	r_dontmaplines = dontmaplines;
	
	// [RH] Hack to make windows into underwater areas possible
	r_fakingunderwater = false;

	// [RH] Setup particles for this frame
	P_FindParticleSubsectors ();

	WallCycles.Clock();
	ActorRenderFlags savedflags = camera->renderflags;
	// Never draw the player unless in chasecam mode
	if (!r_showviewer)
	{
		camera->renderflags |= RF_INVISIBLE;
	}
	// Link the polyobjects right before drawing the scene to reduce the amounts of calls to this function
	PO_LinkToSubsectors();
	InSubsector = NULL;
	R_RenderBSPNode (nodes + numnodes - 1);	// The head node is the last node output.
	R_3D_ResetClip(); // reset clips (floor/ceiling)
	camera->renderflags = savedflags;
	WallCycles.Unclock();

	NetUpdate ();

	if (viewactive)
	{
		PlaneCycles.Clock();
		R_DrawPlanes ();
		R_DrawPortals ();
		PlaneCycles.Unclock();

		// [RH] Walk through mirrors
		// [ZZ] Merged with portals
		size_t lastportal = WallPortals.Size();
		for (unsigned int i = 0; i < lastportal; i++)
		{
			R_EnterPortal(&WallPortals[i], 0);
		}

		CurrentPortal = NULL;
		CurrentPortalUniq = 0;

		NetUpdate ();
		
		MaskedCycles.Clock();
		R_DrawMasked ();
		MaskedCycles.Unclock();

		NetUpdate ();
	}
	WallPortals.Clear ();
	interpolator.RestoreInterpolations ();
	R_SetupBuffer ();

	// If we don't want shadered colormaps, NULL it now so that the
	// copy to the screen does not use a special colormap shader.
	if (!r_shadercolormaps)
	{
		realfixedcolormap = NULL;
	}
}

//==========================================================================
//
// R_RenderViewToCanvas
//
// Pre: Canvas is already locked.
//
//==========================================================================

void R_RenderViewToCanvas (AActor *actor, DCanvas *canvas,
	int x, int y, int width, int height, bool dontmaplines)
{
	const bool savedviewactive = viewactive;

	viewwidth = width;
	RenderTarget = canvas;
	bRenderingToCanvas = true;

	R_SetWindow (12, width, height, height);
	viewwindowx = x;
	viewwindowy = y;
	viewactive = true;

	R_RenderActorView (actor, dontmaplines);

	RenderTarget = screen;
	bRenderingToCanvas = false;
	R_ExecuteSetViewSize ();
	screen->Lock (true);
	R_SetupBuffer ();
	screen->Unlock ();
	viewactive = savedviewactive;
}

//==========================================================================
//
// R_MultiresInit
//
// Called from V_SetResolution()
//
//==========================================================================

void R_MultiresInit ()
{
	R_PlaneInitData ();
}


//==========================================================================
//
// STAT fps
//
// Displays statistics about rendering times
//
//==========================================================================
extern cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;
extern cycle_t FrameCycles;

ADD_STAT (fps)
{
	FString out;
	out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms",
		FrameCycles.TimeMS(), WallCycles.TimeMS(), PlaneCycles.TimeMS(), MaskedCycles.TimeMS());
	return out;
}


static double f_acc, w_acc,p_acc,m_acc;
static int acc_c;

ADD_STAT (fps_accumulated)
{
	f_acc += FrameCycles.TimeMS();
	w_acc += WallCycles.TimeMS();
	p_acc += PlaneCycles.TimeMS();
	m_acc += MaskedCycles.TimeMS();
	acc_c++;
	FString out;
	out.Format("frame=%04.1f ms  walls=%04.1f ms  planes=%04.1f ms  masked=%04.1f ms  %d counts",
		f_acc/acc_c, w_acc/acc_c, p_acc/acc_c, m_acc/acc_c, acc_c);
	Printf(PRINT_LOG, "%s\n", out.GetChars());
	return out;
}

//==========================================================================
//
// STAT wallcycles
//
// Displays the minimum number of cycles spent drawing walls
//
//==========================================================================

static double bestwallcycles = HUGE_VAL;

ADD_STAT (wallcycles)
{
	FString out;
	double cycles = WallCycles.Time();
	if (cycles && cycles < bestwallcycles)
		bestwallcycles = cycles;
	out.Format ("%g", bestwallcycles);
	return out;
}

//==========================================================================
//
// CCMD clearwallcycles
//
// Resets the count of minimum wall drawing cycles
//
//==========================================================================

CCMD (clearwallcycles)
{
	bestwallcycles = HUGE_VAL;
}

#if 1
// To use these, also uncomment the clock/unclock in wallscan
static double bestscancycles = HUGE_VAL;

ADD_STAT (scancycles)
{
	FString out;
	double scancycles = WallScanCycles.Time();
	if (scancycles && scancycles < bestscancycles)
		bestscancycles = scancycles;
	out.Format ("%g", bestscancycles);
	return out;
}

CCMD (clearscancycles)
{
	bestscancycles = HUGE_VAL;
}
#endif
