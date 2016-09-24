#ifndef __R_SWRENDERER_H
#define __R_SWRENDERER_H

#include "r_renderer.h"

struct FSoftwareRenderer : public FRenderer
{
	// Can be overridden so that the colormaps for sector color/fade won't be built.
	virtual bool UsesColormap() const override;

	// precache one texture
	void PrecacheTexture(FTexture *tex, int cache);
	virtual void Precache(BYTE *texhitlist, TMap<PClassActor*, bool> &actorhitlist) override;

	// render 3D view
	virtual void RenderView(player_t *player) override;

	// Remap voxel palette
	virtual void RemapVoxels() override;

	// renders view to a savegame picture
	virtual void WriteSavePic (player_t *player, FileWriter *file, int width, int height) override;

	// draws player sprites with hardware acceleration (only useful for software rendering)
	virtual void DrawRemainingPlayerSprites() override;

	virtual int GetMaxViewPitch(bool down) override;

	void OnModeSet () override;
	void ErrorCleanup () override;
	void ClearBuffer(int color) override;
	void Init() override;
	void SetWindow (int windowSize, int fullWidth, int fullHeight, int stHeight, float trueratio) override;
	void SetupFrame(player_t *player) override;
	void CopyStackedViewParameters() override;
	void RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, int fov) override;
	sector_t *FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, bool back) override;

};


#endif
