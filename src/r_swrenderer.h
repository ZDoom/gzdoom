#ifndef __R_SWRENDERER_H
#define __R_SWRENDERER_H

#include "r_renderer.h"

struct FSoftwareRenderer : public FRenderer
{
	// Can be overridden so that the colormaps for sector color/fade won't be built.
	virtual bool UsesColormap() const;

	// precache one texture
	virtual void PrecacheTexture(FTexture *tex, int cache);

	// render 3D view
	virtual void RenderView(player_t *player);

	// Remap voxel palette
	virtual void RemapVoxels();

	// renders view to a savegame picture
	virtual void WriteSavePic (player_t *player, FILE *file, int width, int height);

	// draws player sprites with hardware acceleration (only useful for software rendering)
	virtual void DrawRemainingPlayerSprites();

	virtual int GetMaxViewPitch(bool down);

	void OnModeSet ();
	void ErrorCleanup ();
	void ClearBuffer(int color);
	void Init();
	void SetWindow (int windowSize, int fullWidth, int fullHeight, int stHeight, int trueratio);
	void SetupFrame(player_t *player);
	void CopyStackedViewParameters();
	void RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, int fov);
	sector_t *FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, bool back);

};


#endif
