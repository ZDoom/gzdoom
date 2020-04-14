/*
** textures.h
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
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

#ifndef __TEXTURES_H
#define __TEXTURES_H

#include "basics.h"
#include "vectors.h"
#include "colormatcher.h"
#include "renderstyle.h"
#include "textureid.h"
#include <vector>
#include "hw_texcontainer.h"

// 15 because 0th texture is our texture
#define MAX_CUSTOM_HW_SHADER_TEXTURES 15

typedef TMap<int, bool> SpriteHits;
class FImageSource;
class FGameTexture;

enum MaterialShaderIndex
{
	SHADER_Default,
	SHADER_Warp1,
	SHADER_Warp2,
	SHADER_Specular,
	SHADER_PBR,
	SHADER_Paletted,
	SHADER_NoTexture,
	SHADER_BasicFuzz,
	SHADER_SmoothFuzz,
	SHADER_SwirlyFuzz,
	SHADER_TranslucentFuzz,
	SHADER_JaggedFuzz,
	SHADER_NoiseFuzz,
	SHADER_SmoothNoiseFuzz,
	SHADER_SoftwareFuzz,
	FIRST_USER_SHADER
};

enum texflags
{
	// These get Or'ed into uTextureMode because it only uses its 3 lowermost bits.
	TEXF_Brightmap = 0x10000,
	TEXF_Detailmap = 0x20000,
	TEXF_Glowmap = 0x40000,
};



enum
{
	SFlag_Brightmap = 1,
	SFlag_Detailmap = 2,
	SFlag_Glowmap = 4,
};

struct UserShaderDesc
{
	FString shader;
	MaterialShaderIndex shaderType;
	FString defines;
	bool disablealphatest = false;
	uint8_t shaderFlags = 0;
};

extern TArray<UserShaderDesc> usershaders;


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

enum ECreateTexBufferFlags
{
	CTF_Expand = 2,			// create buffer with a one-pixel wide border
	CTF_ProcessData = 4,	// run postprocessing on the generated buffer. This is only needed when using the data for a hardware texture.
	CTF_CheckOnly = 8,		// Only runs the code to get a content ID but does not create a texture. Can be used to access a caching system for the hardware textures.
};



class FBitmap;
struct FRemapTable;
struct FCopyInfo;
class FScanner;

// Texture IDs
class FTextureManager;
class FTerrainTypeArray;
class IHardwareTexture;
class FMaterial;
class FMultipatchTextureBuilder;

extern int r_spriteadjustSW, r_spriteadjustHW;

class FNullTextureID : public FTextureID
{
public:
	FNullTextureID() : FTextureID(0) {}
};

enum FTextureFormat : uint32_t
{
	TEX_Pal,
	TEX_Gray,
	TEX_RGB,		// Actually ARGB

	TEX_Count
};

class ISoftwareTexture
{
public:
	virtual ~ISoftwareTexture() = default;
};

class FGLRenderState;

struct spriteframewithrotate;
class FSerializer;
namespace OpenGLRenderer
{
	class FGLRenderState;
	class FHardwareTexture;
}

union FContentIdBuilder
{
	uint64_t id;
	struct
	{
		unsigned imageID : 24;
		unsigned translation : 16;
		unsigned expand : 1;
		unsigned scaler : 4;
		unsigned scalefactor : 4;
	};
};

struct FTextureBuffer
{
	uint8_t *mBuffer = nullptr;
	int mWidth = 0;
	int mHeight = 0;
	uint64_t mContentId = 0;	// unique content identifier. (Two images created from the same image source with the same settings will return the same value.)

	FTextureBuffer() = default;

	~FTextureBuffer()
	{
		if (mBuffer) delete[] mBuffer;
	}

	FTextureBuffer(const FTextureBuffer &other) = delete;
	FTextureBuffer(FTextureBuffer &&other)
	{
		mBuffer = other.mBuffer;
		mWidth = other.mWidth;
		mHeight = other.mHeight;
		mContentId = other.mContentId;
		other.mBuffer = nullptr;
	}

	FTextureBuffer& operator=(FTextureBuffer &&other)
	{
		mBuffer = other.mBuffer;
		mWidth = other.mWidth;
		mHeight = other.mHeight;
		mContentId = other.mContentId;
		other.mBuffer = nullptr;
		return *this;
	}

};

struct SpritePositioningInfo
{
	uint16_t trim[4];
	int spriteWidth, spriteHeight;
	float mSpriteU[2], mSpriteV[2];
	FloatRect mSpriteRect;

	float GetSpriteUL() const { return mSpriteU[0]; }
	float GetSpriteVT() const { return mSpriteV[0]; }
	float GetSpriteUR() const { return mSpriteU[1]; }
	float GetSpriteVB() const { return mSpriteV[1]; }

	const FloatRect &GetSpriteRect() const
	{
		return mSpriteRect;
	}

};

// Base texture class
class FTexture
{
	friend class FGameTexture;	// only for the porting work
	friend class GLDefsParser;
	friend class FMultipatchTextureBuilder;
	
	// The serializer also needs access to more specific info that shouldn't be accessible through the interface.
	friend FSerializer &Serialize(FSerializer &arc, const char *key, FTextureID &value, FTextureID *defval);

	// For now only give access to classes which cannot be reworked yet. None of these should remain here when all is done.
	friend class FWarpTexture;
	friend class FMaterial;
	friend class OpenGLRenderer::FGLRenderState;	// For now this needs access to some fields in ApplyMaterial. This should be rerouted through the Material class
	friend class VkRenderState;
	friend class PolyRenderState;
	friend struct FTexCoordInfo;
	friend class OpenGLRenderer::FHardwareTexture;
	friend class VkHardwareTexture;
	friend class PolyHardwareTexture;
	friend class FMultiPatchTexture;
	friend class FSkyBox;
	friend class FBrightmapTexture;
	friend class FFont;


public:

	SpritePositioningInfo spi;
	int8_t mTrimResult = -1;

	static FTexture *CreateTexture(const char *name, int lumpnum, ETextureType usetype);
	virtual ~FTexture ();
	virtual FImageSource *GetImage() const { return nullptr; }
	void AddAutoMaterials();
	void CreateUpsampledTextureBuffer(FTextureBuffer &texbuffer, bool hasAlpha, bool checkonly);
	void CleanHardwareTextures(bool cleannormal, bool cleanextended);

	// These are mainly meant for 2D code which only needs logical information about the texture to position it properly.
	int GetDisplayWidth() { int foo = int((Width * 2) / Scale.X); return (foo >> 1) + (foo & 1); }
	int GetDisplayHeight() { int foo = int((Height * 2) / Scale.Y); return (foo >> 1) + (foo & 1); }
	double GetDisplayWidthDouble() { return Width / Scale.X; }
	double GetDisplayHeightDouble() { return Height / Scale.Y; }
	int GetDisplayLeftOffset() { return GetScaledLeftOffset(0); }
	int GetDisplayTopOffset() { return GetScaledTopOffset(0); }
	double GetDisplayLeftOffsetDouble(int adjusted = 0) { return _LeftOffset[adjusted] / Scale.X; }
	double GetDisplayTopOffsetDouble(int adjusted = 0) { return _TopOffset[adjusted] / Scale.Y; }
	FTexture* GetFrontSkyLayer();

	int GetTexelWidth() { return Width; }
	int GetTexelHeight() { return Height; }
	int GetTexelLeftOffset(int adjusted) { return _LeftOffset[adjusted]; }
	int GetTexelTopOffset(int adjusted) { return _TopOffset[adjusted]; }

	
	bool isValid() const { return UseType != ETextureType::Null; }
	bool isSWCanvas() const { return UseType == ETextureType::SWCanvas; }
	bool isSkybox() const { return bSkybox; }
	bool isFullbrightDisabled() const { return bDisableFullbright; }
	bool isHardwareCanvas() const { return bHasCanvas; }	// There's two here so that this can deal with software canvases in the hardware renderer later.
	bool isCanvas() const { return bHasCanvas; }
	int isWarped() const { return bWarped; }
	int GetRotations() const { return Rotations; }
	float GetShaderSpeed() const { return shaderspeed; }
	void SetRotations(int rot) { Rotations = int16_t(rot); }
	bool isSprite() const { return UseType == ETextureType::Sprite || UseType == ETextureType::SkinSprite || UseType == ETextureType::Decal; }
	
	const FString &GetName() const { return Name; }
	void SetNoDecals(bool on) { bNoDecals = on;  }
	void SetWarpStyle(int style) { bWarped = style; }
	bool allowNoDecals() const { return bNoDecals; }
	bool isScaled() const { return Scale.X != 1 || Scale.Y != 1; }
	bool isMasked() const { return bMasked; }
	void SetSkyOffset(int offs) { SkyOffset = offs; }
	int GetSkyOffset() const { return SkyOffset; }
	FTextureID GetID() const { return id; }
	PalEntry GetSkyCapColor(bool bottom);
	FTexture *GetRawTexture();
	virtual int GetSourceLump() { return SourceLump; }	// needed by the scripted GetName method.
	void GetGlowColor(float *data);
	bool isGlowing() const { return bGlowing; }
	bool isAutoGlowing() const { return bAutoGlowing; }
	int GetGlowHeight() const { return GlowHeight; }
	bool isFullbright() const { return bFullbright; }
	void CreateDefaultBrightmap();
	bool FindHoles(const unsigned char * buffer, int w, int h);
	void SetUseType(ETextureType type) { UseType = type; }
	int GetSourceLump() const { return SourceLump;  }
	ETextureType GetUseType() const { return UseType; }
	void SetSpeed(float fac) { shaderspeed = fac; }
	bool UseWorldPanning() const  { return bWorldPanning; }
	void SetWorldPanning(bool on) { bWorldPanning = on; }
	void SetDisplaySize(int fitwidth, int fitheight);


	void CopySize(FTexture* BaseTexture)
	{
		Width = BaseTexture->GetTexelWidth();
		Height = BaseTexture->GetTexelHeight();
		_TopOffset[0] = BaseTexture->_TopOffset[0];
		_TopOffset[1] = BaseTexture->_TopOffset[1];
		_LeftOffset[0] = BaseTexture->_LeftOffset[0];
		_LeftOffset[1] = BaseTexture->_LeftOffset[1];
		Scale = BaseTexture->Scale;
	}

	// This is only used for the null texture and for Heretic's skies.
	void SetSize(int w, int h)
	{
		Width = w;
		Height = h;
	}

	bool TrimBorders(uint16_t* rect);
	void SetSpriteRect();
	bool ShouldExpandSprite();
	void SetupSpriteData();
	int GetAreas(FloatRect** pAreas) const;

	// Returns the whole texture, stored in column-major order
	virtual TArray<uint8_t> Get8BitPixels(bool alphatex);
	virtual FBitmap GetBgraBitmap(const PalEntry *remap, int *trans = nullptr);

	static bool SmoothEdges(unsigned char * buffer,int w, int h);
	static PalEntry averageColor(const uint32_t *data, int size, int maxout);

	
	ISoftwareTexture* GetSoftwareTexture()
	{
		return SoftwareTexture;
	}
	void SetSoftwareTextue(ISoftwareTexture* swtex)
	{
		SoftwareTexture = swtex;
	}

protected:

	DVector2 Scale;

	int SourceLump;
	FTextureID id;

	FMaterial *Material[2] = { nullptr, nullptr };
public:
	FHardwareTextureContainer SystemTextures;
protected:
	ISoftwareTexture *SoftwareTexture = nullptr;

	// None of the following pointers are owned by this texture, they are all controlled by the texture manager.

	// Offset-less version for COMPATF_MASKEDMIDTEX
	FTexture *OffsetLess = nullptr;
	// Front sky layer variant where color 0 is transparent
	FTexture* FrontSkyLayer = nullptr;
	public:
	// Paletted variant
	FTexture *PalVersion = nullptr;
	// Material layers
	FTexture *Brightmap = nullptr;
	FTexture* Detailmap = nullptr;
	FTexture* Glowmap = nullptr;
	FTexture *Normal = nullptr;							// Normal map texture
	FTexture *Specular = nullptr;						// Specular light texture for the diffuse+normal+specular light model
	FTexture *Metallic = nullptr;						// Metalness texture for the physically based rendering (PBR) light model
	FTexture *Roughness = nullptr;						// Roughness texture for PBR
	FTexture *AmbientOcclusion = nullptr;				// Ambient occlusion texture for PBR
	
	FTexture *CustomShaderTextures[MAX_CUSTOM_HW_SHADER_TEXTURES] = { nullptr }; // Custom texture maps for custom hardware shaders

	protected:

	FString Name;
	ETextureType UseType;	// This texture's primary purpose

	uint8_t bNoDecals:1;		// Decals should not stick to texture
	uint8_t bNoRemap0:1;		// Do not remap color 0 (used by front layer of parallax skies)
	uint8_t bWorldPanning:1;	// Texture is panned in world units rather than texels
	uint8_t bMasked:1;			// Texture (might) have holes
	uint8_t bAlphaTexture:1;	// Texture is an alpha channel without color information
	uint8_t bHasCanvas:1;		// Texture is based off FCanvasTexture
	uint8_t bWarped:2;			// This is a warped texture. Used to avoid multiple warps on one texture
	uint8_t bComplex:1;		// Will be used to mark extended MultipatchTextures that have to be
							// fully composited before subjected to any kind of postprocessing instead of
							// doing it per patch.
	uint8_t bMultiPatch:2;		// This is a multipatch texture (we really could use real type info for textures...)
	uint8_t bFullNameTexture : 1;
	uint8_t bBrightmapChecked : 1;				// Set to 1 if brightmap has been checked
	public:
	uint8_t bGlowing : 1;						// Texture glow color
	uint8_t bAutoGlowing : 1;					// Glow info is determined from texture image.
	uint8_t bFullbright : 1;					// always draw fullbright
	uint8_t bDisableFullbright : 1;				// This texture will not be displayed as fullbright sprite
	protected:
	uint8_t bSkybox : 1;						// is a cubic skybox
	uint8_t bNoCompress : 1;
	int8_t bTranslucent : 2;
	int8_t bExpandSprite = -1;
	bool bHiresHasColorKey = false;				// Support for old color-keyed Doomsday textures

	uint16_t Rotations;
	int16_t SkyOffset;
	FloatRect *areas = nullptr;
	int areacount = 0;
	public:
	int GlowHeight = 128;
	PalEntry GlowColor = 0;
	private:
	float Glossiness = 10.f;
	float SpecularLevel = 0.1f;
	float shaderspeed = 1.f;
	int shaderindex = 0;

	int GetScaledWidth () { int foo = int((Width * 2) / Scale.X); return (foo >> 1) + (foo & 1); }
	int GetScaledHeight () { int foo = int((Height * 2) / Scale.Y); return (foo >> 1) + (foo & 1); }
	double GetScaledWidthDouble () { return Width / Scale.X; }
	double GetScaledHeightDouble () { return Height / Scale.Y; }
	double GetScaleY() const { return Scale.Y; }

	// Now with improved offset adjustment.
	int GetLeftOffset(int adjusted) { return _LeftOffset[adjusted]; }
	int GetTopOffset(int adjusted) { return _TopOffset[adjusted]; }
	int GetScaledLeftOffset (int adjusted) { int foo = int((_LeftOffset[adjusted] * 2) / Scale.X); return (foo >> 1) + (foo & 1); }
	int GetScaledTopOffset (int adjusted) { int foo = int((_TopOffset[adjusted] * 2) / Scale.Y); return (foo >> 1) + (foo & 1); }

	// Interfaces for the different renderers. Everything that needs to check renderer-dependent offsets
	// should use these, so that if changes are needed, this is the only place to edit.

	// For the hardware renderer. The software renderer's have been offloaded to FSoftwareTexture
	int GetLeftOffsetHW() { return _LeftOffset[r_spriteadjustHW]; }
	int GetTopOffsetHW() { return _TopOffset[r_spriteadjustHW]; }

	virtual void ResolvePatches() {}

public:
	void SetScale(const DVector2 &scale)
	{
		Scale = scale;
	}

protected:
	uint16_t Width, Height;
	int16_t _LeftOffset[2], _TopOffset[2];

	FTexture (const char *name = NULL, int lumpnum = -1);

public:
	FTextureBuffer CreateTexBuffer(int translation, int flags = 0);
	virtual bool DetermineTranslucency();
	bool GetTranslucency()
	{
		return bTranslucent != -1 ? bTranslucent : DetermineTranslucency();
	}
	FMaterial* GetMaterial(int num)
	{
		return Material[num];
	}
	FTexture* GetPalVersion()
	{
		return PalVersion;
	}

private:
	int CheckDDPK3();
	int CheckExternalFile(bool & hascolorkey);
	
	bool bSWSkyColorDone = false;
	PalEntry FloorSkyColor;
	PalEntry CeilingSkyColor;

public:

	void CheckTrans(unsigned char * buffer, int size, int trans);
	bool ProcessData(unsigned char * buffer, int w, int h, bool ispatch);
	int CheckRealHeight();

	friend class FTextureManager;
};


// A texture that can be drawn to.

class FCanvasTexture : public FTexture
{
public:
	FCanvasTexture(const char* name, int width, int height)
	{
		Name = name;
		Width = width;
		Height = height;

		bMasked = false;
		bHasCanvas = true;
		bTranslucent = false;
		bExpandSprite = false;
		UseType = ETextureType::Wall;
	}

	void NeedUpdate() { bNeedsUpdate = true; }
	void SetUpdated(bool rendertype) { bNeedsUpdate = false; bFirstUpdate = false; bLastUpdateType = rendertype; }

protected:

	bool bLastUpdateType = false;
	bool bNeedsUpdate = true;
public:
	bool bFirstUpdate = true;

	friend struct FCanvasTextureInfo;
};


// A wrapper around a hardware texture, to allow using it in the 2D drawing interface.
class FWrapperTexture : public FTexture
{
	int Format;
public:
	FWrapperTexture(int w, int h, int bits = 1);
	IHardwareTexture *GetSystemTexture()
	{
		return SystemTextures.GetHardwareTexture(0, false);
	}

	int GetColorFormat() const
	{
		return Format;
	}
};


class FImageTexture : public FTexture
{
	FImageSource* mImage;
protected:
	FImageTexture(const char *name) : FTexture(name) {}
	void SetFromImage();
public:
	FImageTexture(FImageSource* image, const char* name = nullptr) noexcept;
	virtual TArray<uint8_t> Get8BitPixels(bool alphatex);

	void SetImage(FImageSource* img)	// This is only for the multipatch texture builder!
	{
		mImage = img;
	}

	FImageSource* GetImage() const override { return mImage; }
	FBitmap GetBgraBitmap(const PalEntry* p, int* trans) override;
	bool DetermineTranslucency() override;

};

struct MaterialLayers
{
	float Glossiness;
	float SpecularLevel;
	FGameTexture* Brightmap;
	FGameTexture* Normal;
	FGameTexture* Specular;
	FGameTexture* Metallic;
	FGameTexture* Roughness;
	FGameTexture* AmbientOcclusion;
	FGameTexture* CustomShaderTextures[MAX_CUSTOM_HW_SHADER_TEXTURES];
};

struct FTexCoordInfo
{
	int mRenderWidth;
	int mRenderHeight;
	int mWidth;
	FVector2 mScale;
	FVector2 mTempScale;
	bool mWorldPanning;

	float FloatToTexU(float v) const { return v / mRenderWidth; }
	float FloatToTexV(float v) const { return v / mRenderHeight; }
	float RowOffset(float ofs) const;
	float TextureOffset(float ofs) const;
	float TextureAdjustWidth() const;
	void GetFromTexture(FTexture *tex, float x, float y, bool forceworldpanning);
	void GetFromTexture(FGameTexture* tex, float x, float y, bool forceworldpanning);
};

//-----------------------------------------------------------------------------
//
// Todo: Get rid of this
// The faces can easily be stored in the material layer array
//
//-----------------------------------------------------------------------------

class FSkyBox : public FImageTexture
{
public:

	FTexture* previous;
	FTexture* faces[6];
	bool fliptop;

	FSkyBox(const char* name);
	void SetSize();

	bool Is3Face() const
	{
		return faces[5] == nullptr;
	}

	bool IsFlipped() const
	{
		return fliptop;
	}
};

// Refactoring helper to allow piece by piece adjustment of the API
class FGameTexture
{
	FTexture wrapped;
public:
	FTexture* GetTexture() { return &wrapped; }
	int GetSourceLump() const { return wrapped.GetSourceLump(); }
	void SetBrightmap(FGameTexture* tex) { wrapped.Brightmap = tex->GetTexture(); }

	double GetDisplayWidth() /*const*/ { return wrapped.GetDisplayWidthDouble(); }
	double GetDisplayHeight() /*const*/ { return wrapped.GetDisplayHeightDouble(); }
	int GetTexelWidth() /*const*/ { return wrapped.GetTexelWidth(); }
	int GetTexelHeight() /*const*/ { return wrapped.GetTexelHeight(); }
	int GetTexelLeftOffset(int adjusted = 0) /*const*/ { return wrapped.GetTexelLeftOffset(adjusted); }
	int GetTexelTopOffset(int adjusted = 0) /*const*/ { return wrapped.GetTexelTopOffset(adjusted); }
	double GetDisplayLeftOffset(int adjusted = 0) /*const*/ { return wrapped.GetDisplayLeftOffsetDouble(adjusted); }
	double GetDisplayTopOffset(int adjusted = 0) /*const*/ { return wrapped.GetDisplayTopOffsetDouble(adjusted); }

	bool isValid() { return wrapped.isValid(); }
	bool isWarped() { return wrapped.isWarped(); }
	bool isMasked() { return wrapped.isMasked(); }
	bool isHardwareCanvas() const { return wrapped.isHardwareCanvas(); }	// There's two here so that this can deal with software canvases in the hardware renderer later.
	bool isSoftwareCanvas() const { return wrapped.isCanvas(); }
	bool isMiscPatch() const { return wrapped.GetUseType() == ETextureType::MiscPatch; }	// only used by the intermission screen to decide whether to tile the background image or not. 
	bool isMultiPatch() const { return wrapped.bMultiPatch; }
	bool isFullbrightDisabled() const { return wrapped.isFullbrightDisabled(); }
	bool isFullbright() const { return wrapped.isFullbright(); }
	bool expandSprites() const { return wrapped.bExpandSprite; }
	bool useWorldPanning() const { return wrapped.UseWorldPanning();  }
	bool allowNoDecals() const { return wrapped.allowNoDecals(); }
	void SetTranslucent(bool on) { wrapped.bTranslucent = on; }
	ETextureType GetUseType() const { return wrapped.GetUseType(); }
	void SetUseType(ETextureType type) { wrapped.SetUseType(type); }
	float GetShaderSpeed() const { return wrapped.GetShaderSpeed(); }
	uint16_t GetRotations() const { return wrapped.GetRotations(); }
	void SetRotations(int index) { wrapped.SetRotations(index); }
	int GetSkyOffset() const { return wrapped.GetSkyOffset(); }
	FTextureID GetID() const { return wrapped.GetID(); }
	ISoftwareTexture* GetSoftwareTexture() { return wrapped.GetSoftwareTexture(); }
	void SetSoftwareTexture(ISoftwareTexture* swtex) { wrapped.SetSoftwareTextue(swtex); }
	void SetScale(DVector2 vec) { wrapped.SetScale(vec); }
	const FString& GetName() const { return wrapped.GetName(); }
	void SetShaderSpeed(float speed) { wrapped.shaderspeed = speed; }
	void SetShaderIndex(int index) { wrapped.shaderindex = index; }
	void SetShaderLayers(MaterialLayers& lay)
	{
		// Only update layers that have something defind.
		if (lay.Glossiness > -1000) wrapped.Glossiness = lay.Glossiness;
		if (lay.SpecularLevel > -1000) wrapped.SpecularLevel = lay.SpecularLevel;
		if (lay.Brightmap) wrapped.Brightmap = lay.Brightmap->GetTexture();
		if (lay.Normal) wrapped.Normal = lay.Normal->GetTexture();
		if (lay.Specular) wrapped.Specular = lay.Specular->GetTexture();
		if (lay.Metallic) wrapped.Metallic = lay.Metallic->GetTexture();
		if (lay.Roughness) wrapped.Roughness = lay.Roughness->GetTexture();
		if (lay.AmbientOcclusion) wrapped.AmbientOcclusion = lay.AmbientOcclusion->GetTexture();
		for (int i = 0; i < MAX_CUSTOM_HW_SHADER_TEXTURES; i++)
		{
			if (lay.CustomShaderTextures[i]) wrapped.CustomShaderTextures[i] = lay.CustomShaderTextures[i]->GetTexture();
		}
	}

	void CopySize(FGameTexture* BaseTexture)
	{
		wrapped.CopySize(&BaseTexture->wrapped);
	}

	// These substitutions must be done on the material level because their sizes can differ. Substitution must happen before any coordinate calculations take place.
	FGameTexture* GetPalVersion() { return reinterpret_cast<FGameTexture*>(wrapped.GetPalVersion()); }
	FGameTexture* GetRawTexture() { return reinterpret_cast<FGameTexture*>(wrapped.GetRawTexture()); }
	FGameTexture* GetFrontSkyLayer() { return reinterpret_cast<FGameTexture*>(wrapped.GetFrontSkyLayer()); }

	// Glowing is a pure material property that should not filter down to the actual texture objects.
	void GetGlowColor(float* data) { wrapped.GetGlowColor(data); }
	bool isGlowing() const { return wrapped.isGlowing(); }
	bool isAutoGlowing() const { return wrapped.isAutoGlowing(); }
	int GetGlowHeight() const { return wrapped.GetGlowHeight(); }
	void SetAutoGlowing() { auto tex = GetTexture(); tex->bAutoGlowing = tex->bGlowing = tex->bFullbright = true; }
	void SetGlowHeight(int v) { wrapped.GlowHeight = v; }
	void SetFullbright() { wrapped.bFullbright = true;  }
	void SetDisableFullbright(bool on) { wrapped.bDisableFullbright = on; }
	void SetGlowing(PalEntry color) { auto tex = GetTexture(); tex->bAutoGlowing = false;	tex->bGlowing = true; tex->GlowColor = color; }

	bool isUserContent() const;
	void AddAutoMaterials() { wrapped.AddAutoMaterials(); }
	int CheckRealHeight() { return wrapped.CheckRealHeight(); }
	bool isSkybox() const { return wrapped.isSkybox(); }
	void SetSize(int x, int y) { wrapped.SetSize(x, y); }

	void SetSpriteRect() { wrapped.SetSpriteRect(); }
	const SpritePositioningInfo& GetSpritePositioning(int which) { /* todo: keep two sets of positioning infd*/ if (wrapped.mTrimResult == -1) wrapped.SetupSpriteData(); return wrapped.spi; }
	int GetAreas(FloatRect** pAreas) const { return wrapped.GetAreas(pAreas); }
	PalEntry GetSkyCapColor(bool bottom) { return wrapped.GetSkyCapColor(bottom); }

	bool GetTranslucency()
	{
		return wrapped.GetTranslucency();
	}

	FGameTexture *GetSkyFace(int num)
	{
		return reinterpret_cast<FGameTexture*>(isSkybox() ? static_cast<FSkyBox*>(&wrapped)->faces[num] : nullptr);
	}

};


#endif


