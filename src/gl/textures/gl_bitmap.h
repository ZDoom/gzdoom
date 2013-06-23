#ifndef __GL_BITMAP_H
#define __GL_BITMAP_H

#include "textures/bitmap.h"
#include "gl/textures/gl_material.h"


void ModifyPalette(PalEntry * pout, PalEntry * pin, int cm, int count);

class FGLBitmap : public FBitmap
{
	int cm;
	int translation;
public:

	FGLBitmap() { cm = CM_DEFAULT; translation = 0; }
	FGLBitmap(BYTE *buffer, int pitch, int width, int height)
		: FBitmap(buffer, pitch, width, height)
	{ cm = CM_DEFAULT; translation = 0; }

	void SetTranslationInfo(int _cm, int _trans=-1337)
	{
		if (_cm != -1) cm = _cm;
		if (_trans != -1337) translation = _trans;

	}

	virtual void CopyPixelDataRGB(int originx, int originy, const BYTE *patch, int srcwidth, 
								int srcheight, int step_x, int step_y, int rotate, int ct, FCopyInfo *inf = NULL);
	virtual void CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
								int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf = NULL);
};

#endif
