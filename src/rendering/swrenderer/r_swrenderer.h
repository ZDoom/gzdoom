
#pragma once

#include "swrenderer/r_renderer.h"
#include "swrenderer/scene/r_scene.h"

struct FSoftwareRenderer : public FRenderer
{
	FSoftwareRenderer();

	// precache textures
	void Precache(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist) override;

	// render 3D view
	void RenderView(player_t *player, DCanvas *target, void *videobuffer, int bufferpitch) override;

	// renders view to a savegame picture
	void WriteSavePic (player_t *player, FileWriter *file, int width, int height) override;

	// draws player sprites with hardware acceleration (only useful for software rendering)
	void DrawRemainingPlayerSprites() override;

	void SetClearColor(int color) override;
	void RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, double fov);

	void SetColormap(FLevelLocals *Level) override;
	void Init() override;

private:
	void PreparePrecache(FGameTexture *tex, int cache);
	void PrecacheTexture(FGameTexture *tex, int cache);

	swrenderer::RenderScene mScene;
};

FRenderer *CreateSWRenderer();
