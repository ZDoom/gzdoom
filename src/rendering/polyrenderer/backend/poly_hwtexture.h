#pragma once

#ifdef LoadImage
#undef LoadImage
#endif

#define SHADED_TEXTURE -1
#define DIRECT_PALETTE -2

#include "tarray.h"
#include "hwrenderer/textures/hw_ihwtexture.h"
#include "volk/volk.h"

struct FMaterialState;
class PolyBuffer;

class PolyHardwareTexture : public IHardwareTexture
{
public:
	PolyHardwareTexture();
	~PolyHardwareTexture();

	static void ResetAll();
	void Reset();

	void Precache(FMaterial *mat, int translation, int flags);

	DCanvas *GetImage(const FMaterialState &state);
	DCanvas *GetImage(FTexture *tex, int translation, int flags);
	PolyDepthStencil *GetDepthStencil(FTexture *tex);

	// Software renderer stuff
	void AllocateBuffer(int w, int h, int texelsize) override;
	uint8_t *MapBuffer() override;
	unsigned int CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const char *name) override;

	// Wipe screen
	void CreateWipeTexture(int w, int h, const char *name);

private:
	void CreateImage(FTexture *tex, int translation, int flags);

	static PolyHardwareTexture *First;
	PolyHardwareTexture *Prev = nullptr;
	PolyHardwareTexture *Next = nullptr;
	std::unique_ptr<DCanvas> mCanvas;
	std::unique_ptr<PolyDepthStencil> mDepthStencil;
};
