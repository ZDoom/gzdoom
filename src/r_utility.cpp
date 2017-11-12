//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
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
#include "r_renderer.h"
#include "r_data/colormaps.h"
#include "serializer.h"
#include "r_utility.h"
#include "d_player.h"
#include "p_local.h"
#include "g_levellocals.h"
#include "p_maputl.h"
#include "sbar.h"
#include "math/cmath.h"
#include "vm.h"
#include "i_time.h"


// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern bool DrawFSHUD;		// [RH] Defined in d_main.cpp
EXTERN_CVAR (Bool, cl_capfps)

// TYPES -------------------------------------------------------------------

struct InterpolationViewer
{
	struct instance
	{
		DVector3 Pos;
		DRotator Angles;
	};

	AActor *ViewActor;
	int otic;
	instance Old, New;
};

// PRIVATE DATA DECLARATIONS -----------------------------------------------
static TArray<InterpolationViewer> PastViewers;
static FRandom pr_torchflicker ("TorchFlicker");
static FRandom pr_hom;
bool NoInterpolateView;	// GL needs access to this.
static TArray<DVector3a> InterpolationPath;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CVAR (Bool, r_deathcamera, false, CVAR_ARCHIVE)
CVAR (Int, r_clearbuffer, 0, 0)
CVAR (Bool, r_drawvoxels, true, 0)
CVAR (Bool, r_drawplayersprites, true, 0)	// [RH] Draw player sprites?
CUSTOM_CVAR(Float, r_quakeintensity, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0.f) self = 0.f;
	else if (self > 1.f) self = 1.f;
}

int 			viewwindowx;
int 			viewwindowy;
int				viewwidth;
int 			viewheight;

FRenderViewpoint::FRenderViewpoint()
{
	player = nullptr;
	Pos = { 0.0, 0.0, 0.0 };
	ActorPos = { 0.0, 0.0, 0.0 };
	Angles = { 0.0, 0.0, 0.0 };
	Path[0] = { 0.0, 0.0, 0.0 };
	Path[1] = { 0.0, 0.0, 0.0 };
	Cos = 0.0;
	Sin = 0.0;
	TanCos = 0.0;
	TanSin = 0.0;
	camera = nullptr;
	sector = nullptr;
	FieldOfView = 90.; // Angles in the SCREENWIDTH wide window
	TicFrac = 0.0;
	FrameTime = 0;
	extralight = 0;
	showviewer = false;
}

FRenderViewpoint r_viewpoint;
FViewWindow		r_viewwindow;

bool			r_NoInterpolate;

angle_t			LocalViewAngle;
int				LocalViewPitch;
bool			LocalKeyboardTurner;

int				setblocks;
bool			setsizeneeded;

unsigned int	R_OldBlend = ~0;
int 			validcount = 1; 	// increment every time a check is made
FCanvasTextureInfo *FCanvasTextureInfo::List;

DVector3a view;
DAngle viewpitch;

DEFINE_GLOBAL(LocalViewPitch);

// CODE --------------------------------------------------------------------
static void R_Shutdown ();

//==========================================================================
//
// R_SetFOV
//
// Changes the field of view in degrees
//
//==========================================================================

void R_SetFOV (FRenderViewpoint &viewpoint, DAngle fov)
{

	if (fov < 5.) fov = 5.;
	else if (fov > 170.) fov = 170.;
	if (fov != viewpoint.FieldOfView)
	{
		viewpoint.FieldOfView = fov;
		setsizeneeded = true;
	}
}

//==========================================================================
//
// R_SetViewSize
//
// Do not really change anything here, because it might be in the middle
// of a refresh. The change will take effect next refresh.
//
//==========================================================================

void R_SetViewSize (int blocks)
{
	setsizeneeded = true;
	setblocks = blocks;
}

//==========================================================================
//
// R_SetWindow
//
//==========================================================================

void R_SetWindow (FRenderViewpoint &viewpoint, FViewWindow &viewwindow, int windowSize, int fullWidth, int fullHeight, int stHeight, bool renderingToCanvas)
{
	if (windowSize >= 11)
	{
		viewwidth = fullWidth;
		freelookviewheight = viewheight = fullHeight;
	}
	else if (windowSize == 10)
	{
		viewwidth = fullWidth;
		viewheight = stHeight;
		freelookviewheight = fullHeight;
	}
	else
	{
		viewwidth = ((setblocks*fullWidth)/10) & (~15);
		viewheight = ((setblocks*stHeight)/10)&~7;
		freelookviewheight = ((setblocks*fullHeight)/10)&~7;
	}

	if (renderingToCanvas)
	{
		viewwindow.WidescreenRatio = fullWidth / (float)fullHeight;
	}
	else
	{
		viewwindow.WidescreenRatio = ActiveRatio(fullWidth, fullHeight);
	}

	DrawFSHUD = (windowSize == 11);
	
	// [RH] Sky height fix for screens not 200 (or 240) pixels tall
	R_InitSkyMap ();

	viewwindow.centery = viewheight/2;
	viewwindow.centerx = viewwidth/2;
	if (AspectTallerThanWide(viewwindow.WidescreenRatio))
	{
		viewwindow.centerxwide = viewwindow.centerx;
	}
	else
	{
		viewwindow.centerxwide = viewwindow.centerx * AspectMultiplier(viewwindow.WidescreenRatio) / 48;
	}


	DAngle fov = viewpoint.FieldOfView;

	// For widescreen displays, increase the FOV so that the middle part of the
	// screen that would be visible on a 4:3 display has the requested FOV.
	if (viewwindow.centerxwide != viewwindow.centerx)
	{ // centerxwide is what centerx would be if the display was not widescreen
		fov = DAngle::ToDegrees(2 * atan(viewwindow.centerx * tan(fov.Radians()/2) / double(viewwindow.centerxwide)));
		if (fov > 170.) fov = 170.;
	}
	viewwindow.FocalTangent = tan(fov.Radians() / 2);
}

//==========================================================================
//
// R_ExecuteSetViewSize
//
//==========================================================================

void R_ExecuteSetViewSize (FRenderViewpoint &viewpoint, FViewWindow &viewwindow)
{
	setsizeneeded = false;
	V_SetBorderNeedRefresh();

	R_SetWindow (viewpoint, viewwindow, setblocks, SCREENWIDTH, SCREENHEIGHT, StatusBar->GetTopOfStatusbar());

	// Handle resize, e.g. smaller view windows with border and/or status bar.
	viewwindowx = (screen->GetWidth() - viewwidth) >> 1;

	// Same with base row offset.
	viewwindowy = (viewwidth == screen->GetWidth()) ? 0 : (StatusBar->GetTopOfStatusbar() - viewheight) >> 1;
}

//==========================================================================
//
// r_visibility
//
// Controls how quickly light ramps across a 1/z range.
//
//==========================================================================

double R_ClampVisibility(double vis)
{
	// Allow negative visibilities, just for novelty's sake
	return clamp(vis, -204.7, 204.7);	// (205 and larger do not work in 5:4 aspect ratio)
}

CUSTOM_CVAR(Float, r_visibility, 8.0f, CVAR_NOINITCALL)
{
	if (netgame && self != 8.0f)
	{
		Printf("Visibility cannot be changed in net games.\n");
		self = 8.0f;
	}
	else
	{
		float clampValue = (float)R_ClampVisibility(self);
		if (self != clampValue)
			self = clampValue;
	}
}

//==========================================================================
//
// R_GetGlobVis
//
// Calculates the global visibility constant used by the software renderer
//
//==========================================================================

double R_GetGlobVis(const FViewWindow &viewwindow, double vis)
{
	vis = R_ClampVisibility(vis);

	double virtwidth = screen->GetWidth();
	double virtheight = screen->GetHeight();

	if (AspectTallerThanWide(viewwindow.WidescreenRatio))
	{
		virtheight = (virtheight * AspectMultiplier(viewwindow.WidescreenRatio)) / 48;
	}
	else
	{
		virtwidth = (virtwidth * AspectMultiplier(viewwindow.WidescreenRatio)) / 48;
	}

	double YaspectMul = 320.0 * virtheight / (200.0 * virtwidth);
	double InvZtoScale = YaspectMul * viewwindow.centerx;

	double wallVisibility = vis;

	// Prevent overflow on walls
	double maxVisForWall = (InvZtoScale * (screen->GetWidth() * r_Yaspect) / (viewwidth * screen->GetHeight() * viewwindow.FocalTangent));
	maxVisForWall = 32767.0 / maxVisForWall;
	if (vis < 0 && vis < -maxVisForWall)
		wallVisibility = -maxVisForWall;
	else if (vis > 0 && vis > maxVisForWall)
		wallVisibility = maxVisForWall;

	wallVisibility = InvZtoScale * screen->GetWidth() * AspectBaseHeight(viewwindow.WidescreenRatio) / (viewwidth * screen->GetHeight() * 3) * (wallVisibility * viewwindow.FocalTangent);

	return wallVisibility / viewwindow.FocalTangent;
}

//==========================================================================
//
// CVAR screenblocks
//
// Selects the size of the visible window
//
//==========================================================================

CUSTOM_CVAR (Int, screenblocks, 10, CVAR_ARCHIVE)
{
	if (self > 12)
		self = 12;
	else if (self < 3)
		self = 3;
	else
		R_SetViewSize (self);
}

//==========================================================================
//
// R_PointInSubsector
//
//==========================================================================

subsector_t *R_PointInSubsector (fixed_t x, fixed_t y)
{
	node_t *node;
	int side;

	// single subsector is a special case
	if (level.nodes.Size() == 0)
		return &level.subsectors[0];
				
	node = level.HeadNode();

	do
	{
		side = R_PointOnSide (x, y, node);
		node = (node_t *)node->children[side];
	}
	while (!((size_t)node & 1));
		
	return (subsector_t *)((uint8_t *)node - 1);
}

//==========================================================================
//
// R_Init
//
//==========================================================================

void R_Init ()
{
	atterm (R_Shutdown);

	StartScreen->Progress();
	// Colormap init moved back to InitPalette()
	//R_InitColormaps ();
	//StartScreen->Progress();

	R_InitTranslationTables ();
	R_SetViewSize (screenblocks);
	Renderer->Init();
}

//==========================================================================
//
// R_Shutdown
//
//==========================================================================

static void R_Shutdown ()
{
	R_DeinitTranslationTables();
	R_DeinitColormaps ();
	FCanvasTextureInfo::EmptyList();
}

//==========================================================================
//
// R_InterpolateView
//
//==========================================================================

//CVAR (Int, tf, 0, 0)
EXTERN_CVAR (Bool, cl_noprediction)

void R_InterpolateView (FRenderViewpoint &viewpoint, player_t *player, double Frac, InterpolationViewer *iview)
{
	if (NoInterpolateView)
	{
		InterpolationPath.Clear();
		NoInterpolateView = false;
		iview->Old = iview->New;
	}
	int oldgroup = R_PointInSubsector(iview->Old.Pos)->sector->PortalGroup;
	int newgroup = R_PointInSubsector(iview->New.Pos)->sector->PortalGroup;

	DAngle oviewangle = iview->Old.Angles.Yaw;
	DAngle nviewangle = iview->New.Angles.Yaw;
	if (!cl_capfps)
	{
		if ((iview->Old.Pos.X != iview->New.Pos.X || iview->Old.Pos.Y != iview->New.Pos.Y) && InterpolationPath.Size() > 0)
		{
			DVector3 view = iview->New.Pos;

			// Interpolating through line portals is a messy affair.
			// What needs be done is to store the portal transitions of the camera actor as waypoints
			// and then find out on which part of the path the current view lies.
			// Needless to say, this doesn't work for chasecam mode.
			if (!viewpoint.showviewer)
			{
				double pathlen = 0;
				double zdiff = 0;
				double totalzdiff = 0;
				DAngle adiff = 0.;
				DAngle totaladiff = 0.;
				double oviewz = iview->Old.Pos.Z;
				double nviewz = iview->New.Pos.Z;
				DVector3a oldpos = { { iview->Old.Pos.X, iview->Old.Pos.Y, 0 }, 0. };
				DVector3a newpos = { { iview->New.Pos.X, iview->New.Pos.Y, 0 }, 0. };
				InterpolationPath.Push(newpos);	// add this to  the array to simplify the loops below

				for (unsigned i = 0; i < InterpolationPath.Size(); i += 2)
				{
					DVector3a &start = i == 0 ? oldpos : InterpolationPath[i - 1];
					DVector3a &end = InterpolationPath[i];
					pathlen += (end.pos - start.pos).Length();
					totalzdiff += start.pos.Z;
					totaladiff += start.angle;
				}
				double interpolatedlen = Frac * pathlen;

				for (unsigned i = 0; i < InterpolationPath.Size(); i += 2)
				{
					DVector3a &start = i == 0 ? oldpos : InterpolationPath[i - 1];
					DVector3a &end = InterpolationPath[i];
					double fraglen = (end.pos - start.pos).Length();
					zdiff += start.pos.Z;
					adiff += start.angle;
					if (fraglen <= interpolatedlen)
					{
						interpolatedlen -= fraglen;
					}
					else
					{
						double fragfrac = interpolatedlen / fraglen;
						oviewz += zdiff;
						nviewz -= totalzdiff - zdiff;
						oviewangle += adiff;
						nviewangle -= totaladiff - adiff;
						DVector2 viewpos = start.pos + (fragfrac * (end.pos - start.pos));
						viewpoint.Pos = { viewpos, oviewz + Frac * (nviewz - oviewz) };
						break;
					}
				}
				InterpolationPath.Pop();
				viewpoint.Path[0] = iview->Old.Pos;
				viewpoint.Path[1] = viewpoint.Path[0] + (InterpolationPath[0].pos - viewpoint.Path[0]).XY().MakeResize(pathlen);
			}
		}
		else
		{
			DVector2 disp = Displacements.getOffset(oldgroup, newgroup);
			viewpoint.Pos = iview->Old.Pos + (iview->New.Pos - iview->Old.Pos - disp) * Frac;
			viewpoint.Path[0] = viewpoint.Path[1] = iview->New.Pos;
		}
	}
	else
	{
		viewpoint.Pos = iview->New.Pos;
		viewpoint.Path[0] = viewpoint.Path[1] = iview->New.Pos;
	}
	if (player != NULL &&
		!(player->cheats & CF_INTERPVIEW) &&
		player - players == consoleplayer &&
		viewpoint.camera == player->mo &&
		!demoplayback &&
		iview->New.Pos.X == viewpoint.camera->X() &&
		iview->New.Pos.Y == viewpoint.camera->Y() && 
		!(player->cheats & (CF_TOTALLYFROZEN|CF_FROZEN)) &&
		player->playerstate == PST_LIVE &&
		player->mo->reactiontime == 0 &&
		!NoInterpolateView &&
		!paused &&
		(!netgame || !cl_noprediction) &&
		!LocalKeyboardTurner)
	{
		viewpoint.Angles.Yaw = (nviewangle + AngleToFloat(LocalViewAngle & 0xFFFF0000)).Normalized180();
		DAngle delta = player->centering ? DAngle(0.) : AngleToFloat(int(LocalViewPitch & 0xFFFF0000));
		viewpoint.Angles.Pitch = clamp<DAngle>((iview->New.Angles.Pitch - delta).Normalized180(), player->MinPitch, player->MaxPitch);
		viewpoint.Angles.Roll = iview->New.Angles.Roll.Normalized180();
	}
	else
	{
		viewpoint.Angles.Pitch = (iview->Old.Angles.Pitch + deltaangle(iview->Old.Angles.Pitch, iview->New.Angles.Pitch) * Frac).Normalized180();
		viewpoint.Angles.Yaw = (oviewangle + deltaangle(oviewangle, nviewangle) * Frac).Normalized180();
		viewpoint.Angles.Roll = (iview->Old.Angles.Roll + deltaangle(iview->Old.Angles.Roll, iview->New.Angles.Roll) * Frac).Normalized180();
	}
	
	// Due to interpolation this is not necessarily the same as the sector the camera is in.
	viewpoint.sector = R_PointInSubsector(viewpoint.Pos)->sector;
	bool moved = false;
	while (!viewpoint.sector->PortalBlocksMovement(sector_t::ceiling))
	{
		if (viewpoint.Pos.Z > viewpoint.sector->GetPortalPlaneZ(sector_t::ceiling))
		{
			viewpoint.Pos += viewpoint.sector->GetPortalDisplacement(sector_t::ceiling);
			viewpoint.ActorPos += viewpoint.sector->GetPortalDisplacement(sector_t::ceiling);
			viewpoint.sector = R_PointInSubsector(viewpoint.Pos)->sector;
			moved = true;
		}
		else break;
	}
	if (!moved)
	{
		while (!viewpoint.sector->PortalBlocksMovement(sector_t::floor))
		{
			if (viewpoint.Pos.Z < viewpoint.sector->GetPortalPlaneZ(sector_t::floor))
			{
				viewpoint.Pos += viewpoint.sector->GetPortalDisplacement(sector_t::floor);
				viewpoint.ActorPos += viewpoint.sector->GetPortalDisplacement(sector_t::floor);
				viewpoint.sector = R_PointInSubsector(viewpoint.Pos)->sector;
				moved = true;
			}
			else break;
		}
	}
}

//==========================================================================
//
// R_ResetViewInterpolation
//
//==========================================================================

void R_ResetViewInterpolation ()
{
	InterpolationPath.Clear();
	NoInterpolateView = true;
}

//==========================================================================
//
// R_SetViewAngle
//
//==========================================================================

void R_SetViewAngle (FRenderViewpoint &viewpoint, const FViewWindow &viewwindow)
{
	viewpoint.Sin = viewpoint.Angles.Yaw.Sin();
	viewpoint.Cos = viewpoint.Angles.Yaw.Cos();

	viewpoint.TanSin = viewwindow.FocalTangent * viewpoint.Sin;
	viewpoint.TanCos = viewwindow.FocalTangent * viewpoint.Cos;
}

//==========================================================================
//
// FindPastViewer
//
//==========================================================================

static InterpolationViewer *FindPastViewer (AActor *actor)
{
	for (unsigned int i = 0; i < PastViewers.Size(); ++i)
	{
		if (PastViewers[i].ViewActor == actor)
		{
			return &PastViewers[i];
		}
	}

	// Not found, so make a new one
	InterpolationViewer iview;
	memset(&iview, 0, sizeof(iview));
	iview.ViewActor = actor;
	iview.otic = -1;
	InterpolationPath.Clear();
	return &PastViewers[PastViewers.Push (iview)];
}

//==========================================================================
//
// R_FreePastViewers
//
//==========================================================================

void R_FreePastViewers ()
{
	InterpolationPath.Clear();
	PastViewers.Clear ();
}

//==========================================================================
//
// R_ClearPastViewer
//
// If the actor changed in a non-interpolatable way, remove it.
//
//==========================================================================

void R_ClearPastViewer (AActor *actor)
{
	InterpolationPath.Clear();
	for (unsigned int i = 0; i < PastViewers.Size(); ++i)
	{
		if (PastViewers[i].ViewActor == actor)
		{
			// Found it, so remove it.
			if (i == PastViewers.Size())
			{
				PastViewers.Delete (i);
			}
			else
			{
				PastViewers.Pop (PastViewers[i]);
			}
		}
	}
}

//==========================================================================
//
// R_RebuildViewInterpolation
//
//==========================================================================

void R_RebuildViewInterpolation(player_t *player)
{
	if (player == NULL || player->camera == NULL)
		return;

	if (!NoInterpolateView)
		return;
	NoInterpolateView = false;

	InterpolationViewer *iview = FindPastViewer(player->camera);

	iview->Old = iview->New;
	InterpolationPath.Clear();
}

//==========================================================================
//
// R_GetViewInterpolationStatus
//
//==========================================================================

bool R_GetViewInterpolationStatus()
{
	return NoInterpolateView;
}


//==========================================================================
//
// R_ClearInterpolationPath
//
//==========================================================================

void R_ClearInterpolationPath()
{
	InterpolationPath.Clear();
}

//==========================================================================
//
// R_AddInterpolationPoint
//
//==========================================================================

void R_AddInterpolationPoint(const DVector3a &vec)
{
	InterpolationPath.Push(vec);
}

//==========================================================================
//
// QuakePower
//
//==========================================================================

static double QuakePower(double factor, double intensity, double offset)
{ 
	double randumb;
	if (intensity == 0)
	{
		randumb = 0;
	}
	else
	{
		randumb = pr_torchflicker.GenRand_Real2() * (intensity * 2) - intensity;
	}
	return factor * (offset + randumb);
}

//==========================================================================
//
// R_SetupFrame
//
//==========================================================================

void R_SetupFrame (FRenderViewpoint &viewpoint, FViewWindow &viewwindow, AActor *actor)
{
	if (actor == NULL)
	{
		I_Error ("Tried to render from a NULL actor.");
	}

	player_t *player = actor->player;
	unsigned int newblend;
	InterpolationViewer *iview;
	bool unlinked = false;

	if (player != NULL && player->mo == actor)
	{	// [RH] Use camera instead of viewplayer
		viewpoint.camera = player->camera;
		if (viewpoint.camera == NULL)
		{
			viewpoint.camera = player->camera = player->mo;
		}
	}
	else
	{
		viewpoint.camera = actor;
	}

	if (viewpoint.camera == NULL)
	{
		I_Error ("You lost your body. Bad dehacked work is likely to blame.");
	}

	iview = FindPastViewer (viewpoint.camera);

	int nowtic = I_GetTime ();
	if (iview->otic != -1 && nowtic > iview->otic)
	{
		iview->otic = nowtic;
		iview->Old = iview->New;
	}

	if (player != NULL && gamestate != GS_TITLELEVEL &&
		((player->cheats & CF_CHASECAM) || (r_deathcamera && viewpoint.camera->health <= 0)))
	{
		sector_t *oldsector = R_PointInSubsector(iview->Old.Pos)->sector;
		// [RH] Use chasecam view
		DVector3 campos;
		DAngle camangle;
		P_AimCamera (viewpoint.camera, campos, camangle, viewpoint.sector, unlinked);	// fixme: This needs to translate the angle, too.
		iview->New.Pos = campos;
		iview->New.Angles.Yaw = camangle;
		
		viewpoint.showviewer = true;
		// Interpolating this is a very complicated thing because nothing keeps track of the aim camera's movement, so whenever we detect a portal transition
		// it's probably best to just reset the interpolation for this move.
		// Note that this can still cause problems with unusually linked portals
		if (viewpoint.sector->PortalGroup != oldsector->PortalGroup || (unlinked && ((iview->New.Pos.XY() - iview->Old.Pos.XY()).LengthSquared()) > 256*256))
		{
			iview->otic = nowtic;
			iview->Old = iview->New;
			r_NoInterpolate = true;
		}
		viewpoint.ActorPos = campos;
	}
	else
	{
		viewpoint.ActorPos = iview->New.Pos = { viewpoint.camera->Pos().XY(), viewpoint.camera->player ? viewpoint.camera->player->viewz : viewpoint.camera->Z() + viewpoint.camera->GetCameraHeight() };
		viewpoint.sector = viewpoint.camera->Sector;
		viewpoint.showviewer = false;
	}
	iview->New.Angles = viewpoint.camera->Angles;
	if (viewpoint.camera->player != 0)
	{
		player = viewpoint.camera->player;
	}

	if (iview->otic == -1 || r_NoInterpolate)
	{
		R_ResetViewInterpolation ();
		iview->otic = nowtic;
	}

	viewpoint.TicFrac = I_GetTimeFrac ();
	if (cl_capfps || r_NoInterpolate)
	{
		viewpoint.TicFrac = 1.;
	}
	R_InterpolateView (viewpoint, player, viewpoint.TicFrac, iview);

	R_SetViewAngle (viewpoint, viewwindow);

	interpolator.DoInterpolations (viewpoint.TicFrac);

	// Keep the view within the sector's floor and ceiling
	if (viewpoint.sector->PortalBlocksMovement(sector_t::ceiling))
	{
		double theZ = viewpoint.sector->ceilingplane.ZatPoint(viewpoint.Pos) - 4;
		if (viewpoint.Pos.Z > theZ)
		{
			viewpoint.Pos.Z = theZ;
		}
	}

	if (viewpoint.sector->PortalBlocksMovement(sector_t::floor))
	{
		double theZ = viewpoint.sector->floorplane.ZatPoint(viewpoint.Pos) + 4;
		if (viewpoint.Pos.Z < theZ)
		{
			viewpoint.Pos.Z = theZ;
		}
	}

	if (!paused)
	{
		FQuakeJiggers jiggers;

		memset(&jiggers, 0, sizeof(jiggers));
		if (DEarthquake::StaticGetQuakeIntensities(viewpoint.camera, jiggers) > 0)
		{
			double quakefactor = r_quakeintensity;
			DAngle an;

			if (jiggers.RollIntensity != 0 || jiggers.RollWave != 0)
			{
				viewpoint.Angles.Roll += QuakePower(quakefactor, jiggers.RollIntensity, jiggers.RollWave);
			}
			if (jiggers.RelIntensity.X != 0 || jiggers.RelOffset.X != 0)
			{
				an = viewpoint.camera->Angles.Yaw;
				double power = QuakePower(quakefactor, jiggers.RelIntensity.X, jiggers.RelOffset.X);
				viewpoint.Pos += an.ToVector(power);
			}
			if (jiggers.RelIntensity.Y != 0 || jiggers.RelOffset.Y != 0)
			{
				an = viewpoint.camera->Angles.Yaw + 90;
				double power = QuakePower(quakefactor, jiggers.RelIntensity.Y, jiggers.RelOffset.Y);
				viewpoint.Pos += an.ToVector(power);
			}
			// FIXME: Relative Z is not relative
			if (jiggers.RelIntensity.Z != 0 || jiggers.RelOffset.Z != 0)
			{
				viewpoint.Pos.Z += QuakePower(quakefactor, jiggers.RelIntensity.Z, jiggers.RelOffset.Z);
			}
			if (jiggers.Intensity.X != 0 || jiggers.Offset.X != 0)
			{
				viewpoint.Pos.X += QuakePower(quakefactor, jiggers.Intensity.X, jiggers.Offset.X);
			}
			if (jiggers.Intensity.Y != 0 || jiggers.Offset.Y != 0)
			{
				viewpoint.Pos.Y += QuakePower(quakefactor, jiggers.Intensity.Y, jiggers.Offset.Y);
			}
			if (jiggers.Intensity.Z != 0 || jiggers.Offset.Z != 0)
			{
				viewpoint.Pos.Z += QuakePower(quakefactor, jiggers.Intensity.Z, jiggers.Offset.Z);
			}
		}
	}

	viewpoint.extralight = viewpoint.camera->player ? viewpoint.camera->player->extralight : 0;

	// killough 3/20/98, 4/4/98: select colormap based on player status
	// [RH] Can also select a blend
	newblend = 0;

	TArray<lightlist_t> &lightlist = viewpoint.sector->e->XFloor.lightlist;
	if (lightlist.Size() > 0)
	{
		for(unsigned int i = 0; i < lightlist.Size(); i++)
		{
			secplane_t *plane;
			int viewside;
			plane = (i < lightlist.Size()-1) ? &lightlist[i+1].plane : &viewpoint.sector->floorplane;
			viewside = plane->PointOnSide(viewpoint.Pos);
			// Reverse the direction of the test if the plane was downward facing.
			// We want to know if the view is above it, whatever its orientation may be.
			if (plane->fC() < 0)
				viewside = -viewside;
			if (viewside > 0)
			{
				// 3d floor 'fog' is rendered as a blending value
				PalEntry blendv = lightlist[i].blend;

				// If no alpha is set, use 50%
				if (blendv.a==0 && blendv!=0) blendv.a=128;
				newblend = blendv.d;
				break;
			}
		}
	}
	else
	{
		const sector_t *s = viewpoint.sector->GetHeightSec();
		if (s != NULL)
		{
			newblend = s->floorplane.PointOnSide(viewpoint.Pos) < 0
				? s->bottommap
				: s->ceilingplane.PointOnSide(viewpoint.Pos) < 0
				? s->topmap
				: s->midmap;
			if (APART(newblend) == 0 && newblend >= fakecmaps.Size())
				newblend = 0;
		}
	}

	// [RH] Don't override testblend unless entering a sector with a
	//		blend different from the previous sector's. Same goes with
	//		NormalLight's maps pointer.
	if (R_OldBlend != newblend)
	{
		R_OldBlend = newblend;
		if (APART(newblend))
		{
			BaseBlendR = RPART(newblend);
			BaseBlendG = GPART(newblend);
			BaseBlendB = BPART(newblend);
			BaseBlendA = APART(newblend) / 255.f;
		}
		else
		{
			BaseBlendR = BaseBlendG = BaseBlendB = 0;
			BaseBlendA = 0.f;
		}
	}

	validcount++;

	if (r_clearbuffer != 0)
	{
		int color;
		int hom = r_clearbuffer;

		if (hom == 3)
		{
			hom = ((screen->FrameTime / 128) & 1) + 1;
		}
		if (hom == 1)
		{
			color = GPalette.BlackIndex;
		}
		else if (hom == 2)
		{
			color = GPalette.WhiteIndex;
		}
		else if (hom == 4)
		{
			color = (screen->FrameTime / 32) & 255;
		}
		else
		{
			color = pr_hom();
		}
		Renderer->SetClearColor(color);
	}
}


//==========================================================================
//
// FCanvasTextureInfo :: Add
//
// Assigns a camera to a canvas texture.
//
//==========================================================================

void FCanvasTextureInfo::Add (AActor *viewpoint, FTextureID picnum, double fov)
{
	FCanvasTextureInfo *probe;
	FCanvasTexture *texture;

	if (!picnum.isValid())
	{
		return;
	}
	texture = static_cast<FCanvasTexture *>(TexMan[picnum]);
	if (!texture->bHasCanvas)
	{
		Printf ("%s is not a valid target for a camera\n", texture->Name.GetChars());
		return;
	}

	// Is this texture already assigned to a camera?
	for (probe = List; probe != NULL; probe = probe->Next)
	{
		if (probe->Texture == texture)
		{
			// Yes, change its assignment to this new camera
			if (probe->Viewpoint != viewpoint || probe->FOV != fov)
			{
				texture->bFirstUpdate = true;
			}
			probe->Viewpoint = viewpoint;
			probe->FOV = fov;
			return;
		}
	}
	// No, create a new assignment
	probe = new FCanvasTextureInfo;
	probe->Viewpoint = viewpoint;
	probe->Texture = texture;
	probe->PicNum = picnum;
	probe->FOV = fov;
	probe->Next = List;
	texture->bFirstUpdate = true;
	List = probe;
}

// [ZZ] expose this to ZScript
DEFINE_ACTION_FUNCTION(_TexMan, SetCameraToTexture)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(viewpoint, AActor);
	PARAM_STRING(texturename); // [ZZ] there is no point in having this as FTextureID because it's easier to refer to a cameratexture by name and it isn't executed too often to cache it.
	PARAM_FLOAT(fov);
	FTextureID textureid = TexMan.CheckForTexture(texturename, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
	FCanvasTextureInfo::Add(viewpoint, textureid, fov);
	return 0;
}

//==========================================================================
//
// FCanvasTextureInfo :: UpdateAll
//
// Updates all canvas textures that were visible in the last frame.
//
//==========================================================================

void FCanvasTextureInfo::UpdateAll ()
{
	FCanvasTextureInfo *probe;

	for (probe = List; probe != NULL; probe = probe->Next)
	{
		if (probe->Viewpoint != NULL && probe->Texture->bNeedsUpdate)
		{
			Renderer->RenderTextureView(probe->Texture, probe->Viewpoint, probe->FOV);
		}
	}
}

//==========================================================================
//
// FCanvasTextureInfo :: EmptyList
//
// Removes all camera->texture assignments.
//
//==========================================================================

void FCanvasTextureInfo::EmptyList ()
{
	FCanvasTextureInfo *probe, *next;

	for (probe = List; probe != NULL; probe = next)
	{
		next = probe->Next;
		probe->Texture->Unload();
		delete probe;
	}
	List = NULL;
}

//==========================================================================
//
// FCanvasTextureInfo :: Serialize
//
// Reads or writes the current set of mappings in an archive.
//
//==========================================================================

void FCanvasTextureInfo::Serialize(FSerializer &arc)
{
	if (arc.isWriting())
	{
		if (List != nullptr)
		{
			if (arc.BeginArray("canvastextures"))
			{
				FCanvasTextureInfo *probe;

				for (probe = List; probe != nullptr; probe = probe->Next)
				{
					if (probe->Texture != nullptr && probe->Viewpoint != nullptr)
					{
						if (arc.BeginObject(nullptr))
						{
							arc("viewpoint", probe->Viewpoint)
								("fov", probe->FOV)
								("texture", probe->PicNum)
								.EndObject();
						}
					}
				}
				arc.EndArray();
			}
		}
	}
	else
	{
		if (arc.BeginArray("canvastextures"))
		{
			AActor *viewpoint = nullptr;
			double fov;
			FTextureID picnum;
			while (arc.BeginObject(nullptr))
			{
				arc("viewpoint", viewpoint)
					("fov", fov)
					("texture", picnum)
					.EndObject();
				Add(viewpoint, picnum, fov);
			}
			arc.EndArray();
		}
	}
}

//==========================================================================
//
// FCanvasTextureInfo :: Mark
//
// Marks all viewpoints in the list for the collector.
//
//==========================================================================

void FCanvasTextureInfo::Mark()
{
	for (FCanvasTextureInfo *probe = List; probe != NULL; probe = probe->Next)
	{
		GC::Mark(probe->Viewpoint);
	}
}


//==========================================================================
//
// CVAR transsouls
//
// How translucent things drawn with STYLE_SoulTrans are. Normally, only
// Lost Souls have this render style.
// Values less than 0.25 will automatically be set to
// 0.25 to ensure some degree of visibility. Likewise, values above 1.0 will
// be set to 1.0, because anything higher doesn't make sense.
//
//==========================================================================

CUSTOM_CVAR(Float, transsouls, 0.75f, CVAR_ARCHIVE)
{
	if (self < 0.25f)
	{
		self = 0.25f;
	}
	else if (self > 1.f)
	{
		self = 1.f;
	}
}

CUSTOM_CVAR(Float, maxviewpitch, 90.f, CVAR_ARCHIVE | CVAR_SERVERINFO)
{
	if (self>90.f) self = 90.f;
	else if (self<-90.f) self = -90.f;
	if (usergame)
	{
		// [SP] Update pitch limits to the netgame/gamesim.
		players[consoleplayer].SendPitchLimits();
	}
}
