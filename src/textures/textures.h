#ifndef __TEXTURES_H
#define __TEXTURES_H

#include "doomtype.h"

struct FloatRect
{
	float left,top;
	float width,height;


	void Offset(float xofs,float yofs)
	{
		left+=xofs;
		top+=yofs;
	}
	void Scale(float xfac,float yfac)
	{
		left*=xfac;
		width*=xfac;
		top*=yfac;
		height*=yfac;
	}
};


class FBitmap;
struct FRemapTable;
struct FCopyInfo;
class FScanner;
struct PClass;
class FArchive;

// Texture IDs
class FTextureManager;
class FTerrainTypeArray;
class FGLTexture;
class FMaterial;

class FTextureID
{
	friend class FTextureManager;
	friend FArchive &operator<< (FArchive &arc, FTextureID &tex);
	friend FTextureID GetHUDIcon(const PClass *cls);
	friend void R_InitSpriteDefs ();

public:
	FTextureID() throw() {}
	bool isNull() const { return texnum == 0; }
	bool isValid() const { return texnum > 0; }
	bool Exists() const { return texnum >= 0; }
	void SetInvalid() { texnum = -1; }
	void SetNull() { texnum = 0; }
	bool operator ==(const FTextureID &other) const { return texnum == other.texnum; }
	bool operator !=(const FTextureID &other) const { return texnum != other.texnum; }
	FTextureID operator +(int offset) throw();
	int GetIndex() const { return texnum; }	// Use this only if you absolutely need the index!

	// The switch list needs these to sort the switches by texture index
	int operator -(FTextureID other) const { return texnum - other.texnum; }
	bool operator < (FTextureID other) const { return texnum < other.texnum; }
	bool operator > (FTextureID other) const { return texnum > other.texnum; }
	
protected:
	FTextureID(int num) { texnum = num; }
private:
	int texnum;
};

class FNullTextureID : public FTextureID
{
public:
	FNullTextureID() : FTextureID(0) {}
};

FArchive &operator<< (FArchive &arc, FTextureID &tex);

//
// Animating textures and planes
//
// [RH] Expanded to work with a Hexen ANIMDEFS lump
//

struct FAnimDef
{
	FTextureID 	BasePic;
	WORD	NumFrames;
	WORD	CurFrame;
	BYTE	AnimType;
	DWORD	SwitchTime;			// Time to advance to next frame
	struct FAnimFrame
	{
		DWORD	SpeedMin;		// Speeds are in ms, not tics
		DWORD	SpeedRange;
		FTextureID	FramePic;
	} Frames[1];
	enum
	{
		ANIM_Forward,
		ANIM_Backward,
		ANIM_OscillateUp,
		ANIM_OscillateDown,
		ANIM_DiscreteFrames
	};

	void SetSwitchTime (DWORD mstime);
};

struct FSwitchDef
{
	FTextureID PreTexture;		// texture to switch from
	FSwitchDef *PairDef;		// switch def to use to return to PreTexture
	WORD NumFrames;		// # of animation frames
	bool QuestPanel;	// Special texture for Strife mission
	int Sound;			// sound to play at start of animation. Changed to int to avoiud having to include s_sound here.
	struct frame		// Array of times followed by array of textures
	{					//   actual length of each array is <NumFrames>
		WORD TimeMin;
		WORD TimeRnd;
		FTextureID Texture;
	} frames[1];
};

struct FDoorAnimation
{
	FTextureID BaseTexture;
	FTextureID *TextureFrames;
	int NumTextureFrames;
	FName OpenSound;
	FName CloseSound;
};

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures, and we compose
// textures from the TEXTURE1/2 lists of patches.
struct patch_t
{ 
	SWORD			width;			// bounding box size 
	SWORD			height; 
	SWORD			leftoffset; 	// pixels to the left of origin 
	SWORD			topoffset;		// pixels below the origin 
	DWORD 			columnofs[];	// only [width] used
	// the [0] is &columnofs[width] 
};

class FileReader;

// All FTextures present their data to the world in 8-bit format, but if
// the source data is something else, this is it.
enum FTextureFormat
{
	TEX_Pal,
	TEX_Gray,
	TEX_RGB,		// Actually ARGB
	TEX_DXT1,
	TEX_DXT2,
	TEX_DXT3,
	TEX_DXT4,
	TEX_DXT5,
};

class FNativeTexture;

// Base texture class
class FTexture
{
public:
	static FTexture *CreateTexture(const char *name, int lumpnum, int usetype);
	static FTexture *CreateTexture(int lumpnum, int usetype);
	virtual ~FTexture ();

	SWORD LeftOffset, TopOffset;

	BYTE WidthBits, HeightBits;

	fixed_t		xScale;
	fixed_t		yScale;

	int SourceLump;
	FTextureID id;

	union
	{
		char Name[9];
		DWORD dwName;		// Used with sprites
	};
	BYTE UseType;	// This texture's primary purpose

	BYTE bNoDecals:1;		// Decals should not stick to texture
	BYTE bNoRemap0:1;		// Do not remap color 0 (used by front layer of parallax skies)
	BYTE bWorldPanning:1;	// Texture is panned in world units rather than texels
	BYTE bMasked:1;			// Texture (might) have holes
	BYTE bAlphaTexture:1;	// Texture is an alpha channel without color information
	BYTE bHasCanvas:1;		// Texture is based off FCanvasTexture
	BYTE bWarped:2;			// This is a warped texture. Used to avoid multiple warps on one texture
	BYTE bComplex:1;		// Will be used to mark extended MultipatchTextures that have to be
							// fully composited before subjected to any kind of postprocessing instead of
							// doing it per patch.
	BYTE bMultiPatch:1;		// This is a multipatch texture (we really could use real type info for textures...)
	BYTE bKeepAround:1;		// This texture was used as part of a multi-patch texture. Do not free it.

	WORD Rotations;
	SWORD SkyOffset;

	enum // UseTypes
	{
		TEX_Any,
		TEX_Wall,
		TEX_Flat,
		TEX_Sprite,
		TEX_WallPatch,
		TEX_Build,
		TEX_SkinSprite,
		TEX_Decal,
		TEX_MiscPatch,
		TEX_FontChar,
		TEX_Override,	// For patches between TX_START/TX_END
		TEX_Autopage,	// Automap background - used to enable the use of FAutomapTexture
		TEX_SkinGraphic,
		TEX_Null,
		TEX_FirstDefined,
	};

	struct Span
	{
		WORD TopOffset;
		WORD Length;	// A length of 0 terminates this column
	};

	// Returns a single column of the texture
	virtual const BYTE *GetColumn (unsigned int column, const Span **spans_out) = 0;

	// Returns the whole texture, stored in column-major order
	virtual const BYTE *GetPixels () = 0;
	
	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate=0, FCopyInfo *inf = NULL);
	int CopyTrueColorTranslated(FBitmap *bmp, int x, int y, int rotate, FRemapTable *remap, FCopyInfo *inf = NULL);
	virtual bool UseBasePalette();
	virtual int GetSourceLump() { return SourceLump; }
	virtual FTexture *GetRedirect(bool wantwarped);
	virtual FTexture *GetRawTexture();		// for FMultiPatchTexture to override
	FTextureID GetID() const { return id; }

	virtual void Unload () = 0;

	// Returns the native pixel format for this image
	virtual FTextureFormat GetFormat();

	// Returns a native 3D representation of the texture
	FNativeTexture *GetNative(bool wrapping);

	// Frees the native 3D representation of the texture
	void KillNative();

	// Fill the native texture buffer with pixel data for this image
	virtual void FillBuffer(BYTE *buff, int pitch, int height, FTextureFormat fmt);

	int GetWidth () { return Width; }
	int GetHeight () { return Height; }

	int GetScaledWidth () { int foo = (Width << 17) / xScale; return (foo >> 1) + (foo & 1); }
	int GetScaledHeight () { int foo = (Height << 17) / yScale; return (foo >> 1) + (foo & 1); }
	double GetScaledWidthDouble () { return (Width * 65536.) / xScale; }
	double GetScaledHeightDouble () { return (Height * 65536.) / yScale; }

	int GetScaledLeftOffset () { int foo = (LeftOffset << 17) / xScale; return (foo >> 1) + (foo & 1); }
	int GetScaledTopOffset () { int foo = (TopOffset << 17) / yScale; return (foo >> 1) + (foo & 1); }
	double GetScaledLeftOffsetDouble() { return (LeftOffset * 65536.) / xScale; }
	double GetScaledTopOffsetDouble() { return (TopOffset * 65536.) / yScale; }

	virtual void SetFrontSkyLayer();

	void CopyToBlock (BYTE *dest, int dwidth, int dheight, int x, int y, const BYTE *translation=NULL)
	{
		CopyToBlock(dest, dwidth, dheight, x, y, 0, translation);
	}

	void CopyToBlock (BYTE *dest, int dwidth, int dheight, int x, int y, int rotate, const BYTE *translation=NULL);

	// Returns true if the next call to GetPixels() will return an image different from the
	// last call to GetPixels(). This should be considered valid only if a call to CheckModified()
	// is immediately followed by a call to GetPixels().
	virtual bool CheckModified ();

	static void InitGrayMap();

	void CopySize(FTexture *BaseTexture)
	{
		Width = BaseTexture->GetWidth();
		Height = BaseTexture->GetHeight();
		TopOffset = BaseTexture->TopOffset;
		LeftOffset = BaseTexture->LeftOffset;
		WidthBits = BaseTexture->WidthBits;
		HeightBits = BaseTexture->HeightBits;
		xScale = BaseTexture->xScale;
		yScale = BaseTexture->yScale;
		WidthMask = (1 << WidthBits) - 1;
	}

	void SetScaledSize(int fitwidth, int fitheight);

	virtual void HackHack (int newheight);	// called by FMultipatchTexture to discover corrupt patches.

protected:
	WORD Width, Height, WidthMask;
	static BYTE GrayMap[256];
	FNativeTexture *Native;

	FTexture (const char *name = NULL, int lumpnum = -1);

	Span **CreateSpans (const BYTE *pixels) const;
	void FreeSpans (Span **spans) const;
	void CalcBitSize ();
	void CopyInfo(FTexture *other)
	{
		CopySize(other);
		bNoDecals = other->bNoDecals;
		Rotations = other->Rotations;
		gl_info = other->gl_info;
		gl_info.Brightmap = NULL;
		gl_info.areas = NULL;
	}

public:
	static void FlipSquareBlock (BYTE *block, int x, int y);
	static void FlipSquareBlockRemap (BYTE *block, int x, int y, const BYTE *remap);
	static void FlipNonSquareBlock (BYTE *blockto, const BYTE *blockfrom, int x, int y, int srcpitch);
	static void FlipNonSquareBlockRemap (BYTE *blockto, const BYTE *blockfrom, int x, int y, int srcpitch, const BYTE *remap);

	friend class D3DTex;

public:

	struct MiscGLInfo
	{
		FMaterial *Material;
		FGLTexture *SystemTexture;
		FTexture *Brightmap;
		FTexture *DecalTexture;					// This is needed for decals of UseType TEX_MiscPatch-
		PalEntry GlowColor;
		PalEntry FloorSkyColor;
		PalEntry CeilingSkyColor;
		int GlowHeight;
		FloatRect *areas;
		int areacount;
		int shaderindex;
		float shaderspeed;
		int mIsTransparent:2;
		bool bGlowing:1;						// Texture glows
		bool bFullbright:1;						// always draw fullbright
		bool bSkybox:1;							// This is a skybox
		bool bSkyColorDone:1;					// Fill color for sky
		char bBrightmapChecked:1;				// Set to 1 if brightmap has been checked
		bool bBrightmap:1;						// This is a brightmap
		bool bBrightmapDisablesFullbright:1;	// This disables fullbright display
		bool bNoFilter:1;
		bool bNoCompress:1;
		bool mExpanded:1;

		MiscGLInfo() throw ();
		~MiscGLInfo();
	};
	MiscGLInfo gl_info;

	virtual void PrecacheGL();
	virtual void UncacheGL();
	void GetGlowColor(float *data);
	PalEntry GetSkyCapColor(bool bottom);
	bool isGlowing() { return gl_info.bGlowing; }
	bool isFullbright() { return gl_info.bFullbright; }
	void CreateDefaultBrightmap();
	bool FindHoles(const unsigned char * buffer, int w, int h);
	static bool SmoothEdges(unsigned char * buffer,int w, int h);
	void CheckTrans(unsigned char * buffer, int size, int trans);
	bool ProcessData(unsigned char * buffer, int w, int h, bool ispatch);
};

// Texture manager
class FTextureManager
{
public:
	FTextureManager ();
	~FTextureManager ();

	// Get texture without translation
	FTexture *operator[] (FTextureID texnum)
	{
		if ((unsigned)texnum.GetIndex() >= Textures.Size()) return NULL;
		return Textures[texnum.GetIndex()].Texture;
	}
	FTexture *operator[] (const char *texname)
	{
		FTextureID texnum = GetTexture (texname, FTexture::TEX_MiscPatch);
		if (!texnum.Exists()) return NULL;
		return Textures[texnum.GetIndex()].Texture;
	}
	FTexture *ByIndex(int i)
	{
		if (unsigned(i) >= Textures.Size()) return NULL;
		return Textures[i].Texture;
	}
	FTexture *FindTexture(const char *texname, int usetype = FTexture::TEX_MiscPatch, BITFIELD flags = TEXMAN_TryAny);

	// Get texture with translation
	FTexture *operator() (FTextureID texnum, bool withpalcheck=false)
	{
		if ((size_t)texnum.texnum >= Textures.Size()) return NULL;
		int picnum = Translation[texnum.texnum];
		if (withpalcheck)
		{
			picnum = PalCheck(picnum).GetIndex();
		}
		return Textures[picnum].Texture;
	}
	FTexture *operator() (const char *texname)
	{
		FTextureID texnum = GetTexture (texname, FTexture::TEX_MiscPatch);
		if (texnum.texnum == -1) return NULL;
		return Textures[Translation[texnum.texnum]].Texture;
	}

	FTexture *ByIndexTranslated(int i)
	{
		if (unsigned(i) >= Textures.Size()) return NULL;
		return Textures[Translation[i]].Texture;
	}

	FTextureID PalCheck(FTextureID tex);

	enum
	{
		TEXMAN_TryAny = 1,
		TEXMAN_Overridable = 2,
		TEXMAN_ReturnFirst = 4,
		TEXMAN_AllowSkins = 8
	};

	FTextureID CheckForTexture (const char *name, int usetype, BITFIELD flags=TEXMAN_TryAny);
	FTextureID GetTexture (const char *name, int usetype, BITFIELD flags=0);
	FTextureID FindTextureByLumpNum (int lumpnum);
	int ListTextures (const char *name, TArray<FTextureID> &list);

	void AddTexturesLump (const void *lumpdata, int lumpsize, int deflumpnum, int patcheslump, int firstdup=0, bool texture1=false);
	void AddTexturesLumps (int lump1, int lump2, int patcheslump);
	void AddGroup(int wadnum, int ns, int usetype);
	void AddPatches (int lumpnum);
	void AddHiresTextures (int wadnum);
	void LoadTextureDefs(int wadnum, const char *lumpname);
	void ParseXTexture(FScanner &sc, int usetype);
	void SortTexturesByType(int start, int end);
	bool AreTexturesCompatible (FTextureID picnum1, FTextureID picnum2);

	FTextureID CreateTexture (int lumpnum, int usetype=FTexture::TEX_Any);	// Also calls AddTexture
	FTextureID AddTexture (FTexture *texture);
	FTextureID GetDefaultTexture() const { return DefaultTexture; }

	void LoadTextureX(int wadnum);
	void AddTexturesForWad(int wadnum);
	void Init();
	void DeleteAll();

	// Replaces one texture with another. The new texture will be assigned
	// the same name, slot, and use type as the texture it is replacing.
	// The old texture will no longer be managed. Set free true if you want
	// the old texture to be deleted or set it false if you want it to
	// be left alone in memory. You will still need to delete it at some
	// point, because the texture manager no longer knows about it.
	// This function can be used for such things as warping textures.
	void ReplaceTexture (FTextureID picnum, FTexture *newtexture, bool free);

	void UnloadAll ();

	int NumTextures () const { return (int)Textures.Size(); }
	void PrecacheLevel (void);

	void WriteTexture (FArchive &arc, int picnum);
	int ReadTexture (FArchive &arc);

	void UpdateAnimations (DWORD mstime);
	int GuesstimateNumTextures ();

	FSwitchDef *FindSwitch (FTextureID texture);
	FDoorAnimation *FindAnimatedDoor (FTextureID picnum);

private:

	// texture counting
	int CountTexturesX ();
	int CountLumpTextures (int lumpnum);

	// Build tiles
	void AddTiles (void *tiles);
	int CountTiles (void *tiles);
	int CountBuildTiles ();
	void InitBuildTiles ();

	// Animation stuff
	void AddAnim (FAnimDef *anim);
	void FixAnimations ();
	void InitAnimated ();
	void InitAnimDefs ();
	void AddSimpleAnim (FTextureID picnum, int animcount, int animtype, DWORD speedmin, DWORD speedrange=0);
	void AddComplexAnim (FTextureID picnum, const TArray<FAnimDef::FAnimFrame> &frames);
	void ParseAnim (FScanner &sc, int usetype);
	void ParseRangeAnim (FScanner &sc, FTextureID picnum, int usetype, bool missing);
	void ParsePicAnim (FScanner &sc, FTextureID picnum, int usetype, bool missing, TArray<FAnimDef::FAnimFrame> &frames);
	void ParseWarp(FScanner &sc);
	void ParseCameraTexture(FScanner &sc);
	FTextureID ParseFramenum (FScanner &sc, FTextureID basepicnum, int usetype, bool allowMissing);
	void ParseTime (FScanner &sc, DWORD &min, DWORD &max);
	FTexture *Texture(FTextureID id) { return Textures[id.GetIndex()].Texture; }
	void SetTranslation (FTextureID fromtexnum, FTextureID totexnum);
	void ParseAnimatedDoor(FScanner &sc);

	void InitPalettedVersions();

	// Switches

	void InitSwitchList ();
	void ProcessSwitchDef (FScanner &sc);
	FSwitchDef *ParseSwitchDef (FScanner &sc, bool ignoreBad);
	void AddSwitchPair (FSwitchDef *def1, FSwitchDef *def2);

	struct TextureHash
	{
		FTexture *Texture;
		int HashNext;
	};
	enum { HASH_END = -1, HASH_SIZE = 1027 };
	TArray<TextureHash> Textures;
	TArray<int> Translation;
	int HashFirst[HASH_SIZE];
	FTextureID DefaultTexture;
	TArray<int> FirstTextureForFile;
	TMap<int,int> PalettedVersions;		// maps from normal -> paletted version

	TArray<FAnimDef *> mAnimations;
	TArray<FSwitchDef *> mSwitchDefs;
	TArray<FDoorAnimation> mAnimatedDoors;
	TArray<BYTE *> BuildTileFiles;
};

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

	virtual int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate=0, FCopyInfo *inf = NULL);
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
class AActor;
class FArchive;

class FCanvasTexture : public FTexture
{
public:
	FCanvasTexture (const char *name, int width, int height);
	~FCanvasTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	bool CheckModified ();
	void NeedUpdate() { bNeedsUpdate=true; }
	void SetUpdated() { bNeedsUpdate = false; bDidUpdate = true; bFirstUpdate = false; }
	DSimpleCanvas *GetCanvas() { return Canvas; }
	void MakeTexture ();

protected:

	DSimpleCanvas *Canvas;
	BYTE *Pixels;
	Span DummySpans[2];
	bool bNeedsUpdate;
	bool bDidUpdate;
	bool bPixelsAllocated;
public:
	bool bFirstUpdate;


	friend struct FCanvasTextureInfo;
};

extern FTextureManager TexMan;

#endif


