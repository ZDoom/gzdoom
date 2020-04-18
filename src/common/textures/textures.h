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
#include "refcounted.h"
#include "xs_Float.h"

// 15 because 0th texture is our texture
#define MAX_CUSTOM_HW_SHADER_TEXTURES 15

typedef TMap<int, bool> SpriteHits;
class FImageSource;
class FGameTexture;
class IHardwareTexture;

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
	uint8_t mTrimResult;

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
class FTexture : public RefCountedBase
{
	friend class FGameTexture;	// only for the porting work

protected:
	FHardwareTextureContainer SystemTextures;
	FloatRect* areas = nullptr;
	int SourceLump;
	uint16_t Width = 0, Height = 0;

	bool Masked = false;			// Texture (might) have holes
	bool bHasCanvas = false;
	int8_t bTranslucent = -1;
	int8_t areacount = 0;			// this is capped at 4 sections.


public:

	IHardwareTexture* GetHardwareTexture(int translation, int scaleflags);
	static FTexture *CreateTexture(int lumpnum, bool allowflats = false);
	virtual FImageSource *GetImage() const { return nullptr; }
	void CreateUpsampledTextureBuffer(FTextureBuffer &texbuffer, bool hasAlpha, bool checkonly);
	void CleanHardwareTextures();

	int GetWidth() { return Width; }
	int GetHeight() { return Height; }
	
	bool isHardwareCanvas() const { return bHasCanvas; }	// There's two here so that this can deal with software canvases in the hardware renderer later.
	bool isCanvas() const { return bHasCanvas; }
	
	int GetSourceLump() { return SourceLump; }	// needed by the scripted GetName method.
	bool FindHoles(const unsigned char * buffer, int w, int h);


	void CopySize(FTexture* BaseTexture)
	{
		Width = BaseTexture->GetWidth();
		Height = BaseTexture->GetHeight();
	}

	// This is only used for the null texture and for Heretic's skies.
	void SetSize(int w, int h)
	{
		Width = w;
		Height = h;
	}

	bool TrimBorders(uint16_t* rect);
	int GetAreas(FloatRect** pAreas) const;

	// Returns the whole texture, stored in column-major order
	virtual TArray<uint8_t> Get8BitPixels(bool alphatex);
	virtual FBitmap GetBgraBitmap(const PalEntry *remap, int *trans = nullptr);

	static bool SmoothEdges(unsigned char * buffer,int w, int h);


	virtual void ResolvePatches() {}


	FTexture (int lumpnum = -1);

public:
	FTextureBuffer CreateTexBuffer(int translation, int flags = 0);
	virtual bool DetermineTranslucency();
	bool GetTranslucency()
	{
		return bTranslucent != -1 ? bTranslucent : DetermineTranslucency();
	}

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
	FCanvasTexture(int width, int height)
	{
		Width = width;
		Height = height;

		bHasCanvas = true;
		aspectRatio = (float)width / height;
	}

	void NeedUpdate() { bNeedsUpdate = true; }
	void SetUpdated(bool rendertype) { bNeedsUpdate = false; bFirstUpdate = false; bLastUpdateType = rendertype; }

protected:

	bool bLastUpdateType = false;
	bool bNeedsUpdate = true;
public:
	bool bFirstUpdate = true;
	float aspectRatio;

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
		return SystemTextures.GetHardwareTexture(0, 0);
	}

	int GetColorFormat() const
	{
		return Format;
	}
};


class FImageTexture : public FTexture
{
	FImageSource* mImage;
	bool bNoRemap0 = false;
protected:
	void SetFromImage();
public:
	FImageTexture(FImageSource* image) noexcept;
	virtual TArray<uint8_t> Get8BitPixels(bool alphatex);

	void SetImage(FImageSource* img)
	{
		mImage = img;
		SetFromImage();
	}
	void SetNoRemap0() { bNoRemap0 = true; }

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
	void GetFromTexture(FGameTexture* tex, float x, float y, bool forceworldpanning);
};

enum
{
	CLAMP_NONE = 0,
	CLAMP_X = 1,
	CLAMP_Y = 2,
	CLAMP_XY = 3,
	CLAMP_XY_NOMIP = 4,
	CLAMP_NOFILTER = 5,
	CLAMP_CAMTEX = 6,
};


enum EGameTexFlags
{
	GTexf_NoDecals = 1,						// Decals should not stick to texture
	GTexf_WorldPanning = 2,					// Texture is panned in world units rather than texels
	GTexf_FullNameTexture = 4,				// Name is taken from the file system.
	GTexf_Glowing = 8,						// Texture emits a glow
	GTexf_AutoGlowing = 16,					// Glow info is determined from texture image.
	GTexf_RenderFullbright = 32,			// always draw fullbright
	GTexf_DisableFullbrightSprites = 64,	// This texture will not be displayed as fullbright sprite
	GTexf_BrightmapChecked = 128,			// Check for a colormap-based brightmap was already done.
};

// Refactoring helper to allow piece by piece adjustment of the API
class FGameTexture
{
	friend class FMaterial;

	// Material layers. These are shared so reference counting is used.
	RefCountedPtr<FTexture> Base;
	RefCountedPtr<FTexture> Brightmap;
	RefCountedPtr<FTexture> Detailmap;
	RefCountedPtr<FTexture> Glowmap;
	RefCountedPtr<FTexture> Normal;							// Normal map texture
	RefCountedPtr<FTexture> Specular;						// Specular light texture for the diffuse+normal+specular light model
	RefCountedPtr<FTexture> Metallic;						// Metalness texture for the physically based rendering (PBR) light model
	RefCountedPtr<FTexture> Roughness;						// Roughness texture for PBR
	RefCountedPtr<FTexture> AmbientOcclusion;				// Ambient occlusion texture for PBR
	RefCountedPtr<FTexture> CustomShaderTextures[MAX_CUSTOM_HW_SHADER_TEXTURES]; // Custom texture maps for custom hardware shaders

	FString Name;
	FTextureID id;

	uint16_t TexelWidth, TexelHeight;
	int16_t LeftOffset[2], TopOffset[2];
	float DisplayWidth, DisplayHeight;
	float ScaleX, ScaleY;

	int8_t shouldUpscaleFlag = 0;				// Without explicit setup, scaling is disabled for a texture.
	ETextureType UseType = ETextureType::Wall;	// This texture's primary purpose
	SpritePositioningInfo* spi = nullptr;

	ISoftwareTexture* SoftwareTexture = nullptr;
	FMaterial* Material[4] = {  };

	// Material properties
	float Glossiness = 10.f;
	float SpecularLevel = 0.1f;
	float shaderspeed = 1.f;
	int shaderindex = 0;

	int flags = 0;
	uint8_t warped = 0, expandSprite = -1;
	uint16_t GlowHeight;
	PalEntry GlowColor = 0;

	int16_t SkyOffset = 0;
	uint16_t Rotations = 0xffff;


public:
	FGameTexture(FTexture* wrap, const char *name);
	~FGameTexture();
	void Setup(FTexture* wrap);
	FTextureID GetID() const { return id; }
	void SetID(FTextureID newid) { id = newid; }	// should only be called by the texture manager
	const FString& GetName() const { return Name; }
	void SetName(const char* name) { Name = name; }	// should only be called by setup code.

	float GetScaleX() { return ScaleX; }
	float GetScaleY() { return ScaleY; }
	float GetDisplayWidth() const { return DisplayWidth; }
	float GetDisplayHeight() const { return DisplayHeight; }
	int GetTexelWidth() const { return TexelWidth; }
	int GetTexelHeight() const { return TexelHeight; }

	void CreateDefaultBrightmap();
	void AddAutoMaterials();
	bool ShouldExpandSprite();
	void SetupSpriteData();
	void SetSpriteRect();

	ETextureType GetUseType() const { return UseType; }
	void SetUpscaleFlag(int what) { shouldUpscaleFlag = what; }
	int GetUpscaleFlag() { return shouldUpscaleFlag; }

	FTexture* GetTexture() { return Base.get(); }
	int GetSourceLump() const { return Base->GetSourceLump(); }
	void SetBrightmap(FGameTexture* tex) { Brightmap = tex->GetTexture(); }

	int GetTexelLeftOffset(int adjusted = 0) const { return LeftOffset[adjusted]; }
	int GetTexelTopOffset(int adjusted = 0) const { return TopOffset[adjusted]; }
	float GetDisplayLeftOffset(int adjusted = 0) const { return LeftOffset[adjusted] / ScaleX; }
	float GetDisplayTopOffset(int adjusted = 0) const { return TopOffset[adjusted] / ScaleY; }

	bool isMiscPatch() const { return GetUseType() == ETextureType::MiscPatch; }	// only used by the intermission screen to decide whether to tile the background image or not. 
	bool isFullbrightDisabled() const { return !!(flags & GTexf_DisableFullbrightSprites); }
	bool isFullbright() const { return !!(flags & GTexf_RenderFullbright); }
	bool isFullNameTexture() const { return !!(flags & GTexf_FullNameTexture); }
	bool expandSprites() const { return !!expandSprite; }
	bool useWorldPanning() const { return !!(flags & GTexf_WorldPanning);  }
	void SetWorldPanning(bool on) { if (on) flags |= GTexf_WorldPanning; else flags &= ~GTexf_WorldPanning; }
	bool allowNoDecals() const { return !!(flags & GTexf_NoDecals);	}
	void SetNoDecals(bool on) { if (on) flags |= GTexf_NoDecals; else flags &= ~GTexf_NoDecals; }

	bool isValid() const { return UseType != ETextureType::Null; }
	int isWarped() { return warped; }
	void SetWarpStyle(int style) { warped = style; }
	bool isMasked() { return Base->Masked; }
	bool isHardwareCanvas() const { return Base->isHardwareCanvas(); }	// There's two here so that this can deal with software canvases in the hardware renderer later.
	bool isSoftwareCanvas() const { return Base->isCanvas(); }

	void SetTranslucent(bool on) { Base->bTranslucent = on; }
	void SetUseType(ETextureType type) { UseType = type; }
	int GetRotations() const { return Rotations; }
	void SetRotations(int rot) { Rotations = int16_t(rot); }
	void SetSkyOffset(int offs) { SkyOffset = offs; }
	int GetSkyOffset() const { return SkyOffset; }

	ISoftwareTexture* GetSoftwareTexture()
	{
		return SoftwareTexture;
	}
	void SetSoftwareTexture(ISoftwareTexture* swtex)
	{
		SoftwareTexture = swtex;
	}

	FMaterial* GetMaterial(int num)
	{
		return Material[num];
	}

	int GetShaderIndex() const { return shaderindex; }
	float GetShaderSpeed() const { return shaderspeed; }
	void SetShaderSpeed(float speed) { shaderspeed = speed; }
	void SetShaderIndex(int index) { shaderindex = index; }
	void SetShaderLayers(MaterialLayers& lay)
	{
		// Only update layers that have something defind.
		if (lay.Glossiness > -1000) Glossiness = lay.Glossiness;
		if (lay.SpecularLevel > -1000) SpecularLevel = lay.SpecularLevel;
		if (lay.Brightmap) Brightmap = lay.Brightmap->GetTexture();
		if (lay.Normal) Normal = lay.Normal->GetTexture();
		if (lay.Specular) Specular = lay.Specular->GetTexture();
		if (lay.Metallic) Metallic = lay.Metallic->GetTexture();
		if (lay.Roughness) Roughness = lay.Roughness->GetTexture();
		if (lay.AmbientOcclusion) AmbientOcclusion = lay.AmbientOcclusion->GetTexture();
		for (int i = 0; i < MAX_CUSTOM_HW_SHADER_TEXTURES; i++)
		{
			if (lay.CustomShaderTextures[i]) CustomShaderTextures[i] = lay.CustomShaderTextures[i]->GetTexture();
		}
	}
	float GetGlossiness() const { return Glossiness; }
	float GetSpecularLevel() const { return SpecularLevel; }

	void CopySize(FGameTexture* BaseTexture)
	{
		Base->CopySize(BaseTexture->Base.get());
	}

	// Glowing is a pure material property that should not filter down to the actual texture objects.
	void GetGlowColor(float* data);
	bool isGlowing() const { return !!(flags & GTexf_Glowing); }
	bool isAutoGlowing() const { return !!(flags & GTexf_AutoGlowing); }
	int GetGlowHeight() const { return GlowHeight; }
	void SetAutoGlowing() { flags |= (GTexf_AutoGlowing | GTexf_Glowing | GTexf_RenderFullbright); }
	void SetGlowHeight(int v) { GlowHeight = v; }
	void SetFullbright() { flags |= GTexf_RenderFullbright;  }
	void SetDisableFullbright(bool on) { if (on) flags |= GTexf_DisableFullbrightSprites; else flags &= ~GTexf_DisableFullbrightSprites; }
	void SetGlowing(PalEntry color) { flags = (flags & ~GTexf_AutoGlowing) | GTexf_Glowing; GlowColor = color; }

	bool isUserContent() const;
	int CheckRealHeight() { return xs_RoundToInt(Base->CheckRealHeight() / ScaleY); }
	void SetSize(int x, int y) 
	{ 
		TexelWidth = x; 
		TexelHeight = y;
		SetDisplaySize(float(x), float(y));
	}
	void SetDisplaySize(float w, float h) 
	{ 
		DisplayWidth = w;
		DisplayHeight = h;
		ScaleX = TexelWidth / w;
		ScaleY = TexelHeight / h;

		// compensate for roundoff errors
		if (int(ScaleX * w) != TexelWidth) ScaleX += (1 / 65536.);
		if (int(ScaleY * h) != TexelHeight) ScaleY += (1 / 65536.);

	}
	void SetOffsets(int which, int x, int y)
	{
		LeftOffset[which] = x;
		TopOffset[which] = y;
	}
	void SetScale(float x, float y) 
	{
		ScaleX = x;
		ScaleY = y;
		DisplayWidth = x * TexelWidth;
		DisplayHeight = y * TexelHeight;
	}

	const SpritePositioningInfo& GetSpritePositioning(int which) { if (spi == nullptr) SetupSpriteData(); return spi[which]; }
	int GetAreas(FloatRect** pAreas) const;

	bool GetTranslucency()
	{
		return Base->GetTranslucency();
	}

	int GetClampMode(int clampmode)
	{
		if (GetUseType() == ETextureType::SWCanvas) clampmode = CLAMP_NOFILTER;
		else if (isHardwareCanvas()) clampmode = CLAMP_CAMTEX;
		else if ((isWarped() || shaderindex >= FIRST_USER_SHADER) && clampmode <= CLAMP_XY) clampmode = CLAMP_NONE;
		return clampmode;
	}

	void CleanHardwareData(bool full = true);

};

inline FGameTexture* MakeGameTexture(FTexture* tex, const char *name, ETextureType useType)
{
	if (!tex) return nullptr;
	auto t = new FGameTexture(tex, name);
	t->SetUseType(useType);
	return t;
}

enum EUpscaleFlags
{
	UF_None = 0,
	UF_Texture = 1,
	UF_Sprite = 2,
	UF_Font = 4
};

extern int upscalemask;
void UpdateUpscaleMask();

int calcShouldUpscale(FGameTexture* tex);
inline int shouldUpscale(FGameTexture* tex, EUpscaleFlags UseType)
{
	// This only checks the global scale mask and the texture's validation for upscaling. Everything else has been done up front elsewhere.
	if (!(upscalemask & UseType)) return 0;
	return tex->GetUpscaleFlag();
}

#endif


