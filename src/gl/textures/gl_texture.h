/*
** gl_hqresize.h
** Contains high quality upsampling functions.
**
**---------------------------------------------------------------------------
** Copyright 2008 Benjamin Berkels
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __GL_HQRESIZE_H__
#define __GL_HQRESIZE_H__

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

