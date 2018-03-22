#ifndef __GL_TEXTURE_H__
#define __GL_TEXTURE_H__

#include "r_defs.h"
#include "textures/textures.h"

class FBrightmapTexture : public FWorldTexture
{
public:
	FBrightmapTexture (FTexture *source);

	uint8_t *MakeTexture(FRenderStyle style) override;
	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf) override;
	bool UseBasePalette() override { return false; }

protected:
	FTexture *SourcePic;
};


void gl_GenerateGlobalBrightmapFromColormap();



unsigned char *gl_CreateUpsampledTextureBuffer ( const FTexture *inputTexture, unsigned char *inputBuffer, const int inWidth, const int inHeight, int &outWidth, int &outHeight, bool hasAlpha );
int CheckDDPK3(FTexture *tex);
int CheckExternalFile(FTexture *tex, bool & hascolorkey);

#endif	// __GL_HQRESIZE_H__

