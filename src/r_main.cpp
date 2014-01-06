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
void RP_RenderBSPNode (void *node);
bool RP_SetupFrame (bool backside);
void R_DeinitSprites();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void R_ShutdownRenderer();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern short *openings;
extern bool r_fakingunderwater;
extern "C" int fuzzviewheight;


// PRIVATE DATA DECLARATIONS -----------------------------------------------

static float CurrentVisibility = 8.f;
static fixed_t MaxVisForWall;
static fixed_t MaxVisForFloor;
static bool polyclipped;
extern bool r_showviewer;
bool r_dontmaplines;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (String, r_viewsize, "", CVAR_NOSET)
CVAR (Int, r_polymost, 0, 0)
CVAR (Bool, r_shadercolormaps, true, CVAR_ARCHIVE)

fixed_t			r_BaseVisibility;
fixed_t			r_WallVisibility;
fixed_t			r_FloorVisibility;
float			r_TiltVisibility;
fixed_t			r_SpriteVisibility;
fixed_t			r_ParticleVisibility;
fixed_t			r_SkyVisibility;

fixed_t			GlobVis;
fixed_t			viewingrangerecip;
fixed_t			FocalLengthX;
fixed_t			FocalLengthY;
float			FocalLengthXfloat;
FDynamicColormap*basecolormap;		// [RH] colormap currently drawing with
int				fixedlightlev;
lighttable_t	*fixedcolormap;
FSpecialColormap *realfixedcolormap;
float			WallTMapScale;
float			WallTMapScale2;


bool			bRenderingToCanvas;	// [RH] True if rendering to a special canvas
fixed_t			globaluclip, globaldclip;
fixed_t 		centerxfrac;
fixed_t 		centeryfrac;
fixed_t			yaspectmul;
fixed_t			baseyaspectmul;		// yaspectmul without a forced aspect ratio
float			iyaspectmulfloat;
fixed_t			InvZtoScale;

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
void (STACK_ARGS *hcolfunc_post4) (int sx, int yl, int yh);

cycle_t WallCycles, PlaneCycles, MaskedCycles, WallScanCycles;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int lastcenteryfrac;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// viewangletox
//
// Used solely for construction the xtoviewangle table.
//
//==========================================================================

static inline int viewangletox(int i)
{
	if (finetangent[i] > FRACUNIT*2)
	{
		return -1;
	}
	else if (finetangent[i] < -FRACUNIT*2)
	{
		return viewwidth+1;
	}
	else
	{
		int t = FixedMul(finetangent[i], FocalLengthX);
		t = (centerxfrac - t + FRACUNIT-1) >> FRACBITS;
		return clamp(t, -1, viewwidth+1);
	}
}

//==========================================================================
//
// R_InitTextureMapping
//
//==========================================================================

void R_InitTextureMapping ()
{
	int i, x;

	// Calc focallength so FieldOfView fineangles covers viewwidth.
	FocalLengthX = FixedDiv (centerxfrac, FocalTangent);
	FocalLengthY = Scale (centerxfrac, yaspectmul, FocalTangent);
	FocalLengthXfloat = (float)FocalLengthX / 65536.f;

	// This is 1/FocalTangent before the widescreen extension of FOV.
	viewingrangerecip = DivScale32(1, finetangent[FINEANGLES/4+(FieldOfView/2)]);

	// [RH] Do not generate viewangletox, because texture mapping is no
	// longer done with trig, so it's not needed.

	// Now generate xtoviewangle for sky texture mapping.
	// We do this with a hybrid approach: The center 90 degree span is
	// constructed as per the original code:
	//   Scan xtoviewangle to find the smallest view angle that maps to x.
	//   (viewangletox is sorted in non-increasing order.)
	//   This reduces the chances of "doubling-up" of texture columns in
	//   the drawn sky texture.
	// The remaining arcs are done with tantoangle instead.

	const int t1 = MAX<int>(centerx - (FocalLengthX >> FRACBITS), 0);
	const int t2 = MIN<int>(centerx + (FocalLengthX >> FRACBITS), viewwidth);
	const fixed_t dfocus = FocalLengthX >> DBITS;

	for (i = 0, x = t2; x >= t1; --x)
	{
		while(viewangletox(i) > x)
		{
			++i;
		}
		xtoviewangle[x] = (i << ANGLETOFINESHIFT) - ANGLE_90;
	}
	for (x = t2 + 1; x <= viewwidth; ++x)
	{
		xtoviewangle[x] = ANGLE_270 + tantoangle[dfocus / (x - centerx)];
	}
	for (x = 0; x < t1; ++x)
	{
		xtoviewangle[x] = (angle_t)(-(signed)xtoviewangle[viewwidth - x]);
	}
}

//==========================================================================
//
// R_SetVisibility
//
// Changes how rapidly things get dark with distance
//
//==========================================================================

void R_SetVisibility (float vis)
{
	// Allow negative visibilities, just for novelty's sake
	//vis = clamp (vis, -204.7f, 204.7f);

	CurrentVisibility = vis;

	if (FocalTangent == 0 || FocalLengthY == 0)
	{ // If r_visibility is called before the renderer is all set up, don't
	  // divide by zero. This will be called again later, and the proper
	  // values can be initialized then.
		return;
	}

	r_BaseVisibility = xs_RoundToInt(vis * 65536.f);

	// Prevent overflow on walls
	if (r_BaseVisibility < 0 && r_BaseVisibility < -MaxVisForWall)
		r_WallVisibility = -MaxVisForWall;
	else if (r_BaseVisibility > 0 && r_BaseVisibility > MaxVisForWall)
		r_WallVisibility = MaxVisForWall;
	else
		r_WallVisibility = r_BaseVisibility;

	r_WallVisibility = FixedMul (Scale (InvZtoScale, SCREENWIDTH*BaseRatioSizes[WidescreenRatio][1],
		viewwidth*SCREENHEIGHT*3), FixedMul (r_WallVisibility, FocalTangent));

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

	r_FloorVisibility = Scale (160*FRACUNIT, r_FloorVisibility, FocalLengthY);

	r_TiltVisibility = vis * (float)FocalTangent * (16.f * 320.f) / (float)viewwidth;
	r_SpriteVisibility = r_WallVisibility;
}

//==========================================================================
//
// R_GetVisibility
//
//==========================================================================

float R_GetVisibility ()
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
		R_SetVisibility ((float)atof (argv[1]));
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
	halfviewwidth = (viewwidth >> 1) - 1;

	lastcenteryfrac = 1<<30;
	centerxfrac = centerx<<FRACBITS;
	centeryfrac = centery<<FRACBITS;

	virtwidth = virtwidth2 = fullWidth;
	virtheight = virtheight2 = fullHeight;

	if (trueratio & 4)
	{
		virtheight2 = virtheight2 * BaseRatioSizes[trueratio][3] / 48;
	}
	else
	{
		virtwidth2 = virtwidth2 * BaseRatioSizes[trueratio][3] / 48;
	}

	if (WidescreenRatio & 4)
	{
		virtheight = virtheight * BaseRatioSizes[WidescreenRatio][3] / 48;
	}
	else
	{
		virtwidth = virtwidth * BaseRatioSizes[WidescreenRatio][3] / 48;
	}

	baseyaspectmul = Scale(320 << FRACBITS, virtheight2, r_Yaspect * virtwidth2);
	yaspectmul = Scale ((320<<FRACBITS), virtheight, r_Yaspect * virtwidth);
	iyaspectmulfloat = (float)virtwidth * r_Yaspect / 320.f / (float)virtheight;
	InvZtoScale = yaspectmul * centerx;

	WallTMapScale = (float)centerx * 32.f;
	WallTMapScale2 = iyaspectmulfloat * 2.f / (float)centerx;

	// psprite scales
	pspritexscale = (centerxwide << FRACBITS) / 160;
	pspriteyscale = FixedMul (pspritexscale, yaspectmul);
	pspritexiscale = FixedDiv (FRACUNIT, pspritexscale);

	// thing clipping
	clearbufshort (screenheightarray, viewwidth, (short)viewheight);

	R_InitTextureMapping ();

	MaxVisForWall = FixedMul (Scale (InvZtoScale, SCREENWIDTH*r_Yaspect,
		viewwidth*SCREENHEIGHT), FocalTangent);
	MaxVisForWall = FixedDiv (0x7fff0000, MaxVisForWall);
	MaxVisForFloor = Scale (FixedDiv (0x7fff0000, viewheight<<(FRACBITS-2)), FocalLengthY, 160*FRACUNIT);

	// Reset r_*Visibility vars
	R_SetVisibility (R_GetVisibility ());
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
	stacked_viewx = viewx;
	stacked_viewy = viewy;
	stacked_viewz = viewz;
	stacked_angle = viewangle;
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
	{
		fixed_t dy;
		
		if (camera != NULL)
		{
			dy = FixedMul (FocalLengthY, finetangent[(ANGLE_90-viewpitch)>>ANGLETOFINESHIFT]);
		}
		else
		{
			dy = 0;
		}

		centeryfrac = (viewheight << (FRACBITS-1)) + dy;
		centery = centeryfrac >> FRACBITS;
		globaluclip = FixedDiv (-centeryfrac, InvZtoScale);
		globaldclip = FixedDiv ((viewheight<<FRACBITS)-centeryfrac, InvZtoScale);

		//centeryfrac &= 0xffff0000;
		int e, i;

		i = 0;
		e = viewheight;
		fixed_t focus = FocalLengthY;
		fixed_t den;
		if (i < centery)
		{
			den = centeryfrac - (i << FRACBITS) - FRACUNIT/2;
			if (e <= centery)
			{
				do {
					yslope[i] = FixedDiv (focus, den);
					den -= FRACUNIT;
				} while (++i < e);
			}
			else
			{
				do {
					yslope[i] = FixedDiv (focus, den);
					den -= FRACUNIT;
				} while (++i < centery);
				den = (i << FRACBITS) - centeryfrac + FRACUNIT/2;
				do {
					yslope[i] = FixedDiv (focus, den);
					den += FRACUNIT;
				} while (++i < e);
			}
		}
		else
		{
			den = (i << FRACBITS) - centeryfrac + FRACUNIT/2;
			do {
				yslope[i] = FixedDiv (focus, den);
				den += FRACUNIT;
			} while (++i < e);
		}
	}
}

void R_SetupPolymost()
{
	if (r_polymost)
	{
		polyclipped = RP_SetupFrame (false);
	}
}

//==========================================================================
//
// R_EnterMirror
//
// [RH] Draw the reflection inside a mirror
//
//==========================================================================

void R_EnterMirror (drawseg_t *ds, int depth)
{
	angle_t startang = viewangle;
	fixed_t startx = viewx;
	fixed_t starty = viewy;

	CurrentMirror++;

	unsigned int mirrorsAtStart = WallMirrors.Size ();

	vertex_t *v1 = ds->curline->v1;

	// Reflect the current view behind the mirror.
	if (ds->curline->linedef->dx == 0)
	{ // vertical mirror
		viewx = v1->x - startx + v1->x;
	}
	else if (ds->curline->linedef->dy == 0)
	{ // horizontal mirror
		viewy = v1->y - starty + v1->y;
	}
	else
	{ // any mirror--use floats to avoid integer overflow
		vertex_t *v2 = ds->curline->v2;

		float dx = FIXED2FLOAT(v2->x - v1->x);
		float dy = FIXED2FLOAT(v2->y - v1->y);
		float x1 = FIXED2FLOAT(v1->x);
		float y1 = FIXED2FLOAT(v1->y);
		float x = FIXED2FLOAT(startx);
		float y = FIXED2FLOAT(starty);

		// the above two cases catch len == 0
		float r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

		viewx = FLOAT2FIXED((x1 + r * dx)*2 - x);
		viewy = FLOAT2FIXED((y1 + r * dy)*2 - y);
	}
	viewangle = 2*R_PointToAngle2 (ds->curline->v1->x, ds->curline->v1->y,
								   ds->curline->v2->x, ds->curline->v2->y) - startang;

	viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
	viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];

	viewtansin = FixedMul (FocalTangent, viewsin);
	viewtancos = FixedMul (FocalTangent, viewcos);

	R_CopyStackedViewParameters();

	validcount++;
	ActiveWallMirror = ds->curline;

	R_ClearPlanes (false);
	R_ClearClipSegs (ds->x1, ds->x2 + 1);

	memcpy (ceilingclip + ds->x1, openings + ds->sprtopclip, (ds->x2 - ds->x1 + 1)*sizeof(*ceilingclip));
	memcpy (floorclip + ds->x1, openings + ds->sprbottomclip, (ds->x2 - ds->x1 + 1)*sizeof(*floorclip));

	WindowLeft = ds->x1;
	WindowRight = ds->x2;
	MirrorFlags = (depth + 1) & 1;

	R_RenderBSPNode (nodes + numnodes - 1);
	R_3D_ResetClip(); // reset clips (floor/ceiling)

	R_DrawPlanes ();
	R_DrawSkyBoxes ();

	// Allow up to 4 recursions through a mirror
	if (depth < 4)
	{
		unsigned int mirrorsAtEnd = WallMirrors.Size ();

		for (; mirrorsAtStart < mirrorsAtEnd; mirrorsAtStart++)
		{
			R_EnterMirror (drawsegs + WallMirrors[mirrorsAtStart], depth + 1);
		}
	}

	viewangle = startang;
	viewx = startx;
	viewy = starty;
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
	WindowRight = viewwidth - 1;
	MirrorFlags = 0;
	ActiveWallMirror = NULL;

	r_dontmaplines = dontmaplines;
	
	// [RH] Hack to make windows into underwater areas possible
	r_fakingunderwater = false;

	// [RH] Setup particles for this frame
	P_FindParticleSubsectors ();

	WallCycles.Clock();
	DWORD savedflags = camera->renderflags;
	// Never draw the player unless in chasecam mode
	if (!r_showviewer)
	{
		camera->renderflags |= RF_INVISIBLE;
	}
	// Link the polyobjects right before drawing the scene to reduce the amounts of calls to this function
	PO_LinkToSubsectors();
	if (r_polymost < 2)
	{
		R_RenderBSPNode (nodes + numnodes - 1);	// The head node is the last node output.
		R_3D_ResetClip(); // reset clips (floor/ceiling)
	}
	camera->renderflags = savedflags;
	WallCycles.Unclock();

	NetUpdate ();

	if (viewactive)
	{
		PlaneCycles.Clock();
		R_DrawPlanes ();
		R_DrawSkyBoxes ();
		PlaneCycles.Unclock();

		// [RH] Walk through mirrors
		size_t lastmirror = WallMirrors.Size ();
		for (unsigned int i = 0; i < lastmirror; i++)
		{
			R_EnterMirror (drawsegs + WallMirrors[i], 0);
		}

		NetUpdate ();
		
		MaskedCycles.Clock();
		R_DrawMasked ();
		MaskedCycles.Unclock();

		NetUpdate ();

		if (r_polymost)
		{
			RP_RenderBSPNode (nodes + numnodes - 1);
			if (polyclipped)
			{
				RP_SetupFrame (true);
				RP_RenderBSPNode (nodes + numnodes - 1);
			}
		}
	}
	WallMirrors.Clear ();
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
