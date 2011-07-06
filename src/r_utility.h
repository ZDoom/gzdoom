#ifndef __R_UTIL_H
#define __R_UTIL_H

#include "r_state.h"
//
// Stuff from r_main.h that's needed outside the rendering code.

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS			32

extern fixed_t			viewx;
extern fixed_t			viewy;

extern fixed_t			viewtancos;
extern fixed_t			viewtansin;
extern fixed_t			FocalTangent;

extern bool				r_NoInterpolate;
extern int				validcount;

extern angle_t			LocalViewAngle;			// [RH] Added to consoleplayer's angle
extern int				LocalViewPitch;			// [RH] Used directly instead of consoleplayer's pitch
extern bool				LocalKeyboardTurner;	// [RH] The local player used the keyboard to turn, so interpolate
extern int				WidescreenRatio;

extern fixed_t			r_TicFrac;
extern DWORD			r_FrameTime;

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

angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
inline angle_t R_PointToAngle (fixed_t x, fixed_t y) { return R_PointToAngle2 (viewx, viewy, x, y); }
subsector_t *R_PointInSubsector (fixed_t x, fixed_t y);
fixed_t R_PointToDist2 (fixed_t dx, fixed_t dy);
void R_ResetViewInterpolation ();
void R_SetViewSize (int blocks);

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
