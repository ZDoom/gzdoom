#pragma once

#include "textures.h"
//-----------------------------------------------------------------------------
//
// This is not a real texture but will be added to the texture manager
// so that it can be handled like any other sky.
//
//-----------------------------------------------------------------------------

class FSkyBox : public FTexture
{
public:

	FTexture * faces[6];
	bool fliptop;

	FSkyBox(const char *name = nullptr);
	~FSkyBox();
	const uint8_t *GetColumn(FRenderStyle style, unsigned int column, const FSoftwareTextureSpan **spans_out);
	const uint8_t *GetPixels (FRenderStyle style);
	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf);
	bool UseBasePalette();
	void Unload ();

	void SetSize()
	{
		if (faces[0]) 
		{
			CopySize(faces[0]);
		}
	}

	bool Is3Face() const
	{
		return faces[5]==NULL;
	}

	bool IsFlipped() const
	{
		return fliptop;
	}
};
