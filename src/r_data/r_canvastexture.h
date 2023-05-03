#pragma once

class FCanvas;
class FCanvasTexture;

// This list keeps track of the cameras that draw into canvas textures.
struct FCanvasTextureEntry
{
	TObjPtr<AActor*> Viewpoint;
	FCanvasTexture *Texture;
	FTextureID PicNum;
	double FOV;
};


struct FCanvasTextureInfo
{
	TArray<FCanvasTextureEntry> List;
	
	void Add (AActor *viewpoint, FTextureID picnum, double fov);
	void UpdateAll (std::function<void(AActor *, FCanvasTexture *, double fov)> callback);
	void EmptyList ();
	void Serialize(FSerializer &arc);
	void Mark();
	
};
