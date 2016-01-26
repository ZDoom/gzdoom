#ifndef __GL_BITMAP_H
#define __GL_BITMAP_H

#include "textures/bitmap.h"
#include "gl/textures/gl_material.h"


class FGLBitmap : public FBitmap
{
	int translation;
public:

	FGLBitmap()
	{
		translation = 0;
	}
	FGLBitmap(BYTE *buffer, int pitch, int width, int height)
		: FBitmap(buffer, pitch, width, height)
	{
		translation = 0;
	}

	void SetTranslationInfo(int _trans)
	{
		if (_trans != -1337) translation = _trans;

	}

	virtual void CopyPixelDataRGB(int originx, int originy, const BYTE *patch, int srcwidth, 
								int srcheight, int step_x, int step_y, int rotate, int ct, FCopyInfo *inf = NULL,
		/* for PNG tRNS */		int r=0, int g=0, int b=0);

	virtual void CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
								int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf = NULL);
};

#endif
