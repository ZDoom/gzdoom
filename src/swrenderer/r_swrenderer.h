
#pragma once

#include "r_renderer.h"
#include "swrenderer/scene/r_scene.h"

struct FSoftwareRenderer : public FRenderer
{
	FSoftwareRenderer();
	~FSoftwareRenderer();

	// Can be overridden so that the colormaps for sector color/fade won't be built.
	bool UsesColormap() const override;

	// precache textures
	void Precache(BYTE *texhitlist, TMap<PClassActor*, bool> &actorhitlist) override;

	// render 3D view
	void RenderView(player_t *player) override;

	// Remap voxel palette
	void RemapVoxels() override;

	// renders view to a savegame picture
	void WriteSavePic (player_t *player, FileWriter *file, int width, int height) override;

	// draws player sprites with hardware acceleration (only useful for software rendering)
	void DrawRemainingPlayerSprites() override;

	int GetMaxViewPitch(bool down) override;
	bool RequireGLNodes() override;

	void OnModeSet() override;
	void SetClearColor(int color) override;
	void Init() override;
	void RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, int fov) override;
	sector_t *FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel) override;

	void StateChanged(AActor *actor) override;
	void PreprocessLevel() override;
	void CleanLevelData() override;

private:
	void PrecacheTexture(FTexture *tex, int cache);

	swrenderer::RenderScene mScene;
};
