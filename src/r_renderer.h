#ifndef __R_RENDERER_H
#define __R_RENDERER_H

#include <stdio.h>

struct FRenderer;
extern FRenderer *SWRenderer;

class FSerializer;
class FTexture;
class AActor;
class player_t;
struct sector_t;
class FCanvasTexture;
class FileWriter;
class DCanvas;
struct FLevelLocals;

struct FRenderer
{
	virtual ~FRenderer() {}

	// precache one texture
	virtual void Precache(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist) = 0;

	// render 3D view
	virtual void RenderView(player_t *player, DCanvas *target, void *videobuffer) = 0;

	// renders view to a savegame picture
	virtual void WriteSavePic(player_t *player, FileWriter *file, int width, int height) = 0;

	// draws player sprites with hardware acceleration (only useful for software rendering)
	virtual void DrawRemainingPlayerSprites() = 0;

	// set up the colormap for a newly loaded level.
	virtual void SetColormap(FLevelLocals *) = 0;

	virtual void SetClearColor(int color) = 0;

	virtual void Init() = 0;

};


#endif
