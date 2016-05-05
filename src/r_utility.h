#ifndef __R_UTIL_H
#define __R_UTIL_H

#include "r_state.h"
#include "vectors.h"
//
// Stuff from r_main.h that's needed outside the rendering code.

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS			32

extern DCanvas			*RenderTarget;

extern DVector3			ViewPos;
extern DAngle			ViewAngle;
extern DAngle			ViewPitch;
extern DAngle			ViewRoll;
extern DVector3			ViewPath[2];

extern "C" int			centerx, centerxwide;
extern "C" int			centery;

extern int				setblocks;

extern double			ViewTanCos;
extern double			ViewTanSin;
extern double			FocalTangent;

extern bool				r_NoInterpolate;
extern int				validcount;

extern angle_t			LocalViewAngle;			// [RH] Added to consoleplayer's angle
extern int				LocalViewPitch;			// [RH] Used directly instead of consoleplayer's pitch
extern bool				LocalKeyboardTurner;	// [RH] The local player used the keyboard to turn, so interpolate
extern int				WidescreenRatio;

extern double			r_TicFracF;
extern DWORD			r_FrameTime;
extern int				extralight;
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
void R_SetFOV (DAngle fov);
void R_SetupFrame (AActor * camera);
void R_SetViewAngle ();

// Called by startup code.
void R_Init (void);
void R_ExecuteSetViewSize (void);

// Called by M_Responder.
void R_SetViewSize (int blocks);
void R_SetWindow (int windowSize, int fullWidth, int fullHeight, int stHeight);


extern void R_FreePastViewers ();
extern void R_ClearPastViewer (AActor *actor);

// This list keeps track of the cameras that draw into canvas textures.
struct FCanvasTextureInfo
{
	FCanvasTextureInfo *Next;
	TObjPtr<AActor> Viewpoint;
	FCanvasTexture *Texture;
	FTextureID PicNum;
	int FOV;

	static void Add (AActor *viewpoint, FTextureID picnum, int fov);
	static void UpdateAll ();
	static void EmptyList ();
	static void Serialize (FArchive &arc);
	static void Mark();

private:
	static FCanvasTextureInfo *List;
};


#endif
