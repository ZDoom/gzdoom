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


// A texture that is just a single patch
class FPatchTexture : public FTexture
{
public:
	~FPatchTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	int SourceLump;
	BYTE *Pixels;
	Span **Spans;

	static bool Check(FileReader & file);
	static FTexture *Create(FileReader & file, int lumpnum);
	FPatchTexture (int lumpnum, patch_t *header);

	virtual void MakeTexture ();
	void HackHack (int newheight);

	friend class FTexture;
	friend class FMultiPatchTexture;
};

// In-memory representation of a single PNAMES lump entry
struct FPatchLookup
{
	char Name[9];
	FTexture *Texture;
};

// A texture defined in a TEXTURE1 or TEXTURE2 lump
class FMultiPatchTexture : public FTexture
{
public:
	FMultiPatchTexture (const void *texdef, FPatchLookup *patchlookup, int maxpatchnum, bool strife);
	~FMultiPatchTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	virtual void SetFrontSkyLayer ();

protected:
	BYTE *Pixels;
	Span **Spans;

	struct TexPart
	{
		SWORD OriginX, OriginY;
		FTexture *Texture;
	};

	int NumParts;
	TexPart *Parts;
	bool bRedirect;

	void MakeTexture ();

private:
	void CheckForHacks ();
};

// A texture defined between F_START and F_END markers
class FFlatTexture : public FTexture
{
public:
	~FFlatTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	int SourceLump;
	BYTE *Pixels;
	Span DummySpans[2];

	static bool Check(FileReader & file);
	static FTexture *Create(FileReader & file, int lumpnum);
	FFlatTexture (int lumpnum);

	void MakeTexture ();

	friend class FTexture;
};

// A texture defined in a Build TILESxxx.ART file
class FBuildTexture : public FTexture
{
public:
	FBuildTexture (int tilenum, const BYTE *pixels, int width, int height, int left, int top);
	~FBuildTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	const BYTE *Pixels;
	Span **Spans;
};


// A raw 320x200 graphic used by Heretic and Hexen fullscreen images
class FRawPageTexture : public FTexture
{
public:
	~FRawPageTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	static bool Check(FileReader & file);
	static FTexture *Create(FileReader & file, int lumpnum);
	FRawPageTexture (int lumpnum);

	int SourceLump;
	BYTE *Pixels;
	static const Span DummySpans[2];

	void MakeTexture ();

	friend class FTexture;
};

// A raw 320x? graphic used by Heretic and Hexen for the automap parchment
class FAutomapTexture : public FTexture
{
public:
	~FAutomapTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	void MakeTexture ();

private:

	static bool Check(FileReader & file);
	static FTexture *Create(FileReader & file, int lumpnum);
	FAutomapTexture (int lumpnum);


	BYTE *Pixels;
	Span DummySpan[2];
	int LumpNum;

	friend class FTexture;
};



// An IMGZ image (mostly just crosshairs)
class FIMGZTexture : public FTexture
{
public:
	~FIMGZTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:

	static bool Check(FileReader & file);
	static FTexture *Create(FileReader & file, int lumpnum);
	FIMGZTexture (int lumpnum, WORD w, WORD h, SWORD l, SWORD t);

	int SourceLump;
	BYTE *Pixels;
	Span **Spans;

	void MakeTexture ();

	struct ImageHeader;
	friend class FTexture;
};


// A PNG image
class FPNGTexture : public FTexture
{
public:
	~FPNGTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:

	static bool Check (FileReader &file);
	static FTexture *Create (FileReader &file, int lumpnum);
	FPNGTexture (FileReader &lump, int lumpnum, int width, int height, BYTE bitdepth, BYTE colortype, BYTE interlace);

	int SourceLump;
	BYTE *Pixels;
	Span **Spans;

	BYTE BitDepth;
	BYTE ColorType;
	BYTE Interlace;

	BYTE *PaletteMap;
	int PaletteSize;
	DWORD StartOfIDAT;

	void MakeTexture ();

	friend class FTexture;
};


// A DDS image, with DXTx compression
class FDDSTexture : public FTexture
{
public:
	~FDDSTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	static bool Check (FileReader &file);
	static FTexture *Create (FileReader &file, int lumpnum);
	FDDSTexture (FileReader &lump, int lumpnum, void *surfdesc);

	int SourceLump;
	BYTE *Pixels;
	Span **Spans;

	DWORD Format;

	DWORD RMask, GMask, BMask, AMask;
	BYTE RShiftL, GShiftL, BShiftL, AShiftL;
	BYTE RShiftR, GShiftR, BShiftR, AShiftR;

	SDWORD Pitch;
	DWORD LinearSize;

	static void CalcBitShift (DWORD mask, BYTE *lshift, BYTE *rshift);

	void MakeTexture ();
	void ReadRGB (FWadLump &lump);
	void DecompressDXT1 (FWadLump &lump);
	void DecompressDXT3 (FWadLump &lump, bool premultiplied);
	void DecompressDXT5 (FWadLump &lump, bool premultiplied);

	friend class FTexture;
};

// A JPEG image
class FJPEGTexture : public FTexture
{
public:
	~FJPEGTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:

	static bool Check (FileReader &file);
	static FTexture *Create (FileReader &file, int lumpnum);
	FJPEGTexture (int lumpnum, int width, int height);

	int SourceLump;
	BYTE *Pixels;
	Span DummySpans[2];

	void MakeTexture ();

	friend class FTexture;
};

// A TGA texture

#pragma pack(1)

struct TGAHeader
{
	BYTE		id_len;
	BYTE		has_cm;
	BYTE		img_type;
	SWORD		cm_first;
	SWORD		cm_length;
	BYTE		cm_size;
	
	SWORD		x_origin;
	SWORD		y_origin;
	SWORD		width;
	SWORD		height;
	BYTE		bpp;
	BYTE		img_desc;
};

#pragma pack()

struct TGAHeader;

class FTGATexture : public FTexture
{
public:
	~FTGATexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	int SourceLump;
	BYTE *Pixels;
	Span **Spans;

	static bool Check(FileReader & file);
	static FTexture *Create(FileReader & file, int lumpnum);
	FTGATexture (int lumpnum, TGAHeader *);
	void ReadCompressed(FileReader &lump, BYTE * buffer, int bytesperpixel);

	virtual void MakeTexture ();

	friend class FTexture;
};

// A PCX texture

#pragma pack(1)

struct PCXHeader
{
  BYTE manufacturer;
  BYTE version;
  BYTE encoding;
  BYTE bitsPerPixel;

  WORD xmin, ymin;
  WORD xmax, ymax;
  WORD horzRes, vertRes;

  BYTE palette[48];
  BYTE reserved;
  BYTE numColorPlanes;

  WORD bytesPerScanLine;
  WORD paletteType;
  WORD horzSize, vertSize;

  BYTE padding[54];

};
#pragma pack()

class FPCXTexture : public FTexture
{
public:
	~FPCXTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	int SourceLump;
	BYTE *Pixels;
	Span DummySpans[2];

	static bool Check(FileReader & file);
	static FTexture *Create(FileReader & file, int lumpnum);
	FPCXTexture (int lumpnum, PCXHeader &);
	void ReadPCX1bit (BYTE *dst, FileReader & lump, PCXHeader *hdr);
	void ReadPCX4bits (BYTE *dst, FileReader & lump, PCXHeader *hdr);
	void ReadPCX8bits (BYTE *dst, FileReader & lump, PCXHeader *hdr);
	void ReadPCX24bits (BYTE *dst, FileReader & lump, PCXHeader *hdr, int planes);

	virtual void MakeTexture ();

	friend class FTexture;
};



// A texture that returns a wiggly version of another texture.
class FWarpTexture : public FTexture
{
public:
	FWarpTexture (FTexture *source);
	~FWarpTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	bool CheckModified ();

protected:
	FTexture *SourcePic;
	BYTE *Pixels;
	Span **Spans;
	DWORD GenTime;

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
	AActor *Viewpoint;
	FCanvasTexture *Texture;
	int PicNum;
	int FOV;

	static void Add (AActor *viewpoint, int picnum, int fov);
	static void UpdateAll ();
	static void EmptyList ();
	static void Serialize (FArchive &arc);

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
