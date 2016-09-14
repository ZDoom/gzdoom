#ifndef __GL_TEXTURE_H__
#define __GL_TEXTURE_H__

#include "r_defs.h"

class FBrightmapTexture : public FTexture
{
public:
	FBrightmapTexture (FTexture *source);
	~FBrightmapTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf);
	bool UseBasePalette() { return false; }

protected:
	FTexture *SourcePic;
	//BYTE *Pixels;
	//Span **Spans;
};


void gl_GenerateGlobalBrightmapFromColormap();
PalEntry averageColor(const DWORD *data, int size, int maxout);



unsigned char *gl_CreateUpsampledTextureBuffer ( const FTexture *inputTexture, unsigned char *inputBuffer, const int inWidth, const int inHeight, int &outWidth, int &outHeight, bool hasAlpha );
int CheckDDPK3(FTexture *tex);
int CheckExternalFile(FTexture *tex, bool & hascolorkey);

#endif	// __GL_HQRESIZE_H__

