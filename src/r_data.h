// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	Refresh module, data I/O, caching, retrieval of graphics
//	by name.
//
//-----------------------------------------------------------------------------


#ifndef __R_DATA__
#define __R_DATA__

#include "r_defs.h"
#include "r_state.h"
#include "v_video.h"

class FWadLump;


// A texture that doesn't really exist
class FDummyTexture : public FTexture
{
public:
	FDummyTexture ();
	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	void SetSize (int width, int height);
};

// A texture that returns a wiggly version of another texture.
class FWarpTexture : public FTexture
{
public:
	FWarpTexture (FTexture *source);
	~FWarpTexture ();

	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int w=-1, int h=-1, int rotate=0, FCopyInfo *inf = NULL);
	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	bool CheckModified ();

	float GetSpeed() const { return Speed; }
	int GetSourceLump() { return SourcePic->GetSourceLump(); }
	void SetSpeed(float fac) { Speed = fac; }
	FTexture *GetRedirect(bool wantwarped);

	DWORD GenTime;
protected:
	FTexture *SourcePic;
	BYTE *Pixels;
	Span **Spans;
	float Speed;

	virtual void MakeTexture (DWORD time);
};

// [GRB] Eternity-like warping
class FWarp2Texture : public FWarpTexture
{
public:
	FWarp2Texture (FTexture *source);

protected:
	void MakeTexture (DWORD time);
};

// A texture that can be drawn to.
class DSimpleCanvas;
class FCanvasTexture : public FTexture
{
public:
	FCanvasTexture (const char *name, int width, int height);
	~FCanvasTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	bool CheckModified ();
	void RenderView (AActor *viewpoint, int fov);
	void RenderGLView(AActor *viewpoint, int fov);
	void NeedUpdate() { bNeedsUpdate=true; }

protected:
	DSimpleCanvas *Canvas;
	BYTE *Pixels;
	Span DummySpans[2];
	BYTE bNeedsUpdate:1;
	BYTE bDidUpdate:1;
	BYTE bFirstUpdate:1;

	void MakeTexture ();

	friend struct FCanvasTextureInfo;
};

// This list keeps track of the cameras that draw into canvas textures.
struct FCanvasTextureInfo
{
	FCanvasTextureInfo *Next;
	TObjPtr<AActor> Viewpoint;
	FCanvasTexture *Texture;
	FTextureID PicNum;
	int FOV;

	static void Add (AActor *viewpoint, FTextureID picnum, int fov);
	static void UpdateAll ();
	static void EmptyList ();
	static void Serialize (FArchive &arc);
	static void Mark();

private:
	static FCanvasTextureInfo *List;
};

// I/O, setting up the stuff.
void R_InitData (void);
void R_DeinitData ();
void R_PrecacheLevel (void);


// Retrieval.


DWORD R_ColormapNumForName(const char *name);	// killough 4/4/98
void R_SetDefaultColormap (const char *name);	// [RH] change normal fadetable
DWORD R_BlendForColormap (DWORD map);		// [RH] return calculated blend for a colormap
extern BYTE *realcolormaps;						// [RH] make the colormaps externally visible
extern size_t numfakecmaps;

int R_FindSkin (const char *name, int pclass);	// [RH] Find a skin

#endif
