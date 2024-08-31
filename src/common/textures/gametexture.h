#pragma once
#include <stdint.h>
#include <memory>
#include "vectors.h"
#include "floatrect.h"
#include "refcounted.h"
#include "xs_Float.h"
#include "palentry.h"
#include "zstring.h"
#include "textureid.h"

// 15 because 0th texture is our texture
#define MAX_CUSTOM_HW_SHADER_TEXTURES 15
class FTexture;
class ISoftwareTexture;
class FMaterial;

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
	GTexf_AutoMaterialsAdded = 256,			// AddAutoMaterials has been called on this texture.
	GTexf_OffsetsNotForFont = 512,			// The offsets must be ignored when using this texture in a font.
	GTexf_NoTrim = 1024,					// Don't perform trimming on this texture.
	GTexf_Seen = 2048,						// Set to true when the texture is being used for rendering. Must be cleared manually if the check is needed.
};

struct FMaterialLayers
{
	RefCountedPtr<FTexture> Detailmap;
	RefCountedPtr<FTexture> Glowmap;
	RefCountedPtr<FTexture> Normal;							// Normal map texture
	RefCountedPtr<FTexture> Specular;						// Specular light texture for the diffuse+normal+specular light model
	RefCountedPtr<FTexture> Metallic;						// Metalness texture for the physically based rendering (PBR) light model
	RefCountedPtr<FTexture> Roughness;						// Roughness texture for PBR
	RefCountedPtr<FTexture> AmbientOcclusion;				// Ambient occlusion texture for PBR
	RefCountedPtr<FTexture> CustomShaderTextures[MAX_CUSTOM_HW_SHADER_TEXTURES]; // Custom texture maps for custom hardware shaders
};

// Refactoring helper to allow piece by piece adjustment of the API
class FGameTexture
{
	friend class FMaterial;
	friend class GLDefsParser;	// this needs access to set up the texture properly

	// Material layers. These are shared so reference counting is used.
	RefCountedPtr<FTexture> Base;
	RefCountedPtr<FTexture> Brightmap;
	std::unique_ptr<FMaterialLayers> Layers;

	FString Name;
	FTextureID id;

	uint16_t TexelWidth, TexelHeight;
	int16_t LeftOffset[2], TopOffset[2];
	float DisplayWidth, DisplayHeight;
	float ScaleX, ScaleY;

	int8_t shouldUpscaleFlag = 1;
	ETextureType UseType = ETextureType::Wall;	// This texture's primary purpose
	SpritePositioningInfo* spi = nullptr;

	ISoftwareTexture* SoftwareTexture = nullptr;
	FMaterial* Material[5] = {  };

	// Material properties
	FVector2 detailScale = { 1.f, 1.f };
	float Glossiness = 10.f;
	float SpecularLevel = 0.1f;
	float shaderspeed = 1.f;
	int shaderindex = 0;

	int flags = 0;
	uint8_t warped = 0;
	int8_t expandSprite = -1;
	uint16_t GlowHeight = 128;
	PalEntry GlowColor = 0;

	int16_t SkyOffset = 0;
	uint16_t Rotations = 0xffff;


public:
	float alphaThreshold = 0.5f;

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
	void SetUpscaleFlag(int what, bool manual = false) 
	{ 
		if ((shouldUpscaleFlag & 2) && !manual) return;	// if set manually this may not be reset.
		shouldUpscaleFlag = what | (manual? 2 : 0); 
	}
	int GetUpscaleFlag() { return shouldUpscaleFlag & 1; }

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
	bool expandSprites() { return expandSprite == -1? ShouldExpandSprite() : !!expandSprite; }
	bool useWorldPanning() const { return !!(flags & GTexf_WorldPanning);  }
	void SetWorldPanning(bool on) { if (on) flags |= GTexf_WorldPanning; else flags &= ~GTexf_WorldPanning; }
	void SetNoTrimming(bool on) { if (on) flags |= GTexf_NoTrim; else flags &= ~GTexf_NoTrim; }
	bool GetNoTrimming() { return !!(flags & GTexf_NoTrim); }
	bool allowNoDecals() const { return !!(flags & GTexf_NoDecals);	}
	void SetNoDecals(bool on) { if (on) flags |= GTexf_NoDecals; else flags &= ~GTexf_NoDecals; }
	void SetOffsetsNotForFont() { flags |= GTexf_OffsetsNotForFont; }

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
	void setSeen() { flags |= GTexf_Seen; }
	bool isSeen(bool reset) 
	{ 
		int v = flags & GTexf_Seen;   
		if (reset) flags &= ~GTexf_Seen;
		return v;
	}

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

		bool needlayers = (lay.Normal || lay.Specular || lay.Metallic || lay.Roughness || lay.AmbientOcclusion);
		for (int i = 0; i < MAX_CUSTOM_HW_SHADER_TEXTURES && !needlayers; i++)
		{
			if (lay.CustomShaderTextures[i]) needlayers = true;
		}
		if (needlayers)
		{
			Layers = std::make_unique<FMaterialLayers>();

			if (lay.Normal) Layers->Normal = lay.Normal->GetTexture();
			if (lay.Specular) Layers->Specular = lay.Specular->GetTexture();
			if (lay.Metallic) Layers->Metallic = lay.Metallic->GetTexture();
			if (lay.Roughness) Layers->Roughness = lay.Roughness->GetTexture();
			if (lay.AmbientOcclusion) Layers->AmbientOcclusion = lay.AmbientOcclusion->GetTexture();
			for (int i = 0; i < MAX_CUSTOM_HW_SHADER_TEXTURES; i++)
			{
				if (lay.CustomShaderTextures[i]) Layers->CustomShaderTextures[i] = lay.CustomShaderTextures[i]->GetTexture();
			}
		}
	}
	float GetGlossiness() const { return Glossiness; }
	float GetSpecularLevel() const { return SpecularLevel; }

	void CopySize(FGameTexture* BaseTexture, bool forfont = false)
	{
		Base->CopySize(BaseTexture->Base.get());
		SetDisplaySize(BaseTexture->GetDisplayWidth(), BaseTexture->GetDisplayHeight());
		if (!forfont || !(BaseTexture->flags & GTexf_OffsetsNotForFont))
		{
			SetOffsets(0, BaseTexture->GetTexelLeftOffset(0), BaseTexture->GetTexelTopOffset(0));
			SetOffsets(1, BaseTexture->GetTexelLeftOffset(1), BaseTexture->GetTexelTopOffset(1));
		}
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
	void SetDisableBrightmap() { flags |= GTexf_BrightmapChecked; Brightmap = nullptr; }

	bool isUserContent() const;
	int CheckRealHeight() { return xs_RoundToInt(Base->CheckRealHeight() / ScaleY); }
	void SetSize(int x, int y) 
	{ 
		TexelWidth = x; 
		TexelHeight = y;
		SetDisplaySize(float(x), float(y));
		if (GetTexture()) GetTexture()->SetSize(x, y);
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
	void SetBase(FTexture* Tex)
	{
		Base = Tex;
	}
	void SetOffsets(int which, int x, int y)
	{
		LeftOffset[which] = x;
		TopOffset[which] = y;
	}
	void SetOffsets(int x, int y)
	{
		LeftOffset[0] = x;
		TopOffset[0] = y;
		LeftOffset[1] = x;
		TopOffset[1] = y;
	}
	void SetScale(float x, float y) 
	{
		ScaleX = x;
		ScaleY = y;
		DisplayWidth = TexelWidth / x;
		DisplayHeight = TexelHeight / y;
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

	void GetLayers(TArray<FTexture*>& layers)
	{
		layers.Clear();
		for (auto tex : { Base.get(), Brightmap.get() })
		{
			if (tex != nullptr) layers.Push(tex);
		}
		if (Layers)
		{
			for (auto tex : { Layers->Detailmap.get(), Layers->Glowmap.get(), Layers->Normal.get(), Layers->Specular.get(), Layers->Metallic.get(), Layers->Roughness.get(), Layers->AmbientOcclusion.get() })
			{
				if (tex != nullptr) layers.Push(tex);
			}
			for (auto& tex : Layers->CustomShaderTextures)
			{
				if (tex != nullptr) layers.Push(tex.get());
			}
		}
	}

	FVector2 GetDetailScale() const
	{
		return detailScale;
	}

	void SetDetailScale(float x, float y)
	{
		detailScale.X = x;
		detailScale.Y = y;
	}

	FTexture* GetBrightmap()
	{
		if (Brightmap.get() || (flags & GTexf_BrightmapChecked)) return Brightmap.get();
		CreateDefaultBrightmap();
		return Brightmap.get();
	}
	FTexture* GetGlowmap()
	{
		if (!Layers) return nullptr;
		return Layers->Glowmap.get();
	}
	FTexture* GetDetailmap()
	{
		if (!Layers) return nullptr;
		return Layers->Detailmap.get();
	}
	FTexture* GetNormalmap()
	{
		if (!Layers) return nullptr;
		return Layers->Normal.get();
	}
	FTexture* GetSpecularmap()
	{
		if (!Layers) return nullptr;
		return Layers->Specular.get();
	}
	FTexture* GetMetallic()
	{
		if (!Layers) return nullptr;
		return Layers->Metallic.get();
	}
	FTexture* GetRoughness()
	{
		if (!Layers) return nullptr;
		return Layers->Roughness.get();
	}
	FTexture* GetAmbientOcclusion()
	{
		if (!Layers) return nullptr;
		return Layers->AmbientOcclusion.get();
	}

	void SetGlowmap(FTexture *T)
	{
		if (!Layers) Layers = std::make_unique<FMaterialLayers>();
		Layers->Glowmap = T;
	}
	void SetDetailmap(FTexture* T)
	{
		if (!Layers) Layers = std::make_unique<FMaterialLayers>();
		Layers->Detailmap = T;
	}
	void SetNormalmap(FTexture* T)
	{
		if (!Layers) Layers = std::make_unique<FMaterialLayers>();
		Layers->Normal = T;
	}
	void SetSpecularmap(FTexture* T)
	{
		if (!Layers) Layers = std::make_unique<FMaterialLayers>();
		Layers->Specular = T;
	}

};

inline FGameTexture* MakeGameTexture(FTexture* tex, const char *name, ETextureType useType)
{
	if (!tex) return nullptr;
	auto t = new FGameTexture(tex, name);
	t->SetUseType(useType);
	return t;
}

enum EUpscaleFlags : int
{
	UF_None = 0,
	UF_Texture = 1,
	UF_Sprite = 2,
	UF_Font = 4,
	UF_Skin = 8
};

extern int upscalemask;
void UpdateUpscaleMask();

void calcShouldUpscale(FGameTexture* tex);
inline int shouldUpscale(FGameTexture* tex, EUpscaleFlags UseType)
{
	// This only checks the global scale mask and the texture's validation for upscaling. Everything else has been done up front elsewhere.
	if (!(upscalemask & UseType)) return 0;
	return tex->GetUpscaleFlag();
}

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
