#ifndef __R_UTIL_H
#define __R_UTIL_H

#include "r_state.h"
#include "vectors.h"

class FSerializer;
struct FViewWindow;
//
// Stuff from r_main.h that's needed outside the rendering code.

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS			32

struct FRenderViewpoint
{
	FRenderViewpoint();

	player_t		*player;		// For which player is this viewpoint being renderered? (can be null for camera textures)
	DVector3		Pos;			// Camera position
	DVector3		ActorPos;		// Camera actor's position
	DRotator		Angles;			// Camera angles
	FRotator		HWAngles;		// Actual rotation angles for the hardware renderer
	DVector2		ViewVector;		// HWR only: direction the camera is facing.
	AActor			*ViewActor;		// either the same as camera or nullptr

	DVector3		Path[2];		// View path for portal calculations
	double			Cos;			// cos(Angles.Yaw)
	double			Sin;			// sin(Angles.Yaw)
	double			TanCos;			// FocalTangent * cos(Angles.Yaw)
	double			TanSin;			// FocalTangent * sin(Angles.Yaw)

	AActor			*camera;		// camera actor
	sector_t		*sector;		// [RH] keep track of sector viewing from
	DAngle			FieldOfView;	// current field of view

	double			TicFrac;		// fraction of tic for interpolation
	uint32_t		FrameTime;		// current frame's time in tics.
	
	int				extralight;		// extralight to be added to this viewpoint
	bool			showviewer;		// show the camera actor?


	void SetViewAngle(const FViewWindow &viewwindow);

};

extern FRenderViewpoint r_viewpoint;

//-----------------------------------
struct FViewWindow
{
	double FocalTangent = 0.0;
	int centerx = 0;
	int centerxwide = 0;
	int centery = 0;
	float WidescreenRatio = 0.0f;
};

extern FViewWindow r_viewwindow;

//-----------------------------------


extern int				setblocks;
extern bool				r_NoInterpolate;
extern int				validcount;

extern angle_t			LocalViewAngle;			// [RH] Added to consoleplayer's angle
extern int				LocalViewPitch;			// [RH] Used directly instead of consoleplayer's pitch
extern bool				LocalKeyboardTurner;	// [RH] The local player used the keyboard to turn, so interpolate

extern unsigned int		R_OldBlend;

const double			r_Yaspect = 200.0;		// Why did I make this a variable? It's never set anywhere.


//==========================================================================
//
// R_PointOnSide
//
// Traverse BSP (sub) tree, check point against partition plane.
// Returns side 0 (front/on) or 1 (back).
//
// [RH] inlined, stripped down, and made more precise
//
//==========================================================================

inline int R_PointOnSide (fixed_t x, fixed_t y, const node_t *node)
{
	return DMulScale32 (y-node->y, node->dx, node->x-x, node->dy) > 0;
}
inline int R_PointOnSide(double x, double y, const node_t *node)
{
	return DMulScale32(FLOAT2FIXED(y) - node->y, node->dx, node->x - FLOAT2FIXED(x), node->dy) > 0;
}
inline int R_PointOnSide(const DVector2 &pos, const node_t *node)
{
	return DMulScale32(FLOAT2FIXED(pos.Y) - node->y, node->dx, node->x - FLOAT2FIXED(pos.X), node->dy) > 0;
}

// Used for interpolation waypoints.
struct DVector3a
{
	DVector3 pos;
	DAngle angle;
};


subsector_t *R_PointInSubsector (fixed_t x, fixed_t y);
inline subsector_t *R_PointInSubsector(const DVector2 &pos)
{
	return R_PointInSubsector(FLOAT2FIXED(pos.X), FLOAT2FIXED(pos.Y));
}
void R_ResetViewInterpolation ();
void R_RebuildViewInterpolation(player_t *player);
bool R_GetViewInterpolationStatus();
void R_ClearInterpolationPath();
void R_AddInterpolationPoint(const DVector3a &vec);
void R_SetViewSize (int blocks);
void R_SetFOV (FRenderViewpoint &viewpoint, DAngle fov);
void R_SetupFrame (FRenderViewpoint &viewpoint, FViewWindow &viewwindow, AActor * camera);
void R_SetViewAngle (FRenderViewpoint &viewpoint, const FViewWindow &viewwindow);

// Called by startup code.
void R_Init (void);
void R_ExecuteSetViewSize (FRenderViewpoint &viewpoint, FViewWindow &viewwindow);

// Called by M_Responder.
void R_SetViewSize (int blocks);
void R_SetWindow (FRenderViewpoint &viewpoint, FViewWindow &viewwindow, int windowSize, int fullWidth, int fullHeight, int stHeight, bool renderingToCanvas = false);

double R_GetGlobVis(const FViewWindow &viewwindow, double vis);
double R_ClampVisibility(double vis);

extern void R_FreePastViewers ();
extern void R_ClearPastViewer (AActor *actor);

class FCanvasTexture;
// This list keeps track of the cameras that draw into canvas textures.
struct FCanvasTextureInfo
{
	FCanvasTextureInfo *Next;
	TObjPtr<AActor*> Viewpoint;
	FCanvasTexture *Texture;
	FTextureID PicNum;
	double FOV;

	static void Add (AActor *viewpoint, FTextureID picnum, double fov);
	static void UpdateAll ();
	static void EmptyList ();
	static void Serialize(FSerializer &arc);
	static void Mark();

private:
	static FCanvasTextureInfo *List;
};


#endif
