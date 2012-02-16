#ifndef __R_RENDERER_H
#define __R_RENDERER_H

#include <stdio.h>

struct FRenderer;
extern FRenderer *Renderer;
class FArchive;
class FTexture;
class AActor;
class player_t;
struct sector_t;
class FCanvasTexture;

struct FRenderer
{
	FRenderer()
	{
		Renderer = this;
	}

	virtual ~FRenderer()
	{
		Renderer = NULL;
	}

	// Can be overridden so that the colormaps for sector color/fade won't be built.
	virtual bool UsesColormap() const = 0;

	// precache one texture
	virtual void PrecacheTexture(FTexture *tex, int cache) = 0;

	// render 3D view
	virtual void RenderView(player_t *player) = 0;

	// Remap voxel palette
	virtual void RemapVoxels() {}

	// renders view to a savegame picture
	virtual void WriteSavePic (player_t *player, FILE *file, int width, int height) = 0;

	// draws player sprites with hardware acceleration (only useful for software rendering)
	virtual void DrawRemainingPlayerSprites() {}

	// notifies the renderer that an actor has changed state.
	virtual void StateChanged(AActor *actor) {}

	// notify the renderer that serialization of the curent level is about to start/end
	virtual void StartSerialize(FArchive &arc) {}
	virtual void EndSerialize(FArchive &arc) {}

	virtual int GetMaxViewPitch(bool down) = 0;	// return value is in plain degrees

	virtual void OnModeSet () {}
	virtual void ErrorCleanup () {}
	virtual void ClearBuffer(int color) = 0;
	virtual void Init() = 0;
	virtual void SetWindow (int windowSize, int fullWidth, int fullHeight, int stHeight, int trueratio) {}
	virtual void SetupFrame(player_t *player) {}
	virtual void CopyStackedViewParameters() {}
	virtual void RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, int fov) = 0;
	virtual sector_t *FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, bool back) = 0;
	virtual void SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog) {}
	virtual void PreprocessLevel() {}
	virtual void CleanLevelData() {}
	virtual bool RequireGLNodes() { return false; }


};


#endif
