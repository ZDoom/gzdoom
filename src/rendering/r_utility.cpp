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
#include "c_dispatch.h"
#include "v_video.h"
#include "stats.h"
#include "i_video.h"
#include "a_sharedglobal.h"
#include "p_3dmidtex.h"
#include "r_data/r_interpolate.h"
#include "po_man.h"
#include "p_effect.h"
#include "st_start.h"
#include "v_font.h"
#include "swrenderer/r_renderer.h"
#include "serializer.h"
#include "r_utility.h"
#include "d_player.h"
#include "p_local.h"
#include "g_levellocals.h"
#include "p_maputl.h"
#include "sbar.h"
#include "vm.h"
#include "i_time.h"
#include "actorinlines.h"
#include "g_game.h"
#include "i_system.h"
#include "v_draw.h"

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

CUSTOM_CVARD(Int, r_actorspriteshadow, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "render actor sprite shadows. 0 = off, 1 = default, 2 = always on")
{
	if (self < 0)
		self = 0;
	else if (self > 2)
		self = 2;
}
CUSTOM_CVARD(Float, r_actorspriteshadowdist, 1500.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG, "how far sprite shadows should be rendered")
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 8192.f)
		self = 8192.f;
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

unsigned int	R_OldBlend = ~0;
int 			validcount = 1; 	// increment every time a check is made
int 			dl_validcount = 1; 	// increment every time a check is made
int			freelookviewheight;

DVector3a view;
DAngle viewpitch;

DEFINE_GLOBAL(LocalViewPitch);

// CODE --------------------------------------------------------------------

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
		DrawFSHUD = (windowSize == 11);
	}

	
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
//
//
//==========================================================================

FRenderer *CreateSWRenderer();
FRenderer* SWRenderer;

//==========================================================================
//
// R_Init
//
//==========================================================================

void R_Init ()
{
	StartScreen->Progress();
	R_InitTranslationTables ();
	R_SetViewSize (screenblocks);

	if (SWRenderer == NULL)
	{
		SWRenderer = CreateSWRenderer();
	}

	SWRenderer->Init();
}

//==========================================================================
//
// R_Shutdown
//
//==========================================================================

void R_Shutdown ()
{
	if (SWRenderer != nullptr) delete SWRenderer;
	SWRenderer = nullptr;
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
	auto Level = viewpoint.ViewLevel;
	int oldgroup = Level->PointInRenderSubsector(iview->Old.Pos)->sector->PortalGroup;
	int newgroup = Level->PointInRenderSubsector(iview->New.Pos)->sector->PortalGroup;

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
			DVector2 disp = viewpoint.ViewLevel->Displacements.getOffset(oldgroup, newgroup);
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
	viewpoint.sector = Level->PointInRenderSubsector(viewpoint.Pos)->sector;
	bool moved = false;
	while (!viewpoint.sector->PortalBlocksMovement(sector_t::ceiling))
	{
		if (viewpoint.Pos.Z > viewpoint.sector->GetPortalPlaneZ(sector_t::ceiling))
		{
			viewpoint.Pos += viewpoint.sector->GetPortalDisplacement(sector_t::ceiling);
			viewpoint.ActorPos += viewpoint.sector->GetPortalDisplacement(sector_t::ceiling);
			viewpoint.sector = Level->PointInRenderSubsector(viewpoint.Pos)->sector;
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
				viewpoint.sector = Level->PointInRenderSubsector(viewpoint.Pos)->sector;
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
// sets all values derived from the view angle.
//
//==========================================================================

void FRenderViewpoint::SetViewAngle (const FViewWindow &viewwindow)
{
	Sin = Angles.Yaw.Sin();
	Cos = Angles.Yaw.Cos();

	TanSin = viewwindow.FocalTangent * Sin;
	TanCos = viewwindow.FocalTangent * Cos;

	DVector2 v = Angles.Yaw.ToVector();
	ViewVector.X = v.X;
	ViewVector.Y = v.Y;
	HWAngles.Yaw = float(270.0 - Angles.Yaw.Degrees);

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
	viewpoint.ViewLevel = actor->Level;

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
		sector_t *oldsector = viewpoint.ViewLevel->PointInRenderSubsector(iview->Old.Pos)->sector;
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

	// [MC] Apply the view angles first, which is the offsets. If the absolute isn't desired,
	// add the standard angles on top of it.
	viewpoint.Angles = viewpoint.camera->ViewAngles;

	if (!(viewpoint.camera->flags8 & MF8_ABSVIEWANGLES))
	{
		viewpoint.Angles += viewpoint.camera->Angles;
	}

	iview->New.Angles = viewpoint.Angles;
	if (viewpoint.camera->player != 0)
	{
		player = viewpoint.camera->player;
	}

	if (iview->otic == -1 || r_NoInterpolate || (viewpoint.camera->renderflags & RF_NOINTERPOLATEVIEW))
	{
		viewpoint.camera->renderflags &= ~RF_NOINTERPOLATEVIEW;
		R_ResetViewInterpolation ();
		iview->otic = nowtic;
	}

	viewpoint.TicFrac = I_GetTimeFrac ();
	if (cl_capfps || r_NoInterpolate)
	{
		viewpoint.TicFrac = 1.;
	}
	R_InterpolateView (viewpoint, player, viewpoint.TicFrac, iview);

	viewpoint.SetViewAngle (viewwindow);

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
		if (DEarthquake::StaticGetQuakeIntensities(viewpoint.TicFrac, viewpoint.camera, jiggers) > 0)
		{
			double quakefactor = r_quakeintensity;
			DVector3 pos; pos.Zero();
			if (jiggers.RollIntensity != 0 || jiggers.RollWave != 0)
			{
				viewpoint.Angles.Roll += QuakePower(quakefactor, jiggers.RollIntensity, jiggers.RollWave);
			}
			if (jiggers.RelIntensity.X != 0 || jiggers.RelOffset.X != 0)
			{
				pos.X += QuakePower(quakefactor, jiggers.RelIntensity.X, jiggers.RelOffset.X);
			}
			if (jiggers.RelIntensity.Y != 0 || jiggers.RelOffset.Y != 0)
			{
				pos.Y += QuakePower(quakefactor, jiggers.RelIntensity.Y, jiggers.RelOffset.Y);
			}
			if (jiggers.RelIntensity.Z != 0 || jiggers.RelOffset.Z != 0)
			{
				pos.Z += QuakePower(quakefactor, jiggers.RelIntensity.Z, jiggers.RelOffset.Z);
			}
			// [MC] Tremendous thanks to Marisa Kirisame for helping me with this.
			// Use a rotation matrix to make the view relative.
			if (!pos.isZero())
			{
				DAngle yaw = viewpoint.camera->Angles.Yaw;
				DAngle pitch = viewpoint.camera->Angles.Pitch;
				DAngle roll = viewpoint.camera->Angles.Roll;
				DVector3 relx, rely, relz;
				DMatrix3x3 rot =
					DMatrix3x3(DVector3(0., 0., 1.), yaw.Cos(), yaw.Sin()) *
					DMatrix3x3(DVector3(0., 1., 0.), pitch.Cos(), pitch.Sin()) *
					DMatrix3x3(DVector3(1., 0., 0.), roll.Cos(), roll.Sin());
				relx = DVector3(1., 0., 0.)*rot;
				rely = DVector3(0., 1., 0.)*rot;
				relz = DVector3(0., 0., 1.)*rot;
				viewpoint.Pos += relx * pos.X + rely * pos.Y + relz * pos.Z;
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
		screen->SetClearColor(color);
		SWRenderer->SetClearColor(color);
	}
    else
	{
		screen->SetClearColor(GPalette.BlackIndex);
    }
	
	
	// And finally some info that is needed for the hardware renderer
	
	// Scale the pitch to account for the pixel stretching, because the playsim doesn't know about this and treats it as 1:1.
	// However, to set up a projection matrix this needs to be adjusted.
	double radPitch = viewpoint.Angles.Pitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * actor->Level->info->pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);
	viewpoint.HWAngles.Pitch = RAD2DEG((float)asin(angy / alen));
	
	viewpoint.HWAngles.Roll.Degrees = (float)viewpoint.Angles.Roll.Degrees;    // copied for convenience.
	
	// ViewActor only gets set, if the camera actor should not be rendered
	if (actor->player && actor->player - players == consoleplayer &&
		((actor->player->cheats & CF_CHASECAM) || (r_deathcamera && actor->health <= 0)) && actor == actor->player->mo)
	{
		viewpoint.ViewActor = nullptr;
	}
	else
	{
		viewpoint.ViewActor = actor;
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

//==========================================================================
//
// R_ShouldDrawSpriteShadow
//
//==========================================================================

bool R_ShouldDrawSpriteShadow(AActor *thing)
{
	switch (r_actorspriteshadow)
	{
	case 1:
		return (thing->renderflags & RF_CASTSPRITESHADOW);

	case 2:
		if (thing->renderflags & RF_CASTSPRITESHADOW)
		{
			return true;
		}
		return (thing->renderflags & RF_CASTSPRITESHADOW) || (!(thing->renderflags & RF_NOSPRITESHADOW) && ((thing->flags3 & MF3_ISMONSTER) || thing->player != nullptr));

	default:
	case 0:
		return false;
	}
}
